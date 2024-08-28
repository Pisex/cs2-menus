#include <stdio.h>
#include "utils.h"
#include "metamod_oslink.h"
#include "schemasystem/schemasystem.h"
#include <iostream>
#include <iomanip>
#include <ctime>
#include <sstream>

Menus g_Menus;
PLUGIN_EXPOSE(Menus, g_Menus);

CGlobalVars* gpGlobals = nullptr;
IVEngineServer2* engine = nullptr;
CCSGameRules* g_pGameRules = nullptr;
CEntitySystem* g_pEntitySystem = nullptr;
IGameEventSystem* g_gameEventSystem = nullptr;
IGameEventManager2* gameeventmanager = nullptr;
CGameEntitySystem* g_pGameEntitySystem = nullptr;
INetworkGameServer* g_pNetworkGameServer = nullptr;

float g_flUniversalTime;
float g_flLastTickedTime;
bool g_bHasTicked;


class CRecipientFilter : public IRecipientFilter
{
public:
	CRecipientFilter(NetChannelBufType_t nBufType = BUF_RELIABLE, bool bInitMessage = false) : m_nBufType(nBufType), m_bInitMessage(bInitMessage) {}

	~CRecipientFilter() override {}

	NetChannelBufType_t GetNetworkBufType(void) const override
	{
		return m_nBufType;
	}

	bool IsInitMessage(void) const override
	{
		return m_bInitMessage;
	}

	int GetRecipientCount(void) const override
	{
		return m_Recipients.Count();
	}

	CPlayerSlot GetRecipientIndex(int slot) const override
	{
		if (slot < 0 || slot >= GetRecipientCount())
		{
			return CPlayerSlot(-1);
		}

		return m_Recipients[slot];
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
		if (!GameEntitySystem())
		{
			return;
		}
		for (int i = 0; i <= gpGlobals->maxClients; i++)
		{
			CBaseEntity *ent = static_cast<CBaseEntity *>(GameEntitySystem()->GetEntityInstance(CEntityIndex(i)));
			if (ent)
			{
				AddRecipient(i);
			}
		}
	}

private:
	// Can't copy this unless we explicitly do it!
	CRecipientFilter(CRecipientFilter const &source)
	{
		Assert(0);
	}

	NetChannelBufType_t m_nBufType;
	bool m_bInitMessage;
	CUtlVectorFixed<CPlayerSlot, 64> m_Recipients;
};

class CBroadcastRecipientFilter : public CRecipientFilter
{
public:
	CBroadcastRecipientFilter(void)
	{
		AddAllPlayers();
	}
};

class CSingleRecipientFilter : public IRecipientFilter
{
public:
	CSingleRecipientFilter(int iRecipient, NetChannelBufType_t nBufType = BUF_RELIABLE, bool bInitMessage = false)
		: m_nBufType(nBufType), m_bInitMessage(bInitMessage), m_iRecipient(iRecipient)
	{
	}

	~CSingleRecipientFilter() override {}

	NetChannelBufType_t GetNetworkBufType(void) const override
	{
		return m_nBufType;
	}

	bool IsInitMessage(void) const override
	{
		return m_bInitMessage;
	}

	int GetRecipientCount(void) const override
	{
		return 1;
	}

	CPlayerSlot GetRecipientIndex(int slot) const override
	{
		return CPlayerSlot(m_iRecipient);
	}

private:
	NetChannelBufType_t m_nBufType;
	bool m_bInitMessage;
	int m_iRecipient;
};

CGameEntitySystem* GameEntitySystem()
{
	g_pGameEntitySystem = *reinterpret_cast<CGameEntitySystem**>(reinterpret_cast<uintptr_t>(g_pGameResourceServiceServer) + WIN_LINUX(0x58, 0x50));
	return g_pGameEntitySystem;
}

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

char szLanguage[16];
int g_iMenuType;
int g_iMenuTime;
int g_iDelayAuthFailKick;

int g_iOnTakeDamageAliveId = -1;

class GameSessionConfiguration_t { };
SH_DECL_HOOK3_void(IServerGameDLL, GameFrame, SH_NOATTRIB, 0, bool, bool, bool);
SH_DECL_HOOK2(IGameEventManager2, FireEvent, SH_NOATTRIB, 0, bool, IGameEvent*, bool);
SH_DECL_HOOK2_void(IServerGameClients, ClientCommand, SH_NOATTRIB, 0, CPlayerSlot, const CCommand&);
SH_DECL_HOOK3_void(ICvar, DispatchConCommand, SH_NOATTRIB, 0, ConCommandHandle, const CCommandContext&, const CCommand&);
SH_DECL_HOOK3_void(INetworkServerService, StartupServer, SH_NOATTRIB, 0, const GameSessionConfiguration_t&, ISource2WorldSession*, const char*);
SH_DECL_HOOK0_void(IServerGameDLL, GameServerSteamAPIActivated, SH_NOATTRIB, 0);
SH_DECL_HOOK5_void(IServerGameClients, ClientDisconnect, SH_NOATTRIB, 0, CPlayerSlot, ENetworkDisconnectionReason, const char *, uint64, const char *);
SH_DECL_HOOK4_void(IServerGameClients, ClientPutInServer, SH_NOATTRIB, 0, CPlayerSlot, char const *, int, uint64);
SH_DECL_HOOK6_void(IServerGameClients, OnClientConnected, SH_NOATTRIB, 0, CPlayerSlot, const char*, uint64, const char *, const char *, bool);
SH_DECL_HOOK6(IServerGameClients, ClientConnect, SH_NOATTRIB, 0, bool, CPlayerSlot, const char*, uint64, const char *, bool, CBufferString *);

SH_DECL_MANUALHOOK1(OnTakeDamage_Alive, 0, 0, 0, bool, CTakeDamageInfoContainer *);

void (*UTIL_Say)(const CCommandContext& ctx, CCommand& args) = nullptr;
void (*UTIL_SayTeam)(const CCommandContext& ctx, CCommand& args) = nullptr;
void (*UTIL_SetModel)(CBaseModelEntity*, const char* szModel) = nullptr;
void (*UTIL_DispatchSpawn)(CEntityInstance*, CEntityKeyValues*) = nullptr;
CBaseEntity* (*UTIL_CreateEntity)(const char *pClassName, CEntityIndex iForceEdictIndex) = nullptr;
void (*UTIL_Remove)(CEntityInstance*) = nullptr;
void (*UTIL_AcceptInput)(CEntityInstance*, const char* szString, CEntityInstance*, CEntityInstance*, const variant_t& value, int outputID) = nullptr;
IGameEventListener2* (*UTIL_GetLegacyGameEventListener)(CPlayerSlot slot) = nullptr;
void (*UTIL_TakeDamage)(CEntityInstance*, CTakeDamageInfo) = nullptr;

using namespace DynLibUtils;

funchook_t* m_SayHook;
funchook_t* m_SayTeamHook;
funchook_t* m_TakeDamageHook;

bool containsOnlyDigits(const std::string& str) {
	return str.find_first_not_of("0123456789") == std::string::npos;
}

void SayTeamHook(const CCommandContext& ctx, CCommand& args)
{
	bool bCallback = true;
	bCallback = g_pUtilsApi->SendChatListenerPreCallback(ctx.GetPlayerSlot().Get(), args.ArgS());
	if(args[1][0])
	{
		if(g_pEntitySystem)
		{
			auto pController = CCSPlayerController::FromSlot(ctx.GetPlayerSlot().Get());
			if(bCallback && pController && pController->GetPawn() && pController->m_steamID() != 0 && g_MenuPlayer[ctx.GetPlayerSlot().Get()].bEnabled && containsOnlyDigits(std::string(args[1] + 1)))
				bCallback = false;
		}
	}
	bCallback = g_pUtilsApi->SendChatListenerPostCallback(ctx.GetPlayerSlot().Get(), args.ArgS(), bCallback);
	if(bCallback)
	{
		UTIL_SayTeam(ctx, args);
	}
}

void SayHook(const CCommandContext& ctx, CCommand& args)
{
	bool bCallback = true;
	bCallback = g_pUtilsApi->SendChatListenerPreCallback(ctx.GetPlayerSlot().Get(), args.ArgS());
	if(args[1][0])
	{
		if(g_pEntitySystem)
		{
			auto pController = CCSPlayerController::FromSlot(ctx.GetPlayerSlot().Get());
			if(bCallback && pController && pController->GetPawn() && pController->m_steamID() != 0 && g_MenuPlayer[ctx.GetPlayerSlot().Get()].bEnabled && containsOnlyDigits(std::string(args[1] + 1)))
				bCallback = false;
		}
	}
	bCallback = g_pUtilsApi->SendChatListenerPostCallback(ctx.GetPlayerSlot().Get(), args.ArgS(), bCallback);
	if(bCallback)
	{
		UTIL_Say(ctx, args);
	}
}

void Hook_TakeDamage(CEntityInstance* pEntity, CTakeDamageInfo info)
{
	if (pEntity)
	{
		CCSPlayerPawn* pPawn = (CCSPlayerPawn*)pEntity;
		if (pPawn)
		{
			auto pController = pPawn->m_hController();
			if (pController)
			{
				int iPlayerSlot = pController->GetEntityIndex().Get() - 1;
				if (iPlayerSlot >= 0 && iPlayerSlot < 64)
				{
					if (!g_pUtilsApi->SendHookOnTakeDamagePre(iPlayerSlot, info))
						return;
				}
			}
		}
	}
	UTIL_TakeDamage(pEntity, info);
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
	auto& hMenuPlayer = g_MenuPlayer[iSlot];
	auto& hMenu = hMenuPlayer.hMenu;
	if(hMenuPlayer.bEnabled)
	{
		hMenuPlayer.iEnd = std::time(0) + g_iMenuTime;
		if(iButton == 9 && hMenu.bExit)
		{
			hMenuPlayer.iList = 0;
			hMenuPlayer.bEnabled = false;
			if(g_iMenuType == 0)
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
				g_pMenusCore->DisplayPlayerMenu(hMenu, iSlot, false);
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
					g_pMenusCore->DisplayPlayerMenu(hMenu, iSlot, false);
				}
				else if(hMenu.hFunc) hMenu.hFunc("back", "back", 7, iSlot);
			}
		}
		else
		{
			int iItems = size(hMenu.hItems);
			int iItem = hMenuPlayer.iList*5+iButton-1;
			if(iItems <= iItem) return 1;
			if(hMenu.hItems[iItem].iType != 1) return 1;
			if(hMenu.hFunc) hMenu.hFunc(hMenu.hItems[iItem].sBack.c_str(), hMenu.hItems[iItem].sText.c_str(), iButton, iSlot);
		}
		return 1;
	}
	return 0;
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
		KeyValues::AutoDelete g_kvCore("Core");
		const char *pszPath = "addons/configs/core.cfg";

		if (!g_kvCore->LoadFromFile(g_pFullFileSystem, pszPath))
		{
			g_pUtilsApi->ErrorLog("[%s] Failed to load %s\n", g_PLAPI->GetLogTag(), pszPath);
			return false;
		}

		g_SMAPI->Format(szLanguage, sizeof(szLanguage), "%s", g_kvCore->GetString("ServerLang", "en"));
		g_iMenuType = g_kvCore->GetInt("MenuType", 0);
		g_iMenuTime = g_kvCore->GetInt("MenuTime", 60);
		g_iDelayAuthFailKick = g_kvCore->GetInt("delay_auth_fail_kick", 30);
	}

	g_pUtilsApi->LoadTranslations("menus.phrases");

	KeyValues::AutoDelete g_kvSigs("Gamedata");
	const char *pszPath = "addons/configs/signatures.ini";

	if (!g_kvSigs->LoadFromFile(g_pFullFileSystem, pszPath))
	{
		g_pUtilsApi->ErrorLog("[%s] Failed to load %s\n", g_PLAPI->GetLogTag(), pszPath);
		return false;
	}
	const char* pszSay = g_kvSigs->GetString("UTIL_Say");
	CModule libserver(g_pSource2Server);
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

	UTIL_TakeDamage = libserver.FindPattern(g_kvSigs->GetString("OnTakeDamagePre")).RCast< decltype(UTIL_TakeDamage) >();
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

	UTIL_SetModel = libserver.FindPattern(g_kvSigs->GetString("CBaseModelEntity_SetModel")).RCast< decltype(UTIL_SetModel) >();
	if (!UTIL_SetModel)
	{
		g_pUtilsApi->ErrorLog("[%s] Failed to find function to get CBaseModelEntity_SetModel", g_PLAPI->GetLogTag());
	}

	UTIL_AcceptInput = libserver.FindPattern(g_kvSigs->GetString("UTIL_AcceptInput")).RCast< decltype(UTIL_AcceptInput) >();
	if (!UTIL_AcceptInput)
	{
		g_pUtilsApi->ErrorLog("[%s] Failed to find function to get UTIL_AcceptInput", g_PLAPI->GetLogTag());
	}
	UTIL_Remove = libserver.FindPattern(g_kvSigs->GetString("UTIL_Remove")).RCast< decltype(UTIL_Remove) >();
	if (!UTIL_Remove)
	{
		g_pUtilsApi->ErrorLog("[%s] Failed to find function to get UTIL_Remove", g_PLAPI->GetLogTag());
	}
	UTIL_DispatchSpawn = libserver.FindPattern(g_kvSigs->GetString("UTIL_DispatchSpawn")).RCast< decltype(UTIL_DispatchSpawn) >();
	if (!UTIL_DispatchSpawn)
	{
		g_pUtilsApi->ErrorLog("[%s] Failed to find function to get UTIL_DispatchSpawn", g_PLAPI->GetLogTag());
	}
	UTIL_CreateEntity = libserver.FindPattern(g_kvSigs->GetString("UTIL_CreateEntity")).RCast< decltype(UTIL_CreateEntity) >();
	if (!UTIL_CreateEntity)
	{
		g_pUtilsApi->ErrorLog("[%s] Failed to find function to get UTIL_CreateEntity", g_PLAPI->GetLogTag());
	}
	UTIL_GetLegacyGameEventListener = libserver.FindPattern(g_kvSigs->GetString("GetLegacyGameEventListener")).RCast< decltype(UTIL_GetLegacyGameEventListener) >();
	if (!UTIL_GetLegacyGameEventListener)
	{
		g_pUtilsApi->ErrorLog("[%s] Failed to find function to get GetLegacyGameEventListener", g_PLAPI->GetLogTag());
	}

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

	auto gameEventManagerFn = libserver.FindPattern(g_kvSigs->GetString("GetGameEventManager"));
	if( !gameEventManagerFn ) {
		g_pUtilsApi->ErrorLog("[%s] Failed to find function to get GetGameEventManager", g_PLAPI->GetLogTag());
	}
	else
	{
		gameeventmanager = gameEventManagerFn.Offset(0x1F).ResolveRelativeAddress(0x3, 0x7).GetValue<IGameEventManager2*>();
		SH_ADD_HOOK(IGameEventManager2, FireEvent, gameeventmanager, SH_MEMBER(this, &Menus::FireEvent), false);
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
		if(g_iMenuType != 2) if(CheckActionMenu(iSlot, CCSPlayerController::FromSlot(iSlot), 1)) return true;
		return false;
	});
	g_pUtilsApi->RegCommand(g_PLID, {"mm_2"}, {}, [](int iSlot, const char* szContent){
		if(g_iMenuType != 2) if(CheckActionMenu(iSlot, CCSPlayerController::FromSlot(iSlot), 2)) return true;
		return false;
	});
	g_pUtilsApi->RegCommand(g_PLID, {"mm_3"}, {}, [](int iSlot, const char* szContent){
		if(g_iMenuType != 2) if(CheckActionMenu(iSlot, CCSPlayerController::FromSlot(iSlot), 3)) return true;
		return false;
	});
	g_pUtilsApi->RegCommand(g_PLID, {"mm_4"}, {}, [](int iSlot, const char* szContent){
		if(g_iMenuType != 2) if(CheckActionMenu(iSlot, CCSPlayerController::FromSlot(iSlot), 4)) return true;
		return false;
	});
	g_pUtilsApi->RegCommand(g_PLID, {"mm_5"}, {}, [](int iSlot, const char* szContent){
		if(g_iMenuType != 2) if(CheckActionMenu(iSlot, CCSPlayerController::FromSlot(iSlot), 5)) return true;
		return false;
	});
	g_pUtilsApi->RegCommand(g_PLID, {"mm_6"}, {}, [](int iSlot, const char* szContent){
		if(g_iMenuType != 2) if(CheckActionMenu(iSlot, CCSPlayerController::FromSlot(iSlot), 6)) return true;
		return false;
	});
	g_pUtilsApi->RegCommand(g_PLID, {"mm_7"}, {}, [](int iSlot, const char* szContent){
		if(g_iMenuType != 2) if(CheckActionMenu(iSlot, CCSPlayerController::FromSlot(iSlot), 7)) return true;
		return false;
	});
	g_pUtilsApi->RegCommand(g_PLID, {"mm_8"}, {}, [](int iSlot, const char* szContent){
		if(g_iMenuType != 2) if(CheckActionMenu(iSlot, CCSPlayerController::FromSlot(iSlot), 8)) return true;
		return false;
	});
	g_pUtilsApi->RegCommand(g_PLID, {"mm_9"}, {}, [](int iSlot, const char* szContent){
		if(g_iMenuType != 2) if(CheckActionMenu(iSlot, CCSPlayerController::FromSlot(iSlot), 9)) return true;
		return false;
	});

	return true;
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
	}
	g_pUtilsApi->NextFrame();

	if (simulating && g_bHasTicked)
	{
		g_flUniversalTime += gpGlobals->curtime - g_flLastTickedTime;
	}

	g_flLastTickedTime = gpGlobals->curtime;
	g_bHasTicked = true;

	for (int i = g_timers.Tail(); i != g_timers.InvalidIndex();)
	{
		auto timer = g_timers[i];

		int prevIndex = i;
		i = g_timers.Previous(i);

		if (timer->m_flLastExecute == -1)
			timer->m_flLastExecute = g_flUniversalTime;

		// Timer execute 
		if (timer->m_flLastExecute + timer->m_flInterval <= g_flUniversalTime)
		{
			if (!timer->Execute())
			{
				delete timer;
				g_timers.Remove(prevIndex);
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

void Menus::OnDispatchConCommand(ConCommandHandle cmdHandle, const CCommandContext& ctx, const CCommand& args)
{
	if (!g_pEntitySystem)
		return;

	auto iCommandPlayerSlot = ctx.GetPlayerSlot();

	bool bSay = !V_strcmp(args.Arg(0), "say");
	bool bTeamSay = !V_strcmp(args.Arg(0), "say_team");

	if (iCommandPlayerSlot != -1 && (bSay || bTeamSay))
	{
		auto pController = CCSPlayerController::FromSlot(iCommandPlayerSlot.Get());
		bool bCommand = *args[1] == '!' || *args[1] == '/';
		bool bSilent = *args[1] == '/';

		if (bCommand)
		{
			char *pszMessage = (char *)(args.ArgS() + 2);
			CCommand arg;
			arg.Tokenize(args.ArgS() + 2);
			if(arg[0][0])
			{
				if(containsOnlyDigits(std::string(arg[0])))
				{
					if(g_iMenuType != 2)
					{
						int iButton = atoi(arg[0]);
						if(CheckActionMenu(iCommandPlayerSlot.Get(), pController, iButton))
							RETURN_META(MRES_SUPERCEDE);
					}
				}
			}
		}

		if(std::string(args[1]).size() > 1)
		{
			char *pszMessage = (char *)(args.ArgS() + 1);
			CCommand arg;
			arg.Tokenize(pszMessage);
			bool bFound = g_pUtilsApi->FindAndSendCommandCallback(arg[0], ctx.GetPlayerSlot().Get(), pszMessage, false);
			if(bFound) RETURN_META(MRES_SUPERCEDE);
		}
	}
}

void Menus::StartupServer(const GameSessionConfiguration_t& config, ISource2WorldSession*, const char*)
{
	for(int i = 0; i < 64; i++)
	{
		g_MenuPlayer[i].clear();
		g_TextMenuPlayer[i] = "";
	}
	g_Offsets.clear();
	g_ChainOffsets.clear();
	g_bHasTicked = false;
	g_pGameRules = nullptr;
	g_pEntitySystem = GameEntitySystem();
	gpGlobals = engine->GetServerGlobals();
	g_pUtilsApi->SendHookStartup();
}

void Menus::OnClientDisconnect( CPlayerSlot slot, ENetworkDisconnectionReason reason, const char *pszName, uint64 xuid, const char *pszNetworkID )
{
	delete m_Players[slot.Get()];
	m_Players[slot.Get()] = nullptr;

	if (xuid == 0)
    	return;

	g_MenuPlayer[slot.Get()].clear();
	g_TextMenuPlayer[slot.Get()] = "";
}

void MenusApi::SetTitleMenu(Menu& hMenu, const char* szTitle)
{
	hMenu.szTitle = std::string(szTitle);
}

void MenusApi::SetBackMenu(Menu& hMenu, bool bBack)
{
	hMenu.bBack = bBack;
}

void MenusApi::SetExitMenu(Menu& hMenu, bool bExit)
{
	hMenu.bExit = bExit;
}

void MenusApi::DisplayPlayerMenu(Menu& hMenu, int iSlot, bool bClose = true)
{
	MenuPlayer& hMenuPlayer = g_MenuPlayer[iSlot];
	if (hMenuPlayer.bEnabled && bClose)
		hMenuPlayer.clear();

	if(!hMenuPlayer.bEnabled)
	{
		hMenuPlayer.bEnabled = true;
		hMenuPlayer.hMenu = hMenu;
		hMenuPlayer.iEnd = std::time(0) + g_iMenuTime;
		new CTimer(0.0f,[iSlot, &hMenu, &hMenuPlayer]() {
			if(!hMenuPlayer.bEnabled) return -1.0f;
			if(std::time(0) >= hMenuPlayer.iEnd)
			{
				hMenuPlayer.clear();
				return -1.0f;
			}
			if(g_iMenuType == 1 && g_TextMenuPlayer[iSlot].size() > 0)
			{
				g_pUtilsCore->PrintToCenterHtml(iSlot, 0.0f, g_TextMenuPlayer[iSlot].c_str());
			}
			return 0.0f;
		});
	}
	if(g_iMenuType == 0)
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
					g_SMAPI->Format(sBuff, sizeof(sBuff), " \x04[!%i]\x01 %s", iCount+1, hMenu.hItems[l].sText.c_str());
					g_pUtilsCore->PrintToChat(iSlot, sBuff);
					break;
				case 2:
					g_SMAPI->Format(sBuff, sizeof(sBuff), " \x08[!%i]\x01 %s", iCount+1, hMenu.hItems[l].sText.c_str());
					g_pUtilsCore->PrintToChat(iSlot, sBuff);
					break;
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
	else if(g_iMenuType == 1)
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
	if(iType != 0)
	{
		Items hItem;
		hItem.iType = iType;
		hItem.sBack = std::string(sBack);
		hItem.sText = escapeString(sText);
		hMenu.hItems.push_back(hItem);
	}
}

void MenusApi::ClosePlayerMenu(int iSlot)
{
	g_MenuPlayer[iSlot].clear();
	g_TextMenuPlayer[iSlot] = "";
}

void ClientPrintFilter(IRecipientFilter *filter, int msg_dest, const char *msg_name, const char *param1, const char *param2, const char *param3, const char *param4)
{
	INetworkMessageInternal *netmsg = g_pNetworkMessages->FindNetworkMessagePartial("TextMsg");
	auto msg = netmsg->AllocateMessage()->ToPB<CUserMessageTextMsg>();
	msg->set_dest(msg_dest);
	msg->add_param(msg_name);
	msg->add_param(param1);
	msg->add_param(param2);
	msg->add_param(param3);
	msg->add_param(param4);

	g_gameEventSystem->PostEventAbstract(0, false, filter, netmsg, msg, 0);
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

	CBroadcastRecipientFilter *filter = new CBroadcastRecipientFilter;
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
	{
		ConMsg("%s\n", buf);
		return;
	}

	std::string colorizedBuf = Colorizer(buf);

	g_pUtilsApi->NextFrame([iSlot, pPlayerController, colorizedBuf](){
		if(pPlayerController->m_hPawn() && pPlayerController->m_steamID() > 0)
		{
			CSingleRecipientFilter *filter = new CSingleRecipientFilter(iSlot);
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

	CSingleRecipientFilter *filter = new CSingleRecipientFilter(iSlot);
	ClientPrintFilter(filter, HUD_PRINTCONSOLE, buf, "", "", "", "");
}

void UtilsApi::PrintToConsoleAll(const char *msg, ...)
{
	va_list args;
	va_start(args, msg);

	char buf[512];
	V_vsnprintf(buf, sizeof(buf), msg, args);
	va_end(args);

	CBroadcastRecipientFilter *filter = new CBroadcastRecipientFilter;
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
	{
		ConMsg("%s\n", buf);
		return;
	}

	CSingleRecipientFilter *filter = new CSingleRecipientFilter(iSlot);
	ClientPrintFilter(filter, HUD_PRINTCENTER, buf, "", "", "", "");
}

void UtilsApi::PrintToCenterAll(const char *msg, ...)
{
	va_list args;
	va_start(args, msg);

	char buf[512];
	V_vsnprintf(buf, sizeof(buf), msg, args);
	va_end(args);

	CBroadcastRecipientFilter *filter = new CBroadcastRecipientFilter;
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
	{
		ConMsg("%s\n", buf);
		return;
	}

	CSingleRecipientFilter *filter = new CSingleRecipientFilter(iSlot);
	ClientPrintFilter(filter, HUD_PRINTALERT, buf, "", "", "", "");
}

void UtilsApi::PrintToAlertAll(const char *msg, ...)
{
	va_list args;
	va_start(args, msg);

	char buf[512];
	V_vsnprintf(buf, sizeof(buf), msg, args);
	va_end(args);

	CBroadcastRecipientFilter *filter = new CBroadcastRecipientFilter;
	ClientPrintFilter(filter, HUD_PRINTALERT, buf, "", "", "", "");
}

void UtilsApi::PrintToCenterHtml(int iSlot, int iDuration, const char *msg, ...)
{
	va_list args;
	va_start(args, msg);

	char buf[2048];
	V_vsnprintf(buf, sizeof(buf), msg, args);
	va_end(args);

	CCSPlayerController* pPlayerController = CCSPlayerController::FromSlot(iSlot);
	if (!pPlayerController || pPlayerController->m_steamID() <= 0)
	{
		ConMsg("%s\n", buf);
		return;
	}
	int iEnd = std::time(0) + iDuration;
	if(UTIL_GetLegacyGameEventListener)
	{
		IGameEvent* pEvent = gameeventmanager->CreateEvent("show_survival_respawn_status");
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
    	UTIL_AcceptInput(pEntity, szInputName, pActivator, pCaller, value, 0);
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
		pEntity->NetworkStateChanged(nLocalOffset, nArrayIndex, *reinterpret_cast<ChangeAccessorFieldPathIndex_t*>(networkVarChainer + 32));
    }
}

void UtilsApi::SetStateChanged(CBaseEntity* CEntity, const char* sClassName, const char* sFieldName, int extraOffset = 0)
{
	if(CEntity)
	{
		if(g_Offsets[sClassName][sFieldName] == 0 || g_ChainOffsets[sClassName][sFieldName] == 0)
		{
			int offset = schema::GetServerOffset(sClassName, sFieldName);
			g_Offsets[sClassName][sFieldName] = offset;
			int chainOffset = schema::FindChainOffset(sClassName);
			g_ChainOffsets[sClassName][sFieldName] = chainOffset;
			if (chainOffset != 0)
			{
				ChainNetworkStateChanged((uintptr_t)(CEntity) + chainOffset, offset, 0xFFFFFFFF);
				return;
			}
			const auto entity = static_cast<CEntityInstance*>(CEntity);
			entity->NetworkStateChanged(offset);
		}
		else
		{
			int offset = g_Offsets[sClassName][sFieldName];
			int chainOffset = g_ChainOffsets[sClassName][sFieldName];
			if (chainOffset != 0)
			{
				ChainNetworkStateChanged((uintptr_t)(CEntity) + chainOffset, offset, 0xFFFFFFFF);
				return;
			}
			const auto entity = static_cast<CEntityInstance*>(CEntity);
			entity->NetworkStateChanged(offset);
		}
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

///////////////////////////////////////
const char* Menus::GetLicense()
{
	return "GPL";
}

const char* Menus::GetVersion()
{
	return "1.6.5";
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