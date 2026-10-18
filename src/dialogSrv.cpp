#include "dialogSrv.h"
#include "dialog_tree.h"
#include "myerror.h"

#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <new>

zDlgSrv* g_pDlgSrv = nullptr;

namespace
{
std::mutex dialogNilMutex;

void EraseDialogNodes(DialogTreeNode* node)
{
    while (node != DialogTreeControl::_Nil)
    {
        EraseDialogNodes(node->m_right);
        DialogTreeNode* const next = node->m_left;
        delete node;
        node = next;
    }
}

STRING DialogReadLine(std::FILE* file)
{
    char buffer[4096];
    return STRING(std::fgets(buffer, sizeof(buffer), file) ? buffer : "");
}
}

DialogTreeNode* DialogTreeControl::_Nil = nullptr;
unsigned int DialogTreeControl::_Nilrefs = 0;

DialogTreeControl::DialogTreeControl()
    : m_state0(0), m_state1(0), m_policyFlag(0), m_size(0)
{
    std::lock_guard<std::mutex> guard(dialogNilMutex);
    if (!_Nil)
    {
        _Nil = new DialogTreeNode;
        _Nil->m_left = nullptr;
        _Nil->m_right = nullptr;
        _Nil->m_parent = nullptr;
        _Nil->m_colorOrState = 1;
    }
    try
    {
        m_head = new DialogTreeNode;
    }
    catch (...)
    {
        if (_Nilrefs == 0)
        {
            delete _Nil;
            _Nil = nullptr;
        }
        throw;
    }
    ++_Nilrefs;
    m_head->m_parent = _Nil;
    m_head->m_left = m_head;
    m_head->m_right = m_head;
    m_head->m_colorOrState = 0;
}

DialogTreeControl::~DialogTreeControl()
{
    Clear();
    delete m_head;
    m_head = nullptr;
    std::lock_guard<std::mutex> guard(dialogNilMutex);
    if (--_Nilrefs == 0)
    {
        delete _Nil;
        _Nil = nullptr;
    }
}

void DialogTreeControl::Clear()
{
    EraseDialogNodes(m_head->m_parent);
    m_head->m_parent = _Nil;
    m_head->m_left = m_head;
    m_head->m_right = m_head;
    m_size = 0;
}

DIALOG*& DialogTreeControl::Record(int key)
{
    DialogTreeNode* node = m_head->m_parent;
    DialogTreeNode* parent = m_head;
    bool insertOnLeft = true;
    while (node != _Nil)
    {
        parent = node;
        insertOnLeft = key < node->m_key;
        node = insertOnLeft ? node->m_left : node->m_right;
    }
    DialogTreeNode* const insertionParent = parent;
    bool insertRequired = m_policyFlag != 0;
    if (!insertRequired)
    {
        if (insertOnLeft)
        {
            if (parent == m_head->m_left)
                insertRequired = true;
            else
                parent = DialogTreeOperations::Predecessor(parent, _Nil);
        }
        if (!insertRequired && parent->m_key < key)
            insertRequired = true;
    }
    if (insertRequired)
    {
        parent = DialogTreeOperations::Insert(m_head, m_size, node,
            insertionParent, _Nil,
            [key](DialogTreeNode* value)
            {
                value->m_key = key;
                value->m_dialog = nullptr;
            },
            [key](const DialogTreeNode* value) { return key < value->m_key; });
    }
    return parent->m_dialog;
}

COMMENT::COMMENT(STRING text, int dialogKey)
    : m_ownedString(text), m_dialogKey(dialogKey)
{
}

DialogVector8Control::DialogVector8Control() noexcept
    : m_state0(0), m_begin(nullptr), m_end(nullptr), m_capEnd(nullptr)
{
}

DialogVector8Control::~DialogVector8Control()
{
    Clear();
    ::operator delete(m_begin);
}

void DialogVector8Control::Clear()
{
    while (m_end != m_begin)
        (--m_end)->~COMMENT();
}

void DialogVector8Control::Append(const COMMENT& value)
{
    if (m_end == m_capEnd)
    {
        const std::size_t count = m_begin ? m_end - m_begin : 0;
        const std::size_t capacity = count + (count > 1 ? count : 1);
        COMMENT* const data = static_cast<COMMENT*>(
            ::operator new(capacity * sizeof(COMMENT)));
        std::size_t constructed = 0;
        try
        {
            for (; constructed < count; ++constructed)
                ::new (data + constructed) COMMENT(m_begin[constructed]);
            ::new (data + constructed) COMMENT(value);
        }
        catch (...)
        {
            while (constructed)
                data[--constructed].~COMMENT();
            ::operator delete(data);
            throw;
        }
        Clear();
        ::operator delete(m_begin);
        m_begin = data;
        m_end = data + count + 1;
        m_capEnd = data + capacity;
    }
    else
    {
        ::new (m_end) COMMENT(value);
        ++m_end;
    }
}

PHRASE::PHRASE() noexcept {}
PHRASE::~PHRASE() = default;

PHRASE::PHRASE(const STRING& text, PAUSE_TYPE pauseType,
    const STRING& voiceFile, int sfx, int pauseDuration)
{
    m_sfx = sfx;
    m_text = text;
    m_text.RemoveEndChars(" \n\r\t");
    m_pauseType = pauseType;
    m_voiceFile = voiceFile;
    m_pauseDuration = pauseDuration;
}

PHRASE& PHRASE::operator=(const PHRASE& source)
{
    m_text = source.m_text;
    m_voiceFile = source.m_voiceFile;
    m_pauseType = source.m_pauseType;
    m_sfx = source.m_sfx;
    m_pauseDuration = source.m_pauseDuration;
    return *this;
}

int PHRASE::GetDuration() const
{
    if (m_pauseDuration != 0)
        return m_pauseDuration;
    int lineCount = m_text.CountChar('\n') + 1;
    if (lineCount < 1)
        lineCount = 1;
    if (lineCount > 5)
        lineCount = 5;
    return g_pDlgSrv->m_lineDuration[lineCount - 1];
}

template<> LIST<PHRASE>::LIST() : m_count(0), m_capacity(0), m_data(nullptr) {}

template<> LIST<PHRASE>::~LIST()
{
    delete[] m_data;
    m_data = nullptr;
    m_count = 0;
}

template<> void LIST<PHRASE>::Insert(PHRASE value)
{
    if (m_count >= m_capacity)
    {
        const unsigned int grown = static_cast<unsigned int>(m_capacity) * 2u + 4u;
        const int capacity = BitsToSigned32(grown);
        if (capacity > m_capacity)
        {
            PHRASE* const data = new PHRASE[grown];
            for (int index = 0; index < m_capacity; ++index)
                data[index] = m_data[index];
            delete[] m_data;
            m_data = data;
            m_capacity = capacity;
        }
    }
    PHRASE* const destination = m_data + m_count;
    m_count = BitsToSigned32(static_cast<unsigned int>(m_count) + 1u);
    *destination = value;
}

DIALOG::DIALOG() : m_coor(0.0f, 0.0f, 50.0f)
{
    m_unitVid[0] = 0;
    m_unitVid[1] = 0;
    m_radio = 0;
    m_faceDirection = 0;

}

DIALOG::~DIALOG() = default;

const STRING& DIALOG::GetPhrase(int index) const
{
    if (index < m_phrases.m_count)
        return m_phrases.m_data[index].m_text;
    static const STRING empty;
    return empty;
}

int DIALOG::GetSFX(int index) const
{
    return index < m_phrases.m_count ? m_phrases.m_data[index].m_sfx : 0;
}

const STRING& DIALOG::GetVoiceFile(int index) const
{
    if (index < m_phrases.m_count)
        return m_phrases.m_data[index].m_voiceFile;
    static const STRING empty;
    return empty;
}

const VECTOR& DIALOG::Coor() const { return m_coor; }

int DIALOG::GetDuration(int index) const
{
    const int duration = index < m_phrases.m_count
        ? m_phrases.m_data[index].GetDuration() : 0;
    return BitsToSigned32(static_cast<unsigned int>(g_pDlgSrv->m_textDurationPercent) *
        static_cast<unsigned int>(duration)) / 100;
}

PHRASE::PAUSE_TYPE DIALOG::GetPauseType(int index) const
{
    if (index < m_phrases.m_count)
        return m_phrases.m_data[index].m_pauseType;
    return m_phrases.m_count ? m_phrases.m_data[m_phrases.m_count - 1].m_pauseType
                            : PHRASE::PAUSE;
}

void DIALOG::AddPhrase(const STRING& text, PHRASE::PAUSE_TYPE pauseType,
    const STRING& voiceFile, int sfx, int pauseDuration)
{
    m_phrases.Insert(PHRASE(text, pauseType, voiceFile, sfx, pauseDuration));
}

const STRING& DIALOG::GetFullText(bool windowsLineEndings)
{
    m_fullText = "";
    const int type = m_radio != 0u ? 1 : 0;
    const int font = m_unitVid[type];
    if (m_phrases.m_count == 0 && font == 0)
        return m_fullText;

    const STRING lineEnding(windowsLineEndings ? "\r\n" : "\n");
    const DIALOG& defaults = g_pDlgSrv->m_dialogRecord;
    if (defaults.m_unitVid[type] != font && font != 0)
    {
        m_fullText += Printf("<Font=%d>", font);
        m_fullText += lineEnding;
    }
    const int radio = BitsToSigned32(m_radio);
    if (radio != 0)
    {
        m_fullText += Printf("<Icon=%d,%d>", radio, BitsToSigned32(m_faceDirection));
        m_fullText += lineEnding;
    }
    if (m_coor.x != defaults.m_coor.x || m_coor.y != defaults.m_coor.y ||
        m_coor.z != defaults.m_coor.z)
    {
        m_fullText += Printf("<Coor=%d,%d,%d>", radio);
        m_fullText += lineEnding;
    }

    const int phraseCount = m_phrases.m_count;
    for (int index = 0; index < phraseCount; ++index)
    {
        if (index != 0)
        {
            m_fullText += lineEnding;
            m_fullText += Printf("<pause>", radio);
            m_fullText += lineEnding;
        }
        const int pauseDuration = m_phrases.m_data[index].m_pauseDuration;
        if (pauseDuration != 0)
        {
            m_fullText += Printf("<PauseDuration=%d>", pauseDuration);
            m_fullText += lineEnding;
        }
        const int sfx = GetSFX(index);
        if (sfx != 0)
        {
            m_fullText += Printf("<SFX=%d>", sfx);
            m_fullText += lineEnding;
        }
        const STRING text(GetPhrase(index));
        const int length = text.Length();
        for (int character = 0; character < length; ++character)
        {
            if (text[character] != '\r' || windowsLineEndings)
                m_fullText += Printf("%c", static_cast<int>(
                    static_cast<signed char>(text[character])));
        }
    }
    m_fullText += lineEnding;
    return m_fullText;
}

zDlgSrv::zDlgSrv()
    : m_textDurationPercent(100), m_showSubtitles(true), m_activeRadioDialog(0)
{
    for (int index = 0; index < 5; ++index)
        m_lineDuration[index] = 0;
}

zDlgSrv::~zDlgSrv() noexcept(false)
{
    ReleaseData();
}

void zDlgSrv::ReleaseData()
{
    for (DialogTreeNode* node = m_dialogTree.m_head->m_left;
         node != m_dialogTree.m_head;
         node = DialogTreeOperations::Successor(node, DialogTreeControl::_Nil))
    {
        delete node->m_dialog;
    }
    m_dialogTree.Clear();
}

bool zDlgSrv::IsDlgNumValid(int key)
{

    return m_dialogTree.Record(key) != nullptr;
}

DIALOG* zDlgSrv::GetDialog(int key)
{
    if (!IsDlgNumValid(key))
        m_dialogTree.Record(key);
    return m_dialogTree.Record(key);
}

int zDlgSrv::GetDlgKey(int group, int var2)
{
    return BitsToSigned32(static_cast<unsigned int>(group) * 1000u +
                         static_cast<unsigned int>(var2));
}

void zDlgSrv::LoadDialogs(const STRING& source)
{
    ReleaseData();
    m_dialogVector.Clear();
    std::FILE* file;
    {
        const STRING mode("rb");
        file = FOpen(source, mode);
    }
    if (!file)
    {
        if (!source.IsEmpty())
            ::Error->Error(STRING("DIALOG"), E_LOAD, source, 0u);
        return;
    }

    STRING phraseText;
    STRING voiceFile;
    int sfx = 0;
    int pauseDuration = 0;
    int currentKey = 0;
    int blankLines = 0;
    m_dialogRecord.m_coor.x = 0.0f;
    m_dialogRecord.m_coor.y = 0.0f;
    m_dialogRecord.m_coor.z = 0.0f;
    m_dialogRecord.m_radio = 0;
    m_dialogRecord.m_faceDirection = 0;
    m_dialogRecord.m_unitVid[0] = 2;

    while (!std::feof(file))
    {
        const STRING originalLine = DialogReadLine(file);
        STRING line(originalLine);
        line.RemoveEndChars(" \n\r\t");
        const char* const text = line.CharPtr();
        const bool numericTag = text[0] == '<' &&
            std::isdigit(static_cast<signed char>(text[1])) != 0 &&
            std::strstr(text, ">") != nullptr;
        const bool comment = text[0] == ';';

        if (text[0] == 0)
            ++blankLines;
        else if (!numericTag && !comment)
            blankLines = 0;
        if (numericTag || comment)
        {
            for (int index = 0; index < blankLines; ++index)
                m_dialogVector.Append(COMMENT(STRING(""), currentKey));
            blankLines = 0;
        }
        if (comment)
        {
            m_dialogVector.Append(COMMENT(line, currentKey));
            continue;
        }

        if (numericTag)
        {
            phraseText.RemoveEndChars(" \n\r\t");
            if (currentKey && !phraseText.IsEmpty())
            {
                DIALOG* const previous = m_dialogTree.Record(currentKey);
                previous->AddPhrase(phraseText, previous->GetPauseType(999999),
                    voiceFile, sfx, pauseDuration);
            }
            const STRING tag = line.After("<").Before(">");
            if (!tag.IsEmpty())
            {
                const int group = tag.Before("_").Int();
                const int index = tag.After("_").Int();
                currentKey = group && index ? GetDlgKey(group, index)
                                           : GetDlgKey(tag.Int(), 0);
                m_dialogTree.Record(currentKey) = new (std::nothrow) DIALOG;
            }
            DIALOG* const current = m_dialogTree.Record(currentKey);
            current->m_unitVid[1] = m_dialogRecord.m_unitVid[1];
            current->m_unitVid[0] = m_dialogRecord.m_unitVid[0];
            phraseText = "";
            voiceFile = "";
            sfx = 0;
            pauseDuration = 0;
            current->m_coor.x = m_dialogRecord.m_coor.x;
            current->m_coor.y = m_dialogRecord.m_coor.y;
            current->m_coor.z = m_dialogRecord.m_coor.z;
            current->m_radio = m_dialogRecord.m_radio;
            current->m_faceDirection = m_dialogRecord.m_faceDirection;
            continue;
        }

        if (std::strcmp(text, "<pause>") == 0 ||
            std::strcmp(text, "<click or pause>") == 0)
        {
            phraseText.RemoveEndChars(" \n\r\t");
            if (currentKey)
            {
                m_dialogTree.Record(currentKey)->AddPhrase(phraseText,
                    std::strcmp(text, "<pause>") == 0 ? PHRASE::PAUSE : PHRASE::CLICK_OR_PAUSE,
                    voiceFile, sfx, pauseDuration);
                sfx = 0;
                pauseDuration = 0;
            }
            phraseText = "";
            continue;
        }
        if (line.HaveFirst("<Speech="))
        {
            voiceFile = line.After("<Speech=").Before(">");
            continue;
        }
        if (std::strncmp(text, "<Font=", 6) == 0)
        {
            const int font = line.After("<Font=").Before(">").Int();
            DIALOG* const target = currentKey ? m_dialogTree.Record(currentKey) : &m_dialogRecord;
            target->m_unitVid[0] = font;
            if (currentKey)
                target->m_unitVid[1] = font;
            continue;
        }
        if (std::strncmp(text, "<DefaultFontForRadio=", 21) == 0)
        {
            m_dialogRecord.m_unitVid[1] = line.After("<DefaultFontForRadio=").Before(">").Int();
            continue;
        }
        if (std::strncmp(text, "<SFX=", 5) == 0)
        {
            sfx = line.After("<SFX=").Before(">").Int();
            continue;
        }
        if (std::strncmp(text, "<PauseDuration=", 15) == 0)
        {
            pauseDuration = line.After("<PauseDuration=").Before(">").Int();
            continue;
        }
        if (std::strncmp(text, "<Coor=", 6) == 0)
        {
            float x, y, z;
            std::sscanf(line.After("<Coor=").Before(">").CharPtr(), "%f, %f, %f", &x, &y, &z);
            DIALOG* const target = currentKey ? m_dialogTree.Record(currentKey) : &m_dialogRecord;
            target->m_coor.x = x;
            target->m_coor.y = y;
            target->m_coor.z = z;
            continue;
        }
        if (std::strncmp(text, "<Icon=", 6) == 0)
        {
            int icon, face;
            std::sscanf(line.After("<Icon=").Before(">").CharPtr(), "%d, %d", &icon, &face);
            DIALOG* const target = currentKey ? m_dialogTree.Record(currentKey) : &m_dialogRecord;
            target->m_radio = static_cast<unsigned int>(icon);
            target->m_faceDirection = static_cast<unsigned int>(face);
            continue;
        }
        phraseText += originalLine;
    }
    phraseText.RemoveEndChars(" \n\r\t");
    if (currentKey && !phraseText.IsEmpty())
    {
        DIALOG* const current = m_dialogTree.Record(currentKey);
        current->AddPhrase(phraseText, current->GetPauseType(999999),
            voiceFile, sfx, pauseDuration);
    }
    std::fclose(file);
}

void zDlgSrv::SaveDialogs(const STRING& destination)
{
    std::FILE* file;
    {
        const STRING mode("w");
        file = FOpen(destination, mode);
    }
    if (!file)
        return;

    COMMENT* comment = m_dialogVector.m_begin;
    for (int header = 0; header < 5; ++header)
    {
        if (comment != m_dialogVector.m_end)
        {
            std::fprintf(file, "%s\n", comment->m_ownedString.CharPtr());
            ++comment;
        }
        if (header == 1)
            std::fprintf(file, "<Font=%d>\n", m_dialogRecord.m_unitVid[0]);
        else if (header == 2)
            std::fprintf(file, "<DefaultFontForRadio=%d>\n", m_dialogRecord.m_unitVid[1]);
        else if (header == 3)
            std::fprintf(file, "<Coor=%d,%d,%d>\n",
                TruncateFloatToInt32(m_dialogRecord.m_coor.x),
                TruncateFloatToInt32(m_dialogRecord.m_coor.y),
                TruncateFloatToInt32(m_dialogRecord.m_coor.z));
    }

    for (int key = 1; key < BitsToSigned32(m_dialogTree.m_size); ++key)
    {
        while (comment != m_dialogVector.m_end && comment->m_dialogKey < key)
        {
            std::fprintf(file, "%s\n", comment->m_ownedString.CharPtr());
            ++comment;
        }
        const int phraseCount = m_dialogTree.Record(key)->m_phrases.m_count;
        DIALOG* const dialog = m_dialogTree.Record(key);
        const int type = dialog->m_radio != 0u ? 1 : 0;
        if (dialog->m_phrases.m_count != 0 || dialog->m_unitVid[type] != 0)
            std::fprintf(file, "<%d>\n", key);
        if (phraseCount != 0)
        {
            const STRING text(m_dialogTree.Record(key)->GetFullText(false));
            std::fprintf(file, "%s", text.CharPtr());
        }
    }
    std::fclose(file);
}
