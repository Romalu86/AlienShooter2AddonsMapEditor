#pragma once

#include "basetype.h"
#include "myerror.h"

class DIALOG;
class SPRITE;
class STRING;
enum DLG_TYPE : int;

struct DIALOG_UNIT_RECORD
{
    DIALOG_UNIT_RECORD();
    int dialogId;
    int phraseIndex;
    unsigned int phraseStartTime;
    bool advanceRequested;
    int previousDialogId;
};

class DIALOG_UNIT
{
    friend class MAN;
public:
    static DIALOG_UNIT* m_pRadioMesUser;
    static DIALOG_UNIT* m_pSpeechUser;
    DIALOG_UNIT();
    virtual ~DIALOG_UNIT() noexcept(false);

    virtual SPRITE* Link() const=0;
    virtual void Error(TYPE_ERROR type, const STRING& subject, unsigned long code) const=0;
    virtual const VECTOR& Coor() const=0;
    virtual int AddLinkToLast(SPRITE* sprite)=0;
    virtual void PlaySFX(int sfxId) const=0;

    void SetDialog(int dialogKey);
    void ClearDialogs();
    int GetDialog() const;

protected:
    void SetRadioMsg(int radioId, int recordIndex, bool hideText = false);
    bool ShowCurPhrase(DLG_TYPE type, bool clearPhrase);
    void ClearDialog(DLG_TYPE type, bool preserveRadio = false);
    void DialogTact();
    void DialogTact(DLG_TYPE type);

    DIALOG_UNIT_RECORD m_dialog[2];
};
