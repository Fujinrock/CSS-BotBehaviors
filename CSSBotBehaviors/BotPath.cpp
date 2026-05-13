#include "BotPath.h"
#include "BotHiding.h"
#include "SigScans.h"
#include "edict.h"
#include "eiface.h"
#include "convar.h"
#include "random.h"
#include "vector.h"
#include "playerinfomanager.h"
#include "basehandle.h"
#include "playerinfomanager.h"
#include "ihandleentity.h"
#include "filesystem.h"
#include "ISmmPlugin.h"
#include <string>

extern IVEngineServer *engine;
extern IPlayerInfoManager *playerinfomgr;
extern ISmmAPI *g_SMAPI;
extern SourceHook::ISourceHook *g_SHPtr;
extern CGlobalVars *gpGlobals;
extern IServerGameEnts *gameents;
extern IVEngineServer *engine;
extern IPlayerInfoManager *playerinfomgr;
extern IFileSystem *filesystem;

ConVar g_cvMaxBotBombFetchDistance( "bot_max_bomb_fetch_distance", "1600.0", FCVAR_NONE );
ConVar g_cvFullPathRandomization( "bot_full_path_randomization", "1", FCVAR_NONE, "Whether danger value should be randomized for every nav area." );
ConVar g_cvDangerAreaDebug( "danger_area_debug", "0", FCVAR_CHEAT );

NavAreaList *TheNavAreaList = *reinterpret_cast<NavAreaList **>( 0x221C2B11 );
CNavMesh *TheNavMesh = **reinterpret_cast<CNavMesh ***>( 0x2233B0DA );

CUtlVector< DangerArea * > TheDangerAreas;

//=======================================================================================================================

bool NavMeshIsGenerating( void )
{
	const int generationModeOffs = 1240;

	return *((char*)TheNavMesh + generationModeOffs) != 0;
}

//=======================================================================================================================

bool IsValidDangerAreaIdxForTeam( int idx, int teamID )
{
	if( idx < 0 || idx >= TheDangerAreas.Count() )
		return false;

	if( TheDangerAreas[ idx ]->iTeam == TEAM_BOTH || TheDangerAreas[ idx ]->iTeam == teamID )
		return true;

	return false;
}

//=======================================================================================================================

// Makes the bot hunt (sets the state machine to hunt state)
void BotHunt( CCSBot *pBot )
{
	const int huntStateOffs = 5524;
	const int stateOffs = 5872;

	void *pStateAdr = *reinterpret_cast<void**>((char*)pBot + stateOffs);

	// Already hunting?
	if( pStateAdr == ((char*)pBot + huntStateOffs) )
		return;

	union
	{
		void (FnEmptyClass::*mfpnew)();
		void* addr;
	}
	u;
	u.addr = CCSBot_Hunt_Sig.Address();
	
	(reinterpret_cast<FnEmptyClass*>(pBot)->*u.mfpnew)();
}

//=======================================================================================================================

float g_fPathRandomizedTimestamp[MAXPLAYERS] = {0.0f};
int g_iSafePathIndex[MAXPLAYERS];
int g_iLastSafePathIndex[MAXPLAYERS];
int g_iSafeSubRouteIndex[MAXPLAYERS];
bool g_bHaveDoneRoundMoneyCheck = false;
int g_iTeamSafePathIndex[NUM_TEAMS];
float g_fRoundStartTimestamp = 0.0;

void DoRoundMoneyCheck( void )
{
	g_bHaveDoneRoundMoneyCheck = true;

	if( !TheDangerAreas.Count() )
	{
		g_iTeamSafePathIndex[ 0 ] = -1;
		g_iTeamSafePathIndex[ 1 ] = -1;
		return;
	}

	int iRoundNumber = (GetNumRoundsPlayed() % 15) + 1; // Assume MR15
	bool bIsPistolRound = iRoundNumber == 1;

	// No point in checking money when it's the first round
	if( bIsPistolRound )
	{
		for( int i = 0; i < NUM_TEAMS; ++i )
		{
			float fTeamRushChance = (i == 0)? 25.f : 5.f;

			if( RandomFloat( 0.f, 100.f ) < fTeamRushChance )
			{
				g_iTeamSafePathIndex[ i ] = GetRandomDangerAreaIdxForTeam( i+2, FOR_TEAM );
			}
			else
			{
				g_iTeamSafePathIndex[ i ] = -1;
			}
		}

		return;
	}

	const int hasHelmetOffs = 3596;
	const int accountOffs = 3612;

	int iTotalMoney[NUM_TEAMS] = {0};
	int iNumTeamMembers[NUM_TEAMS] = {0};
	int iNumHaveHelmet[NUM_TEAMS] = {0};	// Having a helmet is not very eco-like...
	int iNumHavePrimary[NUM_TEAMS] = {0};	// ... and neither is having a primary

	for( int i = 1; i < gpGlobals->maxClients; ++i )
	{
		edict_t *pEdict = engine->PEntityOfEntIndex( i );

		if( !pEdict || pEdict->IsFree() || !pEdict->GetUnknown() )
			continue;

		CBaseEntity *pEnt = pEdict->GetUnknown()->GetBaseEntity();

		if( !pEnt )
			continue;

		int teamID = GetTeamNumber( pEnt );

		if( teamID < TEAM_T )
			continue;

		++iNumTeamMembers[teamID % NUM_TEAMS];

		iTotalMoney[teamID % NUM_TEAMS] += *(int*)((char*)pEnt + accountOffs);

		bool bHasHelmet = *((char*)pEnt + hasHelmetOffs) == 1;

		if( bHasHelmet )
			++iNumHaveHelmet[teamID % NUM_TEAMS];

		if( PlayerHasWeaponInSlot( pEnt, SLOT_PRIMARY ) )
			++iNumHavePrimary[teamID % NUM_TEAMS];
	}

	const int iEcoThreshold = iRoundNumber > 2? 2700 : 2200; // Lower threshold for 2nd round (smg round)

	for( int i = 0; i < 2; ++i )
	{
		if( iNumTeamMembers[ i ] == 0 )
		{
			g_iTeamSafePathIndex[ i ] = -1;
			continue;
		}

		float fTeamRushChance = (i == 0)? 19.f : 6.f; // Less rush chance for CT
		bool bIsEcoRound = true;
		const int iTeamAvgMoney = iTotalMoney[ i ] / iNumTeamMembers[ i ];

		if( iTeamAvgMoney > iEcoThreshold )
		{
			bIsEcoRound = false;
		}

		const int iEquipmentAmt = iNumHaveHelmet[ i ] + iNumHavePrimary[ i ];
		const float fEquipmentRatio = ((float)iEquipmentAmt / iNumTeamMembers[ i ]);
		const float fMaxEquipmentRatio = iRoundNumber > 2? 1.f : 0.7f;

		if( fEquipmentRatio > fMaxEquipmentRatio)
		{
			bIsEcoRound = false;
		}

		if( bIsEcoRound )
		{
			fTeamRushChance = (i == 0)? 85.f : 70.f;
		}

		if( RandomFloat( 0.f, 100.f ) < fTeamRushChance )
		{
			g_iTeamSafePathIndex[ i ] = GetRandomDangerAreaIdxForTeam( i+2, FOR_TEAM );
		}
		else
		{
			g_iTeamSafePathIndex[ i ] = -1;
		}

		if( g_cvDangerAreaDebug.GetBool() )
		{
			g_SMAPI->ConPrintf( "Team %s is %son eco\n", i == 0? "T" : "CT", bIsEcoRound ? "" : "NOT " );
			g_SMAPI->ConPrintf( " L Avg money: $%d\n", iTeamAvgMoney );
			g_SMAPI->ConPrintf( " L Equip. ratio: %.02f\n", fEquipmentRatio );

			if( g_iTeamSafePathIndex[ i ] == -1 )
			{
				g_SMAPI->ConPrint( " L Not using a team path\n" );
			}
			else
			{
				const DangerArea *da = TheDangerAreas[ g_iTeamSafePathIndex[ i ] ];

				g_SMAPI->ConPrintf( " L Team path: %s\n", da->szAreaName[0]? da->szAreaName : "unnamed" );
			}
		}
	}
}

//=======================================================================================================================

// Stop bots from trying to open doors that are already open
bool CBotDoorEnumerator_EnumElement( IHandleEntity *pHandleEntity )
{
	const CBaseHandle &handle = pHandleEntity->GetRefEHandle();

	edict_t *pEdict = engine->PEntityOfEntIndex( handle.GetEntryIndex() );

	CBaseEntity *pEntity = gameents->EdictToBaseEntity( pEdict );

	if( !pEntity )
	{
		return false;
	}

	// TODO: check object caps first (vcall)?

	const int m_toggle_stateOffs = 848; // For func_doors
	const int m_eDoorStateOffs = 1536; // For prop_doors

	const char *classname = pEdict->GetClassName();
	int istate;

	const int PROP_DOOR_CLOSED = 0;
	const int FUNC_DOOR_CLOSED = 1;

	if( Q_strstr( classname, "prop_door" ) == classname )
	{
		istate = *(int*)((char*)pEntity + m_eDoorStateOffs);

		if( istate != PROP_DOOR_CLOSED )	// Something else than "closed"?
			return false;					// Don't consider this door
	}
	else if( Q_strstr( classname, "func_door" ) == classname )
	{
		istate = *(int*)((char*)pEntity + m_toggle_stateOffs);

		if( istate != FUNC_DOOR_CLOSED )	// Same as above comment ^
			return false;
	}
	else // Not a door, stop with this entity
	{
		return false;
	}

	return true; // This door is fine to try opening, send it to the original function
}

//=======================================================================================================================

void OnBotComputePath( CCSBot *me, const Vector & goal, RouteType route )
{
	if( !g_bHaveDoneRoundMoneyCheck )
	{
		DoRoundMoneyCheck();
	}

	float repathTimestamp = *(float*)((char*)me + 12092); // < This is from the beginning of ComputePath

	if( !(gpGlobals->curtime > repathTimestamp)
	|| !TheDangerAreas.Count() )
	{
		return;
	}

	const int teamID = GetTeamNumber( me );
	const float fMaxTeamPathUseTime = 45.f;

	if( route != SAFEST_ROUTE )
	{
		if( g_iTeamSafePathIndex[ teamID % NUM_TEAMS ] != -1
		&& gpGlobals->curtime - g_fRoundStartTimestamp < fMaxTeamPathUseTime )
		{
			BotHunt( me ); // Make the bot hunt so it doesn't start camping
		}
		return;
	}

	// Don't randomize if bot is close to goal
	const Vector &botpos = GetAbsOrigin( me );
	const float minDist = 512.f;

	if( (goal - botpos).IsLengthLessThan( minDist ) )
		return;

	// Randomize danger a bit for all areas
	const int dangerValOffs = 68;
	const int dangerTimestampOffs = 76;

	if( g_cvFullPathRandomization.GetBool() )
	{
		FOR_EACH_LL( (*TheNavAreaList), it )
		{
			CNavArea *area = TheNavAreaList->Element( it );

			*(float*)((char*)area + dangerValOffs + (teamID == TEAM_T ? 0 : 4)) = RandomFloat( 0.0f, 10.0f );
			*(float*)((char*)area + dangerTimestampOffs + (teamID == TEAM_T ? 0 : 4)) = gpGlobals->curtime;
		}
	}

	// Throttle how often the path is changed
	edict_t *pEdict = gameents->BaseEntityToEdict( (CBaseEntity *)me );
	const int entidx = engine->IndexOfEdict( pEdict );
	const int slot = entidx % MAXPLAYERS;

	const float fRerandomizeDelay = 20.f;
	const float fTimestamp = g_fPathRandomizedTimestamp[ slot ];

	if( fTimestamp == 0.0f || gpGlobals->curtime - fTimestamp > fRerandomizeDelay
	|| !IsValidDangerAreaIdxForTeam( g_iSafePathIndex[ slot ], teamID ) )
	{
		g_iSafePathIndex[ slot ] = GetRandomDangerAreaIdxForTeam( teamID );
		g_fPathRandomizedTimestamp[ slot ] = gpGlobals->curtime;
	}

	// Determine the main route to take
	int safeRouteIdx;

	if( g_iTeamSafePathIndex[ teamID % NUM_TEAMS ] != -1
	&& gpGlobals->curtime - g_fRoundStartTimestamp < fMaxTeamPathUseTime )
	{
		safeRouteIdx = g_iTeamSafePathIndex[ teamID % NUM_TEAMS ];
	}
	else
	{
		safeRouteIdx = g_iSafePathIndex[ slot ];
	}

	const DangerArea *safeArea = TheDangerAreas[ safeRouteIdx ];

	// Determine the sub route to take
	int safeSubRouteIdx = -1;

	if( safeRouteIdx != g_iLastSafePathIndex[ slot ] )
	{
		if( safeArea->iNumLinkedAreas )
		{
			safeSubRouteIdx = GetRandomSubRouteIdxForArea( safeArea );
			g_iSafeSubRouteIndex[ slot ] = safeSubRouteIdx;
		}
		else
		{
			g_iSafeSubRouteIndex[ slot ] = -1;
		}
	}
	else
	{
		safeSubRouteIdx = g_iSafeSubRouteIndex[ slot ];
	}

	g_iLastSafePathIndex[ slot ] = safeRouteIdx;

	const float dangerAmount = 10000.f;

	// Make all the safe area's linked areas dangerous too except for the one sub-route
	if( safeArea->iNumLinkedAreas )
	{
		for( int i = 0; i < safeArea->iNumLinkedAreas; ++i )
		{
			if( i == safeSubRouteIdx )
				continue;

			const DangerArea *linkedArea = safeArea->pLinkedAreas[ i ];

			IncreaseDangerNearby( teamID, dangerAmount, linkedArea->pNearestArea, linkedArea->vecPosition, linkedArea->fMaxDistance );
		}
	}

	// Make all main routes except one dangerous
	for( int i = 0; i < TheDangerAreas.Count(); ++i )
	{
		if( i == safeRouteIdx )
			continue;

		const DangerArea *da = TheDangerAreas[ i ];

		if( da->iTeam != TEAM_BOTH && da->iTeam != teamID )
			continue;

		IncreaseDangerNearby( teamID, dangerAmount, da->pNearestArea, da->vecPosition, da->fMaxDistance );
	}

	if( g_cvDangerAreaDebug.GetBool() )
	{
		IPlayerInfo *pInfo = playerinfomgr->GetPlayerInfo( pEdict );

		if( pInfo )
		{
			g_SMAPI->ConPrintf( "%s (%s) is going via %s",
				pInfo->GetName(),
				(teamID == TEAM_T? "T" : "CT"),
				(TheDangerAreas[ safeRouteIdx ]->szAreaName[0] ? TheDangerAreas[ safeRouteIdx ]->szAreaName : "unnamed") );

			if( safeSubRouteIdx != -1 )
			{
				DangerArea *subarea = TheDangerAreas[ safeRouteIdx ]->pLinkedAreas[ safeSubRouteIdx ];

				if( subarea->szAreaName[0] )
					g_SMAPI->ConPrintf( " (subroute %s)", subarea->szAreaName );
				else
					g_SMAPI->ConPrintf( " (subroute %d)", safeSubRouteIdx );
			}

			g_SMAPI->ConPrint( "\n" );
		}
	}
}

//=======================================================================================================================

bool NoticeLooseBomb( CCSBot *me )
{
	const int bombOffs = 6720;

	CBaseHandle *hBomb = (CBaseHandle*)((char*)TheBots + bombOffs);

	edict_t *pEdict = engine->PEntityOfEntIndex( hBomb->GetEntryIndex() );

	if( !pEdict || !pEdict->GetUnknown() )
	{
		return false;
	}

	CBaseEntity *pEnt = pEdict->GetUnknown()->GetBaseEntity();

	if( pEnt )
	{
		const Vector &vecBombAbsOrigin = GetAbsOrigin( pEnt );
		const Vector &vecBotAbsOrigin = GetAbsOrigin( me );

		vec_t distance = vecBotAbsOrigin.DistTo( vecBombAbsOrigin );

		const float maxFetchDistance = g_cvMaxBotBombFetchDistance.GetFloat();

		// Bots shouldn't go fetch the bomb if they're on the other side of the map
		if( distance > maxFetchDistance )
		{
			return false;
		}
		else // Randomize based on distance where the bot notices the bomb or not
		{
			const float baseChance = 2.f;
			const float stepsize = 100.f;
			const float amount = 0.09f;

			float chance = baseChance - (distance / stepsize * amount);

			if( chance < 0.03f )
				chance = 0.03f;

			if( RandomFloat( 0.0, 100.0 ) > chance )
				return false;
		}
	}

	return true;
}

//=======================================================================================================================

CNavArea *GetNearestNavArea( const Vector &pos, bool anyZ/* = false*/, float maxDist/* = 512.f*/, bool checkLOS/* = false*/ )
{
	union {
		CNavArea *(FnEmptyClass::*mfpnew)(const Vector &, bool, float, bool);
		void* addr;
	} u;
	u.addr = CNavMesh_GetNearestNavArea_Sig.Address();

	return (CNavArea*)(reinterpret_cast<FnEmptyClass*>(TheNavMesh)->*u.mfpnew)(pos, anyZ, maxDist, checkLOS);
}

//=======================================================================================================================

CNavArea *GetNavArea( const Vector &pos, float beneathLimit/* = 120.f*/ )
{
	union {
		CNavArea *(FnEmptyClass::*mfpnew)(const Vector &, float);
		void* addr;
	} u;
	u.addr = CNavMesh_GetNavArea_Sig.Address();

	return (CNavArea*)(reinterpret_cast<FnEmptyClass*>(TheNavMesh)->*u.mfpnew)(pos, beneathLimit);
}

//=======================================================================================================================

void IncreaseDangerNearby( int teamID, float amount, CNavArea *startArea, const Vector &pos, float maxRadius )
{
	union {
		CNavArea *(FnEmptyClass::*mfpnew)(int, float, CNavArea *, const Vector &, float);
		void* addr;
	} u;
	u.addr = CNavMesh_IncreaseDangerNearby_Sig.Address();

	(reinterpret_cast<FnEmptyClass*>(TheNavMesh)->*u.mfpnew)(teamID, amount, startArea, pos, maxRadius);
}

//=======================================================================================================================

int GetRandomDangerAreaIdxForTeam( int teamID, bool forTeam /*= false*/ )
{
	if( !TheDangerAreas.Count() || teamID < TEAM_BOTH || teamID > TEAM_CT )
		Error( "GetRandomDangerAreaIdxForTeam invalid call" );

	struct areainfo_t{ int index; float chance; };
	CUtlVector<areainfo_t> indices( 1, TheDangerAreas.Count() );

	float fMaxRandom = 0.0f;

	FOR_EACH_VEC( TheDangerAreas, it )
	{
		const DangerArea *pArea = TheDangerAreas[ it ];

		if( forTeam && !pArea->bUsableAsTeamRoute )
			continue;

		if( pArea->iTeam == TEAM_BOTH
		|| pArea->iTeam == teamID )
		{
			areainfo_t info;
			info.index = it;
			info.chance = pArea->fChance * 10.0f;

			indices.AddToTail( info );
			fMaxRandom += info.chance;
		}
	}

	if( !indices.Count() )
	{
		if( forTeam ) // This could cause infinite recursion, so disable it
		{
			Msg( "Warning: danger area index for team %s not found!\n", teamID == TEAM_T? "T" : "CT" );
			forTeam = false;
			return GetRandomDangerAreaIdxForTeam( teamID, forTeam );
		}

		return GetRandomDangerAreaIdxForTeam( GetOppositeTeamNumber( teamID ), forTeam );
	}

	// Get random area by chance
	const float fRandom = RandomFloat( 0.0f, fMaxRandom );
	const int lastElem = indices.Count() - 1;
	float fChanceChecked = 0.0f;

	FOR_EACH_VEC( indices, it )
	{
		if( it == lastElem )
			return indices[ it ].index;

		fChanceChecked += indices[ it ].chance;

		if( fRandom < fChanceChecked )
			return indices[ it ].index;
	}

	Error( "GetRandomDangerAreaIdxForTeam area not found" );
	return indices[0].index;
}

//=======================================================================================================================

int GetRandomSubRouteIdxForArea( const DangerArea *area )
{
	Assert( area->iNumLinkedAreas );

	const int numAreas = area->iNumLinkedAreas;
	float fMaxRandom = 0.0f;

	for(int i = 0; i < numAreas; ++i )
	{
		fMaxRandom += area->pLinkedAreas[ i ]->fChance * 10.f;
	}

	const float fRandom = RandomFloat( 0.0f, fMaxRandom );
	const int lastElem = numAreas - 1;
	float fChanceChecked = 0.0f;

	for(int i = 0; i < numAreas; ++i )
	{
		if( i == lastElem )
			return i;

		fChanceChecked += area->pLinkedAreas[ i ]->fChance * 10.f;

		if( fRandom < fChanceChecked )
			return i;
	}

	return 0;
}

//=======================================================================================================================

int GetAreaIdxFromString( const char *pString )
{
	unsigned int idx;
	bool bSearchByName = false;

	try
	{
		idx = std::stoi( pString );
	}
	catch( ... )
	{
		bSearchByName = true;
	}

	if( !bSearchByName )
	{
		// Off by 1, because indices are listed as +1 in print
		if( idx > 0 && idx <= (unsigned int)TheDangerAreas.Count() )
		{
			return idx-1;
		}
	}

	// Don't search for unnamed areas
	if( !stricmp( pString, "unnamed" ) )
		return -1;

	FOR_EACH_VEC( TheDangerAreas, i )
	{
		const DangerArea *area = TheDangerAreas[ i ];

		if( area->szAreaName[0] && !stricmp( area->szAreaName, pString ) )
		{
			return i;
		}
	}

	return -1;
}

//=======================================================================================================================

// Return true if cmd should be superceded
bool HandleDangerAreaCmd( edict_t *pEntity )
{
	const char *cmd = engine->Cmd_Argv( 1 );
	const int argc = engine->Cmd_Argc();

	IPlayerInfo *pInfo = playerinfomgr->GetPlayerInfo( pEntity );

	if( !pInfo || pInfo->IsFakeClient() )
	{
		return false;
	}
	if( pInfo->IsDead() || pInfo->GetTeamIndex() < 2 )
	{
		return true;
	}

	if( !stricmp( cmd, "add" ) )
	{
		float radius = 256.f;
		float chance = 1.f;
		int team = TEAM_BOTH;
		bool isTeamRoute = true;
		const char *pAreaName = NULL;
		DangerArea *pLink = NULL;

		if( argc > 2 )
		{
			for( int i = 2; i < argc; ++i )
			{
				const char *pArg = engine->Cmd_Argv( i );

				if( i < argc-1 && !stricmp( pArg, "link" ) )
				{
					int linkToIdx = GetAreaIdxFromString( engine->Cmd_Argv( i+1 ) );

					if( linkToIdx >= 0 )
					{
						pLink = TheDangerAreas[ linkToIdx ];
						++i; // Skip past linked area arg
						continue;
					}
				}

				try
				{
					float fVal = std::stof( pArg );

					if( fVal < 20.f )
						chance = fVal;
					else
						radius = fVal;
				}
				catch( ... )
				{
					if( !stricmp( pArg, "ct" ) )
						team = TEAM_CT;
					else if( !stricmp( pArg, "t" ) )
						team = TEAM_T;
					else if( !stricmp( pArg, "noteam" ) )
						isTeamRoute = false;
					else
						pAreaName = pArg;
				}
			}
		}

		if( pLink && pLink->iNumLinkedAreas >= DangerArea::MAX_LINKED_AREAS )
		{
			engine->ClientPrintf( pEntity, "Failed to link danger area - maximum number of linked areas are set\n" );
			return true;
		}

		Vector vecOrigin( pInfo->GetAbsOrigin() );

		CNavArea *area = GetNearestNavArea( vecOrigin );

		if( !area )
		{
			engine->ClientPrintf( pEntity, "Nav area for danger area not found\n" );
			return true;
		}

		DangerArea *da = new DangerArea();
		da->vecPosition = vecOrigin;
		da->pNearestArea = area;
		da->fMaxDistance = radius;
		da->fChance = chance;
		da->iTeam = team;
		da->iNumLinkedAreas = 0;
		da->bUsableAsTeamRoute = isTeamRoute;

		if( pAreaName )
		{
			strcpy_s( da->szAreaName, sizeof(da->szAreaName), pAreaName );
			da->szAreaName[DangerArea::MAX_NAME_LENGTH-1] = '\000';
		}
		else
		{
			da->szAreaName[0] = '\000';
		}

		if( pLink )
		{
			pLink->pLinkedAreas[ pLink->iNumLinkedAreas ] = da;
			++pLink->iNumLinkedAreas;

			g_SMAPI->ClientConPrintf( pEntity, "Danger area \"%s\" with a radius of %.01f units and a chance of %.01f linked to \"%s\"\n",
				(pAreaName? pAreaName : "unnamed"), radius, chance, (pLink->szAreaName[0] ? pLink->szAreaName : "unnamed") );
		}
		else
		{
			TheDangerAreas.AddToTail( da );

			g_SMAPI->ClientConPrintf( pEntity, "Danger area \"%s\" added with a radius of %.01f units and a chance of %.01f\n", (pAreaName? pAreaName : "unnamed"), radius, chance );
		}

		return true;
	}
	if( !stricmp( cmd, "print" ) )
	{
		if( !TheDangerAreas.Count() )
		{
			engine->ClientPrintf( pEntity, "No danger areas added\n" );
			return true;
		}

		engine->ClientPrintf( pEntity, "\n======== Danger areas ========\n" );
		FOR_EACH_VEC( TheDangerAreas, i )
		{
			const DangerArea *area = TheDangerAreas[ i ];

			g_SMAPI->ClientConPrintf( pEntity, "%d. %s\n", i+1, (area->szAreaName[0] ? area->szAreaName : "unnamed") );
			g_SMAPI->ClientConPrintf( pEntity, " L pos: %.01f, %.01f, %.01f\n", area->vecPosition.x, area->vecPosition.y, area->vecPosition.z );
			if( area->pNearestArea == NULL ) engine->ClientPrintf( pEntity, " L area: NULL!\n" );
			if( area->iTeam == TEAM_BOTH ) g_SMAPI->ClientConPrintf( pEntity, " L team: both\n" );
			else g_SMAPI->ClientConPrintf( pEntity, " L team: %s\n", area->iTeam == TEAM_T? "T":"CT" );
			g_SMAPI->ClientConPrintf( pEntity, " L range: %.01f\n", area->fMaxDistance );
			g_SMAPI->ClientConPrintf( pEntity, " L chance: %.01f\n", area->fChance );
			g_SMAPI->ClientConPrintf( pEntity, " L %d linked areas\n", area->iNumLinkedAreas );
			g_SMAPI->ClientConPrintf( pEntity, " L %susable as a team route\n", area->bUsableAsTeamRoute? "" : "not " );
		}
		engine->ClientPrintf( pEntity, "\n==============================\n" );

		return true;
	}
	if( !stricmp( cmd, "remove" ) )
	{
		if( engine->Cmd_Argc() < 3 )
		{
			engine->ClientPrintf( pEntity, "Danger area index or name required\n" );
			return true;
		}

		char *pRemove = engine->Cmd_Argv( 2 );

		if( !stricmp( pRemove, "all" ) )
		{
			TheDangerAreas.PurgeAndDeleteElements();
			g_SMAPI->ClientConPrintf( pEntity, "All danger areas have been removed\n" );

			return true;
		}

		int idx = GetAreaIdxFromString( pRemove );

		if( idx >= 0 )
		{
			const DangerArea *da = TheDangerAreas[ idx ];

			g_SMAPI->ClientConPrintf( pEntity, "Danger area \"%s\" has been removed\n", da->szAreaName[0]? da->szAreaName : "unnamed" );

			delete TheDangerAreas[ idx ];
			TheDangerAreas.Remove( idx );
		}
		else
		{
			g_SMAPI->ClientConPrintf( pEntity, "Failed to remove danger area \"%s\"\n", pRemove );
		}

		return true;
	}

	return false;
}

//=======================================================================================================================

enum DangerFileVersion
{
	DANGER_AREAS_V1 = 1,
	DANGER_AREAS_V2, // This is just for backwards-compatibility right now...
	DANGER_AREAS_MAX_VER = DANGER_AREAS_V2
};

const char g_filedir[] = "addons/dangerarea/";
const unsigned int g_danger_area_file_hdr = 0x4B1D;
const unsigned int g_danger_area_current_file_ver = DANGER_AREAS_V1;

void WriteDangerAreaToFile( const DangerArea *da, FileHandle_t file )
{
	filesystem->Write( &da->vecPosition, sizeof(Vector), file );

	size_t namelen = strnlen_s( da->szAreaName, DangerArea::MAX_NAME_LENGTH );
	filesystem->Write( &namelen, sizeof(size_t), file );

	if( namelen > 0 )
		filesystem->Write( da->szAreaName, namelen, file );

	filesystem->Write( &da->fMaxDistance, sizeof(float), file );
	filesystem->Write( &da->fChance, sizeof(float), file );
	filesystem->Write( &da->iTeam, sizeof(int), file );
	filesystem->Write( &da->iNumLinkedAreas, sizeof(int), file );
	filesystem->Write( &da->bUsableAsTeamRoute, sizeof(bool), file );

	// Write linked areas recursively
	if( da->iNumLinkedAreas )
	{
		for(int i = 0; i < da->iNumLinkedAreas; ++i )
		{
			WriteDangerAreaToFile( da->pLinkedAreas[ i ], file );
		}
	}
}

//=======================================================================================================================

DangerArea *ReadDangerAreaFromFile( FileHandle_t file )
{
	DangerArea *pArea = new DangerArea();

	filesystem->Read( &pArea->vecPosition, sizeof(Vector), file );

	size_t namelen;

	filesystem->Read( &namelen, sizeof(size_t), file );

	if( namelen > 0 )
	{
		filesystem->Read( pArea->szAreaName, namelen, file );
		pArea->szAreaName[namelen] = '\000';
	}
	else
	{
		pArea->szAreaName[0] = '\000';
	}

	filesystem->Read( &pArea->fMaxDistance, sizeof(float), file );
	filesystem->Read( &pArea->fChance, sizeof(float), file );
	filesystem->Read( &pArea->iTeam, sizeof(int), file );
	filesystem->Read( &pArea->iNumLinkedAreas, sizeof(int), file );
	filesystem->Read( &pArea->bUsableAsTeamRoute, sizeof(bool), file );

	pArea->pNearestArea = GetNearestNavArea( pArea->vecPosition );

	return pArea;
}

//=======================================================================================================================

bool SaveDangerAreasToFile()
{
	filesystem->CreateDirHierarchy( g_filedir );

	char filepath[ MAX_PATH ];
	_snprintf_s( filepath, sizeof(filepath), sizeof(filepath), "%s%s.da", g_filedir, gpGlobals->mapname );

	FileHandle_t file = filesystem->Open( filepath, "wb" );

	if( !file )
	{
		return false;
	}

	filesystem->Write( &g_danger_area_file_hdr, sizeof(unsigned int), file );
	filesystem->Write( &g_danger_area_current_file_ver, sizeof(unsigned int), file );

	int count = TheDangerAreas.Count();
	filesystem->Write( &count, sizeof(int), file );

	FOR_EACH_VEC( TheDangerAreas, i )
	{
		const DangerArea *da = TheDangerAreas[ i ];

		WriteDangerAreaToFile( da, file );
	}

	filesystem->Flush( file );
	filesystem->Close( file );

	return true;
}

//=======================================================================================================================

bool LoadDangerAreasFromFile()
{
	// Delete old areas first
	TheDangerAreas.PurgeAndDeleteElements();

	char filepath[ MAX_PATH ];
	_snprintf_s( filepath, sizeof(filepath), sizeof(filepath), "%s%s.da", g_filedir, gpGlobals->mapname );

	if( !filesystem->FileExists( filepath ) )
		return false;

	FileHandle_t file = filesystem->Open( filepath, "rb" );

	if( !file )
		return false;

	unsigned int hdr;
	filesystem->Read( &hdr, sizeof(unsigned int), file );

	if( hdr != g_danger_area_file_hdr )
	{
		filesystem->Close( file );
		return false;
	}

	unsigned int version;
	filesystem->Read( &version, sizeof(unsigned int), file );
	
	if( version < DANGER_AREAS_V1 || version > DANGER_AREAS_MAX_VER )
	{
		filesystem->Close( file );
		return false;
	}

	int count;
	filesystem->Read( &count, sizeof(int), file );

	DangerArea *pCurrentParent = NULL;
	int iNumChildrenLeft = 0;

	for(int i = 0; i < count; ++i )
	{
		DangerArea *pArea = ReadDangerAreaFromFile( file );

		if( pArea->iNumLinkedAreas )
		{
			if( pCurrentParent )
			{
				delete pArea;
				filesystem->Close( file );
				Warning("WARNING: Failed to load danger areas from file!\n");
				Msg("WARNING: Failed to load danger areas from file!\n");
				AssertMsg( false, "Failed to load danger areas from file" );
				return false;
			}

			count += pArea->iNumLinkedAreas;

			pCurrentParent = pArea;
			iNumChildrenLeft = pArea->iNumLinkedAreas;
			TheDangerAreas.AddToTail( pArea );
		}
		else if( iNumChildrenLeft > 0 )
		{
			if( !pCurrentParent )
			{
				delete pArea;
				filesystem->Close( file );
				Warning("WARNING: Failed to load danger areas from file!\n");
				Msg("WARNING: Failed to load danger areas from file!\n");
				AssertMsg( false, "Failed to load danger areas from file" );
				return false;
			}

			int index = pCurrentParent->iNumLinkedAreas - iNumChildrenLeft;

			pCurrentParent->pLinkedAreas[index] = pArea;

			--iNumChildrenLeft;

			if( iNumChildrenLeft <= 0 )
				pCurrentParent = NULL;
		}
		else
		{
			TheDangerAreas.AddToTail( pArea );
		}
	}

	filesystem->Close( file );

	return true;
}

//=======================================================================================================================

CON_COMMAND(save_danger_areas, "Save the existing danger areas to file.")
{
	if( SaveDangerAreasToFile() )
		g_SMAPI->ConPrint( "Danger areas have been saved to file.\n" );
	else
		g_SMAPI->ConPrint( "Failed to save danger areas to file.\n" );
}

//=======================================================================================================================