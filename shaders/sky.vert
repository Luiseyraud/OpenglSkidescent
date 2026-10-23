//-----------------------------------------------------------------------------
// Vertex shader du ciel : un cube centré sur la caméra
//-----------------------------------------------------------------------------
#version 330 core

layout (location = 0) in vec3 pos;

uniform mat4 view;			// sans la translation : le cube suit la caméra
uniform mat4 projection;

out vec3 Direction;			// direction regardée, du centre vers ce point du cube

void main()
{
	Direction = pos;
	vec4 p = projection * view * vec4(pos, 1.0);

	// z = w : après la division par w, la profondeur vaut 1 (le maximum).
	// Le ciel est donc toujours derrière le reste de la scène.
	gl_Position = p.xyww;
}
