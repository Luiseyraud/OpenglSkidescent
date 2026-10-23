//-----------------------------------------------------------------------------
// Glow.cpp : halos des projecteurs
//-----------------------------------------------------------------------------
#include "Glow.h"
#include "Scene.h"
#include <cmath>

using namespace scene;

//-----------------------------------------------------------------------------
// Constructeur / destructeur
//-----------------------------------------------------------------------------
Glow::Glow()
	: mVAO(0), mVBO(0)
{
}

Glow::~Glow()
{
	if (mVAO == 0)
		return;
	glDeleteVertexArrays(1, &mVAO);
	glDeleteBuffers(1, &mVBO);
}

//-----------------------------------------------------------------------------
// Charge les shaders et crée le carré : 4 coins de (-1, -1) à (1, 1), dessinés
// en GL_TRIANGLE_STRIP (2 triangles)
//-----------------------------------------------------------------------------
bool Glow::load()
{
	if (!mShader.loadShaders("shaders/glow.vert", "shaders/glow.frag"))
		return false;

	const float corners[] = { -1, -1,   1, -1,   -1, 1,   1, 1 };

	glGenVertexArrays(1, &mVAO);
	glGenBuffers(1, &mVBO);
	glBindVertexArray(mVAO);
	glBindBuffer(GL_ARRAY_BUFFER, mVBO);
	glBufferData(GL_ARRAY_BUFFER, sizeof(corners), corners, GL_STATIC_DRAW);
	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (GLvoid*)0);
	glEnableVertexAttribArray(0);
	glBindVertexArray(0);

	return true;
}

//-----------------------------------------------------------------------------
// Dessine un halo devant chaque source
//-----------------------------------------------------------------------------
void Glow::draw(const glm::mat4& view, const glm::mat4& projection, const glm::vec3& eye,
                const std::vector<GlowSource>& sources)
{
	mShader.use();
	mShader.setUniform("view", view);
	mShader.setUniform("projection", projection);

	// Mélange additif : couleur finale = image + halo. La profondeur est testée
	// (un sapin devant le projecteur cache son halo) mais pas écrite.
	glEnable(GL_BLEND);
	glBlendFunc(GL_ONE, GL_ONE);
	glDepthMask(GL_FALSE);
	glBindVertexArray(mVAO);

	for (const GlowSource& source : sources)
	{
		glm::vec3 toEye = glm::normalize(eye - source.position);

		// Force du halo : pour un projecteur, 1 si la caméra est sur l'axe du faisceau,
		// 0 si elle est à 90° ou derrière. Le brouillard l'atténue au loin, comme le reste.
		float facing = 1.0f;
		if (glm::length(source.direction) > 0.0f)
		{
			facing = glm::max(glm::dot(toEye, source.direction), 0.0f);
			facing *= facing;
		}
		float distance = glm::length(eye - source.position);
		float strength = facing * std::exp(-std::pow(distance * FOG_DENSITY, 2.0f));
		if (strength < 0.001f)
			continue;

		// Le carré est avancé vers la caméra, pour ne pas être coupé par la lampe elle-même
		mShader.setUniform("center", source.position + 0.3f * source.size * toEye);
		mShader.setUniform("size", source.size);
		mShader.setUniform("color", source.color * strength);
		glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
	}

	glBindVertexArray(0);
	glDepthMask(GL_TRUE);
	glDisable(GL_BLEND);
}
