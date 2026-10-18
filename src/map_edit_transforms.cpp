#include "map_edit.h"

void MAP_EDIT::ShiftActionCoordinates(SPRITE* sprite,const VECTOR& offset)
{
    for (unsigned int lane=0;lane<4;++lane)
        for (int index=0;index<sprite->ActionStack(static_cast<int>(lane)).No();++index)
        {
            ACT* const action=sprite->ActionItem(lane,index);
            switch (action->command)
            {
            case 33: case 44: case 46: case 63:
            {
                const VECTOR point=VECTOR(action->arg1,action->arg2,action->arg3)+offset;
                action->arg1=TruncateFloatToInt32OrMin(point.x);
                action->arg2=TruncateFloatToInt32OrMin(point.y);
                action->arg3=TruncateFloatToInt32OrMin(point.z);
                break;
            }
            case 37: case 43:
            {
                const VECTOR2 point=VECTOR2(action->arg1,action->arg2)+VECTOR2(offset.x,offset.y);
                action->arg1=TruncateFloatToInt32OrMin(point.x);
                action->arg2=TruncateFloatToInt32OrMin(point.y);
                break;
            }
            }
        }
}
