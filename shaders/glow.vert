//-----------------------------------------------------------------------------
// Halo d'un projecteur : un carré toujours face à la caméra (billboard)
//-----------------------------------------------------------------------------
#version 330 core

layout (location = 0) in vec2 corner;	// coin du carré, de (-1, -1) à (1, 1)

uniform mat4 view;
uniform mat4 projection;
uniform vec3 center;	// centre du halo, dans le monde
uniform float size;		// demi-largeur du carré, en mètres

out vec2 uv;

void main()
{
	// On passe le centre dans le repère de la caméra, PUIS on décale les coins
	// en x et y : dans ce repère, x et y sont les axes de l'écran, donc le carré
	// reste face à la caméra quelle que soit sa direction.
	vec4 c = view * vec4(center, 1.0);
	c.xy += corner * size;
	gl_Position = projection * c;
	uv = corner;
}
