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
IGameEventManager2* gameeventmanager = nullptr;
CGameEntitySystem* g_pGameEntitySystem = nullptr;
INetworkGameServer* g_pNetworkGameServer = nullptr;

float g_flUniversalTime;
float g_flLastTickedTime;
bool g_bHasTicked;

CGameEntitySystem* GameEntitySystem()
{
	g_pGameEntitySystem = *reinterpret_cast<CGameEntitySystem**>(reinterpret_cast<uintptr_t>(g_pGameResourceServiceServer) + WIN_LINUX(0x58, 0x50));
	return g_pGameEntitySystem;
}

std::map<uint32, MenuPlayer> g_MenuPlayer;
std::map<uint32, std::string> g_TextMenuPlayer;

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

void (*UTIL_ClientPrint)(CBasePlayerController *player, int msg_dest, const char* msg_name, const char* param1, const char* param2, const char* param3, const char* param4) = nullptr;
void (*UTIL_ClientPrintAll)(int msg_dest, const char* msg_name, const char* param1, const char* param2, const char* param3, const char* param4) = nullptr;

void (*UTIL_Say)(const CCommandContext& ctx, CCommand& args) = nullptr;
void (*UTIL_SayTeam)(const CCommandContext& ctx, CCommand& args) = nullptr;

using namespace DynLibUtils;

funchook_t* m_SayHook;
funchook_t* m_SayTeamHook;

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
			if(bCallback && pController && pController->GetPawn() && pController->m_steamID() != 0 && g_MenuPlayer[pController->m_steamID()].bEnabled && containsOnlyDigits(std::string(args[1] + 1)))
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
			if(bCallback && pController && pController->GetPawn() && pController->m_steamID() != 0 && g_MenuPlayer[pController->m_steamID()].bEnabled && containsOnlyDigits(std::string(args[1] + 1)))
				bCallback = false;
		}
	}
	bCallback = g_pUtilsApi->SendChatListenerPostCallback(ctx.GetPlayerSlot().Get(), args.ArgS(), bCallback);
	if(bCallback)
	{
		UTIL_Say(ctx, args);
	}
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

bool Menus::Load(PluginId id, ISmmAPI* ismm, char* error, size_t maxlen, bool late)
{
	PLUGIN_SAVEVARS();

	GET_V_IFACE_CURRENT(GetEngineFactory, g_pCVar, ICvar, CVAR_INTERFACE_VERSION);
	GET_V_IFACE_ANY(GetEngineFactory, g_pSchemaSystem, ISchemaSystem, SCHEMASYSTEM_INTERFACE_VERSION);
	GET_V_IFACE_CURRENT(GetEngineFactory, engine, IVEngineServer2, SOURCE2ENGINETOSERVER_INTERFACE_VERSION);
	GET_V_IFACE_CURRENT(GetFileSystemFactory, g_pFullFileSystem, IFileSystem, FILESYSTEM_INTERFACE_VERSION);
	GET_V_IFACE_CURRENT(GetServerFactory, g_pSource2Server, ISource2Server, SOURCE2SERVER_INTERFACE_VERSION);
	GET_V_IFACE_ANY(GetServerFactory, g_pSource2GameClients, IServerGameClients, SOURCE2GAMECLIENTS_INTERFACE_VERSION);
	GET_V_IFACE_CURRENT(GetEngineFactory, g_pNetworkServerService, INetworkServerService, NETWORKSERVERSERVICE_INTERFACE_VERSION);
	GET_V_IFACE_CURRENT(GetEngineFactory, g_pGameResourceServiceServer, IGameResourceService, GAMERESOURCESERVICESERVER_INTERFACE_VERSION);

	CModule libserver(g_pSource2Server);
	UTIL_ClientPrint = libserver.FindPattern(WIN_LINUX("48 85 C9 0F 84 2A 2A 2A 2A 48 8B C4 48 89 58 18", "55 48 89 E5 41 57 49 89 CF 41 56 49 89 D6 41 55 41 89 F5 41 54 4C 8D A5 A0 FE FF FF")).RCast< decltype(UTIL_ClientPrint) >();
	if (!UTIL_ClientPrint)
	{
		V_strncpy(error, "Failed to find function to get UTIL_ClientPrint", maxlen);
		ConColorMsg(Color(255, 0, 0, 255), "[%s] %s\n", GetLogTag(), error);
		return false;
	}
	
	UTIL_ClientPrintAll = libserver.FindPattern(WIN_LINUX("48 89 5C 24 08 48 89 6C 24 10 48 89 74 24 18 57 48 81 EC 70 01 2A 2A 8B E9", "55 48 89 E5 41 57 49 89 D7 41 56 49 89 F6 41 55 41 89 FD")).RCast< decltype(UTIL_ClientPrintAll) >();
	if (!UTIL_ClientPrintAll)
	{
		V_strncpy(error, "Failed to find function to get UTIL_ClientPrintAll", maxlen);
		ConColorMsg(Color(255, 0, 0, 255), "[%s] %s\n", GetLogTag(), error);
		return false;
	}

	UTIL_SayTeam = libserver.FindPattern("55 48 89 E5 41 56 41 55 49 89 F5 41 54 49 89 FC 53 48 83 EC 10 48 8D 05").RCast< decltype(UTIL_SayTeam) >();
	if (!UTIL_SayTeam)
	{
		V_strncpy(error, "Failed to find function to get UTIL_SayTeam", maxlen);
		ConColorMsg(Color(255, 0, 0, 255), "[%s] %s\n", GetLogTag(), error);
		return false;
	}
	m_SayTeamHook = funchook_create();
	funchook_prepare(m_SayTeamHook, (void**)&UTIL_SayTeam, (void*)SayTeamHook);
	funchook_install(m_SayTeamHook, 0);

	UTIL_Say = libserver.FindPattern("55 48 89 E5 41 56 41 55 49 89 F5 41 54 49 89 FC 53 48 83 EC 10 48 8D 05").RCast< decltype(UTIL_Say) >();
	if (!UTIL_Say)
	{
		V_strncpy(error, "Failed to find function to get UTIL_Say", maxlen);
		ConColorMsg(Color(255, 0, 0, 255), "[%s] %s\n", GetLogTag(), error);
		return false;
	}
	m_SayHook = funchook_create();
	funchook_prepare(m_SayHook, (void**)&UTIL_Say, (void*)SayHook);
	funchook_install(m_SayHook, 0);

	auto gameEventManagerFn = libserver.FindPattern( "55 31 C9 BA ? ? ? ? 48 89 E5 41 56 49 89 FE 41 55 49 89 F5 41 54 48 8D 35 ? ? ? ? 53");
	if( !gameEventManagerFn ) {
		snprintf( error, maxlen, "Not found func to get g_pGameEventManager" );
		return false;
	}
	gameeventmanager = gameEventManagerFn.Offset(0x1F).ResolveRelativeAddress(0x3, 0x7).GetValue<IGameEventManager2*>();
	SH_ADD_HOOK(IGameEventManager2, FireEvent, gameeventmanager, SH_MEMBER(this, &Menus::FireEvent), false);

	g_SMAPI->AddListener( this, this );

	SH_ADD_HOOK(INetworkServerService, StartupServer, g_pNetworkServerService, SH_MEMBER(this, &Menus::StartupServer), true);
	SH_ADD_HOOK_MEMFUNC(ICvar, DispatchConCommand, g_pCVar, this, &Menus::OnDispatchConCommand, false);
	SH_ADD_HOOK(IServerGameDLL, GameFrame, g_pSource2Server, SH_MEMBER(this, &Menus::GameFrame), true);
	SH_ADD_HOOK(IServerGameClients, ClientCommand, g_pSource2GameClients, SH_MEMBER(this, &Menus::ClientCommand), false);
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

	KeyValues::AutoDelete g_kvCore("Core");
	const char *pszPath = "addons/configs/core.cfg";

	if (!g_kvCore->LoadFromFile(g_pFullFileSystem, pszPath))
	{
		Warning("Failed to load %s\n", pszPath);
		return false;
	}

	g_SMAPI->Format(szLanguage, sizeof(szLanguage), "%s", g_kvCore->GetString("ServerLang", "en"));
	g_iMenuType = g_kvCore->GetInt("MenuType", 0);
	g_iMenuTime = g_kvCore->GetInt("MenuTime", 60);
	g_iDelayAuthFailKick = g_kvCore->GetInt("delay_auth_fail_kick", 30);

	//0 - в чат
	//1 - также как в чат но в центр
	//2 - выбор через WASD 

	g_pMenusApi = new MenusApi();
	g_pMenusCore = g_pMenusApi;

	g_pUtilsApi = new UtilsApi();
	g_pUtilsCore = g_pUtilsApi;

	g_pPlayersApi = new PlayersApi();
	g_pPlayersCore = g_pPlayersApi;

	g_pUtilsApi->LoadTranslations("menus.phrases");

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

	return true;
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

	funchook_destroy(m_SayHook);
	funchook_destroy(m_SayTeamHook);

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

int CheckActionMenu(int iSlot, CCSPlayerController* pController, int iButton)
{
	auto& hMenuPlayer = g_MenuPlayer[pController->m_steamID()];
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
			hMenuPlayer.hMenu = Menu();
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
						int iButton = std::stoi(arg[0]);
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
	g_MenuPlayer.clear();
	g_TextMenuPlayer.clear();
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

	CCSPlayerController* pPlayerController = CCSPlayerController::FromSlot(slot.Get());
	if (!pPlayerController)
		return;

	uint32 m_steamID = pPlayerController->m_steamID();
	if (m_steamID == 0)
		return;

	auto PlayerMenu = g_MenuPlayer.find(m_steamID);

	if(PlayerMenu == g_MenuPlayer.end()) {
		
		if (PlayerMenu != g_MenuPlayer.end())
			g_MenuPlayer.erase(PlayerMenu);
	}
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
	CCSPlayerController* pPlayer = CCSPlayerController::FromSlot(iSlot);
	if (!pPlayer)
		return;
	uint32 m_steamID = pPlayer->m_steamID();
	if (m_steamID == 0)
		return;
	MenuPlayer& hMenuPlayer = g_MenuPlayer[m_steamID];
	if (hMenuPlayer.bEnabled && bClose) {
		hMenuPlayer.iList = 0;
		hMenuPlayer.bEnabled = false;
	}
	if(!hMenuPlayer.bEnabled)
	{
		hMenuPlayer.bEnabled = true;
		hMenuPlayer.hMenu = hMenu;
		hMenuPlayer.iEnd = std::time(0) + g_iMenuTime;
		new CTimer(0.0f,[iSlot, m_steamID, &hMenu, &hMenuPlayer]() {
			if(!hMenuPlayer.bEnabled) return -1.0f;
			if(std::time(0) >= hMenuPlayer.iEnd)
			{
				hMenuPlayer.iList = 0;
				hMenuPlayer.bEnabled = false;
				return -1.0f;
			}
			if(g_iMenuType == 1 && g_TextMenuPlayer[m_steamID].size() > 0)
			{
				g_pUtilsCore->PrintToCenterHtml(iSlot, 0.0f, g_TextMenuPlayer[m_steamID].c_str());
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
		g_TextMenuPlayer[m_steamID] = sBuff;
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
	CCSPlayerController* pPlayer = CCSPlayerController::FromSlot(iSlot);
	if (!pPlayer)
		return;
	uint32 m_steamID = pPlayer->m_steamID();
	if (m_steamID == 0)
		return;
	MenuPlayer& hMenuPlayer = g_MenuPlayer[m_steamID];
	if (hMenuPlayer.bEnabled) {
		hMenuPlayer.iList = 0;
		hMenuPlayer.bEnabled = false;
		hMenuPlayer.hMenu = Menu();
	}
}

void UtilsApi::PrintToChatAll(const char *msg, ...)
{
	va_list args;
	va_start(args, msg);

	char buf[512];
	V_vsnprintf(buf, sizeof(buf), msg, args);
	va_end(args);

	std::string colorizedBuf = Colorizer(buf);
	UTIL_ClientPrintAll(3, colorizedBuf.c_str(), nullptr, nullptr, nullptr, nullptr);
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

	g_pUtilsApi->NextFrame([pPlayerController, colorizedBuf](){
		if(pPlayerController->m_hPawn() && pPlayerController->m_steamID() > 0)
			UTIL_ClientPrint(pPlayerController, 3, colorizedBuf.c_str(), nullptr, nullptr, nullptr, nullptr);
	});
}

void UtilsApi::PrintToConsole(int iSlot, const char *msg, ...)
{
	va_list args;
	va_start(args, msg);

	char buf[512];
	V_vsnprintf(buf, sizeof(buf), msg, args);
	va_end(args);

	engine->ClientPrintf(iSlot, buf);
}

void UtilsApi::PrintToConsoleAll(const char *msg, ...)
{
	va_list args;
	va_start(args, msg);

	char buf[512];
	V_vsnprintf(buf, sizeof(buf), msg, args);
	va_end(args);

	UTIL_ClientPrintAll(2, buf, nullptr, nullptr, nullptr, nullptr);
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

	g_pUtilsApi->NextFrame([pPlayerController, buf](){
		if(pPlayerController->m_hPawn() && pPlayerController->m_steamID() > 0)
			UTIL_ClientPrint(pPlayerController, 4, buf, nullptr, nullptr, nullptr, nullptr);
	});
}

void UtilsApi::PrintToCenterAll(const char *msg, ...)
{
	va_list args;
	va_start(args, msg);

	char buf[512];
	V_vsnprintf(buf, sizeof(buf), msg, args);
	va_end(args);

	UTIL_ClientPrintAll(4, buf, nullptr, nullptr, nullptr, nullptr);
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
	new CTimer(0.f, [iEnd, iSlot, buf]()
	{
		IGameEvent* pEvent = gameeventmanager->CreateEvent("show_survival_respawn_status");
		if(!pEvent) return -1.0f;
		pEvent->SetString("loc_token", buf);
		pEvent->SetInt("duration", 5);
		pEvent->SetInt("userid", iSlot);
		gameeventmanager->FireEvent(pEvent);
		if((iEnd - std::time(0)) > 0)
		{
			return 0.f;
		}
		// gameeventmanager->FreeEvent(pEvent);
		return -1.0f;
	});
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
	pEvent->SetInt("duration", 5);
	pEvent->SetInt("userid", -1);
	new CTimer(0.f, [pEvent, iEnd]()
	{
		gameeventmanager->FireEvent(pEvent);
		if(std::time(0) >= iEnd)
			return 0.f;
		gameeventmanager->FreeEvent(pEvent);
		return -1.0f;
	});
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
			static const auto datatable_hash = hash_32_fnv1a_const(sClassName);
			static const auto prop_hash = hash_32_fnv1a_const(sFieldName);
			int offset = schema::GetOffset(sClassName, datatable_hash, sFieldName, prop_hash).offset;
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
			CEntity->m_lastNetworkChange() = gpGlobals->curtime;
			CEntity->m_isSteadyState().ClearAll();
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
			CEntity->m_lastNetworkChange() = gpGlobals->curtime;
			CEntity->m_isSteadyState().ClearAll();
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

///////////////////////////////////////
const char* Menus::GetLicense()
{
	return "GPL";
}

const char* Menus::GetVersion()
{
	return "1.4";
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
