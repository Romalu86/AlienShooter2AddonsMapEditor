#include "map_edit.h"
#include "graph.h"
#include "engine_globals.h"

namespace {

int defaultScriptLoadState = 1;
}

void MAP_EDIT::Load(STRING name)
{
    undo.Reset();
    MAP::Load(name);
    lastSaveTime = ::RealCurrentTime;
    if (options.optGammaOff)
    {

        savedGamma = Graph->gamma;
        Graph->SetGamma(GAMMA());
    }
    SetScrollBox(VECTOR2(-450, -450), SizeXY() + VECTOR2(450, 450));
    defaultScriptLoadState = 0;
    m_script.Load(STRING("maps\\default.lgc"));
    m_script.CallFunction(-1, "iii", 0, 0, 0);
    m_script.Release();
    editFileName = m_currentMapFileName;
    DrawMapName();
    Mouse->Enable();
}
