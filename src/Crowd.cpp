//-----------------------------------------------------------------------------
// Crowd.cpp : le public
//-----------------------------------------------------------------------------
#include "Crowd.h"
#include "Scene.h"
#include <random>
#include <iostream>
#include "glm/gtc/constants.hpp"

using namespace scene;

// Parties du corps : leur numéro est rangé dans la coordonnée de texture u des
// sommets. Le vertex shader s'en sert pour choisir la couleur et pour savoir
// quels sommets appartiennent aux bras (qui tournent autour de l'épaule).
enum Part { LEGS = 0, TORSO = 1, HEAD = 2, HAT = 3, LEFT_ARM = 4, RIGHT_ARM = 5 };

// Palette : 8 couleurs (colonnes) pour chacune des 4 lignes (sRGB)
const int PALETTE_COLUMNS = 8;
const unsigned char PALETTE[4][8][3] = {
	{ { 200, 30, 30 }, { 30, 70, 190 }, { 240, 190, 20 }, { 40, 140, 60 },		// vestes
	  { 240, 110, 20 }, { 110, 50, 150 }, { 230, 230, 235 }, { 30, 30, 35 } },
	{ { 25, 25, 30 }, { 30, 40, 80 }, { 90, 90, 95 }, { 80, 55, 40 },			// pantalons
	  { 60, 60, 70 }, { 40, 50, 60 }, { 20, 20, 20 }, { 100, 80, 60 } },
	{ { 240, 200, 170 }, { 225, 175, 140 }, { 190, 140, 105 }, { 140, 95, 65 },	// peau
	  { 95, 65, 45 }, { 230, 190, 160 }, { 205, 155, 120 }, { 170, 120, 85 } },
	{ { 210, 30, 40 }, { 240, 240, 240 }, { 20, 30, 70 }, { 30, 130, 60 },		// bonnets
	  { 245, 200, 30 }, { 230, 90, 150 }, { 40, 170, 200 }, { 25, 25, 25 } }
};

//-----------------------------------------------------------------------------
// Ajoute une boîte de (x0, y0, z0) à (x1, y1, z1), avec une normale par face
//-----------------------------------------------------------------------------
static void addBox(std::vector<Vertex>& v, glm::vec3 lo, glm::vec3 hi, Part part)
{
	glm::vec2 uv((float)part, 0.0f);
	auto face = [&](glm::vec3 a, glm::vec3 b, glm::vec3 c, glm::vec3 d, glm::vec3 n) {
		v.insert(v.end(), { { a, n, uv }, { b, n, uv }, { c, n, uv }, { a, n, uv }, { c, n, uv }, { d, n, uv } });
	};
	glm::vec3 p[8];
	for (int i = 0; i < 8; i++)
		p[i] = glm::vec3((i & 1) ? hi.x : lo.x, (i & 2) ? hi.y : lo.y, (i & 4) ? hi.z : lo.z);
	face(p[0], p[4], p[6], p[2], glm::vec3(-1, 0, 0));	// gauche (-X)
	face(p[1], p[3], p[7], p[5], glm::vec3(1, 0, 0));	// droite (+X)
	face(p[0], p[1], p[5], p[4], glm::vec3(0, -1, 0));	// dessous
	face(p[2], p[6], p[7], p[3], glm::vec3(0, 1, 0));	// dessus
	face(p[0], p[2], p[3], p[1], glm::vec3(0, 0, -1));	// arrière (-Z)
	face(p[4], p[5], p[7], p[6], glm::vec3(0, 0, 1));	// avant (+Z) : il regarde vers +Z
}

//-----------------------------------------------------------------------------
// Constructeur / destructeur
//-----------------------------------------------------------------------------
Crowd::Crowd()
	: mVAO(0), mVBO(0), mInstanceVBO(0), mVertexCount(0), mCount(0)
{
}

Crowd::~Crowd()
{
	if (mVAO == 0)
		return;
	glDeleteVertexArrays(1, &mVAO);
	glDeleteBuffers(1, &mVBO);
	glDeleteBuffers(1, &mInstanceVBO);
}

//-----------------------------------------------------------------------------
// Construit le personnage, la palette, et un spectateur par place occupée
//-----------------------------------------------------------------------------
bool Crowd::build(const std::vector<Place>& places)
{
	// Le personnage (1,72 m avec le bonnet), debout, pieds en (0, 0, 0), regarde vers +Z.
	// Les bras pendent le long du buste ; l'épaule est en (+-0,26 ; 1,36).
	std::vector<Vertex> body;
	addBox(body, { -0.15f, 0.00f, -0.10f }, { 0.15f, 0.82f, 0.10f }, LEGS);
	addBox(body, { -0.21f, 0.80f, -0.13f }, { 0.21f, 1.42f, 0.13f }, TORSO);
	addBox(body, { 0.21f, 0.82f, -0.06f }, { 0.31f, 1.40f, 0.06f }, LEFT_ARM);
	addBox(body, { -0.31f, 0.82f, -0.06f }, { -0.21f, 1.40f, 0.06f }, RIGHT_ARM);
	addBox(body, { -0.10f, 1.43f, -0.10f }, { 0.10f, 1.64f, 0.10f }, HEAD);
	addBox(body, { -0.105f, 1.60f, -0.105f }, { 0.105f, 1.72f, 0.105f }, HAT);
	mVertexCount = (GLsizei)body.size();

	// Palette : 8 x 4 pixels, une couleur par pixel
	std::vector<unsigned char> pixels;
	for (int row = 0; row < 4; row++)
		for (int column = 0; column < PALETTE_COLUMNS; column++)
			pixels.insert(pixels.end(), { PALETTE[row][column][0], PALETTE[row][column][1], PALETTE[row][column][2], 255 });
	if (!mPalette.createFromPixels(PALETTE_COLUMNS, 4, pixels.data(), false))
		return false;

	// Les spectateurs, tirés au hasard (graine fixe : le même public à chaque lancement)
	std::mt19937 rng(DECOR_SEED + 4);
	std::uniform_real_distribution<float> random01(0.0f, 1.0f);
	auto between = [&](float a, float b) { return glm::mix(a, b, random01(rng)); };
	auto color = [&]() { return (float)(int)between(0.0f, PALETTE_COLUMNS - 0.001f); };

	std::vector<Person> people;
	for (const Place& place : places)
	{
		if (random01(rng) > CROWD_FILL)		// place vide
			continue;
		Person person;
		person.placement = glm::vec4(place.feet, place.yaw);
		person.colors = glm::vec4(color(), color(), color(), color());
		person.traits = glm::vec4(between(0.9f, 1.08f), between(0.0f, glm::two_pi<float>()),
		                          random01(rng), between(0.0f, 1.5f));
		person.extra = glm::vec4(place.along, 0.0f, 0.0f, 0.0f);
		people.push_back(person);
	}
	mCount = (GLsizei)people.size();
	std::cout << "Public : " << mCount << " spectateurs" << std::endl;

	// Carte graphique : le personnage (attributs 0 à 2, comme la classe Mesh), puis
	// les spectateurs (attributs 3 à 6, un vec4 chacun, qui changent à chaque instance)
	glGenVertexArrays(1, &mVAO);
	glGenBuffers(1, &mVBO);
	glGenBuffers(1, &mInstanceVBO);
	glBindVertexArray(mVAO);

	glBindBuffer(GL_ARRAY_BUFFER, mVBO);
	glBufferData(GL_ARRAY_BUFFER, body.size() * sizeof(Vertex), body.data(), GL_STATIC_DRAW);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (GLvoid*)0);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (GLvoid*)(3 * sizeof(GLfloat)));
	glEnableVertexAttribArray(1);
	glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (GLvoid*)(6 * sizeof(GLfloat)));
	glEnableVertexAttribArray(2);

	glBindBuffer(GL_ARRAY_BUFFER, mInstanceVBO);
	glBufferData(GL_ARRAY_BUFFER, people.size() * sizeof(Person), people.data(), GL_STATIC_DRAW);
	for (int k = 0; k < 4; k++)
	{
		glVertexAttribPointer(3 + k, 4, GL_FLOAT, GL_FALSE, sizeof(Person), (GLvoid*)(k * sizeof(glm::vec4)));
		glEnableVertexAttribArray(3 + k);
		glVertexAttribDivisor(3 + k, 1);
	}
	glBindVertexArray(0);
	return true;
}

//-----------------------------------------------------------------------------
// Dessine tout le public en un seul appel
//-----------------------------------------------------------------------------
void Crowd::draw(ShaderProgram& shader, float t, const glm::vec3& skier, float cheerTime)
{
	if (mCount == 0)
		return;

	shader.setUniform("time", t);
	shader.setUniform("cheerTime", cheerTime);
	shader.setUniform("skier", skier);
	shader.setUniform("olaSpeed", OLA_SPEED);
	shader.setUniform("olaPeriod", OLA_PERIOD);
	shader.setUniform("material.specular", glm::vec3(0.1f));
	shader.setUniform("material.shininess", 16.0f);

	mPalette.bind(0);
	glBindVertexArray(mVAO);
	glDrawArraysInstanced(GL_TRIANGLES, 0, mVertexCount, mCount);
	glBindVertexArray(0);
	mPalette.unbind(0);
}
