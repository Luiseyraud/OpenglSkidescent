//-----------------------------------------------------------------------------
// Terrain.cpp : la montagne, générée par le code
//-----------------------------------------------------------------------------
#include "Terrain.h"
#include "Scene.h"
#include "Mesh.h"	// pour la structure Vertex du cours
#include <iostream>
#include <algorithm>
#include <cmath>
#include "glm/gtc/noise.hpp"

using namespace scene;

const float TABLE_STEP = 0.5f;	// m, pas des tables indexées par z

//-----------------------------------------------------------------------------
// Pente du profil en z : interpolation linéaire entre les clés de SLOPE_PROFILE
//-----------------------------------------------------------------------------
static float slopeAt(float z)
{
	if (z <= SLOPE_PROFILE.front().z) return SLOPE_PROFILE.front().slope;
	if (z >= SLOPE_PROFILE.back().z)  return SLOPE_PROFILE.back().slope;

	size_t k = 1;
	while (SLOPE_PROFILE[k].z < z)
		k++;

	const SlopeKey& a = SLOPE_PROFILE[k - 1];
	const SlopeKey& b = SLOPE_PROFILE[k];
	return glm::mix(a.slope, b.slope, (z - a.z) / (b.z - a.z));
}

//-----------------------------------------------------------------------------
// Moyenne glissante d'une table indexée par z, sur ±radius mètres
//-----------------------------------------------------------------------------
static std::vector<float> smoothTable(const std::vector<float>& table, float radius)
{
	int n = (int)table.size();
	int w = (int)(radius / TABLE_STEP);
	std::vector<float> result(n);
	for (int k = 0; k < n; k++)
	{
		int first = std::max(k - w, 0);
		int last = std::min(k + w, n - 1);
		float sum = 0.0f;
		for (int i = first; i <= last; i++)
			sum += table[i];
		result[k] = sum / (last - first + 1);
	}
	return result;
}

//-----------------------------------------------------------------------------
// Coordonnées des lignes de la grille sur un axe : un pas de TERRAIN_CELL entre
// fineMin et fineMax, puis un pas qui grandit de TERRAIN_CELL_GROWTH à chaque
// case (jusqu'à TERRAIN_MAX_CELL) en allant vers les bords min et max.
// Le détail est là où la caméra passe près du sol ; au loin, de grandes cases suffisent.
//-----------------------------------------------------------------------------
static std::vector<float> gridCoordinates(float min, float fineMin, float fineMax, float max)
{
	// Vers le bord min : on part de fineMin et on recule
	std::vector<float> coords;
	float step = TERRAIN_CELL;
	for (float v = fineMin; v > min; )
	{
		step = std::min(step * TERRAIN_CELL_GROWTH, TERRAIN_MAX_CELL);
		v -= step;
		coords.push_back(std::max(v, min));
	}
	std::reverse(coords.begin(), coords.end());

	// Zone fine
	for (float v = fineMin; v <= fineMax; v += TERRAIN_CELL)
		coords.push_back(v);

	// Vers le bord max
	step = TERRAIN_CELL;
	for (float v = coords.back(); v < max; )
	{
		step = std::min(step * TERRAIN_CELL_GROWTH, TERRAIN_MAX_CELL);
		v += step;
		coords.push_back(std::min(v, max));
	}
	return coords;
}

//-----------------------------------------------------------------------------
// Bruit fractal (fBm) : 5 couches de bruit simplex, chacune 2 fois plus fine et
// NOISE_PERSISTENCE fois plus faible que la précédente. Résultat entre -1 et 1 environ.
//-----------------------------------------------------------------------------
static float fbm(glm::vec2 p)
{
	float sum = 0.0f;
	float amplitude = 0.6f;
	for (int i = 0; i < 5; i++)
	{
		sum += amplitude * glm::simplex(p);
		p *= 2.0f;
		amplitude *= NOISE_PERSISTENCE;
	}
	return sum;
}

//-----------------------------------------------------------------------------
// Relief naturel autour de la piste. d = distance à l'axe de la piste.
//-----------------------------------------------------------------------------
static float relief(float x, float z, float d)
{
	// Bruit : faible près de la piste, fort loin d'elle
	float amplitude = glm::mix(NOISE_NEAR, NOISE_FAR, glm::smoothstep(40.0f, 300.0f, d));
	float r = amplitude * fbm(glm::vec2(x, z) / NOISE_SCALE);

	// Versants de la vallée : le sol remonte quand on s'éloigne de la piste,
	// puis continue à monter doucement au-delà (montagnes à l'arrière-plan)
	r += VALLEY_HEIGHT * glm::smoothstep(VALLEY_START, VALLEY_END, d);
	r += VALLEY_RISE * std::max(d - VALLEY_END, 0.0f);

	// Bosses posées à la main (courbes en cloche)
	for (const Bump& b : BUMPS)
	{
		float dx = x - b.x;
		float dz = z - b.z;
		r += b.height * std::exp(-(dx * dx + dz * dz) / (b.radius * b.radius));
	}
	return r;
}

//-----------------------------------------------------------------------------
// Constructeur : remplit les tables et calcule la forme du tremplin
//-----------------------------------------------------------------------------
Terrain::Terrain(const Spline& piste)
	: mPisteLength(piste.length()), mVAO(0), mVBO(0), mEBO(0), mIndexCount(0)
{
	int n = (int)((TERRAIN_Z_MAX - TERRAIN_Z_MIN) / TABLE_STEP) + 1;

	//--- 1. Profil de pente : on descend de "pente x pas" à chaque pas ----------
	mBase.resize(n);
	mBase[0] = 0.0f;
	for (int k = 1; k < n; k++)
	{
		float zMiddle = TERRAIN_Z_MIN + (k - 0.5f) * TABLE_STEP;
		mBase[k] = mBase[k - 1] - slopeAt(zMiddle) * TABLE_STEP;
	}

	// On décale tout pour que l'arrivée de la piste soit à l'altitude 0
	float finishHeight = lookup(mBase, PISTE_POINTS.back().z);
	for (float& h : mBase)
		h -= finishHeight;

	//--- 2. Tables de la piste : on l'échantillonne tous les 25 cm --------------
	std::vector<glm::vec3> points;
	std::vector<float> distances;
	for (float s = 0.0f; s < piste.length(); s += 0.25f)
	{
		points.push_back(piste.pointAt(s));
		distances.push_back(s);
	}
	points.push_back(piste.pointAt(piste.length()));
	distances.push_back(piste.length());

	for (size_t i = 1; i < points.size(); i++)
	{
		if (points[i].z <= points[i - 1].z)
		{
			std::cerr << "Erreur : la piste doit toujours descendre vers +Z (z = "
			          << points[i].z << ")" << std::endl;
			break;
		}
	}

	// Direction de la piste à ses deux bouts (tangente et cosinus de l'angle avec Z)
	size_t last = points.size() - 1;
	float tanStart = (points[1].x - points[0].x) / (points[1].z - points[0].z);
	float cosStart = (points[1].z - points[0].z) / (distances[1] - distances[0]);
	float tanEnd = (points[last].x - points[last - 1].x) / (points[last].z - points[last - 1].z);
	float cosEnd = (points[last].z - points[last - 1].z) / (distances[last] - distances[last - 1]);

	mPisteX.resize(n);
	mPisteS.resize(n);
	mPisteCos.resize(n);
	std::vector<float> clampedX(n);	// x de la piste, figé au-delà des bouts (pour l'axe de la vallée)
	size_t j = 0;
	for (int k = 0; k < n; k++)
	{
		float z = TERRAIN_Z_MIN + k * TABLE_STEP;

		if (z <= points.front().z)		// avant le départ : on prolonge en ligne droite
		{
			float dz = z - points.front().z;
			mPisteX[k] = points.front().x + dz * tanStart;
			mPisteS[k] = dz / cosStart;
			mPisteCos[k] = cosStart;
			clampedX[k] = points.front().x;
		}
		else if (z >= points.back().z)	// après l'arrivée : idem
		{
			float dz = z - points.back().z;
			mPisteX[k] = points.back().x + dz * tanEnd;
			mPisteS[k] = piste.length() + dz / cosEnd;
			mPisteCos[k] = cosEnd;
			clampedX[k] = points.back().x;
		}
		else
		{
			// On avance jusqu'aux deux échantillons qui encadrent z, puis on interpole
			while (points[j + 1].z < z)
				j++;
			float a = (z - points[j].z) / (points[j + 1].z - points[j].z);
			mPisteX[k] = glm::mix(points[j].x, points[j + 1].x, a);
			mPisteS[k] = glm::mix(distances[j], distances[j + 1], a);
			mPisteCos[k] = (points[j + 1].z - points[j].z) / (distances[j + 1] - distances[j]);
			clampedX[k] = mPisteX[k];
		}
	}

	//--- 3. Versions lissées, pour le terrain loin de la piste ----------------
	// Axe de la vallée = la piste lissée : il ne suit pas chaque virage, ce qui
	// évite de déformer les versants.
	mValleyX = smoothTable(clampedX, VALLEY_SMOOTHING);

	// Profil de pente lissé : le replat du tremplin ne fait pas de marche sur
	// toute la largeur de la vallée.
	mBaseSmooth = smoothTable(mBase, PROFILE_SMOOTHING);

	//--- 4. Tremplin ----------------------------------------------------------
	float theta = glm::radians(JUMP_ANGLE_DEG);
	float alpha = slopeAt(JUMP_Z);	// pente du replat sous le tremplin

	// Bosse d'élan en forme de parabole H * ((u + L) / L)^2 : sa pente au bord
	// vaut 2H/L par rapport au replat. On veut tan(theta) par rapport à l'horizontale.
	mKickerHeight = (std::tan(theta) + alpha) * KICKER_LENGTH / 2.0f;
	mJumpS = lookup(mPisteS, JUMP_Z);
	mLipY = lookup(mBase, JUMP_Z) + mKickerHeight;

	// Trajectoire prévue après le bord (voir jumpTrajectory). Le skieur avance
	// à vitesse HORIZONTALE constante SKIER_SPEED, donc vx = SKIER_SPEED.
	float vx = SKIER_SPEED;
	float vy = SKIER_SPEED * std::tan(theta);
	float T = JUMP_AIR_TIME;
	mLandingU = vx * T;											// distance de l'impact
	float landingY = vy * T - 0.5f * GRAVITY * T * T;			// hauteur de l'impact (par rapport au bord)
	float trajectorySlope = (vy - GRAVITY * T) / vx;			// pente de la trajectoire à l'impact

	// Zone de réception : une droite qui passe par le point d'impact, un peu moins
	// raide que la trajectoire. Elle part de la table (à -TABLE_DROP sous le bord).
	mLandingSlope = LANDING_RATIO * trajectorySlope;
	mTableEnd = mLandingU - (landingY + TABLE_DROP) / mLandingSlope;

	// Fin du tremplin : la zone de réception redescend jusqu'à la piste sur
	// JUMP_RUNOUT mètres après l'impact (voir jumpHeight).
	mJumpEnd = mLandingU + JUMP_RUNOUT;

	// Vérification : le skieur doit passer au-dessus de la fin de la table
	float yAtTableEnd = jumpTrajectory(mTableEnd) - mLipY;
	std::cout << "Tremplin : bosse de " << mKickerHeight << " m, table de " << mTableEnd
	          << " m, impact à " << mLandingU << " m, fin à " << mJumpEnd << " m, pente de réception "
	          << glm::degrees(std::atan(-mLandingSlope)) << " deg" << std::endl;
	if (yAtTableEnd < -TABLE_DROP + 0.3f)
		std::cerr << "Attention : le skieur passe trop près du bout de la table" << std::endl;

	//--- 5. Zones aplanies : hauteur du terrain naturel au centre de chacune ---
	for (const FlatZone& zone : FLAT_ZONES)
		mFlatHeights.push_back(naturalHeight(zone.x, zone.z));
}

//-----------------------------------------------------------------------------
// Destructeur
//-----------------------------------------------------------------------------
Terrain::~Terrain()
{
	if (mVAO == 0)		// maillage jamais construit : rien à libérer sur la carte graphique
		return;
	glDeleteVertexArrays(1, &mVAO);
	glDeleteBuffers(1, &mVBO);
	glDeleteBuffers(1, &mEBO);
}

//-----------------------------------------------------------------------------
// Lecture d'une table indexée par z, avec interpolation linéaire
//-----------------------------------------------------------------------------
float Terrain::lookup(const std::vector<float>& table, float z) const
{
	float f = (z - TERRAIN_Z_MIN) / TABLE_STEP;
	f = glm::clamp(f, 0.0f, (float)(table.size() - 1));
	int k = std::min((int)f, (int)table.size() - 2);
	return glm::mix(table[k], table[k + 1], f - k);
}

//-----------------------------------------------------------------------------
// Hauteur du skieur en vol, u mètres après le bord du tremplin : une parabole.
// Il décolle à la vitesse horizontale vx = SKIER_SPEED, avec l'angle JUMP_ANGLE_DEG :
//   y(u) = yBord + tan(theta) u - g u² / (2 vx²)
//-----------------------------------------------------------------------------
float Terrain::jumpTrajectory(float u) const
{
	float theta = glm::radians(JUMP_ANGLE_DEG);
	return mLipY + std::tan(theta) * u - GRAVITY * u * u / (2.0f * SKIER_SPEED * SKIER_SPEED);
}

//-----------------------------------------------------------------------------
// Hauteur du tremplin. u = distance après le bord, base = hauteur de la pente.
//-----------------------------------------------------------------------------
float Terrain::jumpHeight(float u, float base) const
{
	if (u < -KICKER_LENGTH || u > mJumpEnd)		// avant ou après le tremplin
		return base;

	if (u <= 0.0f)				// bosse d'élan
	{
		float a = (u + KICKER_LENGTH) / KICKER_LENGTH;
		return base + mKickerHeight * a * a;
	}

	// Table, un peu plus basse que le bord (la marche est adoucie sur 1,5 m)
	float y = mLipY - TABLE_DROP * glm::smoothstep(0.0f, 1.5f, u);

	// Zone de réception
	if (u > mTableEnd)
		y += mLandingSlope * (u - mTableEnd);

	// CORRECTION : après l'impact, la zone de réception redescend en douceur jusqu'à
	// la piste. Sans cela, elle restait au-dessus de la piste (la table est
	// horizontale alors que la piste descend) : une longue bosse de neige au milieu
	// de la piste, jusqu'à z = 430 environ. smoothstep garde la pente de réception
	// au point d'impact, puis la rend plus raide, puis la ramène à celle de la piste.
	if (u > mLandingU)
		y = glm::mix(y, base, glm::smoothstep(mLandingU, mJumpEnd, u));

	// Le tremplin est de la neige ajoutée : il ne descend jamais sous la pente
	return glm::max(y, base);
}

//-----------------------------------------------------------------------------
// Distance horizontale à l'axe de la piste (exacte en ligne droite, approchée en virage)
//-----------------------------------------------------------------------------
float Terrain::distanceToPiste(float x, float z) const
{
	return std::fabs(x - lookup(mPisteX, z)) * lookup(mPisteCos, z);
}

//-----------------------------------------------------------------------------
// Hauteur du sol en (x, z) : le terrain naturel, aplani dans les FLAT_ZONES
//-----------------------------------------------------------------------------
float Terrain::height(float x, float z) const
{
	float h = naturalHeight(x, z);
	for (size_t i = 0; i < FLAT_ZONES.size(); i++)
	{
		const FlatZone& zone = FLAT_ZONES[i];
		float dist = glm::length(glm::vec2(x - zone.x, z - zone.z));
		float flat = 1.0f - glm::smoothstep(zone.radius, zone.radius + 15.0f, dist);	// 1 dans la zone, 0 à 15 m au-delà
		h = glm::mix(h, mFlatHeights[i], flat);
	}
	return h;
}

//-----------------------------------------------------------------------------
// Hauteur du maillage dessiné en (x, z). Entre les sommets de la grille, les
// triangles sont plats : leur hauteur diffère un peu de height() là où le sol
// est courbé (flancs du tremplin). Ce qui est posé à plat sur la neige (les
// bandes bleues) doit suivre les triangles, sinon il passe dessous par endroits.
// Calcul exact dans la zone fine (cases de TERRAIN_CELL mètres), height() ailleurs.
//-----------------------------------------------------------------------------
float Terrain::meshHeight(float x, float z) const
{
	if (x < TERRAIN_FINE_X_MIN || x >= TERRAIN_FINE_X_MAX || z < TERRAIN_FINE_Z_MIN || z >= TERRAIN_FINE_Z_MAX)
		return height(x, z);

	// Coin (x0, z0) de la case, et position (fx, fz) dans la case, de 0 à 1
	float x0 = TERRAIN_FINE_X_MIN + std::floor((x - TERRAIN_FINE_X_MIN) / TERRAIN_CELL) * TERRAIN_CELL;
	float z0 = TERRAIN_FINE_Z_MIN + std::floor((z - TERRAIN_FINE_Z_MIN) / TERRAIN_CELL) * TERRAIN_CELL;
	float fx = (x - x0) / TERRAIN_CELL;
	float fz = (z - z0) / TERRAIN_CELL;
	float x1 = x0 + TERRAIN_CELL;
	float z1 = z0 + TERRAIN_CELL;

	// La case est coupée par la diagonale (x1, z0) - (x0, z1), comme dans buildMesh.
	// Dans chaque triangle, la hauteur varie linéairement (interpolation barycentrique).
	float hb = height(x0, z1);
	float hc = height(x1, z0);
	if (fx + fz <= 1.0f)
	{
		float ha = height(x0, z0);
		return ha + fx * (hc - ha) + fz * (hb - ha);
	}
	float hd = height(x1, z1);
	return hd + (1.0f - fx) * (hb - hd) + (1.0f - fz) * (hc - hd);
}

//-----------------------------------------------------------------------------
// Hauteur du terrain naturel en (x, z)
//-----------------------------------------------------------------------------
float Terrain::naturalHeight(float x, float z) const
{
	float d = distanceToPiste(x, z);

	// Profil de pente : exact près de la piste, lissé loin d'elle
	float base = glm::mix(lookup(mBase, z), lookup(mBaseSmooth, z),
	                      glm::smoothstep(PISTE_HALF_WIDTH + PISTE_MARGIN, PROFILE_BLEND, d));

	// Distance à l'axe de la vallée, pour les versants
	float dValley = std::fabs(x - lookup(mValleyX, z));

	// onPiste vaut 1 sur la piste et 0 loin d'elle. La piste n'existe qu'entre
	// le départ et un peu après l'arrivée (fondu de 40 m avant, 60 m après).
	float s = lookup(mPisteS, z);
	float onPiste = (1.0f - glm::smoothstep(PISTE_HALF_WIDTH, PISTE_HALF_WIDTH + PISTE_MARGIN, d))
	              * (1.0f - glm::smoothstep(0.0f, 40.0f, -s))
	              * (1.0f - glm::smoothstep(0.0f, 60.0f, s - mPisteLength));

	// Relief : effacé sur la piste
	float h = base + (1.0f - onPiste) * relief(x, z, dValley);

	// Tremplin : seulement près de l'axe, avec des flancs en pente douce
	float onJump = 1.0f - glm::smoothstep(JUMP_HALF_WIDTH, JUMP_HALF_WIDTH + JUMP_SIDE, d);
	if (onJump > 0.0f)
		h += onJump * (jumpHeight(s - mJumpS, base) - base);

	return h;
}

//-----------------------------------------------------------------------------
// Normale du sol en (x, z), par différences finies
//-----------------------------------------------------------------------------
glm::vec3 Terrain::normal(float x, float z) const
{
	const float e = 0.5f;
	float dx = height(x + e, z) - height(x - e, z);
	float dz = height(x, z + e) - height(x, z - e);
	return glm::normalize(glm::vec3(-dx, 2.0f * e, -dz));
}

//-----------------------------------------------------------------------------
// Construit la grille de triangles et l'envoie à la carte graphique
//-----------------------------------------------------------------------------
bool Terrain::buildMesh()
{
	// Coordonnées des lignes de la grille : fines près de la piste, grossières au loin
	std::vector<float> xs = gridCoordinates(TERRAIN_X_MIN, TERRAIN_FINE_X_MIN, TERRAIN_FINE_X_MAX, TERRAIN_X_MAX);
	std::vector<float> zs = gridCoordinates(TERRAIN_Z_MIN, TERRAIN_FINE_Z_MIN, TERRAIN_FINE_Z_MAX, TERRAIN_Z_MAX);
	int nx = (int)xs.size();
	int nz = (int)zs.size();

	// Hauteur de chaque sommet de la grille
	std::vector<float> heights(nx * nz);
	for (int j = 0; j < nz; j++)
		for (int i = 0; i < nx; i++)
			heights[j * nx + i] = height(xs[i], zs[j]);

	// Sommets : position, normale, coordonnées de texture
	std::vector<Vertex> vertices(nx * nz);
	for (int j = 0; j < nz; j++)
	{
		for (int i = 0; i < nx; i++)
		{
			float x = xs[i];
			float z = zs[j];
			Vertex& v = vertices[j * nx + i];
			v.position = glm::vec3(x, heights[j * nx + i], z);

			// Normale par différences centrales sur la grille (d'un seul côté au bord)
			int i0 = std::max(i - 1, 0), i1 = std::min(i + 1, nx - 1);
			int j0 = std::max(j - 1, 0), j1 = std::min(j + 1, nz - 1);
			float dhdx = (heights[j * nx + i1] - heights[j * nx + i0]) / (xs[i1] - xs[i0]);
			float dhdz = (heights[j1 * nx + i] - heights[j0 * nx + i]) / (zs[j1] - zs[j0]);
			v.normal = glm::normalize(glm::vec3(-dhdx, 1.0f, -dhdz));

			// La texture se répète tous les SNOW_TILE mètres
			v.texCoords = glm::vec2(x, z) / SNOW_TILE;
		}
	}

	// Indices : 2 triangles par case, tournés dans le sens trigonométrique vus d'en haut
	std::vector<GLuint> indices;
	indices.reserve((size_t)(nx - 1) * (nz - 1) * 6);
	for (int j = 0; j < nz - 1; j++)
	{
		for (int i = 0; i < nx - 1; i++)
		{
			GLuint a = j * nx + i;	// (i,   j)
			GLuint b = a + nx;		// (i,   j+1)
			GLuint c = a + 1;		// (i+1, j)
			GLuint d = b + 1;		// (i+1, j+1)
			indices.insert(indices.end(), { a, b, c, c, b, d });
		}
	}
	mIndexCount = (GLsizei)indices.size();

	// Envoi à la carte graphique (même disposition des attributs que la classe Mesh)
	glGenVertexArrays(1, &mVAO);
	glGenBuffers(1, &mVBO);
	glGenBuffers(1, &mEBO);

	glBindVertexArray(mVAO);
	glBindBuffer(GL_ARRAY_BUFFER, mVBO);
	glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(Vertex), vertices.data(), GL_STATIC_DRAW);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mEBO);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(GLuint), indices.data(), GL_STATIC_DRAW);

	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (GLvoid*)0);						// position
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (GLvoid*)(3 * sizeof(GLfloat)));	// normale
	glEnableVertexAttribArray(1);
	glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (GLvoid*)(6 * sizeof(GLfloat)));	// UV
	glEnableVertexAttribArray(2);

	glBindVertexArray(0);

	std::cout << "Terrain : " << nx * nz << " sommets, " << mIndexCount / 3 << " triangles" << std::endl;
	return true;
}

//-----------------------------------------------------------------------------
// Dessin du terrain
//-----------------------------------------------------------------------------
void Terrain::draw()
{
	glBindVertexArray(mVAO);
	glDrawElements(GL_TRIANGLES, mIndexCount, GL_UNSIGNED_INT, 0);
	glBindVertexArray(0);
}
