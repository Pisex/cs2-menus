#pragma once

#include <map>
#include <string>
#include <vector>
#include <chrono>
#include <unordered_map>

struct SndOpEventGuid_t;
class CPhysicsQuery;

PLUGIN_GLOBALVARS();

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

extern CGlobalVars* gpGlobals;
extern IVEngineServer2* engine;
extern CCSGameRules* g_pGameRules;
extern CEntitySystem* g_pEntitySystem;
extern CPhysicsQuery* g_pGameTraceManager;
extern IGameEventSystem* g_gameEventSystem;
extern IGameEventManager2* gameeventmanager;
extern CGameEntitySystem* g_pGameEntitySystem;
extern INetworkGameServer* g_pNetworkGameServer;

// ============================================================================

extern float g_flUniversalTime;
extern float g_flLastTickedTime;
extern bool  g_bHasTicked;

// ============================================================================

extern int g_iCommitSuicide;
extern int g_iRemoveWeapons;
extern int g_iChangeTeam;
extern int g_iCollisionRulesChanged;
extern int g_iTeleport;
extern int g_iRespawn;
extern int g_iDropWeapon;
extern int g_iOnTakeDamageAliveId;

// ============================================================================

extern KeyValues* g_hKVData;

extern char        szLanguage[16];
extern int         g_iMenuTypeDefault;
extern int         g_iMenuTime;
extern int         g_iDelayAuthFailKick;
extern bool        g_bMenuAddon;
extern const char* g_szMenuURL;
extern bool        g_bMenuFlashFix;
extern bool        g_bAccessUserChangeType;
extern bool        g_bStopingUser;
extern bool        g_bPanoramaMenu;
extern int         g_iTimeoutMenu;
extern int         g_iSoundType;
extern std::string g_szServerID;
extern bool        g_bAllowDisableNotify;
extern bool        g_bNotifyDisabledDefault;
extern std::string g_szSettingsCommand;

extern std::map<std::string, std::string> g_mapSounds;
extern std::vector<std::string>           g_vCommandEater;
extern std::vector<std::string>           g_mapPrecache;

// ============================================================================

extern int                       g_iMenuType[64];
extern int                       g_iMenuItem[64];
extern std::chrono::milliseconds g_iMenuLastButtonInput[64];
extern MenuPlayer                g_MenuPlayer[64];
extern std::string               g_TextMenuPlayer[64];
extern bool                      g_bNotifyDisabled[64];
extern std::string               g_szMenuDesc[64];
extern std::vector<ItemExtra>    g_vItemExtra[64];
extern std::unordered_map<std::string, CHandle<CCSCustomHudLayout>> g_mapHudLayouts[64];
// Глобальные лейауты: одна сущность на всех игроков (ключ - имя лейаута).
extern std::unordered_map<std::string, CHandle<CCSCustomHudLayout>> g_mapGlobalHudLayouts;

// ============================================================================

extern std::map<std::string, std::string>                    g_vecPhrases;
extern std::map<std::string, std::map<std::string, int>>     g_Offsets;
extern std::map<std::string, std::map<std::string, int>>     g_ChainOffsets;
extern std::unordered_map<const Menu*, std::string>          g_mapMenuDesc;
extern std::unordered_map<const Menu*, std::vector<ItemExtra>> g_mapItemExtra;
extern std::map<int, std::vector<int>>                       g_mapTransmitState;

// ============================================================================

extern std::map<int, std::map<std::string, CommandCallback>> ConsoleCommands;
extern std::map<int, std::map<std::string, CommandCallback>> ChatCommands;

// ============================================================================

class MenusApi;
class UtilsApi;
class PlayersApi;
class LayoutApi;
class Player;

extern MenusApi*    g_pMenusApi;
extern IMenusApi*   g_pMenusCore;
extern UtilsApi*    g_pUtilsApi;
extern IUtilsApi*   g_pUtilsCore;
extern PlayersApi*  g_pPlayersApi;
extern IPlayersApi* g_pPlayersCore;
extern LayoutApi*   g_pLayoutApi;
extern ILayoutApi*  g_pLayoutCore;
extern ICookiesApi* g_pCookies;

extern Player* m_Players[64];

// ============================================================================

extern void (*UTIL_Remove)(CEntityInstance*);
extern int (*UTIL_TakeDamage)(CCSPlayer_DamageReactServices*, CTakeDamageInfo*);
extern bool (*UTIL_IsHearingClient)(void* serverClient, int index);
extern void (*UTIL_Say)(const CCommandContext& ctx, CCommand& args);
extern void (*UTIL_SetModel)(CBaseModelEntity*, const char* szModel);
extern void (*UTIL_DispatchSpawn)(CEntityInstance*, CEntityKeyValues*);
extern void (*UTIL_SayTeam)(const CCommandContext& ctx, CCommand& args);
extern void (*UTIL_SwitchTeam)(CCSPlayerController* pPlayer, int iTeam);
extern void (*UTIL_StopSoundEvent)(CBaseEntity *pEntity, const char *pszSound);
extern void (*UTIL_RespawnPlayer)(CBasePlayerController* pController, CCSPlayerPawn* pPawn, bool a3, bool a4, bool a5, bool a6);
extern IGameEventListener2* (*UTIL_GetLegacyGameEventListener)(CPlayerSlot slot);
extern CBaseEntity* (*UTIL_CreateEntity)(const char *pClassName, CEntityIndex iForceEdictIndex);
extern void (*UTIL_SetMoveType)(CBaseEntity *pThis, MoveType_t nMoveType, MoveCollide_t nMoveCollide);
extern SndOpEventGuid_t (*UTIL_EmitSoundFilter)(uint8_t unk1[32], IRecipientFilter& filter, CEntityIndex ent, const EmitSound_t& params);
extern void (*UTIL_AcceptInput)(CEntityInstance* pThis, const char* pInputName, CEntityInstance* pActivator, CEntityInstance* pCaller, variant_t& pValue);
extern bool (*UTIL_TraceShape)(CPhysicsQuery*, const Ray_t* ray, const Vector* start, const Vector* end, CTraceFilter* filter, trace_t* trace);
extern void (*UTIL_TerminateRound)(CGameRules* pGameRules, float delay, unsigned int reason, int64 teamid);

// ============================================================================

CGameEntitySystem* GameEntitySystem();
std::string Colorizer(std::string str);
bool containsOnlyDigits(const std::string& str);
std::vector<std::string> SplitStringBySpace(const std::string& input);
int  UTIL_SanitizeMenuType(int iType);
ItemExtra& UTIL_PushItemExtra(Menu& hMenu);
ItemExtra* UTIL_ResolveItemExtra(const ItemRef& ref);

int Hook_TakeDamage(CCSPlayer_DamageReactServices* pService, CTakeDamageInfo* info);

int  CheckActionMenu(int iSlot, CCSPlayerController* pController, int iButton);
void UTIL_EnsureLayout(int iSlot);
void UTIL_DestroyLayout(int iSlot);
void UTIL_ResetPlayerSlot(int iSlot);
void UTIL_HandleLayoutClick(int iSlot, const char* szLayoutName, const char* szButton);
