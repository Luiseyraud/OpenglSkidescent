//-----------------------------------------------------------------------------
// Recorder.cpp : rendu hors écran et sauvegarde des images
//-----------------------------------------------------------------------------
#include "Recorder.h"
#include <iostream>
#include <vector>
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write/stb_image_write.h"

//-----------------------------------------------------------------------------
// Constructeur : crée les deux FBO
//-----------------------------------------------------------------------------
Recorder::Recorder(int width, int height)
	: mWidth(width), mHeight(height)
{
	// FBO de dessin : couleur + profondeur, 4 échantillons par pixel
	glGenFramebuffers(1, &mFBO);
	glBindFramebuffer(GL_FRAMEBUFFER, mFBO);

	glGenRenderbuffers(1, &mColorBuffer);
	glBindRenderbuffer(GL_RENDERBUFFER, mColorBuffer);
	glRenderbufferStorageMultisample(GL_RENDERBUFFER, SAMPLES, GL_RGBA8, mWidth, mHeight);
	glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, mColorBuffer);

	glGenRenderbuffers(1, &mDepthBuffer);
	glBindRenderbuffer(GL_RENDERBUFFER, mDepthBuffer);
	glRenderbufferStorageMultisample(GL_RENDERBUFFER, SAMPLES, GL_DEPTH_COMPONENT24, mWidth, mHeight);
	glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, mDepthBuffer);

	if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
		std::cerr << "Erreur : le framebuffer de dessin est incomplet" << std::endl;

	// FBO résolu : une image couleur normale, qu'on peut lire
	glGenFramebuffers(1, &mResolveFBO);
	glBindFramebuffer(GL_FRAMEBUFFER, mResolveFBO);

	glGenRenderbuffers(1, &mResolveBuffer);
	glBindRenderbuffer(GL_RENDERBUFFER, mResolveBuffer);
	glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA8, mWidth, mHeight);
	glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, mResolveBuffer);

	if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
		std::cerr << "Erreur : le framebuffer résolu est incomplet" << std::endl;

	glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

//-----------------------------------------------------------------------------
// Destructeur
//-----------------------------------------------------------------------------
Recorder::~Recorder()
{
	glDeleteRenderbuffers(1, &mColorBuffer);
	glDeleteRenderbuffers(1, &mDepthBuffer);
	glDeleteRenderbuffers(1, &mResolveBuffer);
	glDeleteFramebuffers(1, &mFBO);
	glDeleteFramebuffers(1, &mResolveFBO);
}

//-----------------------------------------------------------------------------
// Les dessins suivants vont dans l'image hors écran
//-----------------------------------------------------------------------------
void Recorder::begin()
{
	glBindFramebuffer(GL_FRAMEBUFFER, mFBO);
	glViewport(0, 0, mWidth, mHeight);
}

//-----------------------------------------------------------------------------
// Résout le MSAA (copie du FBO de dessin vers le FBO résolu), puis retour à la fenêtre
//-----------------------------------------------------------------------------
void Recorder::end()
{
	glBindFramebuffer(GL_READ_FRAMEBUFFER, mFBO);
	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, mResolveFBO);
	glBlitFramebuffer(0, 0, mWidth, mHeight, 0, 0, mWidth, mHeight, GL_COLOR_BUFFER_BIT, GL_NEAREST);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

//-----------------------------------------------------------------------------
// Lit les pixels de l'image résolue (RGB, lignes de bas en haut)
//-----------------------------------------------------------------------------
void Recorder::readPixels(std::vector<unsigned char>& pixels)
{
	pixels.resize(mWidth * mHeight * 3);
	glBindFramebuffer(GL_READ_FRAMEBUFFER, mResolveFBO);
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
