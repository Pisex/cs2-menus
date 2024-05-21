#pragma once
#include "CBaseCombatCharacter.h"
#include "CPlayer_ItemServices.h"
#include "CPlayer_WeaponServices.h"
#include "CBasePlayerController.h"
#include "ehandle.h"
#include "schemasystem.h"
#include "virtual.h"

enum PlayerButtons : uint64_t
{
	Attack = (1 << 0),
	Jump = (1 << 1),
	Duck = (1 << 2),
	Forward = (1 << 3),
	Back = (1 << 4),
	Use = (1 << 5),
	Cancel = (1 << 6),
	Left = (1 << 7),
	Right = (1 << 8),
	Moveleft = (1 << 9),
	Moveright = (1 << 10),
	Attack2 = (1 << 11),
	Run = (1 << 12),
	Reload = (1 << 13),
	Alt1 = (1 << 14),
	Alt2 = (1 << 15),
	Speed = (1 << 16),   /** Player is holding the speed key */
	Walk = (1 << 17),   /** Player holding walk key */
	Zoom = (1 << 18),   /** Zoom key for HUD zoom */
	Weapon1 = (1 << 19),   /** weapon defines these bits */
	Weapon2 = (1 << 20),   /** weapon defines these bits */
	Bullrush = (1 << 21),
	Grenade1 = (1 << 22),   /** grenade 1 */
	Grenade2 = (1 << 23),   /** grenade 2 */
	Attack3 = (1 << 24)
};

class CInButtonState
{
public:
    SCHEMA_FIELD(uint64_t[3], CInButtonState, m_pButtonStates);
};

class CPlayer_MovementServices : public CPlayerPawnComponent
{
public:
    SCHEMA_FIELD(CInButtonState, CPlayer_MovementServices, m_nButtons);
};

class CBasePlayerPawn : public CBaseCombatCharacter
{
public:
    SCHEMA_FIELD(CPlayer_MovementServices*, CBasePlayerPawn, m_pMovementServices);
	SCHEMA_FIELD(CPlayer_ItemServices*, CBasePlayerPawn, m_pItemServices);
	SCHEMA_FIELD(CPlayer_WeaponServices*, CBasePlayerPawn, m_pWeaponServices);
	SCHEMA_FIELD(CHandle<CBasePlayerController>, CBasePlayerPawn, m_hController);
	auto ForceRespawn() {
		return CALL_VIRTUAL(void, 327, this);
	}
};