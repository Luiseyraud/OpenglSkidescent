//-----------------------------------------------------------------------------
// Neige qui tombe : un flocon = un petit carré face à la caméra (billboard).
// Sa position et la lumière qu'il reçoit sont calculées ici, une fois par
// sommet (les 4 coins d'un flocon reçoivent la même lumière).
// Les lumières ont les mêmes noms que dans scene.frag (même code C++ pour les régler).
//-----------------------------------------------------------------------------
#version 330 core

layout (location = 0) in vec2 corner;	// coin du carré, de (-1, -1) à (1, 1)
layout (location = 1) in vec4 start;	// xyz : position dans la boîte (0 à 1), w : phase
layout (location = 2) in vec4 motion;	// x : rayon (m), y : chute (m/s), z : amplitude et w : vitesse du balancement

struct DirectionalLight { vec3 direction; vec3 diffuse; vec3 specular; };
struct PointLight { vec3 position; vec3 diffuse; vec3 specular; float constant; float linear; float exponent; };
struct SpotLight
{
	vec3 position; vec3 direction; float cosInnerCone; float cosOuterCone;
	vec3 diffuse; vec3 specular; int on; float constant; float linear; float exponent;
};

#define MAX_POINT_LIGHTS 10		// comme dans scene.frag
#define MAX_FLOODLIGHTS 72

uniform mat4 view;
uniform mat4 projection;
uniform vec3 viewPos;			// position de la caméra
uniform float time;				// temps du skieur : la neige ralentit pendant le saut
uniform vec3 boxSize;			// taille de la boîte de neige autour de la caméra (m)
uniform vec3 wind;				// vent (m/s)
uniform float pixelScale;		// taille en pixels d'un objet de 1 m vu à 1 m
uniform float scatter;			// force de la diffusion vers l'avant

uniform DirectionalLight sunLight;
uniform PointLight pointLights[MAX_POINT_LIGHTS];
uniform SpotLight spotLight;	// la frontale
uniform SpotLight floodLights[MAX_FLOODLIGHTS];
uniform int floodLightCount;
uniform vec3 ambientSky;

out vec2 uv;
out vec3 light;		// lumière reçue par le flocon
out float alpha;

// Diffusion vers l'avant : un flocon renvoie surtout la lumière qui arrive de
// derrière lui (vu de la caméra). toLight : direction du flocon vers la lumière.
float phase(vec3 toLight, vec3 viewDir)
{
	return 0.6 + 2.0 * pow(max(dot(toLight, viewDir), 0.0), 6.0);
}

// Lumière d'un spot reçue en p (même cône et même atténuation que scene.frag)
vec3 spot(SpotLight l, vec3 p, vec3 viewDir)
{
	vec3 toLight = l.position - p;
	float d = length(toLight);
	toLight /= d;
	float cone = smoothstep(l.cosOuterCone, l.cosInnerCone, dot(-toLight, normalize(l.direction)));
	return l.diffuse * cone * phase(toLight, viewDir) / (l.constant + l.linear * d + l.exponent * d * d);
}

void main()
{
	// 1. Position : départ dans la boîte, puis chute, vent et balancement
	vec3 p = start.xyz * boxSize + wind * time;
	p.y -= motion.y * time;
	p.x += motion.z * sin(time * motion.w + start.w);
	p.z += motion.z * cos(time * motion.w * 0.7 + start.w);

	// La boîte se répète à l'infini : on ramène le flocon dans la boîte centrée sur la caméra
	p = viewPos + mod(p - viewPos + 0.5 * boxSize, boxSize) - 0.5 * boxSize;

	// 2. Taille : au moins 1,5 pixel à l'écran, sinon les flocons lointains
	//    scintillent. Un flocon ainsi agrandi est rendu plus transparent (même
	//    quantité de lumière à l'écran).
	vec4 c = view * vec4(p, 1.0);
	float depth = max(-c.z, 0.01);
	float radius = motion.x;
	float pixels = radius * pixelScale / depth;
	alpha = 0.85;
	if (pixels < 1.5)
	{
		alpha *= (pixels / 1.5) * (pixels / 1.5);
		radius = 1.5 * depth / pixelScale;
	}

	// Fondu tout près de la caméra et au bord de la boîte (on ne voit pas les flocons apparaître)
	float dist = length(p - viewPos);
	alpha *= smoothstep(0.5, 2.0, dist) * (1.0 - smoothstep(0.35 * boxSize.x, 0.5 * boxSize.x, dist));

	// Billboard : dans le repère de la caméra, x et y sont les axes de l'écran
	c.xy += corner * radius;
	gl_Position = projection * c;
	uv = corner;

	// 3. Lumière reçue : ciel, soleil, frontale, projecteurs, lampes
	vec3 viewDir = normalize(p - viewPos);
	vec3 sunDir = normalize(-sunLight.direction);
	light = ambientSky + sunLight.diffuse * (0.3 + scatter * pow(max(dot(sunDir, viewDir), 0.0), 8.0));
	light += spot(spotLight, p, viewDir);
	for (int i = 0; i < floodLightCount; i++)
		light += spot(floodLights[i], p, viewDir);
	for (int i = 0; i < MAX_POINT_LIGHTS; i++)
	{
		vec3 toLight = pointLights[i].position - p;
		float d = length(toLight);
		light += pointLights[i].diffuse * phase(toLight / d, viewDir)
		       / (pointLights[i].constant + pointLights[i].linear * d + pointLights[i].exponent * d * d);
	}
}
