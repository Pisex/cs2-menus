#include <stdio.h>
#include "utils.h"
#include "metamod_oslink.h"

Menus g_Menus;
PLUGIN_EXPOSE(Menus, g_Menus);

CGlobalVars* gpGlobals = nullptr;
IVEngineServer2* engine = nullptr;
CCSGameRules* g_pGameRules = nullptr;
CEntitySystem* g_pEntitySystem = nullptr;
CSchemaSystem* g_pCSchemaSystem = nullptr;
IGameEventManager2* gameeventmanager = nullptr;
CGameEntitySystem* g_pGameEntitySystem = nullptr;
INetworkGameServer* g_pNetworkGameServer = nullptr;
IGameResourceServiceServer* g_pGameResourceService = nullptr;

std::map<uint32, MenuPlayer> g_MenuPlayer;

std::map<std::string, std::string> g_vecPhrases;

MenusApi* g_pMenusApi = nullptr;
IMenusApi* g_pMenusCore = nullptr;

UtilsApi* g_pUtilsApi = nullptr;
IUtilsApi* g_pUtilsCore = nullptr;

char szLanguage[16];

class GameSessionConfiguration_t { };
SH_DECL_HOOK3_void(IServerGameDLL, GameFrame, SH_NOATTRIB, 0, bool, bool, bool);
SH_DECL_HOOK2(IGameEventManager2, FireEvent, SH_NOATTRIB, 0, bool, IGameEvent*, bool);
SH_DECL_HOOK2_void(IServerGameClients, ClientCommand, SH_NOATTRIB, 0, CPlayerSlot, const CCommand&);
SH_DECL_HOOK3_void(ICvar, DispatchConCommand, SH_NOATTRIB, 0, ConCommandHandle, const CCommandContext&, const CCommand&);
SH_DECL_HOOK5_void(IServerGameClients, ClientDisconnect, SH_NOATTRIB, 0, CPlayerSlot, int, const char *, uint64, const char *);
SH_DECL_HOOK3_void(INetworkServerService, StartupServer, SH_NOATTRIB, 0, const GameSessionConfiguration_t&, ISource2WorldSession*, const char*);

void (*UTIL_ClientPrint)(CBasePlayerController *player, int msg_dest, const char* msg_name, const char* param1, const char* param2, const char* param3, const char* param4) = nullptr;
void (*UTIL_ClientPrintAll)(int msg_dest, const char* msg_name, const char* param1, const char* param2, const char* param3, const char* param4) = nullptr;

void (*UTIL_StateChanged)(CNetworkTransmitComponent& networkTransmitComponent, CEntityInstance *ent, int64 offset, int16 a4, int16 a5) = nullptr;
void (*UTIL_NetworkStateChanged)(int64 chainEntity, int64 offset, int64 a3) = nullptr;

void (*UTIL_Say)(const CCommandContext& ctx, CCommand& args) = nullptr;
void (*UTIL_SayTeam)(const CCommandContext& ctx, CCommand& args) = nullptr;

funchook_t* m_SayHook;
funchook_t* m_SayTeamHook;

bool containsOnlyDigits(const std::string& str) {
	return str.find_first_not_of("0123456789") == std::string::npos;
}

void SayTeamHook(const CCommandContext& ctx, CCommand& args)
{
	bool bCallback = true;
	bCallback = g_pUtilsApi->SendChatListenerCallback(ctx.GetPlayerSlot().Get(), args.ArgS());
	if(args[1][0])
	{
		if(bCallback && containsOnlyDigits(std::string(args[1] + 1)))
			bCallback = false;
	}
	if(bCallback)
	{
		UTIL_SayTeam(ctx, args);
	}
}

void SayHook(const CCommandContext& ctx, CCommand& args)
{
	bool bCallback = true;
	bCallback = g_pUtilsApi->SendChatListenerCallback(ctx.GetPlayerSlot().Get(), args.ArgS());
	if(args[1][0])
	{
		if(bCallback && containsOnlyDigits(std::string(args[1] + 1)))
			bCallback = false;
	}
	if(bCallback)
	{
		UTIL_Say(ctx, args);
	}
}

std::string Colorizer(std::string str)
{
	for (int i = 0; i < std::size(colors_hex); i++)
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

	*ret = META_IFACE_FAILED;
	return nullptr;
}

bool Menus::Load(PluginId id, ISmmAPI* ismm, char* error, size_t maxlen, bool late)
{
	PLUGIN_SAVEVARS();

	GET_V_IFACE_CURRENT(GetEngineFactory, g_pCVar, ICvar, CVAR_INTERFACE_VERSION);
	GET_V_IFACE_ANY(GetEngineFactory, g_pCSchemaSystem, CSchemaSystem, SCHEMASYSTEM_INTERFACE_VERSION);
	GET_V_IFACE_CURRENT(GetEngineFactory, engine, IVEngineServer2, SOURCE2ENGINETOSERVER_INTERFACE_VERSION);
	GET_V_IFACE_CURRENT(GetFileSystemFactory, g_pFullFileSystem, IFileSystem, FILESYSTEM_INTERFACE_VERSION);
	GET_V_IFACE_CURRENT(GetServerFactory, g_pSource2Server, ISource2Server, SOURCE2SERVER_INTERFACE_VERSION);
	GET_V_IFACE_ANY(GetServerFactory, g_pSource2GameClients, IServerGameClients, SOURCE2GAMECLIENTS_INTERFACE_VERSION);
	GET_V_IFACE_CURRENT(GetEngineFactory, g_pNetworkServerService, INetworkServerService, NETWORKSERVERSERVICE_INTERFACE_VERSION);
	GET_V_IFACE_CURRENT(GetEngineFactory, g_pGameResourceService, IGameResourceServiceServer, GAMERESOURCESERVICESERVER_INTERFACE_VERSION);

	CModule libserver(g_pSource2Server);
	UTIL_ClientPrint = libserver.FindPatternSIMD(WIN_LINUX("48 85 C9 0F 84 2A 2A 2A 2A 48 8B C4 48 89 58 18", "55 48 89 E5 41 57 49 89 CF 41 56 49 89 D6 41 55 41 89 F5 41 54 4C 8D A5 A0 FE FF FF")).RCast< decltype(UTIL_ClientPrint) >();
	if (!UTIL_ClientPrint)
	{
		V_strncpy(error, "Failed to find function to get UTIL_ClientPrint", maxlen);
		ConColorMsg(Color(255, 0, 0, 255), "[%s] %s\n", GetLogTag(), error);
		std::string sBuffer = "meta unload "+std::to_string(g_PLID);
		engine->ServerCommand(sBuffer.c_str());
		return false;
	}
	
	UTIL_ClientPrintAll = libserver.FindPatternSIMD(WIN_LINUX("48 89 5C 24 08 48 89 6C 24 10 48 89 74 24 18 57 48 81 EC 70 01 2A 2A 8B E9", "55 48 89 E5 41 57 49 89 D7 41 56 49 89 F6 41 55 41 89 FD")).RCast< decltype(UTIL_ClientPrintAll) >();
	if (!UTIL_ClientPrintAll)
	{
		V_strncpy(error, "Failed to find function to get UTIL_ClientPrintAll", sizeof(error));
		ConColorMsg(Color(255, 0, 0, 255), "[%s] %s\n", GetLogTag(), error);
		std::string sBuffer = "meta unload "+std::to_string(g_PLID);
		engine->ServerCommand(sBuffer.c_str());
		return false;
	}

	UTIL_SayTeam = libserver.FindPatternSIMD("55 48 89 E5 41 56 41 55 49 89 F5 41 54 49 89 FC 53 48 83 EC 10 48 8D 05").RCast< decltype(UTIL_SayTeam) >();
	if (!UTIL_SayTeam)
	{
		V_strncpy(error, "Failed to find function to get UTIL_SayTeam", sizeof(error));
		ConColorMsg(Color(255, 0, 0, 255), "[%s] %s\n", GetLogTag(), error);
		std::string sBuffer = "meta unload "+std::to_string(g_PLID);
		engine->ServerCommand(sBuffer.c_str());
		return false;
	}
	m_SayTeamHook = funchook_create();
	funchook_prepare(m_SayTeamHook, (void**)&UTIL_SayTeam, (void*)SayTeamHook);
	funchook_install(m_SayTeamHook, 0);

	UTIL_Say = libserver.FindPatternSIMD("55 48 89 E5 41 56 41 55 49 89 F5 41 54 49 89 FC 53 48 83 EC 10 48 8D 05").RCast< decltype(UTIL_Say) >();
	if (!UTIL_Say)
	{
		V_strncpy(error, "Failed to find function to get UTIL_Say", sizeof(error));
		ConColorMsg(Color(255, 0, 0, 255), "[%s] %s\n", GetLogTag(), error);
		std::string sBuffer = "meta unload "+std::to_string(g_PLID);
		engine->ServerCommand(sBuffer.c_str());
		return false;
	}
	m_SayHook = funchook_create();
	funchook_prepare(m_SayHook, (void**)&UTIL_Say, (void*)SayHook);
	funchook_install(m_SayHook, 0);
	
	UTIL_StateChanged = libserver.FindPatternSIMD("55 48 89 E5 41 57 41 56 41 55 41 54 53 89 D3").RCast< decltype(UTIL_StateChanged) >();
	if (!UTIL_StateChanged)
	{
		V_strncpy(error, "Failed to find function to get UTIL_StateChanged", maxlen);
		ConColorMsg(Color(255, 0, 0, 255), "[%s] %s\n", GetLogTag(), error);
		std::string sBuffer = "meta unload "+std::to_string(g_PLID);
		engine->ServerCommand(sBuffer.c_str());
		return false;
	}

	UTIL_NetworkStateChanged = libserver.FindPatternSIMD("83 FF 07 0F 87 ? ? ? ? 55 89 FF 48 89 E5 41 56 41 55 41 54 49 89 F4 53 48 89 D3 48 8D 15 E5 C2 20 00").RCast< decltype(UTIL_NetworkStateChanged) >();
	if (!UTIL_NetworkStateChanged)
	{
		V_strncpy(error, "Failed to find function to get UTIL_NetworkStateChanged", maxlen);
		ConColorMsg(Color(255, 0, 0, 255), "[%s] %s\n", GetLogTag(), error);
		std::string sBuffer = "meta unload "+std::to_string(g_PLID);
		engine->ServerCommand(sBuffer.c_str());
		return false;
	}

	g_SMAPI->AddListener( this, this );

	gameeventmanager = static_cast<IGameEventManager2*>(CallVFunc<IToolGameEventAPI*, 91>(g_pSource2Server));
	SH_ADD_HOOK(INetworkServerService, StartupServer, g_pNetworkServerService, SH_MEMBER(this, &Menus::StartupServer), true);
	SH_ADD_HOOK_MEMFUNC(IServerGameClients, ClientDisconnect, g_pSource2GameClients, this, &Menus::OnClientDisconnect, true);
	SH_ADD_HOOK_MEMFUNC(ICvar, DispatchConCommand, g_pCVar, this, &Menus::OnDispatchConCommand, false);
	SH_ADD_HOOK(IServerGameDLL, GameFrame, g_pSource2Server, SH_MEMBER(this, &Menus::GameFrame), true);
	SH_ADD_HOOK(IServerGameClients, ClientCommand, g_pSource2GameClients, SH_MEMBER(this, &Menus::ClientCommand), false);
	SH_ADD_HOOK(IGameEventManager2, FireEvent, gameeventmanager, SH_MEMBER(this, &Menus::FireEvent), false);

	if (late)
	{
		g_pGameEntitySystem = *reinterpret_cast<CGameEntitySystem**>(reinterpret_cast<uintptr_t>(g_pGameResourceService) + WIN_LINUX(0x58, 0x50));
		g_pEntitySystem = g_pGameEntitySystem;
		g_pNetworkGameServer = g_pNetworkServerService->GetIGameServer();
		gpGlobals = g_pNetworkGameServer->GetGlobals();
	}

	KeyValues::AutoDelete g_kvCore("Core");
	const char *pszPath = "addons/configs/core.cfg";

	if (!g_kvCore->LoadFromFile(g_pFullFileSystem, pszPath))
	{
		Warning("Failed to load %s\n", pszPath);
		return false;
	}

	g_SMAPI->Format(szLanguage, sizeof(szLanguage), "%s", g_kvCore->GetString("ServerLang", "en"));
	
	KeyValues::AutoDelete g_kvPhrasesRanks("Phrases");
	const char *pszPath2 = "addons/translations/menus.phrases.txt";

	if (!g_kvPhrasesRanks->LoadFromFile(g_pFullFileSystem, pszPath2))
	{
		Warning("Failed to load %s\n", pszPath2);
		return false;
	}

	for (KeyValues *pKey = g_kvPhrasesRanks->GetFirstTrueSubKey(); pKey; pKey = pKey->GetNextTrueSubKey())
		g_vecPhrases[std::string(pKey->GetName())] = std::string(pKey->GetString(szLanguage));

	g_pMenusApi = new MenusApi();
	g_pMenusCore = g_pMenusApi;

	g_pUtilsApi = new UtilsApi();
	g_pUtilsCore = g_pUtilsApi;

	return true;
}

bool Menus::Unload(char *error, size_t maxlen)
{
	SH_REMOVE_HOOK_MEMFUNC(ICvar, DispatchConCommand, g_pCVar, this, &Menus::OnDispatchConCommand, false);
	SH_REMOVE_HOOK(IServerGameDLL, GameFrame, g_pSource2Server, SH_MEMBER(this, &Menus::GameFrame), true);
	SH_REMOVE_HOOK(IGameEventManager2, FireEvent, gameeventmanager, SH_MEMBER(this, &Menus::FireEvent), false);
	SH_REMOVE_HOOK(IServerGameClients, ClientCommand, g_pSource2GameClients, SH_MEMBER(this, &Menus::ClientCommand), false);
	SH_REMOVE_HOOK_MEMFUNC(IServerGameClients, ClientDisconnect, g_pSource2GameClients, this, &Menus::OnClientDisconnect, true);
	SH_REMOVE_HOOK(INetworkServerService, StartupServer, g_pNetworkServerService, SH_MEMBER(this, &Menus::StartupServer), true);

	funchook_destroy(m_SayHook);
	funchook_destroy(m_SayTeamHook);

	ConVar_Unregister();
	
	return true;
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
}

void Menus::ClientCommand(CPlayerSlot slot, const CCommand &args)
{
	bool bFound = g_pUtilsApi->FindAndSendCommandCallback(args.Arg(0), slot.Get(), args.ArgS());
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
		auto pController = (CCSPlayerController*)g_pEntitySystem->GetBaseEntity((CEntityIndex)(iCommandPlayerSlot.Get() + 1));
		bool bCommand = *args[1] == '!' || *args[1] == '/';
		bool bSilent = *args[1] == '/';

		if (bCommand)
		{
			if(args[1][0])
			{
				if(containsOnlyDigits(std::string(args.Arg(1)+1)))
				{
					auto& hMenuPlayer = g_MenuPlayer[pController->m_steamID()];
					auto& hMenu = hMenuPlayer.hMenu;
					if(hMenuPlayer.bEnabled)
					{
						int iButton = std::stoi(args[1]+1);
						if(iButton == 9 && hMenu.bExit)
						{
							hMenuPlayer.iList = 0;
							hMenuPlayer.bEnabled = false;
							for (size_t i = 0; i < 8; i++)
							{
								g_pUtilsCore->PrintToChat(iCommandPlayerSlot.Get(), " \x08-\x01");
							}
							if(hMenu.hFunc) hMenu.hFunc("exit", "exit", 9, iCommandPlayerSlot.Get());
							hMenuPlayer.hMenu = Menu();
						}
						else if(iButton == 8)
						{
							int iItems = size(hMenu.hItems) / 5;
							if (size(hMenu.hItems) % 5 > 0) iItems++;
							if(iItems > hMenuPlayer.iList+1)
							{
								hMenuPlayer.iList++;
								g_pMenusCore->DisplayPlayerMenu(hMenu, iCommandPlayerSlot.Get(), false);
								if(hMenu.hFunc) hMenu.hFunc("next", "next", 8, iCommandPlayerSlot.Get());
							}
						}
						else if(iButton == 7)
						{
							if(hMenuPlayer.iList != 0 || hMenuPlayer.hMenu.bBack)
							{
								if(hMenuPlayer.iList != 0)
								{
									hMenuPlayer.iList--;
									g_pMenusCore->DisplayPlayerMenu(hMenu, iCommandPlayerSlot.Get(), false);
								}
								else if(hMenu.hFunc) hMenu.hFunc("back", "back", 7, iCommandPlayerSlot.Get());
							}
						}
						else
						{
							int iItem = hMenuPlayer.iList*5+iButton-1;
							if(hMenu.hItems.size() <= iItem) RETURN_META(MRES_SUPERCEDE);
							if(hMenu.hItems[iItem].iType != 1) RETURN_META(MRES_SUPERCEDE);
							if(hMenu.hFunc) hMenu.hFunc(hMenu.hItems[iItem].sBack.c_str(), hMenu.hItems[iItem].sText.c_str(), iButton, iCommandPlayerSlot.Get());
						}
						RETURN_META(MRES_SUPERCEDE);
					}
				}
			}
		}

		if(args[1][0])
		{
			char *pszMessage = (char *)(args.ArgS() + 1);
			CCommand arg;
			arg.Tokenize(pszMessage);
			g_pUtilsApi->FindAndSendCommandCallback(arg[0], ctx.GetPlayerSlot().Get(), pszMessage);
		}
	}
	SH_CALL(g_pCVar, &ICvar::DispatchConCommand)(cmdHandle, ctx, args);
}

void Menus::StartupServer(const GameSessionConfiguration_t& config, ISource2WorldSession*, const char*)
{
	static bool bDone = false;
	if (!bDone)
	{
		g_pGameEntitySystem = *reinterpret_cast<CGameEntitySystem**>(reinterpret_cast<uintptr_t>(g_pGameResourceService) + WIN_LINUX(0x58, 0x50));
		g_pEntitySystem = g_pGameEntitySystem;

		g_pNetworkGameServer = g_pNetworkServerService->GetIGameServer();
		gpGlobals = g_pNetworkGameServer->GetGlobals();

		bDone = true;
		g_pUtilsApi->SendHookStartup();
	}
}

void Menus::OnClientDisconnect(CPlayerSlot slot, int reason, const char *pszName, uint64 xuid, const char *pszNetworkID)
{
	if (xuid == 0)
    	return;

	CCSPlayerController* pPlayerController = static_cast<CCSPlayerController*>(g_pEntitySystem->GetBaseEntity(static_cast<CEntityIndex>(slot.Get() + 1)));
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
	CCSPlayerController* pPlayer = static_cast<CCSPlayerController*>(g_pEntitySystem->GetBaseEntity(static_cast<CEntityIndex>(iSlot + 1)));
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
	}
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
				for (size_t i = 0; i < iC-iCount; i++)
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

void MenusApi::AddItemMenu(Menu& hMenu, const char* sBack, const char* sText, int iType = 1)
{
	if(iType != 0)
	{
		Items hItem;
		hItem.iType = iType;
		hItem.sBack = std::string(sBack);
		hItem.sText = std::string(sText);
		hMenu.hItems.push_back(hItem);
	}
}

void MenusApi::ClosePlayerMenu(int iSlot)
{
	CCSPlayerController* pPlayer = static_cast<CCSPlayerController*>(g_pEntitySystem->GetBaseEntity(static_cast<CEntityIndex>(iSlot + 1)));
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

	CCSPlayerController* pPlayerController = static_cast<CCSPlayerController*>(g_pEntitySystem->GetBaseEntity(static_cast<CEntityIndex>(iSlot + 1)));
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

void UtilsApi::SetStateChanged(CBaseEntity* entity, const char* sClassName, const char* sFieldName, int extraOffset = 0)
{
	if(entity)
	{
		SC_CBaseEntity* CEntity = (SC_CBaseEntity*)entity;
		int offset = g_pCSchemaSystem->GetServerOffset(sClassName, sFieldName);
		
		int chainOffset = g_pCSchemaSystem->GetServerOffset(sClassName, "__m_pChainEntity");
		if (chainOffset != -1)
		{
			UTIL_NetworkStateChanged((uintptr_t)(CEntity) + chainOffset, offset, 0xFFFFFFFF);
			return;
		}
		UTIL_StateChanged(CEntity->m_NetworkTransmitComponent(), CEntity, offset, -1, -1);
		CEntity->m_lastNetworkChange() = gpGlobals->curtime;
		CEntity->m_isSteadyState().ClearAll();
	}
}

///////////////////////////////////////
const char* Menus::GetLicense()
{
	return "GPL";
}

const char* Menus::GetVersion()
{
	return "1.0";
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
