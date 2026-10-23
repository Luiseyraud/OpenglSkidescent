//-----------------------------------------------------------------------------
// World.cpp : chargement et dessin de la scène
//-----------------------------------------------------------------------------
#include "World.h"
#include "Decor.h"
#include "Course.h"
#include "Timeline.h"
#include <iostream>
#include <string>
#include "GLFW/glfw3.h"
#include "glm/gtc/matrix_transform.hpp"

using namespace scene;

//-----------------------------------------------------------------------------
// Chargement de la scène
//-----------------------------------------------------------------------------
bool World::load()
{
	if (!shader.loadShaders("shaders/scene.vert", "shaders/scene.frag"))
		return false;
	if (!bulbShader.loadShaders("shaders/bulb.vert", "shaders/bulb.frag"))
		return false;
	if (!crowdShader.loadShaders("shaders/crowd.vert", "shaders/scene.frag"))
		return false;
	if (!sky.load() || !glow.load())
		return false;

	double start = glfwGetTime();
	if (!terrain.buildMesh())
		return false;
	std::cout << "Terrain construit en " << glfwGetTime() - start << " s" << std::endl;

	// Modèles (convertis par tools/convertir_modeles.sh) et leurs textures.
	// Les textures "palette" sont chargées sans mipmaps : en réduisant l'image,
	// les mipmaps mélangeraient les carrés de couleur voisins.
	bool ok = snowTexture.loadTexture("textures/neige.jpg", true)
	       && chalet.loadOBJ("models/chalet.obj")     && chaletTexture.loadTexture("textures/chalet.jpg", true)
	       && skier.loadOBJ("models/skieur.obj")      && skierTexture.loadTexture("textures/skieur_palette.png", false)
	       && lampPost.loadOBJ("models/lampPost.obj") && lampPostTexture.loadTexture("textures/lamp_post_diffuse.png", true)
	       && tree.loadOBJ("models/sapin.obj")        && treeTexture.loadTexture("textures/sapin_palette.png", false)
	       && rock.loadOBJ("models/rocher.obj")       && rockTexture.loadTexture("textures/rocher_palette.png", false)
	       && bulb.loadOBJ("models/ampoule.obj");
	if (!ok)
		return false;

	// Pylônes d'éclairage : placés avant le décor, qui doit laisser leur pied libre
	floodlights = placeFloodlights(piste, terrain);
	if ((int)floodlights.size() > MAX_FLOODLIGHTS)
	{
		std::cerr << "Trop de pylônes : " << floodlights.size() << " (le shader en accepte " << MAX_FLOODLIGHTS << ")" << std::endl;
		floodlights.resize(MAX_FLOODLIGHTS);
	}
	std::vector<glm::vec3> pylonFeet;
	std::vector<glm::mat4> pylonModels;
	for (const Floodlight& light : floodlights)
	{
		pylonFeet.push_back(glm::vec3(light.model[3]));		// 4e colonne de la matrice : la translation
		pylonModels.push_back(light.model);
	}

	// Décor : positions des sapins et des rochers, envoyées une fois pour toutes
	std::vector<glm::mat4> trees = placeTrees(terrain, pylonFeet);
	std::vector<glm::mat4> rocks = placeRocks(terrain, pylonFeet);
	tree.setInstances(trees);
	rock.setInstances(rocks);
	std::cout << "Décor : " << trees.size() << " sapins, " << rocks.size() << " rochers" << std::endl;

	// Équipement de course : bandes bleues et filets (maillages calculés par le code),
	// drapeaux (modèle téléchargé, en plusieurs exemplaires)
	std::vector<Vertex> netVertices, poleVertices;
	buildNets(piste, terrain, netVertices, poleVertices);
	int netWidth, netHeight;
	std::vector<unsigned char> netImage = netPixels(netWidth, netHeight);
	std::vector<glm::mat4> flags = placeFlags(piste, terrain);

	ok = lines.create(buildLines(piste, terrain))
	  && lineTexture.createSolidColor(LINE_COLOR.r, LINE_COLOR.g, LINE_COLOR.b)
	  && net.create(netVertices)
	  && netTexture.createFromPixels(netWidth, netHeight, netImage.data(), true)
	  && netPoles.create(poleVertices)
	  && netPoleTexture.createSolidColor(NET_POLE_COLOR.r, NET_POLE_COLOR.g, NET_POLE_COLOR.b)
	  && flag.loadOBJ("models/drapeau.obj") && flagTexture.loadTexture("textures/drapeau_palette.png", false)
	  && pylon.loadOBJ("models/pylone.obj") && pylonTexture.loadTexture("textures/pylone_palette.png", false)
	  && lenses.loadOBJ("models/projecteurs.obj");
	if (!ok)
		return false;
	flag.setInstances(flags);
	pylon.setInstances(pylonModels);

	// Effets de neige : la trace et les gerbes sont calculées une fois pour toute la
	// vidéo, à partir du trajet du skieur
	if (!trail.build(piste, terrain) || !spray.build(piste, terrain))
		return false;
	std::cout << "Piste : " << flags.size() << " drapeaux, " << poleVertices.size() / 30 << " poteaux de filet" << std::endl;

	// Tribunes : la structure, le même siège à chaque place (une liste d'instances
	// par couleur), les guirlandes, et le public
	if (!buildStands(piste, terrain, "models/sieges.txt", stands))
		return false;
	ok = stand.loadOBJ("models/tribune.obj") && standTexture.loadTexture("textures/tribune_palette.png", false)
	  && wires.create(stands.wires) && wireTexture.createSolidColor(35, 35, 38)
	  && crowd.build(stands.places);
	for (int k = 0; k < 3 && ok; k++)
	{
		std::vector<glm::mat4> section;
		for (const Place& place : stands.places)
			if (place.section == k)
				section.push_back(place.seat);
		ok = seats[k].loadOBJ("models/siege.obj")
		  && seatTextures[k].createSolidColor(SEAT_COLORS[k].r, SEAT_COLORS[k].g, SEAT_COLORS[k].b);
		seats[k].setInstances(section);
	}
	if (!ok)
		return false;

	// Le public se lève quand le skieur arrive à CHEER_DISTANCE mètres de l'arrivée
	for (float t = 0.0f; t < timeline::DURATION; t += 0.01f)
		if (timeline::skierDistance(t, piste) >= piste.length() - CHEER_DISTANCE)
		{
			cheerTime = t;
			break;
		}

	// Halos : les projecteurs (forts dans leur faisceau), puis les ampoules des guirlandes
	for (const Floodlight& light : floodlights)
		glowSources.push_back({ light.position, light.direction, FLOODLIGHT_COLOR * FLOODLIGHT_GLOW_STRENGTH, FLOODLIGHT_GLOW_SIZE });
	for (const glm::vec3& b : stands.bulbs)
		glowSources.push_back({ b, glm::vec3(0.0f), GARLAND_COLOR * 0.5f, 0.35f });

	return true;
}

//-----------------------------------------------------------------------------
// Matrice model d'un objet posé sur le terrain en (x, z), tourné autour de Y
//-----------------------------------------------------------------------------
glm::mat4 World::onGround(const glm::vec3& position, float angleDeg) const
{
	glm::vec3 p(position.x, terrain.height(position.x, position.z), position.z);
	return glm::translate(glm::mat4(1.0f), p)
	     * glm::rotate(glm::mat4(1.0f), glm::radians(angleDeg), glm::vec3(0.0f, 1.0f, 0.0f));
}

//-----------------------------------------------------------------------------
// Prépare un shader qui utilise scene.frag : caméra, ciel, brouillard, lumières.
// Sert au shader de la scène et à celui du public.
//-----------------------------------------------------------------------------
void World::prepareShader(ShaderProgram& program, const glm::mat4& view, const glm::mat4& projection,
                          const glm::vec3& eye, const glm::mat4& skierModel)
{
	// Must be called BEFORE setting uniforms because setting uniforms is done
	// on the currently active shader program.
	program.use();
	program.setUniform("view", view);
	program.setUniform("projection", projection);
	program.setUniform("viewPos", eye);
	program.setUniformSampler("material.diffuseMap", 0);

	// Ciel (pour la couleur du brouillard), lumière ambiante et brouillard
	Sky::setUniforms(program);
	program.setUniform("ambientSky", AMBIENT_SKY);
	program.setUniform("ambientGround", AMBIENT_GROUND);
	program.setUniform("fogDensity", FOG_DENSITY);
	program.setUniform("twoSided", 0);
	setLights(program, skierModel);
}

//-----------------------------------------------------------------------------
// Lumières : le soleil (directionnelle), 10 ponctuelles, la frontale et les
// projecteurs (spots)
//-----------------------------------------------------------------------------
void World::setLights(ShaderProgram& program, const glm::mat4& skierModel)
{
	// Soleil : chaud et rasant. Le shader attend -L.
	program.setUniform("sunLight.direction", -sunDirection());
	program.setUniform("sunLight.diffuse", SUN_COLOR * SUN_INTENSITY);
	program.setUniform("sunLight.specular", SUN_COLOR * 0.3f);

	// Ponctuelles : les ampoules des 3 lampadaires et la lampe du chalet, avec une
	// atténuation 1 / (1 + 0.09 d + 0.032 d²) (portée d'une cinquantaine de mètres) ;
	// puis les guirlandes des tribunes, plus faibles et de plus courte portée.
	std::vector<glm::vec3> lamps;
	for (const glm::vec3& lamp : LAMP_POSTS)
		lamps.push_back(glm::vec3(onGround(lamp, 0.0f) * glm::vec4(0.0f, LAMP_BULB_HEIGHT, 0.0f, 1.0f)));
	lamps.push_back(glm::vec3(onGround(CHALET_POS, CHALET_ANGLE_DEG) * glm::vec4(CHALET_LIGHT_OFFSET, 1.0f)));
	int lampCount = (int)lamps.size();
	lamps.insert(lamps.end(), stands.lights.begin(), stands.lights.end());

	for (int i = 0; i < MAX_POINT_LIGHTS; i++)
	{
		std::string light = "pointLights[" + std::to_string(i) + "].";
		bool garland = (i >= lampCount);
		glm::vec3 color = garland ? GARLAND_COLOR * GARLAND_INTENSITY : LAMP_COLOR * LAMP_INTENSITY;
		if (i >= (int)lamps.size())
			color = glm::vec3(0.0f);		// lumière inutilisée : éteinte
		program.setUniform((light + "position").c_str(), (i < (int)lamps.size()) ? lamps[i] : glm::vec3(0.0f));
		program.setUniform((light + "diffuse").c_str(), color);
		program.setUniform((light + "specular").c_str(), color * 0.25f);
		program.setUniform((light + "constant").c_str(), 1.0f);
		program.setUniform((light + "linear").c_str(), garland ? 0.14f : 0.09f);
		program.setUniform((light + "exponent").c_str(), garland ? 0.07f : 0.032f);
	}

	// Spot : la frontale du skieur, sur sa tête, éclaire devant lui vers le sol.
	// Elle suit le skieur : on passe par sa matrice model.
	glm::vec3 headPos = glm::vec3(skierModel * glm::vec4(HEADLAMP_OFFSET, 1.0f));
	glm::vec3 headDir = glm::vec3(skierModel * glm::vec4(HEADLAMP_DIRECTION, 0.0f));
	program.setUniform("spotLight.position", headPos);
	program.setUniform("spotLight.direction", headDir);
	program.setUniform("spotLight.diffuse", HEADLAMP_COLOR * HEADLAMP_INTENSITY);
	program.setUniform("spotLight.specular", HEADLAMP_COLOR);
	program.setUniform("spotLight.cosInnerCone", glm::cos(glm::radians(12.0f)));
	program.setUniform("spotLight.cosOuterCone", glm::cos(glm::radians(18.0f)));
	program.setUniform("spotLight.constant", 1.0f);
	program.setUniform("spotLight.linear", 0.045f);
	program.setUniform("spotLight.exponent", 0.0075f);
	program.setUniform("spotLight.on", 1);

	// Spots : les projecteurs des pylônes. Atténuation 1 / (1 + 0.01 d²) : en 1/d²
	// au loin, comme une vraie lumière (le 1 évite la division par 0 tout près).
	program.setUniform("floodLightCount", (int)floodlights.size());
	for (size_t i = 0; i < floodlights.size(); i++)
	{
		std::string light = "floodLights[" + std::to_string(i) + "].";
		program.setUniform((light + "position").c_str(), floodlights[i].position);
		program.setUniform((light + "direction").c_str(), floodlights[i].direction);
		program.setUniform((light + "diffuse").c_str(), FLOODLIGHT_COLOR * FLOODLIGHT_INTENSITY);
		program.setUniform((light + "specular").c_str(), FLOODLIGHT_COLOR * FLOODLIGHT_INTENSITY);
		program.setUniform((light + "cosInnerCone").c_str(), glm::cos(glm::radians(FLOODLIGHT_INNER_DEG)));
		program.setUniform((light + "cosOuterCone").c_str(), glm::cos(glm::radians(FLOODLIGHT_OUTER_DEG)));
		program.setUniform((light + "constant").c_str(), 1.0f);
		program.setUniform((light + "linear").c_str(), 0.0f);
		program.setUniform((light + "exponent").c_str(), FLOODLIGHT_QUADRATIC);
	}
}

//-----------------------------------------------------------------------------
// Dessine un objet seul, avec sa matrice model
//-----------------------------------------------------------------------------
void World::drawObject(Mesh& mesh, Texture2D& texture, const glm::mat4& model, float specular, float shininess)
{
	shader.setUniform("instanced", 0);
	shader.setUniform("model", model);
	shader.setUniform("material.specular", glm::vec3(specular));
	shader.setUniform("material.shininess", shininess);
	texture.bind(0);
	mesh.draw();
	texture.unbind(0);
}

//-----------------------------------------------------------------------------
// Dessine tous les exemplaires d'un modèle (instancing)
//-----------------------------------------------------------------------------
void World::drawInstances(Mesh& mesh, Texture2D& texture, float specular, float shininess)
{
	shader.setUniform("instanced", 1);
	shader.setUniform("material.specular", glm::vec3(specular));
	shader.setUniform("material.shininess", shininess);
	texture.bind(0);
	mesh.drawInstanced();
	texture.unbind(0);
}

//-----------------------------------------------------------------------------
// Dessin de la scène, vue depuis eye
//-----------------------------------------------------------------------------
void World::render(const glm::mat4& view, const glm::mat4& projection, const glm::vec3& eye, float t)
{
	// Le skieur à l'instant t (position, orientation, flexion)
	timeline::SkierState skierState = timeline::skierState(t, piste, terrain);
	glm::mat4 skierModel = skierState.model;

	// Le ciel recouvre tout le fond : la couleur d'effacement ne se voit pas
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	prepareShader(shader, view, projection, eye, skierModel);

	// Terrain (déjà placé dans le monde) : neige très peu brillante
	shader.setUniform("instanced", 0);
	shader.setUniform("model", glm::mat4(1.0f));
	shader.setUniform("material.specular", glm::vec3(0.1f));
	shader.setUniform("material.shininess", 16.0f);
	snowTexture.bind(0);
	terrain.draw();
	snowTexture.unbind(0);

	// Bandes bleues, peintes sur la neige. Elles sont déjà un peu au-dessus de la
	// neige ; glPolygonOffset les rapproche encore un peu de la caméra dans le test
	// de profondeur, pour qu'elles ne clignotent pas avec la neige (z-fighting).
	glEnable(GL_POLYGON_OFFSET_FILL);
	glPolygonOffset(-1.0f, -1.0f);
	drawObject(lines, lineTexture, glm::mat4(1.0f), 0.1f, 16.0f);

	// Trace des skis : ses bords s'effacent (alpha), avec l'alpha to coverage
	// comme les filets
	shader.setUniform("model", glm::mat4(1.0f));
	shader.setUniform("material.specular", glm::vec3(0.1f));
	shader.setUniform("material.shininess", 16.0f);
	glEnable(GL_SAMPLE_ALPHA_TO_COVERAGE);
	glEnable(GL_SAMPLE_ALPHA_TO_ONE);
	trail.draw(t);
	glDisable(GL_SAMPLE_ALPHA_TO_ONE);
	glDisable(GL_SAMPLE_ALPHA_TO_COVERAGE);
	glDisable(GL_POLYGON_OFFSET_FILL);

	// Drapeaux, pylônes d'éclairage et poteaux des filets
	drawInstances(flag, flagTexture, 0.2f, 16.0f);
	drawInstances(pylon, pylonTexture, 0.4f, 32.0f);
	drawObject(netPoles, netPoleTexture, glm::mat4(1.0f), 0.2f, 16.0f);

	// Filets. Leur texture a des trous (alpha = 0). Avec l'"alpha to coverage", l'alpha
	// décide combien des 4 échantillons du MSAA le fragment recouvre : on voit à
	// travers les trous, sans avoir à trier les objets transparents. ALPHA_TO_ONE
	// écrit ensuite un alpha de 1 dans l'image.
	// twoSided : le filet est une surface sans épaisseur, éclairée des deux côtés.
	shader.setUniform("twoSided", 1);
	glEnable(GL_SAMPLE_ALPHA_TO_COVERAGE);
	glEnable(GL_SAMPLE_ALPHA_TO_ONE);
	drawObject(net, netTexture, glm::mat4(1.0f), 0.1f, 16.0f);
	glDisable(GL_SAMPLE_ALPHA_TO_ONE);
	glDisable(GL_SAMPLE_ALPHA_TO_COVERAGE);
	shader.setUniform("twoSided", 0);

	// Objets de l'arrivée
	drawObject(chalet, chaletTexture, onGround(CHALET_POS, CHALET_ANGLE_DEG), 0.05f, 8.0f);
	drawObject(skier, skierTexture, skierModel, 0.3f, 32.0f);
	for (const glm::vec3& lamp : LAMP_POSTS)
		drawObject(lampPost, lampPostTexture, onGround(lamp, 0.0f), 0.5f, 32.0f);

	// Tribunes : la structure, les sièges (une couleur par section), les guirlandes
	for (const glm::mat4& model : stands.models)
		drawObject(stand, standTexture, model, 0.1f, 16.0f);
	for (int k = 0; k < 3; k++)
		drawInstances(seats[k], seatTextures[k], 0.3f, 32.0f);
	drawObject(wires, wireTexture, glm::mat4(1.0f), 0.2f, 16.0f);

	// Décor (instancing)
	drawInstances(tree, treeTexture, 0.05f, 8.0f);
	drawInstances(rock, rockTexture, 0.1f, 16.0f);

	// Le public : son propre vertex shader (l'animation), le même éclairage
	prepareShader(crowdShader, view, projection, eye, skierModel);
	crowd.draw(crowdShader, t, skierState.position, cheerTime);

	// Ampoules des lampadaires : sphères de couleur unie (shader "bulb" du cours)
	bulbShader.use();
	bulbShader.setUniform("view", view);
	bulbShader.setUniform("projection", projection);
	bulbShader.setUniform("lightColor", glm::vec3(1.0f, 0.85f, 0.6f));
	for (const glm::vec3& lamp : LAMP_POSTS)
	{
		glm::mat4 model = glm::translate(onGround(lamp, 0.0f), glm::vec3(0.0f, LAMP_BULB_HEIGHT, 0.0f));
		bulbShader.setUniform("model", glm::scale(model, glm::vec3(0.8f)));
		bulb.draw();
	}

	// Vitres des projecteurs : allumées, donc d'une couleur unie très claire, sans
	// éclairage (même shader que les ampoules)
	bulbShader.setUniform("lightColor", FLOODLIGHT_LENS_COLOR);
	for (const Floodlight& light : floodlights)
	{
		bulbShader.setUniform("model", light.model);
		lenses.draw();
	}

	// Ampoules des guirlandes
	bulbShader.setUniform("lightColor", glm::vec3(1.0f, 0.88f, 0.65f));
	for (const glm::vec3& b : stands.bulbs)
	{
		bulbShader.setUniform("model", glm::scale(glm::translate(glm::mat4(1.0f), b), glm::vec3(GARLAND_BULB_SCALE)));
		bulb.draw();
	}

	// Le ciel : il ne remplit que les pixels où rien n'a été dessiné
	sky.draw(view, projection);

	// Les gerbes de neige, transparentes : après le ciel, pour se mélanger à l'image complète
	spray.draw(view, projection, eye, t);

	// Les halos des projecteurs et des ampoules en tout dernier : ils s'ajoutent à l'image finie
	glow.draw(view, projection, eye, glowSources);
}
