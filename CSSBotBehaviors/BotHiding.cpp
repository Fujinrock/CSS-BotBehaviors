#include "BotHiding.h"
#include "BotPath.h"
#include "SigScans.h"
#include "util.h"
#include "edict.h"
#include "convar.h"
#include "eiface.h"
#include "playerinfomanager.h"
#include "filesystem.h"
#include "ISmmPlugin.h"

extern IVEngineServer *engine;
extern IPlayerInfoManager *playerinfomgr;
extern CGlobalVars *gpGlobals;
extern IFileSystem *filesystem;
extern ISmmAPI *g_SMAPI;
extern SourceHook::ISourceHook *g_SHPtr;

CUtlVector< HidingSpotLookInfo > TheLookSpots;			///< Spots to look at while bots are hiding
CUtlVector< HidingSpot * > DisconnectedHidingSpots;		///< Hiding spots whose nav areas have been removed

extern const ConVar *g_pCvNavEdit;
extern const ConVar *g_pCvNavQuicksave;

// Address is m_Head of the list (*0x221C7253) minus sizeof(CUtlMemory<ListElem_t>) which is 12
HidingSpotList *TheHidingSpotList = (HidingSpotList*)(((char*)(*reinterpret_cast<void**>( 0x221C7253 ))) - 12);

// CNavArea's static variable m_isReset
bool *g_bNavMeshIsBeingDestroyed = *reinterpret_cast<bool**>( 0x221C7709 );

#define MAX_NAV_ID_LEN 128
char g_szNavIdentifier[ MAX_NAV_ID_LEN ] = { '\000' };

bool g_bDoNavSaveComputations = false;

//=======================================================================================================================

enum
{
	IN_COVER			= 0x01,		///< In a corner with good hard cover nearby
	GOOD_SNIPER_SPOT	= 0x02,		///< Had at least one decent sniping corridor
	IDEAL_SNIPER_SPOT	= 0x04,		///< Can see either very far, or a large area, or both
	EXPOSED				= 0x08		///< Spot in the open, usually on a ledge or cliff
};

//=======================================================================================================================

HidingSpot *CreateHidingSpot( void )
{
	const int CreateHidingSpotVTOffs = 1;

	void **this_ptr = *(void ***)&TheNavMesh;
	void **vtable = *(void***)TheNavMesh;
	void *func = vtable[ CreateHidingSpotVTOffs ];
	
	union
	{
		HidingSpot *(FnEmptyClass::* mfpnew)();
		void* addr;
	}
	u;
	u.addr = func;

	return (HidingSpot *)(reinterpret_cast<FnEmptyClass*>(this_ptr)->*u.mfpnew)();
}

//=======================================================================================================================

void DestroyHidingSpots( void )
{
	if( !CNavMesh_DestroyHidingSpots_Sig.IsSet() )
	{
		Warning( "CNavMesh::DestroyHidingSpots address not set\n" );
		return;
	}

	union
	{
		void (FnEmptyClass::*mfpnew)();
		void* addr;
	}
	u;
	u.addr = CNavMesh_DestroyHidingSpots_Sig.Address();

	(reinterpret_cast<FnEmptyClass*>(TheNavMesh)->*u.mfpnew)();
}

//=======================================================================================================================

HidingSpotList &GetNavAreaHidingSpotList( CNavArea *pArea )
{
	const int hidingSpotListOffs = 84;

	return *(HidingSpotList*)((char*)pArea + hidingSpotListOffs);
}

//=======================================================================================================================

const uint INVALID_SPOT_ID = ~0;

uint GetHidingSpotID( HidingSpot *spot )
{
	if( !spot )
		return INVALID_SPOT_ID;

	const int idOffs = 16;

	return *(uint*)((char*)spot + idOffs);
}

//=======================================================================================================================

HidingSpot *GetHidingSpotByID( uint id )
{
	FOR_EACH_LL( (*TheHidingSpotList), it )
	{
		HidingSpot *spot = TheHidingSpotList->Element( it );

		if( GetHidingSpotID( spot ) == id )
			return spot;
	}

	return NULL;
}

//=======================================================================================================================

void OnNavAreaRemoved( CNavArea *area )
{
	if( *g_bNavMeshIsBeingDestroyed )
		return;

	g_bDoNavSaveComputations = true;

	const HidingSpotList &hidingSpots = GetNavAreaHidingSpotList( area );

	FOR_EACH_LL( hidingSpots, hsi )
	{
		const uint id = GetHidingSpotID( hidingSpots[ hsi ] );

		FOR_EACH_VEC( TheLookSpots, lsi )
		{
			if( TheLookSpots[ lsi ].spotID == id )
				TheLookSpots.Remove( lsi );
		}

		DisconnectedHidingSpots.AddToTail( hidingSpots[ hsi ] );
	}
}

//=======================================================================================================================

const char *GetNavFileName( void )
{
	char gamePath[256];
	engine->GetGameDir( gamePath, 256 );

	static char filename[256];
	Q_snprintf( filename, sizeof( filename ), "%s\\maps\\%s.nav", gamePath, STRING( gpGlobals->mapname ) );

	return filename;
}

//=======================================================================================================================

void ComputeApproachAreas( CNavArea *pArea );
void ComputeSpotEncounters( CNavArea *pArea );
void ComputeEarliestOccupyTime( CNavArea *pArea );

bool PreNavMeshSave( void )
{
	// Don't do this if the nav mesh was just auto-analyzed or no edits were done that would require this
	if( g_bDoNavSaveComputations
	&& (!g_pCvNavQuicksave || !g_pCvNavQuicksave->GetBool())
	&& engine->Cmd_Argc()
	&& !stricmp( engine->Cmd_Argv( 0 ), "nav_save" ) )
	{
		// Temporarily remove all disconnected hiding spots from TheHidingSpotList
		// so we don't compute spot encounters for hiding spots that will be removed
		FOR_EACH_VEC( DisconnectedHidingSpots, it )
		{
			TheHidingSpotList->FindAndRemove( DisconnectedHidingSpots[ it ] );
		}

		// TODO: add progress bar like on nav generation? <-- Probably requires running this every frame...
		// -------------------------------------------------------------------
		
		// 1. Compute approach areas
		FOR_EACH_LL( (*TheNavAreaList), it )
		{
			CNavArea *area = TheNavAreaList->Element( it );

			ComputeApproachAreas( area );
		}

		// 2. Compute spot encounters
		FOR_EACH_LL( (*TheNavAreaList), it )
		{
			CNavArea *area = TheNavAreaList->Element( it );

			ComputeSpotEncounters( area );
		}

		// 3. Compute earliest occupy times
		FOR_EACH_LL( (*TheNavAreaList), it )
		{
			CNavArea *area = TheNavAreaList->Element( it );

			ComputeEarliestOccupyTime( area );
		}
		
		// -------------------------------------------------------------------

		// Add the disconnected hiding spots back so they can be free'd at the end of the map
		FOR_EACH_VEC( DisconnectedHidingSpots, it )
		{
			TheHidingSpotList->AddToTail( DisconnectedHidingSpots[ it ] );
		}
	}

	g_bDoNavSaveComputations = false;

	RETURN_META_VALUE( MRES_HANDLED, true );
}

//=======================================================================================================================

enum {
	LS_FILE_LS_PLUS_ID = 1,
	LS_FILE_MAX_VERSION = LS_FILE_LS_PLUS_ID
};
const uint LS_FILE_VERSION = LS_FILE_MAX_VERSION;
const uint LS_FILE_IDENTIFIER = 0x90D4B1D;

// Save look-spots and a nav identifier at the end of the nav file
bool PostNavMeshSave( void )
{
	bool result = META_RESULT_ORIG_RET(bool);

	if( !result )
	{
		RETURN_META_VALUE( MRES_IGNORED, result );
	}

	// Saving after auto-generation?
	if( !engine->Cmd_Argc() || stricmp( engine->Cmd_Argv( 0 ), "nav_save" ) )
	{
		RETURN_META_VALUE( MRES_IGNORED, result );
	}

	const char *identifier = (g_szNavIdentifier[0] ? g_szNavIdentifier : NULL);

	if( TheLookSpots.Count() == 0 && (!identifier && engine->Cmd_Argc() < 2) )
	{
		// No look-spots or nav identifier to save
		RETURN_META_VALUE( MRES_IGNORED, result );
	}

	// Save look-spots at the end of the nav file
	// Also save an identifier (piece of text supplied by the user)
	const char *filename = GetNavFileName();

	if( !filesystem->FileExists( filename ) )
	{
		RETURN_META_VALUE( MRES_IGNORED, result );
	}

	FileHandle_t file = filesystem->Open( filename, "ab" );

	if( !file )
	{
		RETURN_META_VALUE( MRES_IGNORED, result );
	}

	// Keep track of how many bytes we're writing
	const uint startPos = filesystem->Size( file );

	// Write file version
	filesystem->Write( &LS_FILE_VERSION, sizeof(uint), file );

	// Write look-spots
	int count = TheLookSpots.Count();
	filesystem->Write( &count, sizeof(int), file );

	FOR_EACH_VEC( TheLookSpots, i )
	{
		const HidingSpotLookInfo &info = TheLookSpots[ i ];

		filesystem->Write( &info.spotID, sizeof(uint), file );
		filesystem->Write( &info.target, sizeof(Vector), file );
	}

	// Write nav identifier
	int identifierLen = 0;

	if( engine->Cmd_Argc() >= 2 ) // Set a new nav identifier that came with the command?
	{
		identifier = engine->Cmd_Argv( 1 );
		identifierLen = strnlen_s( identifier, MAX_NAV_ID_LEN-1 );
		strncpy( g_szNavIdentifier, identifier, MAX_NAV_ID_LEN-1 );
		g_szNavIdentifier[ identifierLen ] = '\000';
	}
	else if( identifier ) // Using a previously set nav identifier from g_szNavIdentifier?
	{
		identifierLen = strnlen_s( identifier, MAX_NAV_ID_LEN-1 );
	}

	filesystem->Write( &identifierLen, sizeof(int), file );

	if( identifierLen > 0 )
	{
		filesystem->Write( identifier, identifierLen, file );
	}

	// Write number of bytes written and file identifier
	const uint endPos = filesystem->Tell( file );
	const int bytes = (endPos - startPos) + 2 * sizeof(uint);

	filesystem->Write( &bytes, sizeof(int), file );
	filesystem->Write( &LS_FILE_IDENTIFIER, sizeof(uint), file );

	filesystem->Flush( file );
	filesystem->Close( file );

	RETURN_META_VALUE( MRES_HANDLED, result );
}

//=======================================================================================================================

// Try to load look-spots and the nav identifier from the nav file.
// Also load the danger areas from their own file.
NavErrorType PostNavMeshLoad( void )
{
	g_bDoNavSaveComputations = false;

	NavErrorType result = META_RESULT_ORIG_RET(NavErrorType);

	if( result != NAV_OK )
	{
		RETURN_META_VALUE( MRES_IGNORED, result );
	}

	// Load danger areas first in case something goes wrong here later on
	LoadDangerAreasFromFile();

	const char *filename = GetNavFileName();
	FileHandle_t file = filesystem->Open( filename, "rb" );

	if( !file )
	{
		RETURN_META_VALUE( MRES_IGNORED, result );
	}

	const int infoSize = 2 * sizeof(uint);
	if( filesystem->Size( file ) < infoSize )
	{
		filesystem->Close( file );
		RETURN_META_VALUE( MRES_IGNORED, result );
	}

	// Check the file identifier and data size from the end of the file
	filesystem->Seek( file, -infoSize, FILESYSTEM_SEEK_TAIL );

	int bytes;
	uint file_id;
	filesystem->Read( &bytes, sizeof(int), file );
	filesystem->Read( &file_id, sizeof(uint), file );

	// Does this nav file have any extra data saved?
	if( file_id != LS_FILE_IDENTIFIER )
	{
		filesystem->Close( file );
		RETURN_META_VALUE( MRES_IGNORED, result );
	}

	// Go to the beginning of the extra data and start reading
	filesystem->Seek( file, -bytes, FILESYSTEM_SEEK_TAIL );

	// Read version number
	uint version;
	filesystem->Read( &version, sizeof(uint), file );

	if( version < LS_FILE_LS_PLUS_ID || version > LS_FILE_MAX_VERSION )
	{
		filesystem->Close( file );
		RETURN_META_VALUE( MRES_IGNORED, result );
	}

	// Read look-spots
	TheLookSpots.Purge();

	int count;
	filesystem->Read( &count, sizeof(int), file );

	for( int i = 0; i < count; ++i )
	{
		HidingSpotLookInfo info;

		filesystem->Read( &info.spotID, sizeof(uint), file );
		filesystem->Read( &info.target, sizeof(Vector), file );

		if( GetHidingSpotByID( info.spotID ) == NULL )
		{
			Msg( "WARNING: Hiding spot %d for look-spot not found!\n", info.spotID );
		}

		TheLookSpots.AddToTail( info );
	}

	// Read nav identifier
	g_szNavIdentifier[ 0 ] = '\000';

	int identifierLen;
	filesystem->Read( &identifierLen, sizeof(int), file );

	if( identifierLen < 0 || identifierLen >= MAX_NAV_ID_LEN )
	{
		filesystem->Close( file );
		RETURN_META_VALUE( MRES_IGNORED, result );
	}

	if( identifierLen > 0 )
	{
		filesystem->Read( g_szNavIdentifier, identifierLen, file );
	}

	g_szNavIdentifier[ identifierLen ] = '\000';

	// Done reading
	filesystem->Close( file );

	RETURN_META_VALUE( MRES_HANDLED, result );
}

//=======================================================================================================================

enum PriorityType
{
	PRIORITY_LOW, PRIORITY_MEDIUM, PRIORITY_HIGH, PRIORITY_UNINTERRUPTABLE
};

#define INHIBIT_LOOK_AROUND	true

void BotLookAtSpot( CCSBot *me, const Vector &spot, float duration, PriorityType priority, bool inhibitLookAround = false )
{
	const int LOOK_TOWARDS_SPOT = 1;
	const int iInhibitLookAroundTimestampOffs = 12876;
	const int iLookAtSpotStateOffs = 12880;
	const int iLookAtSpotOffs = 12884;
	const int iLookAtSpotPriority = 12896;
	const int iLookAtSpotDuration = 12900;
	const int iLookAtSpotAngleTolerance = 12908;
	const int iLookAtSpotClearIfClose = 12912;
	const int iLookAtSpotAttack = 12913;
	const int iLookAtDesc = 12914;

	// TODO: check priority of current spot

	*(int*)((char*)me + iLookAtSpotStateOffs) = LOOK_TOWARDS_SPOT;
	*(Vector*)((char*)me + iLookAtSpotOffs) = spot;
	*(PriorityType*)((char*)me + iLookAtSpotPriority) = priority;
	*(float*)((char*)me + iLookAtSpotDuration) = duration;
	*(float*)((char*)me + iLookAtSpotAngleTolerance) = 0.97f;
	*(bool*)((char*)me + iLookAtSpotClearIfClose) = false;
	*(bool*)((char*)me + iLookAtSpotAttack) = false;
	*((const char**)me + iLookAtDesc) = "Custom spot";

	if( inhibitLookAround )
	{
		*(float*)((char*)me + iInhibitLookAroundTimestampOffs) = gpGlobals->curtime + duration;
	}
}

//=======================================================================================================================

HidingSpot *GetNavAreaClosestHidingSpot( CNavArea *pArea, const Vector &vecPosition )
{
	if( !pArea )
		return NULL;

	const HidingSpotList &hidingSpots = GetNavAreaHidingSpotList( pArea );

	const int posOffs = 4;
	HidingSpot *pClosestSpot = NULL;

	// Bot has to be less than 20 units away from the spot
	float fClosestDistance = 20.f * 20.f;

	// Check which hiding spot the bot is at
	FOR_EACH_LL( hidingSpots, it )
	{
		HidingSpot *spot = hidingSpots[ it ];

		Vector vecSpotOrigin = *(Vector*)((char*)spot + posOffs);

		float dist = (vecSpotOrigin - vecPosition).LengthSqr();

		if( dist < fClosestDistance )
		{
			fClosestDistance = dist;
			pClosestSpot = spot;
		}
	}

	return pClosestSpot;
}

//=======================================================================================================================

bool ShouldBlockHide( CCSBot *me )
{
	if( !g_bHaveDoneRoundMoneyCheck )
	{
		DoRoundMoneyCheck();
	}
	
	const int teamID = GetTeamNumber( me );
	const float fMaxTeamPathUseTime = 45.f;
	const int isBombPlantedOffs = 6704;

	// T's are allowed to hide if bomb is planted
	if( teamID == TEAM_T && (*(char*)TheBots + isBombPlantedOffs) == 1 )
		return false;

	// A rushing team member is not allowed to hide
	if( g_iTeamSafePathIndex[ teamID % NUM_TEAMS ] != -1
	&& gpGlobals->curtime - g_fRoundStartTimestamp < fMaxTeamPathUseTime )
	{
		BotHunt( me ); // Make the bot hunt so it doesn't start camping
		return true;
	}

	return false;
}

//=======================================================================================================================

void OnBotReachHidingSpot( CCSBot *me, void *pHideState )
{
	const int lastKnownAreaOffs = 5912;

	CNavArea *pArea = *(CNavArea**)((char*)me + lastKnownAreaOffs);

	if( !pArea )
		return;

	const Vector &vecBotOrigin = GetAbsOrigin( me );

	HidingSpot *pSpot = GetNavAreaClosestHidingSpot( pArea, vecBotOrigin );

	if( !pSpot )
	{
		DBG_CODE(
			g_SMAPI->ConPrint( "Failed to find bot's hiding spot\n" );
		);
		return;
	}

	uint uClosestSpotID = GetHidingSpotID( pSpot );
	Vector vecLookSpot; // Init to NAN

	// Find the look-spot for this hiding spot
	FOR_EACH_VEC( TheLookSpots, it )
	{
		if( TheLookSpots[ it ].spotID == uClosestSpotID )
		{
			vecLookSpot = TheLookSpots[ it ].target;
			break;
		}
	}

	if( !vecLookSpot.IsValid() )
		return;

	// Make the bot look at the spot and inhibit look-around behavior
	const int durationOffs = 28;

	float duration = *(float*)((char*)pHideState + durationOffs);

	BotLookAtSpot( me, vecLookSpot, duration, PRIORITY_LOW, INHIBIT_LOOK_AROUND );

	DBG_CODE(
		g_SMAPI->ConPrintf( "Found angles for hiding spot %d\n", uClosestSpotID );
	);
}

//=======================================================================================================================

// Removes approach areas and spot encounters from all nav areas
bool StripNavigationAreas( void )
{
	if( !CNavMesh_StripNavigationAreas_Sig.IsSet() )
	{
		Msg( "CNavMesh::StripNavigationAreas address not found\n" );
		return false;
	}

	union
	{
		void (FnEmptyClass::*mfpnew)();
		void* addr;
	}
	u;
	u.addr = CNavMesh_StripNavigationAreas_Sig.Address();

	(reinterpret_cast<FnEmptyClass*>(TheNavMesh)->*u.mfpnew)();

	return true;
}

//=======================================================================================================================

bool RemoveLookSpotFromHidingSpot( HidingSpot *spot )
{
	if( !spot )
		return false;

	uint uSpotID = GetHidingSpotID( spot );

	FOR_EACH_VEC( TheLookSpots, it )
	{
		if( TheLookSpots[ it ].spotID == uSpotID )
		{
			TheLookSpots.Remove( it );
			return true;
		}
	}

	return false;
}

//=======================================================================================================================

bool RemoveHidingSpotFromList( HidingSpot *spot, HidingSpotList &list )
{
	FOR_EACH_LL( list, it )
	{
		if( list[ it ] == spot )
		{
			list.Remove( it );
			return true;
		}
	}

	return false;
}

//=======================================================================================================================

// Return true if cmd should be superceded
bool HandleHidingSpotCmd( edict_t *pEntity )
{
	const char *cmd = engine->Cmd_Argv( 1 );
	const int argc = engine->Cmd_Argc();

	IPlayerInfo *pInfo = playerinfomgr->GetPlayerInfo( pEntity );

	if( !pInfo || pInfo->IsFakeClient() )
	{
		return false;
	}
	if( pInfo->IsDead() || pInfo->GetTeamIndex() < TEAM_T )
	{
		return true;
	}

	if( !g_pCvNavEdit || !g_pCvNavEdit->GetBool() )
	{
		engine->ClientPrintf( pEntity, "nav_edit has to be on\n" );
		return true;
	}

	if( !stricmp( cmd, "add" ) )
	{
		unsigned char spotFlags = 0;
		bool saveLookPosition = true;

		for( int i = 2; i < argc; ++i )
		{
			cmd = engine->Cmd_Argv( i );

			if( !stricmp( cmd, "noview" ) )
				saveLookPosition = false;
			else if( !stricmp( cmd, "in_cover" ) )
				spotFlags = IN_COVER;
			else if( !stricmp( cmd, "sniper_good" ) )
				spotFlags = GOOD_SNIPER_SPOT;
			else if( !stricmp( cmd, "sniper_ideal" ) )
				spotFlags = IDEAL_SNIPER_SPOT;
			else if( !stricmp( cmd, "exposed" ) )
				spotFlags = EXPOSED;
			else
			{
				engine->ClientPrintf( pEntity, "Invalid hiding spot type\n" );
				return true;
			}
		}

		if( !spotFlags )
		{
			engine->ClientPrintf( pEntity, "Hiding spot type is required. Valid types are:\n" );
			engine->ClientPrintf( pEntity, "  in_cover\n  sniper_good\n  sniper_ideal\n  exposed\n" );
			return true;
		}

		const Vector vecOrigin( pInfo->GetAbsOrigin() );

		// A user-defined hiding spot still has to be on a nav area
		// (If you change this to GetNearestNavArea, remember to uncomment the detour for HidingSpotPostLoad)
		CNavArea *area = GetNavArea( vecOrigin + Vector( 0.f, 0.f, 36.f ) );

		if( !area )
		{
			engine->ClientPrintf( pEntity, "Nav area for hiding spot not found\n" );
			return true;
		}

		HidingSpot *spot = CreateHidingSpot();

		if( !spot )
		{
			engine->ClientPrintf( pEntity, "Failed to create hiding spot\n" );
			return true;
		}

		const int posOffs = 4;
		const int flagsOffs = 28;

		*(Vector *)((char*)spot + posOffs) = vecOrigin;
		*((unsigned char*)spot + flagsOffs) = spotFlags;
		uint spotID = GetHidingSpotID( spot );

		GetNavAreaHidingSpotList( area ).AddToTail( spot );

		g_bDoNavSaveComputations = true;

		// Add view info to the look-spots vector
		if( saveLookPosition )
		{
			// Assume the player is standing, since this shouldn't have to be too accurate anyways.
			Vector eyePosition = vecOrigin + Vector( 0.f, 0.f, 64.f );
			Vector dir;

			const QAngle &eyeAngles = GetEyeAngles( pEntity );

			AngleVectors( eyeAngles, &dir );
			const Vector spotTarget = eyePosition + dir * 2000.f;

			HidingSpotLookInfo lookInfo;

			lookInfo.target = spotTarget;
			lookInfo.spotID = spotID;

			TheLookSpots.AddToTail( lookInfo );
		}

		g_SMAPI->ClientConPrintf( pEntity, "Hiding spot created at your location%s\n", saveLookPosition ? " with your eye angles" : "" );
		return true;
	}
	else if( !stricmp( cmd, "remove" ) )
	{
		const Vector vecOrigin( pInfo->GetAbsOrigin() );

		CNavArea *area = GetNavArea( vecOrigin + Vector( 0.f, 0.f, 36.f ) );
		HidingSpot *pClosestSpot = GetNavAreaClosestHidingSpot( area, vecOrigin );

		if( argc > 2 && !stricmp( engine->Cmd_Argv( 2 ), "lookspot" ) )
		{
			if( argc == 4 && !stricmp( engine->Cmd_Argv( 3 ), "all" ) )
			{
				TheLookSpots.Purge();
				engine->ClientPrintf( pEntity, "All hiding spot look-spots have been removed\n" );
				return true;
			}

			if( !pClosestSpot )
			{
				engine->ClientPrintf( pEntity, "Hiding spot not found\n" );
				return true;
			}

			if( RemoveLookSpotFromHidingSpot( pClosestSpot ) )
			{
				engine->ClientPrintf( pEntity, "Look-spot removed from hiding spot\n" );
			}
			else
			{
				engine->ClientPrintf( pEntity, "No look-spot for hiding spot was found\n" );
			}
		}
		else // Remove a hiding spot
		{
			if( !pClosestSpot )
			{
				engine->ClientPrintf( pEntity, "Hiding spot not found\n" );
				return true;
			}

			// Do *NOT* remove the hiding spot from TheHidingSpotList (see notes on ComputeSpotEncounters)
			// Just remove it from the nav area's hiding spot list
			RemoveHidingSpotFromList( pClosestSpot, GetNavAreaHidingSpotList( area ) );
			RemoveLookSpotFromHidingSpot( pClosestSpot );
			StripNavigationAreas(); // Invalidate all spot encounters
			DisconnectedHidingSpots.AddToTail( pClosestSpot );
			g_bDoNavSaveComputations = true;

			engine->ClientPrintf( pEntity, "Hiding spot removed\n" );
		}

		return true;
	}

	return false;
}

//=======================================================================================================================

// ISSUE (FIXED?) : If you do this, spot encounters will be computed for hiding spots that will not be saved in the .nav file,
// because deleting/splitting/merging etc. does not delete the hiding spot objects when the nav area that they belong to is deleted.
// All the spots that are not connected to any areas anymore stay on TheHidingSpotList until the NavMesh is destroyed at the end of the map.
// Normally this is not a problem, because all spot encounters are invalidated when deleting/splitting/merging areas, but if you compute them again,
// the encounters are computed against all spots on TheHidingSpotList. When the nav is then later loaded, some of the spot encounters will (silently) have NULL hiding areas,
// which could cause problems. The fact that the hiding spots stay on TheHidingSpotList and are deleted at the end of the map can be used as a safe way to remove user-created hiding spots
// without having to worry about memory leaks, though. They would simply have to be disconnected from the nav area so they are not saved on nav_save.
// SOLUTION (implemented) : All the hiding spots that do not belong to any nav area can be temporarily removed from the HidingSpotList for the computing, and added back afterwards.
void ComputeSpotEncounters( CNavArea *pArea )
{
	if( !CNavArea_ComputeSpotEncounters_Sig.IsSet() )
	{
		Msg( "CNavArea::ComputeSpotEncounters address not found\n" );
		return;
	}

	union
	{
		void (FnEmptyClass::*mfpnew)();
		void* addr;
	}
	u;
	u.addr = CNavArea_ComputeSpotEncounters_Sig.Address();

	(reinterpret_cast<FnEmptyClass*>(pArea)->*u.mfpnew)();
}

//=======================================================================================================================

void ComputeApproachAreas( CNavArea *pArea )
{
	if( !CNavArea_ComputeApproachAreas_Sig.IsSet() )
	{
		Msg( "CNavArea::ComputeApproachAreas address not found\n" );
		return;
	}

	union
	{
		void (FnEmptyClass::*mfpnew)();
		void* addr;
	}
	u;
	u.addr = CNavArea_ComputeApproachAreas_Sig.Address();

	(reinterpret_cast<FnEmptyClass*>(pArea)->*u.mfpnew)();
}

//=======================================================================================================================

void ComputeEarliestOccupyTime( CNavArea *pArea )
{
	if( !CNavArea_ComputeEarliestOccupyTimes_Sig.IsSet() )
	{
		Msg( "CNavArea::ComputeEarliestOccupyTimes address not found\n" );
		return;
	}

	union
	{
		void (FnEmptyClass::*mfpnew)();
		void* addr;
	}
	u;
	u.addr = CNavArea_ComputeEarliestOccupyTimes_Sig.Address();

	(reinterpret_cast<FnEmptyClass*>(pArea)->*u.mfpnew)();
}

//=======================================================================================================================

CON_COMMAND(nav_remove_all_hiding_spots, "Removes all hiding spots from the navigation mesh.")
{
	if( engine->IsDedicatedServer() )
		return;

	if( !g_pCvNavEdit || !g_pCvNavEdit->GetBool() )
		return;

	DestroyHidingSpots();
	TheLookSpots.Purge();
	DisconnectedHidingSpots.Purge();

	// Empty the spot encounter list of all nav areas
	StripNavigationAreas();
}

//=======================================================================================================================

CON_COMMAND(nav_identify, "Prints the currently loaded navigation mesh's identifying string if it has one.")
{
	if( g_szNavIdentifier[ 0 ] )
	{
		Msg( "Navigation mesh identifier: %s\n", g_szNavIdentifier );
	}
	else
	{
		Msg( "This navigation mesh has no identifier.\n" );
	}
}

//=======================================================================================================================