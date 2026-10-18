#pragma once
#include "mylib.h"
#include "basetype.h"
#include "myerror.h"

struct HINSTANCE__;

class NET final
{
public:
    NET(HINSTANCE__* hInstance, STRING title);
    ~NET() noexcept(false);

    virtual long Send(const void* data,unsigned int size,unsigned char type,int player);
    virtual long SendGuaranteed(const void* data,unsigned int size,unsigned char type,int player);
    virtual int Receive(void* out,unsigned int size,int player);

    long InitDirectPlay();
    long LobbyLaunch();
    long CreateDeviceAddress();
    long CreateHostAddress(STRING hostAddress);
    void AddReceivedData(void* data,unsigned int size,unsigned long sender,unsigned char ready);

    STRING GetPlayerName(int player) const;
    int GetNoGame() const;
    STRING GetGameList(int index) const;
    void StartHostSearch(STRING hostAddress);
    void CancelHostSearch();
    long GameCreate(STRING gameName);
    STRING GetPlayerNameForHost(int player) const;
    long GameJoinByName(STRING gameName);
    long GameJoin(int index);
    long GameDisconnect();
    void SetPlayerName(STRING playerName);
    void GameStart(STRING text);
    void GameEnd();
    void NetworkOn();
    void NetworkOff();
    void SetChatColor(int index,unsigned long color);
    void SendChat(STRING text);
    STRING GetChat(int index) const;
    void SendString(int type,const STRING& text);
    STRING ReceiveString(int type,int player);
    void DestroyPlayer(int player);
    unsigned long Error(TYPE_ERROR type, const STRING& text, unsigned long detail) const;

    unsigned int IsConnected() const;
    unsigned int IsHost() const;
    unsigned int IsLobby() const;
    unsigned int NoPlayers() const;
    unsigned long PlayerDPNID(unsigned int index) const;

    struct NET_PLAYER
    {
        NET_PLAYER();
        ~NET_PLAYER();

        unsigned int m_dpnid;
        STRING       m_name;
        unsigned int m_chatColor;
        unsigned int m_0C;
        unsigned int m_pendingDestroy;
        unsigned int m_roundTripLatencyMs;
    };

    unsigned int  m_flags;
    unsigned int  m_receiveSender[64];
    unsigned int  m_receiveSize[64];
    unsigned char m_receiveData[64][1024];
    unsigned char m_receiveType[64];
    unsigned int  m_receiveRead;
    unsigned int  m_receiveWrite;
    unsigned int  m_checkListCrc;
    unsigned int  m_noPlayers;
    unsigned int  m_localPlayer;
    NET_PLAYER    m_players[8];
    STRING        m_playerName;
    unsigned int  m_chatPlayer[256];
    STRING        m_chatText[256];
    unsigned int  m_chatEnd;
};

