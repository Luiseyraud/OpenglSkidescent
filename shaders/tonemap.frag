//-----------------------------------------------------------------------------
// Image finale : scène HDR + bloom, tone mapping, correction gamma
//-----------------------------------------------------------------------------
#version 330 core

in vec2 uv;

uniform sampler2D scene;		// image HDR (couleurs linéaires, peuvent dépasser 1)
uniform sampler2D bloom;		// somme des niveaux flous
uniform float bloomStrength;	// part du bloom dans l'image finale

out vec4 frag_color;

// Tone mapping : identité jusqu'à 0,8 (l'image garde son aspect), puis
// compression douce vers 1 (pente 1 au raccord, asymptote 1) au lieu d'une coupure
vec3 toneMap(vec3 c)
{
	vec3 over = max(c - 0.8, 0.0);
	return min(c, 0.8) + 0.2 * (1.0 - exp(-over / 0.2));
}

void main()
{
	vec3 color = mix(texture(scene, uv).rgb, texture(bloom, uv).rgb, bloomStrength);
	color = toneMap(color);
	color = pow(color, vec3(1.0 / 2.2));	// correction gamma

	// Tramage : un bruit de +-0,5/255 casse les marches de couleur des dégradés
	// sombres (le ciel du soir), visibles en 8 bits
	float noise = fract(sin(dot(gl_FragCoord.xy, vec2(12.9898, 78.233))) * 43758.5453);
	color += (noise - 0.5) / 255.0;

	frag_color = vec4(color, 1.0);
}
