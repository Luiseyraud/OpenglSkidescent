//-----------------------------------------------------------------------------
// Triangle qui couvre tout l'écran, sans VBO : ses 3 sommets (0,0), (2,0) et
// (0,2) en coordonnées de texture sont calculés à partir de leur numéro.
// La partie visible (de 0 à 1) couvre exactement l'écran.
//-----------------------------------------------------------------------------
#version 330 core

out vec2 uv;

void main()
{
	uv = vec2((gl_VertexID << 1) & 2, gl_VertexID & 2);
	gl_Position = vec4(uv * 2.0 - 1.0, 0.0, 1.0);
}
