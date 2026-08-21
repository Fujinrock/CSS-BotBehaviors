#include "util.h"
#include "SigScans.h"
#include "calltemplates.h"
#include "vector.h"
#include "edict.h"
#include "eiface.h"
#include "bitbuf.h"
#include "playerinfomanager.h"
#include "tier1/utlmap.h"
#include "ISmmAPI.h"
#include <stdarg.h>

extern IServerGameEnts *gameents;
extern IVEngineServer *engine;
extern IPlayerInfoManager *playerinfomgr;
extern CGlobalVars *gpGlobals;
extern ISmmAPI *g_SMAPI;

CCSBotManager *TheBots = NULL;
CCSGameRules **CSGameRules = NULL; // This is a pointer to the global pointer for simplicity's sake, so remember to dereference it once when using it

//=======================================================================================================================

int GetEntityIndex( void *pEntity )
{
	return engine->IndexOfEdict( gameents->BaseEntityToEdict( (CBaseEntity*)pEntity ) );
}

//=======================================================================================================================

CBaseEntity *GetIndexEntity( int index )
{
	return gameents->EdictToBaseEntity( engine->PEntityOfEntIndex( index ) );
}

//=======================================================================================================================

int GetTeamNumber( void *pEntity )
{
	static const int iTeamNumOffs = 520;

	return *(int*)((char*)pEntity + iTeamNumOffs);
}

//=======================================================================================================================

bool IsBot( void *pEntity )
{
	if( !pEntity )
		return false;

	int idx = GetEntityIndex( pEntity );

	if( idx <= 0 || idx > gpGlobals->maxClients )
		return false;

	edict_t *edict = engine->PEntityOfEntIndex( idx );

	if( !edict )
		return false;

	IPlayerInfo *pInfo = playerinfomgr->GetPlayerInfo( edict );

	if( !pInfo )
		return false;

	return pInfo->IsFakeClient() && !pInfo->IsHLTV();
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

CBaseEntity *GetPlayerWeaponInSlot( CCSPlayer *player, int slot )
{
	const unsigned int WeaponGetSlotVTIndex = 224;

	return CallObjectVirtualFunc_1<CBaseEntity*>( player, WeaponGetSlotVTIndex, slot );
}

//=======================================================================================================================

bool PlayerHasWeaponInSlot( CCSPlayer *player, int slot )
{
	return GetPlayerWeaponInSlot( player, slot ) != NULL;
}

//=======================================================================================================================

int GetNumRoundsPlayed( void )
{
	int iCTWins, iTWins;

	GetTeamWins( iCTWins, iTWins );

	return iCTWins + iTWins;
}

//=======================================================================================================================

void GetTeamWins( int &CTWins, int &TWins )
{
	const int iNumCTWinsOffs = 608;
	const int iNumTWinsOffs = 610;

	CTWins = *(short*)((char*)*CSGameRules + iNumCTWinsOffs);
	TWins = *(short*)((char*)*CSGameRules + iNumTWinsOffs);
}

//=======================================================================================================================

int GetTeamConsecutiveLosses( int team )
{
	const int numConsecutiveCTLosesOffs = 612;
	const int numConsecutiveTerroristLosesOffs = 616;

	if( team % NUM_TEAMS == TEAM_T - 2 )
	{
		return *(int*)((char*)*CSGameRules + numConsecutiveTerroristLosesOffs);
	}
	if( team % NUM_TEAMS == TEAM_CT - 2 )
	{
		return *(int*)((char*)*CSGameRules + numConsecutiveCTLosesOffs);
	}

	return 0;
}

//=======================================================================================================================

bool MapHasBombTarget( void )
{
	const int mapHasBombTargetOffs = 552;

	return (*(char*)*CSGameRules + mapHasBombTargetOffs) != 0;
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

	const unsigned int EyeAnglesVTIndex = 118;

	return CallObjectVirtualFunc_0<const QAngle &>( pEntity, EyeAnglesVTIndex );
}

//=======================================================================================================================

CCSPlayer *GetListenServerHost( void )
{
	if( engine->IsDedicatedServer() )
		return NULL;

	return (CCSPlayer*)GetIndexEntity( 1 );
}

//=======================================================================================================================

void PrintToListenServerHostConsole( const char *fmt, ... )
{
	if( engine->IsDedicatedServer() )
		return;

	edict_t *pHost = engine->PEntityOfEntIndex( 1 );

	if( !pHost )
		return;

	va_list vl;
	va_start( vl, fmt );

	char szMsg[256];
	_vsnprintf( szMsg, sizeof(szMsg), fmt, vl );

	va_end( vl );

	g_SMAPI->ClientConPrintf( pHost, szMsg );
}

//=======================================================================================================================

void PrintCenterMessage( CCSPlayer *player, const char *szMsg )
{
	if( !player )
		return;

	CRecipientFilter filter;
	filter.AddRecipient( player );

	const int textMsgIndex = 5;
	const int printCenterIndex = 4;

	bf_write *bf = engine->UserMessageBegin( &filter, textMsgIndex );

	if( bf )
	{
		bf->WriteByte( printCenterIndex );
		bf->WriteString( szMsg );
		engine->MessageEnd();
	}
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

CNavArea *GetBotLastKnownArea( CCSBot *bot )
{
	const int lastKnownAreaOffs = 5912;
	return *(CNavArea**)((char*)bot + lastKnownAreaOffs);
}

//=======================================================================================================================

bool BotsOnTheServer( void )
{
	for( int i = 1; i <= gpGlobals->maxClients; ++i )
	{
		edict_t *pEdict = engine->PEntityOfEntIndex( i );
		IPlayerInfo *pInfo = playerinfomgr->GetPlayerInfo( pEdict );

		if( !pEdict || pEdict->IsFree() || !pInfo )
			continue;

		if( pInfo->IsFakeClient() && !pInfo->IsHLTV() )
			return true;
	}

	return false;
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

	const unsigned int GetWeaponIDVTIndex = 323;

	return CallObjectVirtualFunc_0<CSWeaponID>( weapon, GetWeaponIDVTIndex );
}

//=======================================================================================================================

CCSWeaponInfo *GetWeaponInfo( CSWeaponID weaponID )
{
	typedef CCSWeaponInfo *(*SFN)(CSWeaponID);

	SFN func = (SFN)GetWeaponInfo_Sig.Address();

	return func( weaponID );
}

//=======================================================================================================================

static CUtlMap< CSWeaponID, int > s_WeaponPrices( 0, 29 );

int GetWeaponPrice( CSWeaponID weaponID )
{
	if( weaponID <= WEAPON_NONE || weaponID > WEAPON_P90 )
		return 0;

	AssertMsg( s_WeaponPrices.IsValidIndex( s_WeaponPrices.Find(weaponID) ), "Weapon ID missing from weapon prices map" );

	return s_WeaponPrices[ weaponID ];
}

//=======================================================================================================================

void InitWeaponPrices( void )
{
	s_WeaponPrices.RemoveAll();

	for( int i = WEAPON_P228; i <= WEAPON_P90; ++i )
	{
		CCSWeaponInfo *pInfo = GetWeaponInfo( (CSWeaponID)i );

		s_WeaponPrices[ i ] = pInfo ? pInfo->GetRealWeaponPrice() : 0;
	}
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