//-----------------------------------------------------------------------------
// Timeline.h : le storyboard de la vidéo.
//
// Tout est fonction du temps t (en secondes) : le skieur et la caméra se
// calculent directement à partir de t, sans mémoire des images précédentes.
// Le mode interactif, --record et --frame donnent donc la même image pour un même t.
// Changer de plan (une "coupe"), c'est simplement changer de calcul à un instant donné.
//-----------------------------------------------------------------------------
#ifndef TIMELINE_H
#define TIMELINE_H

#include "glm/glm.hpp"
#include "Scene.h"
#include "Spline.h"
#include "Terrain.h"

namespace timeline
{
	const float DURATION = 60.0f;			// s, durée de la vidéo

	// Les plans :
	const float AERIAL_BLEND    = 6.5f;		//  0-11 s : plan large aérien qui plonge vers le départ ;
	const float AERIAL_END      = 11.0f;	//  de 6,5 à 11 s, il se fond dans la caméra de suivi (sans coupe)

	// Le skieur part à 6,25 s et accélère (SKIER_START_ACCEL) jusqu'à SKIER_SPEED.
	// 6,25 = 9 - v / (2a) : ensuite, il passe partout au même instant qu'un skieur
	// lancé à pleine vitesse à 9 s (il atteint le tremplin vers 37 s).
	const float SKI_START       = 9.0f - scene::SKIER_SPEED / (2.0f * scene::SKIER_START_ACCEL);
	const float BACKLIGHT_START = 35.5f;	// 35.5-40.5 s : saut en contre-jour
	const float BACKLIGHT_END   = 40.5f;
	const float ARRIVAL_START   = 53.0f;	// 53-60 s : arrivée au chalet
	// Entre ces plans : caméra de suivi, derrière le skieur

	struct SkierState
	{
		glm::mat4 model;	// matrice model : position, orientation, flexion
		glm::vec3 position;	// position des pieds
		bool flying;		// en l'air pendant le saut
		float speed;		// m/s (horizontale)
		float lean;			// inclinaison dans le virage, en radians (0 en ligne droite)
		glm::vec3 inside;	// direction (horizontale) du centre du virage ; nulle en ligne droite
	};

	struct CameraState
	{
		glm::vec3 eye;		// position
		glm::vec3 target;	// point regardé
		float fov;			// champ de vision vertical, en degrés
	};

	float skierDistance(float t, const Spline& piste);	// distance parcourue sur la piste à l'instant t
	float skierTime(float s);							// instant où il passe à la distance s (après le départ)
	SkierState skierState(float t, const Spline& piste, const Terrain& terrain);
	CameraState cameraState(float t, const Spline& piste, const Terrain& terrain);
}

#endif // TIMELINE_H
