//-----------------------------------------------------------------------------
// Timeline.cpp : le storyboard de la vidéo
//-----------------------------------------------------------------------------
#include "Timeline.h"
#include "Scene.h"
#include <vector>
#include <cmath>
#include <algorithm>
#include "glm/gtc/matrix_transform.hpp"
#include "glm/gtc/constants.hpp"

namespace timeline
{
using namespace scene;

//--- Plans "à la main" : points clés (x, hauteur AU-DESSUS DU SOL, z) ---------
// Un point clé par instant régulier du plan. La caméra passe par chaque point
// (spline Catmull-Rom) : pas de mouvement en ligne brisée.

// Survol : on part très haut au nord, on regarde toute la vallée vers l'aval,
// puis on plonge vers le départ, où le skieur s'élance, et on vient se placer
// derrière lui. Un point clé tous les 2,75 s (0 ; 2,75 ; 5,5 ; 8,25 ; 11 s).
static const std::vector<glm::vec3> AERIAL_EYES = {
	{ -150.0f, 380.0f, -1300.0f },
	{  -40.0f, 220.0f,  -900.0f },
	{  -10.0f,  80.0f,  -640.0f },
	{   -8.0f,  14.0f,  -520.0f },	// derrière le départ
	{   12.0f,   4.0f,  -458.0f }	// à peu près là où sera la caméra de suivi à 11 s
};
static const std::vector<glm::vec3> AERIAL_TARGETS = {
	{   60.0f, 0.0f, -100.0f },
	{   40.0f, 0.0f, -200.0f },
	{   20.0f, 0.0f, -400.0f },
	{   10.0f, 1.5f, -470.0f },		// le skieur au départ
	{   35.0f, 2.0f, -410.0f }
};

// Arrivée : la caméra, près du chalet, glisse lentement et suit le skieur qui arrive
static const std::vector<glm::vec3> ARRIVAL_EYES = {
	{ 141.0f, 6.0f, 454.0f },
	{ 131.0f, 2.5f, 453.0f }		// à ~14 m du point d'arrêt du skieur, dans le couloir sans arbres
};

//--- Caméra de suivi ------------------------------------------------------------
const float FOLLOW_BEHIND = 14.0f;	// m derrière le skieur (environ 0,6 s de retard)
const float FOLLOW_HEIGHT = 3.0f;	// m au-dessus du sol
const float LOOK_AHEAD    = 35.0f;	// m : elle regarde entre le skieur et ce point devant lui
const float LOOK_HEIGHT   = 2.5f;	// m au-dessus du sol

//--- Plan en contre-jour ----------------------------------------------------------
const float BACKLIGHT_DISTANCE = 25.0f;	// m entre la caméra et le sommet du saut
const float BACKLIGHT_SLIDE    = 4.0f;	// m de travelling latéral de part et d'autre

//-----------------------------------------------------------------------------
// (x, hauteur au-dessus du sol, z) -> position absolue
//-----------------------------------------------------------------------------
static std::vector<glm::vec3> aboveGround(const std::vector<glm::vec3>& points, const Terrain& terrain)
{
	std::vector<glm::vec3> result;
	for (const glm::vec3& p : points)
		result.push_back(glm::vec3(p.x, terrain.height(p.x, p.z) + p.y, p.z));
	return result;
}

//-----------------------------------------------------------------------------
// Point de la piste (vu de dessus) à la distance s. Avant le départ et après
// l'arrivée, on prolonge en ligne droite (le skieur s'arrête après l'arrivée,
// la caméra de suivi démarre derrière le départ).
//-----------------------------------------------------------------------------
static glm::vec3 pistePoint(const Spline& piste, float s)
{
	if (s < 0.0f)
		return piste.pointAt(0.0f) + s * piste.tangentAt(0.0f);
	if (s > piste.length())
		return piste.pointAt(piste.length()) + (s - piste.length()) * piste.tangentAt(piste.length());
	return piste.pointAt(s);
}

//-----------------------------------------------------------------------------
// Mouvement du skieur : départ à SKI_START, accélération régulière jusqu'à
// SKIER_SPEED, vitesse constante, puis freinage régulier sur les
// SKIER_BRAKE_DISTANCE derniers mètres. Donne la distance parcourue et la
// vitesse à l'instant t.
//-----------------------------------------------------------------------------
static void skierMotion(float t, const Spline& piste, float& distance, float& speed)
{
	float v = SKIER_SPEED;
	float a = SKIER_START_ACCEL;
	float tAccel = v / a;											// durée de l'accélération
	float dAccel = 0.5f * a * tAccel * tAccel;						// distance de l'accélération
	float stop = piste.length() + SKIER_STOP_EXTRA;					// distance où il s'arrête
	float brakeStart = stop - SKIER_BRAKE_DISTANCE;					// distance où il commence à freiner
	float decel = v * v / (2.0f * SKIER_BRAKE_DISTANCE);			// v² = 2 a d : pour s'arrêter pile

	float tau = std::max(t - SKI_START, 0.0f);						// temps depuis le départ
	if (tau < tAccel)												// départ : d = a t² / 2
	{
		distance = 0.5f * a * tau * tau;
		speed = a * tau;
		return;
	}
	float tBrake = tAccel + (brakeStart - dAccel) / v;				// instant du début du freinage
	if (tau < tBrake)												// vitesse constante
	{
		distance = dAccel + v * (tau - tAccel);
		speed = v;
		return;
	}
	float tb = std::min(tau - tBrake, v / decel);					// temps passé à freiner (v/a jusqu'à l'arrêt)
	distance = brakeStart + v * tb - 0.5f * decel * tb * tb;
	speed = v - decel * tb;
}

//-----------------------------------------------------------------------------
// Temps du skieur où il passe à la distance s, quand il va à vitesse constante
// (après l'accélération, avant le freinage)
//-----------------------------------------------------------------------------
static float skierSimTime(float s)
{
	float tAccel = SKIER_SPEED / SKIER_START_ACCEL;
	float dAccel = 0.5f * SKIER_START_ACCEL * tAccel * tAccel;
	return SKI_START + tAccel + (s - dAccel) / SKIER_SPEED;
}

//-----------------------------------------------------------------------------
// Ralenti. La vitesse du temps du skieur (dtau / dt) vaut 1, sauf pendant le
// saut où elle descend à SLOW_FACTOR, avec des transitions douces (smoothstep).
// On en déduit, une fois pour toutes, l'instant de la vidéo pour chaque temps
// du skieur, tous les SLOW_STEP : t augmente de dtau / vitesse à chaque pas
// (intégration avec un pas fixe). Les deux conversions lisent cette table.
//-----------------------------------------------------------------------------
static const float SLOW_STEP = 0.001f;		// s (temps du skieur)
static std::vector<float> gVideoTimes;		// gVideoTimes[i] : instant de la vidéo pour tau = i x SLOW_STEP

void prepare(const Terrain& terrain)
{
	float takeoff = skierSimTime(terrain.jumpStart());
	float landing = skierSimTime(terrain.jumpStart() + terrain.jumpLength()) + SLOW_AFTER;

	gVideoTimes.clear();
	double t = 0.0;		// double : 60 000 additions de 0,001 en float dériveraient de quelques millisecondes
	for (int i = 0; i * SLOW_STEP <= DURATION + 1.0f; i++)
	{
		float tau = i * SLOW_STEP;
		gVideoTimes.push_back((float)t);
		float slow = glm::smoothstep(takeoff - SLOW_RAMP, takeoff, tau)
		           * (1.0f - glm::smoothstep(landing, landing + SLOW_RAMP, tau));
		t += SLOW_STEP / glm::mix(1.0f, SLOW_FACTOR, slow);
	}
}

float videoTime(float tau)
{
	if (gVideoTimes.empty() || tau <= 0.0f)
		return tau;
	float i = tau / SLOW_STEP;
	int k = (int)i;
	if (k + 1 >= (int)gVideoTimes.size())		// après la table : plus de ralenti
		return gVideoTimes.back() + (tau - (gVideoTimes.size() - 1) * SLOW_STEP);
	return glm::mix(gVideoTimes[k], gVideoTimes[k + 1], i - k);
}

float simTime(float t)
{
	if (gVideoTimes.empty() || t <= 0.0f)
		return t;
	if (t >= gVideoTimes.back())
		return (gVideoTimes.size() - 1) * SLOW_STEP + (t - gVideoTimes.back());
	// La table est croissante : recherche dichotomique de la case qui contient t
	auto it = std::upper_bound(gVideoTimes.begin(), gVideoTimes.end(), t);
	int k = (int)(it - gVideoTimes.begin()) - 1;
	float f = (t - gVideoTimes[k]) / (gVideoTimes[k + 1] - gVideoTimes[k]);
	return (k + f) * SLOW_STEP;
}

float duration()
{
	return videoTime(DURATION);
}

//-----------------------------------------------------------------------------
// Instant de la vidéo où le skieur passe à la distance s (à pleine vitesse)
//-----------------------------------------------------------------------------
float skierTime(float s)
{
	return videoTime(skierSimTime(s));
}

float skierDistance(float t, const Spline& piste)
{
	float distance, speed;
	skierMotion(simTime(t), piste, distance, speed);
	return distance;
}

SkierState skierState(float t, const Spline& piste, const Terrain& terrain)
{
	return skierStateAt(simTime(t), piste, terrain);
}

//-----------------------------------------------------------------------------
// Position et orientation du skieur au temps t du skieur
//-----------------------------------------------------------------------------
SkierState skierStateAt(float t, const Spline& piste, const Terrain& terrain)
{
	float s, speed;
	skierMotion(t, piste, s, speed);
	glm::vec3 p = pistePoint(piste, s);
	glm::vec3 tangent = piste.tangentAt(std::min(s, piste.length()));	// direction horizontale

	// Hauteur, pente (montée en mètres par mètre parcouru) et "haut" du skieur :
	// en vol, ceux de la parabole du tremplin ; au sol, ceux du terrain.
	float u = s - terrain.jumpStart();		// distance après le bord du tremplin
	bool flying = (u > 0.0f && u < terrain.jumpLength());
	float y, slope;
	glm::vec3 up;
	float flight = u / terrain.jumpLength();	// avancement du vol : 0 au décollage, 1 à l'impact
	if (flying)
	{
		y = terrain.jumpTrajectory(u);
		slope = terrain.jumpTrajectory(u + 0.5f) - terrain.jumpTrajectory(u - 0.5f);

		// Le "haut" du skieur passe de la normale du tremplin (au décollage) à celle
		// de la zone de réception (à l'impact) : pas de saut d'angle aux deux bouts.
		glm::vec3 lip = pistePoint(piste, terrain.jumpStart() - 0.5f);
		glm::vec3 landing = pistePoint(piste, terrain.jumpStart() + terrain.jumpLength() + 0.5f);
		up = glm::normalize(glm::mix(terrain.normal(lip.x, lip.z), terrain.normal(landing.x, landing.z), flight));
	}
	else
	{
		glm::vec3 ahead = pistePoint(piste, s + 0.5f);
		glm::vec3 behind = pistePoint(piste, s - 0.5f);
		y = terrain.height(p.x, p.z);
		slope = terrain.height(ahead.x, ahead.z) - terrain.height(behind.x, behind.z);
		up = terrain.normal(p.x, p.z);		// les skis à plat sur la neige
	}
	glm::vec3 forward = glm::normalize(tangent + glm::vec3(0.0f, slope, 0.0f));

	// Inclinaison dans les virages : un virage pris à la vitesse v avec une
	// courbure k (= 1 / rayon) demande de pencher d'un angle atan(v² k / g)
	// vers l'intérieur. La courbure se mesure avec la variation de la tangente.
	float lean = 0.0f;
	glm::vec3 inside(0.0f);
	if (!flying && s < piste.length())
	{
		glm::vec3 change = piste.tangentAt(s + 2.0f) - piste.tangentAt(s - 2.0f);	// sur 4 m
		float curvature = glm::length(change) / 4.0f;
		if (curvature > 1e-4f)
		{
			lean = std::min(std::atan(speed * speed * curvature / GRAVITY), 0.8f);
			inside = glm::normalize(change);	// la tangente tourne vers le centre du virage
			up = glm::normalize(std::cos(lean) * up + std::sin(lean) * inside);
		}
	}

	// Repère du skieur : le modèle regarde vers +Z, haut vers +Y, et X = Y x Z
	glm::vec3 side = glm::normalize(glm::cross(up, forward));
	forward = glm::cross(side, up);		// l'avant, rendu exactement perpendiculaire au haut
	glm::mat4 rotation(glm::vec4(side, 0.0f), glm::vec4(up, 0.0f), glm::vec4(forward, 0.0f), glm::vec4(0.0f, 0.0f, 0.0f, 1.0f));

	// Petite flexion des jambes (le modèle s'écrase de 3 % au plus), quand il skie
	float bend = 0.0f;
	if (speed > 1.0f && !flying)
		bend = 0.03f * (0.5f + 0.5f * std::sin(glm::two_pi<float>() * t / 1.2f));

	// Réception : il s'écrase de 12 % à l'impact, puis se redresse sur 8 m
	float afterLanding = u - terrain.jumpLength();
	if (afterLanding >= 0.0f && afterLanding < 8.0f)
		bend += 0.12f * (1.0f - glm::smoothstep(0.0f, 8.0f, afterLanding));

	// Backflip pendant le vol : un tour complet vers l'arrière, autour de l'axe
	// latéral (X du modèle) et du centre de gravité. Le smoothstep va de 0,05 à 0,77
	// du vol : son milieu (0,41) tombe au sommet du saut, où le skieur est donc
	// exactement à l'envers, devant le soleil. Le tour finit avant l'impact.
	// Angle négatif = l'avant des skis monte d'abord : c'est un salto arrière.
	glm::mat4 flip(1.0f);
	if (flying)
	{
		float angle = glm::two_pi<float>() * glm::smoothstep(0.05f, 0.77f, flight);
		flip = glm::translate(glm::mat4(1.0f), SKIER_CENTER_OF_MASS)
		     * glm::rotate(glm::mat4(1.0f), -angle, glm::vec3(1.0f, 0.0f, 0.0f))
		     * glm::translate(glm::mat4(1.0f), -SKIER_CENTER_OF_MASS);
	}

	SkierState state;
	state.position = glm::vec3(p.x, y, p.z);
	state.model = glm::translate(glm::mat4(1.0f), state.position) * rotation * flip
	            * glm::scale(glm::mat4(1.0f), glm::vec3(1.0f, 1.0f - bend, 1.0f));
	state.flying = flying;
	state.speed = speed;
	state.lean = lean;
	state.inside = inside;
	return state;
}

//-----------------------------------------------------------------------------
// Plan à points clés : progress va de 0 (début du plan) à 1 (fin)
//-----------------------------------------------------------------------------
static glm::vec3 keyframe(const Spline& points, int count, float progress)
{
	return points.evaluate(glm::clamp(progress, 0.0f, 1.0f) * (count - 1));	// u = k sur le k-ième point clé
}

//-----------------------------------------------------------------------------
// Caméra de suivi, derrière le skieur et un peu au-dessus.
// Sans état : elle se place sur la piste, FOLLOW_BEHIND mètres derrière le skieur,
// et vise entre le skieur et un point plus loin devant lui.
//-----------------------------------------------------------------------------
static CameraState followCamera(float t, const Spline& piste, const Terrain& terrain)
{
	float s = skierDistance(t, piste);
	glm::vec3 behind = pistePoint(piste, s - FOLLOW_BEHIND);
	glm::vec3 ahead = pistePoint(piste, s + LOOK_AHEAD);
	glm::vec3 eye(behind.x, terrain.height(behind.x, behind.z) + FOLLOW_HEIGHT, behind.z);
	glm::vec3 lookAhead(ahead.x, terrain.height(ahead.x, ahead.z) + LOOK_HEIGHT, ahead.z);
	glm::vec3 skier = skierState(t, piste, terrain).position + glm::vec3(0.0f, 1.0f, 0.0f);
	return { eye, glm::mix(skier, lookAhead, 0.5f), 55.0f };
}

//-----------------------------------------------------------------------------
// Position et visée de la caméra à l'instant t
//-----------------------------------------------------------------------------
CameraState cameraState(float t, const Spline& piste, const Terrain& terrain)
{
	// Splines des plans à points clés, construites une seule fois (au premier appel)
	static const Spline aerialEyes(aboveGround(AERIAL_EYES, terrain));
	static const Spline aerialTargets(aboveGround(AERIAL_TARGETS, terrain));
	static const Spline arrivalEyes(aboveGround(ARRIVAL_EYES, terrain));

	//--- 0-11 s : plan large aérien, qui se fond dans la caméra de suivi -------
	if (t < AERIAL_END)
	{
		float p = t / AERIAL_END;
		int n = (int)AERIAL_EYES.size();
		CameraState aerial = { keyframe(aerialEyes, n, p), keyframe(aerialTargets, n, p), 45.0f };

		// Pas de coupe : de AERIAL_BLEND à AERIAL_END, on mélange le survol et la caméra
		// de suivi (position, point visé et champ de vision), avec un poids qui passe
		// de 0 à 1 en douceur (smoothstep). Les deux mouvements sont réguliers, leur
		// mélange aussi : la caméra glisse d'un plan à l'autre.
		float w = glm::smoothstep(AERIAL_BLEND, AERIAL_END, t);
		if (w <= 0.0f)
			return aerial;
		CameraState follow = followCamera(t, piste, terrain);
		return { glm::mix(aerial.eye, follow.eye, w), glm::mix(aerial.target, follow.target, w),
		         glm::mix(aerial.fov, follow.fov, w) };
	}

	//--- 53-60 s : arrivée au chalet : on glisse lentement en suivant le skieur --
	float arrivalStart = videoTime(ARRIVAL_START);
	if (t >= arrivalStart)
	{
		float p = (t - arrivalStart) / (duration() - arrivalStart);
		float easeOut = 1.0f - (1.0f - p) * (1.0f - p);	// rapide au début, lent à la fin
		glm::vec3 skier = skierState(t, piste, terrain).position;
		return { keyframe(arrivalEyes, (int)ARRIVAL_EYES.size(), easeOut), skier + glm::vec3(0.0f, 0.9f, 0.0f), 50.0f };
	}

	//--- 35.5-40.5 s : le saut en contre-jour ------------------------------------
	float backlightEnd = videoTime(BACKLIGHT_END);	// le plan dure plus longtemps : il contient le ralenti
	if (t >= BACKLIGHT_START && t < backlightEnd)
	{
		// A = sommet de la parabole du saut (vy / vx = tan(theta), vx = SKIER_SPEED).
		// Le tremplin est sur la ligne droite x = 0, son bord en z = JUMP_Z.
		float uApex = SKIER_SPEED * SKIER_SPEED * std::tan(glm::radians(JUMP_ANGLE_DEG)) / GRAVITY;
		glm::vec3 A(0.0f, terrain.jumpTrajectory(uApex), JUMP_Z + uApex);

		// C = A - L * distance : vu de C, le soleil est exactement derrière A
		glm::vec3 C = A - BACKLIGHT_DISTANCE * sunDirection();

		// Travelling latéral : caméra et cible glissent ensemble, le soleil reste au centre.
		// Il est centré sur le sommet du saut : à cet instant, le décalage est nul et le
		// skieur passe exactement devant le soleil.
		float tApex = skierTime(terrain.jumpStart() + uApex);
		float tLanding = skierTime(terrain.jumpStart() + terrain.jumpLength());
		float p = glm::clamp((t - tApex) / (backlightEnd - BACKLIGHT_START), -0.5f, 0.5f);
		glm::vec3 slide(0.0f, 0.0f, 2.0f * BACKLIGHT_SLIDE * p);

		// Après le sommet, la caméra tourne doucement vers le point de réception, et
		// y reste : on voit la gerbe de neige de la réception, à contre-jour
		glm::vec3 landing = pistePoint(piste, terrain.jumpStart() + terrain.jumpLength());
		landing.y = terrain.height(landing.x, landing.z) + 1.0f;
		glm::vec3 target = glm::mix(A + slide, landing, glm::smoothstep(tApex, tLanding + 0.3f, t));
		return { C + slide, target, 35.0f };
	}

	//--- Sinon : caméra de suivi ---------------------------------------------------
	return followCamera(t, piste, terrain);
}

}
