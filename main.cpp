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
#include <iostream>
#include <vector>
#include <string>
#include <span>
#include "imgui.h"
#include <Windows.h>
#include <physicalmonitorenumerationapi.h>
#include <highlevelmonitorconfigurationapi.h>
#include "misc/cpp/imgui_stdlib.h"


struct Monitor {
    int brightness;
};

//callback function requeired for EnumDisplayMonitors
BOOL CALLBACK MonitorEnumProc(HMONITOR hMonitor, HDC hdcMonitor, LPRECT lprcMonitor, LPARAM dwData)
{
    std::vector<HMONITOR>* data = reinterpret_cast<std::vector<HMONITOR>*>(dwData);
    data->push_back(hMonitor);
    return TRUE;
}

static void glfw_error_callback(int error, const char* description)
{
    fprintf(stderr, "Glfw Error %d: %s\n", error, description);
}

void configFileRead(std::string option, std::string& val) {
    mINI::INIFile file("luz_app_options.ini");
    mINI::INIStructure ini;
    file.read(ini);
    val = ini["options"][option];
}

void configFileWrite(std::string option, std::string newValue) {
    mINI::INIFile file("luz_app_options.ini");
    mINI::INIStructure ini;
    file.read(ini);
    ini["options"][option] = newValue;
    file.write(ini);
}

//from: https://learn.microsoft.com/en-us/cpp/text/how-to-convert-between-various-string-types?view=msvc-170
wchar_t* to_wchar(const char* chars) {
    size_t newsize = strlen(chars) + 1;
    wchar_t* convertedString = new wchar_t[newsize];
    size_t convertedChars = 0;
    mbstowcs_s(&convertedChars, convertedString, newsize, chars, _TRUNCATE);
    return convertedString;
}

class LuzApp {

public:

    LuzApp() {
        setup();
    }

    //Create vector containing monitor handles and other info
    bool getMonitorInfo() {
        std::vector<HMONITOR>  hMonitors;
        EnumDisplayMonitors(NULL, NULL, MonitorEnumProc, (LPARAM)&hMonitors);
        DWORD nPhysMonitors = 0;

        for (auto& monitor : hMonitors) {

            if (!(GetNumberOfPhysicalMonitorsFromHMONITOR(monitor, &nPhysMonitors))) {
                return false;
            }
            LPPHYSICAL_MONITOR physMonitorArr = new PHYSICAL_MONITOR[nPhysMonitors];

            if (!(GetPhysicalMonitorsFromHMONITOR(monitor, nPhysMonitors, physMonitorArr))) {
                delete[]physMonitorArr;
                return false;
            }

            for (auto& monitor : std::span(physMonitorArr, nPhysMonitors)) {
                m_physMonitors.push_back(monitor);
            }

            delete[]physMonitorArr;
        }
        return true;
    }

    //ImGui Frame window content
    void windowContent(ImGuiWindowFlags windowFlags) {
        ImGui::Begin("Adjust Brightness", 0, windowFlags);
        ImGui::Checkbox("Auto", &m_autoBrightness);
        ImGui::InputText("Port", &m_displayedPortName);
        ImGui::SameLine();
        if (ImGui::Button("save")) {
            m_savedPortName = m_displayedPortName;
            configFileWrite("port", m_displayedPortName);
            if (m_autoBrightness) getPort(m_displayedPortName);
        }

        if (m_autoBrightness) {

            //Check if port is open
            if (m_hSerial == 0 && !getPort(m_savedPortName)) {         
                 m_autoBrightness = false;               
            }
            if (!handleAutoBrightness()) {
                m_autoBrightness = false;
            }

            //if auto brightness is true then sliders will be visible but not interactable
            ImGui::BeginDisabled();
            renderSliders();
            ImGui::EndDisabled();

        }

        else {
            if (m_hSerial) {
                closePortHandle();
            }
            renderSliders();
        }
        ImGui::End();
    }

    //Monitor brightness sliders 
    void renderSliders() {
        for (int i{ 0 }; i < m_physMonitors.size(); ++i) {
            brightnessSlider(i);
        }
    }


    void brightnessSlider(int monitorIndex) {
        std::string mon_t = "Monitor " + std::to_string(monitorIndex + 1);
        const char* monitorName = mon_t.c_str();
        ImGuiSliderFlags sliderFlags = ImGuiSliderFlags_NoInput;

        ImGui::SliderInt(monitorName, &m_monitors[monitorIndex].brightness, 0, 100, "%d", sliderFlags);
        if (!m_autoBrightness) //prevents double calling of setMonitorBrightness as setMonitorBrightness will be called in handleAutoBrightness function when autoBrightness is true
        {
            SetMonitorBrightness(m_physMonitors[monitorIndex].hPhysicalMonitor, m_monitors[monitorIndex].brightness);
        }
    }

    //reads incoming lux values on serial port from arduino
    bool handleAutoBrightness() {
        const int bufferSize = 32;
        char buffer[bufferSize] = { };
        buffer[bufferSize - 1] = '\0';

        //read stream
        if (m_hSerial != 0 && !ReadFile(m_hSerial, buffer, bufferSize - 1, 0, NULL)) {
            return false;
        }

        //get latest valid reading -> readings are 9 charcters long and separated by a comma (,)
        std::string data = std::string(buffer, bufferSize);
        std::string delimiter = ",";
        std::string meas{ };
        size_t lastPos = 0;
        size_t nextPos = 0;
        while (((nextPos = data.find(delimiter, lastPos)) != std::string::npos) && (meas.length() != 9)) {
            nextPos = data.find(delimiter, lastPos);
            meas = data.substr(lastPos, nextPos - lastPos);
            lastPos = nextPos + 1;
        }
        double lux = std::stod(meas);

        //map lux value to screen brightness: https://learn.microsoft.com/en-us/windows/win32/sensorsapi/understanding-and-interpreting-lux-values
        DWORD newBrightness = abs(round((log10(lux) / 5) * 100));

        //apply changes to monitors
        for (int i{ 0 }; i < m_physMonitors.size(); ++i) {
            SetMonitorBrightness(m_physMonitors[i].hPhysicalMonitor, newBrightness);
            //update monitors array with screen brightness so it's value will be updated in the gui
            m_monitors[i].brightness = newBrightness;
        }

        return true;
    }

    //getting port that arduino is connected to
    bool getPort(std::string port) {
        if (m_hSerial) closePortHandle(); //close any existing port first
        const wchar_t* portName = to_wchar(port.c_str());

        m_hSerial = CreateFile(portName, GENERIC_READ | GENERIC_WRITE, 0, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);
        delete[]portName;
        if (m_hSerial == INVALID_HANDLE_VALUE)
        {
            //error with port name
            return false;
        }

        DCB serialParams = { 0 };
        serialParams.DCBlength = sizeof(serialParams);

        if (!GetCommState(m_hSerial, &serialParams)) {
            //error getting state
            return false;
        }

        serialParams.BaudRate = CBR_9600;
        serialParams.ByteSize = 8;
        serialParams.StopBits = ONESTOPBIT;
        serialParams.Parity = NOPARITY;

        if (!SetCommState(m_hSerial, &serialParams)) {
            //error setting serial port state
            return false;
        }
        return true;
    }


    //cleanup 
    void closeMonitors() {
        for (int i{ 0 }; i < m_physMonitors.size(); ++i) {
            DestroyPhysicalMonitor(m_physMonitors[i].hPhysicalMonitor);
        }
    }

    void closePortHandle() {
        CloseHandle(m_hSerial);
    }

private:
    HANDLE m_hSerial{};
    std::vector<PHYSICAL_MONITOR> m_physMonitors{};
    std::vector<Monitor> m_monitors{};
    std::string m_displayedPortName{};
    std::string m_savedPortName{};
    bool m_autoBrightness = false;

    void setup() {
        getMonitorInfo();
        for (int i{ 0 }; i < m_physMonitors.size(); ++i) {
            DWORD MinBrightness{};
            DWORD CurrBrightness{};
            DWORD MaxBrightness{};
            GetMonitorBrightness(m_physMonitors[i].hPhysicalMonitor, &MinBrightness, &CurrBrightness, &MaxBrightness);
            Monitor monitor{};
            monitor.brightness = CurrBrightness;
            m_monitors.push_back(monitor);
        }
        //Set port name from config file
        configFileRead("port", m_savedPortName);

        m_displayedPortName = m_savedPortName;
    }

};


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