
#include "luz_app.h"
#pragma comment(lib, "Dxva2.lib")
#if defined(IMGUI_IMPL_OPENGL_ES2)
#include <GLES2/gl2.h>
#endif
#include <GLFW/glfw3.h> // Will drag system OpenGL headers
#if defined(_MSC_VER) && (_MSC_VER >= 1900) && !defined(IMGUI_DISABLE_WIN32_FUNCTIONS)
#pragma comment(lib, "legacy_stdio_definitions")
#endif
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include "ini.h"


static void glfw_error_callback(int error, const char* description)
{
    fprintf(stderr, "Glfw Error %d: %s\n", error, description);
}

void configFileRead(std::string option, std::string& val) {
    mINI::INIFile file("luz_app_options.ini");
    mINI::INIStructure ini;
    file.read(ini);
    val = ini["options"]["port"];
}

void configFileWrite(std::string option, std::string newValue) {
    mINI::INIFile file("luz_app_options.ini");
    mINI::INIStructure ini;
    file.read(ini);
    ini["options"][option] = newValue;
}

//from: https://learn.microsoft.com/en-us/cpp/text/how-to-convert-between-various-string-types?view=msvc-170
wchar_t* to_wchar(const char* chars) {
    size_t newsize = strlen(chars) + 1;
    wchar_t* convertedString = new wchar_t[newsize];
    size_t convertedChars = 0;
    mbstowcs_s(&convertedChars, convertedString, newsize, chars, _TRUNCATE);
    std::wcout << convertedString;
    return convertedString;
}

//Using code from  imGUI openGL demo
int main()
{
    // Setup window
    glfwSetErrorCallback(glfw_error_callback);
    if (!glfwInit())
        return 1;

    // Decide GL+GLSL versions
    #if defined(IMGUI_IMPL_OPENGL_ES2)
    // GL ES 2.0 + GLSL 100
    const char* glsl_version = "#version 100";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
    glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_ES_API);
    #elif defined(__APPLE__)
    // GL 3.2 + GLSL 150
    const char* glsl_version = "#version 150";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);  // 3.2+ only
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);            // Required on Mac
    #else
    // GL 3.0 + GLSL 130
    const char* glsl_version = "#version 130";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
    

    //glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);  // 3.2+ only
    //glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);            // 3.0+ only
    #endif

    glfwWindowHint(GLFW_TRANSPARENT_FRAMEBUFFER, GLFW_TRUE);

    int count, windowWidth, windowHeight, monitorX, monitorY;

    // Get monitor height and width and set window width and height accordingly
    GLFWmonitor** glfwMonitors = glfwGetMonitors(&count);
    const GLFWvidmode* videoMode = glfwGetVideoMode(glfwMonitors[0]);
    windowWidth = static_cast<int>(videoMode->width / 4);
    windowHeight = static_cast<int>(videoMode->height / 4);
    glfwGetMonitorPos(glfwMonitors[0], &monitorX, &monitorY);

    // Create glfw window
    GLFWwindow* window = glfwCreateWindow(windowWidth, windowHeight, "Brightness Adjuster", NULL, NULL);

    if (window == NULL)
        return 1;
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1); // Enable vsync

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();
  
    // Setup Platform/Renderer backends
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init(glsl_version);

    //Setup LuzApp
    LuzApp LuzApp;

    ImGuiWindowFlags windowFlags = ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse;

   
    // Main Loop 
    while (!glfwWindowShouldClose(window)) {
       
        glfwPollEvents();

        int display_w, display_h;
        glfwGetFramebufferSize(window, &display_w, &display_h);

        //Dear ImGui frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();    

        //Set Dear ImGui frame size
        ImGui::SetNextWindowSize(ImVec2(display_w, display_h));
        ImGui::SetNextWindowPos(ImVec2(0, 0));
      
        LuzApp.windowContent(windowFlags); 
        ImGui::Render();
        glViewport(0, 0, display_w, display_h);
        glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        glfwSwapBuffers(window);
    }

  
    //Close monitors after app termination
    LuzApp.closeMonitors();
    LuzApp.closePortHandle();

    //GUI clean up
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}

