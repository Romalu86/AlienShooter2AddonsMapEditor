#include "map_edit.h"
#include "engine_globals.h"
#include "editor_dialogs.h"
#include "sprite.h"
#include "sprite_collector.h"
#include "vid.h"
#include "mouse.h"
#include "graph.h"
#include "registry.h"
#include <cstdio>
#include <cstring>

namespace {
ACT editAction={0,0,0,0};
int copyBuildCoordinates=0;
int dialogTextNumber=0;
int dialogTextSubNumber=0;
int editedNumber=0;
unsigned int logicalDirection=0;
unsigned int realDirection=0;
int selectedLane=0;
int itemFilter=0;
int namePending=0;

const int actionCommands[50]={
    33,44,46,32,37,45,35,134,39,38,41,40,73,71,72,79,77,78,43,131,
    136,115,116,95,88,113,98,104,62,85,91,63,135,
    0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16
};

MAP_EDIT* Editor() { return static_cast<MAP_EDIT*>(Map); }
HINSTANCE EditorInstance() { return Editor()->Instance(); }
SPRITE* FirstSelected(MAP_EDIT* editor) {
    return editor->selectedSprites.m_count ? editor->selectedSprites.m_data[0] : 0;
}
}

ACT* EditorStackSearchAction() { return &editAction; }

STRING ActionToText(int command)
{
    switch (command) {
    case 33: return STRING("ACT_MOVE");
    case 45: return STRING("ACT_FOLLOW");
    case 46: return STRING("ACT_STAND_FIRM");
    case 32: return STRING("ACT_ATTACK");
    case 37: return STRING("ACT_COOR_ATTACK");
    case 35: return STRING("ACT_BUILD_UNIT");
    case 134: return STRING("ACT_DESTROY_UNIT");
    case 39: return STRING("ACT_STOP");
    case 38: return STRING("ACT_RANDOM");
    case 36: return STRING("ACT_PATROL");
    case 34: return STRING("ACT_MOVE_TO");
    case 44: return STRING("ACT_MOVE_ACCURATELY");
    case 41: return STRING("ACT_ROTATE");
    case 40: return STRING("ACT_PAUSE");
    case 85: return STRING("ACT_DAMAGE");
    case 102: return STRING("ACT_SET_LINK");
    case 104: return STRING("ACT_SET_UPLINK");
    case 88: return STRING("ACT_SET_HP");
    case 98: return STRING("ACT_SET_INVISIBLE");
    case 91: return STRING("ACT_SET_GOAL_FOR_TELEPORT");
    case 113: return STRING("ACT_SET_DEATH_TIMER");
    case 95: return STRING("ACT_SET_BEHAVE");
    case 73: return STRING("ACT_STOP_STACK");
    case 71: return STRING("ACT_GOTO_STACK");
    case 72: return STRING("ACT_CLEAR_STACK");
    case 77: return STRING("ACT_WHILE_NOT_QUEST");
    case 79: return STRING("ACT_WHILE_NOT_SCRIPT_VAR");
    case 78: return STRING("ACT_SET_QUEST");
    case 43: return STRING("ACT_FLAGMAN_TRIGGER");
    case 131: return STRING("ACT_SCRIPT_VAR");
    case 136: return STRING("ACT_SCRIPT_EVENT");
    case 115: return STRING("ACT_SET_DIALOG");
    case 116: return STRING("ACT_SET_FLAGMAN_DIALOG");
    case 63: return STRING("ACT_CHANGE_COOR");
    case 62: return STRING("ACT_CHANGE_VID");
    case 135: return STRING("ACT_PLAY_SFX");
    case 0: return STRING("ANI_STAND");
    case 1: return STRING("ANI_STOP_MOVE/ANI_BUILD");
    case 2: return STRING("ANI_GO");
    case 3: return STRING("ANI_START_MOVE");
    case 4: return STRING("ANI_L_ROTATE");
    case 5: return STRING("ANI_R_ROTATE");
    case 6: return STRING("ANI_OPEN");
    case 7: return STRING("ANI_HIT");
    case 8: return STRING("ANI_FIGHT");
    case 9: return STRING("ANI_SALUT");
    case 10: return STRING("ANI_STAND_OPEN");
    case 11: return STRING("ANI_CLASH_VERT");
    case 12: return STRING("ANI_LAZY/ANI_CLASH");
    case 13: return STRING("ANI_WOUND");
    case 14: return STRING("ANI_BIRTH");
    case 15: return STRING("ANI_DEATH");
    case 16: return STRING("ANI_DEATH2");
    default: return Printf("UNKNOWN%i",command);
    }
}

STRING ActionComment(int command)
{
    switch (command) {

    case 33: return STRING("var1-x,var2-y,var3-z");

    case 44: return STRING("прибыть абсолютно точно в точку назначения");

    case 45: return STRING("преследовать спрайт var1, если var1==0 то следовать за Flagman(0)");

    case 46: return STRING("удерживать координаты (var1, var2, var3), всегда возвращаться в них при случайном смещении");

    case 32: return STRING("атака (SPRITE*)var1");

    case 37: return STRING("одиночный выстрел по координатам var1,var2");

    case 35: return STRING("var1-nvid for building unit (если 0 то один из items), var2,var3 -координаты где родится юнит, если 0 то родится в координатах родителя, если <0 то родится с случайным смещением от родителя");

    case 134: return STRING("var1-nvid уничтожаемого объекта (если -1 то убивает себя), var2,var3 -координаты где уничтожить, если (0,0) то где угодно");

    case 39: return STRING("прекратить движение, если var1 не равно 0, то немедленно затормозить, если var2 не равно 0 то прервать выполнение любой команды");

    case 38: return STRING("выполнить либо лэзи, либо просто повернуть голову, либо ничего не сделать");

    case 36: return STRING("?пока не поддерживается?");

    case 34: return STRING("var1-(SPRITE*)goal");

    case 41: return STRING("повернуться в направлении var1");

    case 40: return STRING("ничего не делать var1 + Random(var2) ms, если var3=1, то пауза кончается вместе с диалогом произносимым юнитом");

    case 85: return STRING("var1-damage,var2-unit вызвавший damage");

    case 102: return STRING("прилинковывает var1");

    case 104: return STRING("прилинковывает к var1 текущий спрайт");

    case 88: return STRING("var1 - новое количество hp, var2 - количество hp в процентах от максимума(если var1==0)");

    case 98: return STRING("var1 -  1-сделать невидимым, 0-сделать видимым, var2 - 1-проницаемый(не сможет находиться, повреждаться от взрывов, мешаться движению), 0-непроницаемый");

    case 91: return STRING("создается новая goal для перемещения с x=var1, y=var2, z=var3");

    case 95: return STRING("сменить поведение юнита var1 - aggressive, var2 - active");

    case 113: return STRING("устанавливает время через которое спрайт умрет, если 999999, то выключить таймер");

    case 73: return STRING("данная команда останавливает выполнение дальнейших команд в стэке (нужно для building)");

    case 71: return STRING("переход в стэке комманд, var1-номер команды к которой нужно перейти");

    case 72: return STRING("очистить actionStack, var1 - номер стека который надо очистить");

    case 131: return STRING("устанавливает в скриптах переменную Action#var1 в значение var2, если var3 = 1, то var2 прибавляется к предыдущему значению Action#var1");

    case 136: return STRING("вызывает скриптовую функцию с параметрами ScriptEvent#var1(this, var2, var3)");

    case 78: return STRING("меняет состояние квеста var1 подквеста var2 в var3: 0 - неактивен, 1 - в процессе, 2 - выполнен, 3 - провален");

    case 77: return STRING("не идет дальше пока состояние квеста var1 подквеста var2 не станет var3: 0 - неактивен, 1 - в процессе, 2 - выполнен, 3 - провален");

    case 79: return STRING("не идет дальше пока переменная ActionN, где N - var1 не станет равной var2");

    case 43: return STRING("не продолжать выполнять команды стека пока Flagman(0)->NearestDistance(var1,var2) > var3");

    case 115: return STRING("устанавливает для данного спрайта режим проигрыания диалога номер var1_var2");

    case 116: return STRING("устанавливает для управляемого игроком спрайта режим проигрыания диалога номер var1");

    case 63: return STRING("var1-x,var2-y,var3-z");

    case 62: return STRING("изменить текущий vid для спрайта, var1- new vid, var2 - new animation, if var2==-1 animation not changed");

    case 135: return STRING("var1 - номер играемого sfx");

    case 0: return STRING("нормальное состояние объекта, если var1 не равно 0, то var1 - nvid уничтожаемого объекта, var2,var3 -координаты где уничтожить, если (0,0) то где угодно");

    case 1: return STRING("процесс остановки движения/строительства дочернего юнита, если var1 не равно 0, то var1 - nvid уничтожаемого объекта, var2,var3 -координаты где уничтожить, если (0,0) то где угодно");

    case 2: return STRING("анимация движения юнита, если var1 не равно 0, то var1 - nvid уничтожаемого объекта, var2,var3 -координаты где уничтожить, если (0,0) то где угодно");

    case 3: return STRING("процесс начала движения юнита, если var1 не равно 0, то var1 - nvid уничтожаемого объекта, var2,var3 -координаты где уничтожить, если (0,0) то где угодно");

    case 4: return STRING("ANI_L_ROTATE, если var1 не равно 0, то var1 - nvid уничтожаемого объекта, var2,var3 -координаты где уничтожить, если (0,0) то где угодно");

    case 5: return STRING("ANI_R_ROTATE, если var1 не равно 0, то var1 - nvid уничтожаемого объекта, var2,var3 -координаты где уничтожить, если (0,0) то где угодно");

    case 6: return STRING("ANI_OPEN, если var1 не равно 0, то var1 - nvid уничтожаемого объекта, var2,var3 -координаты где уничтожить, если (0,0) то где угодно");

    case 7: return STRING("объект поврежден(ударен), если var1 не равно 0, то var1 - nvid уничтожаемого объекта, var2,var3 -координаты где уничтожить, если (0,0) то где угодно");

    case 8: return STRING("юнит атакует кого либо, если var1 не равно 0, то var1 - nvid уничтожаемого объекта, var2,var3 -координаты где уничтожить, если (0,0) то где угодно");

    case 9: return STRING("игрок кликнул на юнита, если var1 не равно 0, то var1 - nvid уничтожаемого объекта, var2,var3 -координаты где уничтожить, если (0,0) то где угодно");

    case 10: return STRING("из этого состояния объект не будет переходить автоматически в ANI_STAND как из других, если var1 не равно 0, то var1 - nvid уничтожаемого объекта, var2,var3 -координаты где уничтожить, если (0,0) то где угодно");

    case 11: return STRING("столкновение с вертикальной стенкой, если var1 не равно 0, то var1 - nvid уничтожаемого объекта, var2,var3 -координаты где уничтожить, если (0,0) то где угодно");

    case 12: return STRING("периодически вызываемая анимация на ничегониделание для B_CREATURE/столкновение с землей, если var1 не равно 0, то var1 - nvid уничтожаемого объекта, var2,var3 -координаты где уничтожить, если (0,0) то где угодно");

    case 13: return STRING("с юнита снята половина Hp, если var1 не равно 0, то var1 - nvid уничтожаемого объекта, var2,var3 -координаты где уничтожить, если (0,0) то где угодно");

    case 14: return STRING("вызывается при рождении объекта, если var1 не равно 0, то var1 - nvid уничтожаемого объекта, var2,var3 -координаты где уничтожить, если (0,0) то где угодно");

    case 15: return STRING("смерть юнита, если var1 не равно 0, то var1 - nvid уничтожаемого объекта, var2,var3 -координаты где уничтожить, если (0,0) то где угодно");

    case 16: return STRING("смерть от повреждений больших половины maxHp за раз, если var1 не равно 0, то var1 - nvid уничтожаемого объекта, var2,var3 -координаты где уничтожить, если (0,0) то где угодно");
    default: return STRING("неизвестное действие");
    }
}

namespace {

void StackDeletedLine(LIST<ACT>& stack,int index)
{
    for (int i=stack.m_count-1;i>=0;--i)
        if (stack.m_data[i].command==71 && stack.m_data[i].arg1>index)
            --stack.m_data[i].arg1;
}
void StackInsertedLine(LIST<ACT>& stack,int index)
{
    for (int i=stack.m_count-1;i>=0;--i)
        if (stack.m_data[i].command==71 && stack.m_data[i].arg1>index)
            ++stack.m_data[i].arg1;
}

void StackToListBox(DIALOG_LIST_BOX& list,const LIST<ACT>& stack,int shift)
{
    LIST<int> selected;
    const int top=list.Message(LB_GETTOPINDEX,0,0);
    for (int i=0;i<list.NoString();++i) selected.Insert(list.IsSelected(i));
    list.Reset();
    for (int i=stack.m_count-1;i>=0;--i) {
        const ACT& action=stack.m_data[i];
        const STRING name=ActionToText(action.command);
        const int arg1=action.command==95 ? action.arg1&1 : action.arg1;
        const int arg2=action.command==95 ? ((action.arg1&2)!=0) : action.arg2;
        const STRING row=Printf("%3i: %-18s %5i %5i %5i",i,name.CharPtr(),arg1,arg2,action.arg3);
        list.AddStringWithData(&row,i);
    }
    const STRING blank("");
    list.AddStringWithData(&blank,-1);
    if (shift!=-999) {
        for (int i=0;i<list.NoString();++i)
            if (i<selected.m_count && selected.m_data[i]) list.SetSelected(i+shift);
    }
    if (top<0) list.Message(LB_SETTOPINDEX,0,0);
    else if (top>=stack.m_count) list.Message(LB_SETTOPINDEX,stack.m_count-1,0);
    else list.Message(LB_SETTOPINDEX,top,0);
}

void FillActionCombo(DIALOG_COMBO_BOX& combo,int command)
{
    for (int i=0;i<50;++i) {
        const STRING name=ActionToText(actionCommands[i]);
        combo.AddStringWithData(&name,actionCommands[i]);
        if (actionCommands[i]==command) combo.SetCurrent(i);
    }
}
void ShowActionArguments(const ACT& action,DIALOG_INT& first,DIALOG_INT& second,DIALOG_INT& third)
{
    if (action.command==95) {
        first=(action.arg1&1)!=0;
        second=(action.arg1&2)!=0;
    } else {
        first=action.arg1;
        second=action.arg2;
    }
    third=action.arg3;
}
void ReadActionArguments(ACT& action,DIALOG_COMBO_BOX& combo,DIALOG_INT& first,DIALOG_INT& second,DIALOG_INT& third)
{
    action.command=combo.GetCurrentStringData();
    if (action.command==95)
        action.arg1=(static_cast<int>(first)!=0 ? 1 : 0)|(static_cast<int>(second)!=0 ? 2 : 0);
    else {
        action.arg1=first;
        action.arg2=second;
    }
    action.arg3=third;
}
void ShowDialogTextButton(DIALOG_BUTTON& button,int command)
{
    if (command==115 || command==116) button.Show();
    else button.Hide();
}
void PasteThreeCoordinates(DIALOG_INT& first,DIALOG_INT& second,DIALOG_INT& third)
{
    STRING clipboard;
    clipboard.ReadFromClipboard(0);
    int x,y,z;
    std::sscanf(clipboard.CharPtr(),"%i,%i,%i",&x,&y,&z);
    first=x; second=y; third=z;
}
void PasteTwoCoordinates(DIALOG_INT& first,DIALOG_INT& second)
{
    STRING clipboard;
    clipboard.ReadFromClipboard(0);
    int x,y;
    std::sscanf(clipboard.CharPtr(),"%i,%i",&x,&y);
    first=x; second=y;
}
template<class Procedure>
Procedure QuestProcedure(const char* name)
{
    const FARPROC exported=GetProcAddress(Editor()->questEditorModule,name);
    Procedure procedure;

    std::memcpy(&procedure,&exported,sizeof(procedure));
    return procedure;
}
}

INT_PTR CALLBACK AppConvertStackLine(HWND window,UINT message,WPARAM wParam,LPARAM)
{
    static ACT source={0,-999999,-999999,-999999};
    static ACT destination={0,-999999,-999999,-999999};
    DIALOG_COMBO_BOX sourceCommand(window,1057),destinationCommand(window,1058);
    DIALOG_INT source1(window,1007),source2(window,1008),source3(window,1009);
    DIALOG_INT destination1(window,1010),destination2(window,1011),destination3(window,1012);
    if (message==WM_INITDIALOG) {
        FillActionCombo(sourceCommand,source.command);
        FillActionCombo(destinationCommand,destination.command);
        ShowActionArguments(source,source1,source2,source3);
        ShowActionArguments(destination,destination1,destination2,destination3);
        return 1;
    }
    if (message==WM_COMMAND) switch (LOWORD(wParam)) {
    case 1116: source1=0; source2=0; source3=0; break;
    case 1117: destination1=0; destination2=0; destination3=0; break;
    case 1057:
        if (destinationCommand.GetCurrentStringData()==0) {
            const STRING name=sourceCommand.GetCurrentString();
            destinationCommand.SetCurrent(&name);
        }
        break;
    case IDOK: {
        ReadActionArguments(source,sourceCommand,source1,source2,source3);
        ReadActionArguments(destination,destinationCommand,destination1,destination2,destination3);
        int iterator;
        for (SPRITE* sprite=Sprites->FirstActiveSprite(&iterator);sprite;sprite=Sprites->NextActiveSprite(&iterator))
            for (int lane=0;lane<4;++lane) sprite->ConvertAction(lane,source,destination);
        EndDialog(window,1);
        break;
    }
    case IDCANCEL: EndDialog(window,0); break;
    }
    return 0;
}

INT_PTR CALLBACK AppDialogText(HWND window,UINT message,WPARAM wParam,LPARAM)
{
    DIALOG_TEXT edit(window,1007);
    STRING text;
    if (message==WM_INITDIALOG) {
        text.LoadFile(STRING("text\\dialogs.txt"));
        edit=&text;
        const STRING prefix=dialogTextSubNumber
            ? "<"+Int2Str(dialogTextNumber)+"_"+Int2Str(dialogTextSubNumber)+">"
            : "<"+Int2Str(dialogTextNumber)+">";
        const int line=text.Before(prefix).CountChar('\n');
        edit.Message(EM_LINESCROLL,0,line);
        return 1;
    }
    if (message==WM_COMMAND) switch (LOWORD(wParam)) {
    case IDOK:

        MoveFileExA("text\\dialogs.txt","text\\dialogs.bak",9);
        text=edit.GetText();
        text.SaveFile(STRING("text\\dialogs.txt"));
        EndDialog(window,1);
        break;
    case IDCANCEL: EndDialog(window,0); break;
    }
    return 0;
}

INT_PTR CALLBACK AppEditStackLine(HWND window,UINT message,WPARAM wParam,LPARAM)
{
    DIALOG_COMBO_BOX command(window,1057);
    DIALOG_INT first(window,1007),second(window,1008),third(window,1009);
    DIALOG_TEXT comment(window,1010);
    DIALOG_BUTTON copyCoordinates(window,2001),dialogButton(window,1115);
    DIALOG_BUTTON pasteXYZ(window,1103),pasteXY(window,1104);
    DIALOG_BUTTON mouseXYZ(window,1105),mouseXY(window,1106);
    if ((first.IsNotify(message,wParam,EN_UPDATE) && std::strchr(first.GetText().CharPtr(),',')) ||
        pasteXYZ.IsClicked(message,wParam)) PasteThreeCoordinates(first,second,third);
    if (mouseXYZ.IsClicked(message,wParam)) {
        first=TruncateFloatToInt32(Mouse->X());
        second=TruncateFloatToInt32(Mouse->Y());
        third=TruncateFloatToInt32(Mouse->Z());
    } else if ((second.IsNotify(message,wParam,EN_UPDATE) && std::strchr(second.GetText().CharPtr(),',')) ||
        pasteXY.IsClicked(message,wParam)) PasteTwoCoordinates(second,third);
    else if (mouseXY.IsClicked(message,wParam)) {
        second=TruncateFloatToInt32(Mouse->X());
        third=TruncateFloatToInt32(Mouse->Y());
    }
    if (message==WM_INITDIALOG) {
        copyCoordinates.SetCheck(0);
        FillActionCombo(command,editAction.command);
        const STRING description=ActionComment(command.GetCurrentStringData());
        comment=&description;
        ShowActionArguments(editAction,first,second,third);
        ShowDialogTextButton(dialogButton,command.GetCurrentStringData());
        return 1;
    }
    if (message==WM_COMMAND) {
        if (LOWORD(wParam)==666 && HIWORD(wParam)==666) {
            typedef void (__cdecl *GetCustomData)(int*,int*);
            const GetCustomData get=QuestProcedure<GetCustomData>("GetCustomData");
            if (get) {
                int quest,state;
                get(&quest,&state);
                first=quest; second=state; third=0;
                if (command.GetCurrentStringData()!=78) {
                    const STRING name("ACT_WHILE_NOT_QUEST");
                    command.SetCurrent(&name);
                }
            }
            Graph->EndPause();
        }
        switch (LOWORD(wParam)) {
        case 1113:
            if (Editor()->questEditorModule) {
                typedef void (__cdecl *CreateQuestWindow)(HINSTANCE,HINSTANCE,LPSTR,int,HWND);
                const CreateQuestWindow create=QuestProcedure<CreateQuestWindow>("CreateNew_zQuestEditorMainWnd");
                if (create) {
                    Graph->BeginPause();
                    create(GetModuleHandleA(0),0,GetCommandLineA(),0,window);
                    typedef void (__cdecl *SetGetQuestMode)();
                    QuestProcedure<SetGetQuestMode>("SetGetQuestMode")();
                }
            }
            break;
        case 1114:
            DialogBoxParamA(EditorInstance(),"MAP_INFO",window,AppMapInfo,0);
            break;
        case 1115:
            dialogTextNumber=first;
            dialogTextSubNumber=second;
            DialogBoxParamA(EditorInstance(),"DIALOG_TEXT",window,AppDialogText,0);
            break;
        case 1116: first=0; second=0; third=0; break;
        case 1057: {
            const STRING description=ActionComment(command.GetCurrentStringData());
            comment=&description;
            ShowDialogTextButton(dialogButton,command.GetCurrentStringData());
            break;
        }
        case IDOK:
            copyBuildCoordinates=copyCoordinates.GetCheck();
            ReadActionArguments(editAction,command,first,second,third);
            if (editAction.command==40 && !editAction.arg1 && editAction.arg3==1)
                editAction.arg1=999999;
            EndDialog(window,1);
            break;
        case IDCANCEL: EndDialog(window,0); break;
        }
    }
    return 0;
}

INT_PTR CALLBACK AppEditNumberLine(HWND window,UINT message,WPARAM wParam,LPARAM)
{
    DIALOG_INT edit(window,1007);
    if (message==WM_INITDIALOG) { edit=editedNumber; return 1; }
    if (message==WM_COMMAND) switch (LOWORD(wParam)) {
    case IDOK: editedNumber=edit; EndDialog(window,1); break;
    case IDCANCEL: EndDialog(window,0); break;
    }
    return 0;
}

INT_PTR CALLBACK AppUnitProperty(HWND window,UINT message,WPARAM wParam,LPARAM lParam)
{
    return Editor()->DialogUnitProperty(window,message,wParam,lParam);
}
int MAP_EDIT::DialogUnitProperty(HWND window,UINT message,WPARAM wParam,LPARAM lParam)
{
    DIALOG_TABS dialogTabs(window,1101);
    DIALOG_BUTTON dialogAggressive(window,1052),dialogCareful(window,1053);
    DIALOG_BUTTON dialogBehavior4(window,1054),dialogBehavior65536(window,1055);
    DIALOG_COMBO_BOX dialogName(window,1058);
    DIALOG_TEXT dialogFilterNames[3]={{window,1071},{window,1072},{window,1073}};
    DIALOG_BUTTON dialogFilters[3]={{window,2001},{window,2002},{window,2003}};
    DIALOG_UNSIGNED dialogDirection(window,1032),dialogSteppedDirection(window,1110);
    DIALOG_INT dialogX(window,1064),dialogY(window,1066),dialogZ(window,1067);
    DIALOG_INT dialogRed(window,1007),dialogGreen(window,1008),dialogBlue(window,1009),dialogAlpha(window,1016);
    DIALOG_FLOAT dialogScaleX(window,1010),dialogScaleY(window,1011),dialogScaleZ(window,1012);
    DIALOG_UNSIGNED dialogArmy(window,1031);
    DIALOG_LIST_BOX dialogItems(window,1013),dialogNumbers(window,1015);
    DIALOG_BUTTON dialogItemButton(window,1022);
    DIALOG_LIST_BOX dialogStack(window,1014);
    DIALOG_BUTTON dialogCut(window,1083),dialogCopy(window,1084),dialogPaste(window,1085);
    DIALOG_BUTTON dialogRemove(window,1086),dialogInsert(window,1087);
    DIALOG_BUTTON dialogDown(window,1089),dialogUp(window,1088);
    DIALOG_BUTTON dialogCopyCoor(window,1104),dialogPasteCoor(window,1103),dialogMouseCoor(window,1105);
    DIALOG_COMBO_BOX dialogVid(window,1057);
    static STRING unitName;
    static LIST<ACT> workingStacks[4];
    static LIST<ACT> actionClipboard;
    int current;
    if (selectedSprites.m_count) {
        current=dialogStack.GetCurrentStringData();
        LIST<ACT>& stack=workingStacks[selectedLane];
        dialogCut.Enable(dialogStack.GetNoSelected()!=0 && workingStacks[selectedLane].m_count!=0);
        dialogCopy.Enable(dialogStack.GetNoSelected()!=0 && workingStacks[selectedLane].m_count!=0);
        dialogPaste.Enable(actionClipboard.m_count!=0);
        if (stack.m_count) {
            dialogInsert.Enable(current>=-1 && current<stack.m_count);
            dialogRemove.Enable(current>=0 && current<stack.m_count);
            dialogUp.Enable(dialogStack.GetNoSelected()!=0 && !dialogStack.IsSelected(stack.m_count-1));
            dialogDown.Enable(dialogStack.GetNoSelected()!=0 && !dialogStack.IsSelected(0));
        } else dialogInsert.Enable(1);
    }

    if (dialogTabs.IsSelChange(message,wParam,lParam)) {
        selectedLane=dialogTabs.GetCurrentItemData();
        StackToListBox(dialogStack,workingStacks[selectedLane],0);
    }
    if (dialogDirection.IsNotify(message,wParam,EN_UPDATE) &&
        logicalDirection!=static_cast<unsigned int>(dialogDirection)) {
        logicalDirection=dialogDirection;
        realDirection=FirstSelected(this)->Vid()->RealDirection(static_cast<unsigned char>(logicalDirection));
        dialogSteppedDirection=realDirection;
    }
    if (dialogSteppedDirection.IsNotify(message,wParam,EN_UPDATE) &&
        realDirection!=static_cast<unsigned int>(dialogSteppedDirection)) {
        realDirection=dialogSteppedDirection;
        logicalDirection=(realDirection<<8)/static_cast<unsigned int>(FirstSelected(this)->Vid()->noDir);
        dialogDirection=logicalDirection;
    }
    if (dialogName.IsNotify(message,wParam,CBN_DROPDOWN)) {
        dialogName.Reset();
        for (int i=0;i<weaponNames.m_count;++i) {
            bool include=false;
            for (int filter=0;filter<3;++filter) {
                if (dialogFilters[filter].GetCheck()) {
                    const STRING filterText=dialogFilterNames[filter].GetText();
                    const STRING lowerFilter=filterText.ToLower();
                    const STRING lowerName=weaponNames.m_data[i].ToLower();
                    if (std::strstr(lowerName.CharPtr(),lowerFilter.CharPtr())) { include=true; break; }
                }
            }
            if (include || (!dialogFilters[0].GetCheck() && !dialogFilters[1].GetCheck() && !dialogFilters[2].GetCheck()))
                dialogName.AddString(&weaponNames.m_data[i]);
        }
        dialogName.SetText(&unitName);
    }
    if (dialogName.IsNotify(message,wParam,CBN_SELENDOK)) {
        namePending=1;
        unitName=dialogName.GetText()+" "+dialogName.GetCurrentString();
        dialogName.SetText(&unitName);
    }
    if (namePending && std::strcmp(dialogName.GetText().CharPtr(),unitName.CharPtr())!=0) {
        namePending=0;
        dialogName.SetText(&unitName);
    }
    if (dialogCopyCoor.IsClicked(message,wParam)) {
        const STRING coordinates=Printf("%i,%i,%i",static_cast<int>(dialogX),static_cast<int>(dialogY),static_cast<int>(dialogZ));
        coordinates.WriteToClipboard(0);
    }
    if ((dialogX.IsNotify(message,wParam,EN_UPDATE) && std::strchr(dialogX.GetText().CharPtr(),',')) ||
        dialogPasteCoor.IsClicked(message,wParam)) {
        STRING clipboard;
        clipboard.ReadFromClipboard(0);
        int x,y,z;
        std::sscanf(clipboard.CharPtr(),"%i,%i,%i",&x,&y,&z);

        dialogX=x; dialogY=y;
    } else if (dialogMouseCoor.IsClicked(message,wParam)) {
        dialogX=TruncateFloatToInt32OrMin(Mouse->X());
        dialogY=TruncateFloatToInt32OrMin(Mouse->Y());
    }

    if (message==WM_INITDIALOG) {
        itemFilter=0;
        namePending=0;
        if (!selectedSprites.m_count) { EndDialog(window,0); return 0; }
        {
            const STRING title="Unit Property for '"+FirstSelected(this)->Vid()->name+"'";
            SetWindowTextA(window,title.CharPtr());
        }
        for (int lane=0;lane<4;++lane) {
            if (FirstSelected(this)->IsActionStackStopped(lane)) {
                workingStacks[lane].Release();
                const STRING name("   ");
                dialogTabs.AddItemWithData(&name,lane);
            } else {
                workingStacks[lane]=FirstSelected(this)->ActionStack(lane);
                const STRING name="("+Int2Str(lane)+")";
                dialogTabs.AddItemWithData(&name,lane);
            }
        }
        selectedLane=0;
        dialogTabs.SetCurrent(0);
        StackToListBox(dialogStack,workingStacks[0],0);
        dialogFilters[0].SetCheck(Registry->GetInt(STRING("FilterBut1"),0));
        dialogFilters[1].SetCheck(Registry->GetInt(STRING("FilterBut2"),0));
        dialogFilters[2].SetCheck(Registry->GetInt(STRING("FilterBut3"),0));
        { const STRING name=Registry->GetString(STRING("FilterName1"),STRING("")); dialogFilterNames[0]=&name; }
        { const STRING name=Registry->GetString(STRING("FilterName2"),STRING("")); dialogFilterNames[1]=&name; }
        { const STRING name=Registry->GetString(STRING("FilterName3"),STRING("")); dialogFilterNames[2]=&name; }
        dialogX=TruncateFloatToInt32OrMin(FirstSelected(this)->X());
        dialogY=TruncateFloatToInt32OrMin(FirstSelected(this)->Y());
        dialogZ=TruncateFloatToInt32OrMin(FirstSelected(this)->Z());
        dialogScaleX=FirstSelected(this)->GetScale().x;
        dialogScaleY=FirstSelected(this)->GetScale().y;
        dialogScaleZ=FirstSelected(this)->GetScale().z;
        SendDlgItemMessageA(window,1106,UDM_SETRANGE32,static_cast<WPARAM>(-255),255);
        SendDlgItemMessageA(window,1107,UDM_SETRANGE32,static_cast<WPARAM>(-255),255);
        SendDlgItemMessageA(window,1108,UDM_SETRANGE32,static_cast<WPARAM>(-255),255);
        SendDlgItemMessageA(window,1111,UDM_SETRANGE32,static_cast<WPARAM>(-255),255);
        dialogRed=FirstSelected(this)->Gamma().Red();
        dialogGreen=FirstSelected(this)->Gamma().Green();
        dialogBlue=FirstSelected(this)->Gamma().Blue();
        dialogAlpha=FirstSelected(this)->Gamma().Alpha();
        dialogArmy=static_cast<unsigned int>(FirstSelected(this)->Army());
        SendDlgItemMessageA(window,1090,UDM_SETRANGE32,0,7);
        SendDlgItemMessageA(window,1091,UDM_SETRANGE32,0,FirstSelected(this)->Vid()->noDir-1);
        realDirection=FirstSelected(this)->RealDirection(); dialogSteppedDirection=realDirection;
        logicalDirection=FirstSelected(this)->Direction().value; dialogDirection=logicalDirection;
        dialogAggressive.SetCheck((FirstSelected(this)->Behave()&1u)!=0);
        dialogCareful.SetCheck((FirstSelected(this)->Behave()&2u)!=0);
        dialogBehavior4.SetCheck((FirstSelected(this)->Behave()&4u)!=0);
        dialogBehavior65536.SetCheck((FirstSelected(this)->Behave()&0x10000u)!=0);
        dialogItems.Reset(); dialogVid.Reset();
        for (int i=0;i<m_vidCount;++i) {
            if (!m_vids[i]) continue;
            if (FirstSelected(this)->IsSpriteClass(m_vids[i]->spriteClass)) {
                int index;
                { const STRING name=m_vids[i]->GetNumberName(); index=dialogVid.AddStringWithData(&name,i); }
                if (FirstSelected(this)->Vid()->nVid==i) dialogVid.SetCurrent(index);
            }
            if (m_vids[i]->spriteType&0x1Cu) {
                int index;
                { const STRING name=m_vids[i]->GetNumberName(); index=dialogItems.AddStringWithData(&name,i); }
                if (FirstSelected(this)->Action(56,i,0,0)) dialogItems.SetSelected(index);
            }
        }
        dialogItemButton.SetCheck(1);
        dialogName.Reset();
        for (int i=0;i<weaponNames.m_count;++i) {
            bool include=false;
            for (int filter=0;filter<3;++filter) {
                if (dialogFilters[filter].GetCheck()) {
                    const STRING filterText=dialogFilterNames[filter].GetText();
                    const STRING lowerFilter=filterText.ToLower();
                    const STRING lowerName=weaponNames.m_data[i].ToLower();
                    if (std::strstr(lowerName.CharPtr(),lowerFilter.CharPtr())) { include=true; break; }
                }
            }
            if (include || (!dialogFilters[0].GetCheck() && !dialogFilters[1].GetCheck() && !dialogFilters[2].GetCheck()))
                dialogName.AddString(&weaponNames.m_data[i]);
        }
        unitName=FirstSelected(this)->GetName();
        dialogName.SetText(&unitName);
        return 1;
    }

    if (message==WM_VKEYTOITEM) {
        if (reinterpret_cast<HWND>(lParam)==GetDlgItem(window,1014)) {
            const int key=LOWORD(wParam);
            if (key==VK_UP && (GetKeyState(VK_SHIFT)&0xFF00)!=0) {
                DialogUnitProperty(window,WM_COMMAND,1089,0); return -2;
            }
            if (key==VK_DOWN && (GetKeyState(VK_SHIFT)&0xFF00)!=0) {
                DialogUnitProperty(window,WM_COMMAND,1088,0); return -2;
            }
            if ((key==VK_INSERT || key=='C') && (GetKeyState(VK_CONTROL)&0xFF00)!=0) {
                DialogUnitProperty(window,WM_COMMAND,1084,0); return -1;
            }
            if ((key==VK_INSERT && (GetKeyState(VK_SHIFT)&0xFF00)!=0) || (key=='V' && (GetKeyState(VK_CONTROL)&0xFF00)!=0)) {
                DialogUnitProperty(window,WM_COMMAND,1085,0); return -1;
            }
            if ((key==VK_DELETE && (GetKeyState(VK_SHIFT)&0xFF00)!=0) || (key=='X' && (GetKeyState(VK_CONTROL)&0xFF00)!=0)) {
                DialogUnitProperty(window,WM_COMMAND,1083,0); return -1;
            }
            if (key=='A' && (GetKeyState(VK_CONTROL)&0xFF00)!=0) {
                const int count=dialogStack.NoString()-1;
                if (count==dialogStack.GetNoSelected())
                    dialogStack.Message(LB_SELITEMRANGE,0,static_cast<LPARAM>(static_cast<unsigned int>(dialogStack.NoString()-1)<<16));
                else
                    dialogStack.Message(LB_SELITEMRANGE,1,static_cast<LPARAM>(static_cast<unsigned int>(dialogStack.NoString()-1)<<16));
                return -1;
            }
            if (key==VK_INSERT) { DialogUnitProperty(window,WM_COMMAND,1087,0); return -1; }
            if (key==VK_DELETE) { DialogUnitProperty(window,WM_COMMAND,1086,0); return -1; }
            if (key==VK_SPACE) {
                LIST<ACT>& stack=workingStacks[selectedLane];
                if (dialogStack.GetCurrentStringData()>=0 && dialogStack.GetCurrentStringData()<workingStacks[selectedLane].m_count) {
                    editAction=workingStacks[selectedLane].m_data[dialogStack.GetCurrentStringData()];
                    if (DialogBoxParamA(Instance(),"EDIT_STACK_LINE",window,AppEditStackLine,0)) {

                        stack.m_data[dialogStack.GetCurrentStringData()+1]=editAction;
                        if (copyBuildCoordinates && editAction.command==35) {
                            for (int item=0;item<workingStacks[selectedLane].m_count;++item)
                                if (workingStacks[selectedLane].m_data[item].command==35) {
                                    workingStacks[selectedLane].m_data[item].arg2=editAction.arg2;
                                    workingStacks[selectedLane].m_data[item].arg3=editAction.arg3;
                                }
                        }
                    }
                    StackToListBox(dialogStack,stack,0);
                    return stack.m_count-1-current;
                }
            }
            return -1;
        }
        if (reinterpret_cast<HWND>(lParam)==GetDlgItem(window,1015)) {
            const int key=LOWORD(wParam);
            if (dialogNumbers.GetCurrentStringIndex()>=0 && key==VK_DELETE)
                dialogNumbers.DeleteString(dialogNumbers.GetCurrentStringIndex());
            else if (dialogNumbers.GetCurrentStringIndex()>=0 && key==VK_SPACE) {
                editedNumber=dialogNumbers.GetCurrentStringData();
                if (DialogBoxParamA(Instance(),"EDIT_NUMBER_LINE",window,AppEditNumberLine,0)) {
                    {
                        const STRING number=Int2Str(editedNumber);
                        dialogNumbers.InsertStringWithData(dialogNumbers.GetCurrentStringIndex(),&number,editedNumber);
                    }
                    dialogNumbers.DeleteString(dialogNumbers.GetCurrentStringIndex());
                    int placeholderIndex;
                    {
                        const STRING placeholder("Enter new number");
                        placeholderIndex=dialogNumbers.FindString(&placeholder);
                    }
                    if (placeholderIndex>=0) dialogNumbers.DeleteString(placeholderIndex);
                }
            }
            else if (key==VK_INSERT || key==VK_SPACE) {
                editedNumber=0;
                if (DialogBoxParamA(Instance(),"EDIT_NUMBER_LINE",window,AppEditNumberLine,0)) {
                    {
                        const STRING number=Int2Str(editedNumber);
                        dialogNumbers.AddStringWithData(&number,editedNumber);
                    }
                    int placeholderIndex;
                    {
                        const STRING placeholder("Enter new number");
                        placeholderIndex=dialogNumbers.FindString(&placeholder);
                    }
                    if (placeholderIndex>=0) dialogNumbers.DeleteString(placeholderIndex);
                }
            }
        }
        return 0;
    }
    if (message!=WM_COMMAND) return 0;

    LIST<ACT>& stack=workingStacks[selectedLane];
    switch (LOWORD(wParam)) {
    case 1083: {
        actionClipboard.Release();
        int lastSelected=-1;
        for (int row=stack.m_count-1;row>=0;--row) {
            if (dialogStack.IsSelected(row)) {
                lastSelected=row;
                const int index=stack.m_count-row-1;
                actionClipboard.Insert(stack.m_data[index]);
                StackDeletedLine(stack,index);
                stack.DeleteNumberS(index);
            }
        }
        if (lastSelected>=0) {
            StackToListBox(dialogStack,stack,-999);
            dialogStack.SetSelected(lastSelected);
        }
        break;
    }
    case 1084:
        actionClipboard.Release();
        for (int row=stack.m_count-1;row>=0;--row)
            if (dialogStack.IsSelected(row)) actionClipboard.Insert(stack.m_data[stack.m_count-row-1]);
        break;
    case 1092:
        actionClipboard.Release();
        for (int row=stack.m_count-1;row>=0;--row) actionClipboard.Insert(stack.m_data[stack.m_count-row-1]);
        break;
    case 1085: {
        if (current<0) current=-1;
        else if (current>=stack.m_count) current=stack.m_count-1;
        for (int i=0;i<actionClipboard.m_count;++i) StackInsertedLine(stack,current+1);
        for (int i=0;i<actionClipboard.m_count;++i) stack.InsertBefore(current+i+1,actionClipboard.m_data[i]);
        StackToListBox(dialogStack,stack,-999);
        for (int i=0;i<actionClipboard.m_count;++i) dialogStack.SetSelected(stack.m_count-current-i-2);
        dialogStack.SetCaretIndex(stack.m_count-current-1);
        break;
    }
    case 1086: {
        int lastSelected=-1;
        for (int row=stack.m_count-1;row>=0;--row) if (dialogStack.IsSelected(row)) {
            lastSelected=row;
            const int index=stack.m_count-row-1;
            StackDeletedLine(stack,index);
            stack.DeleteNumberS(index);
        }
        if (lastSelected>=0) {
            StackToListBox(dialogStack,stack,-999);
            dialogStack.SetSelected(lastSelected);
        }
        break;
    }
    case 1087:
        if (current<0) current=-1;
        else if (current>=stack.m_count) current=stack.m_count-1;
        if (current>=0 && current<stack.m_count) editAction=stack.m_data[current];
        if (DialogBoxParamA(Instance(),"EDIT_STACK_LINE",window,AppEditStackLine,0)) {
            StackInsertedLine(stack,current+1);
            stack.InsertBefore(current+1,editAction);
        }
        StackToListBox(dialogStack,stack,0);
        break;
    case 1089:
        if (dialogStack.IsSelected(0)) return 0;
        for (int row=0;row<dialogStack.NoString();++row) if (row<stack.m_count && dialogStack.IsSelected(row)) {
            for (int i=stack.m_count-1;i>=0;--i)
                if (stack.m_data[i].command==71 && stack.m_data[i].arg1==stack.m_count-row) --stack.m_data[i].arg1;
            stack.ShiftDown(stack.m_count-row-1);
        }
        StackToListBox(dialogStack,stack,-1);
        break;
    case 1088:
        if (dialogStack.IsSelected(stack.m_count-1)) return 0;
        for (int row=dialogStack.NoString()-1;row>=0;--row) if (dialogStack.IsSelected(row)) {
            for (int i=stack.m_count-1;i>=0;--i)
                if (stack.m_data[i].command==71 && stack.m_data[i].arg1==stack.m_count-row-2) ++stack.m_data[i].arg1;
            stack.ShiftUp(stack.m_count-row-1);
        }
        StackToListBox(dialogStack,stack,1);
        break;
    case IDOK: {
        {
            const STRING key("FilterBut1");
            Registry->SetInt(key,dialogFilters[0].GetCheck());
        }
        {
            const STRING key("FilterBut2");
            Registry->SetInt(key,dialogFilters[1].GetCheck());
        }
        {
            const STRING key("FilterBut3");
            Registry->SetInt(key,dialogFilters[2].GetCheck());
        }
        Registry->SetString(STRING("FilterName1"),dialogFilterNames[0].GetText());
        Registry->SetString(STRING("FilterName2"),dialogFilterNames[1].GetText());
        Registry->SetString(STRING("FilterName3"),dialogFilterNames[2].GetText());
        unsigned int behave=dialogAggressive.GetCheck()?1u:0u;
        behave|=dialogCareful.GetCheck()?2u:0u;
        behave|=dialogBehavior4.GetCheck()?4u:0u;
        behave|=dialogBehavior65536.GetCheck()?0x10000u:0u;
        const bool scaleChanged=FirstSelected(this)->GetScale()!=VECTOR(static_cast<float>(dialogScaleX),static_cast<float>(dialogScaleY),static_cast<float>(dialogScaleZ));
        const GAMMA newGamma=GAMMA(static_cast<int>(dialogAlpha),static_cast<int>(dialogRed),static_cast<int>(dialogGreen),static_cast<int>(dialogBlue));
        const GAMMA oldGamma=FirstSelected(this)->Gamma();
        const bool gammaChanged=oldGamma.modulate!=newGamma.modulate || oldGamma.add!=newGamma.add;
        bool nameChanged;
        {
            const STRING text=dialogName.GetText();
            const STRING original=FirstSelected(this)->GetName();
            nameChanged=std::strcmp(original.CharPtr(),text.CharPtr())!=0;
        }
        const bool vidChanged=dialogVid.GetCurrentStringData()!=FirstSelected(this)->Vid()->nVid;
        bool stackChanged=false,itemsChanged=false;
        for (int lane=0;lane<4;++lane) {
            const LIST<ACT>& original=selectedSprites.m_data[0]->ActionStack(lane);
            const LIST<ACT>& edited=workingStacks[lane];
            if (edited.m_count!=original.m_count) stackChanged=true;
            else for (int i=0;i<edited.m_count;++i) {
                const ACT& a=edited.m_data[i];
                const ACT& b=original.m_data[i];
                if (a.command!=b.command || a.arg1!=b.arg1 || a.arg2!=b.arg2 || a.arg3!=b.arg3) stackChanged=true;
            }
        }
        if (itemFilter==0xFFFF) {
            int placeholderIndex;
            {
                const STRING placeholder("Enter new number");
                placeholderIndex=dialogNumbers.FindString(&placeholder);
            }
            if (placeholderIndex>=0) dialogNumbers.DeleteString(placeholderIndex);
            if (dialogNumbers.NoString()!=FirstSelected(this)->NoItems()) itemsChanged=true;
            else for (int i=dialogNumbers.NoString()-1;i>=0;--i) {
                const int value=FirstSelected(this)->Item(i);
                if (value!=dialogNumbers.GetData(i)) itemsChanged=true;
            }
        } else {
            for (int i=dialogItems.NoString()-1;i>=0;--i) {
                if (dialogItems.IsSelected(i)) {
                    if (!FirstSelected(this)->HaveItem(dialogItems.GetData(i))) itemsChanged=true;
                } else if (FirstSelected(this)->HaveItem(dialogItems.GetData(i))) itemsChanged=true;
            }
        }
        const VECTOR target(static_cast<float>(static_cast<int>(dialogX)),static_cast<float>(static_cast<int>(dialogY)),static_cast<float>(static_cast<int>(dialogZ)));
        for (int i=selectedSprites.m_count-1;i>=0;--i) {
            if (FirstSelected(this)->Behave()!=behave) selectedSprites.m_data[i]->SetBehave(behave);
            if (nameChanged) selectedSprites.m_data[i]->SetName(dialogName.GetText());
            if (scaleChanged) selectedSprites.m_data[i]->SetScale(VECTOR(static_cast<float>(dialogScaleX),static_cast<float>(dialogScaleY),static_cast<float>(dialogScaleZ)));
            if (gammaChanged) selectedSprites.m_data[i]->SetGamma(GAMMA(static_cast<int>(dialogAlpha),static_cast<int>(dialogRed),static_cast<int>(dialogGreen),static_cast<int>(dialogBlue)));
            if (vidChanged) selectedSprites.m_data[i]->Action(62,dialogVid.GetCurrentStringData(),0,0);
            if (stackChanged) for (int lane=0;lane<4;++lane) selectedSprites.m_data[i]->SetActionStack(lane,workingStacks[lane]);
            if (FirstSelected(this)->Direction().value!=logicalDirection) selectedSprites.m_data[i]->ChangeDirection(ANGLE(static_cast<unsigned char>(logicalDirection)));
            if (FirstSelected(this)->Army()!=static_cast<int>(static_cast<unsigned int>(dialogArmy)))
                selectedSprites.m_data[i]->Action(97,static_cast<int>(static_cast<unsigned int>(dialogArmy)),0,0);
            if (itemsChanged) {
                selectedSprites.m_data[i]->Action(57,0,0,0);
                if (itemFilter==0xFFFF) {
                    for (int j=0;j<dialogNumbers.NoString();++j) selectedSprites.m_data[i]->InsertItem(dialogNumbers.GetData(j));
                } else {
                    for (int j=0;j<dialogItems.NoString();++j)
                        if (dialogItems.IsSelected(j)) selectedSprites.m_data[i]->InsertItem(dialogItems.GetData(j));
                }
            }
            if (TruncateFloatToInt32OrMin(FirstSelected(this)->X())!=static_cast<int>(dialogX) ||
                TruncateFloatToInt32OrMin(FirstSelected(this)->Y())!=static_cast<int>(dialogY) ||
                TruncateFloatToInt32OrMin(FirstSelected(this)->Z())!=static_cast<int>(dialogZ)) {
                if (options.optFlag16) ShiftActionCoordinates(selectedSprites.m_data[i],target-FirstSelected(this)->Coor());
                selectedSprites.m_data[i]->ChangeCoor(selectedSprites.m_data[i]->Coor()+target-FirstSelected(this)->Coor());
            }
        }
        EndDialog(window,1);
        break;
    }
    case IDCANCEL: EndDialog(window,0); break;
    case 1015:
        if (HIWORD(wParam)==LBN_DBLCLK && itemFilter==0xFFFF) {
            if (dialogNumbers.GetCurrentStringIndex()>=0) {
                editedNumber=dialogNumbers.GetCurrentStringData();
                if (DialogBoxParamA(Instance(),"EDIT_NUMBER_LINE",window,AppEditNumberLine,0)) {
                    {
                        const STRING number=Int2Str(editedNumber);
                        dialogNumbers.InsertStringWithData(dialogNumbers.GetCurrentStringIndex(),&number,editedNumber);
                    }
                    dialogNumbers.DeleteString(dialogNumbers.GetCurrentStringIndex());
                    int placeholderIndex;
                    {
                        const STRING placeholder("Enter new number");
                        placeholderIndex=dialogNumbers.FindString(&placeholder);
                    }
                    if (placeholderIndex>=0) dialogNumbers.DeleteString(placeholderIndex);
                }
            }
            else {
                editedNumber=0;
                if (DialogBoxParamA(Instance(),"EDIT_NUMBER_LINE",window,AppEditNumberLine,0)) {
                    {
                        const STRING number=Int2Str(editedNumber);
                        dialogNumbers.AddStringWithData(&number,editedNumber);
                    }
                    int placeholderIndex;
                    {
                        const STRING placeholder("Enter new number");
                        placeholderIndex=dialogNumbers.FindString(&placeholder);
                    }
                    if (placeholderIndex>=0) dialogNumbers.DeleteString(placeholderIndex);
                }
            }
        }
        break;
    case 1014:
        if (HIWORD(wParam)==LBN_DBLCLK) {
            if (dialogStack.GetCurrentStringData()>=0 && dialogStack.GetCurrentStringData()<workingStacks[selectedLane].m_count) {
                editAction=workingStacks[selectedLane].m_data[dialogStack.GetCurrentStringData()];
                if (DialogBoxParamA(Instance(),"EDIT_STACK_LINE",window,AppEditStackLine,0)) {
                    stack.m_data[dialogStack.GetCurrentStringData()]=editAction;
                    if (copyBuildCoordinates && editAction.command==35) {
                        for (int item=0;item<workingStacks[selectedLane].m_count;++item)
                            if (workingStacks[selectedLane].m_data[item].command==35) {
                                workingStacks[selectedLane].m_data[item].arg2=editAction.arg2;
                                workingStacks[selectedLane].m_data[item].arg3=editAction.arg3;
                            }
                    }
                }
                StackToListBox(dialogStack,stack,0);
            }
        }
        break;
    case 1026: {
        dialogNumbers.Enable(1); dialogNumbers.Show();
        dialogItems.Disable(); dialogItems.Hide();
        itemFilter=0xFFFF;
        dialogNumbers.Reset();
        for (int i=0;i<FirstSelected(this)->NoItems();++i) {
            const int value=FirstSelected(this)->Item(i);
            const STRING number=Int2Str(FirstSelected(this)->Item(i));
            dialogNumbers.AddStringWithData(&number,value);
        }
        if (!FirstSelected(this)->NoItems()) {
            const STRING placeholder("Enter new number");
            dialogNumbers.AddStringWithData(&placeholder,-1);
        }
        break;
    }
    case 1022: case 1023: case 1024: case 1025:
        itemFilter=LOWORD(wParam)==1022 ? 0x1C : LOWORD(wParam)==1023 ? 4 : LOWORD(wParam)==1024 ? 8 : 0x10;
        dialogItems.Enable(1); dialogItems.Show();
        dialogNumbers.Disable(); dialogNumbers.Hide();
        dialogItems.Reset();
        for (int i=0;i<VidCount();++i) {
            if (StoredVid(i) && (StoredVid(i)->spriteType&static_cast<unsigned int>(itemFilter))!=0u) {
                int index;
                { const STRING name=StoredVid(i)->GetNumberName(); index=dialogItems.AddStringWithData(&name,i); }
                if (FirstSelected(this)->Action(56,i,0,0)) dialogItems.SetSelected(index);
            }
        }
        break;
    }
    return 0;
}
