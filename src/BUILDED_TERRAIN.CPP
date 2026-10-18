#include "builded_terrain.h"

BUILDED_TERRAIN::BUILDED_TERRAIN(VID* vid, const VECTOR& position,
    ANGLE direction, SPRITE* parent)
    : SPRITE(vid, position, direction, parent)
{
    if (Animation() < 15)
        (void)CreateChildFor(15, 0);
}

BUILDED_TERRAIN::~BUILDED_TERRAIN()
{
}
