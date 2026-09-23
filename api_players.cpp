#include "utils.h"
#include "menus_internal.h"

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

void PlayersApi::SetConVars(std::vector<int> vPlayers, std::vector<FakeConVar> cvars)
{
	static INetworkMessageInternal* netmsg = g_pNetworkMessages->FindNetworkMessagePartial("CNETMsg_SetConVar");
	std::unique_ptr<CNETMsg_SetConVar_t> msg(netmsg->AllocateMessage()->As<CNETMsg_SetConVar_t>());
	
	for (const auto& cvar : cvars) {
		CMsg_CVars_CVar *cvarEntry = msg->mutable_convars()->add_cvars();
		cvarEntry->set_name(cvar.szCvar.c_str());
		cvarEntry->set_value(cvar.szValue.c_str());
	}

	CPlayerBitVec recipients;
	for (auto i : vPlayers) {
		recipients.Set(i);
	}
	
	g_gameEventSystem->PostEventAbstract(-1, false, ABSOLUTE_PLAYER_LIMIT, reinterpret_cast<const uint64*>(recipients.Base()), netmsg, msg.get(), 0, NetChannelBufType_t::BUF_RELIABLE);
	// msg->Send(recipients);
	// delete msg;
}

void PlayersApi::SetConVar(std::vector<int> vPlayers, const char* name, const char* value)
{
	static INetworkMessageInternal* netmsg = g_pNetworkMessages->FindNetworkMessagePartial("CNETMsg_SetConVar");
	std::unique_ptr<CNETMsg_SetConVar_t> msg(netmsg->AllocateMessage()->As<CNETMsg_SetConVar_t>());
	
	CMsg_CVars_CVar *cvar = msg->mutable_convars()->add_cvars();
	cvar->set_name(name);
	cvar->set_value(value);

	CPlayerBitVec recipients;
	for (auto i : vPlayers) {
		recipients.Set(i);
	}
	
	g_gameEventSystem->PostEventAbstract(-1, false, ABSOLUTE_PLAYER_LIMIT, reinterpret_cast<const uint64*>(recipients.Base()), netmsg, msg.get(), 0, NetChannelBufType_t::BUF_RELIABLE);
	// msg->Send(recipients);
	// delete msg;
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

bool PlayersApi::UseClientCommand(int iSlot, const char* szCommand)
{
	if (iSlot == -1) return false;
	if (iSlot < 0 || iSlot >= 64) return false;
	auto tokens = SplitStringBySpace(szCommand);
	std::string sCommand = "";
	for (size_t i = 1; i < tokens.size(); ++i) {
		sCommand += tokens[i] + " ";
	}
	bool bFound = false;
	CommandCallback fn = nullptr;
	for(auto& item : ConsoleCommands)
	{
		if(item.second[std::string(tokens[0])])
		{
			bFound = true;
			fn = item.second[std::string(tokens[0])];
		}
	}
	for(auto& item : ChatCommands)
	{
		if(item.second[std::string(tokens[0])])
		{
			bFound = true;
			fn = item.second[std::string(tokens[0])];
		}
	}
	if(bFound && fn)
	{
		fn(iSlot, sCommand.c_str());
		return true;
	}
	return false;
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
