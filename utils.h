#ifndef _INCLUDE_METAMOD_SOURCE_STUB_PLUGIN_H_
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
#include "entitykeyvalues.h"
#include <deque>
#include <stack>
#include <functional>
#include "utils.hpp"
#include <utlstring.h>
#include <keyvalues.h>
#include "CCSPlayerController.h"
#include "CCSCustomHudLayout.h"
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
#include "include/players.h"
#include "include/utils.h"
#include "include/layouts.h"
#include "include/cookies.h"
#include <map>
#include <unordered_map>
#include <ctime>
#include <chrono>
#include <array>
#include <thread>
#include <netmessages.h>
#include <igamesystem.h>
#include <usermessages.h>
#include "customhud.pb.h"

static constexpr int CS_UM_CustomHudClicked = 390;
class CCSUsrMsg_CustomHudClicked_t
    : public CUserMessagePB<CS_UM_CustomHudClicked, CCSUsrMsg_CustomHudClicked> {};

class CPhysicsQuery;

class CRecipientFilter : public IRecipientFilter
{
public:
	CRecipientFilter(NetChannelBufType_t nBufType = BUF_RELIABLE, bool bInitMessage = false) :
		m_nBufType(nBufType), m_bInitMessage(bInitMessage) {}

	CRecipientFilter(IRecipientFilter* source, int exceptSlot = -1)
	{
		m_Recipients = source->GetRecipients();
		m_nBufType = source->GetNetworkBufType();
		m_bInitMessage = source->IsInitMessage();

		if (exceptSlot != -1)
			m_Recipients.Clear(exceptSlot);
	}

	~CRecipientFilter() override {}

	NetChannelBufType_t GetNetworkBufType(void) const override { return m_nBufType; }
	bool IsInitMessage(void) const override { return m_bInitMessage; }
	const CPlayerBitVec& GetRecipients(void) const override { return m_Recipients; }
	CPlayerSlot GetPredictedByPlayerSlot() const override { return m_nPredictedByPlayerSlot; }

	void AddRecipient(int iSlot)
	{
		if (iSlot >= 0 && iSlot < 64)
			m_Recipients.Set(iSlot);
	}

protected:
	NetChannelBufType_t m_nBufType;
	CPlayerSlot m_nPredictedByPlayerSlot;
	bool m_bInitMessage;
	CPlayerBitVec m_Recipients;
};

class CSingleRecipientFilter : public CRecipientFilter
{
public:
	CSingleRecipientFilter(CPlayerSlot nRecipientSlot, NetChannelBufType_t nBufType = BUF_RELIABLE, bool bInitMessage = false) :
		CRecipientFilter(nBufType, bInitMessage)
	{
		if (nRecipientSlot.Get() >= 0 && nRecipientSlot.Get() < ABSOLUTE_PLAYER_LIMIT)
			m_Recipients.Set(nRecipientSlot.Get());
	}
};

extern std::map<int, std::map<std::string, CommandCallback>> ConsoleCommands;
extern std::map<int, std::map<std::string, CommandCallback>> ChatCommands;

class Menus final : public ISmmPlugin, public IMetamodListener, public IEntityListener
{
public:
	bool Load(PluginId id, ISmmAPI* ismm, char* error, size_t maxlen, bool late);
	bool Unload(char* error, size_t maxlen);
	void AllPluginsLoaded();
	void* OnMetamodQuery(const char* iface, int* ret);
	void OnPluginUnload(PluginId id);
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

private:
	void ClientCommand(CPlayerSlot slot, const CCommand &args);
	void GameFrame(bool simulating, bool bFirstTick, bool bLastTick);
	void StartupServer(const GameSessionConfiguration_t& config, ISource2WorldSession*, const char*);
    void OnDispatchConCommand(ConCommandRef cmd, const CCommandContext& ctx, const CCommand& args);
	void OnClientDisconnect( CPlayerSlot slot, ENetworkDisconnectionReason reason, const char *pszName, uint64 xuid, const char *pszNetworkID );
	void OnGameServerSteamAPIActivated();
	void OnPreWorldUpdate(bool simulating);
	void OnServerHibernationUpdate(bool bHibernating);
	void OnGameServerSteamAPIDeactivated();
	void OnHostNameChanged(const char *pHostname);
	void OnPreFatalShutdown() const;
	void OnUpdateWhenNotInGame(float flFrameTime);
	void OnServerConVarChanged(const char *pVarName, const char *pValue);
	void OnPostEventAbstract(CSplitScreenSlot nSlot, bool bLocalOnly, int nClientCount, const uint64 *clients, INetworkMessageInternal *pEvent, const CNetMessage *pData, unsigned long nSize, NetChannelBufType_t bufType);
	bool OnSetClientListening(CPlayerSlot iReceiver, CPlayerSlot iSender, bool bListen);

	void OnGameFramePost(bool simulating, bool bFirstTick, bool bLastTick);
	void OnPreWorldUpdatePost(bool simulating);
	void OnServerHibernationUpdatePost(bool bHibernating);
	void OnGameServerSteamAPIActivatedPost();
	void OnGameServerSteamAPIDeactivatedPost();
	void OnHostNameChangedPost(const char *pHostname);
	void OnPreFatalShutdownPost() const;
	void OnUpdateWhenNotInGamePost(float flFrameTime);
	void OnServerConVarChangedPost(const char *pVarName, const char *pValue);
	void OnPostEventAbstractPost(CSplitScreenSlot nSlot, bool bLocalOnly, int nClientCount, const uint64 *clients, INetworkMessageInternal *pEvent, const CNetMessage *pData, unsigned long nSize, NetChannelBufType_t bufType);
	bool OnSetClientListeningPost(CPlayerSlot iReceiver, CPlayerSlot iSender, bool bListen);
	void OnDispatchConCommandPost(ConCommandRef cmd, const CCommandContext& ctx, const CCommand& args);
	bool OnFireEventPost(IGameEvent* pEvent, bool bDontBroadcast);
	void OnValidateAuthTicketHook(ValidateAuthTicketResponse_t *pResponse);
	void OnClientConnected( CPlayerSlot slot, const char *pszName, uint64 xuid, const char *pszNetworkID, const char *pszAddress, bool bFakePlayer );
	bool OnClientConnect( CPlayerSlot slot, const char *pszName, uint64 xuid, const char *pszNetworkID, bool unk1, CBufferString *pRejectReason );
	void OnClientPutInServer( CPlayerSlot slot, char const *pszName, int type, uint64 xuid );
	void OnClientActive( CPlayerSlot slot, bool bLoadGame, char const *pszName, uint64 xuid );
	void OnClientFullyConnect( CPlayerSlot slot );
	void OnClientSettingsChanged( CPlayerSlot slot );
	void OnProcessUsercmds( CPlayerSlot slot, const CCLCMsg_Move_t &msg, bool paused );
	void OnClientVoice( CPlayerSlot slot );
	void OnClientCommandKeyValues( CPlayerSlot slot, KeyValues *pKeyValues );
	bool OnProcessClientVoiceData( CPlayerSlot slot, void *pVoiceInfo );
	void OnCheckTransmit(CCheckTransmitInfo **pInfoInfoList, int nInfoCount, CBitVec<16384> &unionTransmitEdicts, CBitVec<16384> &, const Entity2Networkable_t **pNetworkables, const uint16 *pEntityIndicies, int nEntityIndices, bool bEnablePVSBits);
	void OnClientSvcUserMessage( CPlayerSlot slot, int um_type, uint32 size, const void *buf );
	bool Hook_OnTakeDamage_Alive(CTakeDamageInfoContainer *pInfoContainer);

	void OnServerSideClientClientConnect(int socket, const char* pszName, int nUserID, INetChannel* pNetChannel, uint8 nConnectionTypeFlags, uint32 uChallengeNumber);
	void OnCServerSideClientlientPerformDisconnection(ENetworkDisconnectionReason reason);
	bool OnProcessTick(const CNETMsg_Tick_t& msg);
	bool OnProcessStringCmd(const CNETMsg_StringCmd_t& msg);

	void OnEntityCreated(CEntityInstance* pEntity) override;
	void OnEntitySpawned(CEntityInstance* pEntity) override;
	void OnEntityDeleted(CEntityInstance* pEntity) override;
	void OnEntityParentChanged(CEntityInstance* pEntity, CEntityInstance* pNewParent) override;

	void OnBuildGameSessionManifest(const EventBuildGameSessionManifest_t& msg);

	std::stack<IGameEvent*> m_EventCopies;
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
	void AddRawItemMenu(Menu &hMenu, const char* sBack, const char* sText, int iType);
	MenuType GetMenuType(int iSlot);
	void SetDescriptionMenu(Menu& hMenu, const char* szDescription);
	void AddToggleMenu(Menu& hMenu, const char* sBack, const char* sText, bool bDefault, MenuToggleCallbackFunc func, int iType);
	ItemRef AddSelectMenu(Menu& hMenu, const char* sBack, const char* sText, int iDefault, MenuSelectCallbackFunc func, int iType);
	void AddSelectOption(ItemRef hItem, const char* sOptionBack, const char* sOptionText);
	void SetToggleState(ItemRef hItem, bool bState);
	bool GetToggleState(ItemRef hItem);
	void SetSelectedOption(ItemRef hItem, int iOption);
	int GetSelectedOption(ItemRef hItem);
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
	void TerminateRound(int reason, float delay, int64 teamid) override;
	void AddPrecache(const char* szResource) override;

	void AddCanAcquirePre(SourceMM::PluginId id, CanAcquireCallback callback) override {
		m_CanAcquirePre[id].push_back(callback);
	}
	void AddCanAcquirePost(SourceMM::PluginId id, CanAcquireCallback callback) override {
		m_CanAcquirePost[id].push_back(callback);
	}

	bool HasCanAcquirePre() const { return !m_CanAcquirePre.empty(); }

	AcquireResult::Type SendCanAcquirePre(int iSlot, CPlayer_ItemServices* pItemServices, CEconItemView* pItemView, AcquireMethod::Type eMethod, bool& bHandled) {
		bHandled = false;
		int bestPriority = -1;
		AcquireResult::Type priorityType = AcquireResult::Allowed;
		AcquireResult::Type typedResult = AcquireResult::Allowed;
		bool bHasTyped = false;
		for (auto& item : m_CanAcquirePre) {
			for (auto& cb : item.second) {
				if (!cb) continue;
				AcquireResultInfo info = cb(iSlot, pItemServices, pItemView, eMethod);
				if (info.priority >= 0) {
					if (info.priority > bestPriority) {
						bestPriority = info.priority;
						priorityType = info.type;
					}
				} else if (info.type != AcquireResult::Allowed && !bHasTyped) {
					bHasTyped = true;
					typedResult = info.type;
				}
			}
		}
		if (bestPriority >= 0) {
			bHandled = true;
			return priorityType;
		}
		if (bHasTyped) {
			bHandled = true;
			return typedResult;
		}
		return AcquireResult::Allowed;
	}

	void SendCanAcquirePost(int iSlot, CPlayer_ItemServices* pItemServices, CEconItemView* pItemView, AcquireMethod::Type eMethod) {
		for (auto& item : m_CanAcquirePost) {
			for (auto& cb : item.second) {
				if (cb) cb(iSlot, pItemServices, pItemView, eMethod);
			}
		}
	}
	
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

	void SetStateChanged(CBaseEntity* entity, const char* sClassName, const char* sFieldName, int extraOffset = 0);

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

	void HookEventPre(SourceMM::PluginId id, const char* szName, EventHookPreCallback callback) override {
		m_EventHookPre[id][std::string(szName)].push_back(callback);
	}

	void HookEventPost(SourceMM::PluginId id, const char* szName, EventHookPostCallback callback) override {
		m_EventHookPost[id][std::string(szName)].push_back(callback);
	}

	EventHookResult SendEventHookPre(const char* szName, IGameEvent* pEvent, EventInfo* info) {
		EventHookResult result = EventHookResult::Continue;
		std::string name(szName);
		for (auto& item : m_EventHookPre) {
			auto it = item.second.find(name);
			if (it == item.second.end()) continue;
			for (auto& cb : it->second) {
				if (!cb) continue;
				EventHookResult r = cb(szName, pEvent, info);
				if (r > result) result = r;
			}
		}
		return result;
	}

	void SendEventHookPost(const char* szName, IGameEvent* pEvent, bool bDontBroadcast) {
		std::string name(szName);
		for (auto& item : m_EventHookPost) {
			auto it = item.second.find(name);
			if (it == item.second.end()) continue;
			for (auto& cb : it->second) {
				if (cb) cb(szName, pEvent, bDontBroadcast);
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

	void AddServerListener(SourceMM::PluginId id, IServerListener* pListener) override {
		if (pListener)
			m_ServerListeners[id] = pListener;
	}

	void SendServerStartup() {
		for (auto& item : m_ServerListeners)
			if (item.second) item.second->StartupServer();
	}
	void SendServerMapEnd() {
		for (auto& item : m_ServerListeners)
			if (item.second) item.second->MapEndHook();
	}
	void SendServerMapStart(const char* szMap) {
		for (auto& item : m_ServerListeners)
			if (item.second) item.second->MapStartHook(szMap);
	}
	void SendServerPreWorldUpdate(bool bSimulating) {
		for (auto& item : m_ServerListeners)
			if (item.second) item.second->PreWorldUpdate(bSimulating);
	}
	void SendServerGameFrame(bool bSimulating, bool bFirstTick, bool bLastTick) {
		for (auto& item : m_ServerListeners)
			if (item.second) item.second->GameFrame(bSimulating, bFirstTick, bLastTick);
	}
	void SendServerHibernationUpdate(bool bHibernating) {
		for (auto& item : m_ServerListeners)
			if (item.second) item.second->ServerHibernationUpdate(bHibernating);
	}
	void SendServerSteamAPIActivated() {
		for (auto& item : m_ServerListeners)
			if (item.second) item.second->GameServerSteamAPIActivated();
	}
	void SendServerSteamAPIDeactivated() {
		for (auto& item : m_ServerListeners)
			if (item.second) item.second->GameServerSteamAPIDeactivated();
	}
	void SendServerHostNameChanged(const char* pHostname) {
		for (auto& item : m_ServerListeners)
			if (item.second) item.second->OnHostNameChanged(pHostname);
	}
	void SendServerPreFatalShutdown() {
		for (auto& item : m_ServerListeners)
			if (item.second) item.second->PreFatalShutdown();
	}
	void SendServerUpdateWhenNotInGame(float flFrameTime) {
		for (auto& item : m_ServerListeners)
			if (item.second) item.second->UpdateWhenNotInGame(flFrameTime);
	}
	void SendServerConVarChanged(const char* pVarName, const char* pValue) {
		for (auto& item : m_ServerListeners)
			if (item.second) item.second->ServerConVarChanged(pVarName, pValue);
	}
	bool SendServerSetClientListening(int iReceiver, int iSender, bool bListen) {
		bool bResult = bListen;
		for (auto& item : m_ServerListeners)
			if (item.second && !item.second->SetClientListening(iReceiver, iSender, bResult)) bResult = false;
		return bResult;
	}
	void SendServerPostEvent(int nClientCount, const uint64 *clients, INetworkMessageInternal *pEvent, const CNetMessage *pData) {
		for (auto& item : m_ServerListeners)
			if (item.second) item.second->PostEvent(nClientCount, clients, pEvent, pData);
	}
	bool SendServerDispatchConCommand(int iSlot, const CCommand &args) {
		bool bAllow = true;
		for (auto& item : m_ServerListeners)
			if (item.second && !item.second->DispatchConCommand(iSlot, args)) bAllow = false;
		return bAllow;
	}
	void SendServerFireEvent(IGameEvent *pEvent, bool bDontBroadcast) {
		for (auto& item : m_ServerListeners)
			if (item.second) item.second->FireEvent(pEvent, bDontBroadcast);
	}

	void SendServerPreWorldUpdatePost(bool bSimulating) {
		for (auto& item : m_ServerListeners)
			if (item.second) item.second->PreWorldUpdatePost(bSimulating);
	}
	void SendServerGameFramePost(bool bSimulating, bool bFirstTick, bool bLastTick) {
		for (auto& item : m_ServerListeners)
			if (item.second) item.second->GameFramePost(bSimulating, bFirstTick, bLastTick);
	}
	void SendServerHibernationUpdatePost(bool bHibernating) {
		for (auto& item : m_ServerListeners)
			if (item.second) item.second->ServerHibernationUpdatePost(bHibernating);
	}
	void SendServerSteamAPIActivatedPost() {
		for (auto& item : m_ServerListeners)
			if (item.second) item.second->GameServerSteamAPIActivatedPost();
	}
	void SendServerSteamAPIDeactivatedPost() {
		for (auto& item : m_ServerListeners)
			if (item.second) item.second->GameServerSteamAPIDeactivatedPost();
	}
	void SendServerHostNameChangedPost(const char* pHostname) {
		for (auto& item : m_ServerListeners)
			if (item.second) item.second->OnHostNameChangedPost(pHostname);
	}
	void SendServerPreFatalShutdownPost() {
		for (auto& item : m_ServerListeners)
			if (item.second) item.second->PreFatalShutdownPost();
	}
	void SendServerUpdateWhenNotInGamePost(float flFrameTime) {
		for (auto& item : m_ServerListeners)
			if (item.second) item.second->UpdateWhenNotInGamePost(flFrameTime);
	}
	void SendServerConVarChangedPost(const char* pVarName, const char* pValue) {
		for (auto& item : m_ServerListeners)
			if (item.second) item.second->ServerConVarChangedPost(pVarName, pValue);
	}
	void SendServerSetClientListeningPost(int iReceiver, int iSender, bool bListen) {
		for (auto& item : m_ServerListeners)
			if (item.second) item.second->SetClientListeningPost(iReceiver, iSender, bListen);
	}
	void SendServerPostEventPost(int nClientCount, const uint64 *clients, INetworkMessageInternal *pEvent, const CNetMessage *pData) {
		for (auto& item : m_ServerListeners)
			if (item.second) item.second->PostEventPost(nClientCount, clients, pEvent, pData);
	}
	void SendServerDispatchConCommandPost(int iSlot, const CCommand &args) {
		for (auto& item : m_ServerListeners)
			if (item.second) item.second->DispatchConCommandPost(iSlot, args);
	}
	void SendServerFireEventPost(IGameEvent *pEvent, bool bDontBroadcast) {
		for (auto& item : m_ServerListeners)
			if (item.second) item.second->FireEventPost(pEvent, bDontBroadcast);
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
		ChatHookPre[id].clear();
		ChatHookPost[id].clear();

		StartupHook[id].clear();
		MapEndHooks[id].clear();
		MapStartHooks[id].clear();
		GetGameRules[id].clear();

		HookEvents[id].clear();
		m_EventHookPre[id].clear();
		m_EventHookPost[id].clear();
		m_CanAcquirePre[id].clear();
		m_CanAcquirePost[id].clear();

		OnTakeDamageHook[id].clear();
		OnTakeDamageHookPre[id].clear();
		
		OnHearingClientHook[id].clear();

		m_OnSettingsOpen[id].clear();
		m_OnSettingsItem[id].clear();

		m_EntityListeners.erase(id);

		m_ServerListeners.erase(id);

		ConsoleCommands[id].clear();
		ChatCommands[id].clear();

		m_nextFrame.clear();
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
	bool SendHookOnTakeDamagePre(int iSlot, CTakeDamageInfo* pInfo) {
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
	const char* GetServerID();

	void SetTransmitState(int iEntityIndex, bool bState, std::vector<int> vecSlots);
	void ShowNotify(int iSlot, int iType, const char* szTitle, const char* szText, float flDuration = 5.0f, int iPos = 0, const char* szChatFallback = nullptr);
	void ShowNotifyAll(int iType, const char* szTitle, const char* szText, float flDuration = 5.0f, int iPos = 0, const char* szChatFallback = nullptr);

	void OpenSettingsMenu(int iSlot) override;

	void HookOnSettingsOpen(SourceMM::PluginId id, OnSettingsOpenCallback callback) override {
		m_OnSettingsOpen[id].push_back(callback);
	}

	void HookOnSettingsItem(SourceMM::PluginId id, OnSettingsItemCallback callback) override {
		m_OnSettingsItem[id].push_back(callback);
	}

	void SendHookOnSettingsOpen(int iSlot, Menu& hMenu) {
		for (auto& item : m_OnSettingsOpen) {
			for (auto& cb : item.second) {
				if (cb) cb(iSlot, hMenu);
			}
		}
	}

	void SendHookOnSettingsItem(const char* szBack, const char* szFront, int iItem, int iSlot) {
		for (auto& item : m_OnSettingsItem) {
			for (auto& cb : item.second) {
				if (cb) cb(szBack, szFront, iItem, iSlot);
			}
		}
	}

	void AddEntityListener(SourceMM::PluginId id, IUtilsEntityListener* pListener) override {
		if (pListener)
			m_EntityListeners[id] = pListener;
	}

	void SendEntityCreated(CEntityInstance* pEntity) {
		for (auto& item : m_EntityListeners)
			if (item.second) item.second->OnEntityCreated(pEntity);
	}
	void SendEntitySpawned(CEntityInstance* pEntity) {
		for (auto& item : m_EntityListeners)
			if (item.second) item.second->OnEntitySpawned(pEntity);
	}
	void SendEntityDeleted(CEntityInstance* pEntity) {
		for (auto& item : m_EntityListeners)
			if (item.second) item.second->OnEntityDeleted(pEntity);
	}
	void SendEntityParentChanged(CEntityInstance* pEntity, CEntityInstance* pNewParent) {
		for (auto& item : m_EntityListeners)
			if (item.second) item.second->OnEntityParentChanged(pEntity, pNewParent);
	}
	void SendEntityCheckTransmit(CCheckTransmitInfo **pInfoInfoList, int nInfoCount, CBitVec<16384> &unionTransmitEdicts, CBitVec<16384> &unionTransmitEdicts2, const Entity2Networkable_t **pNetworkables, const uint16 *pEntityIndicies, int nEntityIndices, bool bEnablePVSBits) {
		for (auto& item : m_EntityListeners)
			if (item.second) item.second->CheckTransmit(pInfoInfoList, nInfoCount, unionTransmitEdicts, unionTransmitEdicts2, pNetworkables, pEntityIndicies, nEntityIndices, bEnablePVSBits);
	}

private:
    std::map<int, std::vector<CommandCallbackPre>> ChatHookPre;
    std::map<int, std::vector<CommandCallbackPost>> ChatHookPost;

    std::map<int, std::vector<StartupCallback>> StartupHook;
	std::map<int, std::vector<StartupCallback>> MapEndHooks;
	std::map<int, std::vector<MapStartCallback>> MapStartHooks;
    std::map<int, std::vector<StartupCallback>> GetGameRules;

    std::map<int, std::map<std::string, EventCallback>> HookEvents;
	std::map<int, std::map<std::string, std::vector<EventHookPreCallback>>> m_EventHookPre;
	std::map<int, std::map<std::string, std::vector<EventHookPostCallback>>> m_EventHookPost;
	std::map<int, std::vector<CanAcquireCallback>> m_CanAcquirePre;
	std::map<int, std::vector<CanAcquireCallback>> m_CanAcquirePost;

	std::map<int, std::vector<OnTakeDamageCallback>> OnTakeDamageHook;
	std::map<int, std::vector<OnTakeDamagePreCallback>> OnTakeDamageHookPre;

	std::map<int, std::vector<OnHearingClientCallback>> OnHearingClientHook;

	std::map<int, std::vector<OnSettingsOpenCallback>> m_OnSettingsOpen;
	std::map<int, std::vector<OnSettingsItemCallback>> m_OnSettingsItem;

	std::map<int, IUtilsEntityListener*> m_EntityListeners;

	std::map<int, IServerListener*> m_ServerListeners;

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

extern Player* m_Players[64];

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

	void AddListener(SourceMM::PluginId id, IPlayerListener* pListener) override {
		if (pListener)
			m_Listeners[id] = pListener;
	}

	void ClearAllHooks(SourceMM::PluginId id) {
		m_OnClientAuthorized[id].clear();
		m_Listeners.erase(id);
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
		for (auto& item : m_Listeners)
			if (item.second) item.second->OnClientAuthorized(iSlot, steamID);
	}

	void OnClientConnected(int iSlot) {
		for (auto& item : m_Listeners)
			if (item.second) item.second->OnClientConnected(iSlot);
	}
	bool ClientConnect(int iSlot) {
		bool bAllow = true;
		for (auto& item : m_Listeners)
			if (item.second && !item.second->ClientConnect(iSlot)) bAllow = false;
		return bAllow;
	}
	void ClientPutInServer(int iSlot) {
		for (auto& item : m_Listeners)
			if (item.second) item.second->ClientPutInServer(iSlot);
	}
	void ClientActive(int iSlot) {
		for (auto& item : m_Listeners)
			if (item.second) item.second->ClientActive(iSlot);
	}
	void ClientFullyConnect(int iSlot) {
		for (auto& item : m_Listeners)
			if (item.second) item.second->ClientFullyConnect(iSlot);
	}
	void ClientDisconnect(int iSlot) {
		for (auto& item : m_Listeners)
			if (item.second) item.second->ClientDisconnect(iSlot);
	}
	void ClientCommand(int iSlot, const CCommand &args) {
		for (auto& item : m_Listeners)
			if (item.second) item.second->ClientCommand(iSlot, args);
	}
	void ClientSettingsChanged(int iSlot) {
		for (auto& item : m_Listeners)
			if (item.second) item.second->ClientSettingsChanged(iSlot);
	}
	void ProcessUsercmds(int iSlot, const CCLCMsg_Move_t &msg, bool paused) {
		for (auto& item : m_Listeners)
			if (item.second) item.second->ProcessUsercmds(iSlot, msg, paused);
	}
	void ClientVoice(int iSlot) {
		for (auto& item : m_Listeners)
			if (item.second) item.second->ClientVoice(iSlot);
	}
	void ClientCommandKeyValues(int iSlot, KeyValues *pKeyValues) {
		for (auto& item : m_Listeners)
			if (item.second) item.second->ClientCommandKeyValues(iSlot, pKeyValues);
	}
	void ClientSvcUserMessage(int iSlot, int um_type, uint32 size, const void *buf) {
		for (auto& item : m_Listeners)
			if (item.second) item.second->ClientSvcUserMessage(iSlot, um_type, size, buf);
	}
	bool ProcessClientVoiceData(int iSlot, void *pVoiceInfo) {
		bool bAllow = true;
		for (auto& item : m_Listeners)
			if (item.second && !item.second->ProcessClientVoiceData(iSlot, pVoiceInfo)) bAllow = false;
		return bAllow;
	}
	void OnClientSessionStart(int iSlot) {
		for (auto& item : m_Listeners)
			if (item.second) item.second->OnClientSessionStart(iSlot);
	}
	void OnClientSessionEnd(int iSlot) {
		for (auto& item : m_Listeners)
			if (item.second) item.second->OnClientSessionEnd(iSlot);
	}
	bool ProcessTick(int iSlot, const CNETMsg_Tick_t& msg) {
		bool bAllow = true;
		for (auto& item : m_Listeners)
			if (item.second && !item.second->ProcessTick(iSlot, msg)) bAllow = false;
		return bAllow;
	}
	bool ProcessStringCmd(int iSlot, const CNETMsg_StringCmd_t& msg) {
		bool bAllow = true;
		for (auto& item : m_Listeners)
			if (item.second && !item.second->ProcessStringCmd(iSlot, msg)) bAllow = false;
		return bAllow;
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
	trace_info_t RayTrace(int iSlot);
	void TakeDamage(int iSlot, CTakeDamageInfo* pInfo, bool bHook);
	void RemoveWeapons(int iSlot);

	void SetConVar(std::vector<int> vPlayers, const char* name, const char* value);
	void SetConVars(std::vector<int> vPlayers, std::vector<FakeConVar> cvars);

	void SetConVar(int iSlot, FakeConVar cvar) {
		std::vector<int> vPlayers = {iSlot};
		SetConVar(vPlayers, cvar.szCvar.c_str(), cvar.szValue.c_str());
	}
	void SetConVar(int iSlot, const char* name, const char* value) {
		std::vector<int> vPlayers = {iSlot};
		SetConVar(vPlayers, name, value);
	}
	void SetConVar(std::vector<int> vPlayers, FakeConVar cvar) {
		SetConVar(vPlayers, cvar.szCvar.c_str(), cvar.szValue.c_str());
	}
	void SetConVars(int iSlot, std::vector<FakeConVar> cvars) {
		std::vector<int> vPlayers = {iSlot};
		SetConVars(vPlayers, cvars);
	}

	bool UseClientCommand(int iSlot, const char* szCommand);
private:
	std::map<int, std::vector<OnClientAuthorizedCallback>> m_OnClientAuthorized;
	std::map<int, IPlayerListener*> m_Listeners;
};

class LayoutApi : public ILayoutApi
{
public:
	void Create(int iSlot, const char* szLayoutName, const char* szLayoutPath);
	void SetHasClass(int iSlot, const char* szLayoutName, CUtlString szPanelId, CUtlString szClassName, bool bHasClass);
	void SetInputCapture(int iSlot, const char* szLayoutName, bool bCapture);
	bool GetInputCapture(int iSlot, const char* szLayoutName);
	void SetDialogVariable(int iSlot, const char* szLayoutName, CUtlString szPanelId, CUtlString szVariableName, CUtlString szValue);
	const char* GetDialogVariable(int iSlot, const char* szLayoutName, CUtlString szPanelId, CUtlString szVariableName);
	void Destroy(int iSlot, const char* szLayoutName, float flDelay);

	void SetGlobalHasClass(const char* szLayoutName, CUtlString szPanelId, CUtlString szClassName, bool bHasClass) override;
	void SetGlobalDialogVariable(const char* szLayoutName, CUtlString szPanelId, CUtlString szVariableName, CUtlString szValue) override;
	void SetGlobalInputCapture(const char* szLayoutName, bool bCapture) override;

	void CreateGlobal(const char* szLayoutName, const char* szLayoutPath) override;
	void DestroyGlobal(const char* szLayoutName, float flDelay) override;
	
	void HookOnCustomHudClicked(SourceMM::PluginId id, OnCustomHudClicked callback) override {
		m_OnCustomHudClicked[id].push_back(callback);
	}
	void SendCustomHudClickedCallback(int iSlot, const char* szLayoutName, const char* szButtonId) {
		for(auto& item : m_OnCustomHudClicked)
		{
			for (auto& callback : item.second) {
				if (callback) {
					callback(iSlot, szLayoutName, szButtonId);
				}
			}
		}
	}
	void ClearAllHooks(SourceMM::PluginId id) {
		m_OnCustomHudClicked[id].clear();
	}
private:
	std::map<int, std::vector<OnCustomHudClicked>> m_OnCustomHudClicked;
};

enum MsgDest : int32_t
{
	HUD_PRINTNOTIFY = 1,
	HUD_PRINTCONSOLE = 2,
	HUD_PRINTTALK = 3,
	HUD_PRINTCENTER = 4,
	HUD_PRINTTALK2 = 5,
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
