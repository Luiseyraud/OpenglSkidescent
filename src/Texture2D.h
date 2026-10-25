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
	bool createSolidColor(unsigned char r, unsigned char g, unsigned char b);	// AJOUT
	bool createFromPixels(int width, int height, const unsigned char* rgba, bool generateMipMaps);	// AJOUT
	void bind(GLuint texUnit = 0);
	void unbind(GLuint texUnit = 0);

	// CORRECTION : copie interdite avec "= delete", et non avec des fonctions privées
	// vides (un operator= sans return provoque un avertissement).
	Texture2D(const Texture2D& rhs) = delete;
	Texture2D& operator = (const Texture2D& rhs) = delete;

private:
	GLuint mTexture;
};
#endif //TEXTURE2D_H
