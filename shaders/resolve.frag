//-----------------------------------------------------------------------------
// Résolution du MSAA en HDR : la couleur d'un pixel est la moyenne de ses 4
// échantillons, chacun pondéré par 1 / (1 + sa luminosité). Une moyenne simple
// laisserait un fond très lumineux (le soleil, qui vaut plusieurs fois le blanc)
// effacer un objet fin et sombre devant lui ; avec ces poids, il reste visible.
//-----------------------------------------------------------------------------
#version 330 core

uniform sampler2DMS samples;	// l'image HDR, 4 échantillons par pixel

out vec4 frag_color;

void main()
{
	ivec2 pixel = ivec2(gl_FragCoord.xy);
	vec3 sum = vec3(0.0);
	float weights = 0.0;
	for (int i = 0; i < 4; i++)
	{
		vec3 c = texelFetch(samples, pixel, i).rgb;
		float w = 1.0 / (1.0 + max(c.r, max(c.g, c.b)));
		sum += c * w;
		weights += w;
	}
	frag_color = vec4(sum / weights, 1.0);
}
