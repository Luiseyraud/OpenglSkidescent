//-----------------------------------------------------------------------------
// Snowfall.cpp : la neige qui tombe
//-----------------------------------------------------------------------------
#include "Snowfall.h"
#include "Scene.h"
#include <vector>
#include <random>
#include "glm/gtc/constants.hpp"

using namespace scene;

//-----------------------------------------------------------------------------
// Constructeur / destructeur
//-----------------------------------------------------------------------------
Snowfall::Snowfall()
	: mVAO(0), mCornerVBO(0), mFlakeVBO(0), mCount(0)
{
}

Snowfall::~Snowfall()
{
	if (mVAO == 0)
		return;
	glDeleteVertexArrays(1, &mVAO);
	glDeleteBuffers(1, &mCornerVBO);
	glDeleteBuffers(1, &mFlakeVBO);
}

//-----------------------------------------------------------------------------
// Tire les flocons au hasard (graine fixe) et les envoie à la carte graphique
//-----------------------------------------------------------------------------
bool Snowfall::build()
{
	std::mt19937 rng(DECOR_SEED + 5);
	std::uniform_real_distribution<float> random01(0.0f, 1.0f);
	auto between = [&](float a, float b) { return glm::mix(a, b, random01(rng)); };

	std::vector<Flake> flakes(SNOWFALL_COUNT);
	for (Flake& flake : flakes)
	{
		flake.start = glm::vec4(random01(rng), random01(rng), random01(rng), between(0.0f, glm::two_pi<float>()));
		flake.motion = glm::vec4(between(0.01f, 0.02f), between(0.8f, 1.4f), between(0.2f, 0.5f), between(0.5f, 1.5f));
	}
	mCount = (GLsizei)flakes.size();

	// Les 4 coins d'un carré (GL_TRIANGLE_STRIP), communs à tous les flocons
	const float corners[] = { -1, -1,   1, -1,   -1, 1,   1, 1 };

	glGenVertexArrays(1, &mVAO);
	glGenBuffers(1, &mCornerVBO);
	glGenBuffers(1, &mFlakeVBO);
	glBindVertexArray(mVAO);

	glBindBuffer(GL_ARRAY_BUFFER, mCornerVBO);
	glBufferData(GL_ARRAY_BUFFER, sizeof(corners), corners, GL_STATIC_DRAW);
	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (GLvoid*)0);
	glEnableVertexAttribArray(0);

	glBindBuffer(GL_ARRAY_BUFFER, mFlakeVBO);
	glBufferData(GL_ARRAY_BUFFER, flakes.size() * sizeof(Flake), flakes.data(), GL_STATIC_DRAW);
	for (int k = 0; k < 2; k++)
	{
		glVertexAttribPointer(1 + k, 4, GL_FLOAT, GL_FALSE, sizeof(Flake), (GLvoid*)(k * sizeof(glm::vec4)));
		glEnableVertexAttribArray(1 + k);
		glVertexAttribDivisor(1 + k, 1);
	}
	glBindVertexArray(0);
	return true;
}

//-----------------------------------------------------------------------------
// Dessine tous les flocons en un seul appel
//-----------------------------------------------------------------------------
void Snowfall::draw(ShaderProgram& shader, float tau, float pixelScale)
{
	shader.setUniform("time", tau);
	shader.setUniform("boxSize", SNOWFALL_BOX);
	shader.setUniform("wind", SNOWFALL_WIND);
	shader.setUniform("pixelScale", pixelScale);
	shader.setUniform("scatter", SPRAY_FORWARD_SCATTER);

	// Comme les gerbes : alpha prémultiplié, profondeur testée mais pas écrite
	glEnable(GL_BLEND);
	glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
	glDepthMask(GL_FALSE);

	glBindVertexArray(mVAO);
	glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, mCount);
	glBindVertexArray(0);

	glDepthMask(GL_TRUE);
	glDisable(GL_BLEND);
}
