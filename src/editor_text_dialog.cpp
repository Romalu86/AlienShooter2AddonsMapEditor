#include "map_edit.h"
#include "editor_dialogs.h"
#include "engine_globals.h"
#include <cstring>

namespace {
unsigned int textPropertyBehave;
int textPropertyResetActions;
}

INT_PTR CALLBACK AppTextProperty(HWND window,UINT message,WPARAM command,LPARAM data)
{
    return static_cast<MAP_EDIT*>(Map)->DialogTextProperty(window,message,command,data);
}

int MAP_EDIT::DialogTextProperty(HWND window,UINT message,WPARAM command,LPARAM)
{
    DIALOG_TEXT directionLabel(window,1065),text(window,1013),name(window,1109);
    DIALOG_UNSIGNED direction(window,1032),army(window,1031);
    DIALOG_INT x(window,1064),y(window,1066),z(window,1067);
    DIALOG_INT red(window,1007),green(window,1008),blue(window,1009),alpha(window,1016);
    DIALOG_FLOAT scaleX(window,1010),scaleY(window,1011),scaleZ(window,1012);
    DIALOG_COMBO_BOX alignX(window,1057),alignY(window,1058),replacement(window,1059);
    DIALOG_BUTTON flag200(window,2001);

    if (message==WM_INITDIALOG) {
        if (!selectedSprites.No()) { EndDialog(window,0); return 0; }
        SPRITE* const first=*selectedSprites[0];
        SendDlgItemMessageA(window,1090,UDM_SETRANGE32,0,3);
        SendDlgItemMessageA(window,1091,UDM_SETRANGE32,0,first->Vid()->noDir-1);
        const int colourSpinners[]={1106,1107,1108,1108,1111};
        for (int control:colourSpinners)
            SendDlgItemMessageA(window,control,UDM_SETRANGE32,static_cast<WPARAM>(-255),255);
        textPropertyResetActions=0;
        text=reinterpret_cast<const STRING*>(first->Action(121,0,0,0));
        const STRING originalName=first->GetName(); name=&originalName;
        textPropertyBehave=first->Behave();
        const VECTOR scale=first->GetScale();
        scaleX=scale.x; scaleY=scale.y; scaleZ=scale.z;
        const GAMMA gamma=first->Gamma();
        red=gamma.Red(); green=gamma.Green(); blue=gamma.Blue(); alpha=gamma.Alpha();
        x=TruncateFloatToInt32(first->X()); y=TruncateFloatToInt32(first->Y()); z=TruncateFloatToInt32(first->Z());
        direction=static_cast<unsigned int>(first->RealDirection());
        army=static_cast<unsigned int>(first->Army());
        const STRING label=Printf("Direction=%i",static_cast<int>(first->Direction().value));
        directionLabel=&label;
        const char* horizontal[]={"Left","Center","Right"};
        const char* vertical[]={"Top","Center","Bottom"};
        for (int i=0;i<3;++i) {
            const STRING horizontalLabel(horizontal[i]),verticalLabel(vertical[i]);
            alignX.AddString(&horizontalLabel); alignY.AddString(&verticalLabel);
        }
        alignX.SetCurrent((textPropertyBehave&1u)?1:(textPropertyBehave&2u)?2:0);
        alignY.SetCurrent((textPropertyBehave&8u)?1:(textPropertyBehave&4u)?2:0);
        const int fontButtons[]={1097,1092,1093,1094,1095,1096,1099};
        const unsigned int font=(textPropertyBehave&0x70u)>>4;
        if (font<7u) SendDlgItemMessageA(window,fontButtons[font],BM_SETCHECK,BST_CHECKED,0);
        flag200.SetCheck((textPropertyBehave&0x200u)!=0u);
        replacement.Reset();
        for (int i=0;i<m_vidCount;++i) {
            VID* const vid=m_vids[i];
            if (!vid || !first->IsSpriteClass(vid->spriteClass)) continue;
            const STRING replacementLabel=vid->GetNumberName();
            const int index=replacement.AddStringWithData(&replacementLabel,i);
            if (first->Vid()->nVid==i) replacement.SetCurrent(index);
        }
        return 1;
    }
    if (message!=WM_COMMAND) return 0;
    switch (LOWORD(command)) {
    case 1098: textPropertyResetActions=1; return 0;
    case 1097: textPropertyBehave&=~0x70u; return 0;
    case 1092: textPropertyBehave=(textPropertyBehave&~0x70u)|0x10u; return 0;
    case 1093: textPropertyBehave=(textPropertyBehave&~0x70u)|0x20u; return 0;
    case 1094: textPropertyBehave=(textPropertyBehave&~0x70u)|0x30u; return 0;
    case 1095: textPropertyBehave=(textPropertyBehave&~0x70u)|0x40u; return 0;
    case 1096: textPropertyBehave=(textPropertyBehave&~0x70u)|0x50u; return 0;
    case 1099: textPropertyBehave=(textPropertyBehave&~0x70u)|0x60u; return 0;
    case IDCANCEL: EndDialog(window,0); return 0;
    case IDOK: break;
    default: return 0;
    }

    SPRITE* const first=*selectedSprites[0];
    const STRING newName=name.GetText();
    const bool changeName=std::strcmp(first->GetName().CharPtr(),newName.CharPtr())!=0;
    const int replacementVid=replacement.GetCurrentStringData();
    const bool changeVid=replacementVid!=first->Vid()->nVid;
    const VECTOR newScale(static_cast<float>(scaleX),static_cast<float>(scaleY),static_cast<float>(scaleZ));
    const VECTOR originalScale=first->GetScale();
    const bool changeScale=originalScale!=newScale;
    const GAMMA newGamma(static_cast<int>(alpha),static_cast<int>(red),static_cast<int>(green),static_cast<int>(blue));
    const GAMMA originalGamma=first->Gamma();
    const bool changeGamma=originalGamma.modulate!=newGamma.modulate || originalGamma.add!=newGamma.add;
    textPropertyBehave&=0xFFFFFDF0u;
    const int horizontal=alignX.GetCurrentStringIndex(),vertical=alignY.GetCurrentStringIndex();
    if (horizontal==1) textPropertyBehave|=1u; else if (horizontal==2) textPropertyBehave|=2u;
    if (vertical==1) textPropertyBehave|=8u; else if (vertical==2) textPropertyBehave|=4u;
    if (flag200.GetCheck()) textPropertyBehave|=0x200u;
    const int newX=x,newY=y,newZ=z;
    const VECTOR newPosition(newX,newY,newZ);
    const unsigned int newDirection=direction,newArmy=army;

    for (int i=selectedSprites.No()-1;i>=0;--i) {
        SPRITE* const sprite=*selectedSprites[i];
        if (changeName) sprite->SetName(newName);
        if (changeVid) sprite->Action(62,replacementVid,0,0);
        if (textPropertyResetActions)
            for (int lane=0;lane<4;++lane) sprite->ResetActionStack(lane);
        if (first->RealDirection()!=static_cast<int>(newDirection)) sprite->ChangeRealDirection(newDirection);
        if (first->Army()!=static_cast<int>(newArmy)) sprite->Action(97,static_cast<int>(newArmy),0,0);
        if (TruncateFloatToInt32OrMin(first->X())!=newX || TruncateFloatToInt32OrMin(first->Y())!=newY || TruncateFloatToInt32OrMin(first->Z())!=newZ)
            sprite->ChangeCoor(sprite->Coor()+newPosition-first->Coor());
        if (first->Behave()!=textPropertyBehave) sprite->Action(95,static_cast<int>(textPropertyBehave),0,0);
        if (changeScale) sprite->SetScale(newScale);
        if (changeGamma) sprite->SetGamma(newGamma);
        const STRING newText=text.GetText();
        const STRING* const oldText=reinterpret_cast<const STRING*>(first->Action(121,0,0,0));
        if (std::strcmp(oldText->CharPtr(),newText.CharPtr())!=0 || first->Behave()!=textPropertyBehave) {

            STRING copy(newText);
            first->Action(120,static_cast<int>(reinterpret_cast<INT_PTR>(&copy)),0,0);
        }
    }
    EndDialog(window,1);
    return 0;
}
