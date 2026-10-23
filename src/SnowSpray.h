//-----------------------------------------------------------------------------
// SnowSpray.h : les gerbes de neige soulevées par les skis.
//
// Des particules (des carrés texturés toujours face à la caméra) sont émises
// par l'arrière des skis quand le skieur prend un virage serré, quand il
// freine à l'arrivée, et toutes d'un coup à la réception du saut.
//
// Comme le trajet du skieur est connu d'avance, toutes les particules sont
// calculées au chargement : instant de naissance, position et vitesse de
// départ, durée de vie, taille... Elles sont envoyées une seule fois à la
// carte graphique. À chaque image, on ne donne que le temps t : le vertex
// shader calcule l'âge de chaque particule, sa position (vitesse freinée par
// l'air, chute lente), sa taille, et cache celles qui ne sont pas encore nées
// ou déjà dissipées. Un seul appel de dessin pour toutes les particules.
//
// Leur image vient d'une "planche" de 4 x 4 images (sprite sheet) d'une
// bouffée de neige qui se dissipe, calculée par le code avec du bruit.
//-----------------------------------------------------------------------------
#ifndef SNOW_SPRAY_H
#define SNOW_SPRAY_H

#include <vector>
#define GLEW_STATIC
#include "GL/glew.h"
#include "glm/glm.hpp"
#include "ShaderProgram.h"
#include "Texture2D.h"
#include "Spline.h"
#include "Terrain.h"

class SnowSpray
{
public:
	SnowSpray();
	~SnowSpray();

	bool build(const Spline& piste, const Terrain& terrain);	// contexte OpenGL requis

	// À dessiner après le ciel (les particules transparentes se mélangent à l'image)
	void draw(const glm::mat4& view, const glm::mat4& projection, const glm::vec3& eye, float t);

private:
	// Une particule = 3 vec4, qui deviennent 3 attributs d'instance du vertex shader
	struct Particle
	{
		glm::vec4 start;	// xyz : position de départ, w : instant de naissance
		glm::vec4 velocity;	// xyz : vitesse de départ,  w : durée de vie
		glm::vec4 look;		// x : taille au départ, y : taille à la fin, z : angle, w : opacité
	};

	bool createSpriteSheet();

	ShaderProgram mShader;
	Texture2D mSheet;
	GLuint mVAO, mCornerVBO, mParticleVBO;
	GLsizei mCount;
};

#endif // SNOW_SPRAY_H
