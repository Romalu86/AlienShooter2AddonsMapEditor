#include "vid.h"
#include "region.h"
#include "engine_globals.h"
#include <cstdlib>
#include <new>

VECTOR2 VID::viewMin(0.0f, 1024.0f);
VECTOR2 VID::viewMax(0.0f, 1024.0f);
int VID::MemoryInUse = 0;
unsigned int VID::useTextureMemory = 0;
unsigned int VID::useVertexMemory = 0;
int VID::optDrawFlagmanBehindWalls = 0;
CADR_DATA* VID::lastUse = 0;

VID* EmptyVid = new (std::nothrow) VID;

VID::VID()
{
    treeItem = 0;
    spriteClass = 6u;
    spriteType = 0u;
    property = 0x01000000u;
    sizeXYZ = VECTOR(24, 16, 20);
    halfSizeXY = sizeXYZ.XY() / 2.0f;
    scaleXYZ = VECTOR(1, 1, 1);
    nextMirror = this;
    exchangedVid = this;
    type = 0u;
    layer = 13;
    runtimeFlags = 0x80u;
    storageFlags = 0x14u;
    nVid = -1;
    maxHp = 0;
    rotationSpeed = 0.0256f;
    noDir = 1;
    extendedHeaderValue = 1u;
    directionQuantizationOffset = 0;
    frameSpeedDefault = 71u;
    nLinkVid = 0;
    linkVid = 0;
    nWeapon = 0;
    weapon = 0;
    noGridZ = 0;
    gridZ = 0;
    gridCadrShift = 0;
    cadrLinkXYZ = 0;
    sourceVid = 0;
    lastRenderUseTick = 0u;
    minXGridZ = 0.0f;
    maxXGridZ = 0.0f;
    minYGridZ = 0.0f;
    maxYGridZ = 0.0f;
    actionAuxStateRequired = 0;
    movementTactEnabled = 0;
    notCreateAsChild = 0;
    for (int i = 0;i<AnimationSlotCount;++i)
    {
        sfx[i] = 0;
        nChildVid[i] = 0;
        childVid[i] = 0;
        noAnimCadr[i] = 0;
        animationBaseFrame[i] = 0;
        animationFrameCount[i] = 0;
        frameSpeed[i] = 71;
    }
    ResetSprites();
}

VID::~VID() noexcept(false)
{

    if (NoSprites() != 0)
    {
        const STRING message("Not all sprites with this VID deleted");
        Error(E_ERROR, message, static_cast<unsigned int>(NoSprites()));
    }

    if (nextMirror != this)
    {
        VID* predecessor = nextMirror;
        while (predecessor->nextMirror != this) predecessor = predecessor->nextMirror;
        predecessor->nextMirror = nextMirror;
    }
    else
    {
        if ((storageFlags&8u) == 0u && cadrLinkXYZ)
            delete[] cadrLinkXYZ;
        cadrLinkXYZ = 0;

        if ((storageFlags&8u) == 0u || (property&8u) != 0u)
        {
            if (gridZ)
            {
                delete[] gridZ;
                gridZ = 0;
            }
            if (gridCadrShift)
            {
                delete[] gridCadrShift;
                gridCadrShift = 0;
            }
        }
    }

}

void VID::ResetSprites()
{
    for (unsigned int i = 0;i<ScriptFunctionCount;++i)
        scriptFunction[i] = -1;

    for (int player = 0;player<PlayerSlotCount;++player)
    {
        reCapturedByPlayer[player] = 0;
        killedByPlayer[player] = 0;
        maxHpByPlayer[player] = maxHp;
        spriteCountsByPlayer[player] = 0u;
        spriteLimitByPlayer[player] = -1;
    }

    spriteLimit = -1;
    lastSpriteCountChangeTick = 0u;
    runtimeFlags &= ~0x20u;
}

VID* VID::CreateMirror()
{
    return new (std::nothrow) VID;
}

void VID::Draw(SPRITE*)
{
}

void VID::DrawShadow(const SPRITE*, const VECTOR&)
{
}

void VID::DrawOnlyZ(const SPRITE*)
{
}

void VID::InvalidateDeviceObjects()
{
}

void VID::RestoreDeviceObjects()
{
}

void VID::Load(RESOURCE*)
{
}

void VID::SetReColorForArmy(COLOR)
{
}

void VID::SetFramesToDevice()
{
}

void VID::SetLayer()
{
    layer = 0;
}

int VID::GetMaxHp(unsigned int player) const
{
    return maxHpByPlayer[player & 7u];
}

int VID::NoSprites() const
{
    unsigned int total = 0;
    for (int player = 0; player < PlayerSlotCount; ++player)
        total += spriteCountsByPlayer[player];
    return static_cast<int>(total);
}

int VID::NoSprites(int player) const
{
    return static_cast<int>(spriteCountsByPlayer[player]);
}

int VID::Killed(unsigned int player) const
{
    return killedByPlayer[player & 7u];
}

int VID::Killed() const
{
    int total = 0;
    for (int player = 0; player < PlayerSlotCount; ++player)
        total = AddSigned32Wrap(total, killedByPlayer[player]);
    return total;
}

int VID::ReCaptured(unsigned int player) const
{
    return reCapturedByPlayer[player & 7u];
}

int VID::ReCaptured() const
{
    int total = 0;
    for (int player = 0; player < PlayerSlotCount; ++player)
        total = AddSigned32Wrap(total, reCapturedByPlayer[player]);
    return total;
}

int VID::PropHide() const
{
    return static_cast<int>((runtimeFlags >> 11u) & 1u);
}

int VID::PropNotCreateAsChild() const
{
    return notCreateAsChild;
}

void VID::SetPropNotCreateAsChild(int value)
{
    notCreateAsChild = value;
}

int VID::PropBirthAsSmoke() const
{
    return static_cast<int>(spriteType & 0x80u);
}

int VID::CanFight() const
{
    return nWeapon != 0 && childVid[8] != 0;
}

int VID::RealDirection(unsigned char direction)
{
    const unsigned int wrapped =
        (static_cast<unsigned int>(directionQuantizationOffset) + direction) & 255u;
    return static_cast<int>((wrapped * static_cast<unsigned int>(noDir)) >> 8);
}

ANGLE VID::SteppedDirection(ANGLE direction) const
{
    if (noDir > 1 && (property & 0x20000000u) == 0)
    {
        const int frameDirection = const_cast<VID*>(this)->RealDirection(direction.value);
        const int frameAngle = static_cast<int>(static_cast<unsigned int>(frameDirection) << 8);
        return ANGLE(static_cast<unsigned char>(frameAngle / noDir));
    }
    return direction;
}

STRING VID::GetNumberName() const
{
    return Printf("%04i %s", nVid, name.CharPtr());
}

void VID::Error(TYPE_ERROR errorType, const STRING& subject, unsigned long code) const
{
    const STRING objectFormat("VID [%i-%s]");
    ::Error->Error(objectFormat, errorType, subject, code, nVid, name.CharPtr());
}

void VID::SetGamma(const GAMMA& gamma, int slot)
{
    if (slot<0)
    {
        gammaRaw = gamma;
        for (int i = 0;i<PlayerSlotCount;++i) gammaByPlayer[i] = gamma;
        return;
    }

    if (slot<PlayerSlotCount)
    {
        gammaByPlayer[slot] = gamma;
        return;
    }

    const STRING message("n_gamma in VID::SetGamma");
    Error(E_INVALID, message, static_cast<unsigned int>(slot));
}

int VID::GetMaxAmmo() const
{
    if (linkVid && linkVid->nWeapon && linkVid->childVid[8])
        return linkVid->weapon->MaxAmmo;
    return weapon->MaxAmmo;
}

int VID::GetBuildTime() const
{
    const VID* source = (linkVid && linkVid->nWeapon != 0)?linkVid:this;
    return static_cast<int>(source->weapon->BuildTime)/1000;
}

int VID::GetFireDamageMin() const
{
    const int linkedDamage = linkVid?linkVid->GetFireDamageMin():0;
    const int child15Damage = childVid[15]
        ? MultiplySigned32Wrap(childVid[15]->GetFireDamageMin(), noChild[15]):0;
    const int child14Damage = childVid[14]
        ? MultiplySigned32Wrap(childVid[14]->GetFireDamageMin(), noChild[14]):0;
    const int child8Damage = childVid[8]
        ? MultiplySigned32Wrap(childVid[8]->GetFireDamageMin(), noChild[8]):0;
    int result = AddSigned32Wrap(deathDamageMin, linkedDamage);
    result = AddSigned32Wrap(result, child15Damage);
    result = AddSigned32Wrap(result, child14Damage);
    return AddSigned32Wrap(result, child8Damage);
}

int VID::GetFireDamageMax() const
{
    const int linkedDamage = linkVid?linkVid->GetFireDamageMax():0;
    const int child15Damage = childVid[15]
        ? MultiplySigned32Wrap(childVid[15]->GetFireDamageMax(), noChild[15]):0;
    const int child14Damage = childVid[14]
        ? MultiplySigned32Wrap(childVid[14]->GetFireDamageMax(), noChild[14]):0;
    const int child8Damage = childVid[8]
        ? MultiplySigned32Wrap(childVid[8]->GetFireDamageMax(), noChild[8]):0;
    int result = AddSigned32Wrap(deathDamageMax, linkedDamage);
    result = AddSigned32Wrap(result, child15Damage);
    result = AddSigned32Wrap(result, child14Damage);
    return AddSigned32Wrap(result, child8Damage);
}

void VID::SetPropHideWithoutLink(int hide)
{
    runtimeFlags = (runtimeFlags&~0x00000800u)|(hide?0x00000800u:0u);
}

void VID::SetPropHide(int hide)
{
    runtimeFlags = (runtimeFlags&~0x00000800u)|(hide?0x00000800u:0u);
    if (linkVid)
        linkVid->SetPropHide(hide);
}

unsigned int VID::CalcImportantForSync()
{
    if ((runtimeFlags&0x1000u) != 0u) return 1u;

    if ((spriteType&0x20u) != 0u || spriteClass == 19u)
    {
        runtimeFlags &= ~0x1000u;
        return 0u;
    }

    if (spriteClass == 2u || spriteClass == 7u || spriteClass == 3u ||
       (spriteType&0x18u) != 0u || deathDamageMin != 0 || deathDamageMax != 0 ||
       (property&0x68u) != 0u)
    {
        runtimeFlags |= 0x1000u;
        return 1u;
    }

    if ((runtimeFlags&0x2000u) != 0u) return 0u;
    runtimeFlags |= 0x2000u;

    if (linkVid && linkVid->CalcImportantForSync() != 0u)
    {
        runtimeFlags = (runtimeFlags&~0x2000u)|0x1000u;
        return 1u;
    }

    for (int i = 0;i<AnimationSlotCount;++i)
    {
        if (childVid[i] && childVid[i]->CalcImportantForSync() != 0u)
        {
            runtimeFlags = (runtimeFlags&~0x2000u)|0x1000u;
            return 1u;
        }
    }

    runtimeFlags &= ~0x2000u;
    return (runtimeFlags>>12)&1u;
}
