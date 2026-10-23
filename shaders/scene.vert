//-----------------------------------------------------------------------------
// Vertex shader de la scène (repris du cours : lighting_dir_point_spot.vert),
// avec en plus l'instancing pour les sapins et les rochers.
//-----------------------------------------------------------------------------
#version 330 core

layout (location = 0) in vec3 pos;
layout (location = 1) in vec3 normal;
layout (location = 2) in vec2 texCoord;
layout (location = 3) in mat4 instanceModel;	// AJOUT : matrice de l'exemplaire (locations 3 à 6)

uniform mat4 model;			// model matrix
uniform mat4 view;			// view matrix
uniform mat4 projection;	// projection matrix
uniform bool instanced;		// AJOUT : vrai -> on prend instanceModel au lieu de model

out vec3 FragPos;
out vec3 Normal;
out vec2 TexCoord;

void main()
{
	mat4 M = instanced ? instanceModel : model;

	FragPos = vec3(M * vec4(pos, 1.0f));			// vertex position in world space
	Normal = mat3(transpose(inverse(M))) * normal;	// normal direction in world space

	TexCoord = texCoord;

	gl_Position = projection * view * M * vec4(pos, 1.0f);
}
