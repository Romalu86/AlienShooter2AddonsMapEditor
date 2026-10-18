#pragma once
#include "mylib.h"
#include <cmath>
#pragma push_macro("INPUT")
#undef INPUT
#define INPUT WIN32_INPUT
#include <windows.h>
#include <commctrl.h>
#pragma pop_macro("INPUT")

class DIALOG_ITEM {
public:
    HWND hWnd; int id;
    DIALOG_ITEM(HWND wnd,int rc_id) : hWnd(wnd),id(rc_id) {}

    int Message(unsigned int msg,WPARAM w,LPARAM l)
    {
        return static_cast<int>(SendDlgItemMessageA(hWnd,id,msg,w,l));
    }
    int IsNotify(unsigned int msg,unsigned int wParam,unsigned short notifyMessage)
    {
        if (msg!=0x111u) return 0;
        if ((wParam&0xFFFFu)!=static_cast<unsigned int>(id)) return 0;
        return ((wParam>>16)&0xFFFFu)==static_cast<unsigned int>(notifyMessage);
    }
    void SetText(const STRING* str)
    {
        SetDlgItemTextA(hWnd,id,str->CharPtr());
    }
    STRING GetText() { char buffer[32000]; GetDlgItemTextA(hWnd,id,buffer,sizeof(buffer)); return STRING(buffer); }
    void SetUnsigned(unsigned int val) { SetDlgItemInt(hWnd,id,val,0); }
    unsigned int GetUnsigned() { return GetDlgItemInt(hWnd,id,0,0); }
    void SetInt(int val) { SetDlgItemInt(hWnd,id,static_cast<unsigned int>(val),1); }
    int GetInt() { return static_cast<int>(GetDlgItemInt(hWnd,id,0,1)); }
    void Enable(int flag) { EnableWindow(GetDlgItem(hWnd,id),flag); }
    void Disable() { EnableWindow(GetDlgItem(hWnd,id),0); }
    void Show() { ShowWindow(GetDlgItem(hWnd,id),SW_SHOW); }
    void Hide() { ShowWindow(GetDlgItem(hWnd,id),SW_HIDE); }
};
class DIALOG_COMBO_BOX : public DIALOG_ITEM {
public:
    DIALOG_COMBO_BOX(HWND wnd,int rc_id) : DIALOG_ITEM(wnd,rc_id) {}
    void Reset() { Message(0x14B,0,0); }
    int NoString() { return Message(0x146,0,0); }
    int AddString(const STRING* str)
    {
        return Message(0x143,0,reinterpret_cast<LPARAM>(str->CharPtr()));
    }
    int AddStringWithData(const STRING* str,int data)
    {
        const int index=AddString(str);
        if (index!=-1) Message(0x151,static_cast<unsigned int>(index),data);
        return index;
    }
    void SetCurrent(int index) { Message(0x14E,static_cast<unsigned int>(index),0); }
    void SetCurrent(const STRING* str)
    {
        Message(0x14D,0,reinterpret_cast<LPARAM>(str->CharPtr()));
    }
    int GetCurrentStringIndex() { return Message(0x147,0,0); }
    int GetData(int index) { return Message(0x150,static_cast<unsigned int>(index),0); }
    STRING GetCurrentString() {
        char buffer[4096];
        Message(CB_GETLBTEXT,GetCurrentStringIndex(),reinterpret_cast<LPARAM>(buffer));
        return STRING(buffer);
    }
    int GetCurrentStringData() { return GetData(GetCurrentStringIndex()); }
};
class DIALOG_BUTTON : public DIALOG_ITEM {
public:
    DIALOG_BUTTON(HWND wnd,int rc_id) : DIALOG_ITEM(wnd,rc_id) {}
    unsigned int GetCheck() { return Message(0xF0,0,0)==1; }
    void SetCheck(int check) { Message(0xF1,check!=0,0); }
    int IsClicked(unsigned int msg,unsigned int wParam) { return IsNotify(msg,wParam,0); }
};
class DIALOG_LIST_BOX : public DIALOG_ITEM {
public:
    DIALOG_LIST_BOX(HWND wnd,int rc_id) : DIALOG_ITEM(wnd,rc_id) {}
    int NoString() { return Message(0x18B,0,0); }
    void Reset() { Message(0x184,0,0); }
    void SetColumnWidth(int width) { Message(0x195,static_cast<unsigned int>(width),0); }
    int AddString(const STRING* str)
    {
        return Message(0x180,0,reinterpret_cast<LPARAM>(str->CharPtr()));
    }
    int AddStringWithData(const STRING* str,int data)
    {
        const int index=AddString(str);
        if (index!=-1) Message(0x19A,static_cast<unsigned int>(index),data);
        return index;
    }
    void SetCurrent(int index) { Message(0x186,static_cast<unsigned int>(index),0); }
    int GetCurrentStringIndex() { return Message(0x188,0,0); }
    int GetData(int index) { return Message(0x199,static_cast<unsigned int>(index),0); }
    int GetCurrentStringData() { return GetData(GetCurrentStringIndex()); }
    int GetStringDataIndex(int data) { for (int i=NoString()-1;i>=0;--i) if (GetData(i)==data) return i; return -1; }
    int IsSelChange(unsigned int msg,unsigned int wParam) { return IsNotify(msg,wParam,1); }
    int IsDoubleClicked(unsigned int msg,unsigned int wParam) { return IsNotify(msg,wParam,2); }
    void SetSelected(int index) { Message(0x185u,1u,index); }
    int IsSelected(int index) { return Message(0x187u,static_cast<unsigned int>(index),0)>0; }
    void SetUnSelected(int index) { Message(0x185u,0u,index); }
    int GetNoSelected() { return Message(LB_GETSELCOUNT,0,0); }
    void DeleteString(int index) { Message(LB_DELETESTRING,index,0); }
    void SetCaretIndex(int index) { Message(LB_SETCARETINDEX,index,0); }
    int InsertStringWithData(int index,const STRING* text,int data) {
        const int inserted=Message(LB_INSERTSTRING,index,reinterpret_cast<LPARAM>(text->CharPtr()));
        if (inserted!=-1) Message(LB_SETITEMDATA,inserted,data);
        return inserted;
    }
    int FindString(const STRING* text) {
        return Message(LB_FINDSTRINGEXACT,0,reinterpret_cast<LPARAM>(text->CharPtr()));
    }
};
class DIALOG_TABS : public DIALOG_ITEM {
public:
    DIALOG_TABS(HWND wnd,int rc_id) : DIALOG_ITEM(wnd,rc_id) {}
    int NoItem() { return Message(0x1304,0,0); }
    int SetMinWidth(int width) { return Message(0x1331,0,width); }
    int AddItemWithData(const STRING* str,int data) {
        TCITEMA item={}; item.mask=TCIF_TEXT|TCIF_PARAM; item.pszText=const_cast<char*>(str->CharPtr());
        item.cchTextMax=str->Length(); item.iImage=-1; item.lParam=data;
        return Message(TCM_INSERTITEMA,NoItem(),reinterpret_cast<LPARAM>(&item));
    }
    void SetCurrentWithData(int data) { for (int i=NoItem()-1;i>=0;--i) if (GetData(i)==data) { SetCurrent(i); break; } }
    void SetCurrent(int index) { Message(0x130C,static_cast<unsigned int>(index),0); }
    int GetData(int index) { TCITEMA item={}; item.mask=TCIF_PARAM; item.iImage=-1;
        Message(TCM_GETITEMA,index,reinterpret_cast<LPARAM>(&item)); return static_cast<int>(item.lParam); }
    int GetCurrentItemData() { return GetData(GetCurrentItemIndex()); }
    int GetCurrentItemIndex() { return Message(0x130B,0,0); }
    int IsSelChange(unsigned int msg,unsigned int,LPARAM lParam) {
        return msg==WM_NOTIFY && reinterpret_cast<const NMHDR*>(lParam)->code==static_cast<UINT>(TCN_SELCHANGE);
    }
};
class DIALOG_TEXT : public DIALOG_ITEM {
public:
    DIALOG_TEXT(HWND wnd,int rc_id) : DIALOG_ITEM(wnd,rc_id) {}
    const STRING* operator=(const STRING* str) { SetText(str); return str; }
    operator STRING() { return GetText(); }
};
class DIALOG_UNSIGNED : public DIALOG_ITEM {
public:
    DIALOG_UNSIGNED(HWND wnd,int rc_id) : DIALOG_ITEM(wnd,rc_id) {}
    unsigned int operator=(unsigned int pos) { SetUnsigned(pos); return pos; }
    operator unsigned int() { return GetUnsigned(); }
};
class DIALOG_INT : public DIALOG_ITEM {
public:
    DIALOG_INT(HWND wnd,int rc_id) : DIALOG_ITEM(wnd,rc_id) {}
    int operator=(int pos) { SetInt(pos); return pos; }
    operator int() { return GetInt(); }
};

class DIALOG_FLOAT : public DIALOG_ITEM {
public:
    DIALOG_FLOAT(HWND wnd,int resourceId) : DIALOG_ITEM(wnd,resourceId) {}
    float operator=(float value) {
        const bool integral = std::floor(value)==value || std::isnan(value);
        const STRING text=Printf(integral ? "%.f" : "%f",static_cast<double>(value));
        SetText(&text);
        return value;
    }
    operator float() { return GetText().Float(); }
};
class DIALOG_SLIDER : public DIALOG_ITEM {
public:
    DIALOG_SLIDER(HWND wnd,int rc_id) : DIALOG_ITEM(wnd,rc_id) {}
    void SetRange(int left,int right)
    {
        Message(TBM_SETRANGE,TRUE,MAKELONG(static_cast<unsigned short>(left),static_cast<unsigned short>(right)));
    }
    int operator=(int pos) { Message(0x405u,1u,pos); return pos; }
    operator int() { return Message(0x400u,0u,0); }
};
class DIALOG_RADIO : public DIALOG_ITEM {
    int lastId;
public:
    DIALOG_RADIO(HWND wnd,int firstId,int lastResourceId)
        : DIALOG_ITEM(wnd,firstId),lastId(lastResourceId) {}
    void Check(int index) { CheckRadioButton(hWnd,id,lastId,id+index); }
    void SetItem(int index,const STRING* str)
    {
        SetDlgItemTextA(hWnd,id+index,str->CharPtr());
    }
    int GetIndexClicked(unsigned int msg,unsigned int wParam)
    {
        if (msg!=0x111u || ((wParam>>16)&0xFFFFu)!=0u) return -1;
        const int commandId=static_cast<int>(wParam&0xFFFFu);
        if (commandId<id || commandId>lastId) return -1;
        return commandId-id;
    }
    int GetIndexDblClicked(unsigned int msg,unsigned int wParam)
    {
        if (msg!=0x111u || ((wParam>>16)&0xFFFFu)!=5u) return -1;
        const int commandId=static_cast<int>(wParam&0xFFFFu);
        if (commandId<id || commandId>lastId) return -1;
        return commandId-id;
    }
};

class DIALOG_TREE : public DIALOG_ITEM {
public:
    DIALOG_TREE(HWND wnd,int controlId) : DIALOG_ITEM(wnd,controlId) {}
    void Reset() { Message(TVM_DELETEITEM,0,reinterpret_cast<LPARAM>(TVI_ROOT)); }
    HTREEITEM Current() { return reinterpret_cast<HTREEITEM>(Message(TVM_GETNEXTITEM,TVGN_CARET,0)); }
    void SetCurrent(HTREEITEM item) { Message(TVM_SELECTITEM,TVGN_CARET,reinterpret_cast<LPARAM>(item)); }
    int GetData(HTREEITEM node) {
        TVITEMA item={}; item.mask=TVIF_PARAM; item.hItem=node;
        Message(TVM_GETITEMA,0,reinterpret_cast<LPARAM>(&item)); return static_cast<int>(item.lParam);
    }
    int GetCurrentData() { return GetData(Current()); }
    int IsLeaf() { return !Message(TVM_GETNEXTITEM,TVGN_CHILD,reinterpret_cast<LPARAM>(Current())); }
    int IsSelChange(UINT msg,WPARAM,LPARAM data) {
        return msg==WM_NOTIFY && reinterpret_cast<const NMHDR*>(data)->code==static_cast<UINT>(TVN_SELCHANGEDA);
    }
    HTREEITEM Add(const STRING& text,HTREEITEM parent,int data,bool folder) {
        TVINSERTSTRUCTA item={}; item.hParent=parent; item.hInsertAfter=TVI_LAST;
        item.item.mask=TVIF_TEXT|TVIF_CHILDREN|(folder?0:TVIF_PARAM);
        item.item.pszText=const_cast<char*>(text.CharPtr()); item.item.cchTextMax=text.Length();
        item.item.cChildren=folder?1:0; item.item.lParam=data;
        return reinterpret_cast<HTREEITEM>(Message(TVM_INSERTITEMA,0,reinterpret_cast<LPARAM>(&item)));
    }
    int GetStringIndex(const STRING& text,int parent) {
        int node=Message(TVM_GETNEXTITEM,TVGN_CHILD,parent);
        static char buf[256];
        while (node) {
            TVITEMA item={}; item.mask=TVIF_TEXT; item.hItem=reinterpret_cast<HTREEITEM>(node);
            item.pszText=buf; item.cchTextMax=sizeof(buf);
            Message(TVM_GETITEMA,0,reinterpret_cast<LPARAM>(&item));
            if (text==buf) break;
            node=Message(TVM_GETNEXTITEM,TVGN_NEXT,node);
        }
        return node;
    }
};
