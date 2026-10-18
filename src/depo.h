#pragma once

#include "unit.h"

class DEPO : public UNIT
{
public:

    enum { BuildQueueStorageCount = 100, InitialQueueEntries = 20 };

    DEPO(VID* vid,const VECTOR& position,ANGLE direction,SPRITE* parent=0);
    virtual ~DEPO() override;
    virtual void MoveTact();
    virtual int Action(int command,int argument1,int argument2,int argument3);

private:
    void AddUnitToQueue(int unitId);
    void BuildNextUnit();
    int ActionBuildUnit(int x,int y);

    unsigned int   m_depotSerial;
    unsigned short m_queueUnit[BuildQueueStorageCount];
    unsigned int   m_queueBuildTime[BuildQueueStorageCount];
    unsigned int   m_queueState[BuildQueueStorageCount];
    int            m_buildQueueIndex;
    int            m_buildQueueCapacity;
    int            m_buildQueueCount;
};
