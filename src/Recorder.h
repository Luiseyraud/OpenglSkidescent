//-----------------------------------------------------------------------------
// Recorder.h : rendu hors écran et sauvegarde des images.
//
// On dessine dans un framebuffer (FBO) au lieu de la fenêtre : la taille de
// l'image (1920x1080) ne dépend donc pas de la fenêtre, qui peut même être cachée.
//
// Anticrénelage MSAA 4x : le FBO de dessin garde 4 échantillons par pixel. On ne
// peut pas le lire directement : on le "résout" d'abord (moyenne des 4 échantillons)
// dans un second FBO normal, avec glBlitFramebuffer.
//-----------------------------------------------------------------------------
#ifndef RECORDER_H
#define RECORDER_H

#include <string>
#include <vector>
#define GLEW_STATIC
#include "GL/glew.h"

class Recorder
{
public:
	Recorder(int width, int height);	// contexte OpenGL requis
	~Recorder();

	void begin();	// les dessins suivants vont dans l'image hors écran
	void end();		// résout le MSAA, puis retour au dessin dans la fenêtre
	void readPixels(std::vector<unsigned char>& pixels);	// image résolue, en RGB, de bas en haut
	bool savePNG(const std::string& filename);

	int width() const  { return mWidth; }
	int height() const { return mHeight; }

private:
	static const int SAMPLES = 4;	// échantillons par pixel (MSAA)

	int mWidth, mHeight;
	GLuint mFBO;			// le framebuffer de dessin (multi-échantillons)
	GLuint mColorBuffer;	// son image couleur
	GLuint mDepthBuffer;	// son tampon de profondeur
	GLuint mResolveFBO;		// le framebuffer résolu (1 échantillon), celui qu'on lit
	GLuint mResolveBuffer;	// son image couleur
};

#endif // RECORDER_H
