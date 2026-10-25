//-----------------------------------------------------------------------------
// PostProcess.h : rendu HDR, bloom et tone mapping.
//
// 1. La scène est dessinée dans un framebuffer HDR : ses couleurs sont des
//    nombres à virgule (GL_RGBA16F) qui peuvent dépasser 1. Une ampoule, le
//    soleil ou un projecteur vu de face valent bien plus que le blanc (1).
//    Ce framebuffer a 4 échantillons par pixel (MSAA). On les "résout" (un
//    pixel = la moyenne de ses 4 échantillons) avec une moyenne pondérée : un
//    échantillon compte d'autant moins qu'il est lumineux. Sinon, devant le
//    soleil (bien plus clair que le blanc), un objet fin (bâton de ski) qui ne
//    couvre qu'un échantillon sur 4 disparaîtrait.
// 2. Bloom : on réduit l'image de moitié 6 fois de suite en la floutant, puis
//    on la remonte en additionnant les niveaux. Les zones très lumineuses
//    débordent ainsi sur leurs voisines, comme dans un vrai objectif.
// 3. Tone mapping : les couleurs jusqu'à 0,8 ne changent pas ; au-delà, elles
//    sont compressées en douceur vers 1 au lieu d'être coupées net. Puis la
//    correction gamma, faite ici une seule fois pour toute l'image.
//-----------------------------------------------------------------------------
#ifndef POST_PROCESS_H
#define POST_PROCESS_H

#include <vector>
#define GLEW_STATIC
#include "GL/glew.h"
#include "ShaderProgram.h"

class PostProcess
{
public:
	PostProcess();
	~PostProcess();

	bool load();						// shaders (contexte OpenGL requis)
	void begin(int width, int height);	// les dessins suivants vont dans l'image HDR
	void end(GLuint target);			// bloom + tone mapping, résultat dans le framebuffer target (0 : la fenêtre)

private:
	void allocate(int width, int height);
	void release();

	static const int SAMPLES = 4;		// MSAA
	static const int LEVELS = 6;		// niveaux de réduction du bloom

	int mWidth, mHeight;
	GLuint mMsaaFBO, mMsaaColor, mMsaaDepth;	// image HDR multi-échantillons, où l'on dessine (couleur : texture)
	GLuint mHdrFBO, mHdrTexture;				// la même, résolue (1 échantillon), qu'on peut lire
	GLuint mLevelFBO[LEVELS], mLevelTexture[LEVELS];	// le bloom : 1/2, 1/4, ... 1/64 de la taille
	int mLevelWidth[LEVELS], mLevelHeight[LEVELS];
	GLuint mVAO;								// vide : le triangle plein écran est calculé dans le shader

	ShaderProgram mResolveShader, mDownShader, mUpShader, mFinalShader;
};

#endif // POST_PROCESS_H
