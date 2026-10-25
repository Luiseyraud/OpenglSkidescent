//-----------------------------------------------------------------------------
// World.h : tout ce qui compose la scène (terrain, ciel, modèles, lumières),
// son chargement et son dessin.
//-----------------------------------------------------------------------------
#ifndef WORLD_H
#define WORLD_H

#include "glm/glm.hpp"
#include "ShaderProgram.h"
#include "Texture2D.h"
#include "Mesh.h"
#include "Scene.h"
#include "Spline.h"
#include "Terrain.h"
#include "Sky.h"
#include "Course.h"
#include "Glow.h"
#include "SkiTrail.h"
#include "SnowSpray.h"
#include "Stands.h"
#include "Crowd.h"
#include "Snowfall.h"

class World
{
public:
	bool load();	// contexte OpenGL requis
	void render(const glm::mat4& view, const glm::mat4& projection, const glm::vec3& eye, float t);

	Spline piste{ scene::PISTE_POINTS };
	Terrain terrain{ piste };

private:
	void prepareShader(ShaderProgram& program, const glm::mat4& view, const glm::mat4& projection,
	                   const glm::vec3& eye, const glm::mat4& skierModel);
	void setLights(ShaderProgram& program, const glm::mat4& skierModel);
	glm::mat4 onGround(const glm::vec3& position, float angleDeg) const;
	void drawObject(Mesh& mesh, Texture2D& texture, const glm::mat4& model, float specular, float shininess);
	void drawInstances(Mesh& mesh, Texture2D& texture, float specular, float shininess);
	glm::vec3 floodlightAt(const glm::vec3& point) const;

	Sky sky;
	Glow glow;					// halos des projecteurs
	SkiTrail trail;				// trace des skis
	SnowSpray spray;			// gerbes de neige
	Snowfall snowfall;			// neige qui tombe
	ShaderProgram shader;		// la scène : Blinn-Phong, brouillard
	ShaderProgram bulbShader;	// les ampoules : couleur unie (shader du cours)
	ShaderProgram crowdShader;	// le public : crowd.vert (animation) + scene.frag (éclairage)
	ShaderProgram snowShader;	// la neige qui tombe : snow.vert (position, lumière) + snow.frag

	Texture2D snowTexture;
	Mesh chalet;	Texture2D chaletTexture;
	Mesh skier;		Texture2D skierTexture;
	Mesh lampPost;	Texture2D lampPostTexture;
	Mesh tree;		Texture2D treeTexture;
	Mesh rock;		Texture2D rockTexture;
	Mesh bulb;

	// Équipement de course (Course.h)
	Mesh lines;		Texture2D lineTexture;		// bandes bleues
	Mesh net;		Texture2D netTexture;		// filets
	Mesh netPoles;	Texture2D netPoleTexture;	// poteaux des filets
	Mesh flag;		Texture2D flagTexture;		// drapeaux
	Mesh pylon;		Texture2D pylonTexture;		// pylônes d'éclairage (mât et boîtiers)
	Mesh lenses;								// vitres des projecteurs, dessinées allumées
	std::vector<LightMast> masts;				// pylônes d'éclairage
	std::vector<Floodlight> floodlights;		// tous leurs faisceaux (2 par pylône)

	// Tribunes de l'arrivée (Stands.h) et public (Crowd.h)
	Stands stands;
	Mesh stand;		Texture2D standTexture;		// structure des tribunes
	Mesh seats[3];	Texture2D seatTextures[3];	// le même siège, une couleur par section
	Mesh wires;		Texture2D wireTexture;		// fils et poteaux des guirlandes
	Crowd crowd;
	float cheerTime = 0.0f;						// instant où le public se lève

	std::vector<GlowSource> glowSources;		// halos : projecteurs et ampoules des guirlandes
};

#endif // WORLD_H
