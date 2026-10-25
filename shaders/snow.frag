//-----------------------------------------------------------------------------
// Neige qui tombe : un disque aux bords doux, éclairé par la lumière reçue
// (calculée dans snow.vert). Couleur HDR linéaire, en alpha prémultiplié.
//-----------------------------------------------------------------------------
#version 330 core

in vec2 uv;
in vec3 light;
in float alpha;

out vec4 frag_color;

void main()
{
	float disc = 1.0 - smoothstep(0.3, 1.0, length(uv));	// 1 au centre, 0 au bord du carré
	float a = disc * alpha;
	if (a < 0.003)
		discard;
	frag_color = vec4(0.9 * light * a, a);		// neige blanche à 90 %
}
