//-----------------------------------------------------------------------------
// Course.cpp : bandes bleues, filets, drapeaux et pylônes d'éclairage le long de la piste
//-----------------------------------------------------------------------------
#include "Course.h"
#include "Scene.h"
#include <cmath>
#include <random>
#include <algorithm>
#include "glm/gtc/matrix_transform.hpp"
#include "glm/gtc/constants.hpp"

using namespace scene;

// Texture du filet : une colonne de NET_CELLS_HIGH mailles en losange, de
// NET_CELL_PX pixels de côté, plus une bande pleine en haut (la corde du haut).
// Elle couvre toute la hauteur du filet, et se répète le long du filet.
const int   NET_CELL_PX    = 32;
const int   NET_CELLS_HIGH = 12;
const int   NET_BAND_PX    = 16;
const float NET_STRAND_PX  = 4.0f;	// épaisseur d'un fil, en pixels
const float BURY           = 0.1f;	// m : le bas des filets, des poteaux et des drapeaux est enfoncé dans la neige

//-----------------------------------------------------------------------------
// Direction horizontale perpendiculaire à la piste, vers la GAUCHE du skieur
// qui descend (la tangente tournée de 90° autour de Y)
//-----------------------------------------------------------------------------
static glm::vec3 leftOf(const Spline& piste, float s)
{
	glm::vec3 t = piste.tangentAt(s);
	return glm::normalize(glm::vec3(t.z, 0.0f, -t.x));
}

//-----------------------------------------------------------------------------
// Point au sol à côté de la piste : à la distance s, décalé de offset mètres
// sur le côté (offset > 0 : à gauche, offset < 0 : à droite)
//-----------------------------------------------------------------------------
static glm::vec3 sidePoint(const Spline& piste, const Terrain& terrain, float s, float offset)
{
	glm::vec3 p = piste.pointAt(s) + offset * leftOf(piste, s);
	p.y = terrain.height(p.x, p.z);
	return p;
}

//-----------------------------------------------------------------------------
// Courbure de la piste à la distance s : 1 / rayon du virage.
// Positive quand la piste tourne vers la gauche, négative vers la droite.
// Elle est mesurée sur 20 m : la spline a de brefs pics de courbure à ses
// points de contrôle, qui ne sont pas de vrais virages.
//-----------------------------------------------------------------------------
static float curvature(const Spline& piste, float s)
{
	glm::vec3 change = (piste.tangentAt(s + 10.0f) - piste.tangentAt(s - 10.0f)) / 20.0f;
	return glm::dot(change, leftOf(piste, s));
}

//-----------------------------------------------------------------------------
// Ajoute le quadrilatère abcd (2 triangles : abc et acd)
//-----------------------------------------------------------------------------
static void addQuad(std::vector<Vertex>& vertices, const Vertex& a, const Vertex& b, const Vertex& c, const Vertex& d)
{
	vertices.insert(vertices.end(), { a, b, c, a, c, d });
}

//-----------------------------------------------------------------------------
// Sommet d'une bande bleue : sur la neige, à côté de la piste. Sa hauteur est
// celle des triangles dessinés (meshHeight), un peu relevée (LINE_LIFT).
//-----------------------------------------------------------------------------
static Vertex lineVertex(const Spline& piste, const Terrain& terrain, float s, float offset)
{
	glm::vec3 p = piste.pointAt(s) + offset * leftOf(piste, s);
	p.y = terrain.meshHeight(p.x, p.z) + LINE_LIFT;
	return { p, terrain.normal(p.x, p.z), glm::vec2(0.0f) };	// couleur unie : UV sans importance
}

//-----------------------------------------------------------------------------
// Bandes bleues : de chaque côté, un ruban de LINE_WIDTH mètres de large.
// Tous les LINE_STEP mètres, on pose deux sommets sur la neige (bord intérieur
// et bord extérieur du ruban), et on les relie aux deux d'avant. Le pas est
// petit pour que le ruban suive les plis entre les triangles du terrain.
//-----------------------------------------------------------------------------
std::vector<Vertex> buildLines(const Spline& piste, const Terrain& terrain)
{
	const float LINE_STEP = 0.25f;
	int steps = (int)(piste.length() / LINE_STEP);

	std::vector<Vertex> vertices;
	for (float side : { 1.0f, -1.0f })
	{
		Vertex previousIn, previousOut;
		for (int i = 0; i <= steps; i++)
		{
			float s = i * LINE_STEP;
			Vertex in  = lineVertex(piste, terrain, s, side * (LINE_OFFSET - LINE_WIDTH / 2.0f));
			Vertex out = lineVertex(piste, terrain, s, side * (LINE_OFFSET + LINE_WIDTH / 2.0f));
			if (i > 0)
				addQuad(vertices, previousIn, in, out, previousOut);
			previousIn = in;
			previousOut = out;
		}
	}
	return vertices;
}

//-----------------------------------------------------------------------------
// Un poteau : une boîte de 8 cm de côté, de base à base + height
//-----------------------------------------------------------------------------
static void addPole(std::vector<Vertex>& vertices, const glm::vec3& base, float height)
{
	const float HALF = 0.04f;
	glm::vec3 corners[4] = { { -HALF, 0.0f, -HALF }, { HALF, 0.0f, -HALF }, { HALF, 0.0f, HALF }, { -HALF, 0.0f, HALF } };
	glm::vec3 top(0.0f, height, 0.0f);

	// 4 faces verticales : la normale va du centre du poteau vers le milieu de la face
	for (int i = 0; i < 4; i++)
	{
		glm::vec3 a = base + corners[i];
		glm::vec3 b = base + corners[(i + 1) % 4];
		glm::vec3 n = glm::normalize(corners[i] + corners[(i + 1) % 4]);
		addQuad(vertices, { a, n, glm::vec2(0.0f) }, { b, n, glm::vec2(0.0f) },
		                  { b + top, n, glm::vec2(0.0f) }, { a + top, n, glm::vec2(0.0f) });
	}

	// Le dessus
	glm::vec3 up(0.0f, 1.0f, 0.0f);
	addQuad(vertices, { base + top + corners[0], up, glm::vec2(0.0f) }, { base + top + corners[1], up, glm::vec2(0.0f) },
	                  { base + top + corners[2], up, glm::vec2(0.0f) }, { base + top + corners[3], up, glm::vec2(0.0f) });
}

//-----------------------------------------------------------------------------
// Un filet de s0 à s1 (distances sur la piste), du côté side (+1 gauche, -1 droite).
// Les poteaux sont régulièrement espacés. Entre deux poteaux, le filet est
// découpé en PIECES morceaux : il suit le sol, et son haut pend un peu.
//-----------------------------------------------------------------------------
static void addNet(const Spline& piste, const Terrain& terrain, float s0, float s1, float side,
                   std::vector<Vertex>& net, std::vector<Vertex>& poles)
{
	const int PIECES = 8;
	int spans = std::max(1, (int)std::round((s1 - s0) / NET_POLE_SPACING));

	// Taille d'une maille en mètres : la texture couvre toute la hauteur du filet
	float metersPerPixel = (NET_HEIGHT + BURY) / (NET_CELL_PX * NET_CELLS_HIGH + NET_BAND_PX);
	float cell = NET_CELL_PX * metersPerPixel;

	float length = 0.0f;	// longueur de filet déjà posée (pour la coordonnée u de la texture)
	glm::vec3 previousBase;
	Vertex previousBottom, previousTop;
	for (int i = 0; i <= spans * PIECES; i++)
	{
		float s = glm::mix(s0, s1, (float)i / (spans * PIECES));
		glm::vec3 base = sidePoint(piste, terrain, s, side * NET_OFFSET);
		if (i > 0)
			length += glm::length(base - previousBase);

		// 0 sur un poteau, puis jusqu'à 1 au poteau suivant : le filet pend au milieu
		float between = (float)(i % PIECES) / PIECES;
		float top = NET_HEIGHT - NET_SAG * std::sin(glm::pi<float>() * between);

		// La normale est horizontale, perpendiculaire à la piste. Le shader la retourne
		// quand on voit le filet de l'autre côté (uniform twoSided).
		glm::vec3 normal = side * leftOf(piste, s);
		Vertex bottom = { base - glm::vec3(0.0f, BURY, 0.0f), normal, glm::vec2(length / cell, 0.0f) };
		Vertex up     = { base + glm::vec3(0.0f, top, 0.0f),      normal, glm::vec2(length / cell, 1.0f) };

		if (i > 0)
			addQuad(net, previousBottom, bottom, up, previousTop);

		// Un poteau tous les PIECES morceaux, juste derrière le filet (côté extérieur)
		if (i % PIECES == 0)
			addPole(poles, base + 0.06f * normal - glm::vec3(0.0f, 0.3f, 0.0f), NET_HEIGHT + 0.35f);

		previousBase = base;
		previousBottom = bottom;
		previousTop = up;
	}
}

//-----------------------------------------------------------------------------
// Élargit chaque zone "vrai" de extra cases avant et après
//-----------------------------------------------------------------------------
static std::vector<bool> widen(const std::vector<bool>& zones, int extra)
{
	int count = (int)zones.size();
	std::vector<bool> result(count, false);
	for (int i = 0; i < count; i++)
		if (zones[i])
			for (int j = std::max(0, i - extra); j <= std::min(count - 1, i + extra); j++)
				result[j] = true;
	return result;
}

//-----------------------------------------------------------------------------
// Filets : à l'extérieur des virages serrés (rayon < 1 / NET_MIN_CURVATURE)
//-----------------------------------------------------------------------------
void buildNets(const Spline& piste, const Terrain& terrain, std::vector<Vertex>& net, std::vector<Vertex>& poles)
{
	// 1. Pour chaque mètre de piste : faut-il un filet à gauche ? à droite ?
	//    Le filet va à l'extérieur du virage : à droite quand la piste tourne à gauche.
	int count = (int)piste.length() + 1;
	std::vector<bool> left(count, false), right(count, false);
	for (int i = 0; i < count; i++)
	{
		float k = curvature(piste, (float)i);
		if (k > NET_MIN_CURVATURE)
			right[i] = true;
		if (k < -NET_MIN_CURVATURE)
			left[i] = true;
	}

	// 2. Le filet commence un peu avant le virage et finit un peu après
	left = widen(left, (int)NET_EXTRA);
	right = widen(right, (int)NET_EXTRA);

	// 3. Un filet pour chaque suite continue de mètres où il en faut un
	for (float side : { 1.0f, -1.0f })
	{
		const std::vector<bool>& needed = (side > 0.0f) ? left : right;
		int i = 0;
		while (i < count)
		{
			if (!needed[i]) { i++; continue; }
			int start = i;
			while (i < count && needed[i])
				i++;
			addNet(piste, terrain, (float)start, (float)(i - 1), side, net, poles);
		}
	}
}

//-----------------------------------------------------------------------------
// Drapeaux : tous les FLAG_SPACING mètres, sur les deux bords
//-----------------------------------------------------------------------------
std::vector<glm::mat4> placeFlags(const Spline& piste, const Terrain& terrain)
{
	std::mt19937 rng(DECOR_SEED + 2);
	std::uniform_real_distribution<float> jitter(-0.3f, 0.3f);	// radians (± 17°)

	std::vector<glm::mat4> flags;
	for (float side : { 1.0f, -1.0f })
		for (float s = 0.0f; s <= piste.length(); s += FLAG_SPACING)
		{
			glm::vec3 p = sidePoint(piste, terrain, s, side * FLAG_OFFSET);

			// Le fanion du modèle flotte vers -Z. On le tourne vers +X, comme poussé par un
			// vent d'ouest : la caméra qui suit le skieur (vers +Z) le voit de face.
			float angle = -glm::half_pi<float>() + jitter(rng);
			flags.push_back(glm::translate(glm::mat4(1.0f), p - glm::vec3(0.0f, BURY, 0.0f))
			              * glm::rotate(glm::mat4(1.0f), angle, glm::vec3(0.0f, 1.0f, 0.0f)));
		}
	return flags;
}

//-----------------------------------------------------------------------------
// Pylônes d'éclairage : une paire tous les FLOODLIGHT_SPACING mètres. Les
// 15 projecteurs d'un pylône sont réglés en deux groupes (deux faisceaux) : l'un
// éclaire la piste en face (FLOODLIGHT_AIM_NEAR), l'autre plus bas
// (FLOODLIGHT_AIM_FAR). Chaque bord de la piste a ainsi une tache tous les 30 m.
// Le pylône est tourné vers le milieu de ses deux points visés.
//
// Pour que toutes les taches se ressemblent (même taille, même luminosité),
// chaque faisceau est réglé selon sa distance d au point visé :
//  - son cône a pour demi-angle atan(R / d) : un rayon R à l'endroit visé ;
//  - sa puissance est FLOODLIGHT_INTENSITY x (1 + k d²) : elle compense
//    exactement l'atténuation 1 / (1 + k d²) du shader au point visé.
// Le faisceau qui vise loin est donc plus étroit et plus puissant.
//-----------------------------------------------------------------------------
std::vector<LightMast> placeLightMasts(const Spline& piste, const Terrain& terrain)
{
	std::vector<LightMast> masts;
	for (float s = FLOODLIGHT_FIRST; s < piste.length(); s += FLOODLIGHT_SPACING)
		for (float side : { 1.0f, -1.0f })
		{
			glm::vec3 base = sidePoint(piste, terrain, s, side * FLOODLIGHT_OFFSET);

			// Points visés : sur l'axe de la piste, en face et plus bas
			glm::vec3 targets[2] = { sidePoint(piste, terrain, s + FLOODLIGHT_AIM_NEAR, 0.0f),
			                         sidePoint(piste, terrain, s + FLOODLIGHT_AIM_FAR, 0.0f) };

			// Rotation autour de Y : l'avant du modèle (+X) regarde vers le milieu des
			// deux points visés. rotate(angle, Y) envoie (1, 0, 0) sur (cos angle, 0, -sin angle).
			glm::vec3 facing = 0.5f * (targets[0] + targets[1]) - base;
			float angle = std::atan2(-facing.z, facing.x);

			LightMast mast;
			mast.model = glm::translate(glm::mat4(1.0f), base - glm::vec3(0.0f, 0.3f, 0.0f))	// enfoncé de 30 cm
			           * glm::rotate(glm::mat4(1.0f), angle, glm::vec3(0.0f, 1.0f, 0.0f));
			for (int k = 0; k < 2; k++)
			{
				Floodlight& beam = mast.beams[k];
				beam.position = glm::vec3(mast.model * glm::vec4(FLOODLIGHT_HEAD, 1.0f));
				beam.direction = glm::normalize(targets[k] - beam.position);

				float d = glm::length(targets[k] - beam.position);
				float outer = std::atan(FLOODLIGHT_POOL_RADIUS / d);
				beam.cosOuter = std::cos(outer);
				beam.cosInner = std::cos(FLOODLIGHT_INNER_RATIO * outer);
				beam.power = FLOODLIGHT_INTENSITY * (1.0f + FLOODLIGHT_QUADRATIC * d * d);
			}
			masts.push_back(mast);
		}
	return masts;
}

//-----------------------------------------------------------------------------
// Texture du filet. Dans une maille (u, v de 0 à 1), les fils suivent les deux
// diagonales u + v = entier et u - v = entier : on obtient des losanges.
// Un pixel est opaque (alpha = 1) sur un fil, transparent (alpha = 0) entre.
// Les pixels transparents gardent la couleur du filet : en réduisant l'image
// (mipmaps), le mélange ne fonce pas la couleur.
//-----------------------------------------------------------------------------
std::vector<unsigned char> netPixels(int& width, int& height)
{
	width = NET_CELL_PX;
	height = NET_CELL_PX * NET_CELLS_HIGH + NET_BAND_PX;

	std::vector<unsigned char> pixels;
	for (int y = 0; y < height; y++)
		for (int x = 0; x < width; x++)
		{
			float alpha = 1.0f;		// la bande du haut est pleine
			if (y < NET_CELL_PX * NET_CELLS_HIGH)
			{
				float u = (x + 0.5f) / NET_CELL_PX;
				float v = (y + 0.5f) / NET_CELL_PX;
				// Distance (en pixels) au fil le plus proche
				float d1 = std::fabs(glm::fract(u + v + 0.5f) - 0.5f);
				float d2 = std::fabs(glm::fract(u - v + 0.5f) - 0.5f);
				float d = std::min(d1, d2) / std::sqrt(2.0f) * NET_CELL_PX;
				// Opaque sur le fil, transparent à côté, avec un bord adouci sur 1 pixel
				alpha = glm::clamp(NET_STRAND_PX / 2.0f - d + 0.5f, 0.0f, 1.0f);
			}
			pixels.insert(pixels.end(), { NET_COLOR.r, NET_COLOR.g, NET_COLOR.b, (unsigned char)std::lround(alpha * 255.0f) });
		}
	return pixels;
}
