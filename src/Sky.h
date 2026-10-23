//-----------------------------------------------------------------------------
// Sky.h : le ciel de coucher de soleil, calculé dans un shader (aucune image).
//
// On dessine un cube centré sur la caméra. Pour chaque pixel, le shader prend
// la direction regardée et en déduit la couleur : dégradé chaud à l'horizon,
// plus froid au zénith, et le disque du soleil dans la direction L.
//-----------------------------------------------------------------------------
#ifndef SKY_H
#define SKY_H

#define GLEW_STATIC
#include "GL/glew.h"
#include "glm/glm.hpp"
#include "ShaderProgram.h"

class Sky
{
public:
	Sky();
	~Sky();

	bool load();	// shaders et cube (contexte OpenGL requis)
	void draw(const glm::mat4& view, const glm::mat4& projection);

	// Envoie les couleurs du ciel et la direction du soleil à un shader.
	// Sert aussi au shader de la scène, pour la couleur du brouillard.
	static void setUniforms(ShaderProgram& shader);

private:
	ShaderProgram mShader;
	GLuint mVAO, mVBO;
};

#endif // SKY_H
