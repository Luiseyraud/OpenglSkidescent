//-----------------------------------------------------------------------------
// Bloom, réduction : l'image fait la moitié de la source. Chaque pixel est la
// moyenne de 4 lectures bilinéaires décalées d'un pixel de la source, soit
// une moyenne pondérée de 4 x 4 pixels : réduction et flou à la fois.
//-----------------------------------------------------------------------------
#version 330 core

in vec2 uv;

uniform sampler2D source;
uniform vec2 texel;		// taille d'un pixel de la source (1 / largeur, 1 / hauteur)

out vec4 frag_color;

void main()
{
	vec3 c = texture(source, uv + texel * vec2(-1.0, -1.0)).rgb
	       + texture(source, uv + texel * vec2( 1.0, -1.0)).rgb
	       + texture(source, uv + texel * vec2(-1.0,  1.0)).rgb
	       + texture(source, uv + texel * vec2( 1.0,  1.0)).rgb;
	frag_color = vec4(c * 0.25, 1.0);
}
