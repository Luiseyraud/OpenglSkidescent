//-----------------------------------------------------------------------------
// Stands.cpp : tribunes, places et guirlandes
//-----------------------------------------------------------------------------
#include "Stands.h"
#include "Scene.h"
#include <fstream>
#include <sstream>
#include <iostream>
#include "glm/gtc/matrix_transform.hpp"

using namespace scene;

//-----------------------------------------------------------------------------
// Ajoute un quadrilatère abcd (2 triangles), de normale n
//-----------------------------------------------------------------------------
static void addQuad(std::vector<Vertex>& v, glm::vec3 a, glm::vec3 b, glm::vec3 c, glm::vec3 d, glm::vec3 n)
{
	v.insert(v.end(), { { a, n, glm::vec2(0.0f) }, { b, n, glm::vec2(0.0f) }, { c, n, glm::vec2(0.0f) },
	                    { a, n, glm::vec2(0.0f) }, { c, n, glm::vec2(0.0f) }, { d, n, glm::vec2(0.0f) } });
}

//-----------------------------------------------------------------------------
// Un segment de fil de a à b : deux bandes de 2 cm en croix (une verticale,
// une horizontale), pour qu'il se voie sous tous les angles
//-----------------------------------------------------------------------------
static void addWire(std::vector<Vertex>& v, glm::vec3 a, glm::vec3 b)
{
	glm::vec3 along = glm::normalize(b - a);
	glm::vec3 side = glm::normalize(glm::cross(along, glm::vec3(0.0f, 1.0f, 0.0f))) * 0.01f;
	glm::vec3 up(0.0f, 0.01f, 0.0f);
	addQuad(v, a - up, b - up, b + up, a + up, glm::normalize(side));
	addQuad(v, a - side, b - side, b + side, a + side, glm::vec3(0.0f, 1.0f, 0.0f));
}

//-----------------------------------------------------------------------------
// Un poteau : une boîte de 8 cm de côté, de base à base + height
//-----------------------------------------------------------------------------
static void addPost(std::vector<Vertex>& v, glm::vec3 base, float height, const glm::mat4& model)
{
	const float H = 0.04f;
	glm::vec3 c[4] = { { -H, 0.0f, -H }, { H, 0.0f, -H }, { H, 0.0f, H }, { -H, 0.0f, H } };
	glm::vec3 top(0.0f, height, 0.0f);
	auto world = [&](glm::vec3 p) { return glm::vec3(model * glm::vec4(p, 1.0f)); };
	for (int i = 0; i < 4; i++)
	{
		glm::vec3 n = glm::normalize(glm::vec3(model * glm::vec4(c[i] + c[(i + 1) % 4], 0.0f)));
		addQuad(v, world(base + c[i]), world(base + c[(i + 1) % 4]), world(base + c[(i + 1) % 4] + top), world(base + c[i] + top), n);
	}
}

//-----------------------------------------------------------------------------
// Construit les tribunes
//-----------------------------------------------------------------------------
bool buildStands(const Spline& piste, const Terrain& terrain, const std::string& seatsFile, Stands& stands)
{
	// 1. Places, dans le repère de la tribune
	std::ifstream file(seatsFile);
	if (!file)
	{
		std::cerr << "Fichier des places introuvable : " << seatsFile << std::endl;
		return false;
	}
	struct LocalSeat { glm::vec3 foot; int section; };
	std::vector<LocalSeat> seats;
	std::string line;
	while (std::getline(file, line))
	{
		if (line.empty() || line[0] == '#')
			continue;
		std::istringstream in(line);
		LocalSeat seat;
		in >> seat.foot.x >> seat.foot.y >> seat.foot.z >> seat.section;
		seats.push_back(seat);
	}

	// 2. Les deux tribunes : sur le prolongement de la piste, STAND_ALONG mètres après
	// sa fin, de chaque côté. Le modèle regarde vers +Z ; on le tourne vers l'axe.
	glm::vec3 tangent = piste.tangentAt(piste.length());
	glm::vec3 left(tangent.z, 0.0f, -tangent.x);
	glm::vec3 center = piste.pointAt(piste.length()) + STAND_ALONG * tangent;
	for (float side : { -1.0f, 1.0f })
	{
		glm::vec3 position = center + side * (STAND_OFFSET + STAND_HALF_DEPTH) * left;
		position.y = terrain.height(position.x, position.z);
		glm::vec3 facing = -side * left;							// vers l'axe de la piste
		float yaw = std::atan2(facing.x, facing.z);				// rotate(yaw, Y) envoie +Z sur (sin, 0, cos)
		glm::mat4 model = glm::translate(glm::mat4(1.0f), position)
		                * glm::rotate(glm::mat4(1.0f), yaw, glm::vec3(0.0f, 1.0f, 0.0f));
		stands.models.push_back(model);

		// Les places : le siège, et le spectateur debout 35 cm devant
		for (const LocalSeat& seat : seats)
		{
			Place place;
			place.seat = glm::translate(model, seat.foot);
			place.feet = glm::vec3(model * glm::vec4(seat.foot + glm::vec3(0.0f, 0.0f, 0.35f), 1.0f));
			place.yaw = yaw;
			place.along = seat.foot.x;
			place.section = seat.section;
			stands.places.push_back(place);
		}

		// La guirlande : des poteaux devant la tribune, un fil qui pend entre eux
		// (parabole) au-dessus du public, des ampoules tous les GARLAND_SPACING mètres.
		// Devant, sa lumière éclaire les visages (le soleil couchant est souvent derrière).
		const float FRONT = STAND_HALF_DEPTH + 0.3f;				// z, juste devant la tribune
		const float HALF_LENGTH = 9.0f;
		auto world = [&](glm::vec3 p) { return glm::vec3(model * glm::vec4(p, 1.0f)); };
		for (int i = 0; i < GARLAND_POSTS; i++)
		{
			float x = -HALF_LENGTH + 2.0f * HALF_LENGTH * i / (GARLAND_POSTS - 1);
			addPost(stands.wires, glm::vec3(x, 0.0f, FRONT), GARLAND_HEIGHT, model);
			if (i == GARLAND_POSTS - 1)
				break;

			float x1 = -HALF_LENGTH + 2.0f * HALF_LENGTH * (i + 1) / (GARLAND_POSTS - 1);
			auto wire = [&](float f) {			// point du fil, f de 0 (poteau i) à 1 (poteau i + 1)
				return glm::vec3(glm::mix(x, x1, f), GARLAND_HEIGHT - 0.05f - GARLAND_SAG * 4.0f * f * (1.0f - f), FRONT);
			};
			const int SEGMENTS = 12;
			for (int k = 0; k < SEGMENTS; k++)
				addWire(stands.wires, world(wire((float)k / SEGMENTS)), world(wire((float)(k + 1) / SEGMENTS)));

			int count = (int)((x1 - x) / GARLAND_SPACING);
			for (int k = 1; k < count; k++)
				stands.bulbs.push_back(world(wire((float)k / count) - glm::vec3(0.0f, 0.07f, 0.0f)));
		}

		// 3 lumières ponctuelles par tribune, au niveau de la guirlande
		for (float x : { -6.0f, 0.0f, 6.0f })
			stands.lights.push_back(world(glm::vec3(x, GARLAND_HEIGHT - 0.4f, FRONT)));
	}

	std::cout << "Tribunes : " << stands.places.size() << " places, " << stands.bulbs.size() << " ampoules" << std::endl;
	return true;
}
