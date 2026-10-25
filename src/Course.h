//-----------------------------------------------------------------------------
// Course.h : l'équipement de la piste de course, comme sur un super-G :
//   - les bandes bleues peintes sur la neige, le long des deux bords ;
//   - les drapeaux rouges qui balisent les deux bords ;
//   - les filets de sécurité rouges, à l'extérieur des virages serrés ;
//   - les pylônes d'éclairage, par paires de part et d'autre de la piste.
//
// Tout est calculé à partir de la spline de la piste (position et direction à
// la distance s) et de la hauteur du terrain. Les bandes et les filets sont des
// maillages créés par le code (Mesh::create) ; les drapeaux sont un modèle
// dessiné en plusieurs exemplaires (instancing), comme les sapins.
//-----------------------------------------------------------------------------
#ifndef COURSE_H
#define COURSE_H

#include <vector>
#include "glm/glm.hpp"
#include "Mesh.h"
#include "Spline.h"
#include "Terrain.h"

// Bandes bleues : un ruban de triangles posé sur la neige, de chaque côté
std::vector<Vertex> buildLines(const Spline& piste, const Terrain& terrain);

// Filets : les panneaux de filet (net) et leurs poteaux (poles)
void buildNets(const Spline& piste, const Terrain& terrain, std::vector<Vertex>& net, std::vector<Vertex>& poles);

// Drapeaux : une matrice model par drapeau
std::vector<glm::mat4> placeFlags(const Spline& piste, const Terrain& terrain);

// Un faisceau : un groupe de projecteurs d'un pylône, réglés sur le même point.
// C'est un spot dans le shader.
struct Floodlight
{
	glm::vec3 position;		// d'où part la lumière (devant les projecteurs)
	glm::vec3 direction;	// axe du faisceau (unitaire)
	float power;			// puissance (multiplie FLOODLIGHT_COLOR)
	float cosInner;			// cône du faisceau : plein jusqu'à cet angle (cosinus),
	float cosOuter;			// éteint au-delà
};

// Un pylône d'éclairage : sa matrice model et ses deux faisceaux (près et loin)
struct LightMast
{
	glm::mat4 model;
	Floodlight beams[2];
};
std::vector<LightMast> placeLightMasts(const Spline& piste, const Terrain& terrain);

// Texture du filet, calculée pixel par pixel (RGBA, ligne 0 = bas de l'image)
std::vector<unsigned char> netPixels(int& width, int& height);

#endif // COURSE_H
