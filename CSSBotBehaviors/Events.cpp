#include "Events.h"
#include "BotPath.h"
#include "BotAiming.h"
#include "BotHiding.h"
#include "edict.h"
#include "convar.h"
#include "eiface.h"
#include "random.h"
#include "sourcehook.h"

extern CGlobalVars *gpGlobals;
extern IVEngineServer *engine;
extern SourceHook::ISourceHook *g_SHPtr;

extern const ConVar *g_pCvFreezetime;

//=======================================================================================================================

void CreateDetours( void );

void OnServerActivate( edict_t *pEdictList, int edictCount, int clientMax )
{
	// Create detours after SM
	CreateDetours();

	DoAimPatch();
}

//=======================================================================================================================

void OnClientCommand( edict_t *pEntity )
{
	const char *cmd = engine->Cmd_Argv( 0 );
	const int argc = engine->Cmd_Argc();

#ifdef _DEBUG
	if ( !stricmp( cmd, "zones" ) )
	{
		CCSPlayer *host = GetListenServerHost();
		const Vector &absOrigin = GetAbsOrigin( host );
		CNavArea *pArea = GetNearestNavArea( absOrigin );
		char siteChars[ 4 ] = { 'A', 'B', 'C', 'D' };

		if ( !pArea )
			RETURN_META( MRES_IGNORED );

		for ( int i = 0; i < GetZoneCount(); ++i )
		{
			const Zone *zone = GetZone( i );
			CNavArea *pZoneArea = zone->m_area[ 0 ];

			if ( !pZoneArea )
				continue;

			const float distance = NavAreaTravelDistance( pArea, pZoneArea );

			engine->Con_NPrintf( i, "Distance to %c: %.02f", siteChars[ i ], distance );
		}

		RETURN_META(MRES_SUPERCEDE);
	}
#endif

	if( argc < 2 )
	{
		RETURN_META( MRES_IGNORED );
	}

	if( !stricmp( cmd, "danger" ) )
	{
		if( HandleDangerAreaCmd( pEntity ) )
		{
			RETURN_META(MRES_SUPERCEDE);
		}
		else
		{
			RETURN_META(MRES_IGNORED);
		}
	}
	else if( !stricmp( cmd, "hide" ) )
	{
		if( HandleHidingSpotCmd( pEntity ) )
		{
			RETURN_META(MRES_SUPERCEDE);
		}
		else
		{
			RETURN_META(MRES_IGNORED);
		}
	}

	RETURN_META( MRES_IGNORED );
}

//=======================================================================================================================

bool OnLevelInit( char const *pMapName, char const *pMapEntities, char const *pOldLevel, char const *pLandmarkName, bool loadGame, bool background )
{
	TheDangerAreas.PurgeAndDeleteElements();
	TheLookSpots.Purge();
	DisconnectedHidingSpots.Purge();

	for( int i = 0; i < MAXPLAYERS; ++i )
	{
		g_fPathRandomizedTimestamp[ i ] = 0.0f;
		g_iSafePathIndex[ i ] = -1;
		g_iLastSafePathIndex[ i ] = -1;
	}

	g_szNavIdentifier[ 0 ] = '\000';
	g_bDoNavSaveComputations = false;

	// Re-fetch weapon prices in case the scripts were changed
	//InitWeaponPrices();

	return true;
}

//=======================================================================================================================

static float s_NextForceHuntTime = 0.f;

ConVar g_cvForceBotHunt( "bot_force_hunt", "0", FCVAR_NONE, "Whether bots should periodically start hunting.", true, 0.0, true, 1.0 );
ConVar g_cvForceBotHuntMinInterval( "bot_force_hunt_min_interval", "30.0", FCVAR_NONE, "Minimum time that should pass between forced hunting.", true, 1.0, false, 0.0 );
ConVar g_cvForceBotHuntMaxInterval( "bot_force_hunt_max_interval", "60.0", FCVAR_NONE, "Maximum time that can pass between forced hunting.", true, 1.0, false, 0.0 );

void SetNextHuntTime( void )
{
	s_NextForceHuntTime = gpGlobals->curtime + RandomFloat( g_cvForceBotHuntMinInterval.GetFloat(), g_cvForceBotHuntMaxInterval.GetFloat() );
}

//=======================================================================================================================

void OnGameFrame( bool simulating )
{
	if( !simulating )
		return;

	// Make bots occasionally hunt (useful for DM)
	if( g_cvForceBotHunt.GetBool() && gpGlobals->curtime > s_NextForceHuntTime )
	{
		SetNextHuntTime();

		for( int i = 1; i <= gpGlobals->maxClients; ++i )
		{
			CCSPlayer *player = (CCSPlayer*)GetIndexEntity( i );

			if( !player || !IsBot( player ) )
				continue;

			if( !IsAlive( player ) || GetTeamNumber( player ) < TEAM_T )
				continue;

			// Only make hiding bots hunt
			if( BotIsHiding( player ) )
				BotHunt( player );
		}
	}

	if ( !engine->IsDedicatedServer() )
	{
		DrawActiveHidingSpotLookSpot();
	}
}

//=======================================================================================================================

CEvent_OnRoundStart g_OnRoundStart;

void CEvent_OnRoundStart::FireGameEvent( IGameEvent *event )
{
	for( int i = 0; i < MAXPLAYERS; ++i )
	{
		g_fPathRandomizedTimestamp[ i ] = 0.0f;
		g_iSafePathIndex[ i ] = -1;
		g_iLastSafePathIndex[ i ] = -1;
	}
	g_bHaveDoneRoundMoneyCheck = false;
	// This could be set in round_freeze_end, but this requires less code...
	g_fRoundStartTimestamp = gpGlobals->curtime + (g_pCvFreezetime ? g_pCvFreezetime->GetFloat() : 0.f);

	SetNextHuntTime();
}

//=======================================================================================================================