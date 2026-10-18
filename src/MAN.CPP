#include "man.h"
#include "engine_globals.h"
#include "map.h"
#include "vid.h"

namespace
{

bool ManHasRotatingWeapon(const VID* vid)
{
    return vid->spriteClass == 7u && vid->linkVid &&
        (vid->linkVid->weapon->Property & 0x800u) != 0u;
}

unsigned int ManDirectionDifference(ANGLE first, ANGLE second)
{
    const unsigned char clockwise = static_cast<unsigned char>(first.value-second.value);
    const unsigned char anticlockwise = static_cast<unsigned char>(second.value-first.value);
    return clockwise < anticlockwise ? clockwise : anticlockwise;
}

bool ManPositionChanged(const VECTOR& first, const VECTOR& second)
{
    return first.x < second.x || first.x > second.x ||
        first.y < second.y || first.y > second.y ||
        first.z < second.z || first.z > second.z;
}
}

MAN::MAN(VID* vid, const VECTOR& position, ANGLE direction, SPRITE* parent)
    : UNIT(vid, position, direction, parent)
{
    InsertItem(261);
    for (int slot=2; slot<10; ++slot)
        m_weaponAmmo[slot] = 0;
}

MAN::~MAN()
{
}

void MAN::DetectTact()
{
}

void MAN::MoveTact()
{
    DialogTact();
    VECTOR candidate;
    const float previousZ = Z();
    MoveTactCalcCoor(&candidate);
    if (ManPositionChanged(candidate, Coor()) && Map->InMap(candidate.XY()))
    {
        if (!CanPlaceWithCrushAndGlide(&candidate))
            ChangeCoor(candidate);
        else
            m_runtimeFlags |= 0x1000u;
    }

    if (Goal() && IsCommand(1u))
    {
        const bool reverse = !(m_speed >= 0.0f);
        const ANGLE desired = DirectionTo(Goal()) +
            ANGLE(static_cast<unsigned char>(reverse ? 128u : 0u));
        RotateForGlide(desired, static_cast<int>(::CurrentTime-::PrevCurrentTime));

        if ((m_runtimeFlags & 0x03000000u) == 0x03000000u ||
            IsXYCross(Goal()->Vid(), Goal()->Coor(), Goal()->Direction()))
        {
            Stop();
        }
        else if (Z() < previousZ || Z() > previousZ)
        {
            const float difference = Z()-previousZ;
            Goal()->ChangeCoor(Goal()->Coor()+VECTOR(0.0f, difference, difference));
        }
    }

    if (!Link() || (Link()->Vid()->weapon->Property & 0x800u) == 0u ||
        Vid()->noAnimCadr[6] == 0)
        return;

    const bool moving = m_speed < 0.0f || m_speed > 0.0f;
    if ((moving || (m_runtimeFlags & 0x400u) != 0u) &&
        ManDirectionDifference(Direction(), Link()->Direction()) > 64u)
    {
        if (Animation() == 2 || Animation() == 3 || Animation() == 0)
            ChangeAnimation(6);
        return;
    }

    if ((m_runtimeFlags & 0x400u) != 0u)
        return;

    if (Animation() == 6 && AbsFloat(m_speed) < AbsFloat(MaxSpeed())/4.0f)
    {
        ChangeAnimation(0);
        ChangeDirection(Direction().GetInversed());
    }

    if (!(m_speed < 0.0f || m_speed > 0.0f))
    {
        if (m_turnState != 0)
        {
            const ANGLE turned = RotateTact(Link()->Direction(),
                static_cast<int>(::CurrentTime-::PrevCurrentTime));
            if (turned.value < 16u)
                m_turnState = 0;
        }
        else
        {
            m_turnState = ManDirectionDifference(Direction(), Link()->Direction()) > 55u;
        }
    }
}

int MAN::Action(int actionCode, int firstParameter, int secondParameter, int thirdParameter)
{
    switch (actionCode)
    {
    case ACT_COOR_ATTACK:
        if (Link() && Link()->Animation() < 15 && Link()->Vid()->CanFight() &&
            Link()->IsCommand(0u) && Link()->StateTimer() < 5000u)
        {
            const float height = static_cast<float>(thirdParameter);
            const VECTOR target(static_cast<float>(firstParameter),
                static_cast<float>(secondParameter)+height, height);
            const float distance = (target-SPRITE::Coor()).NearLength();
            if (!(distance > Link()->Vid()->weapon->BattleRange))
            {
                Link()->SetCommand(4, target);
                return 0;
            }
        }
        return 1;

    case ACT_NEXT_COMMAND:
        if (Animation() < 15)
            SetMoveAnimation();
        return 0;

    case ACT_GET_AMMO:
    case ACT_ADD_AMMO:
        return 1024;

    case ACT_CHANGE_VID:
        {
            const int slot = Link() ? Link()->Vid()->nVid-(Vid()->nVid+1) : 1;
            if (ManHasRotatingWeapon(Vid()))
                m_weaponAmmo[slot] = UNIT::Action(ACT_GET_AMMO, 0, 0, 0);

            UNIT::Action(actionCode, firstParameter, secondParameter, thirdParameter);
            if (!ManHasRotatingWeapon(Vid()))
            {
                const int maximum = Vid()->GetMaxAmmo();
                const int current = UNIT::Action(ACT_GET_AMMO, 0, 0, 0);
                UNIT::Action(ACT_ADD_AMMO, maximum-current, 0, 0);
            }
            else
            {
                if (Link())
                    Link()->Action(ACT_CHANGE_VID, Vid()->nVid+slot+1, 0, 0);
                const int current = UNIT::Action(ACT_GET_AMMO, 0, 0, 0);
                UNIT::Action(ACT_ADD_AMMO, m_weaponAmmo[slot]-current, 0, 0);
            }
            return 0;
        }

    default:
        return UNIT::Action(actionCode, firstParameter, secondParameter, thirdParameter);
    }
}
