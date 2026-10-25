//-----------------------------------------------------------------------------
// PostProcess.cpp : rendu HDR, bloom et tone mapping
//-----------------------------------------------------------------------------
#include "PostProcess.h"
#include "Scene.h"
#include <iostream>
#include <algorithm>

//-----------------------------------------------------------------------------
// Constructeur / destructeur
//-----------------------------------------------------------------------------
PostProcess::PostProcess()
	: mWidth(0), mHeight(0), mMsaaFBO(0), mMsaaColor(0), mMsaaDepth(0), mHdrFBO(0), mHdrTexture(0), mVAO(0)
{
	for (int i = 0; i < LEVELS; i++)
		mLevelFBO[i] = mLevelTexture[i] = 0;
}

PostProcess::~PostProcess()
{
	release();
	if (mVAO != 0)
		glDeleteVertexArrays(1, &mVAO);
}

//-----------------------------------------------------------------------------
// Shaders. Les trois passes dessinent un triangle qui couvre tout l'écran : ses
// sommets sont calculés dans post.vert à partir de leur numéro (pas de VBO).
//-----------------------------------------------------------------------------
bool PostProcess::load()
{
	if (!mResolveShader.loadShaders("shaders/post.vert", "shaders/resolve.frag")
	 || !mDownShader.loadShaders("shaders/post.vert", "shaders/bloom_down.frag")
	 || !mUpShader.loadShaders("shaders/post.vert", "shaders/bloom_up.frag")
	 || !mFinalShader.loadShaders("shaders/post.vert", "shaders/tonemap.frag"))
		return false;
	glGenVertexArrays(1, &mVAO);
	return true;
}

//-----------------------------------------------------------------------------
// Crée une texture couleur HDR et le framebuffer qui dessine dedans
//-----------------------------------------------------------------------------
static void createTarget(int width, int height, GLuint& fbo, GLuint& texture)
{
	glGenTextures(1, &texture);
	glBindTexture(GL_TEXTURE_2D, texture);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, width, height, 0, GL_RGBA, GL_FLOAT, nullptr);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);	// lecture bilinéaire : sert au flou
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

	glGenFramebuffers(1, &fbo);
	glBindFramebuffer(GL_FRAMEBUFFER, fbo);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture, 0);
	if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
		std::cerr << "Erreur : framebuffer HDR incomplet" << std::endl;
}

//-----------------------------------------------------------------------------
// Crée les framebuffers pour une image de width x height pixels
//-----------------------------------------------------------------------------
void PostProcess::allocate(int width, int height)
{
	release();
	mWidth = width;
	mHeight = height;

	// Image HDR multi-échantillons : couleur en virgule flottante + profondeur
	glGenFramebuffers(1, &mMsaaFBO);
	glBindFramebuffer(GL_FRAMEBUFFER, mMsaaFBO);
	glGenTextures(1, &mMsaaColor);		// une texture (et pas un renderbuffer) : le shader de résolution la lit
	glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, mMsaaColor);
	glTexImage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, SAMPLES, GL_RGBA16F, width, height, GL_TRUE);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D_MULTISAMPLE, mMsaaColor, 0);
	glGenRenderbuffers(1, &mMsaaDepth);
	glBindRenderbuffer(GL_RENDERBUFFER, mMsaaDepth);
	glRenderbufferStorageMultisample(GL_RENDERBUFFER, SAMPLES, GL_DEPTH_COMPONENT24, width, height);
	glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, mMsaaDepth);
	if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
		std::cerr << "Erreur : framebuffer HDR multi-échantillons incomplet" << std::endl;

	// La même image résolue, puis les niveaux du bloom (moitié, quart, ...)
	createTarget(width, height, mHdrFBO, mHdrTexture);
	for (int i = 0; i < LEVELS; i++)
	{
		mLevelWidth[i] = std::max(1, width >> (i + 1));
		mLevelHeight[i] = std::max(1, height >> (i + 1));
		createTarget(mLevelWidth[i], mLevelHeight[i], mLevelFBO[i], mLevelTexture[i]);
	}
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

//-----------------------------------------------------------------------------
// Libère les framebuffers
//-----------------------------------------------------------------------------
void PostProcess::release()
{
	if (mMsaaFBO == 0)
		return;
	glDeleteFramebuffers(1, &mMsaaFBO);
	glDeleteTextures(1, &mMsaaColor);
	glDeleteRenderbuffers(1, &mMsaaDepth);
	glDeleteFramebuffers(1, &mHdrFBO);
	glDeleteTextures(1, &mHdrTexture);
	glDeleteFramebuffers(LEVELS, mLevelFBO);
	glDeleteTextures(LEVELS, mLevelTexture);
	mMsaaFBO = 0;
}

//-----------------------------------------------------------------------------
// Les dessins suivants vont dans l'image HDR (recréée si la taille a changé)
//-----------------------------------------------------------------------------
void PostProcess::begin(int width, int height)
{
	if (width != mWidth || height != mHeight)
		allocate(width, height);
	glBindFramebuffer(GL_FRAMEBUFFER, mMsaaFBO);
	glViewport(0, 0, width, height);
	glEnable(GL_DEPTH_TEST);
}

//-----------------------------------------------------------------------------
// Bloom et tone mapping : de l'image HDR vers le framebuffer target
//-----------------------------------------------------------------------------
void PostProcess::end(GLuint target)
{
	// Les passes suivantes dessinent un triangle plein : ni profondeur, ni fil de fer
	GLint polygonMode[2];
	glGetIntegerv(GL_POLYGON_MODE, polygonMode);
	glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
	glDisable(GL_DEPTH_TEST);
	glBindVertexArray(mVAO);

	// 1. Résolution du MSAA : moyenne pondérée des 4 échantillons de chaque pixel
	glBindFramebuffer(GL_FRAMEBUFFER, mHdrFBO);
	mResolveShader.use();
	mResolveShader.setUniformSampler("samples", 0);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, mMsaaColor);
	glDrawArrays(GL_TRIANGLES, 0, 3);
	glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, 0);

	// 2. Réductions : chaque niveau est la moitié du précédent, en moyennant 4 x 4 pixels
	mDownShader.use();
	mDownShader.setUniformSampler("source", 0);
	glActiveTexture(GL_TEXTURE0);
	GLuint source = mHdrTexture;
	int sourceWidth = mWidth, sourceHeight = mHeight;
	for (int i = 0; i < LEVELS; i++)
	{
		glBindFramebuffer(GL_FRAMEBUFFER, mLevelFBO[i]);
		glViewport(0, 0, mLevelWidth[i], mLevelHeight[i]);
		glBindTexture(GL_TEXTURE_2D, source);
		mDownShader.setUniform("texel", glm::vec2(1.0f / sourceWidth, 1.0f / sourceHeight));
		glDrawArrays(GL_TRIANGLES, 0, 3);
		source = mLevelTexture[i];
		sourceWidth = mLevelWidth[i];
		sourceHeight = mLevelHeight[i];
	}

	// 3. Remontée : du plus petit niveau au plus grand, chaque niveau agrandi (et
	//    flouté) est AJOUTÉ au niveau au-dessus (mélange additif). Le niveau 0
	//    contient à la fin la somme de tous les flous : petits et grands halos.
	mUpShader.use();
	mUpShader.setUniformSampler("source", 0);
	glEnable(GL_BLEND);
	glBlendFunc(GL_ONE, GL_ONE);
	for (int i = LEVELS - 1; i > 0; i--)
	{
		glBindFramebuffer(GL_FRAMEBUFFER, mLevelFBO[i - 1]);
		glViewport(0, 0, mLevelWidth[i - 1], mLevelHeight[i - 1]);
		glBindTexture(GL_TEXTURE_2D, mLevelTexture[i]);
		mUpShader.setUniform("texel", glm::vec2(1.0f / mLevelWidth[i], 1.0f / mLevelHeight[i]));
		glDrawArrays(GL_TRIANGLES, 0, 3);
	}
	glDisable(GL_BLEND);

	// 4. Image finale : scène + bloom, tone mapping, gamma
	glBindFramebuffer(GL_FRAMEBUFFER, target);
	glViewport(0, 0, mWidth, mHeight);
	mFinalShader.use();
	mFinalShader.setUniformSampler("scene", 0);
	mFinalShader.setUniformSampler("bloom", 1);
	mFinalShader.setUniform("bloomStrength", scene::BLOOM_STRENGTH);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, mHdrTexture);
	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, mLevelTexture[0]);
	glDrawArrays(GL_TRIANGLES, 0, 3);

	glBindTexture(GL_TEXTURE_2D, 0);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, 0);
	glBindVertexArray(0);
	glEnable(GL_DEPTH_TEST);
	glPolygonMode(GL_FRONT_AND_BACK, polygonMode[0]);
}
