//-----------------------------------------------------------------------------
// Sky.cpp : le ciel de coucher de soleil
//-----------------------------------------------------------------------------
#include "Sky.h"
#include "Scene.h"

//-----------------------------------------------------------------------------
// Constructeur / destructeur
//-----------------------------------------------------------------------------
Sky::Sky()
	: mVAO(0), mVBO(0)
{
}

Sky::~Sky()
{
	if (mVAO == 0)
		return;
	glDeleteVertexArrays(1, &mVAO);
	glDeleteBuffers(1, &mVBO);
}

//-----------------------------------------------------------------------------
// Charge les shaders et crée le cube (36 sommets = 6 faces x 2 triangles)
//-----------------------------------------------------------------------------
bool Sky::load()
{
	if (!mShader.loadShaders("shaders/sky.vert", "shaders/sky.frag"))
		return false;

	const float cube[] = {
		-1, 1,-1,  -1,-1,-1,   1,-1,-1,   1,-1,-1,   1, 1,-1,  -1, 1,-1,	// arrière (-Z)
		-1,-1, 1,  -1,-1,-1,  -1, 1,-1,  -1, 1,-1,  -1, 1, 1,  -1,-1, 1,	// gauche (-X)
		 1,-1,-1,   1,-1, 1,   1, 1, 1,   1, 1, 1,   1, 1,-1,   1,-1,-1,	// droite (+X)
		-1,-1, 1,  -1, 1, 1,   1, 1, 1,   1, 1, 1,   1,-1, 1,  -1,-1, 1,	// avant (+Z)
		-1, 1,-1,   1, 1,-1,   1, 1, 1,   1, 1, 1,  -1, 1, 1,  -1, 1,-1,	// haut (+Y)
		-1,-1,-1,  -1,-1, 1,   1,-1,-1,   1,-1,-1,  -1,-1, 1,   1,-1, 1	// bas (-Y)
	};

	glGenVertexArrays(1, &mVAO);
	glGenBuffers(1, &mVBO);
	glBindVertexArray(mVAO);
	glBindBuffer(GL_ARRAY_BUFFER, mVBO);
	glBufferData(GL_ARRAY_BUFFER, sizeof(cube), cube, GL_STATIC_DRAW);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (GLvoid*)0);
	glEnableVertexAttribArray(0);
	glBindVertexArray(0);

	return true;
}

//-----------------------------------------------------------------------------
// Couleurs du ciel et direction du soleil, communes au ciel et au brouillard
//-----------------------------------------------------------------------------
void Sky::setUniforms(ShaderProgram& shader)
{
	shader.setUniform("sky.zenith", scene::SKY_ZENITH);
	shader.setUniform("sky.horizon", scene::SKY_HORIZON);
	shader.setUniform("sky.horizonSun", scene::SKY_HORIZON_SUN);
	shader.setUniform("sky.sunDir", scene::sunDirection());
}

//-----------------------------------------------------------------------------
// Dessine le ciel. À appeler APRÈS la scène : il ne remplit que le fond.
//-----------------------------------------------------------------------------
void Sky::draw(const glm::mat4& view, const glm::mat4& projection)
{
	mShader.use();
	mShader.setUniform("view", glm::mat4(glm::mat3(view)));	// on garde la rotation, pas la translation
	mShader.setUniform("projection", projection);
	setUniforms(mShader);
	mShader.setUniform("sunColor", scene::SUN_COLOR);

	// Le ciel est à la profondeur 1 : il faut "<=" pour qu'il passe là où rien n'est dessiné
	glDepthFunc(GL_LEQUAL);
	glBindVertexArray(mVAO);
	glDrawArrays(GL_TRIANGLES, 0, 36);
	glBindVertexArray(0);
	glDepthFunc(GL_LESS);
}
