#pragma once

#include <alloy/SwapChainSources.hpp>
#include <alloy/common/Macros.h>
#include <GLFW/glfw3.h>
#include <string>

class AppBase {


protected:
	GLFWwindow* window;
	unsigned initialWidth = 640, initialHeight = 480;

	AppBase(unsigned width, unsigned height, const std::string& wndName);

	float timeElapsedSec;

public:

	virtual ~AppBase();

	bool Run();

	void GetFramebufferSize(int& width, int& heigh);

protected:

	virtual void OnAppStart(
		alloy::SwapChainSource* swapChainSrc,
		unsigned surfaceWidth,
		unsigned surfaceHeight) = 0;

	virtual void OnAppExit() = 0;

	//true: continue, false: exit
	virtual bool OnAppUpdate(float deltaSec) = 0;

};
