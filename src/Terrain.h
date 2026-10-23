//-----------------------------------------------------------------------------
// Terrain.h : la montagne, générée par le code (aucun fichier image).
//
// La hauteur h(x, z) est connue partout. C'est la somme de :
//   1. un profil de pente qui ne dépend que de z (raide en haut, plat en bas) ;
//   2. un relief (bruit, versants de la vallée, bosses) qui s'efface sur la piste ;
//   3. le tremplin, posé sur la piste.
//
// Astuce : la piste descend toujours vers +Z, on peut donc la décrire comme une
// fonction x = xPiste(z). La distance d'un point à la piste se calcule alors
// directement, sans chercher le point de la piste le plus proche.
//-----------------------------------------------------------------------------
#ifndef TERRAIN_H
#define TERRAIN_H

#include <vector>
#define GLEW_STATIC
#include "GL/glew.h"
#include "glm/glm.hpp"
#include "Spline.h"

class Terrain
{
public:
	Terrain(const Spline& piste);
	~Terrain();

	float height(float x, float z) const;		// hauteur du sol en (x, z)
	float meshHeight(float x, float z) const;	// hauteur des triangles dessinés en (x, z)
	glm::vec3 normal(float x, float z) const;	// normale du sol en (x, z)
	float distanceToPiste(float x, float z) const;	// distance horizontale à l'axe de la piste

	// Le saut : distance du bord du tremplin sur la piste, longueur du vol,
	// et hauteur du skieur en vol u mètres après le bord
	float jumpStart() const { return mJumpS; }
	float jumpLength() const { return mLandingU; }
	float jumpTrajectory(float u) const;

	bool buildMesh();	// crée la grille de triangles sur la carte graphique (contexte OpenGL requis)
	void draw();

private:
	float lookup(const std::vector<float>& table, float z) const;
	float jumpHeight(float u, float base) const;
	float naturalHeight(float x, float z) const;	// hauteur avant l'aplanissement des FLAT_ZONES

	std::vector<float> mFlatHeights;	// hauteur de chaque zone aplanie

	// Tables indexées par z, avec un pas de TABLE_STEP mètres
	std::vector<float> mBase;		// hauteur du profil de pente
	std::vector<float> mBaseSmooth;	// le même, lissé (utilisé loin de la piste)
	std::vector<float> mPisteX;		// x de l'axe de la piste
	std::vector<float> mPisteS;		// distance parcourue sur la piste
	std::vector<float> mPisteCos;	// cosinus de l'angle entre la piste et l'axe Z
	std::vector<float> mValleyX;	// x de l'axe de la vallée (la piste lissée)
	float mPisteLength;

	// Tremplin (calculé dans le constructeur). u = distance après le bord.
	float mJumpS;			// distance du bord du tremplin sur la piste
	float mLipY;			// hauteur du bord
	float mKickerHeight;	// hauteur de la bosse d'élan au-dessus de la pente
	float mTableEnd;		// u où la table s'arrête et la zone de réception commence
	float mLandingSlope;	// pente de la zone de réception (négative : elle descend)
	float mLandingU;		// u de l'impact : longueur du vol
	float mJumpEnd;			// u de la fin du tremplin : la réception a rejoint la piste

	GLuint mVAO, mVBO, mEBO;
	GLsizei mIndexCount;
};

#endif // TERRAIN_H
