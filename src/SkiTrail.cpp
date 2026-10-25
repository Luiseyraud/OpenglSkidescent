//-----------------------------------------------------------------------------
// SkiTrail.cpp : trace des skis
//-----------------------------------------------------------------------------
#include "SkiTrail.h"
#include "Scene.h"
#include "Timeline.h"
#include "Mesh.h"
#include <cmath>
#include <iostream>
#include <algorithm>

using namespace scene;

const float TRAIL_LIFT = 0.02f;		// m au-dessus de la neige (avec glPolygonOffset, comme les bandes bleues)

// Profil en travers : décalage vers la gauche (m) et inclinaison de la normale
// (> 0 : penchée vers la gauche). Pour chaque sillon (centre c, demi-largeur h) :
//   c-h-2cm : neige plate | c-h : bord incliné | c : fond | c+h : bord incliné | c+h+2cm : plate
// Le bord gauche d'un sillon regarde vers sa droite, et inversement.
struct ProfilePoint { float offset, tilt; };

static std::vector<ProfilePoint> trailProfile()
{
	std::vector<ProfilePoint> profile;
	float h = TRAIL_GROOVE / 2.0f;
	float wall = glm::radians(TRAIL_WALL_DEG);
	for (float c : { -SKI_GAP / 2.0f, SKI_GAP / 2.0f })
	{
		profile.push_back({ c - h - 0.02f, 0.0f });
		profile.push_back({ c - h, wall });
		profile.push_back({ c, 0.0f });
		profile.push_back({ c + h, -wall });
		profile.push_back({ c + h + 0.02f, 0.0f });
	}
	return profile;
}

//-----------------------------------------------------------------------------
// Constructeur / destructeur
//-----------------------------------------------------------------------------
SkiTrail::SkiTrail()
	: mVAO(0), mVBO(0)
{
}

SkiTrail::~SkiTrail()
{
	if (mVAO == 0)
		return;
	glDeleteVertexArrays(1, &mVAO);
	glDeleteBuffers(1, &mVBO);
}

//-----------------------------------------------------------------------------
// Texture du ruban, calculée par le code. u va d'un bord du ruban à l'autre
// (elle ne change pas le long du ruban). Neige blanche comme le terrain, deux
// sillons gris-bleu (dans l'ombre), et un alpha qui s'annule sur les bords.
//-----------------------------------------------------------------------------
bool SkiTrail::createTexture()
{
	std::vector<ProfilePoint> profile = trailProfile();
	float first = profile.front().offset;
	float width = profile.back().offset - first;
	float h = TRAIL_GROOVE / 2.0f;

	const int W = 64, H = 4;
	const glm::vec3 SNOW(254.0f), GROOVE(150.0f, 160.0f, 182.0f);	// sRGB ; 254 = moyenne de neige.jpg
	std::vector<unsigned char> pixels;
	for (int y = 0; y < H; y++)
		for (int x = 0; x < W; x++)
		{
			float u = (x + 0.5f) / W;
			float offset = first + u * width;
			float groove = 0.0f;	// 1 au fond d'un sillon, 0 sur la neige plate
			for (float c : { -SKI_GAP / 2.0f, SKI_GAP / 2.0f })
				groove = std::max(groove, 1.0f - glm::smoothstep(0.5f * h, h, std::fabs(offset - c)));
			glm::vec3 color = glm::mix(SNOW, GROOVE, groove);
			float alpha = glm::smoothstep(0.0f, 0.15f, u) * glm::smoothstep(0.0f, 0.15f, 1.0f - u);
			pixels.insert(pixels.end(), { (unsigned char)color.r, (unsigned char)color.g, (unsigned char)color.b,
			                              (unsigned char)std::lround(alpha * 255.0f) });
		}
	return mTexture.createFromPixels(W, H, pixels.data(), true);
}

//-----------------------------------------------------------------------------
// Construit le ruban. Une section tous les 1/TRAIL_SAMPLES s, de 10 sommets en
// travers de la trace (voir trailProfile).
//-----------------------------------------------------------------------------
bool SkiTrail::build(const Spline& piste, const Terrain& terrain)
{
	if (!createTexture())
		return false;

	std::vector<ProfilePoint> profile = trailProfile();
	int n = (int)profile.size();
	float first = profile.front().offset;
	float width = profile.back().offset - first;

	std::vector<Vertex> vertices;
	std::vector<Vertex> previous;		// section précédente (vide : pas de raccord, après un saut)
	glm::vec3 lastPosition(1e9f);
	int count = (int)((timeline::DURATION - timeline::SKI_START) * TRAIL_SAMPLES) + 1;
	for (int i = 0; i < count; i++)
	{
		float tau = timeline::SKI_START + i / TRAIL_SAMPLES;		// temps du skieur
		timeline::SkierState skier = timeline::skierStateAt(tau, piste, terrain);

		// En l'air, ou arrêté : pas de nouvelle section
		bool moved = glm::length(skier.position - lastPosition) > 0.01f;
		if (skier.flying)
			previous.clear();
		else if (moved)
		{
			// Direction du skieur (3e colonne de sa matrice), à plat, et sa gauche
			glm::vec3 forward = glm::normalize(glm::vec3(skier.model[2].x, 0.0f, skier.model[2].z));
			glm::vec3 left(forward.z, 0.0f, -forward.x);

			std::vector<Vertex> section;
			for (const ProfilePoint& point : profile)
			{
				glm::vec3 p = skier.position + point.offset * left;
				p.y = terrain.meshHeight(p.x, p.z) + TRAIL_LIFT;
				glm::vec3 up = terrain.normal(p.x, p.z);
				glm::vec3 normal = glm::normalize(std::cos(point.tilt) * up + std::sin(point.tilt) * left);
				section.push_back({ p, normal, glm::vec2((point.offset - first) / width, 0.0f) });	// u : en travers
			}

			// Raccord avec la section précédente : un quadrilatère entre deux points voisins du profil
			if (!previous.empty())
				for (int k = 0; k + 1 < n; k++)
					vertices.insert(vertices.end(), { previous[k], section[k], section[k + 1],
					                                  previous[k], section[k + 1], previous[k + 1] });
			previous = section;
			lastPosition = skier.position;
		}
		mVertexCount.push_back((GLsizei)vertices.size());
	}

	glGenVertexArrays(1, &mVAO);
	glGenBuffers(1, &mVBO);
	glBindVertexArray(mVAO);
	glBindBuffer(GL_ARRAY_BUFFER, mVBO);
	glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(Vertex), vertices.data(), GL_STATIC_DRAW);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (GLvoid*)0);						// position
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (GLvoid*)(3 * sizeof(GLfloat)));	// normale
	glEnableVertexAttribArray(1);
	glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (GLvoid*)(6 * sizeof(GLfloat)));	// UV
	glEnableVertexAttribArray(2);
	glBindVertexArray(0);

	std::cout << "Trace des skis : " << vertices.size() / 3 << " triangles" << std::endl;
	return true;
}

//-----------------------------------------------------------------------------
// Dessine le début du ruban : les sections déjà passées à l'instant t
//-----------------------------------------------------------------------------
void SkiTrail::draw(float t)
{
	int i = (int)std::floor((timeline::simTime(t) - timeline::SKI_START) * TRAIL_SAMPLES);
	if (i < 0 || mVertexCount.empty())
		return;
	i = std::min(i, (int)mVertexCount.size() - 1);

	mTexture.bind(0);
	glBindVertexArray(mVAO);
	glDrawArrays(GL_TRIANGLES, 0, mVertexCount[i]);
	glBindVertexArray(0);
	mTexture.unbind(0);
}
