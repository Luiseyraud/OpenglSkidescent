//-----------------------------------------------------------------------------
// Fragment shader du ciel : dégradé de coucher de soleil, puis disque du soleil
//-----------------------------------------------------------------------------
#version 330 core

// Couleurs du ciel (les mêmes que dans scene.frag)
struct Sky
{
	vec3 zenith;		// en haut
	vec3 horizon;		// à l'horizon, loin du soleil
	vec3 horizonSun;	// à l'horizon, du côté du soleil
	vec3 sunDir;		// L : direction VERS le soleil
};

in vec3 Direction;

uniform Sky sky;
uniform vec3 sunColor;

out vec4 frag_color;

//-----------------------------------------------------------------------------
// Couleur du ciel dans la direction dir (même fonction que dans scene.frag)
//-----------------------------------------------------------------------------
vec3 skyColor(vec3 dir)
{
	float sunAmount = max(dot(dir, sky.sunDir), 0.0);
	vec3 horizon = mix(sky.horizon, sky.horizonSun, pow(sunAmount, 4.0));	// plus chaud vers le soleil
	return mix(horizon, sky.zenith, pow(max(dir.y, 0.0), 0.5));				// dégradé horizon -> zénith
}

void main()
{
	vec3 dir = normalize(Direction);
	vec3 color = skyColor(dir);

	// Disque du soleil et halo autour. d = cosinus de l'angle entre dir et le soleil.
	float d = dot(dir, sky.sunDir);
	color += sunColor * (smoothstep(0.9995, 0.9998, d) * 5.0	// disque
	                   + pow(max(d, 0.0), 200.0) * 2.0);			// halo

	// Couleur linéaire HDR : le disque du soleil vaut ~5, plus que le blanc, et le
	// bloom le fait rayonner. Plus fort, son éblouissement noierait la silhouette du
	// skieur à contre-jour. (Gamma : voir tonemap.frag)
	frag_color = vec4(color, 1.0);
}
