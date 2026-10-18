#include <cstdio>
#include <new>
#include <cstring>
#include "railway.h"
#include "texture.h"
#include "engine_globals.h"

#include "myerror.h"
#include "mylib.h"
#include "profile.h"
#include "script.h"
#include "stream.h"
#include "basetype.h"
#include "sprite_collector.h"
#include "sprite.h"
#include "builded_terrain.h"
#include "frame.h"
#include "sprite_list.h"
#include "mouse.h"
#include "mousetips.h"

#include "unit.h"
#include "engine.h"
#include "railway.h"
#include "plane.h"
#include "cannon.h"
#include "building.h"
#include "terrain.h"
#include "man.h"
#include "map.h"
#include <float.h>
#include "groups.h"
#include "input.h"
#include "net.h"
#include "resread.h"
#include "constant.h"
#include "sound.h"
#include "vid.h"
#include "region.h"
#include "rail.h"
#include "depo.h"
#include "robot.h"
#include "balloon.h"
#include "creature.h"
#include <math.h>

int EvFunctionNumber[64] = {};

namespace
{

STRING mapLoadingText;
ANGLE mapLoadingDirection(0);
VID* mapLoadingVid = 0;

const char* mapEventNames[] =
{
    "main", "TrainNotAmmo", "TrainNotPower", "TrainDamage", "TrainCreated",
    "TrainSplit", "TrainDestroy", "TrainDestroyPower", "TrainArrive",
    "???TrainNotArrive", "TrainAttacked", "DepoDestroy", "DepoBirth",
    "DepoAttacked", "DepoFree", "BuildingCapture", "MasterDestroy", "???",
    "???SuperWeaponWounded", "MineBlast", "MineRemove", "EnemyLinked",
    "TrainClash", "UnitCreated", "UnitDestroy", &StringGlobals::EmptyTextCharacter
};

}

struct MapOpenFileName
{
    unsigned int size;
    unsigned int ownerWindow;
    void* instance;
    const char* filter;
    char* customFilter;
    unsigned int maximumCustomFilter;
    unsigned int filterIndex;
    char* file;
    unsigned int maximumFile;
    char* fileTitle;
    unsigned int maximumFileTitle;
    const char* initialDirectory;
    const char* title;
    unsigned int flags;
    unsigned short fileOffset;
    unsigned short extensionOffset;
    const char* defaultExtension;
    unsigned int customData;
    unsigned int hook;
    const char* templateName;
};

#include <stdlib.h>
#include <new>
#include "graph.h"
#include "registry.h"
#include "dialogSrv.h"
#include "player_arcade.h"
#include "player.h"
#include "zm/Input/zInputMap.h"
#include "menu.h"
#  define INPUT WIN32_INPUT
#include <windows.h>
#include <mmsystem.h>
#include <commdlg.h>
#  undef INPUT
#include "runtime/win32_callbacks.h"

static void __cdecl MapPresentationCallback(int mode);

unsigned long PrevRealCurrentTime = 0;
unsigned long RealCurrentTime = 0;
unsigned long PrevCurrentTime = 0;
unsigned long CurrentTime = 0;

static unsigned long pauseStartTick = 0;
static unsigned long demoRealTick = 0;
static unsigned long demoSimulationTick = 0;
static unsigned long frameRateSampleTick = 0;

void MAP::DrawSecondaryInfo()
{
    if ((m_flags&FLAG_DRAW_FPS) != 0u)
    {
        GRAPH* const graph = Graph;
        graph->PrintfXY(graph->ViewMin().x, graph->ViewMin().y+1.0f, "%i", static_cast<int>(m_frameRate04));
    }
    if (Const->DebugMode == 0)
        return;
    SPRITE_COLLECTOR* collector;
    if ((m_flags&FLAG_DRAW_DEBUG_INFO) != 0u)
    {
        const int vertexTexture = static_cast<int>(VID::useTextureMemory);
        const float soundY = Graph->ViewMin().y;
        const float soundX = Graph->ViewMax().x;
        SOUND* const sound = ::Sound;
        const unsigned int activeSound = static_cast<unsigned int>(sound->GetNoPlayed());
        Graph->PrintfXY(soundX-20.0f, soundY+1.0f, "%2i", activeSound);
        const int textureTime = static_cast<int>(TEXTURE::memoryInUse)/1000000;
        GRAPH* graph = Graph;
        graph->PrintfXY(graph->ViewMin().x, graph->ViewMin().y+20.0f, "%i ver/tex %i/%i,%i(%i) obj/ver/tri %u/%u/%u", ::NoBuildAlpha, VID::useVertexMemory, vertexTexture/1000000, vertexTexture%1000000, textureTime, VID_SURFACE::noObj, MESH::noVertex, MESH::noTri);
        graph = Graph;
        const int graphTime = static_cast<int>(graph->currentTime);
        const unsigned int shared = VID_SURFACE::noDrawCall[7];
        graph->PrintfXY(graph->ViewMin().x, graph->ViewMin().y+30.0f, "draw[0] %i/%i draw[1] %i/%i draw[2] %i/%i  draw[3] %i/%i  draw[4] %i/%i sLight %i,%i", VID_SURFACE::noDrawCall[0], VID_SURFACE::noDrawReal[0], VID_SURFACE::noDrawCall[1], VID_SURFACE::noDrawReal[1], VID_SURFACE::noDrawCall[2], VID_SURFACE::noDrawReal[2], VID_SURFACE::noDrawCall[3], VID_SURFACE::noDrawReal[3], VID_SURFACE::noDrawCall[4]+shared, VID_SURFACE::noDrawReal[4]+shared, graphTime/1000000, graphTime%1000000);
        const VECTOR2 entriesPoint = Graph->ViewMin();
        collector = ::Sprites;
        const int freeGround = collector->GetNoFreeBuildedGround();
        collector = ::Sprites;
        const int debugEntries = collector->GetNoDrawedBuildedGround();
        Graph->PrintfXY(entriesPoint.x, entriesPoint.y+50.0f, "%i %i   draw[0,1] %i, %i", debugEntries, freeGround, 0, 0);
    }
    if ((m_flags&FLAG_DRAW_GROUND) != 0u && m_groundPrimary && m_groundHeight>1)
    {
        for (int row = 1;row<m_groundHeight;++row)
        {
            GRAPH* graph = Graph;
            for (int column = 1;column<m_groundWidth;++column)
            {
                const short* const first = reinterpret_cast<const short*>(m_groundPrimary);
                const short* const second = reinterpret_cast<const short*>(m_groundSecondary);
                const int index = row*m_groundWidth+column;
                const int z = first[index]>second[index]?first[index]:second[index];
                const int previousZ = first[index-1]>second[index-1]?first[index-1]:second[index-1];
                const VECTOR2 current(static_cast<float>(column*8+4)-m_shiftXY.x, static_cast<float>(row*8-z+4)-m_shiftXY.y);
                const VECTOR2 previous(static_cast<float>(column*8-4)-m_shiftXY.x, static_cast<float>(row*8-previousZ+4)-m_shiftXY.y);
                const bool currentInside = current.x >= graph->ViewMin().x && current.x<graph->ViewMax().x && current.y >= graph->ViewMin().y && current.y<graph->ViewMax().y;
                const bool previousInside = previous.x >= graph->ViewMin().x && previous.x<graph->ViewMax().x && previous.y >= graph->ViewMin().y && previous.y<graph->ViewMax().y;
                if (currentInside || previousInside)
                {
                    COLOR color;
                    color.value = GRAPH::GRAY.value;
                    Graph->Line(current, previous, color);
                    graph = Graph;
                }
            }
        }
    }
    collector = ::Sprites;
    if ((m_flags&FLAG_DRAW_SPRITE_BOUNDS) != 0u)
    {
        int index = collector->allSprites.m_count-1;
        for (;;)
        {
            SPRITE* sprite = 0;
            while (index >= 0 && index<collector->allSprites.m_count)
            {
                sprite = collector->allSprites.m_data[index];
                if (sprite)
                    break;
                --index;
            }
            if (!sprite)
                break;
            sprite->DrawRectangle();
            collector = ::Sprites;
            --index;
        }
    }
    if ((m_flags&FLAG_DRAW_SPRITE_INFO) != 0u)
    {
        SPRITE* debugSprite = m_menu.SelectedSprite();
        if (!debugSprite)
        {
            const VECTOR2& pickPoint = m_input.MouseXY();
            debugSprite = collector->GetSpriteScr(0x00800000, pickPoint, 0);
        }
        if (debugSprite)
        {
            debugSprite->DrawSecondaryInfo();
        }
        else if (Flagman(m_playerIndex))
        {
            Flagman(m_playerIndex)->DrawSecondaryInfo();
            for (int oy = -17;oy<17;++oy)
            {
                for (int ox = -17;ox<17;++ox)
                {
                    const int centerX = TruncateFloatToInt32(Flagman(m_playerIndex)->X())/8;
                    const int centerY = TruncateFloatToInt32(Flagman(m_playerIndex)->Y())/8;
                    const int gx = centerX+ox;
                    const int gy = centerY+oy;
                    if (gx<0 || gx >= m_groundWidth || gy<0 || gy >= m_groundHeight)
                        continue;
                    const short* const first = reinterpret_cast<const short*>(m_groundPrimary);
                    const short* const second = reinterpret_cast<const short*>(m_groundSecondary);
                    const int index = gy*m_groundWidth+gx;
                    int shade = first[index]>second[index]?first[index]:second[index];
                    shade /= 2;
                    if (shade<0)
                        shade = 0;
                    else if (shade>255)
                        shade = 255;
                    const unsigned int channel = static_cast<unsigned int>(shade);
                    const unsigned int color = 0xFF000000u|(channel<<16u)|(channel<<8u)|channel;
                    const VECTOR2 drawPoint = (Graph->ViewMax()-VECTOR2(100, 50))+VECTOR2(ox, oy)*2.0f;
                    COLOR markerColor;
                    markerColor.value = color;
                    Graph->PutBigPixel(drawPoint, markerColor);
                }
            }
        }
    }
    if ((m_flags&FLAG_DRAW_GROUPS) != 0u)
        m_groups.DrawNumber();
    if ((m_flags&FLAG_DRAW_RAILS) != 0u)
        RailMap.DebugDraw();
}

void MAP::DeletePointerToSprite(SPRITE* sprite)
{
    m_mouseTips.DeletePointerToSprite(sprite);
    if (m_players[0] != 0)
        for (unsigned int i = 0u;i<8u;++i) m_players[i]->DeletePointerToSprite(sprite);
    m_script.DeletePointerToObject(sprite);
    m_groups.DeletePointerToSprite(sprite);
}

int MAP::WorkWndMessage(HWND__* hwnd, unsigned long msg, unsigned long wParam, unsigned long lParam)
{
    INPUT* const input = &m_input;
    if ((static_cast<unsigned char>(m_flags)&FLAG_ACTIVE) != 0u && input->WorkWndMessage(hwnd, msg, wParam, lParam) != 0)
        return 1;
    switch (msg)
    {
        case WM_PAINT:
            ::Error->Log("    WM_PAINT");
        break;
        case WM_ACTIVATEAPP:
        {
            m_flags = (m_flags&~FLAG_ACTIVE)|(wParam != 0u?FLAG_ACTIVE:0u);
            if (::Sound != 0 && Mouse != 0)
            {
                if ((m_flags&FLAG_ACTIVE) != 0u)
                {
                    input->ResumeFFB();
                    ::Sound->Resume();
                    Mouse->Enable();
                }
                else
                {
                    input->PauseFFB();
                    ::Sound->Pause();
                    Mouse->Disable();
                }
            }
            break;
        }
        case WM_DESTROY:
            if ((Graph->capabilityFlags&0x40u) == 0u)
        {
            RECT rect;
            (void)GetWindowRect(reinterpret_cast<HWND>(U32ToPtr(m_dialog)), &rect);
            {
                const STRING xName("WindowPositionX");
                ::Registry->SetInt(xName, rect.left);
            }
            {
                const STRING yName("WindowPositionY");
                ::Registry->SetInt(yName, rect.top);
            }
        }
        m_dialog = 0u;
        PostQuitMessage(0);
        break;
        case WM_SYSCOMMAND:
            if ((wParam == SC_SIZE || wParam == SC_MOVE || wParam == SC_MAXIMIZE || wParam == SC_MONITORPOWER) && (Graph->capabilityFlags&0x40u) != 0u) return 1;
        break;
        case WM_ENTERMENULOOP:
            case WM_ENTERSIZEMOVE: Graph->BeginPause();
        ::Sound->Pause();
        break;
        case WM_EXITMENULOOP:
            case WM_EXITSIZEMOVE: Graph->EndPause();
        ::Sound->Resume();
        break;
        default:
            break;
    }
    return 0;
}

VECTOR2 MAP::ToScreen(const VECTOR2& value) const
{
    return value - m_shiftXY;
}

VECTOR2 MAP::FromScreen(const VECTOR2& value) const
{
    return VECTOR2(m_shiftXY.x+value.x, m_shiftXY.y+value.y);
}

VECTOR2 MAP::ToScreenScaled(const VECTOR2& value) const
{
    GRAPH* const graph = Graph;
    const ANGLE direction = graph->CameraDirection();
    if ((graph->sceneState&2u) == 0u && direction.value == 0u)
        return VECTOR2(value.x-m_shiftXY.x, value.y-m_shiftXY.y);
    if (direction.value != 0u)
    {
        const VECTOR2 center((graph->viewLeft+graph->viewRight)*0.5f,
                             (graph->viewTop+graph->viewBottom)*0.5f);
        const VECTOR2 relative((value.x-graph->viewportOffsetX)*graph->screenScaleX-
                                   m_shiftXY.x*graph->screenScaleX-center.x,
                               (value.y-graph->viewportOffsetY)*graph->screenScaleY-
                                   m_shiftXY.y*graph->screenScaleY-center.y);
        return direction.RotateY(relative)+center;
    }
    const float offsetX = static_cast<float>(TruncateFloatToInt32(m_shiftXY.x*graph->screenScaleX));
    const float offsetY = static_cast<float>(TruncateFloatToInt32(m_shiftXY.y*graph->screenScaleY));
    return VECTOR2((value.x-graph->viewportOffsetX)*graph->screenScaleX-offsetX, (value.y-graph->viewportOffsetY)*graph->screenScaleY-offsetY);
}

void MAP::SetShiftCoor(const VECTOR2& point, int mode)
{
    GRAPH* graph = Graph;
    if (mode == 2)
    {
        const unsigned int x = static_cast<unsigned int>(TruncateFloatToInt32(point.x));
        const unsigned int y = static_cast<unsigned int>(TruncateFloatToInt32(point.y));
        graph->Effect(2, static_cast<int>(x), static_cast<int>(y), -1);
        return;
    }
    const bool absoluteProjection = (static_cast<unsigned int>(mode)&0x10000000u) != 0u;
    float x = point.x-graph->screenWidth*0.5f;
    float y = point.y-graph->screenHeight*0.5f;
    if (!absoluteProjection)
    {
        x += graph->viewportOffsetX;
        y += graph->viewportOffsetY;
        const float minX = m_cameraMinX-graph->viewLeft;
        const float minY = m_cameraMinY-graph->viewTop;
        if (x<minX)
            x = minX;
        if (y<minY)
            y = minY;
        x -= graph->viewportOffsetX*2.0f;
        y -= graph->viewportOffsetY*2.0f;
        const float maxX = m_cameraMaxX-graph->viewRight;
        const float maxY = m_cameraMaxY-graph->viewBottom;
        if (x>maxX)
            x = maxX;
        if (y>maxY)
            y = maxY;
        x += graph->viewportOffsetX;
        y += graph->viewportOffsetY;
    }
    x = static_cast<float>(TruncateFloatToInt32(x));
    y = static_cast<float>(TruncateFloatToInt32(y));
    if ((m_shiftXY.x != x || m_shiftXY.y != y) && !absoluteProjection)
    {
        const float dx = x-m_shiftXY.x;
        const float dy = y-m_shiftXY.y;
        for (int i = 0;i<m_menu.m_count;++i)
        {
            SPRITE* const sprite = m_menu.m_data[i];
            const VECTOR translated(sprite->X()+dx, sprite->Y()+dy, sprite->Z());
            sprite->ChangeCoor(translated);
        }
        if (Mouse)
        {
            const VECTOR translated(Mouse->X()+dx, Mouse->Y()+dy, Mouse->Z());
            Mouse->ChangeCoor(translated);
        }
    }
    m_shiftXY.x = x;
    m_shiftXY.y = y;
    graph = Graph;
    graph->SetViewMatrix(graph->CameraDirection(), VECTOR2(graph->screenScaleX, graph->screenScaleY));
}

void MAP::SetScrollType(int mode, const VECTOR2& first, const VECTOR2& second)
{
    m_scrollMode = static_cast<unsigned int>(mode);
    if (FloatBitsU32(first.x) == FloatBitsU32(-1000.0f) && FloatBitsU32(first.y) == FloatBitsU32(-1000.0f))
    {
        m_scrollMouseMinX = Graph->screenWidth-(640.0f);
        m_scrollMouseMinY = Graph->screenHeight-(480.0f);
        m_scrollMouseMaxX = 640.0f;
        m_scrollMouseMaxY = 480.0f;
    }
    else
    {
        m_scrollMouseMinX = first.x;
        m_scrollMouseMinY = first.y;
        m_scrollMouseMaxX = second.x;
        m_scrollMouseMaxY = second.y;
    }
}

void MAP::ControlShiftCoor()
{
    const unsigned int flags = m_scrollMode;
    INPUT* const input = &m_input;
    const float mouseX = input->m_clientMouseXY.x;
    const float mouseY = input->m_clientMouseXY.y;
    GRAPH* const graph = Graph;
    const float edge = 5.0f;
    const float xStep = 0.04f;
    const float yStep = 0.03f;
    const float half = 0.5f;
    const float velocityScale = 0.001f;
    float velocityX = m_velocityX;
    float velocityY = m_velocityY;
    if ((flags&0x21u) == 0u)
    {
        velocityX = 0.0f;
        velocityY = 0.0f;
    }
    else
    {
        const float maxX = Const->MaxScrollSpeedX;
        if (mouseX <= edge && (flags&1u) != 0u)
        {
            if (velocityX>-maxX)
                velocityX -= xStep;
        }
        else if (mouseX >= graph->screenWidth-edge && (flags&1u) != 0u)
        {
            if (velocityX<maxX)
                velocityX += xStep;
        }
        else if (input->GetAction(3u) != 0 && (flags&0x20u) != 0u)
        {
            if (velocityX>-maxX)
                velocityX -= xStep;
        }
        else if (input->GetAction(4u) != 0 && (flags&0x20u) != 0u)
        {
            if (velocityX<maxX)
                velocityX += xStep;
        }
        else velocityX = 0.0f;
        const float maxY = Const->MaxScrollSpeedY;
        if (mouseY <= edge && (flags&1u) != 0u)
        {
            if (velocityY>-maxY)
                velocityY -= yStep;
        }
        else if (mouseY >= graph->screenHeight-edge && (flags&1u) != 0u)
        {
            if (velocityY<maxY)
                velocityY += yStep;
        }
        else if (input->GetAction(1u) != 0 && (flags&0x20u) != 0u)
        {
            if (velocityY>-maxY)
                velocityY -= yStep;
        }
        else if (input->GetAction(2u) != 0 && (flags&0x20u) != 0u)
        {
            if (velocityY<maxY)
                velocityY += yStep;
        }
        else velocityY = 0.0f;
    }
    m_velocityX = velocityX;
    m_velocityY = velocityY;
    const int playerIndex = static_cast<int>(m_playerIndex);
    SPRITE* const player = Flagman(static_cast<unsigned int>(playerIndex));
    if (player && (flags&4u) != 0u)
    {
        const VECTOR2 playerScreen = player->Coor().ViewCoor();
        const VECTOR2 mapShift(Map->m_shiftXY);
        const VECTOR2 relative = playerScreen-mapShift;
        const float scale = m_scrollFollowSpeedScale*velocityScale;
        velocityX = (relative.x-graph->screenWidth*half)*scale;
        velocityY = (relative.y-graph->screenHeight*half)*scale;
        m_velocityX = velocityX;
        m_velocityY = velocityY;
    }
    else if (player && (flags&8u) != 0u && velocityX == 0.0f && velocityY == 0.0f)
    {
        float targetMouseX = mouseX;
        if (targetMouseX<m_scrollMouseMinX)
            targetMouseX = m_scrollMouseMinX;
        else if (targetMouseX>m_scrollMouseMaxX)
            targetMouseX = m_scrollMouseMaxX;
        float targetMouseY = mouseY;
        if (targetMouseY<m_scrollMouseMinY)
            targetMouseY = m_scrollMouseMinY;
        else if (targetMouseY>m_scrollMouseMaxY)
            targetMouseY = m_scrollMouseMaxY;
        float speedX = 0.0f;
        float speedY = 0.0f;
        VID* const playerVid = player->Vid();
        if (playerVid->spriteClass == 7u)
        {
            speedX = 0.0f;
            speedY = 0.0f;
        }
        const VECTOR2 playerScreen = player->Coor().ViewCoor();
        const VECTOR2 mapShift(Map->m_shiftXY);
        const VECTOR2 relative = playerScreen-mapShift;
        const float targetX = (relative.x+targetMouseX)*half;
        const float targetY = (relative.y+targetMouseY)*half;
        float lead = static_cast<float>(sqrt(static_cast<double>(speedX*speedX+speedY*speedY)))*0.033333335f;
        if (lead<4.0f)
            lead = 4.0f;
        else if (lead>20.0f)
            lead = 20.0f;
        const float centerX = (m_scrollMouseMinX+m_scrollMouseMaxX)*half;
        const float centerY = (m_scrollMouseMinY+m_scrollMouseMaxY)*half;
        const float scale = m_scrollFollowSpeedScale*velocityScale;
        velocityX = (targetX-centerX)*lead*scale;
        velocityY = (targetY-centerY)*lead*scale;
        m_velocityX = velocityX;
        m_velocityY = velocityY;
    }
    else if (player && (flags&0x10u) != 0u && velocityX == 0.0f && velocityY == 0.0f)
    {
        const VECTOR2 playerScreen = player->Coor().ViewCoor();
        const VECTOR2 target(m_shiftXY.x+playerScreen.x-Map->m_shiftXY.x, m_shiftXY.y+playerScreen.y-Map->m_shiftXY.y);
        SetShiftCoor(target, 0);
        return;
    }
    if ((flags&0x40u) == 0u)
    {
        const ANGLE direction = graph->CameraDirection();
        const float cosine = direction.Cos();
        const float sine = direction.Sin();
        const float rotatedX = cosine*velocityX-sine*velocityY;
        velocityY = cosine*velocityY+sine*velocityX;
        velocityX = rotatedX;
    }
    const unsigned int elapsed = ::CurrentTime-::PrevCurrentTime;
    const VECTOR2 target(graph->screenWidth*half+m_shiftXY.x+static_cast<float>(elapsed)*velocityX, graph->screenHeight*half+m_shiftXY.y+static_cast<float>(elapsed)*velocityY);
    SetShiftCoor(target, 0);
}

SPRITE* MAP::Flagman(unsigned int index) const
{
    PLAYER* const entry = m_players[index&7u];
    return entry->m_flagman.m_value;
}

SPRITE* MAP::SpriteUnderCursor() const
{
    const unsigned int index = m_playerIndex&7u;
    PLAYER* const entry = m_players[index];
    return entry->m_transientSprite.m_value;
}

int MAP::DemoTact()
{
    RESOURCE* const stream = &m_resource;
    INPUT* const input = &m_input;
    unsigned int flags = m_flags;
    if ((flags&FLAG_DEMO_PLAYBACK) != 0u)
    {
        int demoTime;
        (void)stream->Read(&demoTime, 4u);
        if (demoTime == -1 || input->GetAction(IA_ANY_KEY) != 0)
            goto playback_finished;
        (void)stream->Read(&m_frameRate04, 4u);
        input->Load(stream);
        unsigned int recordedRandom;
        (void)stream->Read(&recordedRandom, 4u);
        const unsigned int liveRandom = static_cast<unsigned int>(Random(static_cast<int>(50000u)));
        if (recordedRandom != liveRandom)
        {
            m_flags |= FLAG_DEMO_SYNC_ERROR;
            STRING subject("Demo synchro error");
            const unsigned int errorTick = ::CurrentTime;
            if (::Error != 0)
            {
                STRING owner("MAP");
                ::Error->Error(owner, E_ERROR, subject, static_cast<int>(errorTick));
            }
        }
        unsigned int previousDemoTime = demoSimulationTick;
        if (static_cast<unsigned int>(demoTime)-previousDemoTime>71u)
        {
            previousDemoTime = static_cast<unsigned int>(demoTime)-71u;
            demoSimulationTick = previousDemoTime;
        }
        const unsigned int targetTime = static_cast<unsigned int>(demoTime);
        unsigned int now = timeGetTime();
        unsigned int elapsedReal = now-demoRealTick;
        unsigned int elapsedDemo = targetTime-demoSimulationTick;
        if (elapsedDemo>elapsedReal)
        {
            now = timeGetTime();
            elapsedReal = now-demoRealTick;
            elapsedDemo = targetTime-demoSimulationTick;
            if (elapsedDemo >= elapsedReal)
            {
                do
                {
                    now = timeGetTime();
                    elapsedReal = now-demoRealTick;
                    elapsedDemo = targetTime-demoSimulationTick;
                }
                while (elapsedDemo >= elapsedReal);
            }
        }
        ::CurrentTime = targetTime;
        demoSimulationTick = targetTime;
        demoRealTick = timeGetTime();
    }
    flags = m_flags;
    if ((flags&FLAG_DEMO_RECORDING) != 0u)
    {
        (void)stream->Write(&::CurrentTime, 4u);
        (void)stream->Write(&m_frameRate04, 4u);
        input->Save(stream);
        unsigned int randomValue = static_cast<unsigned int>(Random(static_cast<int>(50000u)));
        (void)stream->Write(&randomValue, 4u);
    }
    return 1;
    playback_finished:
    {
        STRING name;
        (void)stream->GoNext(ResourceType::Demo);
        stream->m_ioState = RESOURCE::IO_SEEK;
        (void)std::fseek(stream->m_file, BitsToSigned32(stream->m_recordSize), SEEK_CUR);
        name.Read(stream);
        const char* text = name.m_value;
        const char* expected = StringGlobals::EmptyText();
        const char* mapNameCursor = text;
        const char* emptyNameCursor = expected;
        while (*mapNameCursor == *emptyNameCursor && *mapNameCursor)
        {
            ++mapNameCursor;
            ++emptyNameCursor;
        }
        if (static_cast<unsigned char>(*mapNameCursor) == static_cast<unsigned char>(*emptyNameCursor))
            name = "maps\\logo.map";
        stream->Close();
        Mouse->Enable();
        unsigned int newFlags = m_flags;
        newFlags &= ~FLAG_DEMO_PLAYBACK;
        newFlags |= FLAG_LOAD_REQUESTED;
        m_flags = newFlags;
        m_pendingMapFileName = name;
        input->Tact();
    }
    return 999999u;
}

void MAP::SetFlagman(unsigned int index, SPRITE* sprite) const
{
    m_players[index&7u]->SetFlagman(sprite);
}

int MAP::Tact()
{
    return 0;
}

int MAP::ValidateVid(int index) const
{
    return index >= 0 && index < m_vidCount && m_vids[static_cast<unsigned int>(index)] != 0;
}

VID* MAP::Vid(int index) const
{
    return ValidateVid(index) ? m_vids[index] : ::EmptyVid;
}

PLAYER* MAP::Player() const
{
    return m_players[m_playerIndex];
}

PLAYER* MAP::Player(int index) const
{
    return m_players[static_cast<unsigned int>(index) & 7u];
}

float MAP::ToScreenX(float value) const
{
    return value - m_shiftXY.x;
}

float MAP::ToScreenY(float value) const
{
    return value - m_shiftXY.y;
}

float MAP::ToScreenY(float value, float height) const
{
    return static_cast<float>((static_cast<double>(value) - height) - m_shiftXY.y);
}

const VECTOR2& MAP::SizeXY() const
{
    return m_sizeXY;
}

int MAP::InMap(const VECTOR2& point) const
{
    return point >= VECTOR2(0, 0) && point < SizeXY();
}

void MAP::Error(TYPE_ERROR type, const STRING& subject, unsigned long code)
{
    if (::Error)
        ::Error->Error(STRING("MAP"), type, subject, code);
}

int MAP::IsNetGameStart() const
{
    const NET* const net = reinterpret_cast<const NET*>(::Net);
    return net && (net->m_flags&0x10u) ? 1 : 0;
}

void MAP::SetupProcessorFlags()
{
    (void)_controlfp(0x00020000u, 0x00030000u);
}

int MAP::ScriptRun(int function, const SPRITE* spriteA, const SPRITE* spriteB, int integerArgument)
{
    if (m_flags&FLAG_SCRIPT_DISABLED)
        return 0;
    SCRIPT* const scriptRuntime = &m_script;
    return scriptRuntime->CallFunction(function, "ppi", static_cast<int>(PtrToU32(spriteA)), static_cast<int>(PtrToU32(spriteB)), integerArgument);
}

int MAP::ScriptEventRun(int eventId, const SPRITE* sprite, int integerArgument1, int integerArgument2)
{
    if (m_flags&FLAG_SCRIPT_DISABLED)
        return 0;
    SCRIPT* const scriptRuntime = &m_script;
    return scriptRuntime->CallScriptEventFunction(eventId, "pii", static_cast<int>(PtrToU32(sprite)), integerArgument1, integerArgument2);
}

int MAP::ScriptRun(int function, const char* script, int scriptArgument1, int scriptArgument2, int scriptArgument3)
{
    if (m_flags&FLAG_SCRIPT_DISABLED)
        return 0;
    SCRIPT* const scriptRuntime = &m_script;
    return scriptRuntime->CallFunction(function, script, scriptArgument1, scriptArgument2, scriptArgument3);
}

int MAP::ScriptRun(const STRING& script, const char* name, int scriptArgument1, int scriptArgument2, int scriptArgument3)
{
    if (m_flags&FLAG_SCRIPT_DISABLED)
        return 0;
    SCRIPT* const scriptRuntime = &m_script;
    return scriptRuntime->CallFunction(script, name, scriptArgument1, scriptArgument2, scriptArgument3);
}

void MAP::InvalidateDeviceObjects()
{
    for (int i = 0;i<m_vidCount;++i)
    {
        VID* const storedVid = m_vids[static_cast<unsigned int>(i)];
        if (storedVid)
            storedVid->InvalidateDeviceObjects();
    }
}

RELATION::~RELATION()
{
}

SPRITE* RELATION::Decode(SPRITE* sprite) const
{
    const int index = m_first.Location(sprite);
    return index >= 0 ? m_second.Item(index) : 0;
}

SPRITE* MAP::ReadPointer(STREAM* inputStream) const
{
    SPRITE* savedPointer = reinterpret_cast<SPRITE*>(inputStream);
    (void)inputStream->Read(&savedPointer, 4u);
    if (PtrToU32(savedPointer) == 0xFFFFFFFFu)
        return savedPointer;
    return m_relation.Decode(savedPointer);
}

void MAP::PauseOn()
{
    if ((m_flags&FLAG_PAUSED) == 0u)
    {
        pauseStartTick = ::CurrentTime;
        for (unsigned int i = 0;i<8u;++i)
        {
            PLAYER* const slot = m_players[i];
            slot->m_controlPauseState = 1u;
        }
    }
    m_flags |= FLAG_PAUSED;
}

void MAP::PauseOff()
{
    if ((m_flags&FLAG_PAUSED) != 0u)
    {
        SPRITE_COLLECTOR* spriteCollector = ::Sprites;
        const int spriteCount = spriteCollector->allSprites.m_count;
        int spriteIndex = static_cast<int>(static_cast<unsigned int>(spriteCount)-1u);
        SPRITE* candidate = 0;
        if (spriteIndex >= 0 && spriteIndex<spriteCount)
        {
            SPRITE* const* const spriteTable = spriteCollector->allSprites.m_data;
            while (spriteIndex >= 0)
            {
                SPRITE* const listedSprite = spriteTable[spriteIndex];
                if (listedSprite != 0)
                {
                    candidate = listedSprite;
                    break;
                }
                --spriteIndex;
            }
        }
        const unsigned int resumeTick = pauseStartTick;
        while (candidate != 0)
        {
            if (candidate->m_lastAnimationTick>resumeTick)
                candidate->m_lastAnimationTick = resumeTick;
            --spriteIndex;
            candidate = 0;
            if (spriteIndex >= 0)
            {
                spriteCollector = ::Sprites;
                const int currentCount = spriteCollector->allSprites.m_count;
                if (spriteIndex<currentCount)
                {
                    SPRITE* const* const spriteTable = spriteCollector->allSprites.m_data;
                    while (spriteIndex >= 0)
                    {
                        SPRITE* const listedSprite = spriteTable[spriteIndex];
                        if (listedSprite != 0)
                        {
                            candidate = listedSprite;
                            break;
                        }
                        --spriteIndex;
                    }
                }
            }
        }
        const unsigned int pausedTick = ::CurrentTime;
        Graph->SetTime(pausedTick, resumeTick);
        const unsigned long resumedTick = pauseStartTick;
        ::CurrentTime = resumedTick;
        ::PrevCurrentTime = resumedTick-10u;
        for (unsigned int i = 0;i<8u;++i)
        {
            PLAYER* const slot = m_players[i];
            slot->m_controlPauseState = 0u;
        }
    }
    m_flags &= ~FLAG_PAUSED;
}

void RELATION::Release()
{
    m_first.Release();
    m_second.Release();
}

RELATION::RELATION()
{
}

void MAP::SetGroundZ(const VECTOR& point)
{
    if (InMap(point.XY()))
    {
        const int index = TruncateFloatToInt32(point.x)/8+
            TruncateFloatToInt32(point.y/8.0f)*m_groundWidth;
        if (m_groundPrimary[index]<TruncateFloatToInt32(point.z))
            m_groundPrimary[index] = static_cast<short>(TruncateFloatToInt32(point.z));
    }
}

void MAP::SetTempGroundZ(const VECTOR& point)
{
    if (InMap(point.XY()))
    {
        const int index = TruncateFloatToInt32(point.x)/8+
            TruncateFloatToInt32(point.y/8.0f)*m_groundWidth;
        if (m_groundSecondary[index]<TruncateFloatToInt32(point.z))
            m_groundSecondary[index] = static_cast<short>(TruncateFloatToInt32(point.z));
    }
}

void MAP::ClearTempGroundZ(const VECTOR& point)
{
    if (InMap(point.XY()))
    {
        const int index = TruncateFloatToInt32(point.x)/8+
            TruncateFloatToInt32(point.y/8.0f)*m_groundWidth;
        if (m_groundSecondary[index] == TruncateFloatToInt32(point.z))
            m_groundSecondary[index] = 0;
    }
}

void MAP::RestoreDeviceObjects()
{
    for (int i = 0;i<m_vidCount;++i)
    {
        VID* const observer = m_vids[static_cast<unsigned int>(i)];
        if (observer)
            observer->RestoreDeviceObjects();
    }
    GRAPH* const graph = Graph;
    const float scale = (0.5f);
    const VECTOR2 point( m_shiftXY.x+graph->SizeXY().x*scale, m_shiftXY.y+graph->SizeXY().y*scale);
    SetShiftCoor(point, 0);
}

void RELATION::Insert(SPRITE* first, SPRITE* second)
{
    if (!first)
        return;
    m_first.Insert(first);
    m_second.Insert(second);
}

static int MapAbi_MapGridCoord(float value, float limit, int cells)
{
    if (value<0.0f || value != value)
        return 0;
    if (value >= limit)
        return cells-1;
    return TruncateFloatToInt32(value)/8;
}

int MAP::IsDotUnderGround(const VECTOR& point) const
{
    const int x = MapAbi_MapGridCoord(point.x, m_sizeXY.x, m_groundWidth);
    const int y = MapAbi_MapGridCoord(point.y, m_sizeXY.y, m_groundHeight);
    const int index = y*m_groundWidth+x;
    const short primaryHeight = reinterpret_cast<const short*>(m_groundPrimary)[index];
    if (static_cast<float>(primaryHeight)>point.z)
        return 1;
    const short secondaryHeight = reinterpret_cast<const short*>(m_groundSecondary)[index];
    return static_cast<float>(secondaryHeight)>point.z?1:0;
}

float MAP::GetGroundZ(const VECTOR2& point) const
{
    const int x = MapAbi_MapGridCoord(point.x, m_sizeXY.x, m_groundWidth);
    const int y = MapAbi_MapGridCoord(point.y, m_sizeXY.y, m_groundHeight);
    const int index = y*m_groundWidth+x;
    const short primaryHeight = reinterpret_cast<const short*>(m_groundPrimary)[index];
    const short secondaryHeight = reinterpret_cast<const short*>(m_groundSecondary)[index];
    return static_cast<float>(primaryHeight>secondaryHeight?primaryHeight:secondaryHeight);
}

void MAP::DeleteExtraVid()
{
    SPRITE_COLLECTOR* spriteCollector = ::Sprites;
    SPRITE* candidateSprite = spriteCollector->FirstAllSprite();
    while (candidateSprite != 0)
    {
        VID* candidateVid = candidateSprite->Vid();
        if ((candidateVid->storageFlags&VID::STORAGE_EMBEDDED) != 0u)
        {
            delete candidateSprite;
        }
        spriteCollector = ::Sprites;
        candidateSprite = spriteCollector->NextAllSprite();
    }
    int storedVidCount = m_vidCount;
    for (int i = storedVidCount-1;i >= 0;--i)
    {
        VID* storedVid = m_vids[static_cast<unsigned int>(i)];
        if (storedVid != 0 && (storedVid->storageFlags&VID::STORAGE_EMBEDDED) != 0u)
        {
            delete storedVid;
            m_vids[static_cast<unsigned int>(i)] = 0;
        }
    }
    while (m_vidCount>0)
    {
        const int trailingVidCount = m_vidCount;
        if (m_vids[static_cast<unsigned int>(trailingVidCount-1)] != 0)
            break;
        --m_vidCount;
    }
}

void MAP::ResetGroundZ()
{
    if (m_groundPrimary)
        ::operator delete(m_groundPrimary);
    if (m_groundSecondary)
        ::operator delete(m_groundSecondary);
    m_groundWidth = TruncateDoubleToInt32(static_cast<double>(m_sizeXY.x)+7.0)/8;
    m_groundHeight = TruncateDoubleToInt32(static_cast<double>(m_sizeXY.y)+7.0)/8;
    const unsigned int bytes = static_cast<unsigned int>(m_groundWidth)*static_cast<unsigned int>(m_groundHeight)*2u;
    m_groundPrimary = static_cast<short*>(::operator new(bytes, std::nothrow));
    m_groundSecondary = static_cast<short*>(::operator new(bytes, std::nothrow));
    std::memset(m_groundPrimary, 0, bytes);
    std::memset(m_groundSecondary, 0, bytes);
}

int MAP::IsInitSuccess() const
{
    return (m_flags&FLAG_INITIALIZED) != 0u;
}

void MAP::ExchangeVid(VID* firstVid, VID* secondVid)
{
    VID* first = firstVid;
    VID* second = secondVid;
    if (first == 0 || second == 0 || first == second)
        return;
    VID* globalObject = ::EmptyVid;
    if (first == globalObject || second == globalObject)
        return;
    const int count = m_vidCount;
    for (int i = 0;i<count;++i)
    {
        VID* storedVid = m_vids[static_cast<unsigned int>(i)];
        if (storedVid == 0)
            continue;
        if (storedVid->linkVid == first)
            storedVid->linkVid = second;
        else if (storedVid->linkVid == second)
            storedVid->linkVid = first;
        for (unsigned int child = 0;child<VID::AnimationSlotCount;++child)
        {
            if (storedVid->childVid[child] == first)
                storedVid->childVid[child] = second;
            else if (storedVid->childVid[child] == second)
                storedVid->childVid[child] = first;
        }
    }
    const unsigned int firstIndex = static_cast<unsigned int>(first->nVid);
    const unsigned int secondIndex = static_cast<unsigned int>(second->nVid);
    m_vids[firstIndex] = second;
    m_vids[secondIndex] = first;
    VID* vidValue = second->exchangedVid;
    second->exchangedVid = first->exchangedVid;
    first->exchangedVid = vidValue;
    const int nVidValue = second->nVid;
    second->nVid = first->nVid;
    first->nVid = nVidValue;
    for (unsigned int i = 0;i<VID::ScriptFunctionCount;++i)
    {
        const int function = second->scriptFunction[i];
        second->scriptFunction[i] = first->scriptFunction[i];
        first->scriptFunction[i] = function;
    }
}

STRING STRING::ToLower() const
{
    STRING converted(value);
    (void)_strlwr(converted.value);
    return STRING(converted);
}

STRING MAP::GetMouseTipsString() const
{
    STRING tipsText(StringGlobals::EmptyText());
    STRING selectedTipsText;
    const unsigned int index = m_playerIndex;
    PLAYER* const playerController = m_players[index];
    selectedTipsText = playerController->GetMouseTipsString();
    tipsText = selectedTipsText;
    return tipsText;
}

float MAP::GetGroundZScr(VECTOR2 point) const
{
    if (!(point.x >= 0.0f))
        point.x = 0.0f;
    else if (point.x >= m_sizeXY.x)
        point.x = (m_sizeXY.x-1.0f)/8.0f;
    else
        point.x /= 8.0f;
    if (!(point.y >= 0.0f))
        point.y = 0.0f;
    else if (point.y >= m_sizeXY.y)
        point.y = (m_sizeXY.y-1.0f)/8.0f;
    else
        point.y /= 8.0f;
    const int baseY = TruncateFloatToInt32(point.y);
    const int xIndex = TruncateFloatToInt32(point.x);
    const int width = m_groundWidth;
    const int height = m_groundHeight;
    int row = baseY+32;
    if (row >= height)
        row = height-1;
    int index = width*row+xIndex;
    const short* first = m_groundPrimary;
    const short* second = m_groundSecondary;
    for (;row >= baseY;--row, index -= width)
    {
        const int threshold = (row-baseY)*8;
        if (static_cast<int>(first[index]) >= threshold || static_cast<int>(second[index]) >= threshold)
            return static_cast<float>(threshold);
    }
    return 0.0f;
}

STRING MAP::OpenSaveDialog(int saveDialog, const char* filter) const
{
    MapOpenFileName ofn =
    {
    }
    ;
    char name[4096];
    for (unsigned int i = 0;i<sizeof(name);++i)
        name[i] = 0;
    name[0] = *reinterpret_cast<const unsigned char*>(StringGlobals::EmptyText());
    ofn.size = 76u;
    ofn.ownerWindow = m_dialog;
    ofn.instance = m_hInstance;
    ofn.filter = filter;
    ofn.filterIndex = 1u;
    ofn.file = name;
    ofn.maximumFile = 4096u;
    ofn.initialDirectory = "maps";
    ofn.flags = saveDialog?0x0008080Cu:0x0008180Cu;
    ofn.defaultExtension = "map";
    Graph->BeginPause();
    const unsigned int ok = saveDialog ?GetSaveFileNameA(reinterpret_cast<OPENFILENAMEA*>(&ofn)) :GetOpenFileNameA(reinterpret_cast<OPENFILENAMEA*>(&ofn));
    Graph->EndPause();
    if (ok) return STRING(name);
    return STRING();
}

STRING MAP::OpenDialog(const char* filter) const
{
    return OpenSaveDialog(0, filter);
}

int MAP::IsLineUnderGround(const VECTOR& fromPoint, const VECTOR& toPoint) const
{
    VECTOR point = fromPoint;
    VECTOR step = toPoint-point;
    const float absoluteX = fabsf(step.x);
    const float absoluteY = fabsf(step.y);
    const float stepExtent = (absoluteX>absoluteY?absoluteX:absoluteY)/12.0f;
    if (TruncateFloatToInt32(stepExtent) == 0)
        return IsDotUnderGround(point);
    step /= stepExtent;
    int remaining = TruncateFloatToInt32(stepExtent);
    while (--remaining >= 0)
    {
        point += step;
        if (IsDotUnderGround(point))
        {
            const int distance = TruncateFloatToInt32((point-fromPoint).NearLength());
            return distance != 0?distance:1;
        }
    }
    return 0;
}

void MAP::SetKeyboardLocale(const STRING& locale) const
{
    for (int index = 0;index<16;++index)
    {
        char layoutNameBuffer[512];
        (void)GetKeyboardLayoutNameA(layoutNameBuffer);
        STRING layoutName(layoutNameBuffer);
        layoutName.RemoveBeginChars("0");
        const char* layoutNameCursor = layoutName.value;
        const char* requestedLocaleCursor = locale.m_value;
        while (*layoutNameCursor == *requestedLocaleCursor && *layoutNameCursor)
        {
            ++layoutNameCursor;
            ++requestedLocaleCursor;
        }
        if ((unsigned char)*layoutNameCursor == (unsigned char)*requestedLocaleCursor)
            break;
        (void)ActivateKeyboardLayout(reinterpret_cast<HKL>(1u), 8u);
    }
}

int MAP::StartTact()
{
    unsigned int flags = m_flags&~FLAG_EXEC_REQUESTED;
    m_flags = flags;
    if ((flags&FLAG_PAUSED) == 0u && (::CurrentTime&7u) == 0u)
    {
        SPRITE_COLLECTOR* const collector = ::Sprites;
        collector->OptimizeForDraw();
    }
    flags = m_flags;
    if ((flags&FLAG_LOAD_REQUESTED) != 0u)
    {
        m_flags = flags&~FLAG_LOAD_REQUESTED;
        Load(m_pendingMapFileName);
    }
    unsigned int firstNow;
    unsigned int previous;
    unsigned int delta;
    do
    {
        firstNow = timeGetTime();
        previous = ::RealCurrentTime;
        delta = firstNow-previous;
    }
    while (delta == 0u);
    ::PrevRealCurrentTime = previous;
    ::RealCurrentTime = timeGetTime();
    ::PrevCurrentTime = ::CurrentTime;
    if (delta>71u)
        delta = 71u;
    const int scaledDelta = TruncateDoubleToInt32(static_cast<double>(delta)*static_cast<double>(m_scale40));
    ::CurrentTime = ::CurrentTime+static_cast<unsigned int>(scaledDelta);
    unsigned int frameCounter = m_frameCounter08+1u;
    m_frameCounter08 = frameCounter;
    const unsigned int now = ::RealCurrentTime;
    if (now-frameRateSampleTick >= 1000u)
    {
        frameRateSampleTick = now;
        m_frameRate04 = frameCounter;
        m_frameCounter08 = 0u;
    }
    if (m_players[0] != 0)
        ControlShiftCoor();
    INPUT* const input = &m_input;
    input->Tact();
    if (input->GetAction(IA_BOSS) != 0)
        m_flags ^= FLAG_BOSS_MODE;
    MSG message;
    while (PeekMessageA(&message, 0, 0u, 0u, PM_REMOVE) != 0)
    {
        if (message.message == WM_QUIT)
            return 1;
        bool handled = false;
        const unsigned int dialog = m_dialog;
        if (dialog != 0u)
        {
            GRAPH* const graph = Graph;
            if ((graph->capabilityFlags&1u) == 0u)
            {
                const unsigned int dialogWindow = m_dialogWindow;
                handled = TranslateAcceleratorA(reinterpret_cast<HWND>(U32ToPtr(dialog)), reinterpret_cast<HACCEL>(U32ToPtr(dialogWindow)), &message) != 0;
            }
        }
        if (!handled)
        {
            (void)TranslateMessage(&message);
            (void)DispatchMessageA(&message);
        }
    }
    if (m_simulationFrameCount == 0u)
        input->Reset();
    return (m_flags&FLAG_BOSS_MODE) != 0u;
}

void MAP::LoadWeapon(RESOURCE* resource)
{
    delete[] m_weaponData;
    m_weaponData = 0;
    void* serializedBuffer = 0;
    m_weaponCount = resource->Load(ResourceType::Weapons, &serializedBuffer, 0x280u);
    if (m_weaponCount != 0 && serializedBuffer != 0)
    {
        m_weaponData = new(std::nothrow) WEAPON_RUNTIME[static_cast<unsigned int>(m_weaponCount)];
        const WEAPON_SERIALIZED* const serialized = static_cast<const WEAPON_SERIALIZED*>(serializedBuffer);
        for (int record = 0; record<m_weaponCount; ++record)
        {
            WEAPON_RUNTIME& runtimeWeapon = m_weaponData[record];
            const WEAPON_SERIALIZED& serializedWeapon = serialized[record];
            std::memcpy(static_cast<void*>(&runtimeWeapon), &serializedWeapon, 0x21Cu);
            runtimeWeapon.maxColumn = 7;
            for (int direction = 0; direction<=runtimeWeapon.maxColumn; ++direction)
            {
                runtimeWeapon.Scale[direction] = VECTOR(serializedWeapon.ScaleX[direction], serializedWeapon.ScaleY[direction], serializedWeapon.ScaleZ[direction]);
                runtimeWeapon.Shift[direction] = VECTOR(serializedWeapon.ShiftX[direction], serializedWeapon.ShiftY[direction], serializedWeapon.ShiftZ[direction]);
                runtimeWeapon.DirectionX[direction] = static_cast<unsigned char>(serializedWeapon.DirectionX[direction]);
                runtimeWeapon.DirectionY[direction] = static_cast<unsigned char>(serializedWeapon.DirectionY[direction]);
                runtimeWeapon.DirectionZ[direction] = static_cast<unsigned char>(serializedWeapon.DirectionZ[direction]);
                runtimeWeapon.FrameSpeed[direction] = serializedWeapon.FrameSpeed[direction];
                runtimeWeapon.Speed[direction] = FloatEqualOrUnordered(serializedWeapon.Speed[direction], 999999.0f)
                    ? serializedWeapon.Speed[direction] : serializedWeapon.Speed[direction]/1000.0f;
                runtimeWeapon.ZSpeed[direction] = FloatEqualOrUnordered(serializedWeapon.ZSpeed[direction], 999999.0f)
                    ? serializedWeapon.ZSpeed[direction] : serializedWeapon.ZSpeed[direction]/1000.0f;
            }
        }
        ::operator delete(serializedBuffer);
    }
    ::EmptyVid->weapon = m_weaponData;
}

static void MapReloadVidDiagnostic(const char* subjectText, int category, int errorCode)
{
    STRING subject;
    subject = subjectText;
    MYERROR* sink = ::Error;
    if (sink != 0)
    {
        STRING objectFormat;
        objectFormat = "MAP";
        sink->Error(objectFormat, static_cast<TYPE_ERROR>(category), subject, errorCode);
    }
}

void MAP::ReloadVid()
{
    RESOURCE mapResource;
    RESOURCE* const resource = &mapResource;
    for (int index = 0;index<m_vidCount;++index)
    {
        VID* storedVid = m_vids[static_cast<unsigned int>(index)];
        if (storedVid == 0)
            continue;
        VID* linked = storedVid->exchangedVid;
        if (linked != storedVid)
            ExchangeVid(linked, storedVid);
    }
    STRING* const resourcePath = &m_resourceFileName;
    if (resource->OpenForRead(*resourcePath, ResourceType::Data) != 0)
    {
        MapReloadVidDiagnostic( "resource file", 7, 0);
        return;
    }
    LoadWeapon(resource);
    if (resource->GoBegin(ResourceType::Object) != 0u)
    {
        MapReloadVidDiagnostic( "load 'VID'", 11, 0);
        return;
    }
    do
    {
        int nvid = 0;
        (void)resource->Read(&nvid, 4u);
        if (nvid >= VidSlotCount)
        {
            MapReloadVidDiagnostic( "nvid > MAX_VID", 4, nvid);
        }
        VID* storedVid = m_vids[static_cast<unsigned int>(nvid)];
        if (storedVid != 0 && storedVid->sourceVid == 0)
        {
            VID* parentVid = storedVid->exchangedVid;
            nvid = parentVid->nVid;
            storedVid = m_vids[static_cast<unsigned int>(nvid)];
            storedVid->name.Read( resource);
            storedVid->LoadParameters(resource);
            const int weaponIndex = storedVid->nWeapon;
            WEAPON_RUNTIME* const weaponBase = m_weaponData;
            if (weaponIndex<m_weaponCount)
                storedVid->weapon = weaponBase+static_cast<unsigned int>(weaponIndex);
            else storedVid->weapon = weaponBase;
        }
    }
    while (resource->GoNextSub(ResourceType::Object) == 0);
    for (int index = 0;index<m_vidCount;++index)
    {
        VID* storedVid = m_vids[static_cast<unsigned int>(index)];
        if (storedVid != 0 && storedVid->sourceVid == 0 && (storedVid->storageFlags&VID::STORAGE_EMBEDDED) == 0u)
            storedVid->SetChildAndLink();
    }
    resource->Close();
}

SPRITE* MAP::OldLoadSprite(STREAM* inputStream)
{
    SPRITE* created = 0;
    SPRITE* persistentId;
    short vidIndex;
    short x;
    short y;
    short z;
    unsigned char direction;
    unsigned char legacyFlags;
    inputStream->Read(&persistentId, 4u);
    if (PtrToU32(persistentId) == 0xFFFFFFFFu)
        return reinterpret_cast<SPRITE*>(U32ToPtr(0xFFFFFFFFu));
    inputStream->Read(&vidIndex, 2u);
    inputStream->Read(&x, 2u);
    inputStream->Read(&y, 2u);
    inputStream->Read(&z, 2u);
    inputStream->Read(&direction, 1u);
    inputStream->Read(&legacyFlags, 1u);
    if (ValidateVid(vidIndex))
        created = CreateSprite(m_vids[static_cast<unsigned int>(vidIndex)],
            VECTOR(static_cast<float>(x), static_cast<float>(y), static_cast<float>(z)), ANGLE(direction), 0);
    else if (zmTemp == 0)
        Error(E_CREATE, STRING("sprite, this vid not exist"), vidIndex);
    m_relation.Insert(persistentId, created);
    return created;
}

void MAP::CopyVid(VID* dstVid, VID* srcVid)
{
    VID* const dst = dstVid;
    VID* const src = srcVid;
    VID* const nullVid = ::EmptyVid;
    if (dst == src || dst == 0 || src == 0 || dst == nullVid || src == nullVid || (dst->type&0x40u) == 0u || (src->type&0x40u) == 0u)
    {
        STRING subject("Can't copy vid to vid");
        if (::Error != 0)
        {
            STRING owner("MAP");
            ::Error->Error(owner, E_ERROR, subject, 0u);
        }
        return;
    }
    VID* const previousLink = dst->sourceVid;
    SPRITE* staged[256];
    int stagedCount = 0;
    if (previousLink != src)
    {
        for (unsigned int lane = 0;lane<8u;++lane)
        {
            PLAYER* const laneOwner = m_players[lane];
            SPRITE* sprite = laneOwner->m_flagman.m_value;
            while (sprite != 0 && stagedCount<256)
            {
                if (sprite->Vid() == dst)
                    staged[stagedCount++] = sprite;
                sprite = sprite->ChildChain();
            }
        }
        unsigned int expectedCount = 0u;
        for (unsigned int lane = 0;lane<8u;++lane)
            expectedCount += dst->spriteCountsByPlayer[lane];
        if (expectedCount != static_cast<unsigned int>(stagedCount))
        {
            stagedCount = 0;
            SPRITE* sprite = ::Sprites->FirstAllSprite();
            while (sprite != 0 && stagedCount<256)
            {
                if (sprite->Vid() == dst)
                    staged[stagedCount++] = sprite;
                sprite = ::Sprites->NextAllSprite();
            }
        }
        if (stagedCount>0)
        {
            for (int i = 0;i<stagedCount;++i)
                (void)::Sprites->Delete(staged[i]);
        }
    }
    reinterpret_cast<VID_SURFACE*>(dst)->CopyFromVid(reinterpret_cast<VID_SURFACE*>(src));
    if (previousLink != src && stagedCount>0)
    {
        for (int i = 0;i<stagedCount;++i)
        {
            SPRITE* const sprite = staged[i];
            ::Sprites->Insert(sprite);
            const unsigned int state = static_cast<unsigned int>(sprite->Animation());
            (void)sprite->ChangeAnimation(static_cast<int>(state^1u));
            (void)sprite->ChangeAnimation(static_cast<int>(state));
        }
    }
}

static void MapParseObjectLoadListRange(const char* source, int* loadStates, unsigned int stateCountLimit)
{
    std::FILE* file = 0;
    {
        const STRING mode("rt");
        const STRING sourceName(source);
        if (sourceName.value[0] != 0)
            file = std::fopen(sourceName.value, mode.value);
    }
    if (!file)
        return;
    char line[256];
    while ((std::feof(file)?0x10u:0u) == 0u)
    {
        if (std::fgets(line, static_cast<int>(256u), file) == 0 || line[0] == '\0')
            continue;
        unsigned int lineLength = 0u;
        while (line[lineLength] != '\0')
            ++lineLength;
        while (lineLength != 0u && (line[lineLength-1u] == '\n' || line[lineLength-1u] == '\r'))
            line[--lineLength] = '\0';
        char* comment = std::strchr(line, ';');
        if (comment)
            *comment = '\0';
        if (line[0] == '\0')
            continue;
        char* openSquare = std::strchr(line, '[');
        char* closeSquare = std::strchr(line, ']');
        char* openAngle = std::strchr(line, '<');
        char* closeAngle = std::strchr(line, '>');
        char* dash = std::strchr(line, '-');
        if (openAngle && closeAngle && openAngle<closeAngle)
        {
            *closeAngle = '\0';
            MapParseObjectLoadListRange(openAngle+1, loadStates, stateCountLimit);
            continue;
        }
        const int requestedState = (line[0] == '+')?1:0;
        int first = 0, last = 0;
        if (openSquare && closeSquare && dash && openSquare<closeSquare)
        {
            ++openSquare;
            *dash = '\0';
            first = std::atoi(openSquare);
            ++dash;
            *closeSquare = '\0';
            last = std::atoi(dash);
        }
        else
        {
            first = last = std::atoi(line);
        }
        if (first <= last)
        {
            int* stateEntry = loadStates+first;
            for (int index = first;index <= last;++index, ++stateEntry)
                if (index >= 0 && index<static_cast<int>(stateCountLimit))*stateEntry = requestedState;
        }
    }
    (void)static_cast<unsigned int>(std::fclose(file));
}

SPRITE* MAP::LoadSprite(STREAM* inputStream, int version)
{
    SPRITE* created = 0;
    SPRITE* persistentId;
    int vidIndex;
    VECTOR position;
    ANGLE direction;
    inputStream->Read(&persistentId, 4u);
    if (PtrToU32(persistentId) == 0xFFFFFFFFu)
        return reinterpret_cast<SPRITE*>(U32ToPtr(0xFFFFFFFFu));
    inputStream->Read(&vidIndex, 4u);
    if (version>9)
        position.Read(inputStream);
    else
    {
        int coordinate;
        inputStream->Read(&coordinate, 4u);
        position.x = static_cast<float>(coordinate);
        inputStream->Read(&coordinate, 4u);
        position.y = static_cast<float>(coordinate);
        inputStream->Read(&coordinate, 4u);
        position.z = static_cast<float>(coordinate);
    }
    direction.Read(inputStream);

    static ADD_CREATE_SPRITE creationContext;
    ::AddCreateSprite = &creationContext;
    inputStream->Read(&creationContext.group, 4u);
    if (version >= 16)
    {
        ::AddCreateSprite->gamma.Read(inputStream);
        ::AddCreateSprite->scale.Read(inputStream);
    }
    else
    {
        ::AddCreateSprite->gamma = GAMMA();
        ::AddCreateSprite->scale = VECTOR(1.0f, 1.0f, 1.0f);
    }
    if (ValidateVid(vidIndex))
        created = CreateSprite(m_vids[static_cast<unsigned int>(vidIndex)], position, direction, 0);
    else if (zmTemp == 0)
        Error(E_ERROR, STRING("can't load sprite, this vid not exist"), static_cast<unsigned long>(vidIndex));
    ::AddCreateSprite = 0;
    m_relation.Insert(persistentId, created);
    return created;
}

SPRITE* MAP::CreateSprite(VID* definition, const VECTOR& location, ANGLE direction, SPRITE* parentSprite)
{
    if (!definition || definition == ::EmptyVid)
        return 0;
    if ((definition->runtimeFlags&0x20u) != 0u)
        definition = REGION::ConvertVid(definition, location);
    if (definition->spriteLimit >= 0 && definition->NoSprites() >= definition->spriteLimit)
        return 0;
    SPRITE* created = 0;
    switch (definition->spriteClass)
    {
    case 22:
        created = new(std::nothrow) RAIL(definition, location, direction, parentSprite);
        break;
    case 26:
        created = new(std::nothrow) BALLOON(definition, location, direction, parentSprite);
        break;
    case 3:
        created = new(std::nothrow) BUILDING(definition, location, direction, parentSprite);
        break;
    case 24:
        created = new(std::nothrow) DEPO(definition, location, direction, parentSprite);
        break;
    case 20:
        created = new(std::nothrow) CIV_ROBOT(definition, location, direction, parentSprite);
        break;
    case 25:
        created = new(std::nothrow) CREATURE(definition, location, direction, parentSprite);
        break;
    case 5:
        created = new(std::nothrow) CANNON(definition, location, direction, parentSprite);
        break;
    case 9:
        created = new(std::nothrow) SPRITE(definition, location, direction, parentSprite);
        break;
    case 2:
        created = new(std::nothrow) UNIT(definition, location, direction, parentSprite);
        break;
    case 4:
        created = new(std::nothrow) PLANE(definition, location, direction, parentSprite);
        break;
    case 10:
        created = new(std::nothrow) FRAME(definition, location, direction, parentSprite);
        break;
    case 12:
        created = new(std::nothrow) LINKER(definition, location, direction, parentSprite);
        break;
    case 19:
        created = new(std::nothrow) STEXT(definition, location, direction, parentSprite);
        break;
    case 6:
        created = new(std::nothrow) PRIMITIVE(definition, location, direction, parentSprite);
        break;
    case 23:
        created = new(std::nothrow) REGION(definition, location, direction, parentSprite);
        break;
    case 7:
        created = new(std::nothrow) MAN(definition, location, direction, parentSprite);
        break;
    case 8:
        created = new(std::nothrow) BUILDED_TERRAIN(definition, location, direction, parentSprite);
        break;
    case 21:
        created = new(std::nothrow) ENGINE(definition, location, direction, parentSprite);
        break;
    case 0:
    case 1:
        created = new(std::nothrow) TERRAIN(definition, location, direction, parentSprite);
        break;
    default:
        {
            STRING subject("sprite - Behave is invalidate");
            const unsigned int detail = definition->spriteClass;
            if (::Error)
            {
                STRING owner("MAP");
                ::Error->Error(owner, E_CREATE, subject, detail);
            }
            return 0;
        }
    }
    if ((m_flags&(FLAG_EDITOR|FLAG_LOADING)) == 0u && created)
    {
        const int function = created->Vid()->scriptFunction[14];
        if (function >= 0)
            (void)ScriptRun(function, created, parentSprite, 0);
    }
    return created;
}

struct MapAbi_GroundScanState
{
    int highest;
    int accepted;
};

static int MapAbi_MapGroundGridIndex(float coordinate, float limit, int cellCount)
{
    if (!(coordinate >= 0.0f))
        return 0u;
    if (coordinate >= limit)
        return cellCount-1u;
    return TruncateFloatToInt32(coordinate)/8;
}

static void MapAbi_MapGroundAccumulate(int& highest, int& accepted, int sample)
{
    int difference;
    if (sample>highest)
    {
        difference = sample-highest;
        highest = sample;
    }
    else
    {
        difference = highest-sample;
    }
    if (difference<7)
        accepted = highest;
}

void MAP::ScanGroundHeightRect(float minX, float minY, float maxX, float maxY,
    int& highest, int& accepted) const
{
    const int width = m_groundWidth;
    const int height = m_groundHeight;
    const int x0 = MapAbi_MapGroundGridIndex(minX, m_sizeXY.x, width);
    const int x1 = MapAbi_MapGroundGridIndex(maxX, m_sizeXY.x, width);
    const int y0 = MapAbi_MapGroundGridIndex(minY, m_sizeXY.y, height);
    const int y1 = MapAbi_MapGroundGridIndex(maxY, m_sizeXY.y, height);
    if (y0>y1 || x0>x1)
        return;
    const short* const primary = m_groundPrimary;
    const short* const secondary = m_groundSecondary;
    for (int y = y0;y <= y1;++y)
    {
        const unsigned int row = static_cast<unsigned int>(y)*static_cast<unsigned int>(width);
        for (int x = x0;x <= x1;++x)
        {
            MapAbi_MapGroundAccumulate(highest, accepted, static_cast<int>(primary[row+x]));
            MapAbi_MapGroundAccumulate(highest, accepted, static_cast<int>(secondary[row+x]));
        }
    }
}

float MAP::GetGroundZ(const VID* vid, const VECTOR2& position, ANGLE direction) const
{
    const bool directional = (vid->property&0x100u) != 0u;
    VECTOR2 offset(0.0f, 0.0f);
    VECTOR2 halfSize;
    if (directional)
    {
        offset = direction.RotateY(VECTOR2(0.0f, vid->realSizeXYZ.y*0.27f));
        halfSize = VECTOR2(vid->realSizeXYZ.x*0.6f, vid->realSizeXYZ.y*0.3f);
    }
    else
    {
        halfSize = vid->sizeXYZ.XY()/2.0f;
    }
    if (halfSize.x>3.0f)
        halfSize.x -= 3.0f;
    if (halfSize.y>3.0f)
        halfSize.y -= 3.0f;
    MapAbi_GroundScanState scan =
    {
        -999999, -999999
    }
    ;
    VECTOR2 minimum = (position+offset)-halfSize;
    VECTOR2 maximum = (position+offset)+halfSize;
    ScanGroundHeightRect(minimum.x, minimum.y, maximum.x, maximum.y, scan.highest, scan.accepted);
    if (directional)
    {
        minimum = (position-offset)-halfSize;
        maximum = (position-offset)+halfSize;
        ScanGroundHeightRect(minimum.x, minimum.y, maximum.x, maximum.y, scan.highest, scan.accepted);
    }
    return static_cast<float>((scan.accepted != -999999)?scan.accepted:scan.highest);
}

void MAP::Release()
{
    for (int i = 0;i<m_vidCount;++i)
    {
        VID* const vid = m_vids[static_cast<unsigned int>(i)];
        if (!vid)
            continue;
        unsigned int noVidSum = 0u;
        for (unsigned int playerIndex = 0;playerIndex<8u;++playerIndex)
            noVidSum += static_cast<unsigned int>(vid->spriteCountsByPlayer[playerIndex]);
        if (noVidSum == 0)
            continue;
        ::Error->Log("    NoVid[%3i]=%i %i %i %i %s Layer=%i %s VidType=%x", i, static_cast<int>(vid->spriteCountsByPlayer[0]), static_cast<int>(vid->spriteCountsByPlayer[1]), static_cast<int>(vid->spriteCountsByPlayer[2]), static_cast<int>(vid->spriteCountsByPlayer[3]), vid->name.m_value, vid->layer, vid->vidName.m_value, static_cast<unsigned int>(vid->type));
    }
    m_relation.Release();
    m_velocityX = 0.0f;
    m_scale40 = 1.0f;
    m_frameRate04 = 0u;
    m_frameCounter08 = 0u;
    m_velocityY = 0.0f;
    m_flags &= ~FLAG_DEMO_SYNC_ERROR;
    Graph->movie.Release();
    Graph->SetWind(25, ANGLE(200u));
    Graph->SetCameraDirection(ANGLE(0));
    Graph->SetScreenScale(VECTOR(1.0f, 1.0f, 1.0f));
    Graph->SetEnvironment(0xFFFFFFFFu);
    Graph->Effect(11, 0, 0, 0);
    RESOURCE* const replay = &m_resource;
    if ((m_flags&FLAG_DEMO_PLAYBACK) != 0u)
    {
        Mouse->Enable();
        replay->Close();
    }
    if ((m_flags&FLAG_DEMO_RECORDING) != 0u)
    {
        int marker = -1;
        (void)replay->Write(&marker, 4u);
        replay->PostAppend();
        replay->Close();
    }
    m_flags &= ~(FLAG_DEMO_RECORDING|FLAG_DEMO_PLAYBACK);
    const VECTOR2 defaultFirst(-1000.0f, -1000.0f);
    const VECTOR2 defaultSecond(-1000.0f, -1000.0f);
    SetScrollType(1, defaultFirst, defaultSecond);
    ::Error->Log("Player release");
    *reinterpret_cast<unsigned char*>(&m_pendingNetworkPauseCommand) = 0u;
    for (unsigned int i = 0;i<8u;++i)
    {
        PLAYER* const playerController = m_players[i];
        if (playerController)
            playerController->Release();
    }
    ::Error->Log("Sprite release");
    SPRITE_COLLECTOR* const collector = ::Sprites;
    if (collector)
        collector->DeleteAll();
    void* const groupOwner = m_groups.m_next;
    void* const embeddedGroup = &m_groups;
    if (groupOwner && groupOwner != embeddedGroup)
    {
        STRING subject("Incorrect delete groups in DeleteAll()");
        if (::Error)
        {
            STRING objectFormat("MAP");
            ::Error->Error(objectFormat, E_ERROR, subject, 0);
        }
    }
    ::Error->Log("Menu   release");
    SPRITE_LIST* const menuList = &m_menu;
    if (menuList->m_count != 0)
    {
        STRING subject("Menu sprite exist after delete");
        SPRITE* const sprite = menuList->m_data[0];
        sprite->Error(E_ERROR, subject, 0u);
        menuList->DeleteAll();
    }
    const unsigned int elapsed = ::CurrentTime-m_tick60;
    const unsigned int averageFps = (elapsed != 0u) ? (m_simulationFrameCount*1000u)/elapsed : 0u;
    ::Error->Log("Average fps=%i", averageFps);
    ::Error->Log("Script release");
    m_script.Release();
    m_simulationFrameCount = 0u;
    DeleteExtraVid();
    for (int i = 0;i<m_vidCount;++i)
    {
        VID* const vid = m_vids[static_cast<unsigned int>(i)];
        if (vid)
            vid->ResetSprites();
    }
    for (unsigned int i = 0;i<64u;++i)
        EvFunctionNumber[i] = static_cast<int>(1000000u+i);
    for (unsigned int i = 0;i<10u;++i)
        m_spriteCountsByNameNumber[i] = 0u;
    VID* const emptyVid = ::EmptyVid;
    unsigned int remaining = 0u;
    for (unsigned int playerIndex = 0;playerIndex<8u;++playerIndex)
        remaining += static_cast<unsigned int>(emptyVid->spriteCountsByPlayer[playerIndex]);
    ::Error->Log("No EmptyVid=%i", remaining);
}

int MAP::CanPlaceGroundHeightRect(const VID* vid, const VECTOR& position,
    float minX, float minY, float maxX, float maxY, bool steppedHeight) const
{
    const int width = m_groundWidth;
    const int height = m_groundHeight;
    const int x0 = MapAbi_MapGroundGridIndex(minX, m_sizeXY.x, width);
    const int x1 = MapAbi_MapGroundGridIndex(maxX, m_sizeXY.x, width);
    const int y0 = MapAbi_MapGroundGridIndex(minY, m_sizeXY.y, height);
    const int y1 = MapAbi_MapGroundGridIndex(maxY, m_sizeXY.y, height);
    const short* const primary = m_groundPrimary;
    const short* const secondary = m_groundSecondary;
    if (steppedHeight)
    {
        MapAbi_GroundScanState scan =
        {
            -999999, -999999
        }
        ;
        for (int y = y0;y <= y1;++y)
        {
            const unsigned int row = static_cast<unsigned int>(y)*static_cast<unsigned int>(width);
            for (int x = x0;x <= x1;++x)
            {
                MapAbi_MapGroundAccumulate(scan.highest, scan.accepted, static_cast<int>(primary[row+x]));
                MapAbi_MapGroundAccumulate(scan.highest, scan.accepted, static_cast<int>(secondary[row+x]));
            }
        }
        const int ground = (scan.accepted != -999999)?scan.accepted:scan.highest;
        const float upward = static_cast<float>(ground)-position.z;
        if (upward>vid->forMoveUpZ)
            return 1;
        const float downward = position.z-static_cast<float>(ground);
        if (downward>vid->forMoveDownZ)
            return 2;
        return 0;
    }
    for (int y = y0;y <= y1;++y)
    {
        const unsigned int row = static_cast<unsigned int>(y)*static_cast<unsigned int>(width);
        for (int x = x0;x <= x1;++x)
        {
            if (static_cast<float>(primary[row+x])>position.z || static_cast<float>(secondary[row+x])>position.z)
                return 1;
        }
    }
    return 0;
}

int MAP::CanPlaceInGridZ(const VID* vid, const VECTOR& position, ANGLE direction) const
{
    if (vid->moveMask == 0u)
        return 0;
    const bool directional = (vid->property&0x100u) != 0u;
    const bool steppedHeight = (vid->property&0x200u) != 0u;
    VECTOR2 offset(0.0f, 0.0f);
    VECTOR2 halfSize;
    if (directional)
    {
        offset = direction.RotateY(VECTOR2(0.0f, vid->realSizeXYZ.y*0.27f));
        halfSize = VECTOR2(vid->realSizeXYZ.x*0.6f, vid->realSizeXYZ.y*0.3f);
    }
    else
    {
        halfSize = vid->sizeXYZ.XY()/2.0f;
    }
    if (halfSize.x>3.0f)
        halfSize.x -= 3.0f;
    if (halfSize.y>3.0f)
        halfSize.y -= 3.0f;
    VECTOR2 minimum = (position.XY()+offset)-halfSize;
    VECTOR2 maximum = (position.XY()+offset)+halfSize;
    const int result = CanPlaceGroundHeightRect(vid, position, minimum.x, minimum.y, maximum.x, maximum.y, steppedHeight);
    if (result != 0 || !directional)
        return result;
    minimum = (position.XY()-offset)-halfSize;
    maximum = (position.XY()-offset)+halfSize;
    return CanPlaceGroundHeightRect(vid, position, minimum.x, minimum.y, maximum.x, maximum.y, steppedHeight);
}

void MAP::Save(STRING name)
{
    RESOURCE mapResource;
    RESOURCE* const resource = &mapResource;
    const char* const empty = StringGlobals::EmptyText();
    const char* const requested = name.m_value;
    if (std::strcmp(requested, empty) == 0)
    {
        return;
    }
    STRING* const currentName = &m_currentMapFileName;
    static const char kTemporaryMapName[] = "tmp_del!.map";
    if (std::strcmp(requested, currentName->m_value) == 0)
    {
        STRING temporaryMapName(kTemporaryMapName);
        (void)::MoveFileExA(currentName->m_value, temporaryMapName.m_value, 9u);
        *currentName = kTemporaryMapName;
    }
    if (resource->OpenForWrite(name, ResourceType::Map) != 0)
    {
        MYERROR* const sink = ::Error;
        sink->Window(STRING("Can't open file %s"), name.m_value);
        return;
    }
    bool carryForwardRecords = false;
    for (int index = 0;index<m_vidCount;++index)
    {
        VID* const entry = m_vids[index];
        if (entry && (entry->storageFlags&VID::STORAGE_EMBEDDED) != 0u)
        {
            carryForwardRecords = true;
            break;
        }
    }
    if (carryForwardRecords)
    {
        RESOURCE previousMapResource;
        RESOURCE* const oldResource = &previousMapResource;
        if (oldResource->OpenForRead(*currentName, ResourceType::Map) == 0)
        {
            (void)resource->Copy(oldResource, ResourceType::Weapons);
            (void)resource->Copy(oldResource, ResourceType::Object);
            oldResource->Close();
        }
        else
        {
            MYERROR* const sink = ::Error;
            sink->Window(STRING("Can't open file '%s', needed for save map"), currentName->m_value);
        }
    }
    (void)resource->PreAppend(ResourceType::Graphics, 0);
    Graph->SaveParameters(reinterpret_cast<STREAM*>(resource));
    resource->PostAppend();
    (void)resource->PreAppend(ResourceType::Header, 0);
    m_sizeXY.Save(resource);
    m_shiftXY.Save(resource);
    (void)resource->Write(&::CurrentTime, 4u);
    const unsigned int version = 20u;
    (void)resource->Write(&version, 4u);
    resource->PostAppend();
    const int gridWidth = m_groundWidth;
    const int gridHeight = m_groundHeight;
    const unsigned short* const grid = reinterpret_cast<const unsigned short*>(m_groundPrimary);
    bool gridUsed = false;
    if (gridHeight>0)
    {
        for (int y = 0;y<gridHeight && !gridUsed;++y)
            for (int x = 0;x<gridWidth;++x) if (grid[static_cast<unsigned int>(y)*static_cast<unsigned int>(gridWidth)+static_cast<unsigned int>(x)] != 0u)
        {
            gridUsed = true;
            break;
        }
        if (gridUsed)
        {
            const unsigned int cells = static_cast<unsigned int>(gridWidth)*static_cast<unsigned int>(gridHeight);
            (void)resource->PreAppend(ResourceType::Grid, 0);
            (void)resource->Write(grid, cells*2u);
            resource->PostAppend();
        }
    }
    (void)resource->PreAppend(ResourceType::Sprites, 0);
    for (SPRITE* sprite = ::Sprites->FirstAllSpriteFrom0();sprite;sprite = ::Sprites->NextAllSpriteFrom0())
    {
        if (sprite->ChildBacklink())
            continue;
        int menuIndex = m_menu.m_count-1;
        while (menuIndex >= 0 && m_menu.m_data[menuIndex] != sprite)
            --menuIndex;
        if (menuIndex<0)
            sprite->Save(resource);
    }
    int spriteSentinel = -1;
    (void)resource->Write(&spriteSentinel, 4u);
    resource->PostAppend();
    for (SPRITE* sprite = ::Sprites->FirstAllSpriteFrom0();sprite;sprite = ::Sprites->NextAllSpriteFrom0())
    {
        if (sprite->ChildBacklink())
            continue;
        int menuIndex = m_menu.m_count-1;
        while (menuIndex >= 0 && m_menu.m_data[menuIndex] != sprite)
            --menuIndex;
        if (menuIndex >= 0)
            continue;
        const unsigned int before = static_cast<unsigned int>(resource->Position());
        (void)resource->PreAppend(ResourceType::SpriteData, 0);
        (void)resource->Write(&sprite, 4u);
        (void)sprite->Action(ACT_SAVE, static_cast<int>(PtrToU32(resource)), 0, 0);
        const unsigned int after = static_cast<unsigned int>(resource->Position());
        if (BitsToSigned32(after)>BitsToSigned32(before+5u))
            resource->PostAppend();
    }
    (void)resource->PreAppend(ResourceType::SpriteData, 0);
    (void)resource->Write(&spriteSentinel, 4u);
    resource->PostAppend();
    (void)resource->PreAppend(ResourceType::Player, 0);
    for (unsigned int index = 0;index<8u;++index)
    {
        PLAYER* const player = m_players[index];
        player->Save(reinterpret_cast<STREAM*>(resource));
    }
    resource->PostAppend();
    (void)resource->PreAppend(ResourceType::Groups, 0);
    m_groups.Save(resource);
    resource->PostAppend();
    resource->Close();
    if (std::strcmp(currentName->m_value, kTemporaryMapName) == 0)
    {
        STRING temporaryMapName(kTemporaryMapName);
        (void)std::remove(temporaryMapName.m_value);
        (void)currentName->operator = (name);
    }
}

static bool MapLoadVidFileExists(const STRING& fileName)
{
    if (fileName.m_value[0] == 0)
        return false;
    std::FILE* const file = std::fopen(fileName.m_value, "rb");
    if (!file)
        return false;
    (void)std::fclose(file);
    return true;
}

void MAP::LoadVid(RESOURCE* resource)
{
    int nvid = 0;
    const unsigned int loadStart = timeGetTime();
    STRING* const loadingText = &mapLoadingText;
    PROFILE* const profile = ::Strings;
    if (profile)
    {
        STRING section;
        STRING key;
        STRING fallback;
        section = "strings";
        key = "Loading";
        fallback = "LOADING...";
        STRING localized = (::Strings)->GetString(section, key, fallback);
        *loadingText = localized;
    }
    else
    {
        STRING empty;
        empty = StringGlobals::EmptyText();
        *loadingText = empty;
    }
    if (m_weaponData == 0)
    {
        LoadWeapon(resource);
        const int weaponCount = m_weaponCount;
        if (weaponCount != 0)
        {
            ::Error->Log("LoadWeapon::No=%-5i             sizeof(WEAPON)=%-4i", weaponCount, 572);
        }
    }
    int loadTable[VidSlotCount];
    for (unsigned int i = 0;i<static_cast<unsigned int>(VidSlotCount);++i)
        loadTable[i] = 1;
    MapParseObjectLoadListRange("objects_load_list.txt", loadTable, static_cast<unsigned int>(VidSlotCount));
    if (MapLoadVidFileExists(STRING("vid\\fast_load.vid")) &&
        MapLoadVidFileExists(STRING("vid\\fast_load_data.vid")))
    {
        {
            STRING path("vid\\fast_load.vid");
            m_fastLoadIndex = static_cast<unsigned char*>(FFileMapping(path));
        }
        {
            STRING path("vid\\fast_load_data.vid");
            m_fastLoadData = static_cast<unsigned char*>(FFileMapping(path));
        }
    }
    static const unsigned int kObjectResourceType = ResourceType::Object;
    if (resource->GoBegin(kObjectResourceType) != 0)
        return;
    const bool overlayExistingDepot = m_vidCount != 0;
    do
    {
        (void)resource->Read(&nvid, 4u);
        if (nvid >= VidSlotCount)
        {
            Error(static_cast<TYPE_ERROR>(4), STRING("nvid > MAX_VID"), nvid);
        }
        if (loadTable[nvid] == 0)
            continue;
        VID* previous = m_vids[static_cast<unsigned int>(nvid)];
        if (previous)
        {
            delete previous;
            m_vids[static_cast<unsigned int>(nvid)] = 0;
            Error(static_cast<TYPE_ERROR>(5), STRING("this VID already loaded"), nvid);
        }
        VID* created = CreateVid(resource, nvid);
        m_vids[static_cast<unsigned int>(nvid)] = created;
        if (!created)
            continue;
        if (nvid >= m_vidCount)
            m_vidCount = nvid+1;
        if (overlayExistingDepot)
            created->storageFlags |= VID::STORAGE_EMBEDDED;
        const int weaponIndex = created->nWeapon;
        WEAPON_RUNTIME* weapon = 0;
        if (weaponIndex<m_weaponCount)
        {
            weapon = m_weaponData+weaponIndex;
        }
        else
        {
            {
                STRING subject;
                subject = "nWeapon > noWeapon";
                VID* const current = m_vids[static_cast<unsigned int>(nvid)];
                current->Error((TYPE_ERROR)10, subject, static_cast<unsigned long>(current->nWeapon));
            }
            weapon = m_weaponData;
        }
        m_vids[static_cast<unsigned int>(nvid)]->weapon = weapon;
        if (nvid == 0)
        {
            mapLoadingVid = m_vids[0];
            DispatchMapPresentation(10u);
        }
        else
        {
            DispatchMapPresentation(0u);
        }
    }
    while (resource->GoNextSub(kObjectResourceType) == 0);
    int maxSizeX = 0;
    int maxSizeY = 0;
    for (int index = 0;index<m_vidCount;++index)
    {
        VID* const vid = m_vids[static_cast<unsigned int>(index)];
        if (!vid)
            continue;
        vid->SetChildAndLink();
        VID* const linked = m_vids[static_cast<unsigned int>(index)];
        const int sx = static_cast<int>(linked->vidSizeX);
        const int sy = static_cast<int>(linked->vidSizeY);
        if (sx>maxSizeX)
            maxSizeX = sx;
        if (sy>maxSizeY)
            maxSizeY = sy;
    }
    for (int index = 0;index<m_vidCount;++index)
    {
        VID* const vid = m_vids[static_cast<unsigned int>(index)];
        if (!vid)
            continue;
        (void)vid->CalcImportantForSync();
        ::Error->Log("vid[%i].sync = %i", index, (m_vids[static_cast<unsigned int>(index)]->runtimeFlags>>12u)&1u);
    }
    const unsigned int loadElapsed = timeGetTime()-loadStart;
    ::Error->Log("LoadVid::No   =%-15i   sizeof(VID)   =%-5i    load time     =%-6ims MaxSizeX, Y     =%i,%i sizeof(SPRITE)=%-8i", m_vidCount, 1344, loadElapsed, maxSizeX, maxSizeY, 104);
    const unsigned int waitStart = timeGetTime();
    DispatchMapPresentation(9u);
    while (timeGetTime()-waitStart<300u)
        DispatchMapPresentation(0u);
    unsigned char* const mappedFastLoad = m_fastLoadIndex;
    if (mappedFastLoad)
        (void)UnmapViewOfFile(mappedFastLoad);
    m_fastLoadIndex = 0;
}

void MAP::Load(STRING name)
{
    RESOURCE mapResource;
    RESOURCE* const resource = &mapResource;
    const unsigned int oldGraphTime = ::CurrentTime;
    const int loadingSlotRemaining = Graph->GetEffectState(3);
    SetupProcessorFlags();
    const char* const empty = StringGlobals::EmptyText();
    const char* requested = name.m_value;
    if (std::strcmp(requested, empty) == 0)
    {
        return;
    }
    m_flags |= FLAG_LOADING;
    mapLoadingVid = 0;
    if (Const->DebugMode != 0)
    {
        STRING text("Release previous map");
        Graph->DrawDebugText(text);
    }
    Release();
    if (Const->DebugMode != 0)
    {
        delete ::g_pDlgSrv;
        ::g_pDlgSrv = new(std::nothrow) zDlgSrv();
        ::g_pDlgSrv->LoadDialogs(m_dialogFileName);
    }
    RESOURCE* const demoResource = &m_resource;
    if (m_resource.m_file == 0 && demoResource->OpenForRead(name, ResourceType::Demo) == 0)
    {
        (void)name.Read(demoResource);
        m_flags |= FLAG_DEMO_PLAYBACK;
        requested = name.m_value;
    }
    if (resource->OpenForRead(name, ResourceType::Map) != 0)
    {
        MYERROR* const sink = ::Error;
        sink->Window(STRING("!!!ERROR!!!LOAD: Invalid map file %s"), name.m_value);
        return;
    }
    Mouse->Disable();
    (void)m_previousMapFileName.operator = (m_currentMapFileName);
    (void)m_currentMapFileName.operator = (name);
    unsigned int randomSeed;
    if ((m_flags&FLAG_DEMO_PLAYBACK) != 0u)
    {
        (void)demoResource->Read(&randomSeed, 4u);
    }
    else
    {
        NET* const network = ::Net;
        if ((network->m_flags&0x10u) != 0u)
            randomSeed = 0x00DB2F7Fu;
        else
        {
            randomSeed = ::timeGetTime();
        }
    }
    srand(static_cast<unsigned int>(randomSeed));
    if (Const->DebugMode != 0)
    {
        STRING text("Load extra vid");
        Graph->DrawDebugText(text);
    }
    const bool currentFormat = (resource->GoBegin(ResourceType::Graphics) == 0);
    int loadingTerrainSpriteCount = 0;
    unsigned int lastHardwareTerrainTag = 0u;
    unsigned int mapVersion;
    if (currentFormat)
    {
        if (Const->DebugMode != 0)
        {
            STRING text("Load graph parameters");
            Graph->DrawDebugText(text);
        }
        Graph->LoadParameters(resource, m_sizeXY);
        if (resource->GoNext(ResourceType::Header) != 0)
        {
            Error(E_SECTION, STRING("HEAD"), 0u);
            return;
        }
        else
        {
            m_sizeXY.Read(resource);
            m_shiftXY.Read(resource);
            (void)resource->Read(&::CurrentTime, 4u);
            ::PrevCurrentTime = 1u;
            ::CurrentTime = 10u;
            pauseStartTick = 10u;
            if ((m_flags&FLAG_DEMO_PLAYBACK) != 0u)
            {
                (void)demoResource->Read(&::CurrentTime, 4u);
                m_input.Reset();
            }
            Graph->SetTime(oldGraphTime, ::CurrentTime);
            if ((m_flags&FLAG_DEMO_PLAYBACK) == 0u && m_resource.m_file != 0)
            {
                m_flags |= FLAG_DEMO_RECORDING;
                (void)demoResource->PreAppend(ResourceType::Demo, 0);
                unsigned int textBytes = 1u;
                const char* text = name.m_value;
                while (text[textBytes-1u] != 0)
                    ++textBytes;
                (void)demoResource->Write(text, textBytes);
                (void)demoResource->Write(&randomSeed, 4u);
                (void)demoResource->Write(&::CurrentTime, 4u);
                m_input.Reset();
            }
            m_tick60 = ::CurrentTime;
            (void)resource->Read(&mapVersion, 4u);
            if (static_cast<int>(mapVersion) <= 9)
            {
                m_sizeXY.x = static_cast<float>(*reinterpret_cast<int*>(&m_sizeXY.x));
                m_sizeXY.y = static_cast<float>(*reinterpret_cast<int*>(&m_sizeXY.y));
                m_shiftXY.x = static_cast<float>(*reinterpret_cast<int*>(&m_shiftXY.x));
                m_shiftXY.y = static_cast<float>(*reinterpret_cast<int*>(&m_shiftXY.y));
            }
            NET* const network = ::Net;
            ::Error->Log("CurrentTime   =%-15u   SizeXY = %i, %i Map version   =%-8i NoNetworkPlayer=%i", ::CurrentTime, static_cast<unsigned int>(TruncateFloatToInt32OrMin(m_sizeXY.x)), static_cast<unsigned int>(TruncateFloatToInt32OrMin(m_sizeXY.y)), mapVersion, network->m_noPlayers);
        }
    }
    else
    {
        if (resource->GoBegin(ResourceType::Header) != 0)
        {
            Error(E_SECTION, STRING("HEAD"), 0u);
            return;
        }
        else
        {
            m_sizeXY.Read(resource);
            int legacyWidth;
            int legacyHeight;
            std::memcpy(&legacyWidth, &m_sizeXY.x, 4u);
            std::memcpy(&legacyHeight, &m_sizeXY.y, 4u);
            m_sizeXY.x = static_cast<float>(legacyWidth);
            m_sizeXY.y = static_cast<float>(legacyHeight);
            short legacyCoordinate;
            resource->Read(&legacyCoordinate, 2u);
            m_shiftXY.x = static_cast<float>(legacyCoordinate);
            resource->Read(&legacyCoordinate, 2u);
            m_shiftXY.y = static_cast<float>(legacyCoordinate);
            (void)resource->Read(&::CurrentTime, 4u);
            m_tick60 = ::CurrentTime;
            ::PrevCurrentTime = ::CurrentTime;
            Graph->SetTime(oldGraphTime, ::CurrentTime);
            (void)resource->Read(&mapVersion, 4u);
            Graph->OldLoadParameters(resource, m_sizeXY);
        }
    }
    {
        if (currentFormat && Const->DebugMode != 0)
        {
            STRING text("Create new hash table");
            Graph->DrawDebugText(text);
        }
        SPRITE_COLLECTOR* const oldCollector = ::Sprites;
        if (oldCollector)
        {
            delete oldCollector;
        }
        ::Sprites = new(std::nothrow) SPRITE_COLLECTOR(m_sizeXY);
        SetScrollBox(VECTOR2(0, 0), m_sizeXY);
        if (currentFormat)
        {
            SetShiftCoor(m_shiftXY+Graph->SizeXY()/2.0f, 0);
            if (Const->DebugMode != 0)
            {
                STRING text("Load gridZ");
                Graph->DrawDebugText(text);
            }
        }
        ResetGroundZ();
        if (resource->GoNext(ResourceType::Grid) == 0)
        {

            if (m_groundPrimary)
                ::operator delete(m_groundPrimary);
            m_groundPrimary = 0;
            const unsigned int gridBytes = static_cast<unsigned int>(resource->SubLoad(reinterpret_cast<void**>(&m_groundPrimary), 0));
            const unsigned int expected = static_cast<unsigned int>(m_groundWidth)* static_cast<unsigned int>(m_groundHeight)*2u;
            if (gridBytes != expected)
            {
                Error(E_INVALID, STRING("grid"), gridBytes);
                ResetGroundZ();
            }
            else
            {
                if (currentFormat)
                    ::Error->Log("Load GridZ from map");
            }
        }
        else
        {
            (void)resource->GoBegin(ResourceType::Any);
        }
        if (currentFormat)
            Graph->SetDefaultState();
        if (resource->GoNext(ResourceType::Sprites) != 0)
        {
            Error(E_SECTION, STRING("SPR "), 0u);
            return;
        }
        else
        {
            STREAM* const stream = resource;
            if (currentFormat)
            {
                if (Const->DebugMode != 0)
                {
                    STRING text("Load hardware terrain");
                    Graph->DrawDebugText(text);
                }
                for (;;)
                {
                    SPRITE* sprite = LoadSprite(stream, static_cast<int>(mapVersion));
                    if (sprite == reinterpret_cast<SPRITE*>(AddressToUIntPtr(0xFFFFFFFFu)))
                        break;
                    if (sprite && sprite->IsSpriteClass(8u))
                        ++loadingTerrainSpriteCount;
                    if (sprite && Const->DebugMode != 0 &&
                        lastHardwareTerrainTag != static_cast<unsigned int>(sprite->Vid()->layer))
                    {
                        lastHardwareTerrainTag = static_cast<unsigned int>(sprite->Vid()->layer);
                        STRING text(lastHardwareTerrainTag == 0u ? "Load hardware terrain" : "Load sprites");
                        Graph->DrawDebugText(text);
                    }
                }
                unsigned int vertexMemory = 0u;
                unsigned int indexMemory = 0u;
                unsigned int textureMemory = 0u;
                for (int vidIndex = 0; vidIndex < VidSlotCount; ++vidIndex)
                {
                    VID* const vid = m_vids[vidIndex];
                    if (!vid || vid->NoSprites() == 0 || (vid->type&0x40u) == 0u)
                        continue;
                    const VID_SURFACE* const surface = static_cast<const VID_SURFACE*>(vid);
                    if (!surface->frames || vid->noCadr <= 0)
                        continue;
                    const unsigned int bytesPerPixel = surface->pixelFormat == D3DFMT_A8R8G8B8 ? 4u :
                        surface->pixelFormat == D3DFMT_R8G8B8 ? 3u : surface->pixelFormat == D3DFMT_P8 ? 1u : 2u;
                    for (int frameIndex = 0; frameIndex < vid->noCadr; ++frameIndex)
                    {
                        const CADR_DATA& frame = surface->frames[frameIndex];
                        vertexMemory += static_cast<unsigned int>(frame.extraDataCount)*24u+
                            static_cast<unsigned int>(frame.vertexCount)*20u;
                        indexMemory += static_cast<unsigned int>(frame.indexCount)*2u;
                        const int width = static_cast<short>(frame.width);
                        const int height = static_cast<short>(frame.height);
                        textureMemory += static_cast<unsigned int>(width)*
                            static_cast<unsigned int>(height)*bytesPerPixel;
                    }
                }
                ::Error->Log("VertexBufferMemory = %i  IndexBufferMemory = %i  TextureMemory = %i",
                    BitsToSigned32(vertexMemory), BitsToSigned32(indexMemory), BitsToSigned32(textureMemory));
            }
            else
            {
                for (;;)
                {
                    SPRITE* sprite = OldLoadSprite(stream);
                    if (sprite == reinterpret_cast<SPRITE*>(AddressToUIntPtr(0xFFFFFFFFu)))
                        break;
                }
            }
        }
    }
    {
        RailMap.CreateAdditionalDots();
        if (currentFormat)
        {
            STRING* const globalLoadingText = &mapLoadingText;
            if (loadingTerrainSpriteCount>30 && loadingSlotRemaining>0)
            {
                STRING section("strings");
                STRING key("Loading");
                STRING fallback("LOADING...");
                STRING localized = (::Strings)->GetString(section, key, fallback);
                *globalLoadingText = localized;
                if (mapLoadingVid == 0)
                    mapLoadingVid = m_vids[0];
                DispatchMapPresentation(10u);
            }
            else *globalLoadingText = StringGlobals::EmptyText();
            if (Const->DebugMode != 0)
            {
                STRING text("Load data for sprite");
                Graph->DrawDebugText(text);
            }
        }
        if (resource->GoNext(ResourceType::SpriteData) != 0)
        {
            Error(E_SECTION, STRING("SPRD"), 0u);
            return;
        }
        else
        {
            STREAM* const stream = resource;
            for (;;)
            {
                SPRITE* sprite = ReadPointer(stream);
                if (sprite == reinterpret_cast<SPRITE*>(AddressToUIntPtr(0xFFFFFFFFu)))
                    break;
                if (currentFormat)
                    DispatchMapPresentation(0u);
                if (sprite)
                {
                    (void)sprite->Action(currentFormat?ACT_RESTORE:ACT_LEGACY_RESTORE, static_cast<int>(PtrToU32(resource)), static_cast<int>(mapVersion), 0);
                }
                (void)resource->GoNextSub(ResourceType::SpriteData);
            }
        }
    }
    if (currentFormat)
    {
        if (Const->DebugMode != 0)
        {
            STRING text("Load players info");
            Graph->DrawDebugText(text);
        }
        if (resource->GoNext(ResourceType::Player) != 0)
        {
            Error(E_SECTION, STRING("PLAY"), 0u);
            return;
        }
        else
        {
            for (unsigned int index = 0u;index<8u;++index)
            {
                PLAYER* const player = m_players[index];
                player->Load(resource);
            }
        }
        if (Const->DebugMode != 0)
        {
            STRING text("Load groups info");
            Graph->DrawDebugText(text);
        }
        if (resource->GoNext(ResourceType::Groups) != 0)
        {
            Error(E_SECTION, STRING("GROU"), 0u);
            return;
        }
        m_groups.Load(resource);
    }
    m_flags &= ~FLAG_LOADING;
    resource->Close();
    m_relation.Release();
    ::Error->Log("Vid    release %i %i", TEXTURE::memoryInUse, VID::MemoryInUse);
    const unsigned int loadTick = ::timeGetTime();
    ::RealCurrentTime = loadTick;
    for (unsigned int playerIndex = 0u;playerIndex<8u;++playerIndex)
    {
        PLAYER* const player = m_players[playerIndex];
        player->InitForLoadMap();
    }
    MOUSE* const mapCollision = Mouse;
    {
        VECTOR collisionPosition;
        collisionPosition.x = m_shiftXY.x+m_input.m_clientMouseXY.x;
        collisionPosition.y = m_shiftXY.y+m_input.m_clientMouseXY.y;
        collisionPosition.z = mapCollision->Coor().z;
        mapCollision->ChangeCoor(collisionPosition);
    }
    if (Const->DebugMode != 0)
    {
        STRING text(StringGlobals::EmptyText());
        Graph->DrawDebugText(text);
    }
    if (loadingTerrainSpriteCount>30 && loadingSlotRemaining>0)
    {
        const unsigned int waitStart = ::timeGetTime();
        DispatchMapPresentation(9u);
        while (::timeGetTime()-waitStart<300u)
            DispatchMapPresentation(0u);
        Graph->Effect(10, 0, 0, 500);
    }
}

static bool MapResourceTextContains(const STRING& text, const char* fragment)
{
    return std::strstr(text.value, fragment) != 0;
}

static bool MapResourceTextEquals(const char* left, const char* right)
{
    while (*left == *right)
    {
        if (*left == 0)
            return true;
        ++left;
        ++right;
    }
    return false;
}

static void MapResourceBuildExternalPath(RESOURCE* resource, const STRING& source, STRING* output)
{
    STRING normalizedPath = resource->GetFileName().ToLower();
    STRING reducedDirectory;
    if (MapResourceTextContains(normalizedPath, ".map") && MapResourceTextContains(normalizedPath, ":\\"))
    {
        STRING normalizedSourceDirectory = source.BeforeLast("\\").ToLower();
        STRING mapDirectory = normalizedPath.BeforeLast("\\");
        reducedDirectory = mapDirectory.BeforeLast(normalizedSourceDirectory);
    }
    else
    {
        reducedDirectory = StringGlobals::EmptyText();
    }
    *output = reducedDirectory+source;
}

static void MapResourceAssignNumberAndName(VID* vid, unsigned int out32, const STRING& name)
{
    vid->nVid = static_cast<int>(out32);
    vid->name = name;
}

static void MapResourceAssignIdentity(VID* vid, unsigned int out32, const STRING& name, const STRING& source)
{
    MapResourceAssignNumberAndName(vid, out32, name);
    vid->vidName = source;
}

static void MapResourceLogLoadedVid(VID* vid)
{
    if ((vid->type&0x40u) != 0u)
    {
        VID_SURFACE* const surface = static_cast<VID_SURFACE*>(vid);
        const double maxGrid = static_cast<double>(vid->maxXGridZ);
        const double minGrid = static_cast<double>(vid->minXGridZ);
        const unsigned int storage = (vid->storageFlags>>3u)&1u;
        const unsigned int runtime = (vid->runtimeFlags>>7u)&1u;
        const int linked = vid->cadrLinkXYZ != 0 ? 1 : 0;
        const unsigned int mode = surface->surfaceMode;
        const unsigned int formatCode = surface->renderMode;
        const unsigned int state = surface->pixelFormat;
        STRING pixelFormat = GetPixelFormat(static_cast<_D3DFORMAT>(state));
        const int sizeY = static_cast<int>(vid->vidSizeY);
        const int sizeX = static_cast<int>(vid->vidSizeX);
        const unsigned int type = static_cast<unsigned int>(vid->type);
        const STRING numberName = vid->GetNumberName();
        const STRING withSpace = numberName+" ";
        const STRING withSource = withSpace+vid->vidName;
        const STRING format = withSource+" %i, %i, %x, %s fx=%i mesh=%i LinkDot=%i optActive=%i FastLoad=%i XGridZ[%f-%f]";
        Error->Log(format, sizeX, sizeY, type, pixelFormat.CharPtr(), formatCode, mode, linked, runtime, storage, minGrid, maxGrid);
    }
    else
    {
        const unsigned int sync = (vid->runtimeFlags>>12u)&1u;
        const unsigned int runtime = (vid->runtimeFlags>>7u)&1u;
        const unsigned int type = static_cast<unsigned int>(vid->type);
        const int sizeY = static_cast<int>(vid->vidSizeY);
        const int sizeX = static_cast<int>(vid->vidSizeX);
        const STRING numberName = vid->GetNumberName();
        const STRING withSpace = numberName+" ";
        const STRING withSource = withSpace+vid->vidName;
        const STRING format = withSource+" %i, %i, %x optActive=%i sync=%i";
        Error->Log(format, sizeX, sizeY, type, runtime, sync);
    }
}

VID* MAP::CreateVid(RESOURCE* resource, int nVid)
{
    STRING name;
    STRING source;
    VID baseVid;
    baseVid.noCadr = 32000;
    name.Read(resource);
    std::FILE* const file = resource->m_file;
    const unsigned int parameterOffset = static_cast<unsigned int>(std::ftell(file));
    baseVid.LoadParameters(resource);
    source.Read(resource);
    source = source.ToLower();
    resource->m_ioState = RESOURCE::IO_SEEK;
    (void)static_cast<unsigned int>(std::fseek(resource->m_file, BitsToSigned32(parameterOffset), static_cast<int>(0u)));
    VID* loaded = 0;
    const int objectCount = m_vidCount;
    for (int index = 0;index<objectCount;++index)
    {
        VID* const current = m_vids[static_cast<unsigned int>(index)];
        if (!current)
            continue;
        if (MapResourceTextEquals(current->vidName.value, source.value) && (current->type&0x4000u) == 0u)
        {
            loaded = current->CreateMirror();
            MapResourceAssignIdentity(loaded, nVid, name, source);
            loaded->LoadParameters(resource);
            break;
        }
    }
    if (!loaded)
    {
        if (!MapResourceTextContains(source, "."))
        {
            loaded = new(std::nothrow) VID_FONT(&baseVid, source);
            MapResourceAssignNumberAndName(loaded, nVid, name);
            loaded->LoadParameters(resource);
        }
        else if (MapResourceTextContains(source, ".fon") || MapResourceTextContains(source, ".ttf"))
        {
            return 0;
        }
        else if (MapResourceTextContains(source, ".jpg") || MapResourceTextContains(source, ".tiff") || MapResourceTextContains(source, ".gif") || MapResourceTextContains(source, ".png") || MapResourceTextContains(source, ".tga") || MapResourceTextContains(source, ".bmp"))
        {
            if (!MapLoadVidFileExists(source))
            {
                ::Error->Log("%i LOAD::Can't open file %s", nVid, source.CharPtr());
                return 0;
            }
            loaded = new(std::nothrow) VID_SURFACE(&baseVid, resource, source);
            MapResourceAssignNumberAndName(loaded, nVid, name);
        }
        else
        {
            STRING externalPath;
            MapResourceBuildExternalPath(resource, source, &externalPath);
            RESOURCE archiveStorage;
            RESOURCE* const archive = &archiveStorage;
            if (archive->OpenForRead(externalPath, ResourceType::Video) != 0)
            {
                ::Error->Log("LOAD::Can't open file %s", source.value);
                return 0;
            }
            if (archive->GoBegin(ResourceType::Header) != 0)
            {
                ::Error->Log("!!!ERROR!!!VID '%s': Load() not HEAD ", source.value);
            }
            (void)archive->Read(&baseVid.type, 2u);
            const unsigned short headFlags = baseVid.type;
            if ((headFlags&0x40u) != 0u)
            {
                loaded = new(std::nothrow) VID_SURFACE();
            }
            else if ((headFlags&0x80u) != 0u)
            {
                loaded = new(std::nothrow) VID_LIGHT();
            }
            else
            {
                loaded = new(std::nothrow) VID_SOFTWARE();
            }
            VID* const vid = loaded;
            vid->nVid = nVid;
            vid->type = headFlags;
            vid->name = name;
            vid->vidName = source;
            (void)archive->Read(&vid->frameSpeedDefault, 2u);
            (void)archive->Read(&vid->noCadr, 2u);
            (void)archive->Read(&vid->vidSizeX, 2u);
            (void)archive->Read(&vid->vidSizeY, 2u);
            vid->buildSizeX = static_cast<float>(vid->vidSizeX);
            vid->buildSizeY = static_cast<float>(vid->vidSizeY);
            vid->LoadParameters(resource);
            vid->Load(archive);
        }
    }
    loaded->SetLayer();
    MapResourceLogLoadedVid(loaded);
    return loaded;
}

static void __cdecl MapPresentationCallback(int mode)
{
    const ANGLE direction(mapLoadingDirection.value);
    VID* const vid = mapLoadingVid;
    const int result = Graph->DrawLoadBar(mapLoadingText, vid, direction, static_cast<int>(mode));
    if (result != 0)
        ::Sound->MusicTact();
}

void __cdecl DispatchMapPresentation(int mode)
{
    DrawLoadScreenTact(mode);
}

static unsigned int MapCtorBytesPerPixelOrBlock(unsigned int format)
{
    if (format == 0x31545844u)
        return 4u;
    if (format == 0x33545844u || format == 0x35545844u)
        return 8u;
    if (format == 0x29u)
        return 1u;
    if (format >= 0x14u && format <= 0x1Au)
    {
        const unsigned int formatIndex = format-0x14u;
        if (formatIndex == 0u)
            return 3u;
        if (formatIndex <= 2u)
            return 4u;
        return 2u;
    }
    return 0u;
}

static void MapCtorSerializeFastLoadEntry(VID* rawEntry, FSTREAM& indexFile, FSTREAM& dataFile, const FILETIME& cacheStamp)
{
    VID_SURFACE* const entry = static_cast<VID_SURFACE*>(rawEntry);
    if (!entry || (entry->type&0x40u) == 0u || (entry->surfaceFlags&0x02u) != 0u)
        return;
    STRING* const path = &entry->vidName;
    FILETIME creation, access, write;
    (void)FGetFileTime(*path, &creation, &access, &write);
    RESOURCE videoResource(RESOURCE::OPEN_MODE_APPEND, *path, ResourceType::Video);
    RESOURCE* const resource = &videoResource;
    if (static_cast<unsigned int>(resource->GoBegin(ResourceType::Header)) != 0u)
    {
        return;
    }
    std::FILE* const resourceFile = resource->FileHandle();
    const int current = static_cast<int>(static_cast<unsigned int>(std::ftell(resourceFile)));
    const int packedSize = static_cast<int>(resource->PackedSize());
    const int subRecordSize = static_cast<int>(static_cast<unsigned int>(resource->SubRecordSize()));
    if (subRecordSize-current+packedSize+4<34)
    {
        return;
    }
    const int entryKey = entry->nVid;
    VID* cursor = entry;
    if (entry->nextMirror != entry)
    {
        for (;;)
        {
            if (cursor->nVid<entryKey)
                break;
            cursor = cursor->nextMirror;
            if (cursor->nextMirror == entry)
                break;
        }
    }
    if (cursor->nVid<entryKey)
    {
        return;
    }
    MYERROR* const logger = ::Error;
    (void)logger->Log("fast load file %s", path->value);
    const unsigned int dataOffset = static_cast<unsigned int>(indexFile.Position());
    (void)resource->Shift(22);
    (void)resource->Write(&dataOffset, 4u);
    (void)resource->Write(&cacheStamp, sizeof(cacheStamp));
    resource->Close();
    (void)FSetFileTime(*path, &creation, &access, &write);
    if (!entry->frames)
    {
        return;
    }
    const int frameCount = entry->noCadr;
    if ((entry->type&0x0200u) != 0u)
        dataFile.Write(entry->cadrLinkXYZ, static_cast<unsigned int>(frameCount*12));
    if ((entry->type&0x0008u) != 0u)
        dataFile.Write(entry->palette, 1024u);
    VID* child = entry;
    if (entry->nextMirror != entry)
    {
        for (;;)
        {
            if ((child->property&0x20u) != 0u)
                break;
            child = child->nextMirror;
            if (child->nextMirror == entry)
                break;
        }
    }
    unsigned int childCountMarker = 0u;
    if ((child->property&0x20u) != 0u)
    {
        childCountMarker = static_cast<unsigned int>(child->noGridZ);
        if (childCountMarker != 0u)
        {
            dataFile.Write(child->gridZ, childCountMarker*12u);
            const int childFrameCount = child->noCadr;
            dataFile.Write(child->gridCadrShift, static_cast<unsigned int>(childFrameCount*4));
        }
    }
    dataFile.Write(&childCountMarker, 4u);
    unsigned int* const offsets = new(std::nothrow) unsigned int[static_cast<unsigned int>(frameCount)];
    const unsigned int pixelBase = PtrToU32(entry->surfaceData);
    const unsigned int format = entry->pixelFormat;
    for (int frame = 0;frame<frameCount;++frame)
    {
        CADR_DATA& cadr = entry->frames[frame];
        offsets[frame] = static_cast<unsigned int>(dataFile.Position());
        const unsigned int blockOffset = cadr.frameDataOffset;
        const unsigned int countA = cadr.indexCount;
        const unsigned int countB = cadr.vertexCount;
        dataFile.Write(U32ToPtr(pixelBase+blockOffset), countA*2u);
        dataFile.Write(U32ToPtr(pixelBase+blockOffset+countA*2u), countB*4u);
        dataFile.Write(U32ToPtr(pixelBase+blockOffset+(countA+countB*2u)*2u),
                       static_cast<unsigned int>(cadr.extraDataCount)*4u);
        const unsigned int rowCount = cadr.height;
        if (rowCount != 0u)
        {
            unsigned char* payload = U32ToPtr(pixelBase+blockOffset+(countA+countB*2u)*2u);
            if (format == 0x31545844u || format == 0x33545844u || format == 0x35545844u)
            {
                const unsigned int bits = (format == 0x31545844u)?4u:8u;
                const int width = static_cast<short>(cadr.width);
                const unsigned int bytes = static_cast<unsigned int>((width*static_cast<int>(rowCount)*static_cast<int>(bits))/8);
                dataFile.Write(payload, bytes);
            }
            else
            {
                const unsigned int unit = MapCtorBytesPerPixelOrBlock(format);
                unsigned char* scan = payload;
                while (ReadU16(scan, 0u) != 0u)
                {
                    dataFile.Write(scan, 2u);
                    ++scan;
                    unsigned int runs = ReadU8(scan, 0u);
                    ++scan;
                    for (unsigned int r = 0;r<runs;++r)
                    {
                        dataFile.Write(scan, unit);
                        scan += unit;
                        if ((entry->type&0x8000u) != 0u)
                            ++scan;
                        if ((entry->type&0x2000u) != 0u)
                            ++scan;
                        if ((entry->type&0x0004u) != 0u)
                            scan += 2u;
                    }
                }
                dataFile.Write(scan, 2u);
            }
        }
    }
    for (int frame = 0;frame<frameCount;++frame)
    {
        CADR_DATA& cadr = entry->frames[frame];
        cadr.CalcMaxMinZ(entry);
        indexFile.Write(&cadr.indexCount, sizeof(cadr.indexCount));
        if ((entry->type&0x0800u) != 0u)
            indexFile.Write(&cadr.extraDataCount, sizeof(cadr.extraDataCount));
        indexFile.Write(&cadr.vertexCount, sizeof(cadr.vertexCount));
        indexFile.Write(&cadr.width, sizeof(cadr.width));
        indexFile.Write(&cadr.height, sizeof(cadr.height));
        indexFile.Write(&cadr.offsetX, sizeof(cadr.offsetX));
        indexFile.Write(&cadr.offsetY, sizeof(cadr.offsetY));
        indexFile.Write(&offsets[frame], sizeof(offsets[frame]));
        indexFile.Write(&cadr.minZ, sizeof(cadr.minZ));
        indexFile.Write(&cadr.maxZ, sizeof(cadr.maxZ));
    }
    indexFile.Write(&entry->minXGridZ, sizeof(entry->minXGridZ));
    indexFile.Write(&entry->maxXGridZ, sizeof(entry->maxXGridZ));
    indexFile.Write(&entry->minYGridZ, sizeof(entry->minYGridZ));
    indexFile.Write(&entry->maxYGridZ, sizeof(entry->maxYGridZ));
    delete[] offsets;
}

MAP::MAP(HINSTANCE__* instance, HINSTANCE__* previous, const STRING& commandText, int show, GRAPH_INIT* init)
    : m_mouseTips()
{
    const unsigned int hInstance = PtrToU32(instance);
    const unsigned int hPrevInstance = PtrToU32(previous);
    const unsigned int nShowCmd = static_cast<unsigned int>(show);
    MAP* const map = this;
    const STRING* const command = &commandText;
    GRAPH_INIT* const graphInit = init;
    char executableBuffer[4096];
    executableBuffer[0] = 0;
    if (GetModuleFileNameA(0, executableBuffer, sizeof(executableBuffer)) == 0)
        executableBuffer[0] = 0;
    STRING executablePath(executableBuffer);
    STRING fileName = executablePath.AfterLast("\\");
    STRING baseName = fileName.BeforeLast(".");
    STRING configPath = FCurrentDirectory();
    configPath += "\\";
    configPath += baseName.CharPtr();
    configPath += ".cfg";
    PROFILE profile(configPath);
    if (std::strstr(command->CharPtr(), ".cfg") != 0)
    {
        STRING commandPath = FCurrentDirectory();
        commandPath += "\\";
        commandPath += command->CharPtr();
        profile.Load(commandPath);
    }
    if (::Error == 0)
    {
        STRING logDirectory("Logs");
        if (profile.GetInt(STRING("debug"), STRING("zLocalLogs"), 0) == 0)
        {
            logDirectory = WindowsUserPath;
            logDirectory += "Logs";
        }
        ::Error = new(std::nothrow) MYERROR(logDirectory.CharPtr(), 1, 0);
    }
    MYERROR* const logger = ::Error;
    logger->Log("WindowsUserPath is '%s'", WindowsUserPath.CharPtr());
    logger->Log("ASProtect user name %s", "NotAS");
    const bool createFastLoad = std::strstr(command->value, "createfastload") != 0;
    (void)timeBeginPeriod(1u);
    const unsigned int now = timeGetTime();
    ::RealCurrentTime = now;
    ::PrevRealCurrentTime = now - 10u;
    ANGLE::InitializeYTables();
    map->m_flags = (map->m_flags&0xFCD40003u)|0x00440000u;
    map->m_sizeXY.x = 640.0f;
    map->m_sizeXY.y = 480.0f;
    map->m_shiftXY.x = 0.0f;
    map->m_shiftXY.y = 1.0f;
    map->m_groundPrimary = 0;
    map->m_groundSecondary = 0;
    map->m_playerIndex = 0u;
    map->m_weaponData = 0;
    map->m_weaponCount = 0;
    map->m_vidCount = 0;
    map->m_simulationFrameCount = 0u;
    map->m_tick60 = ::CurrentTime;
    map->m_scale40 = 1.0f;
    map->m_scrollFollowSpeedScale = 1.0f;
    map->m_frameRate04 = 0u;
    map->m_frameCounter08 = 0u;
    map->m_scrollMode = 1u;
    map->m_cameraMinX = 0.0f;
    map->m_cameraMinY = 0.0f;
    map->m_cameraMaxX = 640.0f;
    map->m_cameraMaxY = 480.0f;
    map->m_fastLoadIndex = 0;
    map->m_fastLoadData = 0;
    for (unsigned int i = 0;i<static_cast<unsigned int>(VidSlotCount);++i)
        map->m_vids[i] = 0;
    for (unsigned int i = 0;i<8u;++i)
        map->m_players[i] = 0;
    map->m_hInstance = U32ToPtr(hInstance);
    map->ResetGroundZ();
    DrawLoadScreenTact = &MapPresentationCallback;

    MEMORYSTATUS memoryStatus;
    GlobalMemoryStatus(&memoryStatus);
    (void)logger->Log("Memory:: TotalPhys=%u AvailPhys=%u TotalPageFile=%u AvailPageFile=%u TotalVirtual=%u AvailVirtual=%u", memoryStatus.dwTotalPhys, memoryStatus.dwAvailPhys, memoryStatus.dwTotalPageFile, memoryStatus.dwAvailPageFile, memoryStatus.dwTotalVirtual, memoryStatus.dwAvailVirtual);

    const HRESULT comResult = CoInitialize(0);
    if (comResult<0)
    {
        Error(E_INIT, STRING("COM"), static_cast<unsigned int>(comResult));
        return;
    }
    map->m_comObject = 0;
    {
        STRING key("UniqueLog");
        STRING section("game");
        const unsigned int unique = static_cast<unsigned int>(profile.GetInt(section, key, 0));
        map->m_flags = (map->m_flags&~1u)|(unique&1u);
    }
    map->m_dialogFileName = profile.GetString(STRING("game"), STRING("Dialogs"), STRING(""));
    if (::g_pDlgSrv == 0)
    {
        ::g_pDlgSrv = new(std::nothrow) zDlgSrv();
    }
    ::g_pDlgSrv->LoadDialogs(map->m_dialogFileName);
    {
        STRING fxPath = FCurrentDirectory();
        fxPath += "\\vid\\12345.fx";
        ::Strings = new(std::nothrow) PROFILE(fxPath);
    }
    STRING title;
    title = profile.GetString(STRING("common"), STRING("Title"), STRING(baseName.CharPtr()));
    map->m_windowTitle = title;
    {
        STRING defaultReg("SOFTWARE\\SigmaTeam\\");
        defaultReg += baseName.value;
        STRING regPath;
        regPath = profile.GetString(STRING("common"), STRING("RegPath"), STRING(defaultReg.value));
        ::Registry = new(std::nothrow) REGISTRY(regPath);
    }
    REGISTRY* const registry = ::Registry;
    ::Net = new(std::nothrow) NET(instance, map->m_windowTitle);
    const HWND existing = FindWindowA(baseName.value, map->m_windowTitle.m_value);
    if (existing)
    {
        (void)SetForegroundWindow(existing);
        (void)ShowWindow(existing, 1);
        return;
    }
    map->m_playerIndex = 0u;
    STRING gamePath;
    gamePath = FCurrentDirectory();
    registry->SetString(STRING("GamePath"), gamePath);
    {
        STRING registryLog("Registry path = '");
        registryLog += registry->Path().value;
        registryLog += "'";
        logger->Log(registryLog);
    }
    {
        STRING key("VSync");
        STRING section("graph");
        const int value = profile.GetInt(section, key, 1);
        graphInit->flags |= value != 0?0x02u:0u;
    }
    {
        const int value = profile.GetInt(STRING("graph"), STRING("TripleBuffer"), 0);
        graphInit->flags |= value != 0 ? 0x20u : 0u;
    }
    {
        STRING key("VertexSoftware");
        STRING section("graph");
        const int value = profile.GetInt(section, key, 0);
        graphInit->flags |= value != 0?0x10u:0u;
    }
    {
        STRING key("SaveDemo");
        STRING section("game");
        const int value = profile.GetInt(section, key, 0);
        graphInit->flags |= value != 0?0x08u:0u;
    }
    {
        STRING key("StartDialogIsFull");
        STRING section("game");
        const int value = profile.GetInt(section, key, 0);
        graphInit->flags |= value != 0?0x04u:0u;
    }
    {
        const int value = profile.GetInt(STRING("debug"), STRING("zNoBuildToTerrain"), 0);
        graphInit->flags |= value != 0 ? 0x40u : 0u;
    }
    {
        graphInit->adapter = static_cast<unsigned int>(profile.GetInt(
            STRING("graph"), STRING("Device"), static_cast<int>(graphInit->adapter)));
    }
    {
        STRING key("DefaultScreenX");
        STRING section("graph");
        STRING registryKey("DefaultScreenX");
        const int configured = profile.GetInt(section, key, static_cast<int>(graphInit->width));
        graphInit->width = static_cast<unsigned int>(registry->GetInt(registryKey, configured));
    }
    {
        STRING key("DefaultScreenY");
        STRING section("graph");
        STRING registryKey("DefaultScreenY");
        const int configured = profile.GetInt(section, key, static_cast<int>(graphInit->height));
        graphInit->height = static_cast<unsigned int>(registry->GetInt(registryKey, configured));
    }
    {
        STRING key("DefaultColorBPP");
        STRING section("graph");
        STRING registryKey("DefaultColorBPP");
        const int configured = profile.GetInt(section, key, static_cast<int>(graphInit->colorBits));
        graphInit->colorBits = static_cast<unsigned int>(registry->GetInt(registryKey, configured));
    }
    {
        STRING key("FullScreen");
        const int configured = profile.GetInt(STRING("graph"), STRING("DefaultFullScreen"),
                                              static_cast<int>(graphInit->fullScreen));
        graphInit->fullScreen = static_cast<unsigned int>(registry->GetInt(key, configured));
    }
    const char* source = map->m_windowTitle.value;
    char* destination = graphInit->title;
    while ((*destination++ = *source++) != 0)
    {
    }
    Graph = new(std::nothrow) GRAPH(graphInit);
    GRAPH* const graph = Graph;
    zmTemp = profile.GetInt(STRING("debug"), STRING("zmTemp"), 0);
    if (profile.GetInt(STRING("game"), STRING("StartDialog"), 0) != 0)
    {
        const INT_PTR result = DialogBoxParamA(reinterpret_cast<HINSTANCE>(U32ToPtr(hInstance)), "START_DIALOG", 0, &AppStart, 0);
        if (result == 0u)
        {
            return;
        }
    }
    registry->SetInt(STRING("FullScreen"), (graph->capabilityFlags & 0x40u) != 0u);
    registry->SetInt(STRING("DefaultScreenX"), TruncateFloatToInt32(graph->SizeX()));
    registry->SetInt(STRING("DefaultScreenY"), TruncateFloatToInt32(graph->SizeY()));
    registry->SetInt(STRING("DefaultColorBPP"), (graph->capabilityFlags & 2u) != 0u ? 32 : 16);
    const bool noSystemMenu = profile.GetInt(STRING("debug"), STRING("zNoSysMenu"), 0) != 0;
    if (hPrevInstance == 0u)
    {
        WNDCLASSA windowClass;
        windowClass.hCursor = LoadCursorA(0, IDC_ARROW);
        windowClass.hIcon = LoadIconA(instance, "AppIcon");
        windowClass.lpszMenuName = noSystemMenu ? 0 : "AppMenu";
        windowClass.lpszClassName = baseName.CharPtr();
        windowClass.hbrBackground = reinterpret_cast<HBRUSH>(GetStockObject(4));
        windowClass.hInstance = instance;
        windowClass.style = 0x0Bu;
        windowClass.lpfnWndProc = &AppWndProc;
        windowClass.cbClsExtra = 0;
        windowClass.cbWndExtra = 0;
        if (RegisterClassA(&windowClass) == 0u)
        {
            return;
        }
    }
    int windowX = 0, windowY = 0;
    if ((graph->capabilityFlags&0x40u) == 0u)
    {
        windowX = registry->GetInt(STRING("WindowPositionX"), 0);
        windowY = registry->GetInt(STRING("WindowPositionY"), 0);
    }
    Map = map;
    unsigned int style = 0x90000000u;
    if ((graph->capabilityFlags&0x40u) == 0u && !noSystemMenu)
        style = 0x90CA0000u;
    const int height = TruncateFloatToInt32(graph->SizeY());
    const int width = TruncateFloatToInt32(graph->SizeX());
    const unsigned int hwnd = PtrToU32(CreateWindowExA(0x00040000u, baseName.value, map->m_windowTitle.m_value, style, windowX, windowY, width, height, 0, 0, reinterpret_cast<HINSTANCE>(U32ToPtr(hInstance)), 0));
    map->m_dialog = hwnd;
    (void)ShowWindow(reinterpret_cast<HWND>(U32ToPtr(hwnd)), static_cast<int>(nShowCmd));
    (void)UpdateWindow(reinterpret_cast<HWND>(U32ToPtr(hwnd)));
    map->m_dialogWindow = PtrToU32(LoadAcceleratorsA(reinterpret_cast<HINSTANCE>(U32ToPtr(hInstance)), "AppAccel"));
    (void)ShowCursor(0);
    if (graph->Init(reinterpret_cast<HWND__*>(U32ToPtr(hwnd))) != 0)
    {
        return;
    }
    (void)graph->SetGraphDetail(profile.GetInt(STRING("graph"), STRING("GraphDetail"), 10));
    {
        const int fontY = profile.GetInt(STRING("graph"), STRING("FontSizeY"), 8);
        const int fontX = profile.GetInt(STRING("graph"), STRING("FontSizeX"), 0);
        STRING font;
        font = profile.GetString(STRING("graph"), STRING("Font"), STRING("Arial"));
        graph->SetFont(font, fontX, fontY);
    }
    RESOURCE mapResource;
    {
        RESOURCE* const resource = &mapResource;
        {
            STRING resourceName;
            resourceName = profile.GetString(STRING("game"), STRING("Resource"), STRING("objects.res"));
            map->m_resourceFileName = resourceName;
        }
        if (static_cast<unsigned int>(resource->OpenForRead(map->m_resourceFileName, ResourceType::Data)) != 0u)
        {
            Error(E_OPEN, STRING("resource file"), 0);
            return;
        }
        {
            const int highQuality = profile.GetInt(STRING("sound"), STRING("SoundHighQuality"), 0);
            ::Sound = new(std::nothrow) SOUND(reinterpret_cast<HWND__*>(U32ToPtr(hwnd)), resource, highQuality);
            Const = new(std::nothrow) CONSTANT(resource);
            ::Const = Const;
            {
                STRING key("DebugMode");
                STRING section("game");
                Const->DebugMode = profile.GetInt(section, key, 0);
            }
            {
                STRING key("DrawFPS");
                STRING section("game");
                const unsigned int value = static_cast<unsigned int>(profile.GetInt(section, key, 0));
                map->m_flags = (map->m_flags&~FLAG_DRAW_FPS)|((value&1u)<<19u);
            }
            {
                STRING key("DrawPresentation");
                STRING section("game");
                const unsigned int value = static_cast<unsigned int>(profile.GetInt(section, key, 1));
                map->m_flags = (map->m_flags&~FLAG_SHOW_PRESENTATION)|((value&1u)<<20u);
            }
            {
                STRING key("DrawDebugInfo");
                STRING section("game");
                const unsigned int value = static_cast<unsigned int>(profile.GetInt(section, key, 0));
                map->m_flags = (map->m_flags&~FLAG_DRAW_DEBUG_INFO)|((value&1u)<<18u);
            }
            {
                const unsigned int value = static_cast<unsigned int>(profile.GetInt(
                    STRING("game"), STRING("DrawGammaIntersection"), Const->DebugMode));
                map->m_flags = (map->m_flags & ~FLAG_DRAW_GAMMA_INTERSECTION) | ((value & 1u) << 1u);
            }
        }
        (void)_controlfp(0x00020000u, 0x00030000u);
        map->SetScrollBox(VECTOR2(0, 0), map->m_sizeXY);
        map->SetShiftCoor(graph->SizeXY() / 2.0f, 0);
        {
            ::Sprites = new(std::nothrow) SPRITE_COLLECTOR(map->m_sizeXY);
        }
        if (createFastLoad)
        {
            (void)DeleteFileA("vid\\fast_load.vid");
            (void)DeleteFileA("vid\\fast_load_data.vid");
        }
        map->LoadVid(resource);
        (void)ShowCursor(1);
    }
    (void)SetCursor(0);
    const char* const emptyText = StringGlobals::EmptyText();
    STRING configuredLanguage;
    configuredLanguage = profile.GetString(STRING("common"), STRING("Language"), STRING(emptyText));
    bool languageChanged = false;
    {
        const char* languageCursor = configuredLanguage.value;
        const char* emptyLanguageCursor = emptyText;
        while (*languageCursor && *languageCursor == *emptyLanguageCursor)
        {
            ++languageCursor;
            ++emptyLanguageCursor;
        }
        languageChanged = static_cast<unsigned char>(*languageCursor) != static_cast<unsigned char>(*emptyLanguageCursor);
    }
    if (languageChanged)
    {
        STRING selectedLanguage;
        selectedLanguage = profile.GetString(STRING("common"), STRING("Language"), STRING("409"));
        map->SetKeyboardLocale(selectedLanguage);
    }
    {
        const VECTOR2& size = graph->SizeXY();
        const VECTOR position(size.x*0.5f, size.y*0.5f, 0.0f);
        Mouse = new(std::nothrow) MOUSE(::EmptyVid, position, ANGLE(0), 0);
        ::Mouse = Mouse;
        Mouse->Enable();
    }
    {
        STRING bossKey;
        bossKey = profile.GetString(STRING("game"), STRING("BossKey"), STRING(emptyText));
    }
    {
        STRING inputCfg("input.cfg");
        (void)zInputMap::Load(inputCfg.CharPtr());
        map->m_input.Init(reinterpret_cast<HWND__*>(AddressToUIntPtr(hwnd)));
    }
    for (unsigned int i = 0;i<8u;++i)
    {
        map->m_players[i] = new(std::nothrow) PLAYER_ARCADE(i);
    }
    map->m_cameraMinX = 0.0f;
    map->m_cameraMaxX = map->m_sizeXY.x;
    map->m_cameraMinY = 0.0f;
    map->m_cameraMaxY = map->m_sizeXY.y;
    if (command->value[0] && std::strstr(command->value, ".cfg") == 0 &&
        std::strstr(command->value, "secondexe") == 0)
        map->m_pendingMapFileName = *command;
    else
    {
        STRING startMap;
        startMap = profile.GetString(STRING("game"), STRING("StartMap"), STRING("maps\\logo.map"));
        map->m_pendingMapFileName = startMap;
    }
    if (createFastLoad)
    {
        FSTREAM indexFile("vid\\fast_load.vid", "wb+");
        FSTREAM dataFile("vid\\fast_load_data.vid", "wb+");
        FILETIME systemTime;
        GetSystemTimeAsFileTime(&systemTime);
        indexFile.Write(&systemTime, sizeof(systemTime));
        for (int i = 0; i < m_vidCount; ++i)
            MapCtorSerializeFastLoadEntry(m_vids[i], indexFile, dataFile, systemTime);
        return;
    }
    map->m_flags |= FLAG_INITIALIZED;
    (void)logger->Log("sizeof(SPRITE) = %i  sizeof(INPUT) = %i", 104u, 39u);
    return;
}

MAP::MAP()
    : m_mouseTips()
{
    (void)timeBeginPeriod(1u);
    ::RealCurrentTime = timeGetTime();
    ::PrevRealCurrentTime = ::RealCurrentTime-10u;
    ANGLE::InitializeYTables();
    m_flags = FLAG_DRAW_DEBUG_INFO|FLAG_MOUSE_TIPS;
    m_sizeXY = VECTOR2(640.0f, 480.0f);
    m_shiftXY = VECTOR2(0.0f, 1.0f);
    m_groundPrimary = 0;
    m_groundSecondary = 0;
    m_playerIndex = 0u;
    m_weaponData = 0;
    m_weaponCount = 0;
    m_vidCount = 0;
    m_simulationFrameCount = 0u;
    m_tick60 = ::CurrentTime;
    m_scale40 = 1.0f;
    m_frameRate04 = 0u;
    m_frameCounter08 = 0u;
    m_scrollMode = 1u;
    m_scrollFollowSpeedScale = 1.0f;
    m_fastLoadIndex = 0;
    m_fastLoadData = 0;
    for (unsigned int i = 0;i<VidSlotCount;++i)
        m_vids[i] = 0;
    for (unsigned int i = 0;i<PlayerSlotCount;++i)
        m_players[i] = new(std::nothrow) PLAYER(i);
    ResetGroundZ();
    if (::Error == 0)
        ::Error = new(std::nothrow) MYERROR("", 1, 1);
}

MAP::~MAP() noexcept(false)
{
    void* const self = this;
    MAP* const owner = this;
    MOUSETIPS* const mouseTips = &owner->m_mouseTips;
    mouseTips->Clear();
    if (::Sprites != 0)
        static_cast<MAP*>(self)->Release();
    const unsigned int comObject = PtrToU32(owner->m_comObject);
    if (comObject != 0u)
        (void)DeleteObject(reinterpret_cast<HGDIOBJ>(U32ToPtr(comObject)));
    if (SPRITE_COLLECTOR* const collector = ::Sprites)
    {
        delete collector;
        ::Sprites = 0;
    }
    if (Mouse)
    {
        MOUSE* const mouse = Mouse;
        delete mouse;
        Mouse = 0;
        ::Mouse = 0;
    }
    for (unsigned int i = 0u;i<8u;++i)
    {
        PLAYER* const player = owner->m_players[i];
        delete player;
    }
    if (zDlgSrv* const dialogs = ::g_pDlgSrv)
    {
        delete dialogs;
        ::g_pDlgSrv = 0;
    }
    if (NET* const network = ::Net)
    {
        delete network;
    }
    if (PROFILE* const profile = ::Strings)
    {
        delete profile;
    }
    if (SOUND* const sound = ::Sound)
    {
        delete sound;
    }
    if (CONSTANT* const constants = ::Const)
        delete constants;
    if (REGISTRY* const registry = ::Registry)
    {
        delete registry;
    }
    for (int index = owner->m_vidCount-1;index >= 0;--index)
    {
        VID* const vid = owner->m_vids[static_cast<unsigned int>(index)];
        if (vid)
        {
            delete vid;
            owner->m_vids[static_cast<unsigned int>(index)] = 0;
        }
    }
    owner->m_vidCount = 0;
    (void)::Error->Log("Vid    release (vid mem = %i)", VID::MemoryInUse);
    unsigned char* const mapped0 = owner->m_fastLoadIndex;
    if (mapped0)
        (void)UnmapViewOfFile(mapped0);
    unsigned char* const mapped1 = owner->m_fastLoadData;
    if (mapped1)
        (void)UnmapViewOfFile(mapped1);
    delete Graph;
    if (owner->m_groundPrimary)
        ::operator delete(owner->m_groundPrimary);
    if (owner->m_groundSecondary)
        ::operator delete(owner->m_groundSecondary);
    delete[] owner->m_weaponData;
    CoUninitialize();
    (void)timeEndPeriod(1u);
    if ((static_cast<unsigned char>(owner->m_flags)&1u) != 0u)
    {
        STRING message("end successful");
        ::Error->LogExit(message);
    }
    if (MYERROR* const logger = ::Error)
    {
        delete logger;
    }
}

void MAP::SetScrollBox(const VECTOR2& first, const VECTOR2& second)
{
    m_cameraMinX = first.x;
    m_cameraMaxX = second.x;
    m_cameraMinY = first.y;
    m_cameraMaxY = second.y;
}

int MAP::NextVid(int index, unsigned int type) const
{
    if (index < 0) index = 0;
    int next = index;
    for (;;)
    {
        if (++next >= m_vidCount) next = 0;
        if (next == index) return -1;
        if (ValidateVid(next) && (m_vids[next]->spriteType & type) &&
            m_vids[next]->type != 0)
            return next;
    }
}

int MAP::PrevVid(int index, unsigned int type) const
{
    if (index < 0) index = 0;
    int previous = index;
    for (;;)
    {
        if (--previous < 0) previous = m_vidCount - 1;
        if (previous == index) return -1;
        if (ValidateVid(previous) && (m_vids[previous]->spriteType & type) &&
            m_vids[previous]->type != 0)
            return previous;
    }
}

void MAP::ChangeSizeXY(const VECTOR2& size)
{
    m_sizeXY = size;
    SetScrollBox(VECTOR2(0, 0), m_sizeXY);
    ResetGroundZ();
}

STRING MAP::FileName() const
{
    return m_currentMapFileName;
}
