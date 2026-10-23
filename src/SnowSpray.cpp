//-----------------------------------------------------------------------------
// SnowSpray.cpp : gerbes de neige
//-----------------------------------------------------------------------------
#include "SnowSpray.h"
#include "Scene.h"
#include "Timeline.h"
#include <random>
#include <cmath>
#include <iostream>
#include "glm/gtc/noise.hpp"
#include "glm/gtc/constants.hpp"

using namespace scene;

// Planche d'images : 4 x 4 images de SHEET_CELL pixels
const int SHEET_COLUMNS = 4;
const int SHEET_CELL = 128;

//-----------------------------------------------------------------------------
// Constructeur / destructeur
//-----------------------------------------------------------------------------
SnowSpray::SnowSpray()
	: mVAO(0), mCornerVBO(0), mParticleVBO(0), mCount(0)
{
}

SnowSpray::~SnowSpray()
{
	if (mVAO == 0)
		return;
	glDeleteVertexArrays(1, &mVAO);
	glDeleteBuffers(1, &mCornerVBO);
	glDeleteBuffers(1, &mParticleVBO);
}

//-----------------------------------------------------------------------------
// Bruit fractal (somme de 4 bruits de plus en plus fins), entre -1 et 1 environ
//-----------------------------------------------------------------------------
static float fbm(glm::vec3 p)
{
	float sum = 0.0f, amplitude = 1.0f, total = 0.0f;
	for (int i = 0; i < 4; i++)
	{
		sum += amplitude * glm::simplex(p);
		total += amplitude;
		p *= 2.0f;
		amplitude *= 0.5f;
	}
	return sum / total;
}

//-----------------------------------------------------------------------------
// Planche de 16 images d'une bouffée de neige, de dense (image 0) à presque
// dissipée (image 15). Le bruit change doucement d'une image à la suivante :
// le shader peut passer de l'une à l'autre en fondu.
// Pixels blancs ; l'alpha porte la densité de la poussière.
//-----------------------------------------------------------------------------
bool SnowSpray::createSpriteSheet()
{
	int size = SHEET_COLUMNS * SHEET_CELL;
	std::vector<unsigned char> pixels((size_t)size * size * 4, 255);

	for (int frame = 0; frame < SHEET_COLUMNS * SHEET_COLUMNS; frame++)
	{
		float p = frame / 15.0f;	// avancement de la dissipation, de 0 à 1
		int x0 = (frame % SHEET_COLUMNS) * SHEET_CELL;
		int y0 = (frame / SHEET_COLUMNS) * SHEET_CELL;

		for (int y = 0; y < SHEET_CELL; y++)
			for (int x = 0; x < SHEET_CELL; x++)
			{
				glm::vec2 q = (glm::vec2(x, y) + 0.5f) / (float)SHEET_CELL * 2.0f - 1.0f;	// de -1 à 1
				float r = glm::length(q);

				// Forme : une boule au contour irrégulier, qui grossit
				float edge = fbm(glm::vec3(q * 1.8f, 3.1f + 0.8f * p));
				float radius = glm::mix(0.5f, 0.85f, p) + 0.22f * edge;
				float shape = 1.0f - glm::smoothstep(radius - 0.45f, radius, r);

				// Grain de la poussière : elle se troue de plus en plus en se dissipant
				float grain = 0.5f + 0.5f * fbm(glm::vec3(q * 4.0f, 7.3f + 1.5f * p));
				float holes = glm::smoothstep(0.15f + 0.5f * p, 0.55f + 0.5f * p, grain);
				float density = shape * glm::mix(1.0f, holes, 0.4f + 0.6f * p) * glm::mix(1.0f, 0.45f, p);

				// Rien au bord de l'image, pour ne pas déborder sur les images voisines
				density *= 1.0f - glm::smoothstep(0.85f, 1.0f, std::max(std::fabs(q.x), std::fabs(q.y)));

				size_t i = ((size_t)(y0 + y) * size + (x0 + x)) * 4;
				pixels[i + 3] = (unsigned char)std::lround(glm::clamp(density, 0.0f, 1.0f) * 255.0f);
			}
	}
	return mSheet.createFromPixels(size, size, pixels.data(), true);
}

//-----------------------------------------------------------------------------
// Calcule toutes les particules de la vidéo, puis les envoie à la carte graphique
//-----------------------------------------------------------------------------
bool SnowSpray::build(const Spline& piste, const Terrain& terrain)
{
	if (!mShader.loadShaders("shaders/spray.vert", "shaders/spray.frag") || !createSpriteSheet())
		return false;

	std::mt19937 rng(DECOR_SEED + 3);
	std::uniform_real_distribution<float> random01(0.0f, 1.0f);
	auto between = [&](float a, float b) { return glm::mix(a, b, random01(rng)); };

	std::vector<Particle> particles;
	const float DT = 1.0f / 120.0f;		// pas de temps du calcul
	float pending = 0.0f;				// particules dues, pas encore émises (fraction)
	bool wasFlying = false;
	float previousSpeed = 0.0f;
	int steps = (int)((timeline::DURATION - timeline::SKI_START) / DT);

	for (int i = 0; i <= steps; i++)
	{
		float t = timeline::SKI_START + i * DT;
		timeline::SkierState skier = timeline::skierState(t, piste, terrain);
		float deceleration = (previousSpeed - skier.speed) / DT;
		previousSpeed = skier.speed;
		if (skier.flying)
		{
			wasFlying = true;
			continue;
		}

		// Repère du skieur : ses axes sont les colonnes de sa matrice model
		glm::vec3 left = glm::normalize(glm::vec3(skier.model[0]));
		glm::vec3 up(0.0f, 1.0f, 0.0f);
		glm::vec3 forward = glm::normalize(glm::vec3(skier.model[2].x, 0.0f, skier.model[2].z));
		auto skiPoint = [&](float x, float z) {		// point des skis (repère du modèle) -> monde
			return glm::vec3(skier.model * glm::vec4(x, 0.05f, z, 1.0f));
		};

		// Réception du saut : une grosse gerbe d'un coup, tout autour des skis
		if (wasFlying)
		{
			wasFlying = false;
			for (int k = 0; k < SPRAY_LANDING; k++)
			{
				float angle = between(0.0f, glm::two_pi<float>());
				glm::vec3 outward = std::cos(angle) * left + std::sin(angle) * forward;
				Particle particle;
				particle.start = glm::vec4(skiPoint(between(-0.3f, 0.3f), between(-0.9f, 0.9f)), t);
				particle.velocity = glm::vec4(outward * between(2.0f, 7.0f) + up * between(1.5f, 4.5f)
				                              + forward * skier.speed * between(0.2f, 0.5f), between(1.6f, 2.6f));
				particle.look = glm::vec4(between(0.4f, 0.8f), between(2.0f, 3.5f), between(0.0f, 6.3f), between(0.6f, 1.0f));
				particles.push_back(particle);
			}
		}

		// Virage serré ou freinage : un flot régulier de particules
		float turn = glm::smoothstep(SPRAY_MIN_LEAN, SPRAY_FULL_LEAN, skier.lean);
		float braking = glm::smoothstep(1.0f, 6.0f, deceleration);
		pending += SPRAY_RATE * std::max(turn, braking) * DT;
		while (pending >= 1.0f)
		{
			pending -= 1.0f;
			Particle particle;
			if (turn >= braking)
			{
				// Le ski extérieur (opposé au centre du virage) projette la neige vers l'extérieur
				glm::vec3 outside = -skier.inside;
				float side = (glm::dot(outside, left) > 0.0f) ? 1.0f : -1.0f;
				particle.start = glm::vec4(skiPoint(side * SKI_GAP / 2.0f, SKI_TAIL + between(0.0f, 0.5f)), t);
				particle.velocity = glm::vec4(outside * between(2.5f, 6.0f) * turn + up * between(1.0f, 3.0f)
				                              + forward * skier.speed * between(0.15f, 0.4f), between(0.9f, 1.6f));
			}
			else
			{
				// Freinage : la neige part des deux skis, vers l'avant et les côtés
				float side = (random01(rng) < 0.5f) ? 1.0f : -1.0f;
				particle.start = glm::vec4(skiPoint(side * SKI_GAP / 2.0f, between(-0.6f, 0.6f)), t);
				particle.velocity = glm::vec4(left * side * between(1.0f, 3.5f) + up * between(0.8f, 2.2f)
				                              + forward * skier.speed * between(0.3f, 0.7f), between(0.9f, 1.5f));
			}
			particle.look = glm::vec4(between(0.2f, 0.4f), between(0.7f, 1.4f), between(0.0f, 6.3f), between(0.35f, 0.7f));
			particles.push_back(particle);
		}
	}
	mCount = (GLsizei)particles.size();
	std::cout << "Gerbes de neige : " << mCount << " particules" << std::endl;

	// Les 4 coins d'un carré (GL_TRIANGLE_STRIP), communs à toutes les particules
	const float corners[] = { -1, -1,   1, -1,   -1, 1,   1, 1 };

	glGenVertexArrays(1, &mVAO);
	glGenBuffers(1, &mCornerVBO);
	glGenBuffers(1, &mParticleVBO);
	glBindVertexArray(mVAO);

	glBindBuffer(GL_ARRAY_BUFFER, mCornerVBO);
	glBufferData(GL_ARRAY_BUFFER, sizeof(corners), corners, GL_STATIC_DRAW);
	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (GLvoid*)0);
	glEnableVertexAttribArray(0);

	// Les particules : 3 vec4 par particule, qui changent à chaque instance (divisor 1)
	glBindBuffer(GL_ARRAY_BUFFER, mParticleVBO);
	glBufferData(GL_ARRAY_BUFFER, particles.size() * sizeof(Particle), particles.data(), GL_STATIC_DRAW);
	for (int k = 0; k < 3; k++)
	{
		glVertexAttribPointer(1 + k, 4, GL_FLOAT, GL_FALSE, sizeof(Particle), (GLvoid*)(k * sizeof(glm::vec4)));
		glEnableVertexAttribArray(1 + k);
		glVertexAttribDivisor(1 + k, 1);
	}
	glBindVertexArray(0);
	return true;
}

//-----------------------------------------------------------------------------
// Dessine les particules vivantes à l'instant t
//-----------------------------------------------------------------------------
void SnowSpray::draw(const glm::mat4& view, const glm::mat4& projection, const glm::vec3& eye, float t)
{
	if (mCount == 0)
		return;

	mShader.use();
	mShader.setUniform("view", view);
	mShader.setUniform("projection", projection);
	mShader.setUniform("time", t);
	mShader.setUniform("drag", SPRAY_DRAG);
	mShader.setUniform("settle", SPRAY_SETTLE);
	mShader.setUniform("eye", eye);
	mShader.setUniform("sunDir", sunDirection());
	mShader.setUniform("sunColor", SUN_COLOR * SUN_INTENSITY);
	mShader.setUniform("ambient", AMBIENT_SKY);
	mShader.setUniformSampler("sheet", 0);
	mSheet.bind(0);

	// Transparence en "alpha prémultiplié" : le shader renvoie (couleur x alpha, alpha)
	// et on calcule image = particule + image x (1 - alpha). Pour une particule
	// ordinaire, c'est le même résultat que (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA) ;
	// mais la couleur peut aussi dépasser l'alpha : la poussière à contre-jour brille.
	// La profondeur est testée (un sapin cache la gerbe) mais pas écrite : les
	// particules ne se cachent pas entre elles.
	glEnable(GL_BLEND);
	glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
	glDepthMask(GL_FALSE);

	glBindVertexArray(mVAO);
	glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, mCount);
	glBindVertexArray(0);

	glDepthMask(GL_TRUE);
	glDisable(GL_BLEND);
	mSheet.unbind(0);
}
