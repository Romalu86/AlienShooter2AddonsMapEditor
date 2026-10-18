#include "map_edit.h"

SPRITE* MAP_EDIT::CreateSprite(VID* vid, const VECTOR& position,
                               ANGLE direction, SPRITE* parent)
{
    return MAP::CreateSprite(vid, position, direction, parent);
}
