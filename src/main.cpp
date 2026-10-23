//-----------------------------------------------------------------------------
// main.cpp : la descente du skieur
//
// Utilisation :
//   ./skieur                              mode interactif
//   ./skieur --frame <t> <image.png>      rend l'instant t (en s) dans une image 1920x1080
//   ./skieur --frame <t> <image.png> x y z cx cy cz
//                                         idem, vu depuis (x,y,z) vers (cx,cy,cz)
//   ./skieur --record <video.mp4>         rend toute la vidéo (60 s à 60 images/s)
//
// Mode interactif :
//   C            caméra animée (storyboard) / caméra libre
//   Espace       lecture / pause         Flèches gauche/droite : -2 s / +2 s
//   Caméra libre : souris, W/A/S/D, Z/X (Maj : x10), molette = zoom
//   P            affiche la commande --frame qui reproduit la vue
//   F1           fil de fer              Échap : quitter
//
// Point de départ : la démo Lighting_Multiple du cours (même structure,
// mêmes contrôles).
//-----------------------------------------------------------------------------
#include <iostream>
#include <sstream>
#include <string>
#include <cstdlib>
#include <cmath>
#include <cstdio>
#include <vector>
#include <algorithm>
#define GLEW_STATIC
#include "GL/glew.h"	// Important - this header must come before glfw3 header
#include "GLFW/glfw3.h"
#include "glm/glm.hpp"
#include "glm/gtc/matrix_transform.hpp"

#include "ShaderProgram.h"
#include "Texture2D.h"
#include "Camera.h"
#include "Scene.h"
#include "World.h"
#include "Timeline.h"
#include "Recorder.h"


// Global Variables
const char* APP_TITLE = "La descente du skieur";
int gWindowWidth = 1280;
int gWindowHeight = 720;
GLFWwindow* gWindow = NULL;
bool gWireframe = false;

const int FRAME_WIDTH = 1920;		// taille des images rendues (--frame, --record)
const int FRAME_HEIGHT = 1080;
const int FPS = 60;					// images par seconde de la vidéo

// Lecture du storyboard en mode interactif
bool gAnimatedCamera = true;		// caméra animée (true) ou libre (false)
bool gPlaying = true;				// lecture ou pause
float gTime = 0.0f;					// instant t affiché, en secondes
const float NEAR_PLANE = 0.5f;		// m
const float FAR_PLANE = 5000.0f;	// m (200 dans le cours : trop court pour une montagne)

// Angles de la caméra FPS du cours pour regarder de eye vers target
// (elle regarde dans la direction (cos(pitch) sin(yaw), sin(pitch), cos(pitch) cos(yaw)))
float yawTowards(const glm::vec3& eye, const glm::vec3& target)
{
	glm::vec3 dir = glm::normalize(target - eye);
	return std::atan2(dir.x, dir.z);
}
float pitchTowards(const glm::vec3& eye, const glm::vec3& target)
{
	glm::vec3 dir = glm::normalize(target - eye);
	return std::asin(dir.y);
}

FPSCamera fpsCamera(scene::OVERVIEW_EYE,
                    yawTowards(scene::OVERVIEW_EYE, scene::OVERVIEW_TARGET),
                    pitchTowards(scene::OVERVIEW_EYE, scene::OVERVIEW_TARGET));
const double ZOOM_SENSITIVITY = -3.0;
const float MOVE_SPEED = 20.0f;		// m/s, multipliée par 10 avec la touche Maj
const float MOUSE_SENSITIVITY = 0.1f;


// Function prototypes
void glfw_onKey(GLFWwindow* window, int key, int scancode, int action, int mode);
void glfw_onFramebufferSize(GLFWwindow* window, int width, int height);
void glfw_onMouseScroll(GLFWwindow* window, double deltaX, double deltaY);
void update(double elapsedTime);
void showFPS(GLFWwindow* window);
bool initOpenGL(bool visible);
int runInteractive();
int renderFrame(float t, const std::string& filename, bool customView, glm::vec3 eye, glm::vec3 target);
int recordVideo(const std::string& filename);
void renderAt(World& world, float t, float aspect);

//-----------------------------------------------------------------------------
// Main Application Entry Point
//-----------------------------------------------------------------------------
int main(int argc, char* argv[])
{
	// Lecture des arguments
	bool frameMode = false, recordMode = false;
	float frameTime = 0.0f;
	std::string outputFile;
	bool customView = false;
	glm::vec3 eye(0.0f), target(0.0f);

	if (argc >= 4 && std::string(argv[1]) == "--frame")
	{
		frameMode = true;
		frameTime = (float)std::atof(argv[2]);
		outputFile = argv[3];
		if (argc >= 10)
		{
			customView = true;
			eye    = glm::vec3(std::atof(argv[4]), std::atof(argv[5]), std::atof(argv[6]));
			target = glm::vec3(std::atof(argv[7]), std::atof(argv[8]), std::atof(argv[9]));
		}
	}
	else if (argc == 3 && std::string(argv[1]) == "--record")
	{
		recordMode = true;
		outputFile = argv[2];
	}
	else if (argc > 1)
	{
		std::cerr << "Utilisation : " << argv[0] << " [--frame <t> <image.png> [x y z cx cy cz]] [--record <video.mp4>]" << std::endl;
		return 1;
	}

	// En modes --frame et --record, la fenêtre reste cachée : on dessine hors écran
	if (!initOpenGL(!frameMode && !recordMode))
	{
		// An error occured
		std::cerr << "GLFW initialization failed" << std::endl;
		return -1;
	}

	// Les objets OpenGL sont détruits dans ces fonctions, avant glfwTerminate()
	int result;
	if (frameMode)
		result = renderFrame(frameTime, outputFile, customView, eye, target);
	else if (recordMode)
		result = recordVideo(outputFile);
	else
		result = runInteractive();

	glfwTerminate();
	return result;
}

//-----------------------------------------------------------------------------
// Dessine la scène à l'instant t, vue par la caméra animée du storyboard
//-----------------------------------------------------------------------------
void renderAt(World& world, float t, float aspect)
{
	timeline::CameraState cam = timeline::cameraState(t, world.piste, world.terrain);
	glm::mat4 view = glm::lookAt(cam.eye, cam.target, glm::vec3(0.0f, 1.0f, 0.0f));
	glm::mat4 projection = glm::perspective(glm::radians(cam.fov), aspect, NEAR_PLANE, FAR_PLANE);
	world.render(view, projection, cam.eye, t);
}

//-----------------------------------------------------------------------------
// Mode --frame : rend une seule image et l'enregistre en PNG
//-----------------------------------------------------------------------------
int renderFrame(float t, const std::string& filename, bool customView, glm::vec3 eye, glm::vec3 target)
{
	World world;
	if (!world.load())
		return 1;

	Recorder recorder(FRAME_WIDTH, FRAME_HEIGHT);
	recorder.begin();
	if (customView)		// vue imposée sur la ligne de commande
	{
		glm::mat4 view = glm::lookAt(eye, target, glm::vec3(0.0f, 1.0f, 0.0f));
		glm::mat4 projection = glm::perspective(glm::radians(45.0f), (float)FRAME_WIDTH / FRAME_HEIGHT, NEAR_PLANE, FAR_PLANE);
		world.render(view, projection, eye, t);
	}
	else				// caméra du storyboard
		renderAt(world, t, (float)FRAME_WIDTH / FRAME_HEIGHT);
	recorder.end();

	if (!recorder.savePNG(filename))
		return 1;
	std::cout << "Image t = " << t << " s écrite dans " << filename << std::endl;
	return 0;
}

//-----------------------------------------------------------------------------
// Mode --record : rend toute la vidéo HORS TEMPS RÉEL. L'image n vaut l'instant
// t = n / 60, quel que soit le temps de calcul : la vidéo est toujours fluide.
// Les images sont envoyées à ffmpeg par un tube (popen), qui les encode en MP4.
//-----------------------------------------------------------------------------
int recordVideo(const std::string& filename)
{
	World world;
	if (!world.load())
		return 1;

	// ffmpeg lit des images brutes RGB sur son entrée standard ("-i -").
	// OpenGL range les lignes de bas en haut : le filtre vflip les remet à l'endroit.
	std::ostringstream command;
	command << "ffmpeg -y -loglevel error -f rawvideo -pix_fmt rgb24 -s " << FRAME_WIDTH << "x" << FRAME_HEIGHT
	        << " -r " << FPS << " -i - -vf vflip -c:v libx264 -pix_fmt yuv420p -crf 18 \"" << filename << "\"";
	FILE* ffmpeg = popen(command.str().c_str(), "w");
	if (!ffmpeg)
	{
		std::cerr << "Erreur : impossible de lancer ffmpeg" << std::endl;
		return 1;
	}

	Recorder recorder(FRAME_WIDTH, FRAME_HEIGHT);
	std::vector<unsigned char> pixels;
	int frameCount = (int)(timeline::DURATION * FPS);
	double start = glfwGetTime();

	for (int frame = 0; frame < frameCount; frame++)
	{
		float t = (float)frame / FPS;	// temps fixe : ne dépend pas de la vitesse de calcul

		recorder.begin();
		renderAt(world, t, (float)FRAME_WIDTH / FRAME_HEIGHT);
		recorder.end();
		recorder.readPixels(pixels);
		fwrite(pixels.data(), 1, pixels.size(), ffmpeg);

		if (frame % FPS == 0)
		{
			double elapsed = glfwGetTime() - start;
			std::cout << "\rImage " << frame << " / " << frameCount << " (t = " << (int)t << " s, "
			          << (int)(elapsed / std::max(frame, 1) * (frameCount - frame)) << " s restantes)   " << std::flush;
		}
	}

	int status = pclose(ffmpeg);
	std::cout << "\nVidéo écrite dans " << filename << " en " << (int)(glfwGetTime() - start) << " s" << std::endl;
	return status == 0 ? 0 : 1;
}

//-----------------------------------------------------------------------------
// Mode interactif : lecture du storyboard, ou caméra libre (contrôles du cours)
//-----------------------------------------------------------------------------
int runInteractive()
{
	World world;
	if (!world.load())
		return 1;

	double lastTime = glfwGetTime();

	// Rendering loop
	while (!glfwWindowShouldClose(gWindow))
	{
		showFPS(gWindow);

		double currentTime = glfwGetTime();
		double deltaTime = currentTime - lastTime;

		// Poll for and process events
		glfwPollEvents();
		update(deltaTime);

		// Le temps du storyboard avance en temps réel pendant la lecture (et boucle)
		if (gPlaying)
			gTime = std::fmod(gTime + (float)deltaTime, timeline::DURATION);

		float aspect = (float)gWindowWidth / (float)gWindowHeight;
		if (gAnimatedCamera)
			renderAt(world, gTime, aspect);
		else
		{
			glm::mat4 view = fpsCamera.getViewMatrix();
			glm::mat4 projection = glm::perspective(glm::radians(fpsCamera.getFOV()), aspect, NEAR_PLANE, FAR_PLANE);
			world.render(view, projection, fpsCamera.getPosition(), gTime);
		}

		// Swap front and back buffers
		glfwSwapBuffers(gWindow);

		lastTime = currentTime;
	}

	return 0;
}

//-----------------------------------------------------------------------------
// Initialize GLFW and OpenGL
//-----------------------------------------------------------------------------
bool initOpenGL(bool visible)
{
	// Intialize GLFW
	// GLFW is configured.  Must be called before calling any GLFW functions
	if (!glfwInit())
	{
		// An error occured
		std::cerr << "GLFW initialization failed" << std::endl;
		return false;
	}

	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
	glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
	glfwWindowHint(GLFW_VISIBLE, visible ? GLFW_TRUE : GLFW_FALSE);
	glfwWindowHint(GLFW_SAMPLES, 4);	// MSAA 4x pour la fenêtre (le mode --frame a son propre MSAA)

	// Create an OpenGL 3.3 core, forward compatible context window
	gWindow = glfwCreateWindow(gWindowWidth, gWindowHeight, APP_TITLE, NULL, NULL);
	if (gWindow == NULL)
	{
		std::cerr << "Failed to create GLFW window" << std::endl;
		glfwTerminate();
		return false;
	}

	// Make the window's context the current one
	glfwMakeContextCurrent(gWindow);

	// Initialize GLEW
	glewExperimental = GL_TRUE;
	if (glewInit() != GLEW_OK)
	{
		std::cerr << "Failed to initialize GLEW" << std::endl;
		return false;
	}

	std::cout << "OpenGL : " << glGetString(GL_RENDERER) << std::endl;

	if (visible)
	{
		// Set the required callback functions
		glfwSetKeyCallback(gWindow, glfw_onKey);
		glfwSetFramebufferSizeCallback(gWindow, glfw_onFramebufferSize);
		glfwSetScrollCallback(gWindow, glfw_onMouseScroll);

		// Hides and grabs cursor, unlimited movement
		glfwSetInputMode(gWindow, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
		glfwSetCursorPos(gWindow, gWindowWidth / 2.0, gWindowHeight / 2.0);
	}

	// Define the viewport dimensions
	int w, h;
	glfwGetFramebufferSize(gWindow, &w, &h); // For retina display
	glViewport(0, 0, w, h);

	glEnable(GL_DEPTH_TEST);
	glEnable(GL_MULTISAMPLE);

	return true;
}

//-----------------------------------------------------------------------------
// Is called whenever a key is pressed/released via GLFW
//-----------------------------------------------------------------------------
void glfw_onKey(GLFWwindow* window, int key, int scancode, int action, int mode)
{
	if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
		glfwSetWindowShouldClose(window, GL_TRUE);

	if (key == GLFW_KEY_F1 && action == GLFW_PRESS)
	{
		gWireframe = !gWireframe;
		if (gWireframe)
			glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
		else
			glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
	}

	// P : affiche la commande --frame qui reproduit la vue actuelle
	if (key == GLFW_KEY_P && action == GLFW_PRESS)
	{
		if (gAnimatedCamera)
			std::cout << "./skieur --frame " << gTime << " vue.png" << std::endl;
		else
		{
			glm::vec3 eye = fpsCamera.getPosition();
			glm::vec3 target = eye + 100.0f * fpsCamera.getLook();
			std::cout << "./skieur --frame " << gTime << " vue.png "
			          << eye.x << " " << eye.y << " " << eye.z << " "
			          << target.x << " " << target.y << " " << target.z << std::endl;
		}
	}

	// C : caméra animée / caméra libre (on recentre la souris pour éviter un saut de la vue)
	if (key == GLFW_KEY_C && action == GLFW_PRESS)
	{
		gAnimatedCamera = !gAnimatedCamera;
		glfwSetCursorPos(window, gWindowWidth / 2.0, gWindowHeight / 2.0);
	}

	// Espace : lecture / pause. Flèches : on se déplace de 2 s dans le storyboard.
	if (key == GLFW_KEY_SPACE && action == GLFW_PRESS)
		gPlaying = !gPlaying;
	if (key == GLFW_KEY_RIGHT && (action == GLFW_PRESS || action == GLFW_REPEAT))
		gTime = std::min(gTime + 2.0f, timeline::DURATION - 0.01f);
	if (key == GLFW_KEY_LEFT && (action == GLFW_PRESS || action == GLFW_REPEAT))
		gTime = std::max(gTime - 2.0f, 0.0f);
}

//-----------------------------------------------------------------------------
// Is called when the window is resized
//-----------------------------------------------------------------------------
void glfw_onFramebufferSize(GLFWwindow* window, int width, int height)
{
	gWindowWidth = width;
	gWindowHeight = height;

	// Define the viewport dimensions
	int w, h;
	glfwGetFramebufferSize(gWindow, &w, &h); // For retina display
	glViewport(0, 0, w, h);
}

//-----------------------------------------------------------------------------
// Called by GLFW when the mouse wheel is rotated
//-----------------------------------------------------------------------------
void glfw_onMouseScroll(GLFWwindow* window, double deltaX, double deltaY)
{
	double fov = fpsCamera.getFOV() + deltaY * ZOOM_SENSITIVITY;

	fov = glm::clamp(fov, 1.0, 120.0);

	fpsCamera.setFOV((float)fov);
}

//-----------------------------------------------------------------------------
// Update stuff every frame
//-----------------------------------------------------------------------------
void update(double elapsedTime)
{
	// Avec la caméra animée, la souris et le clavier ne déplacent pas la caméra
	if (gAnimatedCamera)
		return;

	// Camera orientation
	double mouseX, mouseY;

	// Get the current mouse cursor position delta
	glfwGetCursorPos(gWindow, &mouseX, &mouseY);

	// Rotate the camera the difference in mouse distance from the center screen.  Multiply this delta by a speed scaler
	fpsCamera.rotate((float)(gWindowWidth / 2.0 - mouseX) * MOUSE_SENSITIVITY, (float)(gWindowHeight / 2.0 - mouseY) * MOUSE_SENSITIVITY);

	// Clamp mouse cursor to center of screen
	glfwSetCursorPos(gWindow, gWindowWidth / 2.0, gWindowHeight / 2.0);

	// Camera FPS movement : Maj pour aller 10 fois plus vite
	float speed = MOVE_SPEED;
	if (glfwGetKey(gWindow, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS)
		speed *= 10.0f;

	// Forward/backward
	if (glfwGetKey(gWindow, GLFW_KEY_W) == GLFW_PRESS)
		fpsCamera.move(speed * (float)elapsedTime * fpsCamera.getLook());
	else if (glfwGetKey(gWindow, GLFW_KEY_S) == GLFW_PRESS)
		fpsCamera.move(speed * (float)elapsedTime * -fpsCamera.getLook());

	// Strafe left/right
	if (glfwGetKey(gWindow, GLFW_KEY_A) == GLFW_PRESS)
		fpsCamera.move(speed * (float)elapsedTime * -fpsCamera.getRight());
	else if (glfwGetKey(gWindow, GLFW_KEY_D) == GLFW_PRESS)
		fpsCamera.move(speed * (float)elapsedTime * fpsCamera.getRight());

	// Up/down
	if (glfwGetKey(gWindow, GLFW_KEY_Z) == GLFW_PRESS)
		fpsCamera.move(speed * (float)elapsedTime * glm::vec3(0.0f, 1.0f, 0.0f));
	else if (glfwGetKey(gWindow, GLFW_KEY_X) == GLFW_PRESS)
		fpsCamera.move(speed * (float)elapsedTime * -glm::vec3(0.0f, 1.0f, 0.0f));
}

//-----------------------------------------------------------------------------
// Code computes the average frames per second, and also the average time it takes
// to render one frame.  These stats are appended to the window caption bar.
//-----------------------------------------------------------------------------
void showFPS(GLFWwindow* window)
{
	static double previousSeconds = 0.0;
	static int frameCount = 0;
	double elapsedSeconds;
	double currentSeconds = glfwGetTime(); // returns number of seconds since GLFW started, as double float

	elapsedSeconds = currentSeconds - previousSeconds;

	// Limit text updates to 4 times per second
	if (elapsedSeconds > 0.25)
	{
		previousSeconds = currentSeconds;
		double fps = (double)frameCount / elapsedSeconds;
		double msPerFrame = 1000.0 / fps;

		// The C++ way of setting the window title
		std::ostringstream outs;
		outs.precision(3);	// decimal places
		outs << std::fixed
			<< APP_TITLE << "    "
			<< "t = " << gTime << " s" << (gPlaying ? "" : " (pause)")
			<< (gAnimatedCamera ? "    caméra animée" : "    caméra libre") << "    "
			<< "FPS: " << fps << "    "
			<< "Frame Time: " << msPerFrame << " (ms)";
		glfwSetWindowTitle(window, outs.str().c_str());

		// Reset for next average.
		frameCount = 0;
	}

	frameCount++;
}
