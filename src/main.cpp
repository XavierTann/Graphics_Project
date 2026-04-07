#define NOMINMAX 

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <imgui/imgui.h>
#include <imgui/imgui_impl_glfw.h>
#include <imgui/imgui_impl_opengl3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <iostream>
#include <algorithm>
#include <cmath>

#include "shader.h"
#include "shaderSource.h"
#include "Camera.h"
#include "Scene.h"
#include "Renderer.h"
#include "UI.h"
#include "SceneObject.h"

/***********************************************************************/
/***************************   Window State   **************************/
/***********************************************************************/

static unsigned int winWidth = 1280;
static unsigned int winHeight = 720;
static float        lastTime = 0.0f;

/***********************************************************************/
/***************************   Subsystems   ****************************/
/***********************************************************************/

static GLFWwindow* window = nullptr;
static Camera      camera;
static Scene       scene;
static Renderer    renderer;
static UI          ui;
static shader      flameShader;
static shader      smokeShader;

/***********************************************************************/
/*********************   Object Drag State   **************************/
/***********************************************************************/

static bool  isObjectDragging = false;
static int   draggingObjectIndex = -1;
static float lastMouseX = 0.0f;
static float lastMouseY = 0.0f;

/***********************************************************************/
/**********************   Forward Declarations   **********************/
/***********************************************************************/

static void processKeyboard();
static void renderFrame(float dt, float now);
static void tryPickObject(float xpos, float ypos);
static void handleObjectDrag(double xpos, double ypos);

static void cb_framebufferSize(GLFWwindow* win, int w, int h);
static void cb_mouseButton(GLFWwindow* win, int button, int action, int mods);
static void cb_cursorPos(GLFWwindow* win, double xpos, double ypos);
static void cb_scroll(GLFWwindow* win, double xoffset, double yoffset);
static void cb_glfwError(int code, const char* desc);

/***********************************************************************/
/****************************   main   *********************************/
/***********************************************************************/

int main()
{
	//glfw initialisation and window creation
	glfwSetErrorCallback(cb_glfwError);
	if (!glfwInit()) {
		std::cerr << "Failed to initialise GLFW\n";
		return -1;
	}

#ifdef __APPLE__
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
	glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#else
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
#endif
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

	window = glfwCreateWindow((int)winWidth, (int)winHeight, "Ember Engine", NULL, NULL);
	if (!window) {
		std::cerr << "Failed to create GLFW window\n";
		glfwTerminate();
		return -1;
	}
	glfwMakeContextCurrent(window);
	glfwSetFramebufferSizeCallback(window, cb_framebufferSize);
	glfwSetMouseButtonCallback(window, cb_mouseButton);
	glfwSetCursorPosCallback(window, cb_cursorPos);
	glfwSetScrollCallback(window, cb_scroll);

	if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
		std::cerr << "Failed to initialise GLAD\n";
		return -1;
	}
	glEnable(GL_DEPTH_TEST);
	glEnable(GL_BLEND);

	//imGui initialisation
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGui::StyleColorsDark();
	ImGui_ImplGlfw_InitForOpenGL(window, true);
	ImGui_ImplOpenGL3_Init("#version 330");

	//shader compilation
	flameShader.setUpShader(particleVertexShaderSource, particleFragmentShaderSource);
	smokeShader.setUpShader(particleVertexShaderSource, smokeFragmentShaderSource);

	//render initialisation
	renderer.init();

	//scene initialisation
	scene.availableMeshNames = &renderer.meshLoader().availableMeshes;
	scene.meshLoader = &renderer.meshLoader();
	scene.init();

	//camera initialisation
	camera.setAspect((float)winWidth / (float)winHeight);
	camera.update();

	//ui initialisation
	ui.init(&scene, &camera);

	/***********************************************************************/
	/***************************   Render Loop   **************************/
	/***********************************************************************/

	while (!glfwWindowShouldClose(window))
	{
		glfwPollEvents();
		processKeyboard();

		float now = (float)glfwGetTime();
		float dt = now - lastTime;
		if (dt > 0.1f) dt = 0.016f;
		lastTime = now;

		// Start the ImGui frame
		ImGui_ImplOpenGL3_NewFrame();
		ImGui_ImplGlfw_NewFrame();
		ImGui::NewFrame();
		// Build the UI for this frame
		ui.draw(ImGui::GetIO());


		// Act on UI signals from this frame
		if (ui.wantRestart) scene.reset();

		// Re-scan mesh folder
		renderer.meshLoader().scan("data");

		// Update the scene and build instance data for rendering
		renderFrame(dt, now);

		// Render ImGui on top of the scene
		ImGui::Render();
		ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
		glfwSwapBuffers(window);
	}


	// Cleanup
	ImGui_ImplOpenGL3_Shutdown();
	ImGui_ImplGlfw_Shutdown();
	ImGui::DestroyContext();

	renderer.shutdown();
	glDeleteProgram(flameShader.ID);
	glDeleteProgram(smokeShader.ID);

	glfwDestroyWindow(window);
	glfwTerminate();
	return 0;
}

/***********************************************************************/
/************************   Rendering  *********************************/
/***********************************************************************/

//where the main scene rendering happens each frame. It also updates the scene state and prepares instance data for the billboards.

static void renderFrame(float dt, float now)
{
	glClearColor(0.05f, 0.05f, 0.05f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	// Update camera and scene, which will prepare instance data for rendering
	camera.update();
	scene.update(dt, now, camera.getViewProj());


	//get camera matrices and billboard vectors for rendering
	const glm::mat4& view = camera.getView();
	const glm::mat4& proj = camera.getProjection();
	const glm::vec3& right = camera.getBillboardRight();
	const glm::vec3& up = camera.getBillboardUp();
	BillboardLighting lighting;
	lighting.cameraPos = camera.getPosition();
	lighting.fireLightPos = scene.fireLightPosition();
	lighting.fireLightColor = scene.fireLightColor;
	lighting.fireLightIntensity = scene.fireLightStrength();
	lighting.fireLightRange = scene.fireLightRange;
	lighting.time = now;


	renderer.drawGrid(view, proj);
	renderer.drawMarkerPoint(view, proj, scene.emitter.origin, glm::vec4(0.20f, 1.0f, 0.25f, 1.0f), 10.0f);
	renderer.loadDecorationMesh("campfire.glb", glm::vec3(0.15f, -0.05f, 0.0f), 0.00075f);


	// Draw wind arrow if enabled
	if (scene.enableWind && scene.showWind) {
		glm::vec3 windDir = scene.globals.wind;
		float len = glm::length(windDir);
		if (len > 1e-4f) windDir /= len;
		renderer.drawWindArrow(view, proj,
			windDir * scene.windStrength,
			scene.emitter.origin);
	}


	// Draw smoke, flames, and scene objects (objects drawn last to appear on top of particles)
	renderer.drawMeshes(view, proj, scene.objects);
	renderer.drawObjectBillboards(scene.objectInstData, smokeShader, proj, view, right, up, lighting);
	renderer.drawDecorations(view, proj);
	renderer.drawFlames(scene.flameInstData, flameShader, proj, view, right, up, lighting);
	if (scene.smokeEnabled)
		renderer.drawSmoke(scene.smokeInstData, smokeShader, proj, view, right, up, lighting);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}

/***********************************************************************/
/***********************   Keyboard Input   ***************************/
/***********************************************************************/

static void processKeyboard()
{
	if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
		glfwSetWindowShouldClose(window, true);

	// Simulation
	if (glfwGetKey(window, GLFW_KEY_R) == GLFW_PRESS) scene.reset();

    int sel = scene.selectedObjectIndex;
    if (sel >= 0 && sel < (int)scene.objects.size()) {
        const float spd = 1.0f * 0.016f;
        SceneObject& obj = scene.objects[sel];
        if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) obj.pos.z -= spd;
        if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) obj.pos.z += spd;
        if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) obj.pos.x -= spd;
        if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) obj.pos.x += spd;
    }
}

/***********************************************************************/
/*********************   Object Picking / Drag   **********************/
/***********************************************************************/


static void tryPickObject(float xpos, float ypos)
{

	float bestDist2 = 18.0f * 18.0f;
	draggingObjectIndex = -1;

	for (int i = 0; i < (int)scene.objects.size(); ++i) {
		glm::vec4 clip = camera.getViewProj() * glm::vec4(scene.objects[i].pos, 1.0f);
		if (clip.w <= 0.0f) continue;
		glm::vec3 ndc = glm::vec3(clip) / clip.w;
		float sx = (ndc.x * 0.5f + 0.5f) * (float)winWidth;
		float sy = (1.0f - (ndc.y * 0.5f + 0.5f)) * (float)winHeight;
		float dx = sx - xpos, dy = sy - ypos;
		float d2 = dx * dx + dy * dy;
		if (d2 < bestDist2) { bestDist2 = d2; draggingObjectIndex = i; }
	}

	isObjectDragging = (draggingObjectIndex >= 0);
	if (isObjectDragging)
		scene.selectedObjectIndex = draggingObjectIndex;
}


static void handleObjectDrag(double xpos, double ypos)
{
	if (!isObjectDragging || draggingObjectIndex < 0) return;
	SceneObject& obj = scene.objects[draggingObjectIndex];

	float x = (2.0f * (float)xpos) / (float)winWidth - 1.0f;
	float y = 1.0f - (2.0f * (float)ypos) / (float)winHeight;

	glm::mat4 invVP = glm::inverse(camera.getViewProj());
	glm::vec4 nearP = invVP * glm::vec4(x, y, -1.0f, 1.0f);
	glm::vec4 farP = invVP * glm::vec4(x, y, 1.0f, 1.0f);
	if (nearP.w != 0.0f) nearP /= nearP.w;
	if (farP.w != 0.0f) farP /= farP.w;

	glm::vec3 rayO = glm::vec3(nearP);
	glm::vec3 rayD = glm::normalize(glm::vec3(farP - nearP));

	if (std::abs(rayD.y) > 1e-5f) {
		float t = (obj.pos.y - rayO.y) / rayD.y;
		if (t > 0.0f) {
			glm::vec3 hit = rayO + rayD * t;
			obj.pos.x = hit.x;
			obj.pos.z = hit.z;
		}
	}
}

/***********************************************************************/
/***********************   GLFW Callbacks   ***************************/
/***********************************************************************/

static void cb_glfwError(int code, const char* desc)
{
	std::cerr << "GLFW Error " << code << ": " << (desc ? desc : "") << '\n';
}

static void cb_framebufferSize(GLFWwindow*, int w, int h)
{
	glViewport(0, 0, w, h);
	winWidth = (unsigned int)w;
	winHeight = (unsigned int)h;
	camera.setAspect((float)w / (float)h);
}

static void cb_mouseButton(GLFWwindow* win, int button, int action, int mods)
{
	if (ImGui::GetIO().WantCaptureMouse) return;

	double xpos, ypos;
	glfwGetCursorPos(win, &xpos, &ypos);

	if (button == GLFW_MOUSE_BUTTON_LEFT) {
		if (action == GLFW_PRESS) {
			tryPickObject((float)xpos, (float)ypos);
			if (!isObjectDragging)
				camera.onMouseButton(button, action, (float)xpos, (float)ypos);
		}
		else {
			isObjectDragging = false;
			draggingObjectIndex = -1;
			camera.onMouseButton(button, action, (float)xpos, (float)ypos);
		}
	}

	if (button == GLFW_MOUSE_BUTTON_MIDDLE)
		camera.onMouseButton(button, action, (float)xpos, (float)ypos);
}


static void cb_cursorPos(GLFWwindow*, double xpos, double ypos)
{
	if (isObjectDragging)
		handleObjectDrag(xpos, ypos);
	else
		camera.onMouseMove((float)xpos, (float)ypos);
}

static void cb_scroll(GLFWwindow*, double, double yoffset)
{
	if (ImGui::GetIO().WantCaptureMouse) return;
	camera.onScroll((float)yoffset);
}
