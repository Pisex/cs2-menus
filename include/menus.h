#pragma once

#include <functional>
#include <string>

class CBaseEntity;
class CBaseModelEntity;
class CEntityInstance;
class CEntityKeyValues;
class CSteamID;
class CGameEntitySystem;
class CEntitySystem;
class CGlobalVars;
class IGameEvent;
class IGameEventManager2;
struct CTakeDamageInfoContainer;
class CTakeDamageInfo;
class IGameEventListener2;

/////////////////////////////////////////////////////////////////
///////////////////////      PLAYERS     //////////////////////////
/////////////////////////////////////////////////////////////////

typedef std::function<void(int iSlot, uint64 iSteamID64)> OnClientAuthorizedCallback;

#define PLAYERS_INTERFACE "IPlayersApi"
class IPlayersApi
{
public:
    virtual bool IsFakeClient(int iSlot) = 0;
    virtual bool IsAuthenticated(int iSlot) = 0;
    virtual bool IsConnected(int iSlot) = 0;
    virtual bool IsInGame(int iSlot) = 0;
    virtual const char* GetIpAddress(int iSlot) = 0;
    virtual uint64 GetSteamID64(int iSlot) = 0;
    virtual const CSteamID* GetSteamID(int iSlot) = 0;

    virtual void HookOnClientAuthorized(SourceMM::PluginId id, OnClientAuthorizedCallback callback) = 0;

    virtual void CommitSuicide(int iSlot, bool bExplode, bool bForce) = 0;
    virtual void ChangeTeam(int iSlot, int iNewTeam) = 0;
    virtual void Teleport(int iSlot, const Vector *position, const QAngle *angles, const Vector *velocity) = 0;
    virtual void Respawn(int iSlot) = 0;
    virtual void DropWeapon(int iSlot, CBaseEntity* pWeapon, Vector* pVecTarget = nullptr, Vector* pVelocity = nullptr) = 0;
    virtual void SwitchTeam(int iSlot, int iNewTeam) = 0;
    virtual const char* GetPlayerName(int iSlot) = 0;
    virtual void SetPlayerName(int iSlot, const char* szName) = 0;
    virtual void SetMoveType(int iSlot, MoveType_t moveType) = 0;
    virtual void EmitSound(std::vector<int> vPlayers, CEntityIndex ent, std::string sound_name, int pitch, float volume) = 0;
	virtual void EmitSound(int iSlot, CEntityIndex ent, std::string sound_name, int pitch, float volume) = 0;
	virtual void StopSoundEvent(int iSlot, const char* sound_name) = 0;
    virtual IGameEventListener2* GetLegacyGameEventListener(int iSlot) = 0;
    virtual int FindPlayer(uint64 iSteamID64) = 0;
    virtual int FindPlayer(const CSteamID* steamID) = 0;
    virtual int FindPlayer(const char* szName) = 0;
    virtual trace_info_t RayTrace(int iSlot) = 0;
    virtual bool UseClientCommand(int iSlot, const char* szCommand) = 0;
};

/////////////////////////////////////////////////////////////////
///////////////////////      UTILS     //////////////////////////
/////////////////////////////////////////////////////////////////

class CCSGameRules;
class CTimer;

#define Utils_INTERFACE "IUtilsApi"

typedef std::function<bool(int iSlot, const char* szContent)> CommandCallback;
typedef std::function<bool(int iSlot, const char* szContent, bool bTeam)> CommandCallbackPre;
typedef std::function<bool(int iSlot, const char* szContent, bool bMute, bool bTeam)> CommandCallbackPost;
typedef std::function<void(const char* szName, IGameEvent* pEvent, bool bDontBroadcast)> EventCallback;
typedef std::function<void()> StartupCallback;
typedef std::function<bool(int iSlot, CTakeDamageInfoContainer *&pInfoContainer)> OnTakeDamageCallback;
typedef std::function<bool(int iSlot, CTakeDamageInfo *pInfo)> OnTakeDamagePreCallback;
typedef std::function<bool(int iSlot)> OnHearingClientCallback;
typedef std::function<void(const char* szMap)> MapStartCallback;

class IUtilsApi
{
public:
    virtual void PrintToChat(int iSlot, const char* msg, ...) = 0;
    virtual void PrintToChatAll(const char* msg, ...) = 0;
    virtual void NextFrame(std::function<void()> fn) = 0;
    virtual CCSGameRules* GetCCSGameRules() = 0;
    virtual CGameEntitySystem* GetCGameEntitySystem() = 0;
    virtual CEntitySystem* GetCEntitySystem() = 0;
	virtual CGlobalVars* GetCGlobalVars() = 0;
	virtual IGameEventManager2* GetGameEventManager() = 0;

    virtual const char* GetLanguage() = 0;

    virtual void StartupServer(SourceMM::PluginId id, StartupCallback fn) = 0;
    virtual void OnGetGameRules(SourceMM::PluginId id, StartupCallback fn) = 0;

    virtual void RegCommand(SourceMM::PluginId id, const std::vector<std::string> &console, const std::vector<std::string> &chat, const CommandCallback &callback) = 0;
    virtual void AddChatListenerPre(SourceMM::PluginId id, CommandCallbackPre callback) = 0;
    virtual void AddChatListenerPost(SourceMM::PluginId id, CommandCallbackPost callback) = 0;
    virtual void HookEvent(SourceMM::PluginId id, const char* sName, EventCallback callback) = 0;

    virtual void SetStateChanged(CBaseEntity* entity, const char* sClassName, const char* sFieldName, int extraOffset = 0) = 0;

    virtual void ClearAllHooks(SourceMM::PluginId id) = 0;

    virtual void LoadTranslations(const char* szFile) = 0;
	virtual void PrintToConsole(int iSlot, const char* msg, ...) = 0;
	virtual void PrintToConsoleAll(const char* msg, ...) = 0;
	virtual void PrintToCenter(int iSlot, const char* msg, ...) = 0;
	virtual void PrintToCenterAll(const char* msg, ...) = 0;
	virtual void PrintToCenterHtml(int iSlot, int iDuration, const char* msg, ...) = 0;
	virtual void PrintToCenterHtmlAll(int iDuration, const char* msg, ...) = 0;

    virtual void LogToFile(const char* szFile, const char* szText, ...) = 0;
    virtual void ErrorLog(const char* msg, ...) = 0;
    virtual void PrintToAlert(int iSlot, const char *msg, ...) = 0;
	virtual void PrintToAlertAll(const char *msg, ...) = 0;
    virtual void SetEntityModel(CBaseModelEntity* pEntity, const char* szModel) = 0;
    virtual void DispatchSpawn(CEntityInstance* pEntity, CEntityKeyValues*) = 0;
    virtual CBaseEntity* CreateEntityByName(const char *pClassName, CEntityIndex iForceEdictIndex) = 0;
    virtual void RemoveEntity(CEntityInstance* pEntity) = 0;
    virtual void AcceptEntityInput(CEntityInstance* pEntity, const char* szInputName, variant_t value = variant_t(""), CEntityInstance *pActivator = nullptr, CEntityInstance *pCaller = nullptr) = 0;
    virtual CTimer* CreateTimer(float flInterval, std::function<float()> func) = 0;
    virtual void RemoveTimer(CTimer* timer) = 0;
    virtual void HookOnTakeDamage(SourceMM::PluginId id, OnTakeDamageCallback callback) = 0;
    virtual void HookOnTakeDamagePre(SourceMM::PluginId id, OnTakeDamagePreCallback callback) = 0;
    virtual void CollisionRulesChanged(CBaseEntity* pEnt) = 0;
    virtual void TeleportEntity(CBaseEntity* pEnt, const Vector *position, const QAngle *angles, const Vector *velocity) = 0;
    virtual void HookIsHearingClient(SourceMM::PluginId id, OnHearingClientCallback callback) = 0;
    virtual const char* GetVersion() = 0;
    
    virtual void MapEndHook(SourceMM::PluginId id, StartupCallback fn) = 0;
    virtual void MapStartHook(SourceMM::PluginId id, MapStartCallback fn) = 0;
};

/////////////////////////////////////////////////////////////////
///////////////////////      MENUS     //////////////////////////
/////////////////////////////////////////////////////////////////

#define Menus_INTERFACE "IMenusApi"

#define ITEM_HIDE 0
#define ITEM_DEFAULT 1
#define ITEM_DISABLED 2

typedef std::function<void(const char* szBack, const char* szFront, int iItem, int iSlot)> MenuCallbackFunc;

struct Items
{
    int iType;
    std::string sBack;
    std::string sText;
};

struct Menu
{
    std::string szTitle;	
    std::vector<Items> hItems;
    bool bBack = false;
    bool bExit = false;
	MenuCallbackFunc hFunc = nullptr;

    void clear() {
        szTitle.clear();
        hItems.clear();
        bBack = false;
        bExit = false;
        hFunc = nullptr;
    }
};

struct MenuPlayer
{
    bool bEnabled;
    int iList;
    Menu hMenu;
    int iEnd;

    void clear() {
        bEnabled = false;
        iList = 0;
        hMenu.clear();
        iEnd = 0;
    }
};

class IMenusApi
{
public:
	virtual void AddItemMenu(Menu& hMenu, const char* sBack, const char* sText, int iType = 1) = 0;
	virtual void DisplayPlayerMenu(Menu& hMenu, int iSlot, bool bClose = true) = 0;
	virtual void SetExitMenu(Menu& hMenu, bool bExit) = 0;
	virtual void SetBackMenu(Menu& hMenu, bool bBack) = 0;
	virtual void SetTitleMenu(Menu& hMenu, const char* szTitle) = 0;
	virtual void SetCallback(Menu& hMenu, MenuCallbackFunc func) = 0;
    virtual void ClosePlayerMenu(int iSlot) = 0;
    virtual std::string escapeString(const std::string& input) = 0;
    virtual bool IsMenuOpen(int iSlot) = 0;
	virtual void DisplayPlayerMenu(Menu& hMenu, int iSlot, bool bClose = true, bool bReset = true) = 0;
};

/////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////