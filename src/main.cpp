#include "map_edit.h"
#include "graph.h"
#include <new>

namespace {

GRAPH_INIT editorGraphInit = {
    "",
    {640, 800, 800, 1024, 1152, 1280, 1280, 1280, 1440, 1440, 1680, 1920},
    {480, 480, 600, 768, 720, 720, 960, 1024, 900, 960, 1050, 1080}, {16, 32},
    1, 0, 640, 480, 16, 1
};
}

int WINAPI WinMain(HINSTANCE instance, HINSTANCE previous, LPSTR commandLine, int show)
{
    Map = new(std::nothrow) MAP_EDIT(instance, previous, STRING(commandLine), show, &editorGraphInit);
    if (Map)
    {
        if (Map->IsInitSuccess())
            while (!Map->Tact()) {}
        delete Map;
    }
    return 0;
}
