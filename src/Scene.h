//-----------------------------------------------------------------------------
// Scene.h : tous les réglages de la scène au même endroit
// (soleil, tracé de la piste, profil de la pente, tremplin, relief).
//
// Conventions : 1 unité = 1 mètre, Y vers le haut, on descend vers +Z.
//-----------------------------------------------------------------------------
#ifndef SCENE_H
#define SCENE_H

#include <vector>
#include <cmath>
#include "glm/glm.hpp"

namespace scene
{
	//--- Soleil ---------------------------------------------------------------
	// L = direction VERS le soleil : à l'ouest (-X), 10° au-dessus de l'horizon.
	// Il est ainsi sur le côté du skieur au moment du saut (qui va vers +Z).
	// Rappel : le shader du cours attend sunLight.direction = -L.
	const float SUN_ELEVATION_DEG = 10.0f;

	inline glm::vec3 sunDirection()
	{
		float e = glm::radians(SUN_ELEVATION_DEG);
		return glm::vec3(-std::cos(e), std::sin(e), 0.0f);
	}

	//--- Couleurs (linéaires : la correction gamma est faite à l'affichage) ----
	const glm::vec3 SUN_COLOR(1.0f, 0.65f, 0.35f);			// lumière du soleil couchant
	const float     SUN_INTENSITY = 1.6f;
	const glm::vec3 SKY_ZENITH(0.06f, 0.13f, 0.32f);		// ciel en haut
	const glm::vec3 SKY_HORIZON(0.50f, 0.42f, 0.52f);		// horizon loin du soleil
	const glm::vec3 SKY_HORIZON_SUN(1.0f, 0.50f, 0.20f);	// horizon du côté du soleil
	const glm::vec3 AMBIENT_SKY(0.10f, 0.14f, 0.26f);		// ambiante venant du ciel (bleutée)
	const glm::vec3 AMBIENT_GROUND(0.10f, 0.08f, 0.08f);	// ambiante renvoyée par le sol
	const float     FOG_DENSITY = 0.00045f;					// brouillard : 1 / distance caractéristique (m)

	//--- Tracé de la piste (vu de dessus) -------------------------------------
	// Points de contrôle de la spline Catmull-Rom : (x, 0, z).
	// z doit toujours augmenter (le skieur descend vers +Z).
	// Les 5 points sur x = 0 donnent une ligne droite de z = 30 à z = 150 pour le
	// tremplin : un segment Catmull-Rom est droit quand ses 4 points sont alignés.
	inline const std::vector<glm::vec3> PISTE_POINTS = {
		{   0, 0, -480 },	// départ
		{  45, 0, -400 },
		{  90, 0, -320 },
		{  60, 0, -240 },
		{ -10, 0, -165 },
		{ -50, 0,  -95 },
		{   0, 0,  -30 },
		{   0, 0,   30 },	// début de la ligne droite
		{   0, 0,   90 },
		{   0, 0,  150 },	// fin de la ligne droite
		{   0, 0,  210 },
		{  50, 0,  280 },
		{ 110, 0,  350 },
		{ 120, 0,  420 }	// arrivée (le chalet sera près d'ici)
	};

	const float PISTE_HALF_WIDTH = 15.0f;	// demi-largeur de la piste damée
	const float PISTE_MARGIN     = 40.0f;	// largeur de la transition piste -> relief naturel

	//--- Profil de la pente ---------------------------------------------------
	// Pente (en m de descente par m parcouru vers +Z) à quelques valeurs de z.
	// Entre deux clés, la pente varie linéairement.
	struct SlopeKey { float z, slope; };

	inline const std::vector<SlopeKey> SLOPE_PROFILE = {
		{ -2000.0f, 0.00f },	// plateau sommital, au loin
		{ -1400.0f, 0.30f },
		{ -1000.0f, 0.80f },	// sommet, au-dessus du départ
		{  -560.0f, 0.55f },
		{  -500.0f, 0.32f },	// départ
		{   -60.0f, 0.35f },
		{     0.0f, 0.15f },	// replat du tremplin
		{   130.0f, 0.15f },
		{   170.0f, 0.30f },
		{   330.0f, 0.28f },
		{   400.0f, 0.06f },	// aire d'arrivée presque plate
		{   700.0f, 0.02f }
	};

	// Loin de la piste, le profil est lissé (moyenne sur ±PROFILE_SMOOTHING),
	// avec une transition qui finit à PROFILE_BLEND mètres de la piste.
	const float PROFILE_SMOOTHING = 200.0f;
	const float PROFILE_BLEND     = 250.0f;

	//--- Tremplin -------------------------------------------------------------
	// La zone de réception est calculée à partir de la trajectoire prévue du saut
	// (vitesse, angle, temps en l'air), pour que le skieur retombe dessus.
	const float SKIER_SPEED      = 22.0f;	// m/s, vitesse horizontale du skieur (et au décollage)
	const float SKIER_START_ACCEL = 4.0f;	// m/s² : au départ, il accélère jusqu'à SKIER_SPEED (en 5,5 s)
	const float JUMP_Z           = 50.0f;	// position du bord du tremplin (sur la ligne droite)
	const float JUMP_ANGLE_DEG   = 15.0f;	// angle de décollage au-dessus de l'horizontale
	const float JUMP_AIR_TIME    = 1.45f;	// s, temps en l'air prévu
	const float KICKER_LENGTH    = 12.0f;	// longueur de la bosse d'élan
	const float TABLE_DROP       = 0.5f;	// la table est un peu plus basse que le bord
	const float LANDING_RATIO    = 0.6f;	// pente de réception = 60 % de celle de la trajectoire à l'impact
	const float JUMP_HALF_WIDTH  = 6.0f;	// demi-largeur du dessus du tremplin
	const float JUMP_RUNOUT      = 40.0f;	// m après l'impact pour que la réception rejoigne la piste

	// Backflip : le skieur tourne de 360° autour de son axe latéral (épaule à épaule),
	// autour de son centre de gravité (le bassin), et non de l'origine du modèle (les pieds).
	const glm::vec3 SKIER_CENTER_OF_MASS(0.0f, 0.65f, -0.1f);	// dans le repère du modèle
	const float JUMP_SIDE        = 5.0f;	// largeur des flancs du tremplin
	const float GRAVITY          = 9.81f;

	//--- Relief autour de la piste ---------------------------------------------
	const float NOISE_SCALE       = 500.0f;	// m, taille des plus grosses ondulations du bruit
	const float NOISE_PERSISTENCE = 0.4f;	// poids de chaque couche de bruit par rapport à la précédente
	const float NOISE_NEAR        = 4.0f;	// m, amplitude du bruit près de la piste
	const float NOISE_FAR         = 70.0f;	// m, amplitude du bruit loin de la piste
	const float VALLEY_SMOOTHING  = 150.0f;	// m, l'axe de la vallée = la piste lissée sur ±150 m
	const float VALLEY_HEIGHT     = 150.0f;	// m, hauteur des versants de la vallée
	const float VALLEY_START      = 60.0f;	// m, distance à l'axe où les versants commencent
	const float VALLEY_END        = 450.0f;	// m, distance où ils atteignent leur hauteur
	const float VALLEY_RISE       = 0.06f;	// au-delà, le sol monte encore de 6 cm par mètre

	// Bosses posées à la main (gaussiennes) : x, z, rayon, hauteur (négative = creux)
	struct Bump { float x, z, radius, height; };

	inline const std::vector<Bump> BUMPS = {
		{ -450.0f,   50.0f, 300.0f, -150.0f },	// brèche à l'ouest du tremplin : le soleil s'y couche
		{ -1100.0f,  50.0f, 400.0f, -150.0f },	// ... prolongée vers l'ouest (vallée latérale)
		{ -180.0f,  420.0f, 110.0f,   80.0f },	// crête à l'ouest du chalet : le met à l'ombre
		{ -480.0f, -550.0f, 240.0f,  260.0f },	// sommets, pour la ligne d'horizon
		{  480.0f, -650.0f, 220.0f,  220.0f },
		{  520.0f,    0.0f, 200.0f,  160.0f }
	};

	//--- Grille du terrain -----------------------------------------------------
	// Cases de 1 m dans la zone fine (autour de la piste), puis de plus en plus
	// grandes vers les bords (+3 % par case, jusqu'à 8 m).
	const float TERRAIN_X_MIN = -2200.0f, TERRAIN_X_MAX = 2200.0f;
	const float TERRAIN_Z_MIN = -2600.0f, TERRAIN_Z_MAX = 2000.0f;
	const float TERRAIN_FINE_X_MIN = -250.0f, TERRAIN_FINE_X_MAX = 300.0f;
	const float TERRAIN_FINE_Z_MIN = -600.0f, TERRAIN_FINE_Z_MAX = 550.0f;
	const float TERRAIN_CELL        = 1.0f;		// m, taille d'une case dans la zone fine
	const float TERRAIN_CELL_GROWTH = 1.03f;
	const float TERRAIN_MAX_CELL    = 8.0f;		// m
	const float SNOW_TILE     = 6.0f;	// m, taille d'une répétition de la texture de neige

	//--- Arrivée : chalet, skieur, lampadaires ---------------------------------
	// Positions (x, 0, z) : la hauteur vient du terrain.
	const glm::vec3 CHALET_POS(155.0f, 0.0f, 470.0f);
	const float     CHALET_ANGLE_DEG = -145.0f;		// rotation autour de Y : la porte (+Z du modèle) face à l'arrivée

	// Le skieur continue tout droit après la fin de la piste et s'arrête
	// SKIER_STOP_EXTRA mètres plus loin, en freinant sur SKIER_BRAKE_DISTANCE mètres.
	const float SKIER_STOP_EXTRA     = 20.0f;
	const float SKIER_BRAKE_DISTANCE = 40.0f;

	inline const std::vector<glm::vec3> LAMP_POSTS = {
		{ 100.0f, 0.0f, 405.0f },
		{ 147.0f, 0.0f, 447.0f },		// (déplacé de (140, 428), où est maintenant la tribune est)
		{ 142.0f, 0.0f, 458.0f }
	};
	const float LAMP_BULB_HEIGHT = 3.8f;	// hauteur de l'ampoule du lampadaire (valeur du cours)

	// Zones aplanies (le chalet doit être posé à plat) : x, z, rayon
	struct FlatZone { float x, z, radius; };
	inline const std::vector<FlatZone> FLAT_ZONES = {
		{ CHALET_POS.x, CHALET_POS.z, 12.0f },
		{ 121.4f, 430.0f, 26.0f }		// aire d'arrivée et ses deux tribunes (voir STAND_*)
	};

	//--- Lumières de l'arrivée (couleurs linéaires) -----------------------------
	const int MAX_POINT_LIGHTS = 10;	// comme dans shaders/scene.frag : 3 lampadaires, le chalet, 6 pour les guirlandes
	const glm::vec3 LAMP_COLOR(1.0f, 0.62f, 0.28f);		// lampadaires et chalet : chaud
	const float     LAMP_INTENSITY = 4.0f;
	const glm::vec3 HEADLAMP_COLOR(0.85f, 0.92f, 1.0f);	// frontale du skieur : blanc froid
	const float     HEADLAMP_INTENSITY = 3.0f;
	// Positions relatives à l'objet (avant sa rotation) :
	const glm::vec3 CHALET_LIGHT_OFFSET(0.0f, 2.2f, 5.5f);	// lampe devant la porte du chalet
	const glm::vec3 HEADLAMP_OFFSET(0.0f, 1.05f, 0.25f);	// frontale : sur le casque du skieur
	const glm::vec3 HEADLAMP_DIRECTION(0.0f, -0.3f, 1.0f);	// vers l'avant, un peu vers le sol

	//--- Équipement de course, comme sur un super-G ------------------------------
	// Distances mesurées depuis l'axe de la piste, perpendiculairement à elle.
	// Couleurs en sRGB (0 à 255), comme dans un logiciel de dessin : elles vont dans des textures.
	struct Color8 { unsigned char r, g, b; };

	// La piste de course, balisée par les bandes et les drapeaux, est plus étroite
	// (20 m) que la piste damée (2 x PISTE_HALF_WIDTH = 30 m)
	const float COURSE_HALF_WIDTH = 10.0f;

	// Bandes bleues peintes sur la neige, des deux côtés, sur toute la piste
	const float  LINE_OFFSET = COURSE_HALF_WIDTH - 0.5f;	// juste avant le bord de la piste de course
	const float  LINE_WIDTH  = 0.4f;	// m
	const float  LINE_LIFT   = 0.03f;	// m au-dessus de la neige, contre le z-fighting
	const Color8 LINE_COLOR  = { 25, 75, 210 };

	// Drapeaux rouges (models/drapeau.obj) : balisage des deux bords
	const float FLAG_OFFSET  = COURSE_HALF_WIDTH + 0.5f;	// juste après le bord
	const float FLAG_SPACING = 25.0f;	// m entre deux drapeaux du même bord

	// Filets de sécurité rouges : à l'extérieur des virages serrés, au bord de la piste
	// damée (là où on passe du plat de la piste à la pente)
	const float  NET_OFFSET         = PISTE_HALF_WIDTH + 2.0f;
	const float  NET_MIN_CURVATURE  = 1.0f / 100.0f;	// filet si le rayon du virage fait moins de 100 m
	const float  NET_EXTRA          = 15.0f;	// m de filet en plus avant et après le virage
	const float  NET_HEIGHT         = 2.5f;		// m
	const float  NET_POLE_SPACING   = 4.0f;		// m entre deux poteaux
	const float  NET_SAG            = 0.12f;	// m : le haut du filet pend un peu entre deux poteaux
	const Color8 NET_COLOR          = { 200, 20, 20 };
	const Color8 NET_POLE_COLOR     = { 50, 50, 55 };

	// Pylônes d'éclairage (models/pylone.obj et projecteurs.obj) : par paires face
	// à face, de part et d'autre de la piste. Chaque pylône est un spot dans le
	// shader. Les deux pylônes d'une paire visent l'axe de la piste FLOODLIGHT_AIM
	// mètres plus bas : comme sur les vraies pistes éclairées, la lumière va vers
	// l'aval, pour ne pas éblouir les skieurs qui descendent.
	const int   MAX_FLOODLIGHTS    = 16;		// comme MAX_FLOODLIGHTS dans shaders/scene.frag
	const float FLOODLIGHT_FIRST   = 90.0f;		// m : position sur la piste de la première paire
	const float FLOODLIGHT_SPACING = 120.0f;	// m entre deux paires
	const float FLOODLIGHT_OFFSET  = 21.0f;		// m de l'axe : derrière les filets
	const float FLOODLIGHT_AIM     = 25.0f;		// m
	const glm::vec3 FLOODLIGHT_HEAD(0.6f, 13.2f, 0.0f);	// devant les 15 projecteurs (repère du modèle, ils regardent vers +X)
	const glm::vec3 FLOODLIGHT_COLOR(0.80f, 0.90f, 1.0f);	// blanc froid (LED, ~6 000 K)
	const float FLOODLIGHT_INTENSITY = 5.0f;
	const float FLOODLIGHT_INNER_DEG = 8.0f;	// cône du faisceau : plein jusqu'à 8°,
	const float FLOODLIGHT_OUTER_DEG = 30.0f;	// puis s'éteint progressivement jusqu'à 30°
	const float FLOODLIGHT_QUADRATIC = 0.01f;	// atténuation 1 / (1 + 0.01 d²) : en 1/d² au loin, comme en vrai
	const glm::vec3 FLOODLIGHT_LENS_COLOR(0.92f, 0.96f, 1.0f);	// les vitres des projecteurs, allumées
	const float FLOODLIGHT_GLOW_SIZE     = 5.0f;	// m : demi-largeur du halo (Glow.h)
	const float FLOODLIGHT_GLOW_STRENGTH = 1.0f;

	//--- Tribunes de l'arrivée, public et guirlandes --------------------------------
	// Deux tribunes (models/tribune.obj + models/siege.obj aux places de
	// models/sieges.txt), face à face de part et d'autre de l'aire d'arrivée,
	// sur le prolongement de la piste (aire aplanie : voir FLAT_ZONES).
	const float STAND_ALONG    = 10.0f;		// m après la fin de la piste : centre des tribunes
	const float STAND_OFFSET   = 15.0f;		// m entre l'axe et l'avant d'une tribune
	const float STAND_HALF_DEPTH = 4.32f;	// m, de l'avant au centre (mesuré sur le modèle)
	const Color8 SEAT_COLORS[3] = { { 255, 38, 38 }, { 255, 230, 38 }, { 35, 46, 231 } };	// rouge, jaune, bleu (couleurs du modèle)

	// Public (Crowd.h) : un spectateur sur presque chaque siège
	const float CROWD_FILL     = 0.92f;		// part des sièges occupés
	const float CHEER_DISTANCE = 150.0f;	// m : le public se lève quand le skieur est à 150 m de l'arrivée
	const float OLA_SPEED      = 9.0f;		// m/s : vitesse de la ola le long des tribunes (avant l'arrivée)
	const float OLA_PERIOD     = 9.0f;		// s entre deux olas

	// Guirlandes : une rangée d'ampoules suspendue entre des poteaux, devant chaque
	// tribune, au-dessus du public. 3 lumières ponctuelles par tribune représentent leur éclairage.
	const int   GARLAND_POSTS      = 5;		// poteaux par tribune
	const float GARLAND_HEIGHT     = 4.4f;	// m, haut des poteaux (au-dessus du pied de la tribune)
	const float GARLAND_SAG        = 0.35f;	// m : le fil pend entre deux poteaux
	const float GARLAND_SPACING    = 0.75f;	// m entre deux ampoules
	const float GARLAND_BULB_SCALE = 0.3f;	// taille des ampoules (models/ampoule.obj fait 44 cm)
	const glm::vec3 GARLAND_COLOR(1.0f, 0.70f, 0.38f);	// lumière chaude
	const float GARLAND_INTENSITY  = 3.0f;

	//--- Effets de neige -----------------------------------------------------------
	// Trace des skis (SkiTrail.h) : deux sillons, un sous chaque ski
	const float SKI_GAP          = 0.2f;	// m entre les deux skis (mesuré sur le modèle)
	const float SKI_TAIL         = -0.9f;	// z de l'arrière des skis, dans le repère du modèle
	const float TRAIL_GROOVE     = 0.07f;	// m, largeur d'un sillon (la largeur d'un ski)
	const float TRAIL_WALL_DEG   = 25.0f;	// inclinaison des bords du sillon (leur normale)
	const float TRAIL_SAMPLES    = 60.0f;	// sections de trace par seconde

	// Gerbes de neige (SnowSpray.h) : particules émises par l'arrière des skis
	const float SPRAY_MIN_LEAN   = 0.25f;	// rad : pas de gerbe pour un virage plus doux (rayon > ~190 m)
	const float SPRAY_FULL_LEAN  = 0.6f;	// rad : gerbe maximale (rayon < ~70 m)
	const float SPRAY_RATE       = 90.0f;	// particules par seconde, au maximum
	const int   SPRAY_LANDING    = 90;		// particules d'un coup, à la réception du saut
	const float SPRAY_DRAG       = 2.5f;	// 1/s : l'air freine vite la poussière de neige
	const float SPRAY_SETTLE     = 1.2f;	// m/s² : elle retombe lentement

	//--- Décor : sapins et rochers, placés au hasard (graine fixe) --------------
	const unsigned DECOR_SEED = 2026;	// même graine -> toujours le même décor
	const float DECOR_X_MIN = -700.0f, DECOR_X_MAX = 750.0f;
	const float DECOR_Z_MIN = -700.0f, DECOR_Z_MAX = 700.0f;
	const int   TREE_COUNT = 2500;
	const float TREE_LINE = 180.0f;		// m : pas d'arbres au-dessus de cette altitude
	const float TREE_MIN_HEIGHT = 6.0f, TREE_MAX_HEIGHT = 13.0f;
	const int   ROCK_COUNT = 250;
	const float ROCK_MIN_SIZE = 0.8f, ROCK_MAX_SIZE = 4.0f;

	//--- Vue d'ensemble (provisoire, avant la caméra animée) -------------------
	const glm::vec3 OVERVIEW_EYE(650.0f, 600.0f, 550.0f);
	const glm::vec3 OVERVIEW_TARGET(0.0f, 150.0f, -250.0f);
}

#endif // SCENE_H
