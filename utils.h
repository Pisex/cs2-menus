#ifndef _INCLUDE_METAMOD_SOURCE_STUB_PLUGIN_H_
#define _INCLUDE_METAMOD_SOURCE_STUB_PLUGIN_H_

#include <ISmmPlugin.h>
#include <sh_vector.h>
#include "utlvector.h"
#include "ehandle.h"
#include <iserver.h>
#include <entity2/entitysystem.h>
#include "igameevents.h"
#include "vector.h"
#include <deque>
#include <functional>
#include "sdk/utils.hpp"
#include <utlstring.h>
#include <KeyValues.h>
#include "sdk/schemasystem.h"
#include "sdk/CBaseEntity.h"
#include "sdk/CGameRulesProxy.h"
#include "sdk/CBasePlayerPawn.h"
#include "sdk/CCSPlayerController.h"
#include "sdk/CCSPlayer_ItemServices.h"
#include "sdk/CSmokeGrenadeProjectile.h"
#include "sdk/module.h"
#include "funchook.h"
#include "include/menus.h"
#include <map>
#include <ctime>
#include <chrono>
#include <array>
#include <thread>

class Menus final : public ISmmPlugin, public IMetamodListener
{
public:
	bool Load(PluginId id, ISmmAPI* ismm, char* error, size_t maxlen, bool late);
	bool Unload(char* error, size_t maxlen);
	void* OnMetamodQuery(const char* iface, int* ret);
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
	bool FireEvent(IGameEvent* pEvent, bool bDontBroadcast);
	void ClientCommand(CPlayerSlot slot, const CCommand &args);
	void GameFrame(bool simulating, bool bFirstTick, bool bLastTick);
	void StartupServer(const GameSessionConfiguration_t& config, ISource2WorldSession*, const char*);
    void OnDispatchConCommand(ConCommandHandle cmd, const CCommandContext& ctx, const CCommand& args);
	void OnClientDisconnect(CPlayerSlot slot, int reason, const char *pszName, uint64 xuid, const char *pszNetworkID);
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
	
	void StartupServer(SourceMM::PluginId id, StartupCallback fn) override {
		StartupHook[id].push_back(fn);
	}
	
	void OnGetGameRules(SourceMM::PluginId id, StartupCallback fn) override {
		GetGameRules[id].push_back(fn);
	}

	void SetStateChanged(CBaseEntity* entity, const char* sClassName, const char* sFieldName, int extraOffset);

    void RegCommand(SourceMM::PluginId id, const std::vector<std::string> &console, const std::vector<std::string> &chat, const CommandCallback &callback) override {
		for (auto & element : console) {
        	ConsoleCommands[id][element] = callback;	
		}
		for (auto & element : chat) {
        	ChatCommands[id][element] = callback;	
		}
    }

	void AddChatListener(SourceMM::PluginId id, CommandCallback callback) override {
        ChatHook[id].push_back(callback);
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

	bool SendChatListenerCallback(int iSlot, const char* szContent) {
		bool bFound = true;
		for(auto& item : ChatHook)
		{
			for (auto& callback : item.second) {
				if (callback && !callback(iSlot, szContent)) {
					bFound = false;
				}
			}
		}
		return bFound;
	}
	bool FindAndSendCommandCallback(const char* szCommand, int iSlot, const char* szContent) {
		bool bFound = false;
		for(auto& item : ConsoleCommands)
		{
			if(item.second[std::string(szCommand)] && item.second[std::string(szCommand)](iSlot, szContent))
			{
				bFound = true;
			}
		}
		for(auto& item : ChatCommands)
		{
			if(item.second[std::string(szCommand)] && item.second[std::string(szCommand)](iSlot, szContent))
			{
				bFound = true;
			}
		}
		Msg("DEBUG COMMAND: %s | %i\n", szCommand, bFound);
		return bFound;
	}
	
	void ClearAllHooks(SourceMM::PluginId id) override {
		ConsoleCommands[id].clear();
		ChatCommands[id].clear();
		HookEvents[id].clear();
		ChatHook[id].clear();
		StartupHook[id].clear();
		GetGameRules[id].clear();
	}
	
	void NextFrame() {
		while (!m_nextFrame.empty())
		{
			m_nextFrame.front()();
			m_nextFrame.pop_front();
		}
	}
private:
    std::map<int, std::map<std::string, CommandCallback>> ConsoleCommands;
    std::map<int, std::map<std::string, CommandCallback>> ChatCommands;
    std::map<int, std::vector<CommandCallback>> ChatHook;

    std::map<int, std::vector<StartupCallback>> StartupHook;
    std::map<int, std::vector<StartupCallback>> GetGameRules;

    std::map<int, std::map<std::string, EventCallback>> HookEvents;

	std::deque<std::function<void()>> m_nextFrame;
};

const std::string colors_text[] = {
	"{DEFAULT}",
	"{WHITE}",
	"{RED}",
	"{LIGHTPURPLE}",
	"{GREEN}",
	"{LIME}",
	"{LIGHTGREEN}",
	"{LIGHTRED}",
	"{GRAY}",
	"{LIGHTOLIVE}",
	"{OLIVE}",
	"{LIGHTBLUE}",
	"{BLUE}",
	"{PURPLE}",
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
	"\x0A",
	"\xe2\x80\xa9"
};

#endif //_INCLUDE_METAMOD_SOURCE_STUB_PLUGIN_H_
