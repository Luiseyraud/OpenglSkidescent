//-----------------------------------------------------------------------------
// Gerbes de neige : image animée de la planche (4 x 4 images), éclairage de
// la poussière de neige, sortie en alpha prémultiplié (voir SnowSpray::draw)
//-----------------------------------------------------------------------------
#version 330 core

in vec2 uv;
in float progress;
in float opacity;
in vec3 worldPos;

uniform sampler2D sheet;	// planche de 16 images : de la bouffée dense à la poussière dissipée
uniform vec3 eye;			// position de la caméra
uniform vec3 sunDir;		// L : direction VERS le soleil
uniform vec3 sunColor;
uniform vec3 ambient;		// lumière du ciel
uniform vec3 localLight;	// lumière des projecteurs à l'endroit du skieur
uniform float scatter;		// force de la diffusion vers l'avant

out vec4 frag_color;

// Densité lue dans l'image n° frame de la planche
float density(float frame)
{
	vec2 cell = vec2(mod(frame, 4.0), floor(frame / 4.0));
	return texture(sheet, (cell + uv) / 4.0).a;
}

void main()
{
	// Animation : l'âge choisit l'image (0 à 15). On mélange les deux images
	// voisines, pour que le nuage se transforme sans à-coups.
	float f = progress * 15.0;
	float f0 = floor(f);
	float d = mix(density(f0), density(min(f0 + 1.0, 15.0)), f - f0);
	float alpha = d * opacity;
	if (alpha < 0.003)
		discard;

	// Éclairage : la neige (blanche à 90 %) reçoit le ciel, le soleil et les
	// projecteurs. Une poussière fine diffuse surtout la lumière VERS L'AVANT :
	// quand la caméra regarde vers le soleil à travers la gerbe, elle s'illumine
	// (contre-jour).
	vec3 viewDir = normalize(worldPos - eye);
	float forward = pow(max(dot(viewDir, sunDir), 0.0), 6.0);
	vec3 color = 0.9 * (ambient + 0.6 * sunColor + 0.7 * localLight) + scatter * forward * sunColor;

	// Couleur linéaire HDR (gamma : voir tonemap.frag), en alpha prémultiplié
	frag_color = vec4(color * alpha, alpha);
}
