//-----------------------------------------------------------------------------
// Recorder.cpp : rendu hors écran et sauvegarde des images
//-----------------------------------------------------------------------------
#include "Recorder.h"
#include <iostream>
#include <vector>
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write/stb_image_write.h"

//-----------------------------------------------------------------------------
// Constructeur : crée le FBO de l'image finale
//-----------------------------------------------------------------------------
Recorder::Recorder(int width, int height)
	: mWidth(width), mHeight(height)
{
	glGenFramebuffers(1, &mFBO);
	glBindFramebuffer(GL_FRAMEBUFFER, mFBO);

	glGenRenderbuffers(1, &mColorBuffer);
	glBindRenderbuffer(GL_RENDERBUFFER, mColorBuffer);
	glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA8, mWidth, mHeight);
	glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, mColorBuffer);

	if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
		std::cerr << "Erreur : le framebuffer de l'image finale est incomplet" << std::endl;

	glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

//-----------------------------------------------------------------------------
// Destructeur
//-----------------------------------------------------------------------------
Recorder::~Recorder()
{
	glDeleteRenderbuffers(1, &mColorBuffer);
	glDeleteFramebuffers(1, &mFBO);
}

//-----------------------------------------------------------------------------
// Lit les pixels de l'image (RGB, lignes de bas en haut)
//-----------------------------------------------------------------------------
void Recorder::readPixels(std::vector<unsigned char>& pixels)
{
	pixels.resize(mWidth * mHeight * 3);
	glBindFramebuffer(GL_READ_FRAMEBUFFER, mFBO);
	glPixelStorei(GL_PACK_ALIGNMENT, 1);	// lignes collées les unes aux autres, sans remplissage
	glReadPixels(0, 0, mWidth, mHeight, GL_RGB, GL_UNSIGNED_BYTE, pixels.data());
	glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
}

//-----------------------------------------------------------------------------
// Écrit l'image hors écran dans un fichier PNG
//-----------------------------------------------------------------------------
bool Recorder::savePNG(const std::string& filename)
{
	std::vector<unsigned char> pixels;
	readPixels(pixels);

	// OpenGL range les lignes de bas en haut, le PNG de haut en bas
	stbi_flip_vertically_on_write(1);
	if (!stbi_write_png(filename.c_str(), mWidth, mHeight, 3, pixels.data(), mWidth * 3))
	{
		std::cerr << "Erreur : impossible d'écrire " << filename << std::endl;
		return false;
	}
	return true;
}
