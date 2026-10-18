#include "sprite.h"
#include <cstdlib>

namespace {
int AbsDifference(int left,int right)
{
    const int difference=BitsToSigned32(static_cast<unsigned int>(left)-static_cast<unsigned int>(right));
    return difference<0 ? BitsToSigned32(0u-static_cast<unsigned int>(difference)) : difference;
}
}

int SPRITE::AskCycleAction(int lane,int command,int x,int y,int) const
{
    if (!m_extraData) return 0;
    const LIST<ACT>& actions=m_extraData->actionLane[lane];
    if (actions.m_count && command==33 &&
        AbsFloat(static_cast<float>(static_cast<double>(x)-X()))<24.0f)
    {
        const float relativeY=static_cast<float>(static_cast<double>(y)-Y()+Z());
        if (AbsFloat(relativeY)<16.0f) return 1;
    }
    for (int index=1;index<actions.m_count;++index)
    {
        const ACT& action=actions.m_data[index];
        const int projectedY=BitsToSigned32(static_cast<unsigned int>(y)-static_cast<unsigned int>(action.arg2)+static_cast<unsigned int>(action.arg3));
        if ((action.command==33 || action.command==37) &&
            AbsDifference(x,action.arg1)<24 && AbsDifference(projectedY,0)<16) return 1;
    }
    return 0;
}

int SPRITE::AddCycleAction(int lane,int command,int x,int y,int z)
{
    if (!m_extraData) return 0;
    LIST<ACT>& actions=m_extraData->actionLane[lane];
    if (actions.m_count && actions.m_data[0].command==71) return 0;
    const int projected=BitsToSigned32(static_cast<unsigned int>(y)-static_cast<unsigned int>(z));
    if (actions.m_count && (command==33 || command==37) &&
        AbsFloat(static_cast<float>(static_cast<double>(x)-X()))<24.0f)
    {
        const float relativeY=static_cast<float>(static_cast<double>(projected)-Y()+Z());
        if (AbsFloat(relativeY)<16.0f)
        {
            AddAction(lane,command,TruncateFloatToInt32OrMin(X()),TruncateFloatToInt32OrMin(Y()),TruncateFloatToInt32OrMin(Z()));
            AddAction(lane,71,actions.m_count,0,0);
            return 1;
        }
    }
    for (int index=1;index<actions.m_count;++index)
    {
        const ACT action=actions.m_data[index];
        const int delta=BitsToSigned32(static_cast<unsigned int>(projected)-static_cast<unsigned int>(action.arg2)+static_cast<unsigned int>(action.arg3));
        if ((action.command==33 || action.command==37) && AbsDifference(x,action.arg1)<24 && AbsDifference(delta,0)<16)
        {
            AddAction(lane,command,action.arg1,action.arg2,action.arg3);
            AddAction(lane,71,index+1,0,0);
            return 1;
        }
    }
    AddAction(lane,command,x,y,z);
    return 0;
}
