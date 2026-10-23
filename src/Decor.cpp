//-----------------------------------------------------------------------------
// Decor.cpp : placement des sapins et des rochers
//-----------------------------------------------------------------------------
#include "Decor.h"
#include "Scene.h"
#include <random>
#include "glm/gtc/matrix_transform.hpp"
#include "glm/gtc/noise.hpp"
#include "glm/gtc/constants.hpp"

using namespace scene;

//-----------------------------------------------------------------------------
// L'endroit (x, z) est-il libre pour un élément du décor ?
//-----------------------------------------------------------------------------
static bool isFreeSpot(const Terrain& terrain, float x, float z, const std::vector<glm::vec3>& keepClear)
{
	// Pas sur la piste (on laisse 5 m de bord dégagé)
	if (terrain.distanceToPiste(x, z) < PISTE_HALF_WIDTH + 5.0f)
		return false;

	// Pas sur le chalet ni sur les lampadaires (le skieur, lui, reste sur la piste)
	glm::vec2 p(x, z);
	if (glm::length(p - glm::vec2(CHALET_POS.x, CHALET_POS.z)) < 22.0f)
		return false;
	for (const glm::vec3& lamp : LAMP_POSTS)
		if (glm::length(p - glm::vec2(lamp.x, lamp.z)) < 4.0f)
			return false;

	// Ni sur une zone aplanie (chalet, aire d'arrivée et tribunes)
	for (const FlatZone& zone : FLAT_ZONES)
		if (glm::length(p - glm::vec2(zone.x, zone.z)) < zone.radius + 2.0f)
			return false;

	// Ni contre un pylône d'éclairage
	for (const glm::vec3& foot : keepClear)
		if (glm::length(p - glm::vec2(foot.x, foot.z)) < 6.0f)
			return false;

	// Couloir dégagé à l'ouest du tremplin : rien ne doit cacher le soleil
	// dans le plan en contre-jour du backflip (la caméra regarde vers -X)
	if (x < 40.0f && std::fabs(z - (JUMP_Z + 10.0f)) < 40.0f)
		return false;

	return true;
}

//-----------------------------------------------------------------------------
// Matrice model : position, rotation autour de Y, taille
//-----------------------------------------------------------------------------
static glm::mat4 placement(float x, float y, float z, float angle, float size)
{
	return glm::translate(glm::mat4(1.0f), glm::vec3(x, y, z))
	     * glm::rotate(glm::mat4(1.0f), angle, glm::vec3(0.0f, 1.0f, 0.0f))
	     * glm::scale(glm::mat4(1.0f), glm::vec3(size));
}

//-----------------------------------------------------------------------------
// Sapins : en bosquets, sous la limite des arbres, sur des pentes raisonnables
//-----------------------------------------------------------------------------
std::vector<glm::mat4> placeTrees(const Terrain& terrain, const std::vector<glm::vec3>& keepClear)
{
	std::mt19937 rng(DECOR_SEED);
	std::uniform_real_distribution<float> randomX(DECOR_X_MIN, DECOR_X_MAX);
	std::uniform_real_distribution<float> randomZ(DECOR_Z_MIN, DECOR_Z_MAX);
	std::uniform_real_distribution<float> random01(0.0f, 1.0f);

	std::vector<glm::mat4> trees;
	for (int attempt = 0; attempt < 50 * TREE_COUNT && (int)trees.size() < TREE_COUNT; attempt++)
	{
		float x = randomX(rng);
		float z = randomZ(rng);
		float chance = random01(rng);
		float angle = random01(rng) * glm::two_pi<float>();
		float size = glm::mix(TREE_MIN_HEIGHT, TREE_MAX_HEIGHT, random01(rng));

		if (!isFreeSpot(terrain, x, z, keepClear))
			continue;
		float h = terrain.height(x, z);
		if (terrain.normal(x, z).y < 0.8f)		// trop raide (plus de 37°)
			continue;

		// Probabilité de garder l'arbre : forte dans les "bosquets" (là où un bruit
		// est positif), nulle au-dessus de la limite des arbres
		float forest = glm::smoothstep(-0.2f, 0.4f, glm::simplex(glm::vec2(x, z) / 200.0f));
		float altitude = 1.0f - glm::smoothstep(TREE_LINE - 60.0f, TREE_LINE, h);
		if (chance > forest * altitude)
			continue;

		// Un peu enfoncé, pour ne pas flotter sur une pente
		trees.push_back(placement(x, h - 0.4f, z, angle, size));
	}
	return trees;
}

//-----------------------------------------------------------------------------
// Rochers : surtout sur les pentes raides, à moitié enfoncés dans la neige
//-----------------------------------------------------------------------------
std::vector<glm::mat4> placeRocks(const Terrain& terrain, const std::vector<glm::vec3>& keepClear)
{
	std::mt19937 rng(DECOR_SEED + 1);
	std::uniform_real_distribution<float> randomX(DECOR_X_MIN, DECOR_X_MAX);
	std::uniform_real_distribution<float> randomZ(DECOR_Z_MIN, DECOR_Z_MAX);
	std::uniform_real_distribution<float> random01(0.0f, 1.0f);

	std::vector<glm::mat4> rocks;
	for (int attempt = 0; attempt < 50 * ROCK_COUNT && (int)rocks.size() < ROCK_COUNT; attempt++)
	{
		float x = randomX(rng);
		float z = randomZ(rng);
		float chance = random01(rng);
		float angle = random01(rng) * glm::two_pi<float>();
		float size = glm::mix(ROCK_MIN_SIZE, ROCK_MAX_SIZE, random01(rng) * random01(rng));	// plus de petits que de gros

		if (!isFreeSpot(terrain, x, z, keepClear))
			continue;

		// Plus la pente est raide, plus on a de chances de garder le rocher
		float steep = glm::smoothstep(0.95f, 0.7f, terrain.normal(x, z).y);
		if (chance > 0.1f + 0.9f * steep)
			continue;

		rocks.push_back(placement(x, terrain.height(x, z) - 0.3f * size, z, angle, size));
	}
	return rocks;
}
