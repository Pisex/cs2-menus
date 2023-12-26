#pragma once
#include "CBaseCombatCharacter.h"
#include "CPlayer_ItemServices.h"
#include "CPlayer_WeaponServices.h"
#include "CBasePlayerController.h"
#include "ehandle.h"
#include "schemasystem.h"
#include "virtual.h"

class CBasePlayerPawn : public CBaseCombatCharacter
{
public:
	SCHEMA_FIELD(CPlayer_ItemServices*, CBasePlayerPawn, m_pItemServices);
	SCHEMA_FIELD(CPlayer_WeaponServices*, CBasePlayerPawn, m_pWeaponServices);
	SCHEMA_FIELD(CHandle<CBasePlayerController>, CBasePlayerPawn, m_hController);
	auto ForceRespawn() {
		return CALL_VIRTUAL(void, 327, this);
	}
};