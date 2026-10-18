#include "dialog_unit.h"
#include "dialogSrv.h"
#include "engine_globals.h"
#include "map.h"
#include "sprite.h"
#include "sound.h"
#include "terrain.h"
#include "vid.h"

namespace
{

DLG_TYPE DialogType(const DIALOG* dialog)
{
    return static_cast<DLG_TYPE>(dialog->m_radio != 0u ? 1 : 0);
}

int DialogUnitVid(const DIALOG* dialog)
{
    return dialog->m_unitVid[static_cast<int>(DialogType(dialog))];
}

}

DIALOG_UNIT* DIALOG_UNIT::m_pRadioMesUser = 0;
DIALOG_UNIT* DIALOG_UNIT::m_pSpeechUser = 0;

DIALOG_UNIT_RECORD::DIALOG_UNIT_RECORD()
    : dialogId(0), phraseIndex(0), phraseStartTime(0u), advanceRequested(false),
      previousDialogId(0)
{
}

DIALOG_UNIT::DIALOG_UNIT()
{
}

DIALOG_UNIT::~DIALOG_UNIT() noexcept(false)
{
    SetRadioMsg(0, 0);
    if (m_pSpeechUser == this)
    {
        ::Sound->StopSpeech();
        m_pSpeechUser = 0;
    }
}

void DIALOG_UNIT::SetRadioMsg(int radioId, int recordIndex, bool hideText)
{
    if (!gpQuestEvent)
        return;
    if (radioId != 0)
    {
        m_pRadioMesUser = this;
        gpQuestEvent->SetRadio(radioId, recordIndex, hideText);
    }
    else if (m_pRadioMesUser == this)
    {
        gpQuestEvent->SetRadio(0, 0, hideText);
        m_pRadioMesUser = 0;
    }
}

void DIALOG_UNIT::SetDialog(int dialogKey)
{
    if (!::g_pDlgSrv->IsDlgNumValid(dialogKey))
    {
        Error(E_INVALID, STRING("N dialog in SetDialog"), dialogKey);
        return;
    }

    DIALOG* const dialog = ::g_pDlgSrv->GetDialog(dialogKey);
    const DLG_TYPE type = DialogType(dialog);
    DIALOG_UNIT_RECORD& record = m_dialog[static_cast<int>(type)];
    if (dialogKey != 102000 && (record.dialogId == dialogKey ||
        (record.previousDialogId == dialogKey &&
         static_cast<unsigned int>(::CurrentTime-record.phraseStartTime) < 5000u)))
        return;

    const int unitVid = DialogUnitVid(dialog);
    if (!Map->ValidateVid(unitVid))
    {
        Error(E_INVALID, STRING("Vid for dialog in SetDialog"), unitVid);
        return;
    }

    ClearDialog(type);
    record.dialogId = dialogKey;
    record.phraseIndex = 0;
    record.previousDialogId = dialogKey;
    if (!ShowCurPhrase(type, false))
        record.dialogId = 0;
    else if (static_cast<int>(type) == 1)
        ::g_pDlgSrv->m_activeRadioDialog = dialogKey;
}

bool DIALOG_UNIT::ShowCurPhrase(DLG_TYPE type, bool clearPhrase)
{
    DIALOG_UNIT_RECORD& record = m_dialog[static_cast<int>(type)];
    const int dialogKey = record.dialogId;
    const int phraseIndex = record.phraseIndex;
    DIALOG* const dialog = ::g_pDlgSrv->GetDialog(dialogKey);
    if (!clearPhrase)
    {
        record.phraseStartTime = ::CurrentTime;
        record.advanceRequested = false;
    }

    const int sfx = clearPhrase ? 0 : dialog->GetSFX(phraseIndex);
    if (sfx != 0)
        PlaySFX(sfx);

    const STRING speech = dialog->GetVoiceFile(phraseIndex);
    if (!speech.IsEmpty() && !clearPhrase)
    {
        m_pSpeechUser = this;
        ::Sound->PlaySpeech(speech);
    }
    const bool hideText = !::g_pDlgSrv->m_showSubtitles &&
        !dialog->GetVoiceFile(phraseIndex).IsEmpty();

    if (static_cast<int>(DialogType(dialog)) == 1 && gpQuestEvent)
    {
        if (clearPhrase)
            SetRadioMsg(0, 0);
        else
            SetRadioMsg(dialogKey, phraseIndex, hideText);
        return true;
    }

    if (static_cast<int>(DialogType(dialog)) != 0)
        return false;

    if (hideText && !clearPhrase)
        return false;

    const STRING text = clearPhrase ? STRING("") : dialog->GetPhrase(phraseIndex);
    const int unitVid = DialogUnitVid(dialog);
    for (SPRITE* sprite=Link(); sprite; sprite=sprite->Link())
    {
        if (sprite->Vid()->nVid == unitVid)
        {
            sprite->Action(ACT_SET_TEXT, static_cast<int>(PtrToU32(&text)), 0, 0);
            return true;
        }
    }

    if (phraseIndex != 0)
        return false;

    const VECTOR position = (Coor()+dialog->Coor())+VECTOR(0, 200, 200);
    SPRITE* const sprite = Map->CreateSprite(Map->Vid(unitVid), position, ANGLE(0u), 0);
    if (!sprite)
        return false;

    sprite->Action(ACT_SET_BEHAVE, 5, 0, 0);
    AddLinkToLast(sprite);
    sprite->Action(ACT_SET_TEXT, static_cast<int>(PtrToU32(&text)), 0, 0);
    return true;
}

void DIALOG_UNIT::ClearDialog(DLG_TYPE type, bool preserveRadio)
{
    DIALOG_UNIT_RECORD& record = m_dialog[static_cast<int>(type)];
    if (record.dialogId == 0)
        return;

    DIALOG* const dialog = ::g_pDlgSrv->GetDialog(record.dialogId);

    if (static_cast<int>(type) == 0)
    {
        const int unitVid = DialogUnitVid(dialog);
        SPRITE* sprite = Link();
        while (sprite && sprite->Vid()->nVid != unitVid)
            sprite = sprite->Link();
        if (sprite)
        {
            sprite->BreakLink();
            delete sprite;
        }
    }
    else if (!preserveRadio)
    {
        SetRadioMsg(0, 0);
    }

    if (!dialog->GetVoiceFile(record.phraseIndex).IsEmpty() && m_pSpeechUser == this)
    {
        ::Sound->StopSpeech();
        m_pSpeechUser = 0;
    }
    if (::g_pDlgSrv->m_activeRadioDialog == record.dialogId)
        ::g_pDlgSrv->m_activeRadioDialog = 0;

    record.phraseStartTime = ::CurrentTime;
    record.advanceRequested = false;
    record.dialogId = 0;
}

void DIALOG_UNIT::ClearDialogs()
{
    ClearDialog(static_cast<DLG_TYPE>(0));
    ClearDialog(static_cast<DLG_TYPE>(1));
}

void DIALOG_UNIT::DialogTact()
{
    DialogTact(static_cast<DLG_TYPE>(0));
    DialogTact(static_cast<DLG_TYPE>(1));
}

void DIALOG_UNIT::DialogTact(DLG_TYPE type)
{
    DIALOG_UNIT_RECORD& record = m_dialog[static_cast<int>(type)];
    if (record.dialogId == 0)
        return;

    if (::g_pDlgSrv->m_activeRadioDialog != 0 && static_cast<int>(type) == 1 &&
        ::g_pDlgSrv->m_activeRadioDialog != m_dialog[1].dialogId)
    {
        ClearDialog(type, true);
        return;
    }

    DIALOG* const dialog = ::g_pDlgSrv->GetDialog(record.dialogId);
    const unsigned int previousStart = record.phraseStartTime;
    const int phraseIndex = record.phraseIndex;
    if (Map->InputState()->GetAction(23u))
    {
        record.phraseStartTime = ::CurrentTime -
            static_cast<unsigned int>(dialog->GetDuration(phraseIndex)) - 1u;
        record.advanceRequested = true;
    }

    const int elapsed = BitsToSigned32(::CurrentTime - previousStart);
    const bool durationExpired = elapsed > dialog->GetDuration(phraseIndex);
    const bool speechPlaying = durationExpired && ::Sound->m_speech &&
        ::Sound->m_speech->IsPlaying() != 0 && m_pSpeechUser == this;
    if (!(durationExpired && !speechPlaying) && !record.advanceRequested)
        return;

    if (record.phraseIndex+1 >= dialog->m_phrases.m_count)
    {
        ClearDialog(type);
        return;
    }

    const bool advance = elapsed > BitsToSigned32(
        static_cast<unsigned int>(dialog->GetDuration(phraseIndex)) + 100u);
    if (advance)
        ++record.phraseIndex;
    ShowCurPhrase(type, !advance);
}

int DIALOG_UNIT::GetDialog() const
{
    return m_dialog[0].dialogId != 0 || m_dialog[1].dialogId != 0;
}
