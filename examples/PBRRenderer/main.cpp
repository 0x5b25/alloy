
#include "IApp.hpp"
#include "PBRRendererApp.hpp"

#include <iostream>

#include <imgui.h>


int main() {
    auto runner = IAppRunner::Create(800, 600, "PBR Renderer");
    auto app = new PBRRendererApp(runner);
    auto res = runner->Run(app);
    delete app;
    delete runner;
    return res;
}
