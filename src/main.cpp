#include "view/MainWindow.h"
#include "util/Downloader.h"

// Подключаем ImGui для работы с окнами
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

#include <GLFW/glfw3.h>
#include <thread>

int main() {
    // Инициализация GLFW
    if (!glfwInit()) {
        return -1;
    }
    
    GLFWwindow* window = glfwCreateWindow(1280, 720, "SJLauncher", nullptr, nullptr);
    if (!window) {
        glfwTerminate();
        return -1;
    }
    
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1); // VSync
    
    // Инициализация ImGui
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    
    // Настройка стилей и шрифтов
    SetupModrinthStyle();
    SetupFonts(io);
    
    // Инициализация ImGui для GLFW и OpenGL
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 130");
    
    // Загрузка списка версий в фоне
    std::thread(FetchMinecraftVersions).detach();
    
    // Главный цикл
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        
        // Новый кадр ImGui
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
        
        // Рендер UI
        RenderMainWindow(window, io);
        
        // Рендер ImGui
        ImGui::Render();
        int display_w, display_h;
        glfwGetFramebufferSize(window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        
        glfwSwapBuffers(window);
    }
    
    // Очистка
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glfwDestroyWindow(window);
    glfwTerminate();
    
    return 0;
}