#include <stdio.h>
#include "utils.h"
#include "menus_internal.h"
#include "metamod_oslink.h"
#include "schemasystem/schemasystem.h"
#include <serversideclient.h>
#include <iostream>
#include <iomanip>
#include <ctime>
#include <sstream>
#include <chrono>
#include <fstream>

Menus g_Menus;
PLUGIN_EXPOSE(Menus, g_Menus);

CGlobalVars* gpGlobals = nullptr;
IVEngineServer2* engine = nullptr;
CCSGameRules* g_pGameRules = nullptr;
CEntitySystem* g_pEntitySystem = nullptr;
CPhysicsQuery* g_pGameTraceManager = nullptr;
IGameEventSystem* g_gameEventSystem = nullptr;
IGameEventManager2* gameeventmanager = nullptr;
CGameEntitySystem* g_pGameEntitySystem = nullptr;
INetworkGameServer* g_pNetworkGameServer = nullptr;

float g_flUniversalTime;
float g_flLastTickedTime;
bool g_bHasTicked;

int g_iCommitSuicide = 0;
int g_iRemoveWeapons = 0;
int g_iChangeTeam = 0;
int g_iCollisionRulesChanged = 0;
int g_iTeleport = 0;
int g_iRespawn = 0;
int g_iDropWeapon = 0;

CGameEntitySystem* GameEntitySystem()
{
	g_pGameEntitySystem = *reinterpret_cast<CGameEntitySystem**>(reinterpret_cast<uintptr_t>(g_pGameResourceServiceServer) + WIN_LINUX(0x58, 0x50));
	return g_pGameEntitySystem;
}

KeyValues* g_hKVData;

int g_iMenuType[64];
int g_iMenuItem[64];
std::chrono::milliseconds g_iMenuLastButtonInput[64];
MenuPlayer g_MenuPlayer[64];
std::string g_TextMenuPlayer[64];

std::map<std::string, std::string> g_vecPhrases;

std::map<std::string, std::map<std::string, int>> g_Offsets;
std::map<std::string, std::map<std::string, int>> g_ChainOffsets;

MenusApi* g_pMenusApi = nullptr;
IMenusApi* g_pMenusCore = nullptr;

UtilsApi* g_pUtilsApi = nullptr;
IUtilsApi* g_pUtilsCore = nullptr;

PlayersApi* g_pPlayersApi = nullptr;
IPlayersApi* g_pPlayersCore = nullptr;

LayoutApi* g_pLayoutApi = nullptr;
ILayoutApi* g_pLayoutCore = nullptr;

ICookiesApi* g_pCookies = nullptr;

char szLanguage[16];
int g_iMenuTypeDefault;
int g_iMenuTime;
int g_iDelayAuthFailKick;
bool g_bMenuAddon;
const char* g_szMenuURL;
bool g_bMenuFlashFix;
bool g_bAccessUserChangeType;
bool g_bStopingUser;
bool g_bPanoramaMenu;
int g_iTimeoutMenu;
int g_iSoundType;
std::string g_szServerID;
bool g_bAllowDisableNotify;
bool g_bNotifyDisabledDefault;
bool g_bNotifyDisabled[64];

std::string g_szSettingsCommand;

std::map<std::string, std::string> g_mapSounds;
std::unordered_map<std::string, CHandle<CCSCustomHudLayout>> g_mapHudLayouts[64];
std::unordered_map<std::string, CHandle<CCSCustomHudLayout>> g_mapGlobalHudLayouts;

std::unordered_map<const Menu*, std::string> g_mapMenuDesc;
std::string g_szMenuDesc[64];

std::unordered_map<const Menu*, std::vector<ItemExtra>> g_mapItemExtra;
std::vector<ItemExtra> g_vItemExtra[64];

ItemExtra& UTIL_PushItemExtra(Menu& hMenu)
{
	auto& ex = g_mapItemExtra[&hMenu];
	ex.resize(hMenu.hItems.size());
	ex.back() = ItemExtra{};
	return ex.back();
}

ItemExtra* UTIL_ResolveItemExtra(const ItemRef& ref)
{
	if (!ref.pMenu || ref.iIndex < 0) return nullptr;
	auto it = g_mapItemExtra.find(ref.pMenu);
	if (it == g_mapItemExtra.end()) return nullptr;
	if (ref.iIndex >= (int)it->second.size()) return nullptr;
	return &it->second[ref.iIndex];
}

std::vector<std::string> g_vCommandEater;
std::map<int, std::vector<int>> g_mapTransmitState;

int m_iBuildGameSessionManifestHookID;
int g_iOnTakeDamageAliveId = -1;
int g_iOnClientConnectHook = -1;
int g_iOnClientPerformDisconnectionHook = -1;
int g_iProcessTickHook = -1;
int g_iProcessStringCmdHook = -1;

std::vector<std::string> g_mapPrecache;

std::map<int, std::map<std::string, CommandCallback>> ConsoleCommands;
std::map<int, std::map<std::string, CommandCallback>> ChatCommands;
Player* m_Players[64];

// IServerGameClients
SH_DECL_HOOK6_void(IServerGameClients, OnClientConnected, SH_NOATTRIB, 0, CPlayerSlot, const char*, uint64, const char *, const char *, bool);
SH_DECL_HOOK6(IServerGameClients, ClientConnect, SH_NOATTRIB, 0, bool, CPlayerSlot, const char*, uint64, const char *, bool, CBufferString *);
SH_DECL_HOOK4_void(IServerGameClients, ClientPutInServer, SH_NOATTRIB, 0, CPlayerSlot, char const *, int, uint64);
SH_DECL_HOOK4_void(IServerGameClients, ClientActive, SH_NOATTRIB, 0, CPlayerSlot, bool, char const *, uint64);
SH_DECL_HOOK1_void(IServerGameClients, ClientFullyConnect, SH_NOATTRIB, 0, CPlayerSlot);
SH_DECL_HOOK5_void(IServerGameClients, ClientDisconnect, SH_NOATTRIB, 0, CPlayerSlot, ENetworkDisconnectionReason, const char *, uint64, const char *);
SH_DECL_HOOK2_void(IServerGameClients, ClientCommand, SH_NOATTRIB, 0, CPlayerSlot, const CCommand&);
SH_DECL_HOOK1_void(IServerGameClients, ClientSettingsChanged, SH_NOATTRIB, 0, CPlayerSlot);
SH_DECL_HOOK3_void(IServerGameClients, ProcessUsercmds, SH_NOATTRIB, 0, CPlayerSlot, const CCLCMsg_Move_t&, bool);
SH_DECL_HOOK1_void(IServerGameClients, ClientVoice, SH_NOATTRIB, 0, CPlayerSlot);
SH_DECL_HOOK2_void(IServerGameClients, ClientCommandKeyValues, SH_NOATTRIB, 0, CPlayerSlot, KeyValues*);
SH_DECL_HOOK4_void(IServerGameClients, ClientSvcUserMessage, SH_NOATTRIB, 0, CPlayerSlot, int, uint32, const void *);
SH_DECL_HOOK2(IServerGameClients, ProcessClientVoiceData, SH_NOATTRIB, 0, bool, CPlayerSlot, void *);

// ISource2Server
SH_DECL_HOOK1_void(IServerGameDLL, PreWorldUpdate, SH_NOATTRIB, 0, bool);
SH_DECL_HOOK3_void(IServerGameDLL, GameFrame, SH_NOATTRIB, 0, bool, bool, bool);
SH_DECL_HOOK1_void(IServerGameDLL, ServerHibernationUpdate, SH_NOATTRIB, 0, bool);
SH_DECL_HOOK0_void(IServerGameDLL, GameServerSteamAPIActivated, SH_NOATTRIB, 0);
SH_DECL_HOOK0_void(IServerGameDLL, GameServerSteamAPIDeactivated, SH_NOATTRIB, 0);
SH_DECL_HOOK1_void(IServerGameDLL, OnHostNameChanged, SH_NOATTRIB, 0, const char*);
SH_DECL_HOOK0_void(IServerGameDLL, PreFatalShutdown, const, 0);
SH_DECL_HOOK1_void(IServerGameDLL, UpdateWhenNotInGame, SH_NOATTRIB, 0, float);
SH_DECL_HOOK2_void(IServerGameDLL, ServerConVarChanged, SH_NOATTRIB, 0, const char*, const char*);
SH_DECL_HOOK3_void(INetworkServerService, StartupServer, SH_NOATTRIB, 0, const GameSessionConfiguration_t&, ISource2WorldSession*, const char*);

SH_DECL_HOOK8_void(ISource2GameEntities, CheckTransmit, SH_NOATTRIB, 0, CCheckTransmitInfo **, int, CBitVec<16384> &, CBitVec<16384> &, const Entity2Networkable_t **, const uint16 *, int, bool);

SH_DECL_HOOK6_void(CServerSideClient, Connect, SH_NOATTRIB, 0, int, const char*, int, INetChannel*, uint8, uint32);
SH_DECL_HOOK1_void(CServerSideClient, PerformDisconnection, SH_NOATTRIB, 0, ENetworkDisconnectionReason);
SH_DECL_HOOK1(CServerSideClientBase, ProcessTick, SH_NOATTRIB, 0, bool, const CNETMsg_Tick_t&);
SH_DECL_HOOK1(CServerSideClientBase, ProcessStringCmd, SH_NOATTRIB, 0, bool, const CNETMsg_StringCmd_t&);

SH_DECL_HOOK8_void(IGameEventSystem, PostEventAbstract, SH_NOATTRIB, 0, CSplitScreenSlot, bool, int, const uint64 *, INetworkMessageInternal *, const CNetMessage *, unsigned long, NetChannelBufType_t);
SH_DECL_HOOK3(IVEngineServer2, SetClientListening, SH_NOATTRIB, 0, bool, CPlayerSlot, CPlayerSlot, bool);

SH_DECL_HOOK3_void(ICvar, DispatchConCommand, SH_NOATTRIB, 0, ConCommandRef, const CCommandContext&, const CCommand&);
SH_DECL_HOOK2(IGameEventManager2, FireEvent, SH_NOATTRIB, 0, bool, IGameEvent*, bool);
SH_DECL_HOOK1_void(IGameSystem, BuildGameSessionManifest, SH_NOATTRIB, false, const EventBuildGameSessionManifest_t&);

SH_DECL_MANUALHOOK1(OnTakeDamage_Alive, 0, 0, 0, bool, CTakeDamageInfoContainer *);

struct SndOpEventGuid_t;
void (*UTIL_Remove)(CEntityInstance*) = nullptr;
int (*UTIL_TakeDamage)(CCSPlayer_DamageReactServices*, CTakeDamageInfo*) = nullptr;
bool (*UTIL_IsHearingClient)(void* serverClient, int index) = nullptr;
void (*UTIL_Say)(const CCommandContext& ctx, CCommand& args) = nullptr;
void (*UTIL_SetModel)(CBaseModelEntity*, const char* szModel) = nullptr;
void (*UTIL_DispatchSpawn)(CEntityInstance*, CEntityKeyValues*) = nullptr;
void (*UTIL_SayTeam)(const CCommandContext& ctx, CCommand& args) = nullptr;
void (*UTIL_SwitchTeam)(CCSPlayerController* pPlayer, int iTeam) = nullptr;
void (*UTIL_StopSoundEvent)(CBaseEntity *pEntity, const char *pszSound) = nullptr;
void (*UTIL_RespawnPlayer)(CBasePlayerController* pController, CCSPlayerPawn* pPawn, bool a3, bool a4, bool a5, bool a6) = nullptr;
IGameEventListener2* (*UTIL_GetLegacyGameEventListener)(CPlayerSlot slot) = nullptr;
CBaseEntity* (*UTIL_CreateEntity)(const char *pClassName, CEntityIndex iForceEdictIndex) = nullptr;
void (*UTIL_SetMoveType)(CBaseEntity *pThis, MoveType_t nMoveType, MoveCollide_t nMoveCollide) = nullptr;
SndOpEventGuid_t (*UTIL_EmitSoundFilter)(uint8_t unk1[32], IRecipientFilter& filter, CEntityIndex ent, const EmitSound_t& params);
void (*UTIL_AcceptInput)(CEntityInstance* pThis, const char* pInputName, CEntityInstance* pActivator, CEntityInstance* pCaller, variant_t& pValue) = nullptr;
bool (*UTIL_TraceShape)(CPhysicsQuery*, const Ray_t* ray, const Vector* start, const Vector* end, CTraceFilter* filter, trace_t* trace) = nullptr;
void (*UTIL_TerminateRound)(CGameRules* pGameRules, float delay, unsigned int reason, int64 teamid) = nullptr;
AcquireResult::Type (*UTIL_CanAcquire)(CPlayer_ItemServices* pItemServices, CEconItemView* pItemView, AcquireMethod::Type eMethod, uint* pLimit) = nullptr;

using namespace DynLibUtils;

funchook_t* m_SayHook;
funchook_t* m_SayTeamHook;
funchook_t* m_TakeDamageHook;
funchook_t* m_IsHearingClientHook;
funchook_t* m_CanAcquireHook;

std::vector<std::string> SplitStringBySpace(const std::string& input);
bool containsOnlyDigits(const std::string& str) {
	return str.find_first_not_of("0123456789") == std::string::npos;
}

void SayTeamHook(const CCommandContext& ctx, CCommand& args)
{
	bool bCallback = true;
	bCallback = g_pUtilsApi->SendChatListenerPreCallback(ctx.GetPlayerSlot().Get(), args.ArgS(), true);
	if(args[1][0])
	{
		if(g_pEntitySystem)
		{
			auto pController = CCSPlayerController::FromSlot(ctx.GetPlayerSlot().Get());
			if(bCallback && pController && pController->GetPawn() && pController->m_steamID() != 0 && g_MenuPlayer[ctx.GetPlayerSlot().Get()].bEnabled && containsOnlyDigits(std::string(args[1] + 1)))
				bCallback = false;
		}
	}
	bCallback = g_pUtilsApi->SendChatListenerPostCallback(ctx.GetPlayerSlot().Get(), args.ArgS(), bCallback, true);
	if(bCallback)
	{
		UTIL_SayTeam(ctx, args);
	}
}

void SayHook(const CCommandContext& ctx, CCommand& args)
{
	bool bCallback = true;
	bCallback = g_pUtilsApi->SendChatListenerPreCallback(ctx.GetPlayerSlot().Get(), args.ArgS(), false);
	if(args[1][0])
	{
		if(g_pEntitySystem)
		{
			auto pController = CCSPlayerController::FromSlot(ctx.GetPlayerSlot().Get());
			if(bCallback && pController && pController->GetPawn() && pController->m_steamID() != 0 && g_MenuPlayer[ctx.GetPlayerSlot().Get()].bEnabled && containsOnlyDigits(std::string(args[1] + 1)))
				bCallback = false;
		}
	}
	bCallback = g_pUtilsApi->SendChatListenerPostCallback(ctx.GetPlayerSlot().Get(), args.ArgS(), bCallback, false);
	if(bCallback)
	{
		UTIL_Say(ctx, args);
	}
}

int Hook_TakeDamage(CCSPlayer_DamageReactServices* pService, CTakeDamageInfo* info)
{
    if (!pService || !info) return UTIL_TakeDamage(pService, info);

    CCSPlayerPawn* pPawn = pService->GetPawn();
    if (!pPawn) return UTIL_TakeDamage(pService, info);

    auto pController = pPawn->m_hController();
    if (!pController) return UTIL_TakeDamage(pService, info);

    int iPlayerSlot = pController->GetEntityIndex().Get() - 1;
    if (iPlayerSlot < 0 || iPlayerSlot >= 64) return UTIL_TakeDamage(pService, info);

    if (!g_pUtilsApi->SendHookOnTakeDamagePre(iPlayerSlot, info)) return 1;
 	
	return UTIL_TakeDamage(pService, info);
}

std::string Colorizer(std::string str)
{
	for (size_t i = 0; i < std::size(colors_hex); i++)
	{
		size_t pos = 0;

		while ((pos = str.find(colors_text[i], pos)) != std::string::npos)
		{
			str.replace(pos, colors_text[i].length(), colors_hex[i]);
			pos += colors_hex[i].length();
		}
	}

	return str;
}

void* Menus::OnMetamodQuery(const char* iface, int* ret)
{
	if (!strcmp(iface, MENUS_INTERFACE))
	{
		*ret = META_IFACE_OK;
		return g_pMenusCore;
	}
	if (!strcmp(iface, UTILS_INTERFACE))
	{
		*ret = META_IFACE_OK;
		return g_pUtilsCore;
	}
	if (!strcmp(iface, PLAYERS_INTERFACE))
	{
		*ret = META_IFACE_OK;
		return g_pPlayersCore;
	}
	if (!strcmp(iface, LAYOUT_INTERFACE))
	{
		*ret = META_IFACE_OK;
		return g_pLayoutCore;
	}

	*ret = META_IFACE_FAILED;
	return nullptr;
}

int CheckActionMenu(int iSlot, CCSPlayerController* pController, int iButton)
{
	if(!pController) return 0;
	if(iSlot < 0 || iSlot >= 64) return 0;
	auto& hMenuPlayer = g_MenuPlayer[iSlot];
	auto& hMenu = hMenuPlayer.hMenu;
	if(hMenuPlayer.bEnabled)
	{
		hMenuPlayer.iEnd = std::time(0) + g_iMenuTime;
		if(iButton == 9 && hMenu.bExit)
		{
			hMenuPlayer.iList = 0;
			hMenuPlayer.bEnabled = false;
			if(g_iMenuType[iSlot] == 0)
			{
				for (size_t i = 0; i < 8; i++)
				{
					g_pUtilsCore->PrintToChat(iSlot, " \x08-\x01");
				}
			}
			if(hMenu.hFunc) hMenu.hFunc("exit", "exit", 9, iSlot);
			hMenuPlayer.hMenu.clear();
		}
		else if(iButton == 8)
		{
			int iItems = size(hMenu.hItems) / 5;
			if (size(hMenu.hItems) % 5 > 0) iItems++;
			if(iItems > hMenuPlayer.iList+1)
			{
				hMenuPlayer.iList++;
				g_pMenusCore->DisplayPlayerMenu(hMenu, iSlot, false, true);
				if(hMenu.hFunc) hMenu.hFunc("next", "next", 8, iSlot);
			}
		}
		else if(iButton == 7)
		{
			if(hMenuPlayer.iList != 0 || hMenuPlayer.hMenu.bBack)
			{
				if(hMenuPlayer.iList != 0)
				{
					hMenuPlayer.iList--;
					g_pMenusCore->DisplayPlayerMenu(hMenu, iSlot, false, true);
				}
				else if(hMenu.hFunc) hMenu.hFunc("back", "back", 7, iSlot);
			}
		}
		else
		{
			int iItems = size(hMenu.hItems);
			int iItem = hMenuPlayer.iList*5+iButton-1;
			if(iItems <= iItem) return 1;
			if(hMenu.hItems.size() <= iItem) return 1;
			if(hMenu.hItems[iItem].iType != 1) return 1;
			if(hMenu.hFunc) hMenu.hFunc(hMenu.hItems[iItem].sBack.c_str(), hMenu.hItems[iItem].sText.c_str(), iButton, iSlot);
		}
		return 1;
	}
	return 0;
}

bool FASTCALL IsHearingClient(void* serverClient, int index)
{
	return g_pUtilsApi->SendHookOnHearingClient(index)?UTIL_IsHearingClient(serverClient, index):false;
}

AcquireResult::Type CanAcquireHook(CPlayer_ItemServices* pItemServices, CEconItemView* pItemView, AcquireMethod::Type eMethod, uint* pLimit)
{
	if(!pItemServices || !pItemView)
		return UTIL_CanAcquire(pItemServices, pItemView, eMethod, pLimit);

	CCSPlayerPawn* pPawn = pItemServices->GetPawn();
	if(!pPawn)
		return UTIL_CanAcquire(pItemServices, pItemView, eMethod, pLimit);
	CCSPlayerController* pController = (CCSPlayerController*)pPawn->GetController();
	if(!pController)
		return UTIL_CanAcquire(pItemServices, pItemView, eMethod, pLimit);
	int iSlot = pController->GetPlayerSlot();
	if(iSlot < 0 || iSlot >= 64)
		return UTIL_CanAcquire(pItemServices, pItemView, eMethod, pLimit);

	bool bHandled = false;
	AcquireResult::Type result = g_pUtilsApi->SendCanAcquirePre(iSlot, pItemServices, pItemView, eMethod, bHandled);

	if(!bHandled)
		result = UTIL_CanAcquire(pItemServices, pItemView, eMethod, pLimit);

	g_pUtilsApi->SendCanAcquirePost(iSlot, pItemServices, pItemView, eMethod);
	return result;
}

void UtilsApi::SetTransmitState(int iEntityIndex, bool bState, std::vector<int> vecSlots)
{
	if(vecSlots.empty()) {
		for (int i = 0; i < 64; i++)
		{
			if(bState) g_mapTransmitState[iEntityIndex].erase(std::remove(g_mapTransmitState[iEntityIndex].begin(), g_mapTransmitState[iEntityIndex].end(), i), g_mapTransmitState[iEntityIndex].end());
			else g_mapTransmitState[iEntityIndex].push_back(i);
		}
	} else {
		for (int i = 0; i < vecSlots.size(); i++)
		{
			if(bState) g_mapTransmitState[iEntityIndex].erase(std::remove(g_mapTransmitState[iEntityIndex].begin(), g_mapTransmitState[iEntityIndex].end(), vecSlots[i]), g_mapTransmitState[iEntityIndex].end());
			else g_mapTransmitState[iEntityIndex].push_back(vecSlots[i]);
		}
	}
}


void Menus::AllPluginsLoaded() {
	char error[64];
	int ret;
	g_pCookies = (ICookiesApi *)g_SMAPI->MetaFactory(COOKIES_INTERFACE, &ret, NULL);
	if (ret == META_IFACE_FAILED)
	{
		g_pCookies = nullptr;
		g_hKVData = new KeyValues("Data");
		const char *pszPath = "addons/data/menus_data.ini";
		if (!g_hKVData->LoadFromFile(g_pFullFileSystem, pszPath)) {
			char szPath2[256];
			g_SMAPI->Format(szPath2, sizeof(szPath2), "%s/%s", g_SMAPI->GetBaseDir(), pszPath);
			std::fstream file;
			file.open(szPath2, std::fstream::out | std::fstream::trunc);
			file << "\"Data\"\n{\n\n}\n";
			file.close();
		}
		return;
	}
	g_pCookies->HookClientCookieLoaded(g_PLID, [](int iSlot) {
		const char* szMenuType = g_pCookies->GetCookie(iSlot, "Utils.MenuType");
		if (szMenuType && szMenuType[0]) g_iMenuType[iSlot] = UTIL_SanitizeMenuType(atoi(szMenuType));
		else g_iMenuType[iSlot] = UTIL_SanitizeMenuType(g_iMenuTypeDefault);

		const char* szNotifyDisabled = g_pCookies->GetCookie(iSlot, "Utils.NotifyDisabled");
		if (szNotifyDisabled && szNotifyDisabled[0]) g_bNotifyDisabled[iSlot] = atoi(szNotifyDisabled) != 0;
		else g_bNotifyDisabled[iSlot] = g_bNotifyDisabledDefault;
	});
}

int UTIL_SanitizeMenuType(int iType)
{
	if(!g_bPanoramaMenu && iType == 3)
		return (g_iMenuTypeDefault == 3) ? 0 : g_iMenuTypeDefault;
	return iType;
}

int GetClientCookieMenuType(int iSlot)
{
	if(g_pPlayersApi->IsFakeClient(iSlot)) return g_iMenuTypeDefault;
	uint64 m_steamID = g_pPlayersApi->GetSteamID64(iSlot);
	if(m_steamID == 0) return g_iMenuTypeDefault;
	char szSteamID[64];
	g_SMAPI->Format(szSteamID, sizeof(szSteamID), "%llu", m_steamID);
	KeyValues *hData = g_hKVData->FindKey(szSteamID, false);
	if(!hData) return g_iMenuTypeDefault;
	return UTIL_SanitizeMenuType(hData->GetInt("Utils.MenuType", g_iMenuTypeDefault));
}

bool GetClientCookieNotifyDisabled(int iSlot)
{
	if(g_pPlayersApi->IsFakeClient(iSlot)) return g_bNotifyDisabledDefault;
	uint64 m_steamID = g_pPlayersApi->GetSteamID64(iSlot);
	if(m_steamID == 0) return g_bNotifyDisabledDefault;
	char szSteamID[64];
	g_SMAPI->Format(szSteamID, sizeof(szSteamID), "%llu", m_steamID);
	KeyValues *hData = g_hKVData->FindKey(szSteamID, false);
	if(!hData) return g_bNotifyDisabledDefault;
	return hData->GetInt("Utils.NotifyDisabled", g_bNotifyDisabledDefault ? 1 : 0) != 0;
}

bool SetClientCookie(int iSlot, const char* sCookieName, const char* sData)
{
	if(g_pPlayersApi->IsFakeClient(iSlot)) return false;
	uint64 m_steamID = g_pPlayersApi->GetSteamID64(iSlot);
	if(m_steamID == 0) return false;
	char szSteamID[64];
	g_SMAPI->Format(szSteamID, sizeof(szSteamID), "%llu", m_steamID);
	KeyValues *hData = g_hKVData->FindKey(szSteamID, true);
	hData->SetString(sCookieName, sData);
	g_hKVData->SaveToFile(g_pFullFileSystem, "addons/data/menus_data.ini");
	return true;
}

void SettingMenu(int iSlot)
{
	Menu hMenu;
	hMenu.szTitle = g_vecPhrases["MenuMenusTitle"];
	g_pMenusCore->AddItemMenu(hMenu, "0", g_vecPhrases["MenuMenusItem0"].c_str(), g_iMenuType[iSlot] == 0 ? ITEM_DISABLED : ITEM_DEFAULT);
	g_pMenusCore->AddItemMenu(hMenu, "1", g_vecPhrases["MenuMenusItem1"].c_str(), g_iMenuType[iSlot] == 1 ? ITEM_DISABLED : ITEM_DEFAULT);
	g_pMenusCore->AddItemMenu(hMenu, "2", g_vecPhrases["MenuMenusItem2"].c_str(), g_iMenuType[iSlot] == 2 ? ITEM_DISABLED : ITEM_DEFAULT);

	if(g_bPanoramaMenu)
		g_pMenusCore->AddItemMenu(hMenu, "3", g_vecPhrases["MenuMenusItem3"].c_str(), g_iMenuType[iSlot] == 3 ? ITEM_DISABLED : ITEM_DEFAULT);
	g_pMenusCore->SetExitMenu(hMenu, true);
	g_pMenusCore->SetCallback(hMenu, [](const char* szBack, const char* szFront, int iItem, int iSlot){
		if(iItem < 7) {
			int iType = std::atoi(szBack);
			iType = UTIL_SanitizeMenuType(iType);
			g_iMenuType[iSlot] = iType;
			if(g_pCookies) g_pCookies->SetCookie(iSlot, "Utils.MenuType", std::to_string(iType).c_str());
			else SetClientCookie(iSlot, "Utils.MenuType", std::to_string(iType).c_str());
			SettingMenu(iSlot);
		}
	});
	g_pMenusCore->DisplayPlayerMenu(hMenu, iSlot, true, true);
}

void UtilsApi::OpenSettingsMenu(int iSlot)
{
	Menu hMenu;
	hMenu.szTitle = g_vecPhrases["MenuSettingsTitle"];

	if (g_bAccessUserChangeType)
		g_pMenusCore->AddItemMenu(hMenu, "settings_menu_type", g_vecPhrases["MenuMenusTitle"].c_str(), ITEM_DEFAULT);

	if (g_bAllowDisableNotify) {
		g_pMenusCore->AddToggleMenu(hMenu, "settings_notify_toggle",
			g_vecPhrases["MenuNotifyToggle"].c_str(),
			g_bNotifyDisabled[iSlot],
			[](const char* szBack, bool bState, int iItem, int iSlot) {
				g_bNotifyDisabled[iSlot] = bState;
				if (g_pCookies) g_pCookies->SetCookie(iSlot, "Utils.NotifyDisabled", bState ? "1" : "0");
				else SetClientCookie(iSlot, "Utils.NotifyDisabled", bState ? "1" : "0");
			});
	}

	SendHookOnSettingsOpen(iSlot, hMenu);

	g_pMenusCore->SetExitMenu(hMenu, true);
	g_pMenusCore->SetCallback(hMenu, [](const char* szBack, const char* szFront, int iItem, int iSlot) {
		if (strcmp(szBack, "settings_menu_type") == 0) {
			SettingMenu(iSlot);
			return;
		}
		if (strcmp(szBack, "settings_notify_toggle") == 0) {
			g_bNotifyDisabled[iSlot] = !g_bNotifyDisabled[iSlot];
			if (g_pCookies) g_pCookies->SetCookie(iSlot, "Utils.NotifyDisabled", g_bNotifyDisabled[iSlot] ? "1" : "0");
			else SetClientCookie(iSlot, "Utils.NotifyDisabled", g_bNotifyDisabled[iSlot] ? "1" : "0");
			g_pUtilsApi->OpenSettingsMenu(iSlot);
			return;
		}
		g_pUtilsApi->SendHookOnSettingsItem(szBack, szFront, iItem, iSlot);
	});
	g_pMenusCore->DisplayPlayerMenu(hMenu, iSlot, true, true);
}

void Menus::OnCheckTransmit(CCheckTransmitInfo **pInfoInfoList, int nInfoCount, CBitVec<16384> &unionTransmitEdicts, CBitVec<16384> &unionTransmitEdicts2, const Entity2Networkable_t **pNetworkables, const uint16 *pEntityIndicies, int nEntityIndices, bool bEnablePVSBits)
{
	g_pUtilsApi->SendEntityCheckTransmit(pInfoInfoList, nInfoCount, unionTransmitEdicts, unionTransmitEdicts2, pNetworkables, pEntityIndicies, nEntityIndices, bEnablePVSBits);
	if (!g_pEntitySystem) return;

	for (int i = 0; i < nInfoCount; i++)
	{
		auto &pInfo = pInfoInfoList[i];
		int iPlayerSlot = pInfo->m_nPlayerSlot;

		CCSPlayerController* pSelfController = CCSPlayerController::FromSlot(iPlayerSlot);
		if (!pSelfController) continue;

		if(g_mapTransmitState.size() > 0)
		{
			for (auto& [iEntityIndex, vecSlots] : g_mapTransmitState)
			{
				if(vecSlots.size() > 0)
				{
					if(std::find(vecSlots.begin(), vecSlots.end(), iPlayerSlot) != vecSlots.end())
					{
						pInfo->m_pTransmitEntity->Clear(iEntityIndex);
					}
				}
			}
		}
	}
}

static std::string FindHudLayout(int iSlot, uint32 rawHandle)
{
	if (rawHandle == 16777215)
		return "";

	const int rawIndex = rawHandle & 0x7FFF;
	for (auto& [name, hndl] : g_mapHudLayouts[iSlot])
	{
		if (!hndl.IsValid())
			continue;
		if (hndl.ToInt() == rawHandle || hndl.GetEntryIndex() == rawIndex)
			return hndl.Get() != nullptr ? name : "";
	}
	for (auto& [name, hndl] : g_mapGlobalHudLayouts)
	{
		if (!hndl.IsValid())
			continue;
		if (hndl.ToInt() == rawHandle || hndl.GetEntryIndex() == rawIndex)
			return hndl.Get() != nullptr ? name : "";
	}
	return "";
}

void Menus::OnClientSvcUserMessage( CPlayerSlot slot, int um_type, uint32 size, const void *buf )
{
	g_pPlayersApi->ClientSvcUserMessage(slot.Get(), um_type, size, buf);
	if (um_type != CS_UM_CustomHudClicked)
		return;

	CCSUsrMsg_CustomHudClicked msg;
	if (!msg.ParseFromArray(buf, size))
		return;

	const int iSlot = slot.Get();
	const std::string sButtonId = msg.button_id();
	const char* szButton = sButtonId.c_str();

	std::string sLayoutId = FindHudLayout(iSlot, msg.custom_hud_layout());
	if (sLayoutId.empty())
		return;

	UTIL_HandleLayoutClick(iSlot, sLayoutId.c_str(), szButton);
	g_pLayoutApi->SendCustomHudClickedCallback(iSlot, sLayoutId.c_str(), szButton);
}

void Menus::OnEntityCreated(CEntityInstance* pEntity)
{
	g_pUtilsApi->SendEntityCreated(pEntity);
}

void Menus::OnEntitySpawned(CEntityInstance* pEntity)
{
	g_pUtilsApi->SendEntitySpawned(pEntity);
}

void Menus::OnEntityDeleted(CEntityInstance* pEntity)
{
	g_pUtilsApi->SendEntityDeleted(pEntity);
}

void Menus::OnEntityParentChanged(CEntityInstance* pEntity, CEntityInstance* pNewParent)
{
	g_pUtilsApi->SendEntityParentChanged(pEntity, pNewParent);
}

bool Menus::Load(PluginId id, ISmmAPI* ismm, char* error, size_t maxlen, bool late)
{
	PLUGIN_SAVEVARS();
	g_SMAPI->AddListener( this, this );

	GET_V_IFACE_CURRENT(GetEngineFactory, g_pCVar, ICvar, CVAR_INTERFACE_VERSION);
	GET_V_IFACE_ANY(GetEngineFactory, g_pSchemaSystem, ISchemaSystem, SCHEMASYSTEM_INTERFACE_VERSION);
	GET_V_IFACE_CURRENT(GetEngineFactory, engine, IVEngineServer2, SOURCE2ENGINETOSERVER_INTERFACE_VERSION);
	GET_V_IFACE_CURRENT(GetFileSystemFactory, g_pFullFileSystem, IFileSystem, FILESYSTEM_INTERFACE_VERSION);
	GET_V_IFACE_CURRENT(GetServerFactory, g_pSource2Server, ISource2Server, SOURCE2SERVER_INTERFACE_VERSION);
	GET_V_IFACE_ANY(GetEngineFactory, g_gameEventSystem, IGameEventSystem, GAMEEVENTSYSTEM_INTERFACE_VERSION);
	GET_V_IFACE_ANY(GetEngineFactory, g_pNetworkMessages, INetworkMessages, NETWORKMESSAGES_INTERFACE_VERSION);
	GET_V_IFACE_ANY(GetServerFactory, g_pSource2GameClients, IServerGameClients, SOURCE2GAMECLIENTS_INTERFACE_VERSION);
	GET_V_IFACE_ANY(GetServerFactory, g_pSource2GameEntities, ISource2GameEntities, SOURCE2GAMEENTITIES_INTERFACE_VERSION);
	GET_V_IFACE_CURRENT(GetEngineFactory, g_pNetworkServerService, INetworkServerService, NETWORKSERVERSERVICE_INTERFACE_VERSION);
	GET_V_IFACE_CURRENT(GetEngineFactory, g_pGameResourceServiceServer, IGameResourceService, GAMERESOURCESERVICESERVER_INTERFACE_VERSION);

	SH_ADD_HOOK_MEMFUNC(ICvar, DispatchConCommand, g_pCVar, this, &Menus::OnDispatchConCommand, false);
	SH_ADD_HOOK(IServerGameDLL, GameFrame, g_pSource2Server, SH_MEMBER(this, &Menus::GameFrame), true);
	SH_ADD_HOOK(IServerGameDLL, GameServerSteamAPIActivated, g_pSource2Server, SH_MEMBER(this, &Menus::OnGameServerSteamAPIActivated), false);
	SH_ADD_HOOK(IServerGameDLL, PreWorldUpdate, g_pSource2Server, SH_MEMBER(this, &Menus::OnPreWorldUpdate), false);
	SH_ADD_HOOK(IServerGameDLL, ServerHibernationUpdate, g_pSource2Server, SH_MEMBER(this, &Menus::OnServerHibernationUpdate), false);
	SH_ADD_HOOK(IServerGameDLL, GameServerSteamAPIDeactivated, g_pSource2Server, SH_MEMBER(this, &Menus::OnGameServerSteamAPIDeactivated), false);
	SH_ADD_HOOK(IServerGameDLL, OnHostNameChanged, g_pSource2Server, SH_MEMBER(this, &Menus::OnHostNameChanged), false);
	SH_ADD_HOOK(IServerGameDLL, PreFatalShutdown, g_pSource2Server, SH_MEMBER(this, &Menus::OnPreFatalShutdown), false);
	SH_ADD_HOOK(IServerGameDLL, UpdateWhenNotInGame, g_pSource2Server, SH_MEMBER(this, &Menus::OnUpdateWhenNotInGame), false);
	SH_ADD_HOOK(IServerGameDLL, ServerConVarChanged, g_pSource2Server, SH_MEMBER(this, &Menus::OnServerConVarChanged), false);
	SH_ADD_HOOK(IGameEventSystem, PostEventAbstract, g_gameEventSystem, SH_MEMBER(this, &Menus::OnPostEventAbstract), false);
	SH_ADD_HOOK(IVEngineServer2, SetClientListening, engine, SH_MEMBER(this, &Menus::OnSetClientListening), false);

	SH_ADD_HOOK_MEMFUNC(ICvar, DispatchConCommand, g_pCVar, this, &Menus::OnDispatchConCommandPost, true);
	SH_ADD_HOOK(IServerGameDLL, GameFrame, g_pSource2Server, SH_MEMBER(this, &Menus::OnGameFramePost), true);
	SH_ADD_HOOK(IServerGameDLL, PreWorldUpdate, g_pSource2Server, SH_MEMBER(this, &Menus::OnPreWorldUpdatePost), true);
	SH_ADD_HOOK(IServerGameDLL, ServerHibernationUpdate, g_pSource2Server, SH_MEMBER(this, &Menus::OnServerHibernationUpdatePost), true);
	SH_ADD_HOOK(IServerGameDLL, GameServerSteamAPIActivated, g_pSource2Server, SH_MEMBER(this, &Menus::OnGameServerSteamAPIActivatedPost), true);
	SH_ADD_HOOK(IServerGameDLL, GameServerSteamAPIDeactivated, g_pSource2Server, SH_MEMBER(this, &Menus::OnGameServerSteamAPIDeactivatedPost), true);
	SH_ADD_HOOK(IServerGameDLL, OnHostNameChanged, g_pSource2Server, SH_MEMBER(this, &Menus::OnHostNameChangedPost), true);
	SH_ADD_HOOK(IServerGameDLL, PreFatalShutdown, g_pSource2Server, SH_MEMBER(this, &Menus::OnPreFatalShutdownPost), true);
	SH_ADD_HOOK(IServerGameDLL, UpdateWhenNotInGame, g_pSource2Server, SH_MEMBER(this, &Menus::OnUpdateWhenNotInGamePost), true);
	SH_ADD_HOOK(IServerGameDLL, ServerConVarChanged, g_pSource2Server, SH_MEMBER(this, &Menus::OnServerConVarChangedPost), true);
	SH_ADD_HOOK(IGameEventSystem, PostEventAbstract, g_gameEventSystem, SH_MEMBER(this, &Menus::OnPostEventAbstractPost), true);
	SH_ADD_HOOK(IVEngineServer2, SetClientListening, engine, SH_MEMBER(this, &Menus::OnSetClientListeningPost), true);
	SH_ADD_HOOK(IServerGameClients, ClientCommand, g_pSource2GameClients, SH_MEMBER(this, &Menus::ClientCommand), false);
	SH_ADD_HOOK(INetworkServerService, StartupServer, g_pNetworkServerService, SH_MEMBER(this, &Menus::StartupServer), true);
	SH_ADD_HOOK(IServerGameClients, ClientDisconnect, g_pSource2GameClients, SH_MEMBER(this, &Menus::OnClientDisconnect), true);
	SH_ADD_HOOK(IServerGameClients, ClientPutInServer, g_pSource2GameClients, SH_MEMBER(this, &Menus::OnClientPutInServer), true);
	SH_ADD_HOOK(IServerGameClients, OnClientConnected, g_pSource2GameClients, SH_MEMBER(this, &Menus::OnClientConnected), false);
	SH_ADD_HOOK(IServerGameClients, ClientConnect, g_pSource2GameClients, SH_MEMBER(this, &Menus::OnClientConnect), false );	
	SH_ADD_HOOK(ISource2GameEntities, CheckTransmit, g_pSource2GameEntities, SH_MEMBER(this, &Menus::OnCheckTransmit), true);
	SH_ADD_HOOK(IServerGameClients, ClientSvcUserMessage, g_pSource2GameClients, SH_MEMBER(this, &Menus::OnClientSvcUserMessage), false);
	SH_ADD_HOOK(IServerGameClients, ClientActive, g_pSource2GameClients, SH_MEMBER(this, &Menus::OnClientActive), true);
	SH_ADD_HOOK(IServerGameClients, ClientFullyConnect, g_pSource2GameClients, SH_MEMBER(this, &Menus::OnClientFullyConnect), true);
	SH_ADD_HOOK(IServerGameClients, ClientSettingsChanged, g_pSource2GameClients, SH_MEMBER(this, &Menus::OnClientSettingsChanged), false);
	SH_ADD_HOOK(IServerGameClients, ProcessUsercmds, g_pSource2GameClients, SH_MEMBER(this, &Menus::OnProcessUsercmds), false);
	SH_ADD_HOOK(IServerGameClients, ClientVoice, g_pSource2GameClients, SH_MEMBER(this, &Menus::OnClientVoice), false);
	SH_ADD_HOOK(IServerGameClients, ClientCommandKeyValues, g_pSource2GameClients, SH_MEMBER(this, &Menus::OnClientCommandKeyValues), false);
	SH_ADD_HOOK(IServerGameClients, ProcessClientVoiceData, g_pSource2GameClients, SH_MEMBER(this, &Menus::OnProcessClientVoiceData), false);
	
	ConVar_Register(FCVAR_RELEASE | FCVAR_CLIENT_CAN_EXECUTE | FCVAR_GAMEDLL);

	if (late)
	{
		g_pEntitySystem = GameEntitySystem();
		gpGlobals = engine->GetServerGlobals();
	}

	//0 - в чат
	//1 - также как в чат но в центр
	//2 - выбор через WASD 
	//3 - panorama menu

	g_pMenusApi = new MenusApi();
	g_pMenusCore = g_pMenusApi;

	g_pUtilsApi = new UtilsApi();
	g_pUtilsCore = g_pUtilsApi;

	g_pPlayersApi = new PlayersApi();
	g_pPlayersCore = g_pPlayersApi;

	g_pLayoutApi = new LayoutApi();
	g_pLayoutCore = g_pLayoutApi;

	{
		KeyValues* g_kvCore = new KeyValues("Core");
		const char *pszPath = "addons/configs/core.cfg";

		if (!g_kvCore->LoadFromFile(g_pFullFileSystem, pszPath))
		{
			g_pUtilsApi->ErrorLog("[%s] Failed to load %s\n", g_PLAPI->GetLogTag(), pszPath);
			return false;
		}

		g_SMAPI->Format(szLanguage, sizeof(szLanguage), "%s", g_kvCore->GetString("ServerLang", "en"));
		g_iMenuTypeDefault = g_kvCore->GetInt("MenuType", 0);
		g_iMenuTime = g_kvCore->GetInt("MenuTime", 60);
		g_bMenuAddon = g_kvCore->GetBool("MenuAddon", false);
		g_szMenuURL = g_kvCore->GetString("MenuURL");
		g_bMenuFlashFix = g_kvCore->GetBool("MenuFlashFix", false);
		g_iDelayAuthFailKick = g_kvCore->GetInt("delay_auth_fail_kick", 30);
		g_bAccessUserChangeType = g_kvCore->GetBool("AccessUserChangeType", true);
		g_bStopingUser = g_kvCore->GetBool("StopingUser", false);
		g_bPanoramaMenu = g_kvCore->GetBool("PanoramaMenu", true);
		g_iTimeoutMenu = g_kvCore->GetInt("TimeoutInputMenu", 160);
		g_iSoundType = g_kvCore->GetInt("sound_type", 0);
		g_szServerID = g_kvCore->GetString("server_id", "0");
		g_szSettingsCommand = g_kvCore->GetString("SettingsCommand", "!settings");
		g_bAllowDisableNotify = g_kvCore->GetBool("AllowDisableNotify", true);
		g_bNotifyDisabledDefault = g_kvCore->GetBool("NotifyDisabledDefault", false);

		g_mapSounds.clear();
		const char* szBackSound = g_kvCore->GetString("sound_back", "");
		if(szBackSound && szBackSound[0]) g_mapSounds["back"] = szBackSound;
		const char* szNextSound = g_kvCore->GetString("sound_next", "");
		if(szNextSound && szNextSound[0]) g_mapSounds["next"] = szNextSound;
		const char* szSelectSound = g_kvCore->GetString("sound_select", "");
		if(szSelectSound && szSelectSound[0]) g_mapSounds["select"] = szSelectSound;
		const char* szExitSound = g_kvCore->GetString("sound_exit", "");
		if(szExitSound && szExitSound[0]) g_mapSounds["exit"] = szExitSound;
		const char* szMoveSound = g_kvCore->GetString("sound_move", "");
		if(szMoveSound && szMoveSound[0]) g_mapSounds["move"] = szMoveSound;

		const char* szCommandEater = g_kvCore->GetString("commands_eater");
		if(szCommandEater && szCommandEater[0])
		{
			std::string sCommandEater(szCommandEater);
			std::istringstream ss(sCommandEater);
			std::string token;
			while(std::getline(ss, token, ','))
			{
				g_vCommandEater.push_back(token);
			}
		}

		if(g_bAccessUserChangeType)
		{
			const char* g_szMenuChangeCommand = g_kvCore->GetString("MenuChangeCommand");
			g_pUtilsApi->RegCommand(g_PLID, {}, {g_szMenuChangeCommand}, [](int iSlot, const char* szContent){
				if(g_pPlayersApi->IsFakeClient(iSlot)) return false;
				SettingMenu(iSlot);
				return false;
			});
		}

		g_pUtilsApi->RegCommand(g_PLID, {"mm_settings"}, {g_szSettingsCommand}, [](int iSlot, const char* szContent){
			if(g_pPlayersApi->IsFakeClient(iSlot)) return false;
			g_pUtilsApi->OpenSettingsMenu(iSlot);
			return false;
		});

		// g_pUtilsApi->RegCommand(g_PLID, {"notifytest"}, {"notifytest"}, [](int iSlot, const char* szContent){
		// 	if(g_pPlayersApi->IsFakeClient(iSlot)) return true;

		// 	auto tokens = SplitStringBySpace(szContent ? szContent : "");
		// 	int iType = (tokens.size() > 1) ? atoi(tokens[1].c_str()) : 0;
		// 	int iPos  = (tokens.size() > 2) ? atoi(tokens[2].c_str()) : 0;
		// 	float flDur = (tokens.size() > 3) ? (float)atof(tokens[3].c_str()) : 5.0f;
		// 	bool bAll = (tokens.size() > 4) ? (atoi(tokens[4].c_str()) != 0) : false;

		// 	static const char* const s_szTitles[] = { "Успех", "Внимание", "Ошибка" };
		// 	int iT = (iType < 0 || iType > 2) ? 0 : iType;
		// 	char szText[128];
		// 	g_SMAPI->Format(szText, sizeof(szText), "Тест тоста: type=%d pos=%d dur=%.1f", iType, iPos, flDur);
		// 	char szChat[160];
		// 	g_SMAPI->Format(szChat, sizeof(szChat), " \x04[Notify]\x01 %s: %s", s_szTitles[iT], szText);

		// 	if(bAll)
		// 		g_pUtilsApi->ShowNotifyAll(iType, s_szTitles[iT], szText, flDur, iPos, szChat);
		// 	else
		// 		g_pUtilsApi->ShowNotify(iSlot, iType, s_szTitles[iT], szText, flDur, iPos, szChat);
		// 	return true;
		// });
	}

	g_pUtilsApi->LoadTranslations("menus.phrases");

	{
		KeyValues::AutoDelete g_kvPrecacher("Precacher");
		const char* pszPrecachePath = "addons/configs/precacher.ini";
		if (g_kvPrecacher->LoadFromFile(g_pFullFileSystem, pszPrecachePath))
		{
			FOR_EACH_VALUE(g_kvPrecacher, pValue)
			{
				const char* szResource = pValue->GetString(nullptr, nullptr);
				if (szResource && szResource[0])
					g_pUtilsApi->AddPrecache(szResource);
			}
		}
	}

	KeyValues::AutoDelete g_kvSigs("Gamedata");
	const char *pszPath = "addons/configs/signatures.ini";

	if (!g_kvSigs->LoadFromFile(g_pFullFileSystem, pszPath))
	{
		g_pUtilsApi->ErrorLog("[%s] Failed to load %s\n", g_PLAPI->GetLogTag(), pszPath);
		return false;
	}
	CModule libserver(g_pSource2Server);
	const char* pszSay = g_kvSigs->GetString("UTIL_Say");
	if(pszSay && pszSay[0]) {
		UTIL_SayTeam = libserver.FindPattern(pszSay).RCast< decltype(UTIL_SayTeam) >();
		if (!UTIL_SayTeam)
		{
			g_pUtilsApi->ErrorLog("[%s] Failed to find function to get UTIL_Say", g_PLAPI->GetLogTag());
		}
		else
		{
			m_SayTeamHook = funchook_create();
			funchook_prepare(m_SayTeamHook, (void**)&UTIL_SayTeam, (void*)SayTeamHook);
			funchook_install(m_SayTeamHook, 0);
		}

		UTIL_Say = libserver.FindPattern(pszSay).RCast< decltype(UTIL_Say) >();
		if (!UTIL_Say)
		{
			g_pUtilsApi->ErrorLog("[%s] Failed to find function to get UTIL_Say", g_PLAPI->GetLogTag());
		}
		else
		{
			m_SayHook = funchook_create();
			funchook_prepare(m_SayHook, (void**)&UTIL_Say, (void*)SayHook);
			funchook_install(m_SayHook, 0);
		}
	}

	const char* pszTakeDamage = g_kvSigs->GetString("OnTakeDamagePre");
	if(pszTakeDamage && pszTakeDamage[0]) {
		UTIL_TakeDamage = libserver.FindPattern(pszTakeDamage).RCast< decltype(UTIL_TakeDamage) >();
		if (!UTIL_TakeDamage)
		{
			g_pUtilsApi->ErrorLog("[%s] Failed to find function to get UTIL_TakeDamage", g_PLAPI->GetLogTag());
		}
		else
		{
			m_TakeDamageHook = funchook_create();
			funchook_prepare(m_TakeDamageHook, (void**)&UTIL_TakeDamage, (void*)Hook_TakeDamage);
			funchook_install(m_TakeDamageHook, 0);
		}
	}

	CModule libengine(engine);
	const char* pszIsHearingClient = g_kvSigs->GetString("IsHearingClient");
	if(pszIsHearingClient && pszIsHearingClient[0]) {
		UTIL_IsHearingClient = libengine.FindPattern(g_kvSigs->GetString("IsHearingClient")).RCast< decltype(UTIL_IsHearingClient) >();
		if (!UTIL_IsHearingClient)
		{
			g_pUtilsApi->ErrorLog("[%s] Failed to find function to get IsHearingClient", g_PLAPI->GetLogTag());
		}
		else
		{
			m_IsHearingClientHook = funchook_create();
			funchook_prepare(m_IsHearingClientHook, (void**)&UTIL_IsHearingClient, (void*)IsHearingClient);
			funchook_install(m_IsHearingClientHook, 0);
		}
	}

	const char* pszSetModel = g_kvSigs->GetString("CBaseModelEntity_SetModel");
	if(pszSetModel && pszSetModel[0]) {
		UTIL_SetModel = libserver.FindPattern(pszSetModel).RCast< decltype(UTIL_SetModel) >();
		if (!UTIL_SetModel)
		{
			g_pUtilsApi->ErrorLog("[%s] Failed to find function to get CBaseModelEntity_SetModel", g_PLAPI->GetLogTag());
		}
	}

	const char* pszRespawnPlayer = g_kvSigs->GetString("UTIL_RespawnPlayer");
	if(pszRespawnPlayer && pszRespawnPlayer[0]) {
		UTIL_RespawnPlayer = libserver.FindPattern(pszRespawnPlayer).RCast< decltype(UTIL_RespawnPlayer) >();
		if (!UTIL_RespawnPlayer)
		{
			g_pUtilsApi->ErrorLog("[%s] Failed to find function to get UTIL_RespawnPlayer", g_PLAPI->GetLogTag());
		}
	}

	const char* pszAcceptInput = g_kvSigs->GetString("UTIL_AcceptInput");
	if(pszAcceptInput && pszAcceptInput[0]) {
		UTIL_AcceptInput = libserver.FindPattern(pszAcceptInput).RCast< decltype(UTIL_AcceptInput) >();
		if (!UTIL_AcceptInput)
		{
			g_pUtilsApi->ErrorLog("[%s] Failed to find function to get UTIL_AcceptInput", g_PLAPI->GetLogTag());
		}
	}
	
	const char* pszRemove = g_kvSigs->GetString("UTIL_Remove");
	if(pszRemove && pszRemove[0]) {
		UTIL_Remove = libserver.FindPattern(pszRemove).RCast< decltype(UTIL_Remove) >();
		if (!UTIL_Remove)
		{
			g_pUtilsApi->ErrorLog("[%s] Failed to find function to get UTIL_Remove", g_PLAPI->GetLogTag());
		}
	}

	const char* pszDispatchSpawn = g_kvSigs->GetString("UTIL_DispatchSpawn");
	if(pszDispatchSpawn && pszDispatchSpawn[0]) {
		UTIL_DispatchSpawn = libserver.FindPattern(pszDispatchSpawn).RCast< decltype(UTIL_DispatchSpawn) >();
		if (!UTIL_DispatchSpawn)
		{
			g_pUtilsApi->ErrorLog("[%s] Failed to find function to get UTIL_DispatchSpawn", g_PLAPI->GetLogTag());
		}
	}

	const char* pszCreateEntity = g_kvSigs->GetString("UTIL_CreateEntity");
	if(pszCreateEntity && pszCreateEntity[0]) {
		UTIL_CreateEntity = libserver.FindPattern(pszCreateEntity).RCast< decltype(UTIL_CreateEntity) >();
		if (!UTIL_CreateEntity)
		{
			g_pUtilsApi->ErrorLog("[%s] Failed to find function to get UTIL_CreateEntity", g_PLAPI->GetLogTag());
		}
	}

	const char* pszGetLegacyGameEventListener = g_kvSigs->GetString("GetLegacyGameEventListener");
	if(pszGetLegacyGameEventListener && pszGetLegacyGameEventListener[0]) {
		UTIL_GetLegacyGameEventListener = libserver.FindPattern(pszGetLegacyGameEventListener).RCast< decltype(UTIL_GetLegacyGameEventListener) >();
		if (!UTIL_GetLegacyGameEventListener)
		{
			g_pUtilsApi->ErrorLog("[%s] Failed to find function to get GetLegacyGameEventListener", g_PLAPI->GetLogTag());
		}
	}
	
	const char* pszSwitchTeam = g_kvSigs->GetString("SwitchTeam");
	if(pszSwitchTeam && pszSwitchTeam[0]) {
		UTIL_SwitchTeam = libserver.FindPattern(pszSwitchTeam).RCast< decltype(UTIL_SwitchTeam) >();
		if (!UTIL_SwitchTeam)
		{
			g_pUtilsApi->ErrorLog("[%s] Failed to find function to get SwitchTeam", g_PLAPI->GetLogTag());
		}
	}

	const char* pszSetMoveType = g_kvSigs->GetString("SetMoveType");
	if(pszSetMoveType && pszSetMoveType[0]) {
		UTIL_SetMoveType = libserver.FindPattern(pszSetMoveType).RCast< decltype(UTIL_SetMoveType) >();
		if (!UTIL_SetMoveType)
		{
			g_pUtilsApi->ErrorLog("[%s] Failed to find function to get SetMoveType", g_PLAPI->GetLogTag());
		}
	}
	
	const char* pszEmitSoundFilter = g_kvSigs->GetString("EmitSoundFilter");
	if(pszEmitSoundFilter && pszEmitSoundFilter[0]) {
		UTIL_EmitSoundFilter = libserver.FindPattern(pszEmitSoundFilter).RCast< decltype(UTIL_EmitSoundFilter) >();
		if (!UTIL_EmitSoundFilter)
		{
			g_pUtilsApi->ErrorLog("[%s] Failed to find function to get UTIL_EmitSoundFilter", g_PLAPI->GetLogTag());
		}
	}

	const char* pszStopSoundEvent = g_kvSigs->GetString("StopSoundEvent");
	if(pszStopSoundEvent && pszStopSoundEvent[0]) {
		UTIL_StopSoundEvent = libserver.FindPattern(pszStopSoundEvent).RCast< decltype(UTIL_StopSoundEvent) >();
		if (!UTIL_StopSoundEvent)
		{
			g_pUtilsApi->ErrorLog("[%s] Failed to find function to get StopSoundEvent", g_PLAPI->GetLogTag());
		}
	}

	g_iCommitSuicide = g_kvSigs->GetInt("CommitSuicide", 0);
	g_iChangeTeam = g_kvSigs->GetInt("ChangeTeam", 0);
	g_iCollisionRulesChanged = g_kvSigs->GetInt("CollisionRulesChanged", 0);
	g_iTeleport = g_kvSigs->GetInt("Teleport", 0);
	g_iRespawn = g_kvSigs->GetInt("Respawn", 0);
	g_iDropWeapon = g_kvSigs->GetInt("DropWeapon", 0);
	g_iRemoveWeapons = g_kvSigs->GetInt("RemoveWeapons", 0);
	void* pCCSPlayerPawnVTable = libserver.GetVirtualTableByName("CCSPlayerPawn");
	if (!pCCSPlayerPawnVTable)
	{
		g_pUtilsApi->ErrorLog("[%s] Failed to find CCSPlayerPawn vtable", g_PLAPI->GetLogTag());
	}
	else
	{
		SH_MANUALHOOK_RECONFIGURE(OnTakeDamage_Alive, g_kvSigs->GetInt("OnTakeDamage_Alive"), 0, 0);
		g_iOnTakeDamageAliveId = SH_ADD_MANUALDVPHOOK(OnTakeDamage_Alive, pCCSPlayerPawnVTable, SH_MEMBER(this, &Menus::Hook_OnTakeDamage_Alive), false);
	}


	CServerSideClient* pCServerSideClientVTable = libengine.GetVirtualTableByName("CServerSideClient").RCast<CServerSideClient*>();
	g_iOnClientConnectHook = SH_ADD_DVPHOOK(CServerSideClient, Connect, pCServerSideClientVTable, SH_MEMBER(this, &Menus::OnServerSideClientClientConnect), true);
	g_iOnClientPerformDisconnectionHook = SH_ADD_DVPHOOK(CServerSideClient, PerformDisconnection, pCServerSideClientVTable, SH_MEMBER(this, &Menus::OnCServerSideClientlientPerformDisconnection), false);
	g_iProcessTickHook = SH_ADD_DVPHOOK(CServerSideClientBase, ProcessTick, pCServerSideClientVTable, SH_MEMBER(this, &Menus::OnProcessTick), false);
	g_iProcessStringCmdHook = SH_ADD_DVPHOOK(CServerSideClientBase, ProcessStringCmd, pCServerSideClientVTable, SH_MEMBER(this, &Menus::OnProcessStringCmd), false);

	IGameSystem* pCEntityDebugGameSystem = libserver.GetVirtualTableByName("CEntityDebugGameSystem").RCast<IGameSystem*>();
	m_iBuildGameSessionManifestHookID = SH_ADD_DVPHOOK(IGameSystem, BuildGameSessionManifest, pCEntityDebugGameSystem, SH_MEMBER(this, &Menus::OnBuildGameSessionManifest), true);

	const char* pszGameEventManager = g_kvSigs->GetString("GetGameEventManager");
	if(pszGameEventManager && pszGameEventManager[0]) {
		auto gameEventManagerFn = libserver.FindPattern(pszGameEventManager);
		if( !gameEventManagerFn ) {
			g_pUtilsApi->ErrorLog("[%s] Failed to find function to get GetGameEventManager", g_PLAPI->GetLogTag());
		}
		else
		{
			gameeventmanager = gameEventManagerFn.ResolveRelativeAddress(0x3, 0x7).GetValue<IGameEventManager2*>();
			SH_ADD_HOOK(IGameEventManager2, FireEvent, gameeventmanager, SH_MEMBER(this, &Menus::FireEvent), false);
			SH_ADD_HOOK(IGameEventManager2, FireEvent, gameeventmanager, SH_MEMBER(this, &Menus::OnFireEventPost), true);
		}
	}

	const char* pszTraceShape = g_kvSigs->GetString("UTIL_TraceShape");
	if(pszTraceShape && pszTraceShape[0]) {
		UTIL_TraceShape = libserver.FindPattern(pszTraceShape).RCast< decltype(UTIL_TraceShape) >();
		if (!UTIL_TraceShape)
		{
			g_pUtilsApi->ErrorLog("[%s] Failed to find function to get UTIL_TraceShape", g_PLAPI->GetLogTag());
		}
	}

	const char* pszTerminateRound = g_kvSigs->GetString("TerminateRound");
	if(pszTerminateRound && pszTerminateRound[0]) {
		UTIL_TerminateRound = libserver.FindPattern(pszTerminateRound).RCast< decltype(UTIL_TerminateRound) >();
		if (!UTIL_TerminateRound)
		{
			g_pUtilsApi->ErrorLog("[%s] Failed to find function to get TerminateRound", g_PLAPI->GetLogTag());
		}
	}

	const char* pszGameTraceManager = g_kvSigs->GetString("GetGameTraceManager");
	if(pszGameTraceManager && pszGameTraceManager[0]) {
		auto gameTraceManagerFn = libserver.FindPattern(pszGameTraceManager);
		if( !gameTraceManagerFn ) g_pUtilsApi->ErrorLog("[%s] Failed to find function to get GetGameTraceManager", g_PLAPI->GetLogTag());
		else g_pGameTraceManager = *gameTraceManagerFn.ResolveRelativeAddress(3, 7).RCast<CPhysicsQuery**>();
	}

	const char* pszCanAcquire = g_kvSigs->GetString("CanAcquire");
	if(pszCanAcquire && pszCanAcquire[0]) {
		UTIL_CanAcquire = libserver.FindPattern(pszCanAcquire).RCast< decltype(UTIL_CanAcquire) >();
		if (!UTIL_CanAcquire)
		{
			g_pUtilsApi->ErrorLog("[%s] Failed to find function to get CanAcquire", g_PLAPI->GetLogTag());
		}
		else
		{
			m_CanAcquireHook = funchook_create();
			funchook_prepare(m_CanAcquireHook, (void**)&UTIL_CanAcquire, (void*)CanAcquireHook);
			funchook_install(m_CanAcquireHook, 0);
		}
	}

	new CTimer(1.0f, []()
	{
		for(int i = 0; i < 64; i++)
		{
			if (!m_Players[i] || m_Players[i]->IsFakeClient() || m_Players[i]->IsAuthenticated())
				continue;
			
			if(engine->IsClientFullyAuthenticated(CPlayerSlot(i)))
			{
				m_Players[i]->SetAuthenticated(true);
				m_Players[i]->SetSteamId(m_Players[i]->GetUnauthenticatedSteamId());
				g_pPlayersApi->SendClientAuthCallback(i, m_Players[i]->GetUnauthenticatedSteamId64());
			}
		}
		return 1.0f;
	});

	g_pUtilsApi->RegCommand(g_PLID, {"mm_1"}, {}, [](int iSlot, const char* szContent){
		if(g_iMenuType[iSlot] < 2) if(CheckActionMenu(iSlot, CCSPlayerController::FromSlot(iSlot), 1)) return true;
		return false;
	});
	g_pUtilsApi->RegCommand(g_PLID, {"mm_2"}, {}, [](int iSlot, const char* szContent){
		if(g_iMenuType[iSlot] < 2) if(CheckActionMenu(iSlot, CCSPlayerController::FromSlot(iSlot), 2)) return true;
		return false;
	});
	g_pUtilsApi->RegCommand(g_PLID, {"mm_3"}, {}, [](int iSlot, const char* szContent){
		if(g_iMenuType[iSlot] < 2) if(CheckActionMenu(iSlot, CCSPlayerController::FromSlot(iSlot), 3)) return true;
		return false;
	});
	g_pUtilsApi->RegCommand(g_PLID, {"mm_4"}, {}, [](int iSlot, const char* szContent){
		if(g_iMenuType[iSlot] < 2) if(CheckActionMenu(iSlot, CCSPlayerController::FromSlot(iSlot), 4)) return true;
		return false;
	});
	g_pUtilsApi->RegCommand(g_PLID, {"mm_5"}, {}, [](int iSlot, const char* szContent){
		if(g_iMenuType[iSlot] < 2) if(CheckActionMenu(iSlot, CCSPlayerController::FromSlot(iSlot), 5)) return true;
		return false;
	});
	g_pUtilsApi->RegCommand(g_PLID, {"mm_6"}, {}, [](int iSlot, const char* szContent){
		if(g_iMenuType[iSlot] < 2) if(CheckActionMenu(iSlot, CCSPlayerController::FromSlot(iSlot), 6)) return true;
		return false;
	});
	g_pUtilsApi->RegCommand(g_PLID, {"mm_7"}, {}, [](int iSlot, const char* szContent){
		if(g_iMenuType[iSlot] < 2) if(CheckActionMenu(iSlot, CCSPlayerController::FromSlot(iSlot), 7)) return true;
		return false;
	});
	g_pUtilsApi->RegCommand(g_PLID, {"mm_8"}, {}, [](int iSlot, const char* szContent){
		if(g_iMenuType[iSlot] < 2) if(CheckActionMenu(iSlot, CCSPlayerController::FromSlot(iSlot), 8)) return true;
		return false;
	});
	g_pUtilsApi->RegCommand(g_PLID, {"mm_9"}, {}, [](int iSlot, const char* szContent){
		if(g_iMenuType[iSlot] < 2) if(CheckActionMenu(iSlot, CCSPlayerController::FromSlot(iSlot), 9)) return true;
		return false;
	});

	return true;
}

void Menus::OnPluginUnload(PluginId id) {
	g_pUtilsApi->ClearAllHooks(id);
	g_pPlayersApi->ClearAllHooks(id);
	g_pLayoutApi->ClearAllHooks(id);
}

void Menus::OnBuildGameSessionManifest(const EventBuildGameSessionManifest_t& msg)
{
    IEntityResourceManifest* pResourceManifest = msg.m_pResourceManifest;
    for (auto& it : g_mapPrecache)
    {
        pResourceManifest->AddResource(it.c_str());
    }
}

bool Menus::Hook_OnTakeDamage_Alive(CTakeDamageInfoContainer *pInfoContainer)
{
	CCSPlayerPawn *pPawn = META_IFACEPTR(CCSPlayerPawn);
	if(!pPawn) RETURN_META_VALUE(MRES_IGNORED, true);
	CBasePlayerController* pPlayerController = pPawn->m_hController();
    if (pPlayerController)
	{
    	int iPlayerSlot = pPlayerController->GetEntityIndex().Get() - 1;
		g_pUtilsApi->SendHookOnTakeDamage(iPlayerSlot, pInfoContainer);
	}
	RETURN_META_VALUE(MRES_IGNORED, true);
}

void Menus::OnServerSideClientClientConnect(int socket, const char* pszName, int nUserID, INetChannel* pNetChannel, uint8 nConnectionTypeFlags, uint32 uChallengeNumber)
{
	CServerSideClient* pClient = META_IFACEPTR(CServerSideClient);
	if(!pClient) RETURN_META(MRES_IGNORED);
	int iSlot = pClient->GetPlayerSlot().Get();
	if(iSlot >= 0 && iSlot < 64)
		g_pPlayersApi->OnClientSessionStart(iSlot);
	RETURN_META(MRES_IGNORED);
}

void Menus::OnCServerSideClientlientPerformDisconnection(ENetworkDisconnectionReason reason)
{
	CServerSideClient* pClient = META_IFACEPTR(CServerSideClient);
	if(!pClient) RETURN_META(MRES_IGNORED);
	int iSlot = pClient->GetPlayerSlot().Get();
	if(iSlot >= 0 && iSlot < 64)
		g_pPlayersApi->OnClientSessionEnd(iSlot);
	RETURN_META(MRES_IGNORED);
}

bool Menus::OnProcessTick(const CNETMsg_Tick_t& msg)
{
	CServerSideClientBase* pClient = META_IFACEPTR(CServerSideClientBase);
	if(!pClient) RETURN_META_VALUE(MRES_IGNORED, true);
	int iSlot = pClient->GetPlayerSlot().Get();
	if(iSlot >= 0 && iSlot < 64 && !g_pPlayersApi->ProcessTick(iSlot, msg))
		RETURN_META_VALUE(MRES_SUPERCEDE, false);
	RETURN_META_VALUE(MRES_IGNORED, true);
}

bool Menus::OnProcessStringCmd(const CNETMsg_StringCmd_t& msg)
{
	CServerSideClientBase* pClient = META_IFACEPTR(CServerSideClientBase);
	if(!pClient) RETURN_META_VALUE(MRES_IGNORED, true);
	int iSlot = pClient->GetPlayerSlot().Get();
	if(iSlot >= 0 && iSlot < 64 && !g_pPlayersApi->ProcessStringCmd(iSlot, msg))
		RETURN_META_VALUE(MRES_SUPERCEDE, false);
	RETURN_META_VALUE(MRES_IGNORED, true);
}

bool Menus::Unload(char *error, size_t maxlen)
{
	SH_REMOVE_HOOK_MEMFUNC(ICvar, DispatchConCommand, g_pCVar, this, &Menus::OnDispatchConCommand, false);
	SH_REMOVE_HOOK(IServerGameDLL, GameFrame, g_pSource2Server, SH_MEMBER(this, &Menus::GameFrame), true);
	SH_REMOVE_HOOK(IGameEventManager2, FireEvent, gameeventmanager, SH_MEMBER(this, &Menus::FireEvent), false);
	SH_REMOVE_HOOK(IGameEventManager2, FireEvent, gameeventmanager, SH_MEMBER(this, &Menus::OnFireEventPost), true);
	SH_REMOVE_HOOK(IServerGameClients, ClientCommand, g_pSource2GameClients, SH_MEMBER(this, &Menus::ClientCommand), false);
	SH_REMOVE_HOOK(INetworkServerService, StartupServer, g_pNetworkServerService, SH_MEMBER(this, &Menus::StartupServer), true);
	SH_REMOVE_HOOK(IServerGameDLL, GameServerSteamAPIActivated, g_pSource2Server, SH_MEMBER(this, &Menus::OnGameServerSteamAPIActivated), false);
	SH_REMOVE_HOOK(IServerGameDLL, PreWorldUpdate, g_pSource2Server, SH_MEMBER(this, &Menus::OnPreWorldUpdate), false);
	SH_REMOVE_HOOK(IServerGameDLL, ServerHibernationUpdate, g_pSource2Server, SH_MEMBER(this, &Menus::OnServerHibernationUpdate), false);
	SH_REMOVE_HOOK(IServerGameDLL, GameServerSteamAPIDeactivated, g_pSource2Server, SH_MEMBER(this, &Menus::OnGameServerSteamAPIDeactivated), false);
	SH_REMOVE_HOOK(IServerGameDLL, OnHostNameChanged, g_pSource2Server, SH_MEMBER(this, &Menus::OnHostNameChanged), false);
	SH_REMOVE_HOOK(IServerGameDLL, PreFatalShutdown, g_pSource2Server, SH_MEMBER(this, &Menus::OnPreFatalShutdown), false);
	SH_REMOVE_HOOK(IServerGameDLL, UpdateWhenNotInGame, g_pSource2Server, SH_MEMBER(this, &Menus::OnUpdateWhenNotInGame), false);
	SH_REMOVE_HOOK(IServerGameDLL, ServerConVarChanged, g_pSource2Server, SH_MEMBER(this, &Menus::OnServerConVarChanged), false);
	SH_REMOVE_HOOK(IGameEventSystem, PostEventAbstract, g_gameEventSystem, SH_MEMBER(this, &Menus::OnPostEventAbstract), false);
	SH_REMOVE_HOOK(IVEngineServer2, SetClientListening, engine, SH_MEMBER(this, &Menus::OnSetClientListening), false);
	SH_REMOVE_HOOK_MEMFUNC(ICvar, DispatchConCommand, g_pCVar, this, &Menus::OnDispatchConCommandPost, true);
	SH_REMOVE_HOOK(IServerGameDLL, GameFrame, g_pSource2Server, SH_MEMBER(this, &Menus::OnGameFramePost), true);
	SH_REMOVE_HOOK(IServerGameDLL, PreWorldUpdate, g_pSource2Server, SH_MEMBER(this, &Menus::OnPreWorldUpdatePost), true);
	SH_REMOVE_HOOK(IServerGameDLL, ServerHibernationUpdate, g_pSource2Server, SH_MEMBER(this, &Menus::OnServerHibernationUpdatePost), true);
	SH_REMOVE_HOOK(IServerGameDLL, GameServerSteamAPIActivated, g_pSource2Server, SH_MEMBER(this, &Menus::OnGameServerSteamAPIActivatedPost), true);
	SH_REMOVE_HOOK(IServerGameDLL, GameServerSteamAPIDeactivated, g_pSource2Server, SH_MEMBER(this, &Menus::OnGameServerSteamAPIDeactivatedPost), true);
	SH_REMOVE_HOOK(IServerGameDLL, OnHostNameChanged, g_pSource2Server, SH_MEMBER(this, &Menus::OnHostNameChangedPost), true);
	SH_REMOVE_HOOK(IServerGameDLL, PreFatalShutdown, g_pSource2Server, SH_MEMBER(this, &Menus::OnPreFatalShutdownPost), true);
	SH_REMOVE_HOOK(IServerGameDLL, UpdateWhenNotInGame, g_pSource2Server, SH_MEMBER(this, &Menus::OnUpdateWhenNotInGamePost), true);
	SH_REMOVE_HOOK(IServerGameDLL, ServerConVarChanged, g_pSource2Server, SH_MEMBER(this, &Menus::OnServerConVarChangedPost), true);
	SH_REMOVE_HOOK(IGameEventSystem, PostEventAbstract, g_gameEventSystem, SH_MEMBER(this, &Menus::OnPostEventAbstractPost), true);
	SH_REMOVE_HOOK(IVEngineServer2, SetClientListening, engine, SH_MEMBER(this, &Menus::OnSetClientListeningPost), true);
	SH_REMOVE_HOOK(IServerGameClients, ClientDisconnect, g_pSource2GameClients, SH_MEMBER(this, &Menus::OnClientDisconnect), true);
	SH_REMOVE_HOOK(IServerGameClients, ClientPutInServer, g_pSource2GameClients, SH_MEMBER(this, &Menus::OnClientPutInServer), true);
	SH_REMOVE_HOOK(IServerGameClients, OnClientConnected, g_pSource2GameClients, SH_MEMBER(this, &Menus::OnClientConnected), false);
	SH_REMOVE_HOOK(IServerGameClients, ClientConnect, g_pSource2GameClients, SH_MEMBER(this, &Menus::OnClientConnect), false );
	SH_REMOVE_HOOK(IServerGameClients, ClientActive, g_pSource2GameClients, SH_MEMBER(this, &Menus::OnClientActive), true);
	SH_REMOVE_HOOK(IServerGameClients, ClientFullyConnect, g_pSource2GameClients, SH_MEMBER(this, &Menus::OnClientFullyConnect), true);
	SH_REMOVE_HOOK(IServerGameClients, ClientSettingsChanged, g_pSource2GameClients, SH_MEMBER(this, &Menus::OnClientSettingsChanged), false);
	SH_REMOVE_HOOK(IServerGameClients, ProcessUsercmds, g_pSource2GameClients, SH_MEMBER(this, &Menus::OnProcessUsercmds), false);
	SH_REMOVE_HOOK(IServerGameClients, ClientVoice, g_pSource2GameClients, SH_MEMBER(this, &Menus::OnClientVoice), false);
	SH_REMOVE_HOOK(IServerGameClients, ClientCommandKeyValues, g_pSource2GameClients, SH_MEMBER(this, &Menus::OnClientCommandKeyValues), false);
	SH_REMOVE_HOOK(IServerGameClients, ProcessClientVoiceData, g_pSource2GameClients, SH_MEMBER(this, &Menus::OnProcessClientVoiceData), false);
	SH_REMOVE_HOOK(IServerGameClients, ClientSvcUserMessage, g_pSource2GameClients, SH_MEMBER(this, &Menus::OnClientSvcUserMessage), false);

	if(g_iOnTakeDamageAliveId) SH_REMOVE_HOOK_ID(g_iOnTakeDamageAliveId);
	if(g_iOnClientConnectHook) SH_REMOVE_HOOK_ID(g_iOnClientConnectHook);
	if(g_iOnClientPerformDisconnectionHook) SH_REMOVE_HOOK_ID(g_iOnClientPerformDisconnectionHook);
	if(g_iProcessTickHook) SH_REMOVE_HOOK_ID(g_iProcessTickHook);
	if(g_iProcessStringCmdHook) SH_REMOVE_HOOK_ID(g_iProcessStringCmdHook);
	if(m_iBuildGameSessionManifestHookID) SH_REMOVE_HOOK_ID(m_iBuildGameSessionManifestHookID);
	if(m_SayHook) funchook_destroy(m_SayHook);
	if(m_SayTeamHook) funchook_destroy(m_SayTeamHook);
	if(m_TakeDamageHook) funchook_destroy(m_TakeDamageHook);
	if(m_CanAcquireHook) funchook_destroy(m_CanAcquireHook);

	ConVar_Unregister();
	
	return true;
}

void Menus::OnGameServerSteamAPIActivated()
{
	m_CallbackValidateAuthTicketResponse.Register(this, &Menus::OnValidateAuthTicketHook);
	g_pUtilsApi->SendServerSteamAPIActivated();
}

void Menus::OnPreWorldUpdate(bool simulating)
{
	g_pUtilsApi->SendServerPreWorldUpdate(simulating);
}

void Menus::OnServerHibernationUpdate(bool bHibernating)
{
	g_pUtilsApi->SendServerHibernationUpdate(bHibernating);
}

void Menus::OnGameServerSteamAPIDeactivated()
{
	g_pUtilsApi->SendServerSteamAPIDeactivated();
}

void Menus::OnHostNameChanged(const char *pHostname)
{
	g_pUtilsApi->SendServerHostNameChanged(pHostname);
}

void Menus::OnPreFatalShutdown() const
{
	g_pUtilsApi->SendServerPreFatalShutdown();
}

void Menus::OnUpdateWhenNotInGame(float flFrameTime)
{
	g_pUtilsApi->SendServerUpdateWhenNotInGame(flFrameTime);
}

void Menus::OnServerConVarChanged(const char *pVarName, const char *pValue)
{
	g_pUtilsApi->SendServerConVarChanged(pVarName, pValue);
}

void Menus::OnPostEventAbstract(CSplitScreenSlot nSlot, bool bLocalOnly, int nClientCount, const uint64 *clients, INetworkMessageInternal *pEvent, const CNetMessage *pData, unsigned long nSize, NetChannelBufType_t bufType)
{
	g_pUtilsApi->SendServerPostEvent(nClientCount, clients, pEvent, pData);
}

bool Menus::OnSetClientListening(CPlayerSlot iReceiver, CPlayerSlot iSender, bool bListen)
{
	bool bResult = g_pUtilsApi->SendServerSetClientListening(iReceiver.Get(), iSender.Get(), bListen);
	if(bResult != bListen)
		RETURN_META_VALUE_NEWPARAMS(MRES_HANDLED, bResult, &IVEngineServer2::SetClientListening, (iReceiver, iSender, bResult));
	RETURN_META_VALUE(MRES_IGNORED, bResult);
}

void Menus::OnGameFramePost(bool simulating, bool bFirstTick, bool bLastTick)
{
	g_pUtilsApi->SendServerGameFramePost(simulating, bFirstTick, bLastTick);
}

void Menus::OnPreWorldUpdatePost(bool simulating)
{
	g_pUtilsApi->SendServerPreWorldUpdatePost(simulating);
}

void Menus::OnServerHibernationUpdatePost(bool bHibernating)
{
	g_pUtilsApi->SendServerHibernationUpdatePost(bHibernating);
}

void Menus::OnGameServerSteamAPIActivatedPost()
{
	g_pUtilsApi->SendServerSteamAPIActivatedPost();
}

void Menus::OnGameServerSteamAPIDeactivatedPost()
{
	g_pUtilsApi->SendServerSteamAPIDeactivatedPost();
}

void Menus::OnHostNameChangedPost(const char *pHostname)
{
	g_pUtilsApi->SendServerHostNameChangedPost(pHostname);
}

void Menus::OnPreFatalShutdownPost() const
{
	g_pUtilsApi->SendServerPreFatalShutdownPost();
}

void Menus::OnUpdateWhenNotInGamePost(float flFrameTime)
{
	g_pUtilsApi->SendServerUpdateWhenNotInGamePost(flFrameTime);
}

void Menus::OnServerConVarChangedPost(const char *pVarName, const char *pValue)
{
	g_pUtilsApi->SendServerConVarChangedPost(pVarName, pValue);
}

void Menus::OnPostEventAbstractPost(CSplitScreenSlot nSlot, bool bLocalOnly, int nClientCount, const uint64 *clients, INetworkMessageInternal *pEvent, const CNetMessage *pData, unsigned long nSize, NetChannelBufType_t bufType)
{
	g_pUtilsApi->SendServerPostEventPost(nClientCount, clients, pEvent, pData);
}

bool Menus::OnSetClientListeningPost(CPlayerSlot iReceiver, CPlayerSlot iSender, bool bListen)
{
	g_pUtilsApi->SendServerSetClientListeningPost(iReceiver.Get(), iSender.Get(), bListen);
	RETURN_META_VALUE(MRES_IGNORED, bListen);
}

void Menus::OnClientConnected(CPlayerSlot slot, const char* pszName, uint64 xuid, const char* pszNetworkID, const char* pszAddress, bool bFakePlayer)
{
	if(bFakePlayer)
		m_Players[slot.Get()] = new Player(slot.Get(), true);
	g_pPlayersApi->OnClientConnected(slot.Get());
}

bool Menus::OnClientConnect( CPlayerSlot slot, const char *pszName, uint64 xuid, const char *pszNetworkID, bool unk1, CBufferString *pRejectReason )
{
	Player *pPlayer = new Player(slot.Get());
	pPlayer->SetUnauthenticatedSteamId(new CSteamID(xuid));

	std::string ip(pszNetworkID);

	for (size_t i = 0; i < ip.length(); i++)
	{
		if (ip[i] == ':')
		{
			ip = ip.substr(0, i);
			break;
		}
	}
	pPlayer->SetIpAddress(ip);
	pPlayer->SetConnected();
	m_Players[slot.Get()] = pPlayer;
	if(!g_pPlayersApi->ClientConnect(slot.Get()))
		RETURN_META_VALUE(MRES_SUPERCEDE, false);
	RETURN_META_VALUE(MRES_IGNORED, true);
}

void Menus::OnClientPutInServer( CPlayerSlot slot, char const *pszName, int type, uint64 xuid )
{
	m_Players[slot.Get()]->SetInGame(true);

	int iSlot = slot.Get();
	if(iSlot >= 0 && iSlot < 64 && !g_pPlayersApi->IsFakeClient(iSlot))
		UTIL_EnsureLayout(iSlot);
	g_pPlayersApi->ClientPutInServer(iSlot);
}

void Menus::OnClientActive( CPlayerSlot slot, bool bLoadGame, char const *pszName, uint64 xuid )
{
	g_pPlayersApi->ClientActive(slot.Get());
}

void Menus::OnClientFullyConnect( CPlayerSlot slot )
{
	g_pPlayersApi->ClientFullyConnect(slot.Get());
}

void Menus::OnClientSettingsChanged( CPlayerSlot slot )
{
	g_pPlayersApi->ClientSettingsChanged(slot.Get());
}

void Menus::OnProcessUsercmds( CPlayerSlot slot, const CCLCMsg_Move_t &msg, bool paused )
{
	g_pPlayersApi->ProcessUsercmds(slot.Get(), msg, paused);
}

void Menus::OnClientVoice( CPlayerSlot slot )
{
	g_pPlayersApi->ClientVoice(slot.Get());
}

void Menus::OnClientCommandKeyValues( CPlayerSlot slot, KeyValues *pKeyValues )
{
	g_pPlayersApi->ClientCommandKeyValues(slot.Get(), pKeyValues);
}

bool Menus::OnProcessClientVoiceData( CPlayerSlot slot, void *pVoiceInfo )
{
	if(!g_pPlayersApi->ProcessClientVoiceData(slot.Get(), pVoiceInfo))
		RETURN_META_VALUE(MRES_SUPERCEDE, false);
	RETURN_META_VALUE(MRES_IGNORED, true);
}

void Menus::OnValidateAuthTicketHook(ValidateAuthTicketResponse_t *pResponse)
{
	uint64 iSteamId = pResponse->m_SteamID.ConvertToUint64();
	for (int i = 0; i < 64; i++)
	{
		if (!m_Players[i] || m_Players[i]->IsFakeClient() || !(m_Players[i]->GetUnauthenticatedSteamId64() == iSteamId))
			continue;
		switch (pResponse->m_eAuthSessionResponse)
		{
			case k_EAuthSessionResponseOK:
			{
				if(m_Players[i]->IsAuthenticated())
					return;
				m_Players[i]->SetAuthenticated(true);
				m_Players[i]->SetSteamId(m_Players[i]->GetUnauthenticatedSteamId());
				g_pPlayersApi->SendClientAuthCallback(i, iSteamId);

				if(!g_pCookies) {
					g_iMenuType[i] = GetClientCookieMenuType(i);
					g_bNotifyDisabled[i] = GetClientCookieNotifyDisabled(i);
				}
				return;
			}

			case k_EAuthSessionResponseAuthTicketInvalid:
			case k_EAuthSessionResponseAuthTicketInvalidAlreadyUsed:
			{
				if (!g_iDelayAuthFailKick)
					return;

				g_pUtilsApi->PrintToChat(i, g_vecPhrases["AuthTicketInvalid"].c_str());
				[[fallthrough]];
			}

			default:
			{
				if (!g_iDelayAuthFailKick)
					return;

				g_pUtilsApi->PrintToChat(i, g_vecPhrases["AuthFailed"].c_str(), g_iDelayAuthFailKick);

				new CTimer(g_iDelayAuthFailKick, [i]()
				{
					if(!m_Players[i] || m_Players[i]->IsFakeClient() || m_Players[i]->IsAuthenticated()) return -1.f;
					engine->DisconnectClient(i, NETWORK_DISCONNECT_KICKED_NOSTEAMLOGIN);
					return -1.f;
				});
			}
		}
	}
}

void UtilsApi::LoadTranslations(const char* FileName)
{
	KeyValues::AutoDelete g_kvPhrases("Phrases");
	char pszPath[256];
	g_SMAPI->Format(pszPath, sizeof(pszPath), "addons/translations/%s.txt", FileName);

	if (!g_kvPhrases->LoadFromFile(g_pFullFileSystem, pszPath))
	{
		Warning("Failed to load %s\n", pszPath);
		return;
	}

	for (KeyValues *pKey = g_kvPhrases->GetFirstTrueSubKey(); pKey; pKey = pKey->GetNextTrueSubKey())
		g_vecPhrases[std::string(pKey->GetName())] = std::string(pKey->GetString(szLanguage));
}

bool Menus::FireEvent(IGameEvent* pEvent, bool bDontBroadcast)
{
    if (!pEvent) {
        RETURN_META_VALUE(MRES_IGNORED, false);
    }

    const char* szName = pEvent->GetName();
	g_pUtilsApi->SendHookEventCallback(szName, pEvent, bDontBroadcast);
	g_pUtilsApi->SendServerFireEvent(pEvent, bDontBroadcast);

	m_EventCopies.push(gameeventmanager->DuplicateEvent(pEvent));

	EventInfo info{ bDontBroadcast };
	EventHookResult result = g_pUtilsApi->SendEventHookPre(szName, pEvent, &info);

	if(result >= EventHookResult::Handled) {
		gameeventmanager->FreeEvent(pEvent);
		RETURN_META_VALUE(MRES_SUPERCEDE, false);
	}

	if(info.bDontBroadcast != bDontBroadcast) {
		RETURN_META_VALUE_NEWPARAMS(MRES_IGNORED, true, &IGameEventManager2::FireEvent, (pEvent, info.bDontBroadcast));
	}

	RETURN_META_VALUE(MRES_IGNORED, true);
}

bool Menus::OnFireEventPost(IGameEvent* pEvent, bool bDontBroadcast)
{
	if(m_EventCopies.empty()) {
		RETURN_META_VALUE(MRES_IGNORED, true);
	}
	IGameEvent* pCopy = m_EventCopies.top();
	m_EventCopies.pop();
	if(pCopy) {
		g_pUtilsApi->SendServerFireEventPost(pCopy, bDontBroadcast);
		g_pUtilsApi->SendEventHookPost(pCopy->GetName(), pCopy, bDontBroadcast);
		gameeventmanager->FreeEvent(pCopy);
	}
	RETURN_META_VALUE(MRES_IGNORED, true);
}

void Menus::GameFrame(bool simulating, bool bFirstTick, bool bLastTick)
{
	g_pUtilsApi->SendServerGameFrame(simulating, bFirstTick, bLastTick);
	if(!g_pGameRules)
	{
		CCSGameRulesProxy* pGameRulesProxy = static_cast<CCSGameRulesProxy*>(UTIL_FindEntityByClassname("cs_gamerules"));
		if (pGameRulesProxy)
		{
			g_pGameRules = pGameRulesProxy->m_pGameRules();
			if(g_pGameRules) g_pUtilsApi->SendHookGameRules();
		}
	} else if(g_bMenuFlashFix) {
		if(!g_pGameRules->m_bWarmupPeriod() && !g_pGameRules->m_bGameRestart()) g_pGameRules->m_bGameRestart() = g_pGameRules->m_flRestartRoundTime().GetTime() < gpGlobals->curtime;
		else g_pGameRules->m_bGameRestart() = false;
	}
	g_pUtilsApi->NextFrame();

	if (simulating && g_bHasTicked)
	{
		g_flUniversalTime += gpGlobals->curtime - g_flLastTickedTime;
	}

	g_flLastTickedTime = gpGlobals->curtime;
	g_bHasTicked = true;

	for (int i = g_timers.Count() - 1; i >= 0; i--)
	{
		auto timer = g_timers[i];

		if (timer->m_flLastExecute == -1)
			timer->m_flLastExecute = g_flUniversalTime;

		if (timer->m_flLastExecute + timer->m_flInterval <= g_flUniversalTime)
		{
			if (!timer->Execute())
			{
				delete timer;
				g_timers.Remove(i);
			}
			else
			{
				timer->m_flLastExecute = g_flUniversalTime;
			}
		}
	}
}

void Menus::ClientCommand(CPlayerSlot slot, const CCommand &args)
{
	g_pPlayersApi->ClientCommand(slot.Get(), args);
	bool bFound = g_pUtilsApi->FindAndSendCommandCallback(args.Arg(0), slot.Get(), args.ArgS(), true);
	if(bFound) RETURN_META(MRES_SUPERCEDE);
}

std::string StripQuotes(const std::string& str) {
	if (str.size() >= 2 && str.front() == '"' && str.back() == '"')
		return str.substr(1, str.size() - 2);
	return str;
}

std::string TrimTrailingQuote(const std::string& str) {
	if (!str.empty() && str.back() == '"')
		return str.substr(0, str.size() - 1);
	return str;
}

std::vector<std::string> SplitStringBySpace(const std::string& input) {
	std::istringstream iss(input);
	std::vector<std::string> tokens;
	std::string token;
	while (iss >> token) {
		token = TrimTrailingQuote(token);
		tokens.push_back(token);
	}
	return tokens;
}

void Menus::OnDispatchConCommand(ConCommandRef cmdHandle, const CCommandContext& ctx, const CCommand& args)
{
	if (!g_pEntitySystem)
		return;

	auto iCommandPlayerSlot = ctx.GetPlayerSlot();
	if(!g_pUtilsApi->SendServerDispatchConCommand(iCommandPlayerSlot.Get(), args))
		RETURN_META(MRES_SUPERCEDE);
	bool bSay = !V_strcmp(args.Arg(0), "say");
	bool bTeamSay = !V_strcmp(args.Arg(0), "say_team");
	int iSlot = iCommandPlayerSlot.Get();
	if (iSlot != -1 && (bSay || bTeamSay))
	{
		auto pController = CCSPlayerController::FromSlot(iSlot);
		bool bCommand = *args[1] == '!' || *args[1] == '/';
		if (pController && bCommand)
		{
			std::string message = args.ArgS() + 2;
			auto tokens = SplitStringBySpace(message);
			if (!tokens.empty())
			{
				if (containsOnlyDigits(tokens[0]))
				{
					if (g_iMenuType[iSlot] < 2)
					{
						int iButton = atoi(tokens[0].c_str());
						if (CheckActionMenu(iSlot, pController, iButton))
							RETURN_META(MRES_SUPERCEDE);
					}
				}
			}
		}

		if (std::string(args[1]).size() > 1)
		{
			const char* pszArgS = args.ArgS();
			const char* pszMessage = (pszArgS[0] == '"') ? pszArgS + 1 : pszArgS;
			auto tokens = SplitStringBySpace(pszMessage);
			if (!tokens.empty())
			{
				const char* arg0 = tokens[0].c_str();
				bool bFound = g_pUtilsApi->FindAndSendCommandCallback(arg0, iSlot, pszMessage, false);
				if (bFound) RETURN_META(MRES_SUPERCEDE);
				else if (g_vCommandEater.size() > 0 && g_pUtilsApi->FindCommand(arg0))
				{
					for (auto& command : g_vCommandEater)
					{
						if (arg0[0] == command[0])
						{
							RETURN_META(MRES_SUPERCEDE);
						}
					}
				}
			}
		}
	} else {
		bool bFound = g_pUtilsApi->FindAndSendCommandCallback(args.Arg(0), iSlot, args.ArgS(), true);
		if (bFound) RETURN_META(MRES_SUPERCEDE);
	}
}

void Menus::OnDispatchConCommandPost(ConCommandRef cmdHandle, const CCommandContext& ctx, const CCommand& args)
{
	g_pUtilsApi->SendServerDispatchConCommandPost(ctx.GetPlayerSlot().Get(), args);
}

CGlobalVars* getGlobalVars()
{
    INetworkGameServer* server = g_pNetworkServerService->GetIGameServer();
    if (!server) return nullptr;
    return g_pNetworkServerService->GetIGameServer()->GetGlobals();
}

void Menus::StartupServer(const GameSessionConfiguration_t& config, ISource2WorldSession*, const char*)
{
	for(int i = 0; i < 64; i++)
	{
		g_pMenusCore->ClosePlayerMenu(i);
		for (auto& [panelId, layout] : g_mapHudLayouts[i])
		{
			if(layout) g_mapTransmitState[layout->entindex()].clear();
		}
		g_mapHudLayouts[i].clear();
	}
	for (auto& [name, layout] : g_mapGlobalHudLayouts)
	{
		if(layout) g_mapTransmitState[layout->entindex()].clear();
	}
	g_mapGlobalHudLayouts.clear();
	g_Offsets.clear();
	g_ChainOffsets.clear();
	g_pGameRules = nullptr;
	g_pEntitySystem = GameEntitySystem();
	gpGlobals = getGlobalVars();
	if(g_bHasTicked) {
		g_pUtilsApi->SendHookMapEnd();
		g_pUtilsApi->SendServerMapEnd();
	} else {
		char szMapName[256];
		g_SMAPI->Format(szMapName, sizeof(szMapName), "%s", gpGlobals->mapname);
		g_pUtilsApi->SendHookMapStart(szMapName);
		g_pUtilsApi->SendServerMapStart(szMapName);
	}
	g_bHasTicked = false;
	g_pUtilsApi->SendHookStartup();
	static bool s_bServerStarted = false;
	if(!s_bServerStarted)
	{
		s_bServerStarted = true;
		g_pUtilsApi->SendServerStartup();
	}
	g_pGameEntitySystem->AddListenerEntity(this);
}

void Menus::OnClientDisconnect( CPlayerSlot slot, ENetworkDisconnectionReason reason, const char *pszName, uint64 xuid, const char *pszNetworkID )
{
	int iSlot = slot.Get();
	g_pPlayersApi->ClientDisconnect(iSlot);
	g_pMenusCore->ClosePlayerMenu(iSlot);
	if(iSlot >= 0 && iSlot < 64)
		UTIL_DestroyLayout(iSlot);
	if (iSlot < 0 || iSlot >= 64 || !m_Players[iSlot]) return;
	delete m_Players[iSlot];
	m_Players[iSlot] = nullptr;

	if (xuid == 0)
    	return;

	g_MenuPlayer[iSlot].clear();
	g_TextMenuPlayer[iSlot] = "";
	g_iMenuItem[iSlot] = 1;
	g_szMenuDesc[iSlot].clear();
	g_vItemExtra[iSlot].clear();
}

const char* UtilsApi::GetVersion()
{
	return g_PLAPI->GetVersion();
}

const char* UtilsApi::GetServerID()
{
	return g_szServerID.c_str();
}

///////////////////////////////////////
const char* Menus::GetLicense()
{
	return "GPL";
}

const char* Menus::GetVersion()
{
	return "1.9.1";
}

const char* Menus::GetDate()
{
	return __DATE__;
}

const char *Menus::GetLogTag()
{
	return "GameUtils";
}

const char* Menus::GetAuthor()
{
	return "Pisex";
}

const char* Menus::GetDescription()
{
	return "Game Utils";
}

const char* Menus::GetName()
{
	return "Game Utils";
}

const char* Menus::GetURL()
{
	return "https://discord.gg/g798xERK5Y";
}
