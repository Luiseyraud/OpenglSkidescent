//-----------------------------------------------------------------------------
// Simple 2D texture class
//-----------------------------------------------------------------------------
#include "Texture2D.h"
#include <iostream>
#include <cassert>
#include <algorithm>
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image/stb_image.h"

//-----------------------------------------------------------------------------
// Constructor
//-----------------------------------------------------------------------------
Texture2D::Texture2D()
	: mTexture(0)
{
}

//-----------------------------------------------------------------------------
// Destructor
//-----------------------------------------------------------------------------
Texture2D::~Texture2D()
{
	glDeleteTextures(1, &mTexture);
}

//-----------------------------------------------------------------------------
// Load a texture with a given filename using stb image loader
// http://nothings.org/stb_image.h
// Creates mip maps if generateMipMaps is true.
//-----------------------------------------------------------------------------
bool Texture2D::loadTexture(const string& fileName, bool generateMipMaps)
{
	int width, height, components;

	// Use stbi image library to load our image
	unsigned char* imageData = stbi_load(fileName.c_str(), &width, &height, &components, STBI_rgb_alpha);

	if (imageData == NULL)
	{
		std::cerr << "Error loading texture '" << fileName << "'" << std::endl;
		return false;
	}

	// Invert image
	int widthInBytes = width * 4;
	unsigned char *top = NULL;
	unsigned char *bottom = NULL;
	unsigned char temp = 0;
	int halfHeight = height / 2;
	for (int row = 0; row < halfHeight; row++)
	{
		top = imageData + row * widthInBytes;
		bottom = imageData + (height - row - 1) * widthInBytes;
		for (int col = 0; col < widthInBytes; col++)
		{ 
			temp = *top;
			*top = *bottom;
			*bottom = temp;
			top++;
			bottom++;
		}
	}

	// AJOUT : l'envoi à OpenGL est maintenant dans createFromPixels (aussi utilisé
	// pour les textures calculées par le code)
	bool ok = createFromPixels(width, height, imageData, generateMipMaps);
	stbi_image_free(imageData);
	return ok;
}

//-----------------------------------------------------------------------------
// AJOUT : crée la texture à partir d'une image déjà en mémoire (4 octets RGBA
// par pixel, en sRGB, la première ligne étant le BAS de l'image : v = 0).
// Code repris de loadTexture.
//-----------------------------------------------------------------------------
bool Texture2D::createFromPixels(int width, int height, const unsigned char* rgba, bool generateMipMaps)
{
	glGenTextures(1, &mTexture);
	glBindTexture(GL_TEXTURE_2D, mTexture); // all upcoming GL_TEXTURE_2D operations will affect our texture object (mTexture)

	// Set the texture wrapping/filtering options (on the currently bound texture object)
	// GL_CLAMP_TO_EDGE
	// GL_REPEAT
	// GL_MIRRORED_REPEAT
	// GL_CLAMP_TO_BORDER
	// GL_LINEAR
	// GL_NEAREST
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	// CORRECTION : avec GL_LINEAR, les mipmaps n'étaient jamais utilisées (scintillement au loin)
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, generateMipMaps ? GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	// AJOUT : filtrage anisotrope, pour qu'une texture vue de biais (la neige) reste nette
	if (GLEW_EXT_texture_filter_anisotropic)
	{
		GLfloat maxAnisotropy = 1.0f;
		glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &maxAnisotropy);
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, std::min(maxAnisotropy, 8.0f));
	}

	// CORRECTION : les images sont en sRGB. Avec GL_SRGB8_ALPHA8, OpenGL les convertit
	// en couleurs linéaires à la lecture, ce que demande la correction gamma.
	glTexImage2D(GL_TEXTURE_2D, 0, GL_SRGB8_ALPHA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba);

	if (generateMipMaps)
		glGenerateMipmap(GL_TEXTURE_2D);

	glBindTexture(GL_TEXTURE_2D, 0); // unbind texture when done so we don't accidentally mess up our mTexture

	return true;
}

//-----------------------------------------------------------------------------
// AJOUT : crée une texture d'un seul pixel, de couleur unie.
// Utile pour dessiner un objet d'une seule couleur avec le shader du cours,
// qui lit toujours sa couleur dans une texture.
//-----------------------------------------------------------------------------
bool Texture2D::createSolidColor(unsigned char r, unsigned char g, unsigned char b)
{
	unsigned char pixel[4] = { r, g, b, 255 };	// couleur en sRGB, comme une image
	return createFromPixels(1, 1, pixel, false);
}

//-----------------------------------------------------------------------------
// Bind the texture unit passed in as the active texture in the shader
//-----------------------------------------------------------------------------
void Texture2D::bind(GLuint texUnit)
{
	assert(texUnit >= 0 && texUnit < 32);

	glActiveTexture(GL_TEXTURE0 + texUnit);
	glBindTexture(GL_TEXTURE_2D, mTexture);
}

//-----------------------------------------------------------------------------
// Unbind the texture unit passed in as the active texture in the shader
//-----------------------------------------------------------------------------
void Texture2D::unbind(GLuint texUnit)
{
	glActiveTexture(GL_TEXTURE0 + texUnit);
	glBindTexture(GL_TEXTURE_2D, 0);
}
