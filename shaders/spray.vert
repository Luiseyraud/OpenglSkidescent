//-----------------------------------------------------------------------------
// Gerbes de neige : une particule = un carré face à la caméra (billboard).
// Tout est calculé ici à partir du temps : âge, position, taille.
//-----------------------------------------------------------------------------
#version 330 core

layout (location = 0) in vec2 corner;		// coin du carré, de (-1, -1) à (1, 1)
layout (location = 1) in vec4 start;		// xyz : position de départ, w : instant de naissance
layout (location = 2) in vec4 velocity;		// xyz : vitesse de départ,  w : durée de vie
layout (location = 3) in vec4 look;			// x : taille au départ, y : taille à la fin, z : angle, w : opacité

uniform mat4 view;
uniform mat4 projection;
uniform float time;		// instant de l'image
uniform float drag;		// freinage par l'air (1/s)
uniform float settle;	// chute de la poussière (m/s²)

out vec2 uv;			// position dans l'image de la planche (0 à 1)
out float progress;		// âge / durée de vie : 0 à la naissance, 1 à la fin
out float opacity;
out vec3 worldPos;

void main()
{
	float age = time - start.w;
	if (age < 0.0 || age > velocity.w)
	{
		// Pas encore née ou déjà dissipée : les 4 coins au même point hors de l'écran,
		// le carré n'a plus de surface et n'est pas dessiné
		gl_Position = vec4(2.0, 2.0, 2.0, 1.0);
		return;
	}
	progress = age / velocity.w;

	// Position : la vitesse diminue comme exp(-drag t) (frottement de l'air),
	// ce qui donne une distance parcourue v (1 - exp(-drag t)) / drag, et la
	// poussière retombe lentement
	vec3 p = start.xyz + velocity.xyz * (1.0 - exp(-drag * age)) / drag;
	p.y -= 0.5 * settle * age * age;

	// Taille : grandit vite au début, puis lentement. Le centre est remonté pour
	// que le nuage repose sur la neige au lieu d'être coupé par elle.
	float size = mix(look.x, look.y, sqrt(progress));
	p.y += 0.6 * size;
	worldPos = p;

	// Carré tourné d'un angle propre à la particule (et qui tourne lentement) :
	// toutes les particules n'ont pas la même image à l'écran
	float angle = look.z + 0.6 * age;
	vec2 c = mat2(cos(angle), sin(angle), -sin(angle), cos(angle)) * corner;

	// Billboard : dans le repère de la caméra, x et y sont les axes de l'écran
	vec4 center = view * vec4(p, 1.0);
	center.xy += c * size;
	gl_Position = projection * center;

	uv = corner * 0.5 + 0.5;
	// Apparition rapide, puis disparition progressive
	opacity = look.w * smoothstep(0.0, 0.06, progress) * (1.0 - progress);
}
