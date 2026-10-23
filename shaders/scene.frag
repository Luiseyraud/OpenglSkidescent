//-----------------------------------------------------------------------------
// Fragment shader de la scène
//
// Dérivé de lighting_dir_point_spot.frag du cours (Blinn-Phong, 1 lumière
// directionnelle, 3 ponctuelles, 1 spot, mêmes noms d'uniforms). Changements :
//   1. la lumière ambiante vient du ciel (et plus de spotLight.ambient) ;
//   2. la couleur de la texture est lue une seule fois (albedo) ;
//   3. brouillard : au loin, le terrain prend la couleur du ciel derrière lui ;
//   4. correction gamma à la sortie (les calculs se font en couleurs linéaires) ;
//   5. surfaces éclairées des deux côtés (filets), et alpha de la texture en
//      sortie (trous du filet, voir World::render) ;
//   6. en plus de la frontale (spotLight), un tableau de spots : les projecteurs
//      des pylônes d'éclairage (floodLights).
//-----------------------------------------------------------------------------
#version 330 core

struct Material
{
    sampler2D diffuseMap;
    vec3 specular;
    float shininess;
};

struct DirectionalLight
{
	vec3 direction;
	vec3 diffuse;
	vec3 specular;
};

struct PointLight
{
	vec3 position;
	vec3 diffuse;
	vec3 specular;

	float constant;
	float linear;
	float exponent;
};

struct SpotLight
{
	vec3 position;
	vec3 direction;
	float cosInnerCone;
	float cosOuterCone;
	vec3 diffuse;
	vec3 specular;
	int on;

	float constant;
	float linear;
	float exponent;
};

// Couleurs du ciel (les mêmes que dans sky.frag)
struct Sky
{
	vec3 zenith;		// en haut
	vec3 horizon;		// à l'horizon, loin du soleil
	vec3 horizonSun;	// à l'horizon, du côté du soleil
	vec3 sunDir;		// L : direction VERS le soleil
};

in vec2 TexCoord;
in vec3 FragPos;
in vec3 Normal;

#define MAX_POINT_LIGHTS 10		// 3 dans le cours : 3 lampadaires, la lampe du chalet, 6 pour les guirlandes des tribunes

uniform DirectionalLight sunLight;
uniform PointLight pointLights[MAX_POINT_LIGHTS];
uniform SpotLight spotLight;		// la frontale du skieur

#define MAX_FLOODLIGHTS 16				// projecteurs des pylônes (8 paires)
uniform SpotLight floodLights[MAX_FLOODLIGHTS];
uniform int floodLightCount;			// nombre de projecteurs utilisés
uniform Material material;
uniform vec3 viewPos;

uniform Sky sky;
uniform vec3 ambientSky;		// lumière ambiante venant du ciel (bleutée)
uniform vec3 ambientGround;		// lumière ambiante renvoyée par le sol
uniform float fogDensity;
uniform bool twoSided;			// surface sans épaisseur (filet) : éclairée des deux côtés

out vec4 frag_color;

vec3 calcDirectionalLightColor(DirectionalLight light, vec3 normal, vec3 viewDir, vec3 albedo);
vec3 calcPointLightColor(PointLight light, vec3 normal, vec3 fragPos, vec3 viewDir, vec3 albedo);
vec3 calcSpotLightColor(SpotLight light, vec3 normal, vec3 fragPos, vec3 viewDir, vec3 albedo);
vec3 skyColor(vec3 dir);

//-----------------------------------------------------------------------------------------------
// Main Shader Entry
//-----------------------------------------------------------------------------------------------
void main()
{
	vec3 normal = normalize(Normal);
	vec3 viewDir = normalize(viewPos - FragPos);

	// Surface vue de dos : on retourne sa normale vers la caméra
	if (twoSided && dot(normal, viewDir) < 0.0)
		normal = -normal;

	vec4 texel = texture(material.diffuseMap, TexCoord);	// texture sRGB : OpenGL la rend déjà linéaire
	vec3 albedo = texel.rgb;

	// Ambiante ---------------------------------------------------------------------------------
	// Une surface tournée vers le haut reçoit la lumière du ciel, une surface tournée
	// vers le bas celle du sol. On mélange les deux selon la normale.
	vec3 ambient = mix(ambientGround, ambientSky, 0.5 * normal.y + 0.5) * albedo;
	vec3 outColor = ambient;

	outColor += calcDirectionalLightColor(sunLight, normal, viewDir, albedo);

	for(int i = 0; i < MAX_POINT_LIGHTS; i++)
		outColor += calcPointLightColor(pointLights[i], normal, FragPos, viewDir, albedo);

	// If the light isn't on then just return 0 for diffuse and specular colors
	if (spotLight.on == 1)
		outColor += calcSpotLightColor(spotLight, normal, FragPos, viewDir, albedo);

	// Projecteurs des pylônes : même calcul que la frontale
	for (int i = 0; i < floodLightCount; i++)
		outColor += calcSpotLightColor(floodLights[i], normal, FragPos, viewDir, albedo);

	// Brouillard -------------------------------------------------------------------------------
	// Exponentiel au carré : presque rien de près, très fort au loin. Sa couleur est
	// celle du ciel dans la même direction, donc le bord du terrain se fond dans le ciel.
	float distance = length(FragPos - viewPos);
	float visibility = exp(-pow(distance * fogDensity, 2.0));
	outColor = mix(skyColor(-viewDir), outColor, visibility);

	// Correction gamma -------------------------------------------------------------------------
	frag_color = vec4(pow(outColor, vec3(1.0 / 2.2)), texel.a);	// alpha < 1 : trous du filet
}

//-----------------------------------------------------------------------------------------------
// Couleur du ciel dans la direction dir (même fonction que dans sky.frag, sans le disque du soleil)
//-----------------------------------------------------------------------------------------------
vec3 skyColor(vec3 dir)
{
	float sunAmount = max(dot(dir, sky.sunDir), 0.0);
	vec3 horizon = mix(sky.horizon, sky.horizonSun, pow(sunAmount, 4.0));	// plus chaud vers le soleil
	return mix(horizon, sky.zenith, pow(max(dir.y, 0.0), 0.5));				// dégradé horizon -> zénith
}

//-----------------------------------------------------------------------------------------------
// Calculate the direction light effect and return the resulting
// diffuse and specular color summation
//-----------------------------------------------------------------------------------------------
vec3 calcDirectionalLightColor(DirectionalLight light, vec3 normal, vec3 viewDir, vec3 albedo)
{
	vec3 lightDir = normalize(-light.direction);  // negate => Must be a direction from fragment towards the light

	// Diffuse ------------------------------------------------------------------------- --------
	float NdotL = max(dot(normal, lightDir), 0.0);
	vec3 diffuse = light.diffuse * NdotL * albedo;

	// Specular - Blinn-Phong ------------------------------------------------------------------
	vec3 halfDir = normalize(lightDir + viewDir);
	float NDotH = max(dot(normal, halfDir), 0.0f);
	vec3 specular = light.specular * material.specular * pow(NDotH, material.shininess);

	return (diffuse + specular);
}

//-----------------------------------------------------------------------------------------------
// Calculate the point light effect and return the resulting diffuse and specular color summation
//-----------------------------------------------------------------------------------------------
vec3 calcPointLightColor(PointLight light, vec3 normal, vec3 fragPos, vec3 viewDir, vec3 albedo)
{
	vec3 lightDir = normalize(light.position - fragPos);

	// Diffuse ----------------------------------------------------------------------------------
	float NdotL = max(dot(normal, lightDir), 0.0);
	vec3 diffuse = light.diffuse * NdotL * albedo;

	// Specular - Blinn-Phong ------------------------------------------------------------------
	vec3 halfDir = normalize(lightDir + viewDir);
	float NDotH = max(dot(normal, halfDir), 0.0f);
	vec3 specular = light.specular * material.specular * pow(NDotH, material.shininess);

	// Attenuation using Kc, Kl, Kq -------------------------------------------------------------
	float d = length(light.position - fragPos);
	float attenuation = 1.0f / (light.constant + light.linear * d + light.exponent * (d * d));

	diffuse *= attenuation;
	specular *= attenuation;

	return (diffuse + specular);
}

//------------------------------------------------------------------------------------------------
// Calculate the spotlight effect and return the resulting // diffuse and specular color summation
//------------------------------------------------------------------------------------------------
vec3 calcSpotLightColor(SpotLight light, vec3 normal, vec3 fragPos, vec3 viewDir, vec3 albedo)
{
	vec3 lightDir = normalize(light.position - fragPos);
	vec3 spotDir  = normalize(light.direction);

	float cosDir = dot(-lightDir, spotDir);  // angle between the lights direction vector and spotlights direction vector
	float spotIntensity = smoothstep(light.cosOuterCone, light.cosInnerCone, cosDir);

	// Diffuse ----------------------------------------------------------------------------------
	float NdotL = max(dot(normal, lightDir), 0.0);
	vec3 diffuse = light.diffuse * NdotL * albedo;

	// Specular - Blinn-Phong ------------------------------------------------------------------
	vec3 halfDir = normalize(lightDir + viewDir);
	float NDotH = max(dot(normal, halfDir), 0.0f);
	vec3 specular = light.specular * material.specular * pow(NDotH, material.shininess);

	// Attenuation using Kc, Kl, Kq -------------------------------------------------------------
	float d = length(light.position - fragPos);
	float attenuation = 1.0f / (light.constant + light.linear * d + light.exponent * (d * d));

	diffuse *= attenuation * spotIntensity;
	specular *= attenuation * spotIntensity;

	return (diffuse + specular);
}
