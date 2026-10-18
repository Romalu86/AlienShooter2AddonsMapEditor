#include "map_edit.h"
#include "engine_globals.h"

void MAP_EDIT::LoadTerrain(STRING filename)
{
    RESOURCE video;
    filename.Replace(FCurrentDirectory().CharPtr(),"");
    filename.RemoveBeginChars("\\");
    if (video.OpenForRead(filename,ResourceType::Video)) return;
    video.Shift(4);
    int directions=0;
    video.Read(&directions,2);
    video.Close();
    STRING ini;
    if (!ini.LoadFile(STRING("mapedit.ini"))) {
        ::Error->Window(STRING("Can't load file mapedit.ini"));
        return;
    }
    STRING tail=ini.After("NoDir = ");
    tail.RemoveBeginChars(" 0123456789");
    ini=ini.Before("NoDir = ")+"NoDir = "+Int2Str(directions)+tail;
    ini=ini.Before("VidName = ")+"VidName = "+filename;
    ini.SaveFile(STRING("mapedit.ini"));
    DeleteExtraVid();
    Save(STRING("tmp.map"));
    RESOURCE temporary(RESOURCE::OPEN_MODE_APPEND,STRING("tmp.map"),ResourceType::Map);
    if (temporary.IsOpen()) temporary.LoadIni(STRING("mapedit.ini"));
    temporary.Close();
    const STRING originalName=m_currentMapFileName;
    Load(STRING("tmp.map"));
    editFileName=originalName;
    DrawMapName();

    int index=0;
    for (;index<m_vidCount;++index)
        if (m_vids[index] && (m_vids[index]->storageFlags&VID::STORAGE_EMBEDDED)) break;
    if (index>=m_vidCount) { Error(E_MISSING,STRING("terrain vid"),0); return; }
    VID* const terrain=Vid(index);
    if (directions>1) {
        for (int direction=0;direction<directions;++direction) {
            CreateSprite(terrain,VECTOR(
                (direction%((TruncateFloatToInt32OrMin(SizeXY().x)-1)/256+1))*256+128,
                (direction/((TruncateFloatToInt32OrMin(SizeXY().x)-1)/256+1))*256+128,0),
                ANGLE(static_cast<unsigned char>((direction*256)/directions)),0);
        }
    } else {
        CreateSprite(terrain,VECTOR(SizeXY()/2.0f,0.0f),ANGLE(static_cast<unsigned char>(0)),0);
    }
}
