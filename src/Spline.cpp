//-----------------------------------------------------------------------------
// Spline.cpp : courbe Catmull-Rom paramétrée par la longueur d'arc
//-----------------------------------------------------------------------------
#include "Spline.h"
#include <algorithm>
#include <cmath>

//-----------------------------------------------------------------------------
// Constructeur : copie les points et construit la table de longueur d'arc
//-----------------------------------------------------------------------------
Spline::Spline(const std::vector<glm::vec3>& points, int samplesPerSegment)
{
	// Un segment Catmull-Rom entre P1 et P2 a besoin de P0 et P3 autour.
	// Aux deux bouts, on ajoute un point fantôme dans le prolongement.
	mPoints.push_back(2.0f * points[0] - points[1]);
	mPoints.insert(mPoints.end(), points.begin(), points.end());
	mPoints.push_back(2.0f * points[points.size() - 1] - points[points.size() - 2]);

	// On échantillonne la courbe finement et on cumule les longueurs des petits
	// segments de droite entre deux échantillons.
	int numSegments = (int)points.size() - 1;
	int numSamples = numSegments * samplesPerSegment;

	mLength = 0.0f;
	glm::vec3 previous = evaluate(0.0f);
	mTableU.push_back(0.0f);
	mTableS.push_back(0.0f);

	for (int i = 1; i <= numSamples; i++)
	{
		float u = (float)i / samplesPerSegment;
		glm::vec3 current = evaluate(u);
		mLength += glm::length(current - previous);
		mTableU.push_back(u);
		mTableS.push_back(mLength);
		previous = current;
	}
}

//-----------------------------------------------------------------------------
// Point de la courbe pour le paramètre brut u (formule de Catmull-Rom)
//-----------------------------------------------------------------------------
glm::vec3 Spline::evaluate(float u) const
{
	int numSegments = (int)mPoints.size() - 3;
	int i = glm::clamp((int)std::floor(u), 0, numSegments - 1);
	float t = glm::clamp(u - i, 0.0f, 1.0f);

	const glm::vec3& p0 = mPoints[i];
	const glm::vec3& p1 = mPoints[i + 1];
	const glm::vec3& p2 = mPoints[i + 2];
	const glm::vec3& p3 = mPoints[i + 3];

	// La courbe passe par p1 (t = 0) et p2 (t = 1). p0 et p3 fixent les tangentes.
	return 0.5f * ((2.0f * p1)
	             + (-p0 + p2) * t
	             + (2.0f * p0 - 5.0f * p1 + 4.0f * p2 - p3) * t * t
	             + (-p0 + 3.0f * p1 - 3.0f * p2 + p3) * t * t * t);
}

//-----------------------------------------------------------------------------
// Distance s -> paramètre brut u : on cherche s dans la table et on interpole
//-----------------------------------------------------------------------------
float Spline::paramAt(float s) const
{
	s = glm::clamp(s, 0.0f, mLength);

	// Premier échantillon dont la distance dépasse s (recherche dichotomique)
	auto it = std::upper_bound(mTableS.begin(), mTableS.end(), s);
	if (it == mTableS.end())
		return mTableU.back();

	size_t k = it - mTableS.begin();	// mTableS[k-1] <= s < mTableS[k]
	float a = (s - mTableS[k - 1]) / (mTableS[k] - mTableS[k - 1]);
	return glm::mix(mTableU[k - 1], mTableU[k], a);
}

//-----------------------------------------------------------------------------
// Position après une distance s le long de la courbe
//-----------------------------------------------------------------------------
glm::vec3 Spline::pointAt(float s) const
{
	return evaluate(paramAt(s));
}

//-----------------------------------------------------------------------------
// Direction de la courbe à la distance s (différence finie sur 1 m)
//-----------------------------------------------------------------------------
glm::vec3 Spline::tangentAt(float s) const
{
	float s0 = glm::clamp(s - 0.5f, 0.0f, mLength - 1.0f);
	return glm::normalize(pointAt(s0 + 1.0f) - pointAt(s0));
}
