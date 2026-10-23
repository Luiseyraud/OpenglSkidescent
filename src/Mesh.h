//-----------------------------------------------------------------------------
// Basic Mesh class
//-----------------------------------------------------------------------------
#ifndef MESH_H
#define MESH_H

#include <vector>
#include <string>

#define GLEW_STATIC
#include "GL/glew.h"	// Important - this header must come before glfw3 header
#include "glm/glm.hpp"


struct Vertex
{
	glm::vec3 position;
	glm::vec3 normal;
	glm::vec2 texCoords;
};

class Mesh
{
public:

	 Mesh();
	~Mesh();

	bool loadOBJ(const std::string& filename);
	bool create(const std::vector<Vertex>& vertices);	// AJOUT : mesh calculée par le code (triangles)
	void draw();

	// AJOUT : instancing (le même modèle à plusieurs endroits, en un seul appel)
	void setInstances(const std::vector<glm::mat4>& models);
	void drawInstanced();

private:

	void initBuffers();

	bool mLoaded;
	std::vector<Vertex> mVertices;
	GLuint mVBO, mVAO;
	GLuint mInstanceVBO;		// AJOUT : matrices des exemplaires
	GLsizei mInstanceCount;		// AJOUT
};
#endif //MESH_H
