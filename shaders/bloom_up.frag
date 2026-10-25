//-----------------------------------------------------------------------------
// Bloom, remontée : agrandit le niveau plus petit en le floutant (filtre "tente"
// de 3 x 3 lectures : poids 4 au centre, 2 sur les côtés, 1 dans les coins).
// Le résultat est AJOUTÉ au niveau au-dessus (mélange additif, voir PostProcess).
//-----------------------------------------------------------------------------
#version 330 core

in vec2 uv;

uniform sampler2D source;
uniform vec2 texel;		// taille d'un pixel de la source

out vec4 frag_color;

void main()
{
	vec3 c = 4.0 * texture(source, uv).rgb;
	c += 2.0 * (texture(source, uv + texel * vec2(-1.0, 0.0)).rgb + texture(source, uv + texel * vec2(1.0, 0.0)).rgb
	          + texture(source, uv + texel * vec2(0.0, -1.0)).rgb + texture(source, uv + texel * vec2(0.0, 1.0)).rgb);
	c += texture(source, uv + texel * vec2(-1.0, -1.0)).rgb + texture(source, uv + texel * vec2(1.0, -1.0)).rgb
	   + texture(source, uv + texel * vec2(-1.0,  1.0)).rgb + texture(source, uv + texel * vec2(1.0,  1.0)).rgb;
	frag_color = vec4(c / 16.0, 1.0);
}
