//-----------------------------------------------------------------------------
// Crowd.h : le public des tribunes, fait et animé par le code.
//
// Un spectateur est un personnage minimaliste de 6 boîtes : jambes, buste,
// deux bras, tête et bonnet. Le même personnage est dessiné à chaque place
// occupée (instancing), avec pour chacun : sa position, ses couleurs (veste,
// pantalon, peau, bonnet, prises dans une petite texture palette), sa taille,
// et quelques nombres au hasard qui rendent ses mouvements différents.
//
// L'animation est procédurale : le vertex shader (shaders/crowd.vert) calcule
// la pose de chacun à partir du temps. Assis au début, le public fait la ola ;
// quand le skieur approche de l'arrivée, il se lève, lève les bras, sautille
// et se tourne vers lui. Rien n'est stocké d'une image à l'autre.
// L'éclairage est celui de la scène (shaders/scene.frag).
//-----------------------------------------------------------------------------
#ifndef CROWD_H
#define CROWD_H

#include <vector>
#define GLEW_STATIC
#include "GL/glew.h"
#include "glm/glm.hpp"
#include "ShaderProgram.h"
#include "Texture2D.h"
#include "Stands.h"

class Crowd
{
public:
	Crowd();
	~Crowd();

	bool build(const std::vector<Place>& places);	// contexte OpenGL requis

	// Le shader (crowd.vert + scene.frag) doit déjà avoir les uniforms de la scène
	void draw(ShaderProgram& shader, float t, const glm::vec3& skier, float cheerTime);

private:
	// Un spectateur = 4 vec4, attributs d'instance du vertex shader (locations 3 à 6)
	struct Person
	{
		glm::vec4 placement;	// xyz : pieds (debout), w : orientation (rad)
		glm::vec4 colors;		// n° de couleur : veste, pantalon, peau, bonnet
		glm::vec4 traits;		// x : taille, y : phase, z : énergie (0 à 1), w : retard pour se lever (s)
		glm::vec4 extra;		// x : position le long de la tribune (m)
	};

	Texture2D mPalette;
	GLuint mVAO, mVBO, mInstanceVBO;
	GLsizei mVertexCount, mCount;
};

#endif // CROWD_H
