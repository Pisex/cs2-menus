#include <stdio.h>
#include "menus.h"
#include "metamod_oslink.h"

Menus g_Menus;
PLUGIN_EXPOSE(Menus, g_Menus);
IVEngineServer2* engine = nullptr;
IGameResourceServiceServer* g_pGameResourceService = nullptr;
CGameEntitySystem* g_pGameEntitySystem = nullptr;
CEntitySystem* g_pEntitySystem = nullptr;
CSchemaSystem* g_pCSchemaSystem = nullptr;

std::map<uint32, MenuPlayer> g_MenuPlayer;

MenusApi* g_pMenusApi = nullptr;
IMenusApi* g_pMenusCore = nullptr;

class GameSessionConfiguration_t { };
SH_DECL_HOOK3_void(ICvar, DispatchConCommand, SH_NOATTRIB, 0, ConCommandHandle, const CCommandContext&, const CCommand&);
SH_DECL_HOOK5_void(IServerGameClients, ClientDisconnect, SH_NOATTRIB, 0, CPlayerSlot, int, const char *, uint64, const char *);
SH_DECL_HOOK3_void(INetworkServerService, StartupServer, SH_NOATTRIB, 0, const GameSessionConfiguration_t&, ISource2WorldSession*, const char*);

void (*UTIL_ClientPrint)(CBasePlayerController *player, int msg_dest, const char* msg_name, const char* param1, const char* param2, const char* param3, const char* param4) = nullptr;

bool containsOnlyDigits(const std::string& str) {
	return str.find_first_not_of("0123456789") == std::string::npos;
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

void ClientPrint(CBasePlayerController *player, int hud_dest, const char *msg, ...)
{
	va_list args;
	va_start(args, msg);

	char buf[256];
	V_vsnprintf(buf, sizeof(buf), msg, args);
	va_end(args);
		
	std::string colorizedBuf = Colorizer(buf);

	if (player)
		UTIL_ClientPrint(player, hud_dest, colorizedBuf.c_str(), nullptr, nullptr, nullptr, nullptr);
	else
		ConMsg("%s\n", buf);
}

void* Menus::OnMetamodQuery(const char* iface, int* ret)
{
	if (!strcmp(iface, Menus_INTERFACE))
	{
		*ret = META_IFACE_OK;
		return g_pMenusCore;
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
	
	g_SMAPI->AddListener( this, this );

	SH_ADD_HOOK(INetworkServerService, StartupServer, g_pNetworkServerService, SH_MEMBER(this, &Menus::StartupServer), true);
	SH_ADD_HOOK_MEMFUNC(IServerGameClients, ClientDisconnect, g_pSource2GameClients, this, &Menus::OnClientDisconnect, true);
	SH_ADD_HOOK_MEMFUNC(ICvar, DispatchConCommand, g_pCVar, this, &Menus::OnDispatchConCommand, false);

	g_pMenusApi = new MenusApi();
	g_pMenusCore = g_pMenusApi;

	return true;
}

bool Menus::Unload(char *error, size_t maxlen)
{
	SH_REMOVE_HOOK(INetworkServerService, StartupServer, g_pNetworkServerService, SH_MEMBER(this, &Menus::StartupServer), true);
	SH_REMOVE_HOOK_MEMFUNC(IServerGameClients, ClientDisconnect, g_pSource2GameClients, this, &Menus::OnClientDisconnect, true);
	SH_REMOVE_HOOK_MEMFUNC(ICvar, DispatchConCommand, g_pCVar, this, &Menus::OnDispatchConCommand, false);

	ConVar_Unregister();
	
	return true;
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
			char *pszMessage = (char *)(args.ArgS() + 2);
			CCommand arg;
			arg.Tokenize(args.ArgS() + 2);
			if(arg[0][0])
			{
				if(containsOnlyDigits(std::string(arg[0])))
				{
					auto& hMenuPlayer = g_MenuPlayer[pController->m_steamID()];
					auto& hMenu = hMenuPlayer.hMenu;
					if(hMenuPlayer.bEnabled)
					{
						int iButton = std::stoi(arg[0]);
						if(iButton == 9 && hMenu.bExit)
						{
							hMenuPlayer.iList = 0;
							hMenuPlayer.bEnabled = false;
							hMenuPlayer.hMenu = Menu();
							for (size_t i = 0; i < 8; i++)
							{
								ClientPrint(pController, 5, " \x08-\x01");
							}
							if(hMenu.hFunc) hMenu.hFunc("exit", "exit", 9);
						}
						else if(iButton == 8)
						{
							int iItems = size(hMenu.hItems) / 5;
							if (size(hMenu.hItems) % 5 > 0) iItems++;
							if(iItems > hMenuPlayer.iList+1)
							{
								hMenuPlayer.iList++;
								g_pMenusCore->DisplayPlayerMenu(hMenu, iCommandPlayerSlot.Get());
								if(hMenu.hFunc) hMenu.hFunc("next", "next", 8);
							}
						}
						else if(iButton == 7)
						{
							if(hMenuPlayer.iList != 0)
							{
								hMenuPlayer.iList--;
								g_pMenusCore->DisplayPlayerMenu(hMenu, iCommandPlayerSlot.Get());
								if(hMenu.hFunc) hMenu.hFunc("back", "back", 7);
							}
						}
						else
						{
							int iItem = hMenuPlayer.iList*5+iButton-1;
							if(hMenu.hItems.size() <= iItem) return;
							if(hMenu.hItems[iItem].iType != 1) return;
							if(hMenu.hFunc) hMenu.hFunc(hMenu.hItems[iItem].sBack.c_str(), hMenu.hItems[iItem].sText.c_str(), iItem);
						}
					}
				}
			}

			RETURN_META(MRES_SUPERCEDE);
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
		bDone = true;
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

void MenusApi::DisplayPlayerMenu(Menu& hMenu, int iSlot)
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
	}
	if(!hMenuPlayer.bEnabled)
	{
		hMenuPlayer.bEnabled = true;
		hMenuPlayer.hMenu = hMenu;
	}
	char sBuff[64] = "\0";
	int iCount = 0;
	int iItems = size(hMenu.hItems) / 5;
	if (size(hMenu.hItems) % 5 > 0) iItems++;
	ClientPrint(pPlayer, 5, hMenu.szTitle.c_str());
	for (size_t l = hMenuPlayer.iList*5; l < hMenu.hItems.size(); ++l) {
		switch (hMenu.hItems[l].iType)
		{
			case 1:
				g_SMAPI->Format(sBuff, sizeof(sBuff), " \x04[!%i]\x01 %s", iCount+1, hMenu.hItems[l].sText.c_str());
				ClientPrint(pPlayer, 5, sBuff);
				break;
			case 2:
				g_SMAPI->Format(sBuff, sizeof(sBuff), " \x08[!%i]\x01 %s", iCount+1, hMenu.hItems[l].sText.c_str());
				ClientPrint(pPlayer, 5, sBuff);
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
					ClientPrint(pPlayer, 5, " \x08-\x01");
				}
			}
			if(hMenuPlayer.iList > 0 || hMenu.bBack) ClientPrint(pPlayer, 5, " \x08[!7]\x01 ← Назад");
			if(iItems > hMenuPlayer.iList+1) ClientPrint(pPlayer, 5, "  \x08[!8]\x01 → Вперёд");
			ClientPrint(pPlayer, 5, "  \x07[!9]\x01 Выход");
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
	return "Menus";
}

const char* Menus::GetAuthor()
{
	return "Pisex";
}

const char* Menus::GetDescription()
{
	return "[Menus] Core";
}

const char* Menus::GetName()
{
	return "[Menus] Core";
}

const char* Menus::GetURL()
{
	return "https://discord.gg/g798xERK5Y";
}
