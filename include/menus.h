#pragma once

#include <functional>
#include <string>
#include <vector>

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
struct Menu;

struct trace_info_t;

#define PLAYERS_INTERFACE "IPlayersApi"
#define UTILS_INTERFACE "IUtilsApi"
#define LAYOUT_INTERFACE "ILayoutApi"
#define MENUS_INTERFACE "IMenusApi"

/////////////////////////////////////////////////////////////////
///////////////////////      PLAYERS     //////////////////////////
/////////////////////////////////////////////////////////////////

typedef std::function<void(int iSlot, uint64 iSteamID64)> OnClientAuthorizedCallback;

struct FakeConVar
{
    std::string szCvar;
    std::string szValue;
};

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
    // bHook - если true, то вызов будет через хук OnTakeDamage с возможностью отмены, если false - прямой вызов функции нанесения урона без хуков
    virtual void TakeDamage(int iSlot, CTakeDamageInfo* pInfo, bool bHook = true) = 0;
    virtual void RemoveWeapons(int iSlot) = 0;
    
    virtual void SetConVar(int iSlot, FakeConVar cvar) = 0;
    virtual void SetConVar(int iSlot, const char* name, const char* value) = 0;
    virtual void SetConVar(std::vector<int> vPlayers, const char* name, const char* value) = 0;
    virtual void SetConVar(std::vector<int> vPlayers, FakeConVar cvar) = 0;
    virtual void SetConVars(int iSlot, std::vector<FakeConVar> cvars) = 0;
    virtual void SetConVars(std::vector<int> vPlayers, std::vector<FakeConVar> cvars) = 0;
};

/////////////////////////////////////////////////////////////////
///////////////////////      UTILS     //////////////////////////
/////////////////////////////////////////////////////////////////

class CCSGameRules;
class CTimer;

typedef std::function<bool(int iSlot, const char* szContent)> CommandCallback;
typedef std::function<bool(int iSlot, const char* szContent, bool bTeam)> CommandCallbackPre;
typedef std::function<bool(int iSlot, const char* szContent, bool bMute, bool bTeam)> CommandCallbackPost;
typedef std::function<void(const char* szName, IGameEvent* pEvent, bool bDontBroadcast)> EventCallback;
typedef std::function<void()> StartupCallback;
typedef std::function<bool(int iSlot, CTakeDamageInfoContainer *&pInfoContainer)> OnTakeDamageCallback;
typedef std::function<bool(int iSlot, CTakeDamageInfo *pInfo)> OnTakeDamagePreCallback;
typedef std::function<bool(int iSlot)> OnHearingClientCallback;
typedef std::function<void(const char* szMap)> MapStartCallback;

// Вызывается при открытии меню настроек позволяет сторонним плагинам добавить свои пункты
typedef std::function<void(int iSlot, Menu& hMenu)> OnSettingsOpenCallback;
// Вызывается при выборе любого пункта меню настроек (аналог MenuCallbackFunc для CallbackMenu)
typedef std::function<void(const char* szBack, const char* szFront, int iItem, int iSlot)> OnSettingsItemCallback;

// Тип всплывающего уведомления (угловой тост)
enum class NotifyType : int
{
    SUCCESS = 0,
    WARNING = 1,
    ERROR   = 2,
};

// Угол/место экрана, в котором показывается тост
enum class NotifyPos : int
{
    TOP_RIGHT     = 0,
    TOP_LEFT      = 1,
    BOTTOM_RIGHT  = 2,
    BOTTOM_LEFT   = 3,
    TOP_CENTER    = 4,
    CENTER        = 5,
    BOTTOM_CENTER = 6,
    CENTER_LEFT   = 7,
    CENTER_RIGHT  = 8,
};

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

    virtual const char* GetServerID() = 0;

    // Если vecSlots пустой, то состояние будет установлено для всех игроков
    // bState = true - раскрывает энтити для игроков, bState = false - скрывает
    virtual void SetTransmitState(int iEntityIndex, bool bState, std::vector<int> vecSlots) = 0;

    // Показать всплывающее уведомление (панорама-тост) конкретному игроку.
    // iType: 0 = success, 1 = warning, 2 = error (см. NotifyType).
    // iPos: 0=сверху-справа, 1=сверху-слева, 2=снизу-справа, 3=снизу-слева,
    //   4=сверху-по-центру, 5=по-центру, 6=снизу-по-центру, 7=слева-по-центру,
    //   8=справа-по-центру (см. NotifyPos).
    // flDuration - сколько секунд висит до автоскрытия.
    // szChatFallback - если панорама выключена в конфиге (PanoramaMenu 0),
    //   этот текст будет отправлен в чат. Если nullptr/пусто - чат-фолбэк не используется.
    // ВНИМАНИЕ: методы добавлены в конец vtable ради ABI-совместимости, не переносить выше.
    virtual void ShowNotify(int iSlot, int iType, const char* szTitle, const char* szText, float flDuration = 5.0f, int iPos = 0, const char* szChatFallback = nullptr) = 0;

    // То же самое, но для всех игроков на сервере.
    virtual void ShowNotifyAll(int iType, const char* szTitle, const char* szText, float flDuration = 5.0f, int iPos = 0, const char* szChatFallback = nullptr) = 0;

    virtual void OpenSettingsMenu(int iSlot) = 0;
    virtual void HookOnSettingsOpen(SourceMM::PluginId id, OnSettingsOpenCallback callback) = 0;
    virtual void HookOnSettingsItem(SourceMM::PluginId id, OnSettingsItemCallback callback) = 0;
};

/////////////////////////////////////////////////////////////////
///////////////////////      MENUS     //////////////////////////
/////////////////////////////////////////////////////////////////

#define ITEM_HIDE 0
#define ITEM_DEFAULT 1
#define ITEM_DISABLED 2

// Вид итема. Обычные меню (CHAT/CENTER/CENTER_WASD) используют только BUTTON.
enum class ItemKind : int
{
    BUTTON = 0, // обычная кнопка
    TOGGLE = 1, // переключатель вкл/выкл(checkbox), только для MenuType::HUD_LAYOUT
    SELECT = 2, // выпадающий список (дропдаун), только для MenuType::HUD_LAYOUT
};

typedef std::function<void(const char* szBack, const char* szFront, int iItem, int iSlot)> MenuCallbackFunc;

// Вызывается при переключении TOGGLE-итема. bState - новое состояние.
typedef std::function<void(const char* szBack, bool bState, int iItem, int iSlot)> MenuToggleCallbackFunc;
// Вызывается при выборе значения в SELECT-итеме. iOption - индекс выбранной опции.
typedef std::function<void(const char* szBack, const char* szOptionBack, int iOption, int iItem, int iSlot)> MenuSelectCallbackFunc;

struct SelectOption
{
    std::string sBack;
    std::string sText;
};

struct Menu;

struct ItemRef
{
    Menu* pMenu = nullptr;
    int iIndex = -1;
};

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

enum class MenuType : int
{
    CHAT = 0,
    CENTER = 1,
    CENTER_WASD = 2,
    HUD_LAYOUT = 3,
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
    virtual void AddRawItemMenu(Menu &hMenu, const char* sBack, const char* sText, int iType = 1) = 0;
    virtual MenuType GetMenuType(int iSlot) = 0;
    
    /////////////////////////////////////////////////////////////////
    // HUD_LAYOUT: toggle и select итемы.
    // Эти итемы отрисовываются только при MenuType::HUD_LAYOUT.
    // Для остальных типов меню они добавляются как обычные кнопки (BUTTON).
    /////////////////////////////////////////////////////////////////
    virtual void SetDescriptionMenu(Menu& hMenu, const char* szDescription) = 0;

    // Добавить итем-переключатель (toggle). bDefault - начальное состояние.
    // func вызывается при переключении с новым состоянием.
    virtual void AddToggleMenu(Menu& hMenu, const char* sBack, const char* sText, bool bDefault = false, MenuToggleCallbackFunc func = nullptr, int iType = 1) = 0;

    // Добавить итем-дропдаун (select). Возвращает ссылку на созданный итем,
    // чтобы можно было наполнить его опциями через AddSelectOption.
    // iDefault - индекс выбранной по умолчанию опции.
    virtual ItemRef AddSelectMenu(Menu& hMenu, const char* sBack, const char* sText, int iDefault = 0, MenuSelectCallbackFunc func = nullptr, int iType = 1) = 0;

    // Добавить опцию к последнему (или указанному) select-итему.
    virtual void AddSelectOption(ItemRef hItem, const char* sOptionBack, const char* sOptionText) = 0;

    // Управление состоянием toggle-итема.
    virtual void SetToggleState(ItemRef hItem, bool bState) = 0;
    virtual bool GetToggleState(ItemRef hItem) = 0;

    // Управление выбранной опцией select-итема.
    virtual void SetSelectedOption(ItemRef hItem, int iOption) = 0;
    virtual int GetSelectedOption(ItemRef hItem) = 0;
};

typedef std::function<void(int iSlot, const char* szLayoutName, const char* szButton)> OnCustomHudClicked;

class ILayoutApi
{
public:
    virtual void Create(int iSlot, const char* szLayoutName, const char* szLayoutPath) = 0;
    virtual void SetHasClass(int iSlot, const char* szLayoutName, CUtlString szPanelId, CUtlString szClassName, bool bHasClass) = 0;
    virtual void SetInputCapture(int iSlot, const char* szLayoutName, bool bCapture) = 0;
    virtual bool GetInputCapture(int iSlot, const char* szLayoutName) = 0;
    virtual void SetDialogVariable(int iSlot, const char* szLayoutName, CUtlString szPanelId, CUtlString szVariableName, CUtlString szValue) = 0;
    virtual const char* GetDialogVariable(int iSlot, const char* szLayoutName, CUtlString szPanelId, CUtlString szVariableName) = 0;
    virtual void Destroy(int iSlot, const char* szLayoutName, float flDelay) = 0;
    virtual void HookOnCustomHudClicked(SourceMM::PluginId id, OnCustomHudClicked callback) = 0;
};

/////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////
