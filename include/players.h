#pragma once

#include <functional>
#include <string>
#include <vector>

class CBaseEntity;
class CSteamID;
class CTakeDamageInfo;
class IGameEventListener2;
class CCommand;
class KeyValues;
class CCLCMsg_Move_t;
class CNETMsg_Tick_t;
class CNETMsg_StringCmd_t;

struct trace_info_t;

#define PLAYERS_INTERFACE "IPlayersApi"

/////////////////////////////////////////////////////////////////
///////////////////////      PLAYERS     //////////////////////////
/////////////////////////////////////////////////////////////////

typedef std::function<void(int iSlot, uint64 iSteamID64)> OnClientAuthorizedCallback;

struct FakeConVar
{
    std::string szCvar;
    std::string szValue;
};

class IPlayerListener {
public:
	virtual ~IPlayerListener() = default;

    virtual void OnClientConnected(int iSlot) { };
    virtual bool ClientConnect(int iSlot) { return true; };
    virtual void ClientPutInServer(int iSlot) { };
    virtual void ClientActive(int iSlot) { };
    virtual void ClientFullyConnect(int iSlot) { };
    virtual void ClientDisconnect(int iSlot) { };
    virtual void OnClientAuthorized(int iSlot, uint64 iSteamID64) { };
    // Вызывается один раз при заходе игрока
    virtual void OnClientSessionStart(int iSlot) { };
    // Вызывается один раз при выходе игрока
    virtual void OnClientSessionEnd(int iSlot) { };

    virtual void ClientCommand(int iSlot, const CCommand &args) { };
	virtual void ClientSettingsChanged(int iSlot) { };
	virtual void ProcessUsercmds(int iSlot, const CCLCMsg_Move_t &msg, bool paused) { };
	virtual void ClientVoice(int iSlot) { };
	virtual void ClientCommandKeyValues(int iSlot, KeyValues *pKeyValues) { };
	virtual void ClientSvcUserMessage(int iSlot, int um_type, uint32 size, const void *buf) { };
	virtual bool ProcessClientVoiceData(int iSlot, void *pVoiceInfo) { return true; };
    virtual bool ProcessTick(int iSlot, const CNETMsg_Tick_t& msg) { return true; };
    virtual bool ProcessStringCmd(int iSlot, const CNETMsg_StringCmd_t& msg) { return true; };
};

class IPlayersApi
{
public:
    virtual bool IsFakeClient(int iSlot) = 0;
    virtual bool IsAuthenticated(int iSlot) = 0;
    virtual bool IsConnected(int iSlot) = 0;
    virtual bool IsInGame(int iSlot) = 0;
    virtual const char* GetIpAddress(int iSlot) = 0;
    virtual uint64 GetSteamID64(int iSlot) = 0;
    virtual const CSteamID* GetSteamID(int iSlot) = 0;

    virtual void HookOnClientAuthorized(SourceMM::PluginId id, OnClientAuthorizedCallback callback) = 0;

    virtual void CommitSuicide(int iSlot, bool bExplode, bool bForce) = 0;
    virtual void ChangeTeam(int iSlot, int iNewTeam) = 0;
    virtual void Teleport(int iSlot, const Vector *position, const QAngle *angles, const Vector *velocity) = 0;
    virtual void Respawn(int iSlot) = 0;
    virtual void DropWeapon(int iSlot, CBaseEntity* pWeapon, Vector* pVecTarget = nullptr, Vector* pVelocity = nullptr) = 0;
    virtual void SwitchTeam(int iSlot, int iNewTeam) = 0;
    virtual const char* GetPlayerName(int iSlot) = 0;
    virtual void SetPlayerName(int iSlot, const char* szName) = 0;
    virtual void SetMoveType(int iSlot, MoveType_t moveType) = 0;
    virtual void EmitSound(std::vector<int> vPlayers, CEntityIndex ent, std::string sound_name, int pitch, float volume) = 0;
	virtual void EmitSound(int iSlot, CEntityIndex ent, std::string sound_name, int pitch, float volume) = 0;
	virtual void StopSoundEvent(int iSlot, const char* sound_name) = 0;
    virtual IGameEventListener2* GetLegacyGameEventListener(int iSlot) = 0;
    virtual int FindPlayer(uint64 iSteamID64) = 0;
    virtual int FindPlayer(const CSteamID* steamID) = 0;
    virtual int FindPlayer(const char* szName) = 0;
    virtual trace_info_t RayTrace(int iSlot) = 0;
    virtual bool UseClientCommand(int iSlot, const char* szCommand) = 0;
    // bHook - если true, то вызов будет через хук OnTakeDamage с возможностью отмены, если false - прямой вызов функции нанесения урона без хуков
    virtual void TakeDamage(int iSlot, CTakeDamageInfo* pInfo, bool bHook = true) = 0;
    virtual void RemoveWeapons(int iSlot) = 0;
    
    virtual void SetConVar(int iSlot, FakeConVar cvar) = 0;
    virtual void SetConVar(int iSlot, const char* name, const char* value) = 0;
    virtual void SetConVar(std::vector<int> vPlayers, const char* name, const char* value) = 0;
    virtual void SetConVar(std::vector<int> vPlayers, FakeConVar cvar) = 0;
    virtual void SetConVars(int iSlot, std::vector<FakeConVar> cvars) = 0;
    virtual void SetConVars(std::vector<int> vPlayers, std::vector<FakeConVar> cvars) = 0;

    virtual void AddListener(SourceMM::PluginId id, IPlayerListener* pListener) = 0;
};
