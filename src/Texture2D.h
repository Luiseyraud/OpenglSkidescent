//-----------------------------------------------------------------------------
// Simple 2D texture class
//-----------------------------------------------------------------------------
#ifndef TEXTURE2D_H
#define TEXTURE2D_H

#define GLEW_STATIC
#include "GL/glew.h"
#include <string>
using std::string;

class Texture2D
{
public:
	Texture2D();
	virtual ~Texture2D();

	bool loadTexture(const string& fileName, bool generateMipMaps = true);
	void bind(GLuint texUnit = 0);
	void unbind(GLuint texUnit = 0);

	// CORRECTION : interdit la copie (le cours le faisait avec des fonctions privées
	// vides, dont operator= qui ne renvoyait rien et causait un avertissement).
	Texture2D(const Texture2D& rhs) = delete;
	Texture2D& operator = (const Texture2D& rhs) = delete;

private:
	GLuint mTexture;
};
#endif //TEXTURE2D_H
