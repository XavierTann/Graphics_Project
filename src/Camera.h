#pragma once
#include <glm/glm.hpp>

class Camera {
public:

	// Spherical coordinates
    float yaw = -90.0f; 
    float pitch = 0.0f;
    float radius = 2.5f;
    float fovy = 45.0f;
    glm::vec3 target = glm::vec3(0.0f, 0.5f, 0.0f);
    glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f);


	void update(); // Call this after changing any of the above parameters

	// Getters
    glm::mat4 getView()       const { return view_; }
    glm::mat4 getProjection() const { return proj_; }
    glm::mat4 getViewProj()   const { return viewProj_; }
    glm::vec3 getPosition()   const { return position_; }
    glm::vec3 getBillboardRight() const { return right_; }
    glm::vec3 getBillboardUp()    const { return camUp_; }

	// Input handling
    void onMouseButton(int button, int action, float xpos, float ypos);
    void onMouseMove(float xpos, float ypos);
    void onScroll(float yoffset);
    void setAspect(float aspect) { aspect_ = aspect; }

private:
    
	float aspect_ = 16.0f / 9.0f; // Aspect ratio for projection matrix

	//mouse interaction state
    bool  isDragging_ = false; 
    bool  isPanning_ = false;
    float lastMouseX_ = 0.0f;
    float lastMouseY_ = 0.0f;

	//default camera parameters
    glm::vec3 position_ = glm::vec3(0.0f, 0.5f, 2.5f);
    glm::vec3 right_ = glm::vec3(1.0f, 0.0f, 0.0f);
    glm::vec3 camUp_ = glm::vec3(0.0f, 1.0f, 0.0f);
    glm::mat4 view_ = glm::mat4(1.0f);
    glm::mat4 proj_ = glm::mat4(1.0f);
    glm::mat4 viewProj_ = glm::mat4(1.0f);
};