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
int g_iTimeoutMenu;
int g_iSoundType;

std::map<std::string, std::string> g_mapSounds;

std::vector<std::string> g_vCommandEater;

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
void (*UTIL_AcceptInput)(CEntityInstance* pThis, const char* pInputName, CEntityInstance* pActivator, CEntityInstance* pCaller, const variant_t& pValue, int nOutputID, void* pUnk1) = nullptr;
bool (*UTIL_TraceShape)(CPhysicsQuery*, const Ray_t* ray, const Vector* start, const Vector* end, CTraceFilter* filter, trace_t* trace) = nullptr;

// void (*UTIL_ClientPrint)(CBasePlayerController*, int, const char *, const char *, const char *, const char *, const char *) = nullptr;
// void (*UTIL_ClientPrintAll)(int, const char *, const char *, const char *, const char *, const char *) = nullptr;

using namespace DynLibUtils;

funchook_t* m_SayHook;
funchook_t* m_SayTeamHook;
funchook_t* m_TakeDamageHook;
funchook_t* m_IsHearingClientHook;

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
	if (!strcmp(iface, Menus_INTERFACE))
	{
		*ret = META_IFACE_OK;
		return g_pMenusCore;
	}
	if (!strcmp(iface, Utils_INTERFACE))
	{
		*ret = META_IFACE_OK;
		return g_pUtilsCore;
	}
	if (!strcmp(iface, PLAYERS_INTERFACE))
	{
		*ret = META_IFACE_OK;
		return g_pPlayersCore;
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
		if (szMenuType && szMenuType[0]) g_iMenuType[iSlot] = atoi(szMenuType);
		else g_iMenuType[iSlot] = g_iMenuTypeDefault;
	});
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
	return hData->GetInt("Utils.MenuType", g_iMenuTypeDefault);
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
	g_pMenusCore->SetExitMenu(hMenu, true);
	g_pMenusCore->SetCallback(hMenu, [](const char* szBack, const char* szFront, int iItem, int iSlot){
		if(iItem < 7) {
			int iType = std::atoi(szBack);
			g_iMenuType[iSlot] = iType;
			if(g_pCookies) g_pCookies->SetCookie(iSlot, "Utils.MenuType", std::to_string(iType).c_str());
			else SetClientCookie(iSlot, "Utils.MenuType", std::to_string(iType).c_str());
			SettingMenu(iSlot);
		}
	});
	g_pMenusCore->DisplayPlayerMenu(hMenu, iSlot, true, true);
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
	GET_V_IFACE_CURRENT(GetEngineFactory, g_pNetworkServerService, INetworkServerService, NETWORKSERVERSERVICE_INTERFACE_VERSION);
	GET_V_IFACE_CURRENT(GetEngineFactory, g_pGameResourceServiceServer, IGameResourceService, GAMERESOURCESERVICESERVER_INTERFACE_VERSION);

	SH_ADD_HOOK_MEMFUNC(ICvar, DispatchConCommand, g_pCVar, this, &Menus::OnDispatchConCommand, false);
	SH_ADD_HOOK(IServerGameDLL, GameFrame, g_pSource2Server, SH_MEMBER(this, &Menus::GameFrame), true);
	SH_ADD_HOOK(IServerGameClients, ClientCommand, g_pSource2GameClients, SH_MEMBER(this, &Menus::ClientCommand), false);
	SH_ADD_HOOK(INetworkServerService, StartupServer, g_pNetworkServerService, SH_MEMBER(this, &Menus::StartupServer), true);
	SH_ADD_HOOK(IServerGameDLL, GameServerSteamAPIActivated, g_pSource2Server, SH_MEMBER(this, &Menus::OnGameServerSteamAPIActivated), false);
	SH_ADD_HOOK(IServerGameClients, ClientDisconnect, g_pSource2GameClients, SH_MEMBER(this, &Menus::OnClientDisconnect), true);
	SH_ADD_HOOK(IServerGameClients, ClientPutInServer, g_pSource2GameClients, SH_MEMBER(this, &Menus::Hook_ClientPutInServer), true);
	SH_ADD_HOOK(IServerGameClients, OnClientConnected, g_pSource2GameClients, SH_MEMBER(this, &Menus::Hook_OnClientConnected), false);
	SH_ADD_HOOK(IServerGameClients, ClientConnect, g_pSource2GameClients, SH_MEMBER(this, &Menus::Hook_ClientConnect), false );
	
	ConVar_Register(FCVAR_RELEASE | FCVAR_CLIENT_CAN_EXECUTE | FCVAR_GAMEDLL);

	if (late)
	{
		g_pEntitySystem = GameEntitySystem();
		gpGlobals = engine->GetServerGlobals();
	}

	//0 - в чат
	//1 - также как в чат но в центр
	//2 - выбор через WASD 

	g_pMenusApi = new MenusApi();
	g_pMenusCore = g_pMenusApi;

	g_pUtilsApi = new UtilsApi();
	g_pUtilsCore = g_pUtilsApi;

	g_pPlayersApi = new PlayersApi();
	g_pPlayersCore = g_pPlayersApi;

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
		g_iTimeoutMenu = g_kvCore->GetInt("TimeoutInputMenu", 160);
		g_iSoundType = g_kvCore->GetInt("sound_type", 0);

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
			gameeventmanager = gameEventManagerFn.Offset(30).ResolveRelativeAddress(0x3, 0x7).GetValue<IGameEventManager2*>();
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
		if(g_iMenuType[iSlot] != 2) if(CheckActionMenu(iSlot, CCSPlayerController::FromSlot(iSlot), 1)) return true;
		return false;
	});
	g_pUtilsApi->RegCommand(g_PLID, {"mm_2"}, {}, [](int iSlot, const char* szContent){
		if(g_iMenuType[iSlot] != 2) if(CheckActionMenu(iSlot, CCSPlayerController::FromSlot(iSlot), 2)) return true;
		return false;
	});
	g_pUtilsApi->RegCommand(g_PLID, {"mm_3"}, {}, [](int iSlot, const char* szContent){
		if(g_iMenuType[iSlot] != 2) if(CheckActionMenu(iSlot, CCSPlayerController::FromSlot(iSlot), 3)) return true;
		return false;
	});
	g_pUtilsApi->RegCommand(g_PLID, {"mm_4"}, {}, [](int iSlot, const char* szContent){
		if(g_iMenuType[iSlot] != 2) if(CheckActionMenu(iSlot, CCSPlayerController::FromSlot(iSlot), 4)) return true;
		return false;
	});
	g_pUtilsApi->RegCommand(g_PLID, {"mm_5"}, {}, [](int iSlot, const char* szContent){
		if(g_iMenuType[iSlot] != 2) if(CheckActionMenu(iSlot, CCSPlayerController::FromSlot(iSlot), 5)) return true;
		return false;
	});
	g_pUtilsApi->RegCommand(g_PLID, {"mm_6"}, {}, [](int iSlot, const char* szContent){
		if(g_iMenuType[iSlot] != 2) if(CheckActionMenu(iSlot, CCSPlayerController::FromSlot(iSlot), 6)) return true;
		return false;
	});
	g_pUtilsApi->RegCommand(g_PLID, {"mm_7"}, {}, [](int iSlot, const char* szContent){
		if(g_iMenuType[iSlot] != 2) if(CheckActionMenu(iSlot, CCSPlayerController::FromSlot(iSlot), 7)) return true;
		return false;
	});
	g_pUtilsApi->RegCommand(g_PLID, {"mm_8"}, {}, [](int iSlot, const char* szContent){
		if(g_iMenuType[iSlot] != 2) if(CheckActionMenu(iSlot, CCSPlayerController::FromSlot(iSlot), 8)) return true;
		return false;
	});
	g_pUtilsApi->RegCommand(g_PLID, {"mm_9"}, {}, [](int iSlot, const char* szContent){
		if(g_iMenuType[iSlot] != 2) if(CheckActionMenu(iSlot, CCSPlayerController::FromSlot(iSlot), 9)) return true;
		return false;
	});

	return true;
}

void Menus::OnPluginUnload(PluginId id) {
	g_pUtilsApi->ClearAllHooks(id);
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
	SH_REMOVE_HOOK(IServerGameClients, ClientPutInServer, g_pSource2GameClients, SH_MEMBER(this, &Menus::Hook_ClientPutInServer), true);
	SH_REMOVE_HOOK(IServerGameClients, OnClientConnected, g_pSource2GameClients, SH_MEMBER(this, &Menus::Hook_OnClientConnected), false);
	SH_REMOVE_HOOK(IServerGameClients, ClientConnect, g_pSource2GameClients, SH_MEMBER(this, &Menus::Hook_ClientConnect), false );

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

void Menus::Hook_OnClientConnected(CPlayerSlot slot, const char* pszName, uint64 xuid, const char* pszNetworkID, const char* pszAddress, bool bFakePlayer)
{
	if(bFakePlayer)
		m_Players[slot.Get()] = new Player(slot.Get(), true);
}

bool Menus::Hook_ClientConnect( CPlayerSlot slot, const char *pszName, uint64 xuid, const char *pszNetworkID, bool unk1, CBufferString *pRejectReason )
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

void Menus::Hook_ClientPutInServer( CPlayerSlot slot, char const *pszName, int type, uint64 xuid )
{
	m_Players[slot.Get()]->SetInGame(true);
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

		// Timer execute 
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
					if (g_iMenuType[iSlot] != 2)
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

void Menus::StartupServer(const GameSessionConfiguration_t& config, ISource2WorldSession*, const char*)
{
	for(int i = 0; i < 64; i++)
	{
		g_MenuPlayer[i].clear();
		g_TextMenuPlayer[i] = "";
		g_iMenuItem[i] = 1;
	}
	g_Offsets.clear();
	g_ChainOffsets.clear();
	g_pGameRules = nullptr;
	g_pEntitySystem = GameEntitySystem();
	gpGlobals = engine->GetServerGlobals();
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
	if (iSlot < 0 || iSlot >= 64 || !m_Players[iSlot]) return;
	delete m_Players[iSlot];
	m_Players[iSlot] = nullptr;

	if (xuid == 0)
    	return;

	g_MenuPlayer[iSlot].clear();
	g_TextMenuPlayer[iSlot] = "";
	g_iMenuItem[iSlot] = 1;
}

bool MenusApi::IsMenuOpen(int iSlot) {
	return g_MenuPlayer[iSlot].bEnabled;
}

void MenusApi::SetTitleMenu(Menu& hMenu, const char* szTitle) {
	hMenu.szTitle = std::string(szTitle);
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

void MenusApi::DisplayPlayerMenu(Menu& hMenu, int iSlot, bool bClose = true)
{
	if(iSlot < 0 || iSlot >= 64) return;
	DisplayPlayerMenu(hMenu, iSlot, bClose, true);
}

void MenusApi::DisplayPlayerMenu(Menu& hMenu, int iSlot, bool bClose = true, bool bReset = true)
{
	if(iSlot < 0 || iSlot >= 64) return;
    MenuPlayer& hMenuPlayer = g_MenuPlayer[iSlot];
	if (hMenuPlayer.bEnabled && bClose && bReset) {
		hMenuPlayer.clear();
	}
	if(!hMenuPlayer.bEnabled || !bReset)
	{
		g_iMenuItem[iSlot] = 1;
		hMenuPlayer.bEnabled = true;
		hMenuPlayer.hMenu = hMenu;
		hMenuPlayer.iEnd = std::time(0) + g_iMenuTime;
		if(hMenuPlayer.iList > 0) {
			int iLists = RoundToCeil(size(hMenu.hItems) / 5.0);
			while (iLists < hMenuPlayer.iList + 1) {
				hMenuPlayer.iList--;
			}
		}
		new CTimer(0.0f,[iSlot, &hMenu, &hMenuPlayer]() {
			if(!hMenuPlayer.bEnabled) return -1.0f;
			if(std::time(0) >= hMenuPlayer.iEnd)
			{
				hMenuPlayer.clear();
				if(g_iMenuType[iSlot] == 2 && g_bStopingUser) {
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
}

void MenusApi::AddRawItemMenu(Menu &hMenu, const char* sBack, const char* sText, int iType = 1)
{
    if (iType == 0) return;

    Items hItem;
    hItem.iType = iType;
    hItem.sBack = std::string(sBack);
    hItem.sText = std::string(sText);
    hMenu.hItems.push_back(hItem);
}

void MenusApi::ClosePlayerMenu(int iSlot)
{
	if(iSlot < 0 || iSlot >= 64) return;
	if(g_iMenuType[iSlot] == 2 && g_bStopingUser) {
		g_pPlayersApi->SetMoveType(iSlot, MOVETYPE_WALK);
	}
	g_MenuPlayer[iSlot].clear();
	g_TextMenuPlayer[iSlot] = "";
	g_iMenuItem[iSlot] = 1;
}

void ClientPrintFilter(CPlayerBitVec filter, int msg_dest, const char *msg_name, const char *param1, const char *param2, const char *param3, const char *param4)
{
	INetworkMessageInternal *netmsg = g_pNetworkMessages->FindNetworkMessagePartial("TextMsg");
	auto msg = netmsg->AllocateMessage()->ToPB<CUserMessageTextMsg>();
	msg->set_dest(msg_dest);
	msg->add_param(msg_name);
	msg->add_param(param1);
	msg->add_param(param2);
	msg->add_param(param3);
	msg->add_param(param4);

	g_gameEventSystem->PostEventAbstract(-1, false, ABSOLUTE_PLAYER_LIMIT, reinterpret_cast<const uint64*>(filter.Base()), netmsg, msg, 0, NetChannelBufType_t::BUF_RELIABLE);
    delete msg;
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
    	UTIL_AcceptInput(pEntity, szInputName, pActivator, pCaller, value, 0, 0LL);
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

const char* UtilsApi::GetVersion()
{
	return g_PLAPI->GetVersion();
}

///////////////////////////////////////
const char* Menus::GetLicense()
{
	return "GPL";
}

const char* Menus::GetVersion()
{
	return "1.8.6.1";
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