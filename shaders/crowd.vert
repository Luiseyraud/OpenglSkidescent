//-----------------------------------------------------------------------------
// Public des tribunes : un personnage de 6 boîtes, dessiné à chaque place
// (instancing), animé ici à partir du temps. Il fournit au fragment shader de
// la scène (scene.frag) les mêmes sorties que scene.vert : l'éclairage est
// celui du reste de la scène.
//-----------------------------------------------------------------------------
#version 330 core

layout (location = 0) in vec3 pos;
layout (location = 1) in vec3 normal;
layout (location = 2) in vec2 texCoord;		// u : partie du corps (0 jambes, 1 buste, 2 tête, 3 bonnet, 4 bras gauche, 5 bras droit)
layout (location = 3) in vec4 placement;	// xyz : pieds (debout), w : orientation (rad)
layout (location = 4) in vec4 colors;		// n° de couleur dans la palette : veste, pantalon, peau, bonnet
layout (location = 5) in vec4 traits;		// x : taille, y : phase, z : énergie (0 à 1), w : retard pour se lever (s)
layout (location = 6) in vec4 extra;		// x : position le long de la tribune (m)

uniform mat4 view;
uniform mat4 projection;
uniform float time;
uniform float cheerTime;	// instant où le public commence à se lever (le skieur approche)
uniform vec3 skier;			// position du skieur : le public se tourne vers lui
uniform float olaSpeed;		// m/s
uniform float olaPeriod;	// s entre deux olas

out vec3 FragPos;
out vec3 Normal;
out vec2 TexCoord;

const float PI = 3.14159265;

// Rotation d'un angle a dans un plan (sens trigonométrique)
mat2 rotation(float a)
{
	return mat2(cos(a), sin(a), -sin(a), cos(a));
}

void main()
{
	int part = int(texCoord.x + 0.5);
	float phase = traits.y;
	float energy = traits.z;

	// 1. Assis ou debout ------------------------------------------------------
	// Le public se lève quand le skieur approche, chacun avec un petit retard.
	// Avant, une ola parcourt les tribunes : on se lève un instant quand elle passe.
	float cheer = smoothstep(cheerTime + traits.w, cheerTime + traits.w + 0.8, time);
	float front = mod(time * olaSpeed, olaSpeed * olaPeriod) - 15.0;	// position de la vague (m)
	float ola = exp(-pow((extra.x - front) / 1.5, 2.0)) * (1.0 - cheer);
	float up = max(cheer, ola);

	// 2. Les bras tournent autour de l'épaule : le long du corps quand il est
	//    assis, levés pendant la ola, agités quand il acclame le skieur
	float arms = mix(0.1, 2.6, ola);
	arms = mix(arms, mix(0.6, 2.7, energy) + 0.35 * sin(time * 7.0 + phase), cheer);
	vec3 p = pos;
	vec3 n = normal;
	if (part == 4 || part == 5)
	{
		float side = (part == 4) ? 1.0 : -1.0;		// bras gauche (x > 0) ou droit (x < 0)
		vec2 shoulder = vec2(0.26 * side, 1.36);
		p.xy = shoulder + rotation(arms * side) * (p.xy - shoulder);
		n.xy = rotation(arms * side) * n.xy;
	}

	// 3. Le corps : à sa taille, assis (plus bas et en arrière, sur le siège) ou
	//    debout, et les plus énergiques sautillent en criant
	p *= traits.x;
	p.y += mix(-0.38, 0.0, up);
	p.z += mix(-0.35, 0.0, up);
	p.y += cheer * energy * 0.12 * max(sin(time * 10.0 + phase), 0.0);

	// 4. Orientation : face à la piste, qui regarde un peu autour de lui ; debout,
	//    il se tourne vers le skieur (au plus d'un radian)
	vec2 toSkier = skier.xz - placement.xz;
	float look = atan(toSkier.x, toSkier.y);	// angle qui tourne +Z vers le skieur
	float turn = clamp(mod(look - placement.w + PI, 2.0 * PI) - PI, -1.0, 1.0);
	float yaw = placement.w + turn * cheer * 0.9 + 0.12 * sin(time * 0.6 + phase) * (1.0 - cheer);

	// Rotation autour de Y (comme glm::rotate) puis translation aux pieds
	float c = cos(yaw), s = sin(yaw);
	vec3 world = placement.xyz + vec3(c * p.x + s * p.z, p.y, -s * p.x + c * p.z);
	Normal = vec3(c * n.x + s * n.z, n.y, -s * n.x + c * n.z);
	FragPos = world;

	// Couleur : ligne de la palette selon la partie du corps, colonne propre au spectateur
	int row = (part == 0) ? 1 : (part == 2) ? 2 : (part == 3) ? 3 : 0;		// pantalon, peau, bonnet, sinon veste
	TexCoord = vec2((colors[row] + 0.5) / 8.0, (float(row) + 0.5) / 4.0);

	gl_Position = projection * view * vec4(world, 1.0);
}
