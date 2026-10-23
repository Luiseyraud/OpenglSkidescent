//-----------------------------------------------------------------------------
// Stands.h : les deux tribunes de l'arrivée.
//
// Elles sont placées de part et d'autre du prolongement de la piste, tournées
// vers elle. Le modèle d'origine (770 000 triangles) a été coupé en deux par
// tools/extraire_sieges.py : la structure (models/tribune.obj) et UN siège
// (models/siege.obj), dessiné à chacune des 192 places de models/sieges.txt
// (instancing). Chaque place sert aussi à poser un spectateur (Crowd.h).
//
// Une guirlande d'ampoules pend entre des poteaux, devant chaque tribune,
// au-dessus du public. Tout est calculé dans le repère de la tribune, puis passé dans le
// monde avec sa matrice model.
//-----------------------------------------------------------------------------
#ifndef STANDS_H
#define STANDS_H

#include <vector>
#include <string>
#include "glm/glm.hpp"
#include "Mesh.h"
#include "Spline.h"
#include "Terrain.h"

// Une place : ce qu'il faut pour y dessiner un siège et un spectateur
struct Place
{
	glm::mat4 seat;		// matrice model du siège
	glm::vec3 feet;		// pieds du spectateur debout, devant le siège (monde)
	float yaw;			// orientation de la tribune (rad) : le public regarde vers la piste
	float along;		// position le long de la tribune (m), pour la ola
	int section;		// 0, 1, 2 : couleur du siège
};

struct Stands
{
	std::vector<glm::mat4> models;			// matrice model de chaque tribune
	std::vector<Place> places;
	std::vector<glm::vec3> bulbs;			// centre de chaque ampoule des guirlandes
	std::vector<glm::vec3> lights;			// lumières ponctuelles des guirlandes
	std::vector<Vertex> wires;				// fils et poteaux des guirlandes (triangles)
};

// Lit les places dans seatsFile (x y z section, dans le repère de la tribune)
// et calcule tout le reste. Renvoie false si le fichier manque.
bool buildStands(const Spline& piste, const Terrain& terrain, const std::string& seatsFile, Stands& stands);

#endif // STANDS_H
