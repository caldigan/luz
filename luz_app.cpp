#include "luz_app.h"

//callback function requeired for EnumDisplayMonitors
BOOL CALLBACK MonitorEnumProc(HMONITOR hMonitor, HDC hdcMonitor, LPRECT lprcMonitor, LPARAM dwData)
{
	std::vector<HMONITOR>* data = reinterpret_cast<std::vector<HMONITOR>*>(dwData);
	data->push_back(hMonitor);
	return TRUE;
}

//luzApp initialise
LuzApp::LuzApp() {
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
	configFileRead("portName", m_savedPortName);

	m_displayedPortName = m_savedPortName;

	//Set auto brightness checkbox state
	m_autoBrightness = false;
}

//Create vector containing monitor handles and other info
bool LuzApp::getMonitorInfo() {
	std::vector<HMONITOR>  hMonitors;
	EnumDisplayMonitors(NULL, NULL, MonitorEnumProc, (LPARAM)&hMonitors);
	DWORD nPhysMonitors = 0;

	for (auto& monitor : hMonitors) {

		if (!(GetNumberOfPhysicalMonitorsFromHMONITOR(monitor, &nPhysMonitors))) {
			return false;
		}
		LPPHYSICAL_MONITOR physMonitorArr = new PHYSICAL_MONITOR[nPhysMonitors];
		if (!(GetPhysicalMonitorsFromHMONITOR(monitor, nPhysMonitors, physMonitorArr))) {
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
void LuzApp::windowContent(ImGuiWindowFlags windowFlags) {
	ImGui::Begin("Adjust Brightness", 0, windowFlags);
	ImGui::Checkbox("Auto", &m_autoBrightness);
	ImGui::InputText("Port", &m_displayedPortName);
	ImGui::SameLine();
	if (ImGui::Button("save")) {
		m_savedPortName = m_displayedPortName;
		configFileWrite("portName", m_displayedPortName);
		if (m_autoBrightness) getPort(m_displayedPortName);
	}

	if (m_autoBrightness) {

		//Check if port is open
		if (m_hSerial == 0){		
			 
			if (!getPort(m_savedPortName)) {
				m_autoBrightness = false;
			}
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
		if (m_hSerial){
			closePortHandle();
		}
		renderSliders();

	}

	ImGui::End();
}


//Monitor brightness sliders 
void LuzApp::renderSliders() {
	for (int i{ 0 }; i < m_physMonitors.size(); ++i) {
		brightnessSlider(i);
	}
}

void LuzApp::brightnessSlider(int monitorIndex) {
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
bool LuzApp::handleAutoBrightness() {

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
bool LuzApp::getPort(std::string port) {
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
void LuzApp::closeMonitors() {
	for (int i{ 0 }; i < m_physMonitors.size(); ++i) {
		DestroyPhysicalMonitor(m_physMonitors[i].hPhysicalMonitor);
	}
}

void LuzApp::closePortHandle() {
	CloseHandle(m_hSerial);
}