//-----------------------------------------------------------------------------
// Spline.h : courbe Catmull-Rom qui passe par des points de contrôle.
//
// On s'y déplace par la DISTANCE parcourue s (en mètres), pas par le paramètre
// brut de la courbe : avec le paramètre brut, la vitesse changerait d'un
// segment à l'autre. Une table "distance cumulée -> paramètre" fait la conversion.
//-----------------------------------------------------------------------------
#ifndef SPLINE_H
#define SPLINE_H

#include <vector>
#include "glm/glm.hpp"

class Spline
{
public:
	Spline(const std::vector<glm::vec3>& points, int samplesPerSegment = 100);

	float length() const { return mLength; }
	glm::vec3 pointAt(float s) const;	// position après une distance s le long de la courbe
	glm::vec3 tangentAt(float s) const;	// direction (unitaire) de la courbe à la distance s

	// Position pour le paramètre brut u : partie entière = n° du segment, reste =
	// avancée dans le segment. u = 2 tombe exactement sur le 3e point de contrôle.
	// (Utile pour la caméra : un point clé par instant.)
	glm::vec3 evaluate(float u) const;

private:
	float paramAt(float s) const;		// conversion distance -> paramètre brut, avec la table

	std::vector<glm::vec3> mPoints;		// points de contrôle, plus un point fantôme à chaque bout
	std::vector<float> mTableU;			// table : paramètre brut ...
	std::vector<float> mTableS;			// ... et distance cumulée correspondante
	float mLength;
};

#endif // SPLINE_H
