
# Luz
Windows Desktop App to control monitor brightness using Win32 API and ImGui

![Luz Application](/screenshots/luz_app.png)

## Controlling Monitor Brightness
 - Monitor brightness is controlled using sliders

## Auto brightness
- BH1750 Light Sensor used, connected to Arduino Uno, sketch uploaded to Arduino found in /arduino 
- Default port is set to COM5 but this can be changed to the appropriate port by changing the input text and pressing save.
- If there is an error with creating a handle for the port/reading the data then auto brightness will toggle off. 
- luz_app_config.ini (mINI library used for ini file handling) stores port used for auto brightness control

## Libraries used

### C++
- [Dear ImGui](https://github.com/ocornut/imgui)
- [mINI](https://github.com/pulzed/mINI)
- [GLFW](https://www.glfw.org/)

### Arduino
- [BH1750](https://github.com/claws/BH1750)
