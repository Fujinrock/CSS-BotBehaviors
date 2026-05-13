#include "util.h"
#include "vector.h"
#include "edict.h"
#include "eiface.h"
#include "ISmmAPI.h"
#include <stdarg.h>

extern IServerGameEnts *gameents;
extern IVEngineServer *engine;
extern ISmmAPI *g_SMAPI;

// Addresses to global objects directly... should be ok since the game will never be updated
CCSBotManager *TheBots = **reinterpret_cast<CCSBotManager ***>( 0x223139C1 );
CCSGameRules **CSGameRules = *reinterpret_cast<CCSGameRules ***>( 0x2232249B ); // This is a pointer to the global pointer for simplicity's sake,
																				// so remember to dereference it once when using it

//=======================================================================================================================

int GetTeamNumber( void *pEntity )
{
	static const int iTeamNumOffs = 520;

	return *(int*)((char*)pEntity + iTeamNumOffs);
}

//=======================================================================================================================

const Vector &GetAbsOrigin( void *pEntity )
{
	const int absOriginOffs = 660;
	return *(Vector*)((char*)pEntity + absOriginOffs);
}

//=======================================================================================================================

bool IsAlive( void *pEntity )
{
	const int lifestateOffs = 140;

	return *((char*)pEntity + lifestateOffs) == 0;
}

//=======================================================================================================================

int GetOppositeTeamNumber( int teamID )
{
	if( teamID == TEAM_T )
		return TEAM_CT;

	if( teamID == TEAM_CT )
		return TEAM_T;

	return TEAM_BOTH;
}

//=======================================================================================================================

bool PlayerHasWeaponInSlot( CBaseEntity *player, int slot )
{
	const int WeaponGetSlotVTOffs = 224;

	void **this_ptr = *(void ***)&player;
	void **vtable = *(void***)player;
	void *func = vtable[WeaponGetSlotVTOffs];
	
	union
	{
		CBaseEntity *(FnEmptyClass::* mfpnew)(int);
		void* addr;
	}
	u;
	u.addr = func;
	
	return ((CBaseEntity *)(reinterpret_cast<FnEmptyClass*>(this_ptr)->*u.mfpnew)(slot)) != NULL;
}

//=======================================================================================================================

int GetNumRoundsPlayed( void )
{
	const int iNumCTWinsOffs = 608;
	const int iNumTWinsOffs = 610;

	int iCTWins = *(short*)((char*)*CSGameRules + iNumCTWinsOffs);
	int iTWins = *(short*)((char*)*CSGameRules + iNumTWinsOffs);

	return iCTWins + iTWins;
}

//=======================================================================================================================

const QAngle &GetEyeAngles( edict_t *pEdict )
{
	CBaseEntity *pEntity = gameents->EdictToBaseEntity( pEdict );

	if( !pEntity )
	{
		Msg( "GetEyeAngles: could not find player entity\n" );
		return vec3_angle;
	}

	const int EyeAnglesVTOffs = 118;

	void **this_ptr = *(void ***)&pEntity;
	void **vtable = *(void***)pEntity;
	void *func = vtable[ EyeAnglesVTOffs ];
	
	union
	{
		const QAngle &(FnEmptyClass::* mfpnew)();
		void* addr;
	}
	u;
	u.addr = func;

	return (const QAngle &)(reinterpret_cast<FnEmptyClass*>(this_ptr)->*u.mfpnew)();
}

//=======================================================================================================================

void PrintToLocal( const char *fmt, ... )
{
	if( engine->IsDedicatedServer() )
		return;

	edict_t *pLocal = engine->PEntityOfEntIndex( 1 );

	if( !pLocal )
		return;

	va_list vl;
	va_start( vl, fmt );

	char szMsg[256];
	_vsnprintf( szMsg, sizeof(szMsg), fmt, vl );

	va_end( vl );

	g_SMAPI->ClientConPrintf( pLocal, szMsg );
}

//=======================================================================================================================

CCSPlayer *GetBotEnemy( CCSBot *bot )
{
	const int enemyOffs = 13820;
	CBaseHandle *hEnemy = (CBaseHandle*)((char*)bot + enemyOffs);
	return (CCSPlayer*)gameents->EdictToBaseEntity( engine->PEntityOfEntIndex( hEnemy->GetEntryIndex() ) );
}

//=======================================================================================================================

float GetBotSkill( CCSBot *bot )
{
	const int profileOffs = 5220;
	const int skillOffs = 8;

	void *pProfile = *reinterpret_cast< void** >( (char*)bot + profileOffs );

	if( !pProfile )
		return 0.f;

	return *(float*)((char*)pProfile + skillOffs);
}

//=======================================================================================================================

CBaseEntity *GetActiveWeapon( CCSPlayer *player )
{
	const int activeWeaponOffs = 1896;

	CBaseHandle *hWeapon = (CBaseHandle*)((char*)player + activeWeaponOffs);

	return gameents->EdictToBaseEntity( engine->PEntityOfEntIndex( hWeapon->GetEntryIndex() ) );
}

//=======================================================================================================================

CSWeaponID GetWeaponID( CBaseEntity *weapon )
{
	if( !weapon )
		return WEAPON_NONE;

	const int GetWeaponIDVTOffs = 323;

	void **this_ptr = *(void ***)&weapon;
	void **vtable = *(void***)weapon;
	void *func = vtable[ GetWeaponIDVTOffs ];
	
	union {
		CSWeaponID (FnEmptyClass::* mfpnew)();
		void* addr;
	} u; 	u.addr = func;
	
	return (CSWeaponID)(reinterpret_cast<FnEmptyClass*>(this_ptr)->*u.mfpnew)();
}

//=======================================================================================================================

void *GetPlayerLocalData( CCSPlayer *player )
{
	const int localOffs = 1904;

	return (char*)player + localOffs;
}

//=======================================================================================================================

const QAngle &GetPunchAngle( CCSPlayer *player )
{
	const int vecPunchAngleOffs = 112;

	return *(QAngle*)((char*)GetPlayerLocalData( player ) + vecPunchAngleOffs);
}

//=======================================================================================================================

bool IsPlayerDucking( CCSPlayer *player )
{
	const int bDuckedOffs = 80;

	// Check this as a short, because m_bDucking is right after m_bDucked, so we can check both with 2 bytes
	return *(short*)((char*)GetPlayerLocalData( player ) + bDuckedOffs) != 0;
}

//=======================================================================================================================