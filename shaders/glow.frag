//-----------------------------------------------------------------------------
// Halo d'un projecteur : lueur qui diminue du centre vers le bord du carré.
// La couleur est AJOUTÉE à l'image (mélange additif réglé dans Glow::draw).
//-----------------------------------------------------------------------------
#version 330 core

in vec2 uv;

uniform vec3 color;		// couleur du projecteur x force du halo

out vec4 frag_color;

void main()
{
	float r2 = dot(uv, uv);		// distance au centre, au carré (1 = bord du carré)

	// Un cœur éblouissant et un halo large et faible (deux courbes en cloche)
	float glow = 0.6 * exp(-r2 * 14.0) + 0.25 * exp(-r2 * 3.5);

	frag_color = vec4(color * glow, 1.0);
}
