#include "Events.h"
#include "BotPath.h"
#include "BotAiming.h"
#include "BotHiding.h"
#include "edict.h"
#include "convar.h"
#include "eiface.h"
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
	const int argc = engine->Cmd_Argc();

	if( argc < 2 )
	{
		RETURN_META( MRES_IGNORED );
	}

	char *cmd = engine->Cmd_Argv( 0 );

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

	return true;
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
}

//=======================================================================================================================