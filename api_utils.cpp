#include "utils.h"
#include "menus_internal.h"
#include <sstream>
#include <iomanip>
#include <ctime>
#include <fstream>

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

void UtilsApi::SetStateChanged(CBaseEntity* pEntity, const char* sClassName, const char* sFieldName, int extraOffset)
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

void UtilsApi::TerminateRound(int reason, float delay, int64 teamid)
{
	if(UTIL_TerminateRound && g_pGameRules)
	{
		UTIL_TerminateRound(reinterpret_cast<CGameRules*>(g_pGameRules), delay, static_cast<unsigned int>(reason), teamid);
	}
}

void UtilsApi::AddPrecache(const char* szResource)
{
	if(!szResource || !szResource[0])
		return;
	
	for(const auto& it : g_mapPrecache)
		if(it == szResource)
			return;
	g_mapPrecache.push_back(szResource);
}
