﻿#ifndef _INCLUDE_METAMOD_SOURCE_STUB_PLUGIN_H_
#define _INCLUDE_METAMOD_SOURCE_STUB_PLUGIN_H_

#include <ISmmPlugin.h>
#include <sh_vector.h>
#include "utlvector.h"
#include "ehandle.h"
#include <iserver.h>
#include <entity2/entitysystem.h>
#include <steam/steam_gameserver.h>
#include "irecipientfilter.h"
#include "igameevents.h"
#include "entitysystem.h"
#include "vector.h"
#include <deque>
#include <functional>
#include "utils.hpp"
#include <utlstring.h>
#include <KeyValues.h>
#include "CCSPlayerController.h"
#include "igameeventsystem.h"
#include <networksystem/inetworkserializer.h>
#include <networksystem/inetworkmessages.h>
#include <networksystem/netmessage.h>
#include "usermessages.pb.h"
#include "CGameRules.h"
#include "module.h"
#include "ctimer.h"
#include "funchook.h"
#include "include/menus.h"
#include "include/cookies.h"
#include <map>
#include <ctime>
#include <chrono>
#include <array>
#include <thread>

std::map<int, std::map<std::string, CommandCallback>> ConsoleCommands;
std::map<int, std::map<std::string, CommandCallback>> ChatCommands;

class CRecipientFilter : public IRecipientFilter
{
public:
	CRecipientFilter(NetChannelBufType_t nBufType = BUF_RELIABLE, bool bInitMessage = false) : m_bInitMessage(bInitMessage), m_nBufType(nBufType) {}

	~CRecipientFilter() override {}

	bool IsInitMessage(void) const override
	{
		return m_bInitMessage;
	}

	NetChannelBufType_t GetNetworkBufType(void) const override
	{
		return m_nBufType;
	}

	uint64 GetRecipients(void) const override
	{
		uint64 mask = 0;

		for (int i = 0; i < m_Recipients.Count(); ++i)
		{
			int slot = m_Recipients[i].Get();
			if (slot >= 0 && slot < 64)
			{
				mask |= (uint64(1) << slot);
			}
		}

		return mask;
	}

	void AddRecipient(CPlayerSlot slot)
	{
		// Don't add if it already exists
		if (m_Recipients.Find(slot) != m_Recipients.InvalidIndex())
		{
			return;
		}

		m_Recipients.AddToTail(slot);
	}

	void AddAllPlayers()
	{
		m_Recipients.RemoveAll();

		for (int i = 0; i < 64; i++)
		{
			CCSPlayerController* pPlayer = CCSPlayerController::FromSlot(i);
			if (!pPlayer) continue;

			AddRecipient(i);
		}
	}

private:
	// Can't copy this unless we explicitly do it!
	CRecipientFilter(CRecipientFilter const &source)
	{
		Assert(0);
	}

	bool m_bInitMessage;
	NetChannelBufType_t m_nBufType;
	CUtlVectorFixed<CPlayerSlot, 64> m_Recipients;
};

class CSingleRecipientFilter : public IRecipientFilter
{
public:
	CSingleRecipientFilter(int iRecipient, NetChannelBufType_t nBufType = BUF_RELIABLE, bool bInitMessage = false)
		: m_bInitMessage(bInitMessage), m_nBufType(nBufType), m_iRecipient(iRecipient)
	{
	}

	~CSingleRecipientFilter() override {}

	bool IsInitMessage(void) const override
	{
		return m_bInitMessage;
	}

	NetChannelBufType_t GetNetworkBufType(void) const override
	{
		return m_nBufType;
	}

	uint64 GetRecipients(void) const override
	{
		if (m_iRecipient >= 0 && m_iRecipient < 64)
		{
			return uint64(1) << m_iRecipient;
		}
		return 0;
	}

private:
	bool m_bInitMessage;
	NetChannelBufType_t m_nBufType;
	int m_iRecipient;
};

class Menus final : public ISmmPlugin, public IMetamodListener
{
public:
	bool Load(PluginId id, ISmmAPI* ismm, char* error, size_t maxlen, bool late);
	bool Unload(char* error, size_t maxlen);
	void AllPluginsLoaded();
	void* OnMetamodQuery(const char* iface, int* ret);
	bool FireEvent(IGameEvent* pEvent, bool bDontBroadcast);
	STEAM_GAMESERVER_CALLBACK_MANUAL(Menus, OnValidateAuthTicket, ValidateAuthTicketResponse_t, m_CallbackValidateAuthTicketResponse);
private:
	const char* GetAuthor();
	const char* GetName();
	const char* GetDescription();
	const char* GetURL();
	const char* GetLicense();
	const char* GetVersion();
	const char* GetDate();
	const char* GetLogTag();

private: // Hooks
	void ClientCommand(CPlayerSlot slot, const CCommand &args);
	void GameFrame(bool simulating, bool bFirstTick, bool bLastTick);
	void StartupServer(const GameSessionConfiguration_t& config, ISource2WorldSession*, const char*);
    void OnDispatchConCommand(ConCommandRef cmd, const CCommandContext& ctx, const CCommand& args);
	void OnClientDisconnect( CPlayerSlot slot, ENetworkDisconnectionReason reason, const char *pszName, uint64 xuid, const char *pszNetworkID );
	void OnGameServerSteamAPIActivated();
	void OnValidateAuthTicketHook(ValidateAuthTicketResponse_t *pResponse);
	void Hook_OnClientConnected( CPlayerSlot slot, const char *pszName, uint64 xuid, const char *pszNetworkID, const char *pszAddress, bool bFakePlayer );
	bool Hook_ClientConnect( CPlayerSlot slot, const char *pszName, uint64 xuid, const char *pszNetworkID, bool unk1, CBufferString *pRejectReason );
	void Hook_ClientPutInServer( CPlayerSlot slot, char const *pszName, int type, uint64 xuid );
	bool Hook_OnTakeDamage_Alive(CTakeDamageInfoContainer *pInfoContainer);
};

class MenusApi : public IMenusApi {
    void AddItemMenu(Menu& hMenu, const char* sBack, const char* sText, int iType);
    void DisplayPlayerMenu(Menu& hMenu, int iSlot, bool bClose);
    void SetExitMenu(Menu& hMenu, bool bExit);
    void SetBackMenu(Menu& hMenu, bool bBack);
    void SetTitleMenu(Menu& hMenu, const char* szTitle); 
	void ClosePlayerMenu(int iSlot);
    void SetCallback(Menu& hMenu, MenuCallbackFunc func) override {
        hMenu.hFunc = func;
    }
	std::string escapeString(const std::string& input);
	bool IsMenuOpen(int iSlot);
    void DisplayPlayerMenu(Menu& hMenu, int iSlot, bool bClose, bool bReset);
};

class UtilsApi : public IUtilsApi
{
public:
    void PrintToChat(int iSlot, const char* msg, ...);
    void PrintToChatAll(const char* msg, ...);
	void NextFrame(std::function<void()> fn);
	CCSGameRules* GetCCSGameRules();
    CGameEntitySystem* GetCGameEntitySystem();
    CEntitySystem* GetCEntitySystem();
	CGlobalVars* GetCGlobalVars();
	IGameEventManager2* GetGameEventManager();
	const char* GetLanguage();
	void LoadTranslations(const char* szFile);
	void PrintToConsole(int iSlot, const char* msg, ...);
	void PrintToConsoleAll(const char* msg, ...);
	void PrintToCenter(int iSlot, const char* msg, ...);
	void PrintToCenterAll(const char* msg, ...);
	void PrintToCenterHtml(int iSlot, int iDuration, const char* msg, ...);
	void PrintToCenterHtmlAll(int iDuration, const char* msg, ...);
	void LogToFile(const char* szFile, const char* szText, ...);
	void ErrorLog(const char* msg, ...);
	CTimer* CreateTimer(float flInterval, std::function<float()> func);
	void RemoveTimer(CTimer* pTimer);
	
	void StartupServer(SourceMM::PluginId id, StartupCallback fn) override {
		StartupHook[id].push_back(fn);
	}

	void MapEndHook(SourceMM::PluginId id, StartupCallback fn) override {
		MapEndHooks[id].push_back(fn);
	}

	void MapStartHook(SourceMM::PluginId id, MapStartCallback fn) override {
		MapStartHooks[id].push_back(fn);
	}
	
	void OnGetGameRules(SourceMM::PluginId id, StartupCallback fn) override {
		GetGameRules[id].push_back(fn);
	}

	void SetStateChanged(CBaseEntity* entity, const char* sClassName, const char* sFieldName, int extraOffset);

	void AddChatListenerPre(SourceMM::PluginId id, CommandCallbackPre callback) override {
        ChatHookPre[id].push_back(callback);
    }

	void AddChatListenerPost(SourceMM::PluginId id, CommandCallbackPost callback) override {
        ChatHookPost[id].push_back(callback);
    }

	void HookEvent(SourceMM::PluginId id, const char* sName, EventCallback callback) override {
		HookEvents[id][std::string(sName)] = callback;
	}

	void SendHookEventCallback(const char* szName, IGameEvent* pEvent, bool bDontBroadcast) {
		for(auto& item : HookEvents)
		{
			if (item.second[std::string(szName)]) {
				item.second[std::string(szName)](szName, pEvent, bDontBroadcast);
			}
		}
	}

	void SendHookStartup() {
		for(auto& item : StartupHook)
		{
			for (auto& callback : item.second) {
				if (callback) {
					callback();
				}
			}
		}
	}

	void SendHookMapEnd() {
		for(auto& item : MapEndHooks)
		{
			for (auto& callback : item.second) {
				if (callback) {
					callback();
				}
			}
		}
	}

	void SendHookMapStart(const char* szMap) {
		for(auto& item : MapStartHooks)
		{
			for (auto& callback : item.second) {
				if (callback) {
					callback(szMap);
				}
			}
		}
	}

	void SendHookGameRules() {
		for(auto& item : GetGameRules)
		{
			for (auto& callback : item.second) {
				if (callback) {
					callback();
				}
			}
		}
	}

	bool SendChatListenerPreCallback(int iSlot, const char* szContent, bool bTeam) {
		bool bFound = true;
		for(auto& item : ChatHookPre)
		{
			for (auto& callback : item.second) {
				if (callback && !callback(iSlot, szContent, bTeam)) {
					bFound = false;
				}
			}
		}
		return bFound;
	}

	bool SendChatListenerPostCallback(int iSlot, const char* szContent, bool bMute, bool bTeam) {
		bool bFound = bMute;
		for(auto& item : ChatHookPost)
		{
			for (auto& callback : item.second) {
				if (callback && !callback(iSlot, szContent, bMute, bTeam)) {
					bFound = false;
				}
			}
		}
		return bFound;
	}
	static bool FindAndSendCommandCallback(const char* szCommand, int iSlot, const char* szContent, bool bConsole) {
		bool bFound = false;
		if(bConsole)
		{
			std::string fullCommand = std::string(szCommand) + " " + std::string(szContent);
			for (auto& item : ConsoleCommands)
			{
				if (item.second[std::string(szCommand)] &&
					item.second[std::string(szCommand)](iSlot, fullCommand.c_str()))
				{
					bFound = true;
				}
			}
		}
		else
		{
			for(auto& item : ChatCommands)
			{
				if(item.second[std::string(szCommand)] && item.second[std::string(szCommand)](iSlot, szContent))
				{
					bFound = true;
				}
			}
		}
		return bFound;
	}
	
	static void CommandHandler(const CCommandContext& context, const CCommand& args) {
		FindAndSendCommandCallback(args.Arg(0), context.GetPlayerSlot().Get(), args.ArgS(), true);
	}

	void RegCommand(SourceMM::PluginId id, const std::vector<std::string> &console, const std::vector<std::string> &chat, const CommandCallback &callback) override {
		for (const auto &element : console) {
			ConsoleCommands[id][element] = callback;
			if (element.find("mm_") == std::string::npos) {
				continue;
			}
			new ConCommand(
				element.c_str(),
				CommandHandler,
				"",
				FCVAR_LINKED_CONCOMMAND | FCVAR_SERVER_CAN_EXECUTE | FCVAR_CLIENT_CAN_EXECUTE
			);			
		}
	
		for (const auto &element : chat) {
			ChatCommands[id][element] = callback;
		}
	}

	bool FindCommand(const char* szCommand) {
		bool bFound = false;
		for(auto& item : ConsoleCommands)
		{
			if(item.second[std::string(szCommand)])
			{
				bFound = true;
			}
		}
		for(auto& item : ChatCommands)
		{
			if(item.second[std::string(szCommand)])
			{
				bFound = true;
			}
		}
		return bFound;
	}

	
	void PrintToAlert(int iSlot, const char *msg, ...);
	void PrintToAlertAll(const char *msg, ...);
	
	void SetEntityModel(CBaseModelEntity*, const char* szModel);
	void DispatchSpawn(CEntityInstance* pEntity, CEntityKeyValues* pKeyValues);
	CBaseEntity* CreateEntityByName(const char *pClassName, CEntityIndex iForceEdictIndex);
	void RemoveEntity(CEntityInstance* pEntity);
	void AcceptEntityInput(CEntityInstance* pEntity, const char* szInputName, variant_t value, CEntityInstance *pActivator, CEntityInstance *pCaller);
	void CollisionRulesChanged(CBaseEntity* pEnt);
	void TeleportEntity(CBaseEntity* pEnt, const Vector *position, const QAngle *angles, const Vector *velocity);
	
	void ClearAllHooks(SourceMM::PluginId id) override {
		ConsoleCommands[id].clear();
		ChatCommands[id].clear();
		HookEvents[id].clear();
		ChatHookPre[id].clear();
		ChatHookPost[id].clear();
		StartupHook[id].clear();
		MapEndHooks[id].clear();
		MapStartHooks[id].clear();
		GetGameRules[id].clear();
		OnTakeDamageHookPre[id].clear();
		OnTakeDamageHook[id].clear();
	}
	
	void NextFrame() {
		while (!m_nextFrame.empty())
		{
			m_nextFrame.front()();
			m_nextFrame.pop_front();
		}
	}

	void HookOnTakeDamage(SourceMM::PluginId id, OnTakeDamageCallback callback) override {
		OnTakeDamageHook[id].push_back(callback);
	}

	void HookOnTakeDamagePre(SourceMM::PluginId id, OnTakeDamagePreCallback callback) override {
		OnTakeDamageHookPre[id].push_back(callback);
	}

	void HookIsHearingClient(SourceMM::PluginId id, OnHearingClientCallback callback) override {
		OnHearingClientHook[id].push_back(callback);
	}

	bool SendHookOnTakeDamage(int iSlot, CTakeDamageInfoContainer* &pInfoContainer) {
		bool bFound = true;
		for(auto& item : OnTakeDamageHook)
		{
			for (auto& callback : item.second) {
				if (callback && !callback(iSlot, pInfoContainer)) {
					bFound = false;
				}
			}
		}
		return bFound;
	}
	bool SendHookOnTakeDamagePre(int iSlot, CTakeDamageInfo &pInfo) {
		bool bFound = true;
		for(auto& item : OnTakeDamageHookPre)
		{
			for (auto& callback : item.second) {
				if (callback && !callback(iSlot, pInfo)) {
					bFound = false;
				}
			}
		}
		return bFound;
	}
	bool SendHookOnHearingClient(int iSlot) {
		bool bFound = true;
		for(auto& item : OnHearingClientHook)
		{
			for (auto& callback : item.second) {
				if (callback && !callback(iSlot)) {
					bFound = false;
				}
			}
		}
		return bFound;
	}

	const char* GetVersion();
private:
    std::map<int, std::vector<CommandCallbackPre>> ChatHookPre;
    std::map<int, std::vector<CommandCallbackPost>> ChatHookPost;

    std::map<int, std::vector<StartupCallback>> StartupHook;
	std::map<int, std::vector<StartupCallback>> MapEndHooks;
	std::map<int, std::vector<MapStartCallback>> MapStartHooks;
    std::map<int, std::vector<StartupCallback>> GetGameRules;

    std::map<int, std::map<std::string, EventCallback>> HookEvents;

	std::map<int, std::vector<OnTakeDamageCallback>> OnTakeDamageHook;
	std::map<int, std::vector<OnTakeDamagePreCallback>> OnTakeDamageHookPre;

	std::map<int, std::vector<OnHearingClientCallback>> OnHearingClientHook;

	std::deque<std::function<void()>> m_nextFrame;
};

class Player
{
public:
	Player(int iSlot, bool bFakeClient = false) : m_iSlot(iSlot), m_bFakeClient(bFakeClient) {
		m_bAuthenticated = false;
		m_bConnected = false;
		m_bInGame = false;
		m_SteamID = nullptr;
	}
	bool IsFakeClient() { return m_bFakeClient; }
	bool IsAuthenticated() { return m_bAuthenticated; }
	bool IsConnected() { return m_bConnected; }
	bool IsInGame() { return m_bInGame; }
	
	const char* GetIpAddress() { return m_strIp.c_str(); }

	uint64 GetUnauthenticatedSteamId64() { return m_UnauthenticatedSteamID->ConvertToUint64(); }
	const CSteamID* GetUnauthenticatedSteamId() { return m_UnauthenticatedSteamID; }

	uint64 GetSteamId64() { return m_SteamID?m_SteamID->ConvertToUint64():0; }
	const CSteamID* GetSteamId() { return m_SteamID; }

	void SetAuthenticated(bool bAuthenticated) { m_bAuthenticated = bAuthenticated; }
	void SetInGame(bool bInGame) { m_bInGame = bInGame; }
	void SetConnected() { m_bConnected = true; }

	void SetUnauthenticatedSteamId(const CSteamID* steamID) { m_UnauthenticatedSteamID = steamID; }
	
	void SetSteamId(const CSteamID* steamID) { m_SteamID = steamID; }

	void SetIpAddress(std::string strIp) { m_strIp = strIp; }
private:
	int m_iSlot;
	bool m_bFakeClient = false;
	bool m_bAuthenticated = false;
	bool m_bConnected = false;
	bool m_bInGame = false;
	std::string m_strIp;
	const CSteamID* m_UnauthenticatedSteamID;
	const CSteamID* m_SteamID;
};

Player* m_Players[64];

class PlayersApi : public IPlayersApi
{
public:
	bool IsFakeClient(int iSlot) {
		if (iSlot < 0 || iSlot >= 64) {
			return true;
		}
		if(m_Players[iSlot] == nullptr)
			return true;
		else
			return m_Players[iSlot]->IsFakeClient();
	}
	bool IsAuthenticated(int iSlot) {
		if (iSlot < 0 || iSlot >= 64) {
			return false;
		}
		if(m_Players[iSlot] == nullptr)
			return false;
		else
			return m_Players[iSlot]->IsAuthenticated();
	}
	bool IsConnected(int iSlot) {
		if (iSlot < 0 || iSlot >= 64) {
			return false;
		}
		if(m_Players[iSlot] == nullptr)
			return false;
		else
			return m_Players[iSlot]->IsConnected();
	}
	bool IsInGame(int iSlot) {
		if (iSlot < 0 || iSlot >= 64) {
			return false;
		}
		if(m_Players[iSlot] == nullptr)
			return false;
		else
			return m_Players[iSlot]->IsInGame();
	}
	const char* GetIpAddress(int iSlot) {
		if (iSlot < 0 || iSlot >= 64) {
			return "";
		}
		if(m_Players[iSlot] == nullptr)
			return "";
		else
			return m_Players[iSlot]->GetIpAddress();
	}
	uint64 GetSteamID64(int iSlot) {
		if (iSlot < 0 || iSlot >= 64) {
			return 0;
		}
		if(m_Players[iSlot] == nullptr)
			return 0;
		else
			return m_Players[iSlot]->GetSteamId64();
	}
	const CSteamID* GetSteamID(int iSlot) {
		if (iSlot < 0 || iSlot >= 64) {
			return nullptr;
		}
		if(m_Players[iSlot] == nullptr)
			return nullptr;
		else
			return m_Players[iSlot]->GetSteamId();
	}
	
	void HookOnClientAuthorized(SourceMM::PluginId id, OnClientAuthorizedCallback callback) override {
		m_OnClientAuthorized[id].push_back(callback);
	}

	void SendClientAuthCallback(int iSlot, uint64 steamID) {
		for(auto& item : m_OnClientAuthorized)
		{
			for (auto& callback : item.second) {
				if (callback) {
					callback(iSlot, steamID);
				}
			}
		}
	}
	
	void CommitSuicide(int iSlot, bool bExplode, bool bForce);
	void ChangeTeam(int iSlot, int iNewTeam);
	void Teleport(int iSlot, const Vector *position, const QAngle *angles, const Vector *velocity);
	void Respawn(int iSlot);
	void DropWeapon(int iSlot, CBaseEntity* pWeapon, Vector* pVecTarget, Vector* pVelocity);
	void SwitchTeam(int iSlot, int iNewTeam);
	const char* GetPlayerName(int iSlot);
	void SetPlayerName(int iSlot, const char* szName);
	void SetMoveType(int iSlot, MoveType_t moveType);
	void EmitSound(std::vector<int> vPlayers, CEntityIndex ent, std::string sound_name, int pitch, float volume);
	void EmitSound(int iSlot, CEntityIndex ent, std::string sound_name, int pitch, float volume);
	void StopSoundEvent(int iSlot, const char* sound_name);
	IGameEventListener2* GetLegacyGameEventListener(int iSlot);
	int FindPlayer(uint64 iSteamID64);
	int FindPlayer(const CSteamID* steamID);
	int FindPlayer(const char* szName);
private:
	std::map<int, std::vector<OnClientAuthorizedCallback>> m_OnClientAuthorized;
};

enum MsgDest : int32_t
{
	HUD_PRINTNOTIFY = 1,
	HUD_PRINTCONSOLE = 2,
	HUD_PRINTTALK = 3,
	HUD_PRINTCENTER = 4,
	HUD_PRINTTALK2 = 5, // Not sure what the difference between this and HUD_PRINTTALK is...
	HUD_PRINTALERT = 6
};

const std::string colors_text[] = {
    "{DEFAULT}",
    "{WHITE}",
    "{RED}",
    "{LIGHTPURPLE}",
    "{GREEN}",
    "{LIME}",
    "{LIGHTGREEN}",
    "{DARKRED}",
    "{GRAY}",
    "{LIGHTOLIVE}",
    "{OLIVE}",
    "{LIGHTBLUE}",
    "{BLUE}",
    "{PURPLE}",
    "{LIGHTRED}",
    "{GRAYBLUE}",
    "\\n"
};

const std::string colors_hex[] = {
    "\x01",
    "\x01",
    "\x02",
    "\x03",
    "\x04",
    "\x05",
    "\x06",
    "\x07",
    "\x08",
    "\x09",
    "\x10",
    "\x0B",
    "\x0C",
    "\x0E",
    "\x0F",
    "\x0A",
    "\xe2\x80\xa9"
};

#endif //_INCLUDE_METAMOD_SOURCE_STUB_PLUGIN_H_
