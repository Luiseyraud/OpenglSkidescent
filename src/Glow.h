//-----------------------------------------------------------------------------
// Glow.h : le halo lumineux autour des projecteurs allumés.
//
// Un projecteur vu de face éblouit : un halo apparaît autour de lui. On le
// dessine avec un carré toujours tourné vers la caméra (un "billboard"), dont
// le shader calcule une lueur qui diminue du centre vers le bord. Les halos
// sont AJOUTÉS à l'image (mélange additif) : ils n'ont pas de bord visible.
// Pour un projecteur, ils sont forts quand la caméra est dans le faisceau,
// nuls vus de dos ; une ampoule brille dans toutes les directions.
//-----------------------------------------------------------------------------
#ifndef GLOW_H
#define GLOW_H

#include <vector>
#define GLEW_STATIC
#include "GL/glew.h"
#include "glm/glm.hpp"
#include "ShaderProgram.h"

// Une source de halo
struct GlowSource
{
	glm::vec3 position;
	glm::vec3 direction;	// axe du faisceau ; (0, 0, 0) : lumière dans toutes les directions
	glm::vec3 color;		// couleur x force du halo
	float size;				// demi-largeur du halo (m)
};

class Glow
{
public:
	Glow();
	~Glow();

	bool load();	// shaders et carré (contexte OpenGL requis)

	// À appeler en dernier, APRÈS le ciel : les halos s'ajoutent à l'image finie
	void draw(const glm::mat4& view, const glm::mat4& projection, const glm::vec3& eye,
	          const std::vector<GlowSource>& sources);

private:
	ShaderProgram mShader;
	GLuint mVAO, mVBO;
};

#endif // GLOW_H
