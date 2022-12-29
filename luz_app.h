#pragma once
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

void configFileRead(std::string option, std::string& val);
void configFileWrite(std::string option, std::string newValue);
wchar_t* to_wchar(const char* chars);

class LuzApp
{
public:
	LuzApp();
	bool getMonitorInfo();
	void windowContent(ImGuiWindowFlags windowFlags);
	void renderSliders();
	void brightnessSlider(int monitorIndex);
	bool handleAutoBrightness();
	bool getPort(std::string port);
	void closeMonitors();
	void closePortHandle();


private:
	HANDLE m_hSerial;
	std::vector<PHYSICAL_MONITOR> m_physMonitors;
	std::vector<Monitor> m_monitors;
	std::string m_displayedPortName;
	std::string m_savedPortName;

	bool m_autoBrightness;
};