#include <GLFW/glfw3.h>
#include <imgui.h>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_opengl3.h>
#include <iostream>

int main() {
    // Initialize GLFW
    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW" << std::endl;
        return -1;
    }

    // Set OpenGL version
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);

    // Create window
    GLFWwindow* window = glfwCreateWindow(1280, 720, "Fire Simulation UI", nullptr, nullptr);
    if (!window) {
        std::cerr << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1); // Enable vsync

    // Print OpenGL version
    std::cout << "OpenGL version: " << glGetString(GL_VERSION) << std::endl;
    std::cout << "GLSL version: " << glGetString(GL_SHADING_LANGUAGE_VERSION) << std::endl;

    // Initialize ImGui
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();

    // Setup Platform/Renderer backends
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 120");

    // UI variables
    float fireStrength = 1.0f;
    float windStrength = 0.5f;
    float windDirection = 0.0f; // in degrees
    float smokeDensity = 0.3f;
    bool enableParticles = true;

    // Main loop
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        // Start the Dear ImGui frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        // Create UI window
        ImGui::Begin("Fire Simulation Controls");

        ImGui::Text("Adjust the simulation parameters:");

        ImGui::SliderFloat("Fire Strength", &fireStrength, 0.0f, 5.0f);
        ImGui::SliderFloat("Wind Strength", &windStrength, 0.0f, 2.0f);
        ImGui::SliderFloat("Wind Direction (degrees)", &windDirection, 0.0f, 360.0f);
        ImGui::SliderFloat("Smoke Density", &smokeDensity, 0.0f, 1.0f);
        ImGui::Checkbox("Enable Particles", &enableParticles);

        ImGui::Text("Fire Strength: %.2f", fireStrength);
        ImGui::Text("Wind Strength: %.2f", windStrength);
        ImGui::Text("Wind Direction: %.2f", windDirection);
        ImGui::Text("Smoke Density: %.2f", smokeDensity);
        ImGui::Text("Particles: %s", enableParticles ? "Enabled" : "Disabled");

        ImGui::End();

        // Rendering
        ImGui::Render();
        int display_w, display_h;
        glfwGetFramebufferSize(window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        glClearColor(0.45f, 0.55f, 0.60f, 1.00f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window);
    }

    // Cleanup
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}