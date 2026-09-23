#pragma once

#include <functional>
#include <string>
#include <vector>

class CBaseEntity;
class CBaseModelEntity;
class CEntityInstance;
class CEntityKeyValues;
class CGameEntitySystem;
class CEntitySystem;
class CGlobalVars;
class IGameEvent;
class IGameEventManager2;
struct CTakeDamageInfoContainer;
class CTakeDamageInfo;
class CCSGameRules;
class CTimer;
struct Menu;
class INetworkMessageInternal;
class CNetMessage;
class CCommand;
class CPlayer_ItemServices;
class CEconItemView;

#define UTILS_INTERFACE "IUtilsApi"

/////////////////////////////////////////////////////////////////
///////////////////////      UTILS     //////////////////////////
/////////////////////////////////////////////////////////////////

typedef std::function<bool(int iSlot, const char* szContent)> CommandCallback;
typedef std::function<bool(int iSlot, const char* szContent, bool bTeam)> CommandCallbackPre;
typedef std::function<bool(int iSlot, const char* szContent, bool bMute, bool bTeam)> CommandCallbackPost;
typedef std::function<void(const char* szName, IGameEvent* pEvent, bool bDontBroadcast)> EventCallback;

// Тип результата приобретения оружия (соответствует движковому AcquireResult::Type).
namespace AcquireResult
{
    enum Type : int
    {
        Allowed = 0,
        InvalidItem,
        AlreadyOwned,
        AlreadyPurchased,
        ReachedGrenadeTypeLimit,
        ReachedGrenadeTotalLimit,
        NotAllowedByTeam,
        NotAllowedByMap,
        NotAllowedByMode,
        NotAllowedForPurchase,
        NotAllowedByProhibition,
    };
}

// Метод приобретения (подбор/покупка).
namespace AcquireMethod
{
    enum Type : int
    {
        PickUp = 0,
        Buy,
    };
}

// Результат хука CanAcquire от одного плагина. priority - приоритет, с которым
// плагин настаивает на своём вердикте; если приоритеты не заданы (< 0), решение
// выбирается по типу (любой не-Allowed побеждает). type - желаемый вердикт.
struct AcquireResultInfo
{
    int priority;
    AcquireResult::Type type;

    AcquireResultInfo(AcquireResult::Type t) : priority(-1), type(t) {}
    AcquireResultInfo(AcquireResult::Type t, int p) : priority(p), type(t) {}
};

// Хук CanAcquire. Вызывается когда игрок пытается получить/купить оружие.
// Верните AcquireResultInfo{type} или {type, priority}. Смотрите AddCanAcquirePre/Post.
typedef std::function<AcquireResultInfo(int iSlot, CPlayer_ItemServices* pItemServices, CEconItemView* pItemView, AcquireMethod::Type eMethod)> CanAcquireCallback;


// Результат нового хука ивентов. Continue - ничего не менять; Changed - параметры
// изменены (примут эффект); Handled/Stop - отменить ивент (движок его не увидит).
enum class EventHookResult : int
{
    Continue = 0,
    Changed  = 1,
    Handled  = 2,
    Stop     = 3,
};

// Изменяемая информация об ивенте, передаётся в pre-колбэк. bDontBroadcast можно
// поменять внутри pre, чтобы движок разослал/не разослал ивент.
struct EventInfo
{
    bool bDontBroadcast;
};

// Новый хук ивентов. Pre вызывается ДО обработки движком, может отменить (вернув
// Handled/Stop) или изменить параметры (правкой info->bDontBroadcast + возврат Changed).
typedef std::function<EventHookResult(const char* szName, IGameEvent* pEvent, EventInfo* info)> EventHookPreCallback;
// Post вызывается ПОСЛЕ обработки движком. Имя ивента доступно (через копию ивента).
typedef std::function<void(const char* szName, IGameEvent* pEvent, bool bDontBroadcast)> EventHookPostCallback;

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


class IUtilsEntityListener {
public:
	virtual ~IUtilsEntityListener() = default;
    virtual void OnEntityCreated(CEntityInstance* pEntity) { };
	virtual void OnEntitySpawned(CEntityInstance* pEntity) { };
	virtual void OnEntityDeleted(CEntityInstance* pEntity) { };
	virtual void OnEntityParentChanged(CEntityInstance* pEntity, CEntityInstance* pNewParent) { };
    virtual void CheckTransmit(CCheckTransmitInfo **pInfoInfoList, int nInfoCount, CBitVec<16384> &unionTransmitEdicts, CBitVec<16384> &, const Entity2Networkable_t **pNetworkables, const uint16 *pEntityIndicies, int nEntityIndices, bool bEnablePVSBits) { };
};

class IServerListener {
public:
	virtual ~IServerListener() = default;\
    // Тут он вызывается ОДИН РАЗ
    virtual void StartupServer() { };
    virtual void MapEndHook() { };
    virtual void MapStartHook(const char* szMap) { };
    virtual void PreWorldUpdate(bool bSimulating) { };
    virtual void GameFrame(bool bSimulating, bool bFirstTick, bool bLastTick) { };
    virtual void ServerHibernationUpdate(bool bHibernating) { };
    virtual void GameServerSteamAPIActivated() { };
    virtual void GameServerSteamAPIDeactivated() { };
    virtual void OnHostNameChanged(const char *pHostname) { };
    virtual void PreFatalShutdown() { };
    virtual void UpdateWhenNotInGame(float flFrameTime) { };
    virtual void ServerConVarChanged(const char *pVarName, const char *pValue) { };
    virtual bool SetClientListening(int iReceiver, int iSender, bool bListen) { return bListen; };
    virtual void PostEvent(int nClientCount, const uint64 *clients, INetworkMessageInternal *pEvent, const CNetMessage *pData) { };
    virtual bool DispatchConCommand(int iSlot, const CCommand &args) { return true; };
    virtual void FireEvent(IGameEvent *pEvent, bool bDontBroadcast) { };

    // POST-варианты: вызываются ПОСЛЕ выполнения оригинальной функции движком.
    virtual void PreWorldUpdatePost(bool bSimulating) { };
    virtual void GameFramePost(bool bSimulating, bool bFirstTick, bool bLastTick) { };
    virtual void ServerHibernationUpdatePost(bool bHibernating) { };
    virtual void GameServerSteamAPIActivatedPost() { };
    virtual void GameServerSteamAPIDeactivatedPost() { };
    virtual void OnHostNameChangedPost(const char *pHostname) { };
    virtual void PreFatalShutdownPost() { };
    virtual void UpdateWhenNotInGamePost(float flFrameTime) { };
    virtual void ServerConVarChangedPost(const char *pVarName, const char *pValue) { };
    virtual void SetClientListeningPost(int iReceiver, int iSender, bool bListen) { };
    virtual void PostEventPost(int nClientCount, const uint64 *clients, INetworkMessageInternal *pEvent, const CNetMessage *pData) { };
    virtual void DispatchConCommandPost(int iSlot, const CCommand &args) { };
    virtual void FireEventPost(IGameEvent *pEvent, bool bDontBroadcast) { };
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
    
    virtual void TerminateRound(int reason, float delay = 0.0f, int64 teamid = 0) = 0;
    
    virtual void AddServerListener(SourceMM::PluginId id, IServerListener* pListener) = 0;
    virtual void AddEntityListener(SourceMM::PluginId id, IUtilsEntityListener* pListener) = 0;

    // Новый хук ивентов (не путать со старым HookEvent). Pre можно отменить/изменить,
    // post получает имя ивента. szName - имя ивента (например "player_death").
    virtual void HookEventPre(SourceMM::PluginId id, const char* szName, EventHookPreCallback callback) = 0;
    virtual void HookEventPost(SourceMM::PluginId id, const char* szName, EventHookPostCallback callback) = 0;

    virtual void AddPrecache(const char* szResource) = 0;

    // Хук CanAcquire (может ли игрок получить/купить оружие). Pre вызывается ДО
    // движка и может изменить вердикт, Post - ПОСЛЕ (наблюдение).
    // Агрегация Pre: если хотя бы один плагин вернул priority >= 0, побеждает
    // максимальный priority. Если приоритетов нет, побеждает любой не-Allowed тип.
    virtual void AddCanAcquirePre(SourceMM::PluginId id, CanAcquireCallback callback) = 0;
    virtual void AddCanAcquirePost(SourceMM::PluginId id, CanAcquireCallback callback) = 0;
};
