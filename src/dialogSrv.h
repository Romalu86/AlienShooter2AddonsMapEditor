#pragma once
#include "basetype.h"
#include "mylib.h"
#include "list.h"

#include <stddef.h>

class DIALOG;

struct DialogTreeNode
{
    DialogTreeNode* m_left;
    DialogTreeNode* m_parent;
    DialogTreeNode* m_right;
    int m_key;
    DIALOG* m_dialog;
    unsigned int m_colorOrState;

    DialogTreeNode*& LeftLink() { return m_left; }
    DialogTreeNode*& ParentLink() { return m_parent; }
    DialogTreeNode*& RightLink() { return m_right; }
    unsigned int& ColorValue() { return m_colorOrState; }
};

struct DialogTreeInsertResult
{
    DialogTreeNode* m_node;
    unsigned char m_inserted;
};

struct DialogTreeControl
{
    static DialogTreeNode* _Nil;
    static unsigned int _Nilrefs;

    DialogTreeControl();
    ~DialogTreeControl();
    DIALOG*& Record(int key);
    void Clear();

    unsigned char m_state0;
    unsigned char m_state1;
    DialogTreeNode* m_head;
    unsigned char m_policyFlag;
    unsigned int m_size;
};

struct COMMENT
{
    STRING m_ownedString;
    int m_dialogKey;

    COMMENT(STRING text, int dialogKey);
};

struct DialogVector8Control
{
    DialogVector8Control() noexcept;
    ~DialogVector8Control();
    void Append(const COMMENT& comment);
    void Clear();

    unsigned char m_state0;
    COMMENT* m_begin;
    COMMENT* m_end;
    COMMENT* m_capEnd;
};

struct PHRASE
{
    STRING m_text;
    STRING m_voiceFile;
    enum PAUSE_TYPE : int
    {
        PAUSE = 1,
        CLICK_OR_PAUSE = 2
    };

    PAUSE_TYPE m_pauseType;
    int m_sfx;
    int m_pauseDuration;

    PHRASE() noexcept;
    PHRASE(const STRING& text, PAUSE_TYPE pauseType, const STRING& voiceFile,
        int sfx, int pauseDuration);
    PHRASE& operator=(const PHRASE& source);
    int GetDuration() const;

    ~PHRASE();
};

template<> LIST<PHRASE>::LIST();
template<> LIST<PHRASE>::~LIST();
template<> void LIST<PHRASE>::Insert(PHRASE value);

class DIALOG
{
public:
    LIST<PHRASE> m_phrases;
    int m_unitVid[2];
    VECTOR m_coor;
    unsigned int m_radio;
    unsigned int m_faceDirection;
    STRING m_fullText;

    DIALOG();
    ~DIALOG();
    const STRING& GetPhrase(int index) const;
    int GetSFX(int index) const;
    int GetDuration(int index) const;
    PHRASE::PAUSE_TYPE GetPauseType(int index) const;
    void AddPhrase(const STRING& text, PHRASE::PAUSE_TYPE pauseType,
        const STRING& voiceFile, int sfx, int pauseDuration);
    const STRING& GetVoiceFile(int index) const;
    const VECTOR& Coor() const;
    const STRING& GetFullText(bool windowsLineEndings);
};

class zDlgSrv
{
public:
    zDlgSrv();
    virtual ~zDlgSrv() noexcept(false);

    DialogTreeControl m_dialogTree;
    DialogVector8Control m_dialogVector;
    DIALOG m_dialogRecord;
    int m_textDurationPercent;
    int m_lineDuration[5];
    bool m_showSubtitles;
    int m_activeRadioDialog;
    bool IsDlgNumValid(int dialogKey);
    DIALOG* GetDialog(int _iDlgKey);
    int GetDlgKey(int group, int var2);
    void LoadDialogs(const STRING& source);
    void SaveDialogs(const STRING& destination);

private:
    void ReleaseData();
};

typedef zDlgSrv DialogSrv;

extern zDlgSrv* g_pDlgSrv;
