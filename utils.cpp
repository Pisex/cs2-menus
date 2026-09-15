#include <stdio.h>
#include "utils.h"
#include "metamod_oslink.h"
#include "schemasystem/schemasystem.h"
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

std::unordered_map<const Menu*, std::string> g_mapMenuDesc;
std::string g_szMenuDesc[64];

struct ItemExtra
{
	ItemKind iKind = ItemKind::BUTTON;
	bool bToggled = false;
	MenuToggleCallbackFunc hToggleFunc = nullptr;
	std::vector<SelectOption> vOptions;
	int iSelected = 0;
	bool bOpened = false;
	MenuSelectCallbackFunc hSelectFunc = nullptr;
};
std::unordered_map<const Menu*, std::vector<ItemExtra>> g_mapItemExtra;
std::vector<ItemExtra> g_vItemExtra[64];

static ItemExtra& UTIL_PushItemExtra(Menu& hMenu)
{
	auto& ex = g_mapItemExtra[&hMenu];
	ex.resize(hMenu.hItems.size());
	ex.back() = ItemExtra{};
	return ex.back();
}

static ItemExtra* UTIL_ResolveItemExtra(const ItemRef& ref)
{
	if (!ref.pMenu || ref.iIndex < 0) return nullptr;
	auto it = g_mapItemExtra.find(ref.pMenu);
	if (it == g_mapItemExtra.end()) return nullptr;
	if (ref.iIndex >= (int)it->second.size()) return nullptr;
	return &it->second[ref.iIndex];
}

std::vector<std::string> g_vCommandEater;
std::map<int, std::vector<int>> g_mapTransmitState;

int g_iOnTakeDamageAliveId = -1;

SH_DECL_HOOK0_void(IServerGameDLL, GameServerSteamAPIActivated, SH_NOATTRIB, 0);
SH_DECL_HOOK3_void(IServerGameDLL, GameFrame, SH_NOATTRIB, 0, bool, bool, bool);
SH_DECL_HOOK2(IGameEventManager2, FireEvent, SH_NOATTRIB, 0, bool, IGameEvent*, bool);
SH_DECL_HOOK2_void(IServerGameClients, ClientCommand, SH_NOATTRIB, 0, CPlayerSlot, const CCommand&);
SH_DECL_HOOK3_void(ICvar, DispatchConCommand, SH_NOATTRIB, 0, ConCommandRef, const CCommandContext&, const CCommand&);
SH_DECL_HOOK3_void(INetworkServerService, StartupServer, SH_NOATTRIB, 0, const GameSessionConfiguration_t&, ISource2WorldSession*, const char*);
SH_DECL_HOOK5_void(IServerGameClients, ClientDisconnect, SH_NOATTRIB, 0, CPlayerSlot, ENetworkDisconnectionReason, const char *, uint64, const char *);
SH_DECL_HOOK4_void(IServerGameClients, ClientPutInServer, SH_NOATTRIB, 0, CPlayerSlot, char const *, int, uint64);
SH_DECL_HOOK6_void(IServerGameClients, OnClientConnected, SH_NOATTRIB, 0, CPlayerSlot, const char*, uint64, const char *, const char *, bool);
SH_DECL_HOOK6(IServerGameClients, ClientConnect, SH_NOATTRIB, 0, bool, CPlayerSlot, const char*, uint64, const char *, bool, CBufferString *);
SH_DECL_HOOK8_void(ISource2GameEntities, CheckTransmit, SH_NOATTRIB, 0, CCheckTransmitInfo **, int, CBitVec<16384> &, CBitVec<16384> &, const Entity2Networkable_t **, const uint16 *, int, bool);
SH_DECL_MANUALHOOK1(OnTakeDamage_Alive, 0, 0, 0, bool, CTakeDamageInfoContainer *);
SH_DECL_HOOK4_void(IServerGameClients, ClientSvcUserMessage, SH_NOATTRIB, 0, CPlayerSlot, int, uint32, const void *);

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

// void* (*UTIL_SetHasClass)(CCSCustomHudLayout* layout, CUtlString panelId, CUtlString className, bool bHasClass) = nullptr;
// void* (*UTIL_SetInputCaptureEnabled)(CCSCustomHudLayout* layout, int iSlot, bool enable) = nullptr;
// void* (*UTIL_SetDialogVariableStringForPlayer)(CCSCustomHudLayout* layout, int iSlot, CUtlString panelId, CUtlString variableName, CUtlString value) = nullptr;

// void (*UTIL_ClientPrint)(CBasePlayerController*, int, const char *, const char *, const char *, const char *, const char *) = nullptr;
// void (*UTIL_ClientPrintAll)(int, const char *, const char *, const char *, const char *, const char *) = nullptr;

using namespace DynLibUtils;

funchook_t* m_SayHook;
funchook_t* m_SayTeamHook;
funchook_t* m_TakeDamageHook;
funchook_t* m_IsHearingClientHook;

static void UTIL_HandleLayoutClick(int iSlot, const char* szLayoutName, const char* szButton);
static void UTIL_EnsureLayout(int iSlot);
static void UTIL_DestroyLayout(int iSlot);
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

static int UTIL_SanitizeMenuType(int iType);

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

static int UTIL_SanitizeMenuType(int iType)
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

void Menus::OnCheckTransmit(CCheckTransmitInfo **pInfoInfoList, int nInfoCount, CBitVec<16384> &unionTransmitEdicts, CBitVec<16384> &, const Entity2Networkable_t **pNetworkables, const uint16 *pEntityIndicies, int nEntityIndices, bool bEnablePVSBits)
{
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

void Menus::OnClientSvcUserMessage( CPlayerSlot slot, int um_type, uint32 size, const void *buf )
{
	if (um_type != CS_UM_CustomHudClicked)
		return;

	CCSUsrMsg_CustomHudClicked msg;
	if (!msg.ParseFromArray(buf, size))
		return;

	const int iSlot = slot.Get();
	const std::string sButtonId = msg.button_id();
	const char* szButton = sButtonId.c_str();

	UTIL_HandleLayoutClick(iSlot, "utils-dialog", szButton);
	g_pLayoutApi->SendCustomHudClickedCallback(iSlot, "utils-dialog", szButton);
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
	SH_ADD_HOOK(IServerGameClients, ClientCommand, g_pSource2GameClients, SH_MEMBER(this, &Menus::ClientCommand), false);
	SH_ADD_HOOK(INetworkServerService, StartupServer, g_pNetworkServerService, SH_MEMBER(this, &Menus::StartupServer), true);
	SH_ADD_HOOK(IServerGameDLL, GameServerSteamAPIActivated, g_pSource2Server, SH_MEMBER(this, &Menus::OnGameServerSteamAPIActivated), false);
	SH_ADD_HOOK(IServerGameClients, ClientDisconnect, g_pSource2GameClients, SH_MEMBER(this, &Menus::OnClientDisconnect), true);
	SH_ADD_HOOK(IServerGameClients, ClientPutInServer, g_pSource2GameClients, SH_MEMBER(this, &Menus::OnClientPutInServer), true);
	SH_ADD_HOOK(IServerGameClients, OnClientConnected, g_pSource2GameClients, SH_MEMBER(this, &Menus::OnClientConnected), false);
	SH_ADD_HOOK(IServerGameClients, ClientConnect, g_pSource2GameClients, SH_MEMBER(this, &Menus::OnClientConnect), false );	
	SH_ADD_HOOK(ISource2GameEntities, CheckTransmit, g_pSource2GameEntities, SH_MEMBER(this, &Menus::OnCheckTransmit), true);
	SH_ADD_HOOK(IServerGameClients, ClientSvcUserMessage, g_pSource2GameClients, SH_MEMBER(this, &Menus::OnClientSvcUserMessage), false);
	
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

	// UTIL_ClientPrint = libserver.FindPattern("55 48 89 E5 41 57 41 56 41 55 41 54 53 48 83 EC 38 4C 89 45 A0").RCast< decltype(UTIL_ClientPrint) >();
	// if(!UTIL_ClientPrint) g_pUtilsApi->ErrorLog("[%s] Failed to find function to get UTIL_ClientPrint", g_PLAPI->GetLogTag());

	// UTIL_ClientPrintAll = libserver.FindPattern("55 48 89 E5 41 57 4D 89 CF 41 56 4D 89 C6 41 55 49 89 CD 41 54 49 89 D4 53 48 8D 5D B0").RCast< decltype(UTIL_ClientPrintAll) >();
	// if(!UTIL_ClientPrintAll) g_pUtilsApi->ErrorLog("[%s] Failed to find function to get UTIL_ClientPrintAll", g_PLAPI->GetLogTag());

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

	// const char* pszSetHasClass = g_kvSigs->GetString("CustomHudLayout::SetHasClass");
	// if(pszSetHasClass && pszSetHasClass[0]) {
	// 	UTIL_SetHasClass = libserver.FindPattern(pszSetHasClass).RCast< decltype(UTIL_SetHasClass) >();
	// 	if (!UTIL_SetHasClass)
	// 	{
	// 		g_pUtilsApi->ErrorLog("[%s] Failed to find function to get UTIL_SetHasClass", g_PLAPI->GetLogTag());
	// 	}
	// }

	// const char* pszSetInputCaptureEnabled = g_kvSigs->GetString("CustomHudLayout::SetInputCaptureEnabled");
	// if(pszSetInputCaptureEnabled && pszSetInputCaptureEnabled[0]) {
	// 	UTIL_SetInputCaptureEnabled = libserver.FindPattern(pszSetInputCaptureEnabled).RCast< decltype(UTIL_SetInputCaptureEnabled) >();
	// 	if (!UTIL_SetInputCaptureEnabled)
	// 	{
	// 		g_pUtilsApi->ErrorLog("[%s] Failed to find function to get UTIL_SetInputCaptureEnabled", g_PLAPI->GetLogTag());
	// 	}
	// }

	// const char* pszSetDialogVariableStringForPlayer = g_kvSigs->GetString("CustomHudLayout::SetDialogVariableStringForPlayer");
	// if(pszSetDialogVariableStringForPlayer && pszSetDialogVariableStringForPlayer[0]) {
	// 	UTIL_SetDialogVariableStringForPlayer = libserver.FindPattern(pszSetDialogVariableStringForPlayer).RCast< decltype(UTIL_SetDialogVariableStringForPlayer) >();
	// 	if (!UTIL_SetDialogVariableStringForPlayer)
	// 	{
	// 		g_pUtilsApi->ErrorLog("[%s] Failed to find function to get UTIL_SetDialogVariableStringForPlayer", g_PLAPI->GetLogTag());
	// 	}
	// }

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

	const char* pszGameTraceManager = g_kvSigs->GetString("GetGameTraceManager");
	if(pszGameTraceManager && pszGameTraceManager[0]) {
		auto gameTraceManagerFn = libserver.FindPattern(pszGameTraceManager);
		if( !gameTraceManagerFn ) g_pUtilsApi->ErrorLog("[%s] Failed to find function to get GetGameTraceManager", g_PLAPI->GetLogTag());
		else g_pGameTraceManager = *gameTraceManagerFn.ResolveRelativeAddress(3, 7).RCast<CPhysicsQuery**>();
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

bool Menus::Unload(char *error, size_t maxlen)
{
	SH_REMOVE_HOOK_MEMFUNC(ICvar, DispatchConCommand, g_pCVar, this, &Menus::OnDispatchConCommand, false);
	SH_REMOVE_HOOK(IServerGameDLL, GameFrame, g_pSource2Server, SH_MEMBER(this, &Menus::GameFrame), true);
	SH_REMOVE_HOOK(IGameEventManager2, FireEvent, gameeventmanager, SH_MEMBER(this, &Menus::FireEvent), false);
	SH_REMOVE_HOOK(IServerGameClients, ClientCommand, g_pSource2GameClients, SH_MEMBER(this, &Menus::ClientCommand), false);
	SH_REMOVE_HOOK(INetworkServerService, StartupServer, g_pNetworkServerService, SH_MEMBER(this, &Menus::StartupServer), true);
	SH_REMOVE_HOOK(IServerGameDLL, GameServerSteamAPIActivated, g_pSource2Server, SH_MEMBER(this, &Menus::OnGameServerSteamAPIActivated), false);
	SH_REMOVE_HOOK(IServerGameClients, ClientDisconnect, g_pSource2GameClients, SH_MEMBER(this, &Menus::OnClientDisconnect), true);
	SH_REMOVE_HOOK(IServerGameClients, ClientPutInServer, g_pSource2GameClients, SH_MEMBER(this, &Menus::OnClientPutInServer), true);
	SH_REMOVE_HOOK(IServerGameClients, OnClientConnected, g_pSource2GameClients, SH_MEMBER(this, &Menus::OnClientConnected), false);
	SH_REMOVE_HOOK(IServerGameClients, ClientConnect, g_pSource2GameClients, SH_MEMBER(this, &Menus::OnClientConnect), false );

	if(g_iOnTakeDamageAliveId) SH_REMOVE_HOOK_ID(g_iOnTakeDamageAliveId);
	if(m_SayHook) funchook_destroy(m_SayHook);
	if(m_SayTeamHook) funchook_destroy(m_SayTeamHook);
	if(m_TakeDamageHook) funchook_destroy(m_TakeDamageHook);

	ConVar_Unregister();
	
	return true;
}

void Menus::OnGameServerSteamAPIActivated()
{
	m_CallbackValidateAuthTicketResponse.Register(this, &Menus::OnValidateAuthTicketHook);
}

void Menus::OnClientConnected(CPlayerSlot slot, const char* pszName, uint64 xuid, const char* pszNetworkID, const char* pszAddress, bool bFakePlayer)
{
	if(bFakePlayer)
		m_Players[slot.Get()] = new Player(slot.Get(), true);
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
	RETURN_META_VALUE(MRES_IGNORED, true);
}

void Menus::OnClientPutInServer( CPlayerSlot slot, char const *pszName, int type, uint64 xuid )
{
	m_Players[slot.Get()]->SetInGame(true);

	int iSlot = slot.Get();
	if(iSlot >= 0 && iSlot < 64 && !g_pPlayersApi->IsFakeClient(iSlot))
		UTIL_EnsureLayout(iSlot);
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
    RETURN_META_VALUE(MRES_IGNORED, true);
}

void Menus::GameFrame(bool simulating, bool bFirstTick, bool bLastTick)
{
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
	g_Offsets.clear();
	g_ChainOffsets.clear();
	g_pGameRules = nullptr;
	g_pEntitySystem = GameEntitySystem();
	gpGlobals = getGlobalVars();
	if(g_bHasTicked) {
		g_pUtilsApi->SendHookMapEnd();
	} else {
		char szMapName[256];
		g_SMAPI->Format(szMapName, sizeof(szMapName), "%s", gpGlobals->mapname);
		g_pUtilsApi->SendHookMapStart(szMapName);
	}
	g_bHasTicked = false;
	g_pUtilsApi->SendHookStartup();
}

void Menus::OnClientDisconnect( CPlayerSlot slot, ENetworkDisconnectionReason reason, const char *pszName, uint64 xuid, const char *pszNetworkID )
{
	int iSlot = slot.Get();
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

bool MenusApi::IsMenuOpen(int iSlot) {
	return g_MenuPlayer[iSlot].bEnabled;
}

void MenusApi::SetTitleMenu(Menu& hMenu, const char* szTitle) {
	hMenu.szTitle = std::string(szTitle);
}

void MenusApi::SetDescriptionMenu(Menu& hMenu, const char* szDescription) {
	if(szDescription && szDescription[0])
		g_mapMenuDesc[&hMenu] = szDescription;
	else
		g_mapMenuDesc.erase(&hMenu);
}

void MenusApi::SetBackMenu(Menu& hMenu, bool bBack) {
	hMenu.bBack = bBack;
}

void MenusApi::SetExitMenu(Menu& hMenu, bool bExit) {
	hMenu.bExit = bExit;
}

std::string GetMenuText(int iSlot)
{
	if (iSlot < 0 || iSlot >= 64) return "";
	auto& hMenuPlayer = g_MenuPlayer[iSlot];
	auto& hMenu = hMenuPlayer.hMenu;

	CCSPlayerController* pController = CCSPlayerController::FromSlot(iSlot);
	if(!pController) return "";
	CBasePlayerPawn* pPlayerPawn = pController->m_hPawn();
	if(!pPlayerPawn) return "";
	CPlayer_MovementServices* pMovementServices = pPlayerPawn->m_pMovementServices();
	if(!pMovementServices) return "";
	CCSPlayerPawn* pPawn = pController->GetPlayerPawn();
	if(pPawn && pPawn->IsAlive() && g_bStopingUser && pPlayerPawn->m_nActualMoveType() == MOVETYPE_WALK) {
		g_pPlayersApi->SetMoveType(iSlot, MOVETYPE_NONE);
	}
	int buttons = pMovementServices->m_nButtons().m_pButtonStates()[0];
	auto now = std::chrono::system_clock::now();
	std::chrono::milliseconds iTime = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch());

	bool bUp = false;
	bool bDown = false;
	bool bLeft = false;
	bool bRight = false;
	bool bEnter = false;

	if(buttons)
	{
		if(buttons & (1 << 3))
			bUp = true;
		else if(buttons & (1 << 4))
			bDown = true;
		else if(buttons & (1 << 9))
			bLeft = true;
		else if(buttons & (1 << 10))
			bRight = true;
		else if(buttons & (1 << 5))
			bEnter = true;
		if(g_iMenuLastButtonInput[iSlot] < iTime) {
			g_iMenuLastButtonInput[iSlot] = iTime + std::chrono::milliseconds(g_iTimeoutMenu);
			if(buttons & (1 << 3)) {
				if(g_iMenuItem[iSlot] > 1) {
					if(g_mapSounds.find("move") != g_mapSounds.end()) {
						const char* szSound = g_mapSounds["move"].c_str();
						if(g_iSoundType == 1) g_pPlayersApi->EmitSound(iSlot, pPawn->entindex(), szSound, 100, 1.0f);
						else if(g_iSoundType == 2) engine->ClientCommand(iSlot, "play %s", szSound);
					}
					g_iMenuItem[iSlot]--;
				}
			} else if(buttons & (1 << 4)) {
				if(g_iMenuItem[iSlot] < 5 && g_iMenuItem[iSlot] < hMenu.hItems.size()) {
					if(g_mapSounds.find("move") != g_mapSounds.end()) {
						const char* szSound = g_mapSounds["move"].c_str();
						if(g_iSoundType == 1) g_pPlayersApi->EmitSound(iSlot, pPawn->entindex(), szSound, 100, 1.0f);
						else if(g_iSoundType == 2) engine->ClientCommand(iSlot, "play %s", szSound);
					}
					g_iMenuItem[iSlot]++;
				}
			} else if(buttons & (1 << 9)) {
				if(hMenuPlayer.iList != 0 || hMenuPlayer.hMenu.bBack)
				{
					g_iMenuItem[iSlot] = 1;
					if(hMenuPlayer.iList != 0)
					{
						hMenuPlayer.iList--;
						hMenuPlayer.iEnd = std::time(0) + g_iMenuTime;
					}
					else if(hMenu.hFunc) hMenu.hFunc("back", "back", 7, iSlot);
					if(g_mapSounds.find("back") != g_mapSounds.end()) {
						const char* szSound = g_mapSounds["back"].c_str();
						if(g_iSoundType == 1) g_pPlayersApi->EmitSound(iSlot, pPawn->entindex(), szSound, 100, 1.0f);
						else if(g_iSoundType == 2) engine->ClientCommand(iSlot, "play %s", szSound);
					}
				}
			} else if(buttons & (1 << 10)) {
				int iItems = size(hMenu.hItems) / 5;
				if (size(hMenu.hItems) % 5 > 0) iItems++;
				if(iItems > hMenuPlayer.iList+1)
				{
					g_iMenuItem[iSlot] = 1;
					hMenuPlayer.iList++;
					hMenuPlayer.iEnd = std::time(0) + g_iMenuTime;
					if(hMenu.hFunc) hMenu.hFunc("next", "next", 8, iSlot);
					if(g_mapSounds.find("next") != g_mapSounds.end()) {
						const char* szSound = g_mapSounds["next"].c_str();
						if(g_iSoundType == 1) g_pPlayersApi->EmitSound(iSlot, pPawn->entindex(), szSound, 100, 1.0f);
						else if(g_iSoundType == 2) engine->ClientCommand(iSlot, "play %s", szSound);
					}
				}
			} else if(buttons & (1 << 5)) {
				int iButton = g_iMenuItem[iSlot];
				g_iMenuItem[iSlot] = 1;
				int iItems = size(hMenu.hItems);
				int iItem = hMenuPlayer.iList*5+iButton-1;
				if(iItems > iItem && hMenu.hItems[iItem].iType == 1)
				{
					if(hMenu.hFunc) hMenu.hFunc(hMenu.hItems[iItem].sBack.c_str(), hMenu.hItems[iItem].sText.c_str(), iButton, iSlot);
					
					if(g_mapSounds.find("select") != g_mapSounds.end()) {
						const char* szSound = g_mapSounds["select"].c_str();
						if(g_iSoundType == 1) g_pPlayersApi->EmitSound(iSlot, pPawn->entindex(), szSound, 100, 1.0f);
						else if(g_iSoundType == 2) engine->ClientCommand(iSlot, "play %s", szSound);
					}
				}
			} else if(buttons & (1 << 13) && hMenu.bExit) {
				if(g_bStopingUser) g_pPlayersApi->SetMoveType(iSlot, MOVETYPE_WALK);
				CheckActionMenu(iSlot, CCSPlayerController::FromSlot(iSlot), 9);
				if(g_mapSounds.find("exit") != g_mapSounds.end()) {
					const char* szSound = g_mapSounds["exit"].c_str();
					if(g_iSoundType == 1) g_pPlayersApi->EmitSound(iSlot, pPawn->entindex(), szSound, 100, 1.0f);
					else if(g_iSoundType == 2) engine->ClientCommand(iSlot, "play %s", szSound);
				}
				return "";
			}
		}
	}

	std::string sBuff = "";
	char sBuff2[256];
	int iCount = 0;
	int iItems = size(hMenu.hItems) / 5;
	if (size(hMenu.hItems) % 5 > 0) iItems++;
	g_SMAPI->Format(sBuff2, sizeof(sBuff2), g_vecPhrases[std::string("HtmlTitle")].c_str(), hMenu.szTitle.c_str());
	sBuff += std::string(sBuff2);
	for (size_t l = hMenuPlayer.iList*5; l < hMenu.hItems.size(); ++l) {
		switch (hMenu.hItems[l].iType)
		{
			case 1:
				g_SMAPI->Format(sBuff2, sizeof(sBuff2), g_vecPhrases[std::string("HtmlButton")].c_str(), hMenu.hItems[l].sText.c_str());
				sBuff += std::string(sBuff2);
				break;
			case 2:
				g_SMAPI->Format(sBuff2, sizeof(sBuff2), g_vecPhrases[std::string("HtmlButtonBlock")].c_str(), hMenu.hItems[l].sText.c_str());
				sBuff += std::string(sBuff2);
				break;
		}
		if(g_iMenuItem[iSlot] == iCount+1) {
			if(g_bMenuAddon) {
				if(bEnter) sBuff += g_vecPhrases[std::string("HtmlE_PButtons")];
				else sBuff += g_vecPhrases[std::string("HtmlEButtons")];
			} else {
				if(bEnter) g_SMAPI->Format(sBuff2, sizeof(sBuff2), g_vecPhrases[std::string("HtmlE_PButtons_Web")].c_str(), g_szMenuURL);
				else g_SMAPI->Format(sBuff2, sizeof(sBuff2), g_vecPhrases[std::string("HtmlEButtons_Web")].c_str(), g_szMenuURL);
				sBuff += sBuff2;
			}
		}
		sBuff += g_vecPhrases[std::string("HtmlButtonBR")];
		iCount++;
		if(iCount == 5 || l == hMenu.hItems.size()-1)
		{
			int iC = 5;
			if(hMenuPlayer.iList == 0 && !hMenu.bBack) iC++;

			if(g_bMenuAddon) {
				if(bUp) sBuff += g_vecPhrases[std::string("HtmlW_PButtons")];
				else sBuff += g_vecPhrases[std::string("HtmlWButtons")];
			} else {
				if(bUp) g_SMAPI->Format(sBuff2, sizeof(sBuff2), g_vecPhrases[std::string("HtmlW_PButtons_Web")].c_str(), g_szMenuURL);
				else g_SMAPI->Format(sBuff2, sizeof(sBuff2), g_vecPhrases[std::string("HtmlWButtons_Web")].c_str(), g_szMenuURL);
				sBuff += sBuff2;
			}

			if(g_bMenuAddon) {
				if(bDown) sBuff += g_vecPhrases[std::string("HtmlS_PButtons")];
				else sBuff += g_vecPhrases[std::string("HtmlSButtons")];
			} else {
				if(bDown) g_SMAPI->Format(sBuff2, sizeof(sBuff2), g_vecPhrases[std::string("HtmlS_PButtons_Web")].c_str(), g_szMenuURL);
				else g_SMAPI->Format(sBuff2, sizeof(sBuff2), g_vecPhrases[std::string("HtmlSButtons_Web")].c_str(), g_szMenuURL);
				sBuff += sBuff2;
			}

			if(hMenuPlayer.iList > 0 || hMenu.bBack) 
			{
				if(g_bMenuAddon) {
					if(bLeft) sBuff += g_vecPhrases[std::string("HtmlA_PButtons")];
					else sBuff += g_vecPhrases[std::string("HtmlAButtons")];
				} else {
					if(bLeft) g_SMAPI->Format(sBuff2, sizeof(sBuff2), g_vecPhrases[std::string("HtmlA_PButtons_Web")].c_str(), g_szMenuURL);
					else g_SMAPI->Format(sBuff2, sizeof(sBuff2), g_vecPhrases[std::string("HtmlAButtons_Web")].c_str(), g_szMenuURL);
					sBuff += sBuff2;
				}

				if(iItems <= hMenuPlayer.iList+1) {
					if(g_bMenuAddon) sBuff += g_vecPhrases[std::string("HtmlSpaceShortButtons")];
					else {
						g_SMAPI->Format(sBuff2, sizeof(sBuff2), g_vecPhrases[std::string("HtmlSpaceShortButtons_Web")].c_str(), g_szMenuURL);
						sBuff += sBuff2;
					}
				} else {
					if(g_bMenuAddon) {
						if(bRight) sBuff += g_vecPhrases[std::string("HtmlD_PButtons")];
						else sBuff += g_vecPhrases[std::string("HtmlDButtons")];
					} else {
						if(bRight) g_SMAPI->Format(sBuff2, sizeof(sBuff2), g_vecPhrases[std::string("HtmlD_PButtons_Web")].c_str(), g_szMenuURL);
						else g_SMAPI->Format(sBuff2, sizeof(sBuff2), g_vecPhrases[std::string("HtmlDButtons_Web")].c_str(), g_szMenuURL);
						sBuff += sBuff2;
					}
				}
			}
			else if(iItems > hMenuPlayer.iList+1) 
			{
				if(hMenuPlayer.iList == 0 || !hMenu.bBack) {
					if(g_bMenuAddon) sBuff += g_vecPhrases[std::string("HtmlSpaceShortButtons")];
					else {
						g_SMAPI->Format(sBuff2, sizeof(sBuff2), g_vecPhrases[std::string("HtmlSpaceShortButtons_Web")].c_str(), g_szMenuURL);
						sBuff += sBuff2;
					}
				}
				
				if(g_bMenuAddon) {
					if(bRight) sBuff += g_vecPhrases[std::string("HtmlD_PButtons")];
					else sBuff += g_vecPhrases[std::string("HtmlDButtons")];
				} else {
					if(bRight) g_SMAPI->Format(sBuff2, sizeof(sBuff2), g_vecPhrases[std::string("HtmlD_PButtons_Web")].c_str(), g_szMenuURL);
					else g_SMAPI->Format(sBuff2, sizeof(sBuff2), g_vecPhrases[std::string("HtmlDButtons_Web")].c_str(), g_szMenuURL);
					sBuff += sBuff2;
				}
			}
			else if(hMenu.bExit) {
				if(g_bMenuAddon) sBuff += g_vecPhrases[std::string("HtmlSpaceButtons")];
				else {
					g_SMAPI->Format(sBuff2, sizeof(sBuff2), g_vecPhrases[std::string("HtmlSpaceButtons_Web")].c_str(), g_szMenuURL);
					sBuff += sBuff2;
				}
			}
			if(hMenu.bExit) {
				if(g_bMenuAddon) sBuff += g_vecPhrases[std::string("HtmlFButtons")];
				else {
					g_SMAPI->Format(sBuff2, sizeof(sBuff2), g_vecPhrases[std::string("HtmlFButtons_Web")].c_str(), g_szMenuURL);
					sBuff += sBuff2;
				}
			}

			break;
		}
	}
	return sBuff;
}

int RoundToCeil(float value) {
	return static_cast<int>(ceil(value));
}

MenuType MenusApi::GetMenuType(int iSlot)
{
    if (iSlot < 0 || iSlot >= 64)
        return static_cast<MenuType>(g_iMenuTypeDefault);

    return static_cast<MenuType>(g_iMenuType[iSlot]);
}

static const char* const g_szLayoutName = "utils-dialog";
static const char* const g_szItemKinds[] = { "k-empty", "k-value", "k-submenu", "k-toggle", "k-select" };
static const int g_iItemsPerPage = 6;
static const int g_iMaxOptions = 8;

static int g_iNotifyGen[64] = {0};

static void UTIL_SetLayoutItemKind(int iSlot, const char* szItemId, const char* szKind)
{
	for(const char* k : g_szItemKinds)
	{
		g_pLayoutApi->SetHasClass(iSlot, g_szLayoutName, szItemId, k,
			(strcmp(k, szKind) == 0) ? true : false);
	}
}

static void UTIL_RenderLayoutItem(int iSlot, Menu& hMenu, MenuPlayer& hMenuPlayer, int i)
{
	char szItem[64];
	g_SMAPI->Format(szItem, sizeof(szItem), "utils-item-%i", i);
	int iIndex = hMenuPlayer.iList * g_iItemsPerPage + i;

	if(iIndex >= (int)hMenu.hItems.size())
	{
		UTIL_SetLayoutItemKind(iSlot, szItem, "k-empty");
		g_pLayoutApi->SetHasClass(iSlot, g_szLayoutName, szItem, "is-disabled", false);
		g_pLayoutApi->SetHasClass(iSlot, g_szLayoutName, szItem, "is-on", false);
		g_pLayoutApi->SetHasClass(iSlot, g_szLayoutName, szItem, "is-open", false);
		g_pLayoutApi->SetHasClass(iSlot, g_szLayoutName, szItem, "is-up", false);
		g_pLayoutApi->SetDialogVariable(iSlot, g_szLayoutName, szItem, "label", "");
		g_pLayoutApi->SetDialogVariable(iSlot, g_szLayoutName, szItem, "value", "");
		return;
	}

	const auto& item = hMenu.hItems[iIndex];
	g_pLayoutApi->SetDialogVariable(iSlot, g_szLayoutName, szItem, "label", item.sText.c_str());
	g_pLayoutApi->SetHasClass(iSlot, g_szLayoutName, szItem, "is-disabled",
		item.iType == 2 ? true : false);

	static ItemExtra kEmptyExtra;
	const ItemExtra& ex = (iIndex < (int)g_vItemExtra[iSlot].size())
		? g_vItemExtra[iSlot][iIndex] : kEmptyExtra;

	if(ex.iKind == ItemKind::TOGGLE)
	{
		UTIL_SetLayoutItemKind(iSlot, szItem, "k-toggle");
		g_pLayoutApi->SetHasClass(iSlot, g_szLayoutName, szItem, "is-on",
			ex.bToggled ? true : false);
		g_pLayoutApi->SetHasClass(iSlot, g_szLayoutName, szItem, "is-open", false);
		g_pLayoutApi->SetHasClass(iSlot, g_szLayoutName, szItem, "is-up", false);
	}
	else if(ex.iKind == ItemKind::SELECT)
	{
		UTIL_SetLayoutItemKind(iSlot, szItem, "k-select");
		int iSel = ex.iSelected;
		if(iSel < 0 || iSel >= (int)ex.vOptions.size()) iSel = 0;
		const char* szValue = ex.vOptions.empty() ? "" : ex.vOptions[iSel].sText.c_str();
		g_pLayoutApi->SetDialogVariable(iSlot, g_szLayoutName, szItem, "value", szValue);
		char szOpt[64];
		for(int j = 0; j < g_iMaxOptions; j++)
		{
			g_SMAPI->Format(szOpt, sizeof(szOpt), "utils-opt-%i-%i", i, j);
			if(j < (int)ex.vOptions.size())
			{
				g_pLayoutApi->SetDialogVariable(iSlot, g_szLayoutName, szOpt, "value", ex.vOptions[j].sText.c_str());
				g_pLayoutApi->SetHasClass(iSlot, g_szLayoutName, szOpt, "o-show", true);
				g_pLayoutApi->SetHasClass(iSlot, g_szLayoutName, szOpt, "utils-sel",
					(j == iSel) ? true : false);
			}
			else
			{
				g_pLayoutApi->SetHasClass(iSlot, g_szLayoutName, szOpt, "o-show", false);
				g_pLayoutApi->SetHasClass(iSlot, g_szLayoutName, szOpt, "utils-sel", false);
			}
		}
		g_pLayoutApi->SetHasClass(iSlot, g_szLayoutName, szItem, "is-up",
			(i >= 3) ? true : false);
		g_pLayoutApi->SetHasClass(iSlot, g_szLayoutName, szItem, "is-open",
			ex.bOpened ? true : false);
	}
	else
	{
		UTIL_SetLayoutItemKind(iSlot, szItem, "k-submenu");
		g_pLayoutApi->SetHasClass(iSlot, g_szLayoutName, szItem, "is-on", false);
		g_pLayoutApi->SetHasClass(iSlot, g_szLayoutName, szItem, "is-open", false);
		g_pLayoutApi->SetHasClass(iSlot, g_szLayoutName, szItem, "is-up", false);
	}
}

static void UTIL_RenderLayoutContent(int iSlot, Menu& hMenu, MenuPlayer& hMenuPlayer)
{
	g_pLayoutApi->SetDialogVariable(iSlot, g_szLayoutName, "utils-dialog", "title", hMenu.szTitle.c_str());
	g_pLayoutApi->SetDialogVariable(iSlot, g_szLayoutName, "utils-dialog", "desc",
		g_szMenuDesc[iSlot].size() > 0 ? g_szMenuDesc[iSlot].c_str() : "");

	int iPages = RoundToCeil(size(hMenu.hItems) / (double)g_iItemsPerPage);
	if(iPages < 1) iPages = 1;
	char szPage[64];
	g_SMAPI->Format(szPage, sizeof(szPage), "%i / %i", hMenuPlayer.iList + 1, iPages);
	g_pLayoutApi->SetDialogVariable(iSlot, g_szLayoutName, "utils-page", "page", szPage);
	g_pLayoutApi->SetHasClass(iSlot, g_szLayoutName, "utils-prev", "utils-disabled",
		hMenuPlayer.iList == 0 ? true : false);
	g_pLayoutApi->SetHasClass(iSlot, g_szLayoutName, "utils-next", "utils-disabled",
		iPages <= hMenuPlayer.iList + 1 ? true : false);

	g_pLayoutApi->SetHasClass(iSlot, g_szLayoutName, "utils-back-anchor", "utils-hide",
		hMenu.bBack ? false : true);

	for(int i = 0; i < g_iItemsPerPage; i++)
		UTIL_RenderLayoutItem(iSlot, hMenu, hMenuPlayer, i);
}

static void UTIL_CloseLayout(int iSlot, float flDelay)
{
	(void)flDelay;
	g_pLayoutApi->SetHasClass(iSlot, g_szLayoutName, "utils-dialog", "utils-dismissed", true);
	g_pLayoutApi->SetInputCapture(iSlot, g_szLayoutName, false);
}

static void UTIL_EnsureLayout(int iSlot)
{
	if(g_mapHudLayouts[iSlot].find(g_szLayoutName) == g_mapHudLayouts[iSlot].end())
		g_pLayoutApi->Create(iSlot, g_szLayoutName, "panorama/layout/custom_game/menu_ui.xml");
}

static void UTIL_DestroyLayout(int iSlot)
{
	g_pLayoutApi->Destroy(iSlot, g_szLayoutName, 0.0f);
}

void UtilsApi::ShowNotify(int iSlot, int iType, const char* szTitle, const char* szText, float flDuration, int iPos, const char* szChatFallback)
{
	if(iSlot < 0 || iSlot >= 64) return;
	if(!g_pLayoutApi) return;

	if(g_bAllowDisableNotify && g_bNotifyDisabled[iSlot])
	{
		if(szChatFallback && szChatFallback[0])
			g_pUtilsCore->PrintToChat(iSlot, "%s", szChatFallback);
		return;
	}

	if(!g_bPanoramaMenu)
	{
		if(szChatFallback && szChatFallback[0])
			g_pUtilsCore->PrintToChat(iSlot, "%s", szChatFallback);
		return;
	}

	UTIL_EnsureLayout(iSlot);

	static const char* const s_szTypeClass[] = { "ntf-success", "ntf-warning", "ntf-error" };
	static const char* const s_szTypeIcon[]  = { "✓", "!", "✕" };
	if(iType < 0 || iType > 2) iType = 0;

	for(int i = 0; i < 3; i++)
		g_pLayoutApi->SetHasClass(iSlot, g_szLayoutName, "utils-notify", s_szTypeClass[i],
			(i == iType) ? true : false);

	static const char* const s_szPosClass[] = {
		"ntf-pos-tr", "ntf-pos-tl", "ntf-pos-br", "ntf-pos-bl",
		"ntf-pos-tc", "ntf-pos-c", "ntf-pos-bc", "ntf-pos-cl", "ntf-pos-cr"
	};
	const int iPosCount = (int)(sizeof(s_szPosClass) / sizeof(s_szPosClass[0]));
	if(iPos < 0 || iPos >= iPosCount) iPos = 0;

	for(int i = 0; i < iPosCount; i++)
		g_pLayoutApi->SetHasClass(iSlot, g_szLayoutName, "utils-notify", s_szPosClass[i],
			(i == iPos) ? true : false);

	g_pLayoutApi->SetDialogVariable(iSlot, g_szLayoutName, "utils-notify", "ntficon", s_szTypeIcon[iType]);
	g_pLayoutApi->SetDialogVariable(iSlot, g_szLayoutName, "utils-notify", "ntftitle", szTitle ? szTitle : "");
	g_pLayoutApi->SetDialogVariable(iSlot, g_szLayoutName, "utils-notify", "ntftext", szText ? szText : "");

	g_pLayoutApi->SetHasClass(iSlot, g_szLayoutName, "utils-notify", "ntf-hidden", false);

	int iGen = ++g_iNotifyGen[iSlot];
	if(flDuration < 0.1f) flDuration = 0.1f;
	new CTimer(flDuration, [iSlot, iGen]() {
		if(g_iNotifyGen[iSlot] == iGen && g_pLayoutApi)
			g_pLayoutApi->SetHasClass(iSlot, g_szLayoutName, "utils-notify", "ntf-hidden", true);
		return -1.0f;
	});
}

void UtilsApi::ShowNotifyAll(int iType, const char* szTitle, const char* szText, float flDuration, int iPos, const char* szChatFallback)
{
	for(int i = 0; i < 64; i++)
	{
		CCSPlayerController* pController = CCSPlayerController::FromSlot(i);
		if(!pController || g_pPlayersApi->IsFakeClient(i)) continue;
		ShowNotify(i, iType, szTitle, szText, flDuration, iPos, szChatFallback);
	}
}


static void UTIL_HandleLayoutClick(int iSlot, const char* szLayoutName, const char* szButton)
{
	if(iSlot < 0 || iSlot >= 64) return;
	if(!szLayoutName || strcmp(szLayoutName, g_szLayoutName) != 0) return;
	auto& hMenuPlayer = g_MenuPlayer[iSlot];
	if(!hMenuPlayer.bEnabled) return;
	if(g_iMenuType[iSlot] != 3) return;
	auto& hMenu = hMenuPlayer.hMenu;
	hMenuPlayer.iEnd = std::time(0) + g_iMenuTime;

	if(strcmp(szButton, "utils-closeBtn") == 0)
	{
		if(hMenu.hFunc && hMenu.bExit) hMenu.hFunc("exit", "exit", 9, iSlot);
		UTIL_CloseLayout(iSlot, 0.2f);
		hMenuPlayer.clear();
		g_iMenuItem[iSlot] = 1;
		return;
	}

	if(strcmp(szButton, "utils-back") == 0)
	{
		if(hMenu.bBack && hMenu.hFunc) hMenu.hFunc("back", "back", 7, iSlot);
		return;
	}

	if(strcmp(szButton, "utils-prev") == 0)
	{
		if(hMenuPlayer.iList > 0)
		{
			hMenuPlayer.iList--;
			UTIL_RenderLayoutContent(iSlot, hMenu, hMenuPlayer);
		}
		return;
	}
	if(strcmp(szButton, "utils-next") == 0)
	{
		int iPages = RoundToCeil(size(hMenu.hItems) / (double)g_iItemsPerPage);
		if(iPages > hMenuPlayer.iList + 1)
		{
			hMenuPlayer.iList++;
			UTIL_RenderLayoutContent(iSlot, hMenu, hMenuPlayer);
			if(hMenu.hFunc) hMenu.hFunc("next", "next", 8, iSlot);
		}
		return;
	}

	int iVis = -1, iOpt = -1;
	if(sscanf(szButton, "utils-opt-%d-%d", &iVis, &iOpt) == 2 && iVis >= 0 && iVis < g_iItemsPerPage && iOpt >= 0)
	{
		int iIndex = hMenuPlayer.iList * g_iItemsPerPage + iVis;
		if(iIndex < (int)hMenu.hItems.size())
		{
			auto& item = hMenu.hItems[iIndex];
			if(iIndex >= (int)g_vItemExtra[iSlot].size()) return;
			ItemExtra& ex = g_vItemExtra[iSlot][iIndex];
			if(ex.iKind == ItemKind::SELECT && iOpt < (int)ex.vOptions.size())
			{
				ex.iSelected = iOpt;
				ex.bOpened = false;
				if(ex.hSelectFunc)
					ex.hSelectFunc(item.sBack.c_str(), ex.vOptions[iOpt].sBack.c_str(), iOpt, iVis + 1, iSlot);
				UTIL_RenderLayoutItem(iSlot, hMenu, hMenuPlayer, iVis);
			}
		}
		return;
	}

	if(sscanf(szButton, "utils-item-%d", &iVis) == 1 && iVis >= 0 && iVis < g_iItemsPerPage)
	{
		int iIndex = hMenuPlayer.iList * g_iItemsPerPage + iVis;
		if(iIndex >= (int)hMenu.hItems.size()) return;
		auto& item = hMenu.hItems[iIndex];
		if(item.iType == 2) return;
		if(iIndex >= (int)g_vItemExtra[iSlot].size()) return;
		ItemExtra& ex = g_vItemExtra[iSlot][iIndex];
		if(ex.iKind == ItemKind::TOGGLE)
		{
			ex.bToggled = !ex.bToggled;
			if(ex.hToggleFunc) ex.hToggleFunc(item.sBack.c_str(), ex.bToggled, iVis + 1, iSlot);
			UTIL_RenderLayoutItem(iSlot, hMenu, hMenuPlayer, iVis);
		}
		else if(ex.iKind == ItemKind::SELECT)
		{
			bool bOpenNow = !ex.bOpened;
			for(int k = 0; k < g_iItemsPerPage; k++)
			{
				int idx = hMenuPlayer.iList * g_iItemsPerPage + k;
				if(idx < (int)g_vItemExtra[iSlot].size() && g_vItemExtra[iSlot][idx].iKind == ItemKind::SELECT)
				{
					bool bWant = (k == iVis) ? bOpenNow : false;
					if(g_vItemExtra[iSlot][idx].bOpened != bWant)
					{
						g_vItemExtra[iSlot][idx].bOpened = bWant;
						UTIL_RenderLayoutItem(iSlot, hMenu, hMenuPlayer, k);
					}
				}
			}
		}
		else
		{
			if(item.iType == 1 && hMenu.hFunc)
				hMenu.hFunc(item.sBack.c_str(), item.sText.c_str(), iVis + 1, iSlot);
		}
		return;
	}
}

void MenusApi::DisplayPlayerMenu(Menu& hMenu, int iSlot, bool bClose = true)
{
	if(iSlot < 0 || iSlot >= 64) return;
	DisplayPlayerMenu(hMenu, iSlot, bClose, true);
}

void MenusApi::DisplayPlayerMenu(Menu& hMenu, int iSlot, bool bClose = true, bool bReset = true)
{
	if(iSlot < 0 || iSlot >= 64) return;
	
	g_iMenuType[iSlot] = UTIL_SanitizeMenuType(g_iMenuType[iSlot]);
    MenuPlayer& hMenuPlayer = g_MenuPlayer[iSlot];
	if (hMenuPlayer.bEnabled && bClose && bReset) {
		hMenuPlayer.clear();
	}
	if(!hMenuPlayer.bEnabled || !bReset)
	{
		g_iMenuItem[iSlot] = 1;
		hMenuPlayer.bEnabled = true;
		hMenuPlayer.hMenu = hMenu;
		{
			auto it = g_mapMenuDesc.find(&hMenu);
			g_szMenuDesc[iSlot] = (it != g_mapMenuDesc.end()) ? it->second : std::string();
		}
		{
			auto it = g_mapItemExtra.find(&hMenu);
			if(it != g_mapItemExtra.end()) g_vItemExtra[iSlot] = it->second;
			else g_vItemExtra[iSlot].clear();
			g_vItemExtra[iSlot].resize(hMenuPlayer.hMenu.hItems.size());
		}
		hMenuPlayer.iEnd = std::time(0) + g_iMenuTime;
		if(hMenuPlayer.iList > 0) {
			double dPerPage = (g_iMenuType[iSlot] == 3) ? (double)g_iItemsPerPage : 5.0;
			int iLists = RoundToCeil(size(hMenu.hItems) / dPerPage);
			while (iLists < hMenuPlayer.iList + 1) {
				hMenuPlayer.iList--;
			}
		}
		new CTimer(0.0f,[iSlot, &hMenu, &hMenuPlayer]() {
			if(!hMenuPlayer.bEnabled) return -1.0f;
			if(std::time(0) >= hMenuPlayer.iEnd)
			{
				hMenuPlayer.clear();
				if(g_iMenuType[iSlot] == 3) UTIL_CloseLayout(iSlot, 0.2f);
				if(g_iMenuType[iSlot] >= 2 && g_bStopingUser) {
					g_pPlayersApi->SetMoveType(iSlot, MOVETYPE_WALK);
				}
				return -1.0f;
			}
			if(g_iMenuType[iSlot] == 1 && g_TextMenuPlayer[iSlot].size() > 0) {
				g_pUtilsCore->PrintToCenterHtml(iSlot, 0.0f, g_TextMenuPlayer[iSlot].c_str());
			}
			if(g_iMenuType[iSlot] == 2) {
				std::string szMenu = GetMenuText(iSlot);
				if(szMenu.size() > 0) g_pUtilsCore->PrintToCenterHtml(iSlot, 0.0f, szMenu.c_str());
				else {
					hMenuPlayer.clear();
					if(g_iMenuType[iSlot] == 2 && g_bStopingUser) {
						g_pPlayersApi->SetMoveType(iSlot, MOVETYPE_WALK);
					}
					return -1.0f;
				}
			}
			return 0.0f;
		});
	}
	if(g_iMenuType[iSlot] != 3 && g_mapHudLayouts[iSlot].find(g_szLayoutName) != g_mapHudLayouts[iSlot].end())
	{
		g_pLayoutApi->SetHasClass(iSlot, g_szLayoutName, "utils-dialog", "utils-dismissed", true);
		g_pLayoutApi->SetInputCapture(iSlot, g_szLayoutName, false);
	}
	if(g_iMenuType[iSlot] == 0)
	{
		char sBuff[128] = "\0";
		int iCount = 0;
		int iItems = size(hMenu.hItems) / 5;
		if (size(hMenu.hItems) % 5 > 0) iItems++;
		g_pUtilsCore->PrintToChat(iSlot, hMenu.szTitle.c_str());
		for (size_t l = hMenuPlayer.iList*5; l < hMenu.hItems.size(); ++l) {
			switch (hMenu.hItems[l].iType)
			{
				case 1:
				case 2:
				{
					std::string cleanText = hMenu.hItems[l].sText;

					size_t startPos = 0;
					while ((startPos = cleanText.find('<', startPos)) != std::string::npos)
					{
						size_t endPos = cleanText.find('>', startPos);
						if (endPos != std::string::npos)
							cleanText.erase(startPos, endPos - startPos + 1);
						else
							break;
					}

					const char* colorCode = (hMenu.hItems[l].iType == 1) ? "\x04" : "\x08";
					g_SMAPI->Format(sBuff, sizeof(sBuff), " %s[!%i]\x01 %s", colorCode, iCount + 1, cleanText.c_str());
					g_pUtilsCore->PrintToChat(iSlot, sBuff);
					break;
				}
			}
			iCount++;
			if(iCount == 5 || l == hMenu.hItems.size()-1)
			{
				int iC = 5;
				if(hMenuPlayer.iList == 0 && !hMenu.bBack) iC++;
				if(l == hMenu.hItems.size()-1)
				{
					for (int i = 0; i < iC-iCount; i++)
					{
						g_pUtilsCore->PrintToChat(iSlot, " \x08-\x01");
					}
				}
				if(hMenuPlayer.iList > 0 || hMenu.bBack) g_pUtilsCore->PrintToChat(iSlot, g_vecPhrases[std::string("Back")].c_str());
				if(iItems > hMenuPlayer.iList+1) g_pUtilsCore->PrintToChat(iSlot, g_vecPhrases[std::string("Next")].c_str());
				g_pUtilsCore->PrintToChat(iSlot, g_vecPhrases[std::string("Exit")].c_str());
				break;
			}
		}
	}
	else if(g_iMenuType[iSlot] == 1)
	{
		std::string sBuff = "";
		char sBuff2[256];
		int iCount = 0;
		int iItems = size(hMenu.hItems) / 5;
		if (size(hMenu.hItems) % 5 > 0) iItems++;
		g_SMAPI->Format(sBuff2, sizeof(sBuff2), g_vecPhrases[std::string("HtmlTitle")].c_str(), hMenu.szTitle.c_str());
		sBuff += std::string(sBuff2);
		for (size_t l = hMenuPlayer.iList*5; l < hMenu.hItems.size(); ++l) {
			switch (hMenu.hItems[l].iType)
			{
				case 1:
					g_SMAPI->Format(sBuff2, sizeof(sBuff2), g_vecPhrases[std::string("HtmlNumber")].c_str(), iCount+1, hMenu.hItems[l].sText.c_str());
					sBuff += std::string(sBuff2);
					break;
				case 2:
					g_SMAPI->Format(sBuff2, sizeof(sBuff2), g_vecPhrases[std::string("HtmlNumberBlock")].c_str(), iCount+1, hMenu.hItems[l].sText.c_str());
					sBuff += std::string(sBuff2);
					break;
			}
			iCount++;
			if(iCount == 5 || l == hMenu.hItems.size()-1)
			{
				int iC = 5;
				if(hMenuPlayer.iList == 0 && !hMenu.bBack) iC++;
				if(hMenuPlayer.iList > 0 || hMenu.bBack) sBuff += g_vecPhrases[std::string("HtmlBack")];
				if(iItems > hMenuPlayer.iList+1) sBuff += g_vecPhrases[std::string("HtmlNext")];
				sBuff += g_vecPhrases[std::string("HtmlExit")];
				break;
			}
		}
		g_TextMenuPlayer[iSlot] = sBuff;
	}
	else if(g_iMenuType[iSlot] == 3)
	{
		UTIL_EnsureLayout(iSlot);

		UTIL_RenderLayoutContent(iSlot, hMenu, hMenuPlayer);

		g_pLayoutApi->SetHasClass(iSlot, "utils-dialog", "utils-dialog", "utils-dismissed", false);
		g_pLayoutApi->SetInputCapture(iSlot, "utils-dialog", true);
	}
}

std::string MenusApi::escapeString(const std::string& input) {
    std::string escaped;
    for (char c : input) {
        switch (c) {
            case '\\': escaped += "\\\\"; break;
            case '\"': escaped += "\\\""; break;
            case '\n': escaped += "\\n"; break;
            case '\r': escaped += "\\r"; break;
            case '\t': escaped += "\\t"; break;
            case '<': escaped += "&lt;"; break;
            case '>': escaped += "&gt;"; break;
			case '%': escaped += "%%"; break;
            default: escaped += c; break;
        }
    }
    return escaped;
}

void MenusApi::AddItemMenu(Menu& hMenu, const char* sBack, const char* sText, int iType = 1)
{
    if (iType == 0) return;

	Items hItem;
	hItem.iType = iType;
	hItem.sBack = std::string(sBack);
	hItem.sText = escapeString(sText);
	hMenu.hItems.push_back(hItem);
	
	UTIL_PushItemExtra(hMenu);
}

void MenusApi::AddRawItemMenu(Menu &hMenu, const char* sBack, const char* sText, int iType = 1)
{
    if (iType == 0) return;

    Items hItem;
    hItem.iType = iType;
    hItem.sBack = std::string(sBack);
    hItem.sText = std::string(sText);
    hMenu.hItems.push_back(hItem);
	UTIL_PushItemExtra(hMenu);
}

void MenusApi::AddToggleMenu(Menu& hMenu, const char* sBack, const char* sText, bool bDefault = false, MenuToggleCallbackFunc func = nullptr, int iType = 1)
{
	if (iType == 0) return;

	Items hItem;
	hItem.iType = iType;
	hItem.sBack = std::string(sBack);
	hItem.sText = escapeString(sText);
	hMenu.hItems.push_back(hItem);
	ItemExtra& ex = UTIL_PushItemExtra(hMenu);
	ex.iKind = ItemKind::TOGGLE;
	ex.bToggled = bDefault;
	ex.hToggleFunc = func;
}

ItemRef MenusApi::AddSelectMenu(Menu& hMenu, const char* sBack, const char* sText, int iDefault = 0, MenuSelectCallbackFunc func = nullptr, int iType = 1)
{
	Items hItem;
	hItem.iType = (iType == 0) ? 1 : iType;
	hItem.sBack = std::string(sBack);
	hItem.sText = escapeString(sText);
	hMenu.hItems.push_back(hItem);
	ItemExtra& ex = UTIL_PushItemExtra(hMenu);
	ex.iKind = ItemKind::SELECT;
	ex.iSelected = iDefault < 0 ? 0 : iDefault;
	ex.bOpened = false;
	ex.hSelectFunc = func;
	return ItemRef{ &hMenu, (int)hMenu.hItems.size() - 1 };
}

void MenusApi::AddSelectOption(ItemRef hItem, const char* sOptionBack, const char* sOptionText)
{
	ItemExtra* ex = UTIL_ResolveItemExtra(hItem);
	if (!ex) return;
	SelectOption hOption;
	hOption.sBack = std::string(sOptionBack);
	hOption.sText = escapeString(sOptionText);
	ex->vOptions.push_back(hOption);
}

void MenusApi::SetToggleState(ItemRef hItem, bool bState)
{
	ItemExtra* ex = UTIL_ResolveItemExtra(hItem);
	if (ex) ex->bToggled = bState;
}

bool MenusApi::GetToggleState(ItemRef hItem)
{
	ItemExtra* ex = UTIL_ResolveItemExtra(hItem);
	return ex ? ex->bToggled : false;
}

void MenusApi::SetSelectedOption(ItemRef hItem, int iOption)
{
	ItemExtra* ex = UTIL_ResolveItemExtra(hItem);
	if (!ex) return;
	if (ex->vOptions.empty()) { ex->iSelected = 0; return; }
	if (iOption < 0) iOption = 0;
	if (iOption >= (int)ex->vOptions.size()) iOption = (int)ex->vOptions.size() - 1;
	ex->iSelected = iOption;
}

int MenusApi::GetSelectedOption(ItemRef hItem)
{
	ItemExtra* ex = UTIL_ResolveItemExtra(hItem);
	return ex ? ex->iSelected : 0;
}

void MenusApi::ClosePlayerMenu(int iSlot)
{
	if(iSlot < 0 || iSlot >= 64) return;
	if(g_iMenuType[iSlot] == 3) UTIL_CloseLayout(iSlot, 0.2f);
	if(g_iMenuType[iSlot] >= 2 && g_bStopingUser) {
		g_pPlayersApi->SetMoveType(iSlot, MOVETYPE_WALK);
	}
	g_MenuPlayer[iSlot].clear();
	g_TextMenuPlayer[iSlot] = "";
	g_iMenuItem[iSlot] = 1;
}

void ClientPrintFilter(CPlayerBitVec filter, int msg_dest, const char *msg_name, const char *param1, const char *param2, const char *param3, const char *param4)
{
	static INetworkMessageInternal *netmsg = g_pNetworkMessages->FindNetworkMessagePartial("CUserMessageTextMsg");
	std::unique_ptr<CUserMessageTextMsg_t> msg(netmsg->AllocateMessage()->As<CUserMessageTextMsg_t>());
	msg->set_dest(msg_dest);
	msg->add_param(msg_name);
	msg->add_param(param1);
	msg->add_param(param2);
	msg->add_param(param3);
	msg->add_param(param4);
	
	g_gameEventSystem->PostEventAbstract(-1, false, ABSOLUTE_PLAYER_LIMIT, reinterpret_cast<const uint64*>(filter.Base()), netmsg, msg.get(), 0, NetChannelBufType_t::BUF_RELIABLE);
	// msg->Send(filter);
    // delete msg;
}

void UtilsApi::PrintToChatAll(const char *msg, ...)
{
	va_list args;
	va_start(args, msg);

	char buf[512];
	V_vsnprintf(buf, sizeof(buf), msg, args);
	va_end(args);

	std::string colorizedBuf = Colorizer(buf);

	CPlayerBitVec filter;
	for (int i = 0; i < 64; i++) {
		if (g_pPlayersApi->IsFakeClient(i)) continue;
		CCSPlayerController* pPlayerController = CCSPlayerController::FromSlot(i);
		if (pPlayerController && pPlayerController->m_steamID() > 0) {
			filter.Set(i);
		}
	}
	ClientPrintFilter(filter, HUD_PRINTTALK, colorizedBuf.c_str(), "", "", "", "");
}

void UtilsApi::PrintToChat(int iSlot, const char *msg, ...)
{
	va_list args;
	va_start(args, msg);

	char buf[512];
	V_vsnprintf(buf, sizeof(buf), msg, args);
	va_end(args);

	CCSPlayerController* pPlayerController = CCSPlayerController::FromSlot(iSlot);
	if (!pPlayerController || pPlayerController->m_steamID() <= 0)
		return;

	std::string colorizedBuf = Colorizer(buf);

	g_pUtilsApi->NextFrame([iSlot, pPlayerController, colorizedBuf](){
		if(pPlayerController->m_hPawn() && pPlayerController->m_steamID() > 0)
		{
			CPlayerBitVec filter;
			filter.Set(iSlot);
			ClientPrintFilter(filter, HUD_PRINTTALK, colorizedBuf.c_str(), "", "", "", "");
		}
	});
}

void UtilsApi::PrintToConsole(int iSlot, const char *msg, ...)
{
	va_list args;
	va_start(args, msg);

	char buf[512];
	V_vsnprintf(buf, sizeof(buf), msg, args);
	va_end(args);

	if(iSlot < 0 || iSlot >= 64) {
		META_CONPRINT(buf);
		return;
	}

	CPlayerBitVec filter;
	filter.Set(iSlot);
	ClientPrintFilter(filter, HUD_PRINTCONSOLE, buf, "", "", "", "");
}

void UtilsApi::PrintToConsoleAll(const char *msg, ...)
{
	va_list args;
	va_start(args, msg);

	char buf[512];
	V_vsnprintf(buf, sizeof(buf), msg, args);
	va_end(args);

	CPlayerBitVec filter;
	for (int i = 0; i < 64; i++) {
		if (g_pPlayersApi->IsFakeClient(i)) continue;
		CCSPlayerController* pPlayerController = CCSPlayerController::FromSlot(i);
		if (pPlayerController && pPlayerController->m_steamID() > 0) {
			filter.Set(i);
		}
	}
	ClientPrintFilter(filter, HUD_PRINTCONSOLE, buf, "", "", "", "");
}

void UtilsApi::PrintToCenter(int iSlot, const char *msg, ...)
{
	va_list args;
	va_start(args, msg);

	char buf[512];
	V_vsnprintf(buf, sizeof(buf), msg, args);
	va_end(args);

	CCSPlayerController* pPlayerController = CCSPlayerController::FromSlot(iSlot);
	if (!pPlayerController || pPlayerController->m_steamID() <= 0)
		return;

	CPlayerBitVec filter;
	filter.Set(iSlot);
	ClientPrintFilter(filter, HUD_PRINTCENTER, buf, "", "", "", "");
}

void UtilsApi::PrintToCenterAll(const char *msg, ...)
{
	va_list args;
	va_start(args, msg);

	char buf[512];
	V_vsnprintf(buf, sizeof(buf), msg, args);
	va_end(args);

	CPlayerBitVec filter;
	for (int i = 0; i < 64; i++) {
		if (g_pPlayersApi->IsFakeClient(i)) continue;
		CCSPlayerController* pPlayerController = CCSPlayerController::FromSlot(i);
		if (pPlayerController && pPlayerController->m_steamID() > 0) {
			filter.Set(i);
		}
	}
	ClientPrintFilter(filter, HUD_PRINTCENTER, buf, "", "", "", "");
}

void UtilsApi::PrintToAlert(int iSlot, const char *msg, ...)
{
	va_list args;
	va_start(args, msg);

	char buf[512];
	V_vsnprintf(buf, sizeof(buf), msg, args);
	va_end(args);

	CCSPlayerController* pPlayerController = CCSPlayerController::FromSlot(iSlot);
	if (!pPlayerController || pPlayerController->m_steamID() <= 0)
		return;

	CPlayerBitVec filter;
	filter.Set(iSlot);
	ClientPrintFilter(filter, HUD_PRINTALERT, buf, "", "", "", "");
}

void UtilsApi::PrintToAlertAll(const char *msg, ...)
{
	va_list args;
	va_start(args, msg);

	char buf[512];
	V_vsnprintf(buf, sizeof(buf), msg, args);
	va_end(args);

	CPlayerBitVec filter;
	for (int i = 0; i < 64; i++) {
		if (g_pPlayersApi->IsFakeClient(i)) continue;
		CCSPlayerController* pPlayerController = CCSPlayerController::FromSlot(i);
		if (pPlayerController && pPlayerController->m_steamID() > 0) {
			filter.Set(i);
		}
	}
	ClientPrintFilter(filter, HUD_PRINTALERT, buf, "", "", "", "");
}

void UtilsApi::PrintToCenterHtml(int iSlot, int iDuration, const char *msg, ...)
{
	va_list args;
	va_start(args, msg);

	char buf[8192];
	V_vsnprintf(buf, sizeof(buf), msg, args);
	va_end(args);

	CCSPlayerController* pPlayerController = CCSPlayerController::FromSlot(iSlot);
	if (!pPlayerController || pPlayerController->m_steamID() <= 0) return;
	int iEnd = std::time(0) + iDuration;
	if(UTIL_GetLegacyGameEventListener)
	{
		IGameEvent* pEvent = gameeventmanager->CreateEvent("show_survival_respawn_status");
		if(!pEvent) return;
		pEvent->SetString("loc_token", buf);
		pEvent->SetInt("userid", iSlot);
		pEvent->SetInt("duration", iDuration>0?iDuration:5);
		IGameEventListener2* pListener = UTIL_GetLegacyGameEventListener(CPlayerSlot(iSlot));
		if(pListener)
		{
			pListener->FireGameEvent(pEvent);
			gameeventmanager->FreeEvent(pEvent);
		}
	}
	else
	{
		new CTimer(0.f, [iEnd, buf, iSlot]()
		{
			IGameEvent* pEvent = gameeventmanager->CreateEvent("show_survival_respawn_status");
			if(!pEvent) return -1.0f;
			pEvent->SetString("loc_token", buf);
			pEvent->SetInt("duration", 5);
			pEvent->SetInt("userid", iSlot);
			gameeventmanager->FireEvent(pEvent);
			if((iEnd - std::time(0)) > 0)
				return 0.f;
			// gameeventmanager->FreeEvent(pEvent);
			return -1.0f;
		});
	}
}

void UtilsApi::PrintToCenterHtmlAll(int iDuration, const char *msg, ...)
{
	va_list args;
	va_start(args, msg);

	char buf[2048];
	V_vsnprintf(buf, sizeof(buf), msg, args);
	va_end(args);

	int iEnd = std::time(0) + iDuration;
	IGameEvent* pEvent = gameeventmanager->CreateEvent("show_survival_respawn_status");
	pEvent->SetString("loc_token", buf);
	pEvent->SetInt("userid", -1);
	if(UTIL_GetLegacyGameEventListener)
	{
		pEvent->SetInt("duration", iDuration);
		for(int i = 0; i < 64; i++)
		{
			if(!m_Players[i] || m_Players[i]->IsFakeClient()) continue;
			IGameEventListener2* pListener = UTIL_GetLegacyGameEventListener(CPlayerSlot(i));
			if(pListener)
			{
				pListener->FireGameEvent(pEvent);
			}
		}
		gameeventmanager->FreeEvent(pEvent);
	}
	else
	{
		pEvent->SetInt("duration", 5);
		new CTimer(0.f, [iEnd, pEvent]()
		{
			gameeventmanager->FireEvent(pEvent);
			if((iEnd - std::time(0)) > 0)
				return 0.f;
			gameeventmanager->FreeEvent(pEvent);
			return -1.0f;
		});
	}
}

void UtilsApi::SetEntityModel(CBaseModelEntity* pEntity, const char* szModel)
{
	if(pEntity && UTIL_SetModel)
	{
		UTIL_SetModel(pEntity, szModel);
	}
}

void UtilsApi::DispatchSpawn(CEntityInstance* pEntity, CEntityKeyValues* pKeyValues)
{
	if(pEntity && UTIL_DispatchSpawn)
	{
		UTIL_DispatchSpawn(pEntity, pKeyValues);
	}
}

CBaseEntity* UtilsApi::CreateEntityByName(const char* pClassName, CEntityIndex iForceEdictIndex)
{
	return UTIL_CreateEntity?UTIL_CreateEntity(pClassName, iForceEdictIndex):nullptr;
}

void UtilsApi::RemoveEntity(CEntityInstance* pEntity)
{
	if(pEntity && UTIL_Remove)
	{
		UTIL_Remove(pEntity);
	}
}

void UtilsApi::AcceptEntityInput(CEntityInstance* pEntity, const char* szInputName, variant_t value, CEntityInstance *pActivator, CEntityInstance *pCaller)
{
	if(UTIL_AcceptInput)
    	UTIL_AcceptInput(pEntity, szInputName, pActivator, pCaller, value);
}

void UtilsApi::NextFrame(std::function<void()> fn)
{
	m_nextFrame.push_back(fn);
}

CCSGameRules* UtilsApi::GetCCSGameRules()
{
	return g_pGameRules;
}

CGameEntitySystem* UtilsApi::GetCGameEntitySystem()
{
	return g_pGameEntitySystem;
}

CEntitySystem* UtilsApi::GetCEntitySystem()
{
	return g_pEntitySystem;
}

CGlobalVars* UtilsApi::GetCGlobalVars()
{
	return gpGlobals;
}

IGameEventManager2* UtilsApi::GetGameEventManager()
{
	return gameeventmanager;
}

const char* UtilsApi::GetLanguage()
{
	return szLanguage;
}

//Thank komaschenko for help
void ChainNetworkStateChanged(uintptr_t networkVarChainer, uint32 nLocalOffset, int32 nArrayIndex = -1)
{
    CEntityInstance* pEntity = *reinterpret_cast<CEntityInstance**>(networkVarChainer);
    if (pEntity && (pEntity->m_pEntity->m_flags & EF_IS_CONSTRUCTION_IN_PROGRESS) == 0)
	{
		pEntity->NetworkStateChanged({nLocalOffset, nArrayIndex, *reinterpret_cast<ChangeAccessorFieldPathIndex_t*>(networkVarChainer + 32)});
    }
}

void UtilsApi::SetStateChanged(CBaseEntity* pEntity, const char* sClassName, const char* sFieldName, int extraOffset = 0)
{
	if(pEntity)
	{
		int offset, chainOffset;
		if(g_Offsets[sClassName][sFieldName] == 0 || g_ChainOffsets[sClassName][sFieldName] == 0)
		{
			offset = schema::GetServerOffset(sClassName, sFieldName);
			g_Offsets[sClassName][sFieldName] = offset;
			chainOffset = schema::FindChainOffset(sClassName);
			g_ChainOffsets[sClassName][sFieldName] = chainOffset;
		}
		else
		{
			offset = g_Offsets[sClassName][sFieldName];
			chainOffset = g_ChainOffsets[sClassName][sFieldName];
		}
		if (chainOffset != 0)
		{
			ChainNetworkStateChanged((uintptr_t)(pEntity) + chainOffset, offset + extraOffset, 0xFFFFFFFF);
			return;
		}
		const auto entity = static_cast<CEntityInstance*>(pEntity);
		entity->NetworkStateChanged(offset + extraOffset);
	}
}

std::string formatCurrentTime() {
    std::time_t currentTime = std::time(nullptr);
    std::tm* localTime = std::localtime(&currentTime);
    std::ostringstream formattedTime;
    formattedTime << std::put_time(localTime, "%m/%d/%Y - %H:%M:%S");
    return formattedTime.str();
}

std::string formatCurrentTime2() {
    std::time_t currentTime = std::time(nullptr);
    std::tm* localTime = std::localtime(&currentTime);
    std::ostringstream formattedTime;
    formattedTime << std::put_time(localTime, "error_%m-%d-%Y");
    return formattedTime.str();
}

void UtilsApi::LogToFile(const char* filename, const char* msg, ...)
{
	va_list args;
	va_start(args, msg);

	char buf[1024];
	V_vsnprintf(buf, sizeof(buf), msg, args);
	va_end(args);

	char szPath[256], szBuffer[2048];
	g_SMAPI->PathFormat(szPath, sizeof(szPath), "%s/addons/logs/%s.txt", g_SMAPI->GetBaseDir(), filename);
	g_SMAPI->Format(szBuffer, sizeof(szBuffer), "L %s: %s\n", formatCurrentTime().c_str(), buf);
	Msg("%s\n", szBuffer);
	FILE* pFile = fopen(szPath, "a");
	if (pFile)
	{
		fputs(szBuffer, pFile);
		fclose(pFile);
	}
}

void UtilsApi::ErrorLog(const char* msg, ...)
{
	va_list args;
	va_start(args, msg);

	char buf[1024];
	V_vsnprintf(buf, sizeof(buf), msg, args);
	va_end(args);

	ConColorMsg(Color(255, 0, 0, 255), "[Error] %s\n", buf);

	char szPath[256], szBuffer[2048];
	g_SMAPI->PathFormat(szPath, sizeof(szPath), "%s/addons/logs/%s.txt", g_SMAPI->GetBaseDir(), formatCurrentTime2().c_str());
	g_SMAPI->Format(szBuffer, sizeof(szBuffer), "L %s: %s\n", formatCurrentTime().c_str(), buf);

	FILE* pFile = fopen(szPath, "a");
	if (pFile)
	{
		fputs(szBuffer, pFile);
		fclose(pFile);
	}
}

CTimer* UtilsApi::CreateTimer(float flInterval, std::function<float()> func)
{
	return new CTimer(flInterval, func);
}

void UtilsApi::RemoveTimer(CTimer* pTimer)
{
	if(pTimer)
	{
		pTimer->RemoveTimer();
	}
}

void PlayersApi::CommitSuicide(int iSlot, bool bExplode, bool bForce)
{
	if(!g_iCommitSuicide) return;
	CCSPlayerController* pController = CCSPlayerController::FromSlot(iSlot);
	if(!pController) return;
	CBasePlayerPawn* pPawn = pController->GetPlayerPawn();
	if(!pPawn) return;
	CALL_VIRTUAL(void, g_iCommitSuicide, pPawn, bExplode, bForce);
}

void PlayersApi::ChangeTeam(int iSlot, int iNewTeam)
{
	if(!g_iChangeTeam) return;
	CCSPlayerController* pController = CCSPlayerController::FromSlot(iSlot);
	if(!pController) return;
	CALL_VIRTUAL(void, g_iChangeTeam, pController, iNewTeam);
}

void PlayersApi::Teleport(int iSlot, const Vector *position, const QAngle *angles, const Vector *velocity)
{
	if(!g_iTeleport) return;
	CCSPlayerController* pController = CCSPlayerController::FromSlot(iSlot);
	if(!pController) return;
	CCSPlayerPawn* pPawn = pController->GetPlayerPawn();
	if(!pPawn) return;
	CALL_VIRTUAL(void, g_iTeleport, pPawn, position, angles, velocity);
}

void UtilsApi::TeleportEntity(CBaseEntity* pEnt, const Vector *position, const QAngle *angles, const Vector *velocity)
{
	if(!g_iTeleport) return;
	CALL_VIRTUAL(void, g_iTeleport, pEnt, position, angles, velocity);
}

void UtilsApi::CollisionRulesChanged(CBaseEntity* pEnt)
{
	if(!g_iCollisionRulesChanged) return;
	CALL_VIRTUAL(void, g_iCollisionRulesChanged, pEnt);
}

void PlayersApi::Respawn(int iSlot)
{
	if(!g_iRespawn || !UTIL_RespawnPlayer) return;
	CCSPlayerController* pController =  CCSPlayerController::FromSlot(iSlot);
	if(!pController) return;
	CCSPlayerPawn* pawn = pController->GetPlayerPawn();
	if(!pawn || pawn->IsAlive()) return;
	UTIL_RespawnPlayer(pController, pawn, true, false, false, false);
	CALL_VIRTUAL(void, g_iRespawn, pController);
}

void PlayersApi::DropWeapon(int iSlot, CBaseEntity* pWeapon, Vector* pVecTarget, Vector* pVelocity)
{
	if(!g_iDropWeapon) return;
	CCSPlayerController* pController = CCSPlayerController::FromSlot(iSlot);
	if(!pController) return;
	CCSPlayerPawn* pPawn = pController->GetPlayerPawn();
	if(!pPawn) return;
	CCSPlayer_WeaponServices* m_pWeaponServices = pPawn->m_pWeaponServices();
	if(!m_pWeaponServices) return;
	CALL_VIRTUAL(void, g_iDropWeapon, m_pWeaponServices, (CBasePlayerWeapon*)pWeapon, pVecTarget, pVelocity);
}

void PlayersApi::SwitchTeam(int iSlot, int iNewTeam)
{
	if(!UTIL_SwitchTeam) return;
	CCSPlayerController* pController = CCSPlayerController::FromSlot(iSlot);
	if(!pController) return;
	UTIL_SwitchTeam(pController, iNewTeam);
}

const char* PlayersApi::GetPlayerName(int iSlot)
{
	CCSPlayerController* pController = CCSPlayerController::FromSlot(iSlot);
	if(!pController) return "";
	return pController->m_iszPlayerName();
}

void PlayersApi::SetPlayerName(int iSlot, const char* szName)
{
	CCSPlayerController* pController = CCSPlayerController::FromSlot(iSlot);
	if(!pController) return;
	g_SMAPI->Format(pController->m_iszPlayerName(), 128, "%s", szName);
	g_pUtilsApi->SetStateChanged(pController, "CBasePlayerController", "m_iszPlayerName");
}

void PlayersApi::SetMoveType(int iSlot, MoveType_t moveType)
{
	CCSPlayerController* pController = CCSPlayerController::FromSlot(iSlot);
	if(!pController) return;
	CCSPlayerPawn* pPawn = pController->GetPlayerPawn();
	if(!pPawn) return;
	if(!UTIL_SetMoveType) pPawn->SetMoveType(moveType);
	else UTIL_SetMoveType(pPawn, moveType, pPawn->m_MoveCollide());
}

void PlayersApi::EmitSound(std::vector<int> vPlayers, CEntityIndex ent, std::string sound_name, int pitch, float volume)
{
    if(UTIL_EmitSoundFilter)
    {
		uint8_t unk[32];
        EmitSound_t params;
            params.m_pSoundName = sound_name.c_str();
            params.m_flVolume = volume;
            params.m_nPitch = pitch;
		CRecipientFilter filter;
		for(auto i : vPlayers) {
			filter.AddRecipient(i);
		}
		UTIL_EmitSoundFilter(unk, filter, ent, params);
    }
}

void PlayersApi::EmitSound(int iSlot, CEntityIndex ent, std::string sound_name, int pitch, float volume)
{
	if(UTIL_EmitSoundFilter)
	{
		uint8_t unk[32];
		EmitSound_t params;
			params.m_pSoundName = sound_name.c_str();
			params.m_flVolume = volume;
			params.m_nPitch = pitch;
		CSingleRecipientFilter filter(iSlot);
		UTIL_EmitSoundFilter(unk, filter, ent, params);
	}
}

void PlayersApi::StopSoundEvent(int iSlot, const char* sound_name)
{
	if(UTIL_StopSoundEvent)
	{
		CCSPlayerController* pController = CCSPlayerController::FromSlot(iSlot);
		if(!pController) return;
		CCSPlayerPawn* pPawn = pController->GetPlayerPawn();
		if(!pPawn) return;
		UTIL_StopSoundEvent(pPawn, sound_name);
	}
}

int PlayersApi::FindPlayer(uint64 iSteamID64)
{
	int iSlot = -1;
	for(int i = 0; i < 64; i++)
	{
		if(m_Players[i] && m_Players[i]->GetSteamId64() == iSteamID64)
		{
			iSlot = i;
			break;
		}
	}
	return iSlot;
}

int PlayersApi::FindPlayer(const CSteamID* steamID)
{
	int iSlot = -1;
	for(int i = 0; i < 64; i++)
	{
		if(m_Players[i] && m_Players[i]->GetSteamId() == steamID)
		{
			iSlot = i;
			break;
		}
	}
	return iSlot;
}

std::string ToLowerCase(const std::string& str)
{
	std::string lowerStr = str;
	std::transform(lowerStr.begin(), lowerStr.end(), lowerStr.begin(), ::tolower);
	return lowerStr;
}

int PlayersApi::FindPlayer(const char* szName)
{
	int iSlot = -1;
	for(int i = 0; i < 64; i++)
	{
		if(ToLowerCase(engine->GetClientConVarValue(i, "name")) == ToLowerCase(szName))
		{
			iSlot = i;
			break;
		}
	}
	return iSlot;
}

void PlayersApi::SetConVars(std::vector<int> vPlayers, std::vector<FakeConVar> cvars)
{
	static INetworkMessageInternal* netmsg = g_pNetworkMessages->FindNetworkMessagePartial("CNETMsg_SetConVar");
	std::unique_ptr<CNETMsg_SetConVar_t> msg(netmsg->AllocateMessage()->As<CNETMsg_SetConVar_t>());
	
	for (const auto& cvar : cvars) {
		CMsg_CVars_CVar *cvarEntry = msg->mutable_convars()->add_cvars();
		cvarEntry->set_name(cvar.szCvar.c_str());
		cvarEntry->set_value(cvar.szValue.c_str());
	}

	CPlayerBitVec recipients;
	for (auto i : vPlayers) {
		recipients.Set(i);
	}
	
	g_gameEventSystem->PostEventAbstract(-1, false, ABSOLUTE_PLAYER_LIMIT, reinterpret_cast<const uint64*>(recipients.Base()), netmsg, msg.get(), 0, NetChannelBufType_t::BUF_RELIABLE);
	// msg->Send(recipients);
	// delete msg;
}

void PlayersApi::SetConVar(std::vector<int> vPlayers, const char* name, const char* value)
{
	static INetworkMessageInternal* netmsg = g_pNetworkMessages->FindNetworkMessagePartial("CNETMsg_SetConVar");
	std::unique_ptr<CNETMsg_SetConVar_t> msg(netmsg->AllocateMessage()->As<CNETMsg_SetConVar_t>());
	
	CMsg_CVars_CVar *cvar = msg->mutable_convars()->add_cvars();
	cvar->set_name(name);
	cvar->set_value(value);

	CPlayerBitVec recipients;
	for (auto i : vPlayers) {
		recipients.Set(i);
	}
	
	g_gameEventSystem->PostEventAbstract(-1, false, ABSOLUTE_PLAYER_LIMIT, reinterpret_cast<const uint64*>(recipients.Base()), netmsg, msg.get(), 0, NetChannelBufType_t::BUF_RELIABLE);
	// msg->Send(recipients);
	// delete msg;
}

void PlayersApi::RemoveWeapons(int iSlot)
{
	if(!g_iRemoveWeapons) return;
	CCSPlayerController* pController = CCSPlayerController::FromSlot(iSlot);
	if (!pController) return;
	CCSPlayerPawn* pPlayerPawn = pController->GetPlayerPawn();
	if (!pPlayerPawn && !pPlayerPawn->IsAlive()) return;
	CCSPlayer_ItemServices* pItemServices = pPlayerPawn->m_pItemServices();
	if (!pItemServices) return;
	CALL_VIRTUAL(void, g_iRemoveWeapons, pItemServices);
}

void PlayersApi::TakeDamage(int iSlot, CTakeDamageInfo* pInfo, bool bHook)
{
	if(!UTIL_TakeDamage) return;
	CCSPlayerController* pController = CCSPlayerController::FromSlot(iSlot);
	if(!pController) return;
	CCSPlayerPawn* pPawn = pController->GetPlayerPawn();
	if(!pPawn) return;
	CCSPlayer_DamageReactServices* pDamageServices = pPawn->m_pDamageReactServices();
	if(!bHook)
	{
		UTIL_TakeDamage(pDamageServices, pInfo);
		return;
	}
	Hook_TakeDamage(pDamageServices, pInfo);
}

bool PlayersApi::UseClientCommand(int iSlot, const char* szCommand)
{
	if (iSlot == -1) return false;
	if (iSlot < 0 || iSlot >= 64) return false;
	auto tokens = SplitStringBySpace(szCommand);
	std::string sCommand = "";
	for (size_t i = 1; i < tokens.size(); ++i) {
		sCommand += tokens[i] + " ";
	}
	bool bFound = false;
	CommandCallback fn = nullptr;
	for(auto& item : ConsoleCommands)
	{
		if(item.second[std::string(tokens[0])])
		{
			bFound = true;
			fn = item.second[std::string(tokens[0])];
		}
	}
	for(auto& item : ChatCommands)
	{
		if(item.second[std::string(tokens[0])])
		{
			bFound = true;
			fn = item.second[std::string(tokens[0])];
		}
	}
	if(bFound && fn)
	{
		fn(iSlot, sCommand.c_str());
		return true;
	}
	return false;
}

trace_info_t PlayersApi::RayTrace(int iSlot)
{
	if(!UTIL_TraceShape) {
		g_pUtilsApi->ErrorLog("[%s] Failed to find function to get UTIL_TraceShape", g_PLAPI->GetLogTag());
		return trace_info_t();
	}
	if(!g_pGameTraceManager) {
		g_pUtilsApi->ErrorLog("[%s] Failed to find g_pGameTraceManager", g_PLAPI->GetLogTag());
		return trace_info_t();
	}
	CCSPlayerController* pController = CCSPlayerController::FromSlot(iSlot);
	if (!pController) return trace_info_t();
	CCSPlayerPawn* pPawn = pController->GetPlayerPawn();
	if (!pPawn) return trace_info_t();
	Vector vecStart = pPawn->GetEyePosition();
	QAngle angAbsAngles = pPawn->m_angEyeAngles();
	
	Vector vecForward;
	AngleVectors(angAbsAngles, &vecForward);
	Vector vecEnd = vecStart + vecForward * 16384.0f;

	Ray_t ray;

	trace_t trace;
	CTraceFilter filter;
	filter.m_nInteractsWith = 0x1C300B;
	filter.m_nObjectSetMask = 7;
	filter.m_nCollisionGroup = 3;
	filter.SetPassEntity1(pPawn);
	filter.m_nHierarchyIds[0] = pPawn->m_pCollision()->m_collisionAttribute().m_nHierarchyId();

	bool result = UTIL_TraceShape(g_pGameTraceManager, &ray, &vecStart, &vecEnd, &filter, &trace);
	if (result) {
		trace_info_t trace_info;
		trace_info.m_pEnt = trace.m_pEnt;
		trace_info.m_pHitbox = trace.m_pHitbox;
		trace_info.m_vStartPos = trace.m_vStartPos;
		trace_info.m_vEndPos = trace.m_vEndPos;
		trace_info.m_vHitNormal = trace.m_vHitNormal;
		trace_info.m_vHitPoint = trace.m_vHitPoint;
		trace_info.m_flHitOffset = trace.m_flHitOffset;
		trace_info.m_flFraction = trace.m_flFraction;
		trace_info.m_nTriangle = trace.m_nTriangle;
		trace_info.m_nHitboxBoneIndex = trace.m_nHitboxBoneIndex;
		trace_info.m_eRayType = trace.m_eRayType;
		trace_info.m_bStartInSolid = trace.m_bStartInSolid;
		trace_info.m_bExactHitPoint = trace.m_bExactHitPoint;
		return trace_info;
	}
	return trace_info_t();
}

IGameEventListener2* PlayersApi::GetLegacyGameEventListener(int iSlot)
{
	if(UTIL_GetLegacyGameEventListener)
	{
		return UTIL_GetLegacyGameEventListener(CPlayerSlot(iSlot));
	}
	return nullptr;
}

void LayoutApi::Create(int iSlot, const char* szLayoutName, const char* szLayoutPath)
{
	if(iSlot < 0 || iSlot >= 64) return;
	CCSCustomHudLayout* layout = (CCSCustomHudLayout*)g_pUtilsApi->CreateEntityByName("custom_hud_layout", -1);
	CEntityKeyValues* pKeyValues = new CEntityKeyValues();
	pKeyValues->SetString("targetname", szLayoutName);
	pKeyValues->SetString("layout", szLayoutPath);
	g_pUtilsApi->DispatchSpawn(layout, pKeyValues);
	g_mapHudLayouts[iSlot][szLayoutName] = CHandle<CCSCustomHudLayout>(layout);
	g_pUtilsApi->SetTransmitState(layout->entindex(), false, {});
	g_pUtilsApi->SetTransmitState(layout->entindex(), true, {iSlot});
}

void LayoutApi::Destroy(int iSlot, const char* szLayoutName, float flDelay)
{
	if(iSlot < 0 || iSlot >= 64) return;
	std::string key = szLayoutName;
	if(g_mapHudLayouts[iSlot].find(key) != g_mapHudLayouts[iSlot].end())
	{
		CHandle<CCSCustomHudLayout> layout = g_mapHudLayouts[iSlot][key];
		if(layout)
		{
			g_mapTransmitState[layout->entindex()].clear();
			if(flDelay > 0.f)
			{
				new CTimer(flDelay, [iSlot, key]()
				{
					g_pUtilsApi->RemoveEntity(g_mapHudLayouts[iSlot][key]);
					g_mapHudLayouts[iSlot].erase(key);
					return -1.0f;
				});
			}
			else
			{
				g_pUtilsApi->RemoveEntity(layout);
				g_mapHudLayouts[iSlot].erase(key);
			}
		}
	}
}

void LayoutApi::SetHasClass(int iSlot, const char* szLayoutName, CUtlString szPanelId, CUtlString szClassName, bool bHasClass)
{
	if(iSlot < 0 || iSlot >= 64) return;
	if(g_mapHudLayouts[iSlot].find(szLayoutName) != g_mapHudLayouts[iSlot].end())
	{
		CHandle<CCSCustomHudLayout> layout = g_mapHudLayouts[iSlot][szLayoutName];
		if(layout)
		{
			layout->SetHasClass(iSlot, szPanelId, szClassName, bHasClass);
		}
	}
}

void LayoutApi::SetInputCapture(int iSlot, const char* szLayoutName, bool bCapture)
{
	if(iSlot < 0 || iSlot >= 64) return;
	if(g_mapHudLayouts[iSlot].find(szLayoutName) != g_mapHudLayouts[iSlot].end())
	{
		CHandle<CCSCustomHudLayout> layout = g_mapHudLayouts[iSlot][szLayoutName];
		if(layout)
		{
			layout->SetInputCaptureEnabled(iSlot, bCapture);
		}
	}	
}

bool LayoutApi::GetInputCapture(int iSlot, const char* szLayoutName)
{
	if(iSlot < 0 || iSlot >= 64) return false;
	if(g_mapHudLayouts[iSlot].find(szLayoutName) != g_mapHudLayouts[iSlot].end())
	{
		CHandle<CCSCustomHudLayout> layout = g_mapHudLayouts[iSlot][szLayoutName];
		if(layout)
		{
			return layout->GetInputCaptureEnabled(iSlot);
		}
	}
	return false;
}

void LayoutApi::SetDialogVariable(int iSlot, const char* szLayoutName, CUtlString szPanelId, CUtlString szVariableName, CUtlString szValue)
{
	if(iSlot < 0 || iSlot >= 64) return;
	if(g_mapHudLayouts[iSlot].find(szLayoutName) != g_mapHudLayouts[iSlot].end())
	{
		CHandle<CCSCustomHudLayout> layout = g_mapHudLayouts[iSlot][szLayoutName];
		if(layout)
		{
			layout->SetDialogVariableString(iSlot, szPanelId, szVariableName, szValue);
		}
	}
}

const char* LayoutApi::GetDialogVariable(int iSlot, const char* szLayoutName, CUtlString szPanelId, CUtlString szVariableName)
{
	if(iSlot < 0 || iSlot >= 64) return "";
	if(g_mapHudLayouts[iSlot].find(szLayoutName) != g_mapHudLayouts[iSlot].end())
	{
		CHandle<CCSCustomHudLayout> layout = g_mapHudLayouts[iSlot][szLayoutName];
		if(layout)
		{
			return layout->GetDialogVariableString(iSlot, szPanelId, szVariableName);
		}
	}
	return "";
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
	return "1.9.0";
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
