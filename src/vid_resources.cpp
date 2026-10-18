#include "vid.h"
#include "resread.h"
#include "map.h"
#include "sound.h"
#include "engine_globals.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <new>

namespace
{
bool EqualOrUnordered(float left, float right)
{
    return !(left < right) && !(left > right);
}

}

void VID::LoadParameters(RESOURCE* resource)
{
    (void)resource->Read(&spriteType, 4);
    (void)resource->Read(&spriteClass, 4);
    (void)resource->Read(&property, 4);
    (void)resource->Read(&moveMask, 4);
    if (nVid == 0) property |= 0x01000000u;

    sizeXYZ.Read(resource);
    (void)resource->Read(&maxHp, 4);
    (void)resource->Read(&maxSpeed, 4);
    (void)resource->Read(&randomSpeed, 4);
    (void)resource->Read(&maxZSpeed, 4);
    (void)resource->Read(&randomZSpeed, 4);
    (void)resource->Read(&acceleration, 4);
    (void)resource->Read(&slow, 4);
    (void)resource->Read(&rotationSpeed, 4);
    (void)resource->Read(&nWeapon, 4);
    (void)resource->Read(&deathRange, 4);
    (void)resource->Read(&deathDamageMin, 4);
    (void)resource->Read(&deathDamageMax, 4);
    if (deathDamageMax<deathDamageMin)
    {
        deathDamageMax = deathDamageMin;
        Error(E_INVALID, STRING("deathDamageMax < deathDamageMin"), 0);
    }
    (void)resource->Read(&deathPush, 4);
    deathPush /= 50.0f;

    linkXYZ.Read(resource);
    linkXYZ.y = -linkXYZ.y;
    (void)resource->Read(&nLinkVid, 4);
    (void)resource->Read(&topZ, 4);
    (void)resource->Read(&forMoveUpZ, 4);
    (void)resource->Read(&forMoveDownZ, 4);
    (void)resource->Read(&lifeTime, 4);

    resource->Shift(16);
    (void)resource->Read(&noDir, 4);
    (void)resource->Read(noAnimCadr, sizeof(noAnimCadr));
    (void)resource->Read(sfx, sizeof(sfx));
    (void)resource->Read(frameSpeed, sizeof(frameSpeed));
    (void)resource->Read(childX, sizeof(childX));
    (void)resource->Read(childY, sizeof(childY));
    (void)resource->Read(childZ, sizeof(childZ));
    (void)resource->Read(nChildVid, sizeof(nChildVid));
    (void)resource->Read(noChild, sizeof(noChild));

    int gammaRed;
    int gammaGreen;
    int gammaBlue;
    int gammaAlpha;
    (void)resource->Read(&gammaRed, 4);
    (void)resource->Read(&gammaGreen, 4);
    (void)resource->Read(&gammaBlue, 4);
    (void)resource->Read(&gammaAlpha, 4);

    gammaRaw = GAMMA(gammaAlpha, gammaRed, gammaGreen, gammaBlue);
    for (int player = 0;player<PlayerSlotCount;++player) gammaByPlayer[player] = gammaRaw;

    scaleXYZ.Read(resource);

    const float unlimited = 999999.0f;
    if (EqualOrUnordered(rotationSpeed, unlimited)) rotationSpeed = 0.0f;
    else if (EqualOrUnordered(rotationSpeed, 0.0f)) rotationSpeed = unlimited;
    else rotationSpeed = 256.0f/rotationSpeed;

    if (!EqualOrUnordered(maxSpeed, unlimited)) maxSpeed /= 1000.0f;
    if (!EqualOrUnordered(randomSpeed, unlimited)) randomSpeed /= 1000.0f;
    if (!EqualOrUnordered(maxZSpeed, unlimited)) maxZSpeed /= 1000.0f;
    if (!EqualOrUnordered(randomZSpeed, unlimited)) randomZSpeed /= 1000.0f;
    if (!EqualOrUnordered(acceleration, unlimited)) acceleration /= 1000000.0f;
    if (!EqualOrUnordered(slow, unlimited)) slow /= 1000000.0f;

    movementTactEnabled = (!EqualOrUnordered(maxSpeed, 0.0f) ||
                         !EqualOrUnordered(randomSpeed, 0.0f) ||
                         !EqualOrUnordered(maxZSpeed, 0.0f) ||
                         !EqualOrUnordered(randomZSpeed, 0.0f) ||
                         (property&0x06u) != 0u) ? 1 : 0;

    if (noDir == 0)
    {
        Error(E_INVALID, STRING("NoDir == 0"), 0);
        exit(1);
    }

    for (int slot = 0;slot<AnimationSlotCount;++slot)
        if (frameSpeed[slot] == 0) frameSpeed[slot] = static_cast<int>(frameSpeedDefault);

    realSizeXYZ.x = sizeXYZ.x;
    realSizeXYZ.y = sizeXYZ.y;
    realSizeXYZ.z = sizeXYZ.z;

    if ((property&0x100u) != 0u)
    {
        const float yAsX = sizeXYZ.y/0.7070602178573608f;
        if (!(yAsX >= sizeXYZ.x)) sizeXYZ.y = sizeXYZ.x*0.7070602178573608f;
        else sizeXYZ.x = static_cast<float>(yAsX);
    }
    halfSizeXY.x = sizeXYZ.x*0.5f;
    halfSizeXY.y = sizeXYZ.y*0.5f;

    if (noCadr == 0)
        Error(E_INVALID, STRING("noCadr == 0"), 0);
    else if (static_cast<int>(noCadr)<noDir && spriteClass != 19u)
    {
        Error(E_INVALID, STRING("noCadr < noDir"), 0);
        noDir = static_cast<int>(noCadr);
    }

    int requestedFrames = 0;
    for (int slot = 0;slot<AnimationSlotCount;++slot)
        requestedFrames = BitsToSigned32(static_cast<unsigned int>(requestedFrames)+
            static_cast<unsigned int>(noAnimCadr[slot])*static_cast<unsigned int>(noDir));

    if (requestedFrames>static_cast<int>(noCadr))
    {
        Error(E_MISSING, STRING("noCadr for noAnimCadr and noDir"), 0);
        for (int slot = AnimationSlotCount-1;slot >= 0;--slot)
        {
            const int withoutCurrent = BitsToSigned32(static_cast<unsigned int>(requestedFrames)-
                static_cast<unsigned int>(noAnimCadr[slot])*static_cast<unsigned int>(noDir));
            if (withoutCurrent>static_cast<int>(noCadr))
            {
                noAnimCadr[slot] = 0;
                requestedFrames = withoutCurrent;
                continue;
            }
            const int excess = BitsToSigned32(static_cast<unsigned int>(requestedFrames)-
                static_cast<unsigned int>(static_cast<int>(noCadr)));
            noAnimCadr[slot] = BitsToSigned32(static_cast<unsigned int>(noAnimCadr[slot])-
                static_cast<unsigned int>(excess/noDir));
            break;
        }
    }

    directionQuantizationOffset = 128/noDir;

    if ((spriteType == 32u || spriteClass == 10u) && moveMask == 0u && nWeapon == 0)
        runtimeFlags &= ~0x80u;

    int firstRealAnimation = -1;
    int totalFrames = 0;
    for (int slot = 0;slot<AnimationSlotCount;++slot)
    {
        const SOUND* const soundOwner =
            ::Sound;
        const int soundIndex = sfx[slot];
        const SFX* const soundTable = soundOwner->m_sfx;
        if (soundTable)
        {
            bool soundLoaded = false;
            if (soundIndex >= 0 && soundIndex <= soundOwner->m_sfxCount)
            {
                soundLoaded = soundTable[soundIndex].m_bufferCount != 0u;
            }
            if (!soundLoaded && nVid != -1)
            {
                {
                    STRING subject("sfx");
                    Error(E_INVALID, subject, static_cast<unsigned long>(sfx[slot]));
                }
                sfx[slot] = 0;
            }
        }

        const int animationFrames = noAnimCadr[slot];
        if (animationFrames != 0)
        {
            if (animationFrames != 1) runtimeFlags |= 0x80u;
            animationBaseFrame[slot] = totalFrames;
            animationFrameCount[slot] = animationFrames;
            if (firstRealAnimation<0)
            {
                firstRealAnimation = slot;
                for (int previous = 0;previous<slot;++previous)
                    if (animationFrameCount[previous] == 0)
                        animationFrameCount[previous] = animationFrames;
            }
        }
        else
        {
            bool copiedClass10Fallback = false;
            if (spriteClass == 10u && (slot&1) != 0 && slot <= 13 &&
               noAnimCadr[firstRealAnimation+1] != 0)
            {
                animationBaseFrame[slot] = animationBaseFrame[firstRealAnimation+1];
                animationFrameCount[slot] = animationFrameCount[firstRealAnimation+1];
                copiedClass10Fallback = true;
            }
            if (!copiedClass10Fallback)
            {
                animationBaseFrame[slot] = 0;
                animationFrameCount[slot] = firstRealAnimation < 0
                    ? animationBaseFrame[AnimationSlotCount - 1]
                    : animationFrameCount[firstRealAnimation];
            }
        }

        totalFrames = BitsToSigned32(static_cast<unsigned int>(totalFrames)+
            static_cast<unsigned int>(noAnimCadr[slot])*static_cast<unsigned int>(noDir));
        if (totalFrames>static_cast<int>(noCadr))
        {
            Error(E_ERROR, STRING("noCadr and noAnimCadr and noDir"), static_cast<unsigned long>(slot));
            animationBaseFrame[slot] = 0;
            animationFrameCount[slot] = firstRealAnimation < 0
                ? animationBaseFrame[AnimationSlotCount - 1]
                : animationFrameCount[firstRealAnimation];
        }
    }

    if ((property&0x08u) != 0u && noGridZ == 0)
    {
        minXGridZ = maxXGridZ = minYGridZ = maxYGridZ = 0.0f;

        const int widthExtent = static_cast<int>(sizeXYZ.x)+17;
        const int rowStride = widthExtent/8;
        const int heightExtent = static_cast<int>(sizeXYZ.y)+17;
        const int scratchCount = (heightExtent*rowStride)/8+1;
        VECTOR* const scratch = new(std::nothrow) VECTOR[scratchCount];

        noGridZ = 0;
        for (int y = 0;static_cast<float>(y)<sizeXYZ.y;y += 8)
        {
            for (int x = 0;static_cast<float>(x)<sizeXYZ.x;x += 8)
            {
                VECTOR& point = scratch[noGridZ++];
                point.x = static_cast<float>(x)-halfSizeXY.x;
                point.y = static_cast<float>(y)-halfSizeXY.y;
                point.z = sizeXYZ.z;
            }
            VECTOR& right = scratch[noGridZ++];
            right.x = halfSizeXY.x;
            right.y = static_cast<float>(y)-halfSizeXY.y;
            right.z = sizeXYZ.z;
        }
        for (int x = 0;static_cast<float>(x)<sizeXYZ.x;x += 8)
        {
            VECTOR& bottom = scratch[noGridZ++];
            bottom.x = static_cast<float>(x)-halfSizeXY.x;
            bottom.y = halfSizeXY.y;
            bottom.z = sizeXYZ.z;
        }
        VECTOR& corner = scratch[noGridZ++];
        corner.x = halfSizeXY.x;
        corner.y = halfSizeXY.y;
        corner.z = sizeXYZ.z;

        if (gridZ) delete[] gridZ;
        gridZ = new(std::nothrow) VECTOR[noGridZ];
        for (int index = 0;index<noGridZ;++index)
        {
            gridZ[index].x = scratch[index].x;
            gridZ[index].y = scratch[index].y;
            gridZ[index].z = scratch[index].z;
            if (gridZ[index].x<minXGridZ) minXGridZ = gridZ[index].x;
            if (gridZ[index].x>maxXGridZ) maxXGridZ = gridZ[index].x;
            if (gridZ[index].y<minYGridZ) minYGridZ = gridZ[index].y;
            if (gridZ[index].y>maxYGridZ) maxYGridZ = gridZ[index].y;
        }
        delete[] scratch;
    }
}

void VID::SetChildAndLink()
{
    if (nLinkVid != 0)
    {
        if (nLinkVid == nVid)
            Error(E_INVALID, STRING("LinkVid == nVid"), static_cast<unsigned long>(nLinkVid));
        else if (Map->ValidateVid(nLinkVid)) linkVid = Map->Vid(nLinkVid)->exchangedVid;
        else Error(E_INVALID, STRING("LinkVid"), static_cast<unsigned long>(nLinkVid));
    }

    const WEAPON_RUNTIME* const weaponState = weapon;
    if (weaponState->maxColumn >= 0)
    {
        for (int lane = 0; lane <= weaponState->maxColumn; ++lane)
        {
            if (weaponState->GammaR[lane] != 0 ||
               weaponState->GammaG[lane] != 0 ||
               weaponState->GammaB[lane] != 0 ||
               weaponState->GammaA[lane] != 0)
                runtimeFlags |= 0x01u;

            const VECTOR& lane0 = weaponState->Scale[lane];
            if (!EqualOrUnordered(lane0.x, 1.0f) ||
               !EqualOrUnordered(lane0.y, 1.0f) ||
               !EqualOrUnordered(lane0.z, 1.0f))
                runtimeFlags |= 0x02u;

            const VECTOR& lane1 = weaponState->Shift[lane];
            if (!EqualOrUnordered(lane1.x, 0.0f) ||
               !EqualOrUnordered(lane1.y, 0.0f) ||
               !EqualOrUnordered(lane1.z, 0.0f))
                runtimeFlags |= 0x04u;

            if (!EqualOrUnordered(weaponState->Speed[lane], 0.0f))
                runtimeFlags |= 0x10u;

            if (weaponState->DirectionX[lane] != 0 ||
               weaponState->DirectionY[lane] != 0 ||
               weaponState->DirectionZ[lane] != 0)
                runtimeFlags |= 0x08u;
        }
    }

    if (lifeTime != 999999 ||
       (property&0x00200000u) != 0u ||
       !EqualOrUnordered(maxSpeed, randomSpeed) ||
       noGridZ != 0 ||
       (property&0x00182000u) != 0u ||
       (runtimeFlags&0x1Fu) != 0u ||
       (nWeapon != 0 && childVid[8] != 0) ||
       (property&0x28u) != 0u)
        actionAuxStateRequired = 1;

    if ((spriteClass == 2u || spriteClass == 7u) && (noDir == 16 || noDir == 32))
    {
        runtimeFlags |= 0x200u;
        if (linkVid && (linkVid->noDir == 16 || linkVid->noDir == 32))
            linkVid->runtimeFlags |= 0x200u;

        for (int slot = 15; slot <= 16; ++slot)
            if (childVid[slot] && (childVid[slot]->noDir == 16 || childVid[slot]->noDir == 32))
                childVid[slot]->runtimeFlags |= 0x200u;
    }

    if (!EqualOrUnordered(weaponState->Weight, 999999.0f))
        movementTactEnabled |= 1;

    for (int slot = 0; slot<AnimationSlotCount; ++slot)
    {
        const int sourceChild = nChildVid[slot];
        if (sourceChild == 0) continue;

        const unsigned int childIndexBits = static_cast<unsigned int>(sourceChild);
        const unsigned int signMask = 0u-(childIndexBits>>31u);
        const int childIndex = static_cast<int>((childIndexBits^signMask)-signMask);
        if (Map->ValidateVid(childIndex))
        {
            VID* const resolved = Map->Vid(childIndex)->exchangedVid;
            childVid[slot] = resolved;
            if ((resolved->property&0x80u) != 0u || (resolved->property&0x00080000u) != 0u)
                actionAuxStateRequired = 1;
        }
        else
            Error(E_INVALID, STRING("child"), static_cast<unsigned long>(sourceChild));
    }

    runtimeFlags = (runtimeFlags&~0x100u)|((nWeapon != 0 && childVid[8] != 0)?0x100u:0u);
}
