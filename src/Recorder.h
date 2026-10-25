//-----------------------------------------------------------------------------
// Recorder.h : rendu hors écran et sauvegarde des images.
//
// L'image finale (après bloom et tone mapping, voir PostProcess) est dessinée
// dans un framebuffer (FBO) au lieu de la fenêtre : sa taille (1920x1080) ne
// dépend donc pas de la fenêtre, qui peut même être cachée. On la lit ensuite
// avec glReadPixels, pour la vidéo ou une image PNG.
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

	GLuint framebuffer() const { return mFBO; }			// où dessiner l'image finale
	void readPixels(std::vector<unsigned char>& pixels);	// l'image, en RGB, de bas en haut
	bool savePNG(const std::string& filename);

	int width() const  { return mWidth; }
	int height() const { return mHeight; }

private:
	int mWidth, mHeight;
	GLuint mFBO;			// le framebuffer de l'image finale
	GLuint mColorBuffer;	// son image couleur (8 bits par composante)
};

#endif // RECORDER_H
