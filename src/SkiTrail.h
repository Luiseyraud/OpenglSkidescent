//-----------------------------------------------------------------------------
// SkiTrail.h : la trace des skis dans la neige, qui reste derrière le skieur.
//
// Le trajet du skieur est connu d'avance (timeline::skierState). On construit
// donc toute la trace une seule fois, au chargement : un ruban posé sur la
// neige, avec deux sillons (un par ski), coupé pendant le saut. On note
// l'instant où le skieur passe sur chaque section du ruban.
// Pour dessiner l'image de l'instant t, on ne dessine que le début du ruban,
// jusqu'à la section où se trouve le skieur. Aucun calcul par image, et la
// même image en mode interactif, --frame et --record.
//
// Les sillons ne sont pas creusés dans le terrain (ses sommets sont espacés
// de 1 m, un sillon fait 7 cm de large). Ils sont dessinés par la texture du
// ruban : avec le soleil rasant, un sillon est entièrement dans l'ombre, il
// apparaît comme une ligne gris-bleu. Leurs bords ont en plus une normale
// inclinée : le bord tourné vers le soleil s'éclaire. Les deux bords du ruban
// s'effacent (alpha), pour qu'on ne voie pas où il commence.
//-----------------------------------------------------------------------------
#ifndef SKI_TRAIL_H
#define SKI_TRAIL_H

#include <vector>
#define GLEW_STATIC
#include "GL/glew.h"
#include "Spline.h"
#include "Terrain.h"
#include "Texture2D.h"

class SkiTrail
{
public:
	SkiTrail();
	~SkiTrail();

	bool build(const Spline& piste, const Terrain& terrain);	// contexte OpenGL requis
	void draw(float t);		// dessine la trace laissée jusqu'à l'instant t

private:
	bool createTexture();

	Texture2D mTexture;
	std::vector<GLsizei> mVertexCount;	// sommets à dessiner quand le skieur a passé la section i
	GLuint mVAO, mVBO;
};

#endif // SKI_TRAIL_H
