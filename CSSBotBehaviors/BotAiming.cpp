#include "BotAiming.h"
#include "BotPath.h"
#include "calltemplates.h"
#include "util.h"
#include "SigScans.h"
#include "sourcehook.h"
#include "sh_memory.h"
#include "convar.h"
#include "basehandle.h"
#include "eiface.h"

extern const ConVar *g_pCvBotFlipout;

extern IServerGameEnts *gameents;
extern IVEngineServer *engine;

enum VisiblePartType
{
	NONE		= 0x00,
	GUT			= 0x01,
	HEAD		= 0x02,
	LEFT_SIDE	= 0x04,
	RIGHT_SIDE	= 0x08,
	FEET		= 0x10
};

//=======================================================================================================================

void DoAimPatch( void )
{
	static bool patched = false;

	if( !patched )
	{
		// Disable the code that handles aiming at an enemy
		const int jmpOffset = 0x147;
		unsigned char *pPatchAddress = ((unsigned char*)CCSBot_Upkeep_Sig.Address() + jmpOffset);

		if( SourceHook::SetMemAccess( pPatchAddress, 6, SH_MEM_READ|SH_MEM_WRITE|SH_MEM_EXEC ) )
		{
			const unsigned char bytes[] = { 0xE9, 0x15, 0x03, 0x00, 0x00, 0x90 };
			for( int i = 0; i < 6; ++i )
			{
				*pPatchAddress = bytes[ i ];
				++pPatchAddress;
			}
		}

		// Disable the call to UpdateLookAngles
		const int nopOffset = 0x461;
		pPatchAddress = ((unsigned char*)CCSBot_Upkeep_Sig.Address() + nopOffset);

		if( SourceHook::SetMemAccess( pPatchAddress, 7, SH_MEM_READ|SH_MEM_WRITE|SH_MEM_EXEC ) )
		{
			const unsigned char nop = 0x90;
			for( int i = 0; i < 7; ++i )
			{
				*pPatchAddress = nop;
				++pPatchAddress;
			}
		}

		patched = true;
	}
}

//=======================================================================================================================

void UpdateLookAngles( CCSBot *bot )
{
	CallObjectNonVirtualFunc_0<void>( bot, CCSBot_UpdateLookAngles_Sig.Address() );
}

//=======================================================================================================================

void SetBotLookAngles( CCSBot *bot, float yaw, float pitch )
{
	const int lookYawOffs = 13756;
	const int lookPitchOffs = 13748;

	*(float*)((char*)bot + lookYawOffs) = yaw;
	*(float*)((char*)bot + lookPitchOffs) = pitch;
}

//=======================================================================================================================

bool IsBotEnemyPartVisible( CCSBot *bot, VisiblePartType part )
{
	const int visibleEnemyPartsOffs = 13825;

	return (*((unsigned char*)bot + visibleEnemyPartsOffs) & part) != 0;
}

//=======================================================================================================================

const Vector &GetPartPosition( CCSBot *caller, CCSPlayer *targetPlayer, VisiblePartType part )
{
	return CallObjectNonVirtualFunc_2<const Vector &>( caller, CCSBot_GetPartPosition_Sig.Address(), targetPlayer, part );
}

//=======================================================================================================================

// This is more about recoil than being automatic
bool WeaponIsSprayable( CSWeaponID weapon )
{
	switch( weapon )
	{
		case WEAPON_ELITE:
		case WEAPON_TMP:
		case WEAPON_MAC10:
		case WEAPON_MP5NAVY:
		case WEAPON_UMP45:
		case WEAPON_P90:
		case WEAPON_GALIL:
		case WEAPON_AK47:
		case WEAPON_SG552:
		case WEAPON_FAMAS:
		case WEAPON_M4A1:
		case WEAPON_AUG:
		case WEAPON_M249:
		case WEAPON_G3SG1:
		case WEAPON_SG550:
			return true;
	}

	return false;
}

//=======================================================================================================================

void BotAimAtEnemy( CCSBot *me, CCSPlayer *enemy )
{
	const int aimSpotOffs = 13796;
	Vector &vecAimSpot = *(Vector*)((char*)me + aimSpotOffs);

	const int enemyIsVisibleOffs = 13824;
	const float skill = GetBotSkill( me );
	CSWeaponID weapon = GetWeaponID( GetActiveWeapon( me ) );

	if( *(char*)me + enemyIsVisibleOffs == 0 ) // Can't see enemy - aim at their last position
	{
		// TODO: Add bend line of sight ?
		const int lastEnemyPositionOffs = 13828;
		vecAimSpot = *(Vector*)((char*)me + lastEnemyPositionOffs);
	}
	else // Enemy is visible - aim at them
	{
		VisiblePartType aimAtPart;

		bool bVeryCloseToEnemy = (GetAbsOrigin( me ) - GetAbsOrigin( enemy )).IsLengthLessThan( 60.f );

		// Aim at the part that's vertically closest to us if we're very close to the enemy
		if( bVeryCloseToEnemy )
		{
			aimAtPart = IsPlayerDucking( me ) ? GUT : HEAD;
		}
		else
		{
			if( weapon == WEAPON_AWP || (weapon == WEAPON_SCOUT && skill < 0.65) )
			{
				aimAtPart = GUT;
			}
			else if( (skill < 0.5 && WeaponIsSprayable( weapon )) || weapon == WEAPON_M3 || weapon == WEAPON_XM1014 )
			{
				aimAtPart = GUT;
			}
			else
			{
				aimAtPart = HEAD;
			}
		}

		if( IsBotEnemyPartVisible( me, aimAtPart ) )
		{
			vecAimSpot = GetPartPosition( me, enemy, aimAtPart );
		}
		else
		{
			// Desired part is blocked - aim at whatever part is visible 
			if (IsBotEnemyPartVisible( me, HEAD ))
			{
				vecAimSpot = GetPartPosition( me, enemy, HEAD );
			}
			else if (IsBotEnemyPartVisible( me, GUT ))
			{
				vecAimSpot = GetPartPosition( me, enemy, GUT );
			}
			else if (IsBotEnemyPartVisible( me, LEFT_SIDE ))
			{
				vecAimSpot = GetPartPosition( me, enemy, LEFT_SIDE );
			}
			else if (IsBotEnemyPartVisible( me, RIGHT_SIDE ))
			{
				vecAimSpot = GetPartPosition( me, enemy, RIGHT_SIDE );
			}
			else // FEET
			{
				vecAimSpot = GetPartPosition( me, enemy, FEET );
			}
		}
	}

	// Add in the aim offset
	const int aimOffsetOffs = 13764;
	Vector &vecAimOffset = *(Vector*)((char*)me + aimOffsetOffs);
	vecAimSpot += vecAimOffset;

	// Compute and set the angles
	const int eyePositionOffs = 5348;
	const Vector &eyePos = *(Vector*)((char*)me + eyePositionOffs);

	Vector to( vecAimSpot - eyePos );

	QAngle idealAngles;
	VectorAngles( to, idealAngles );

	// Factor in the punch angle (from recoil)
	const QAngle &vecPunchAngle = GetPunchAngle( me );
	idealAngles.x -= vecPunchAngle.x * 2 * max(skill, 0.35f); // TODO: add delay to this dependent on skill
	idealAngles.y -= vecPunchAngle.y * skill * 1.5;

	SetBotLookAngles( me, idealAngles.y, idealAngles.x );
}

//=======================================================================================================================

void DoAiming( CCSBot *bot )
{
	// Early terminations from the original function
	if( NavMeshIsGenerating()
	|| !IsAlive( bot )
	|| (g_pCvBotFlipout && g_pCvBotFlipout->GetBool()) )
	{
		return;
	}

	// Check if we should aim at the enemy
	const int isAimingAtEnemyOffs = 14416;
	if( *((char*)bot + isAimingAtEnemyOffs) != 0 )
	{
		CCSPlayer *pEnemy = GetBotEnemy( bot );

		if( pEnemy && IsAlive(pEnemy) )
		{
			// Do our own aiming
			BotAimAtEnemy( bot, pEnemy );
		}
	}

	// Manually call UpdateLookAngles at the end since we disabled it in the original function
	UpdateLookAngles( bot );
}

//=======================================================================================================================