//-----------------------------------------------------------------------------
// Snowfall.h : la neige qui tombe.
//
// 60 000 flocons remplissent une boîte de SNOWFALL_BOX mètres. Cette boîte se
// répète à l'infini (modulo) et reste centrée sur la caméra : il neige partout
// autour d'elle, avec un nombre fixe de flocons.
//
// Tout est calculé dans le vertex shader (shaders/snow.vert) à partir du temps
// du skieur : chute, vent, balancement. Pendant le ralenti du saut, la neige
// ralentit aussi. Chaque flocon reçoit la lumière des projecteurs, de la
// frontale, des guirlandes et du soleil (sans normale : un flocon renvoie la
// lumière dans toutes les directions, et surtout vers l'avant). Dans les
// faisceaux des projecteurs, les flocons brillent : on voit les faisceaux.
//-----------------------------------------------------------------------------
#ifndef SNOWFALL_H
#define SNOWFALL_H

#define GLEW_STATIC
#include "GL/glew.h"
#include "glm/glm.hpp"
#include "ShaderProgram.h"

class Snowfall
{
public:
	Snowfall();
	~Snowfall();

	bool build();	// contexte OpenGL requis

	// Le shader (snow.vert + snow.frag) doit déjà avoir les uniforms de la scène
	// (caméra, lumières). tau : temps du skieur. pixelScale : taille en pixels
	// d'un objet de 1 m vu à 1 m.
	void draw(ShaderProgram& shader, float tau, float pixelScale);

private:
	// Un flocon = 2 vec4, attributs d'instance (locations 1 et 2)
	struct Flake
	{
		glm::vec4 start;	// xyz : position dans la boîte (de 0 à 1), w : phase du balancement
		glm::vec4 motion;	// x : rayon (m), y : vitesse de chute (m/s), z : amplitude et w : vitesse du balancement
	};

	GLuint mVAO, mCornerVBO, mFlakeVBO;
	GLsizei mCount;
};

#endif // SNOWFALL_H
