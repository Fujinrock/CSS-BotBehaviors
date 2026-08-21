#include "SigScans.h"
#include "CDetour\detours.h"
#include "BotPath.h"
#include "BotHiding.h"
#include "BotAiming.h"
#include "BotLoadout.h"
#include "util.h"
#include "random.h"
#include "edict.h"
#include "convar.h"

extern CGlobalVars *gpGlobals;

extern ConVar g_cvHideOnHuntChance;

CDetour *DComputePath;
CDetour *DNoticeLooseBomb;
CDetour *DHide;
CDetour *DBlind;
//CDetour *DHidingSpotPostLoad;
CDetour *DEnumElement;
CDetour *DOnUpdateHideState;
CDetour *DOnUpdateHuntState;
CDetour *DUpkeep;
CDetour *DFlashbangProjectileDetonate;
CDetour *DNavAreaDestructor;
CDetour *DEnterBuyState;
CDetour *DUpdateBuyState;

/*----------------------------------------------------------------------------------------------------------------------------------------------------------*/

DETOUR_DECL_MEMBER2(ComputePath, bool, const Vector &, goal, RouteType, route )
{
	OnBotComputePath( (CCSBot*)this, goal, route );

	return DETOUR_MEMBER_CALL(ComputePath)( goal, route );
}

/*----------------------------------------------------------------------------------------------------------------------------------------------------------*/

DETOUR_DECL_MEMBER0(NoticeLooseBomb, bool)
{
	bool noticedBomb = DETOUR_MEMBER_CALL(NoticeLooseBomb)();

	if( noticedBomb )
	{
		noticedBomb = ::NoticeLooseBomb( (CCSBot *)this );
	}

	return noticedBomb;
}

/*----------------------------------------------------------------------------------------------------------------------------------------------------------*/

DETOUR_DECL_MEMBER3(Hide, void, const Vector &, hidingSpot, float, duration, bool, holdPosition)
{
	if( ShouldBlockHide( (CCSBot*)this ) )
	{
		BotHunt( (CCSBot*)this ); // Make the bot hunt so it doesn't start camping
		return;
	}

	DETOUR_MEMBER_CALL(Hide)(hidingSpot, duration, holdPosition);
}

/*----------------------------------------------------------------------------------------------------------------------------------------------------------*/
/*
// Should not be needed if GetNavArea is used when creating hiding spots
DETOUR_DECL_MEMBER0(HidingSpotPostLoad, NavErrorType)
{
	const int posOffs = 4;
	const int areaOffs = 24;

	Vector pos = *(Vector*)((char*)this + posOffs);
	pos.z += 36.0;

	CNavArea *pArea = GetNearestNavArea( pos );
	//CNavArea *pArea = GetNavArea( pos );

	AssertMsg( pArea, "Hiding spot has no nav area!" );

	// The original function just uses GetNavArea, which would return NULL for any position
	// that's not within a nav area, so use GetNearestNavArea instead.
	*(CNavArea**)((char*)this + areaOffs) = pArea;

	return NAV_OK;
}
*/
/*----------------------------------------------------------------------------------------------------------------------------------------------------------*/

#define ITERATION_CONTINUE 0
DETOUR_DECL_MEMBER1(EnumElement, int, IHandleEntity *, pHandleEntity )
{
	bool result = CBotDoorEnumerator_EnumElement( pHandleEntity );

	if( !result )
		return ITERATION_CONTINUE; // Go to the next element

	return DETOUR_MEMBER_CALL(EnumElement)(pHandleEntity);
}

/*----------------------------------------------------------------------------------------------------------------------------------------------------------*/

DETOUR_DECL_MEMBER1(OnUpdateHideState, void, CCSBot *, me )
{
	static float s_nextLookSpotCheckTimestamp[ MAXPLAYERS ];

	const int isAtSpotOffs = 25;

	bool isAtSpotPreUpdate = *(bool*)((char*)this + isAtSpotOffs);

	DETOUR_MEMBER_CALL(OnUpdateHideState)(me);

	bool isAtSpotPostUpdate = *(bool*)((char*)this + isAtSpotOffs);

	// Did the bot reach the hiding spot on this update?
	if( !isAtSpotPreUpdate && isAtSpotPostUpdate )
	{
		const int entidx = GetEntityIndex( me );

		OnBotReachHidingSpot( me, this );
		s_nextLookSpotCheckTimestamp[ entidx % MAXPLAYERS ] = gpGlobals->curtime + 0.5f;

		/*if( OnBotReachHidingSpot( me, this ) )
			s_nextLookSpotCheckTimestamp[ entidx % MAXPLAYERS ] = gpGlobals->curtime + 2.f;
		else
			s_nextLookSpotCheckTimestamp[ entidx % MAXPLAYERS ] = 9999999.f;*/
	}
	else if( isAtSpotPreUpdate && BotIsHiding( me ) )
	{
		// Check if we should look at our look-spot again
		const int entidx = GetEntityIndex( me );

		if( gpGlobals->curtime < s_nextLookSpotCheckTimestamp[ entidx % MAXPLAYERS ] )
			return;

		// TODO this is still unreliable... debug needed
		CheckBotHidingLookSpot( me, this );
		s_nextLookSpotCheckTimestamp[ entidx % MAXPLAYERS ] = gpGlobals->curtime + 2.f;

		/*if( CheckBotHidingLookSpot( me, this ) )
			s_nextLookSpotCheckTimestamp[ entidx % MAXPLAYERS ] = gpGlobals->curtime + 2.f;
		else
			s_nextLookSpotCheckTimestamp[ entidx % MAXPLAYERS ] = 9999999.f;*/
	}
}

/*----------------------------------------------------------------------------------------------------------------------------------------------------------*/

DETOUR_DECL_MEMBER1(OnUpdateHuntState, void, CCSBot *, me )
{
	DETOUR_MEMBER_CALL(OnUpdateHuntState)(me);

	int pathResult;

	// Get the result from UpdatePathMovement, which was the last thing called in original function
	__asm
	{
		mov pathResult, eax
	}

	CNavArea *lastKnownArea, **huntAreaPtr;

	const int huntAreaOffs = 4;

	lastKnownArea = GetBotLastKnownArea( me );
	huntAreaPtr = (CNavArea**)((char*)this + huntAreaOffs);

	// Is it time to choose a new hunt area?
	if ( lastKnownArea != *huntAreaPtr && pathResult == 0 )
		return;

	const float hideChance = g_cvHideOnHuntChance.GetFloat();
	const bool bHide = RandomFloat( 0.f, 100.f ) < hideChance && !ShouldBlockHide( me );

	const Zone *zone = NULL;

	if ( !bHide && ((zone = GetTargetZoneForBot( me )) != NULL) )
	{
		// Go to a site
		*huntAreaPtr = GetRandomAreaInZone( zone );
	}
	else
	{
		// Try to hide somewhere instead
		const float duration = RandomFloat( 5.f, 20.f );
		const float range = 1500.f;
		const bool holdPosition = RandomFloat( 0.f, 100.f ) < 65.f;

		if ( BotTryToHide( me, NULL, duration, range, holdPosition, false ) )
		{
			// We will hide instead of hunting
			*huntAreaPtr = NULL;
			return;
		}
		else
		{
			// Can't even hide, just go to a random area
			const int areas = TheNavAreaList->Count();
			const int which = RandomInt( 0, areas-1 );
			*huntAreaPtr = TheNavAreaList->Element( which );
		}
	}

	if ( *huntAreaPtr )
	{
		const int centerOffs = 28;
		const Vector &pos = *(const Vector*)((char*)*huntAreaPtr + centerOffs);

		BotComputePath( me, pos, SAFEST_ROUTE );
	}
}

/*----------------------------------------------------------------------------------------------------------------------------------------------------------*/

DETOUR_DECL_MEMBER0(Upkeep, void)
{
	// Call original function
	DETOUR_MEMBER_CALL(Upkeep)();

	DoAiming( (CCSBot*)this );
}

/*----------------------------------------------------------------------------------------------------------------------------------------------------------*/

static int s_iFlashbangTeam = 0;

DETOUR_DECL_MEMBER0(FlashbangProjectileDetonate, void)
{
	s_iFlashbangTeam = GetTeamNumber( this );

	DETOUR_MEMBER_CALL(FlashbangProjectileDetonate)();
}

/*----------------------------------------------------------------------------------------------------------------------------------------------------------*/

DETOUR_DECL_MEMBER3(Blind, void, float, holdTime, float, fadeTime, float, startingAlpha)
{
	// Ignore team flashes for bots
	if( GetTeamNumber( this ) == s_iFlashbangTeam )
	{
		return;
	}
	
	// Fade time is really long, and half of it + hold time is set for bot blind time, so decrease it
	DETOUR_MEMBER_CALL(Blind)(holdTime, fadeTime * 0.3f, startingAlpha);
}

/*----------------------------------------------------------------------------------------------------------------------------------------------------------*/

DETOUR_DECL_MEMBER0(NavAreaDestructor, void)
{
	OnNavAreaRemoved( (CNavArea*)this );

	DETOUR_MEMBER_CALL(NavAreaDestructor)();
}

/*----------------------------------------------------------------------------------------------------------------------------------------------------------*/

DETOUR_DECL_MEMBER1(OnEnterBuyState, void, CCSBot *, me )
{
	if( !::OnEnterBuyState( me ) )
	{
		DETOUR_MEMBER_CALL(OnEnterBuyState)(me);
	}
}

/*----------------------------------------------------------------------------------------------------------------------------------------------------------*/

DETOUR_DECL_MEMBER1(OnUpdateBuyState, void, CCSBot *, me )
{
	if( !::OnUpdateBuyState( me ) )
	{
		DETOUR_MEMBER_CALL(OnUpdateBuyState)(me);
	}
}

/*----------------------------------------------------------------------------------------------------------------------------------------------------------*/

void CreateDetours( void )
{
	static bool created = false;

	if( created )
		return;

	ENABLE_MEMBER_DETOUR(DComputePath, ComputePath, CCSBot_ComputePath_Sig.Address());
	ENABLE_MEMBER_DETOUR(DNoticeLooseBomb, NoticeLooseBomb, CCSBot_NoticeLooseBomb_Sig.Address());
	ENABLE_MEMBER_DETOUR(DHide, Hide, CCSBot_Hide_Sig.Address());
	ENABLE_MEMBER_DETOUR(DBlind, Blind, CCSBot_Blind_Sig.Address());
	//ENABLE_MEMBER_DETOUR(DHidingSpotPostLoad, HidingSpotPostLoad, HidingSpot_PostLoad_Sig.Address());
	ENABLE_MEMBER_DETOUR(DOnUpdateHideState, OnUpdateHideState, HideState_OnUpdate_Sig.Address());
	ENABLE_MEMBER_DETOUR(DOnUpdateHuntState, OnUpdateHuntState, HuntState_OnUpdate_Sig.Address());
	ENABLE_MEMBER_DETOUR(DUpkeep, Upkeep, CCSBot_Upkeep_Sig.Address());
	ENABLE_MEMBER_DETOUR(DFlashbangProjectileDetonate, FlashbangProjectileDetonate, FlashbangProjectileDetonate_Sig.Address());
	ENABLE_MEMBER_DETOUR(DNavAreaDestructor, NavAreaDestructor, CNavArea_Destructor_Sig.Address());
	ENABLE_MEMBER_DETOUR(DEnumElement, EnumElement, CBotDoorEnumerator_EnumElement_Sig.Address());
	//ENABLE_MEMBER_DETOUR(DEnterBuyState, OnEnterBuyState, BuyState_OnEnter_Sig.Address());
	//ENABLE_MEMBER_DETOUR(DUpdateBuyState, OnUpdateBuyState, BuyState_OnUpdate_Sig.Address());

	created = true;
}

/*----------------------------------------------------------------------------------------------------------------------------------------------------------*/

void RemoveDetours( void )
{
	if ( DComputePath )
		DComputePath->Destroy();
	if ( DNoticeLooseBomb )
		DNoticeLooseBomb->Destroy();
	if ( DHide )
		DHide->Destroy();
	if ( DBlind )
		DBlind->Destroy();
	//if ( DHidingSpotPostLoad )
		//DHidingSpotPostLoad->Destroy();
	if ( DEnumElement )
		DEnumElement->Destroy();
	if ( DOnUpdateHideState )
		DOnUpdateHideState->Destroy();
	if ( DOnUpdateHuntState )
		DOnUpdateHuntState->Destroy();
	if ( DUpkeep )
		DUpkeep->Destroy();
	if ( DFlashbangProjectileDetonate )
		DFlashbangProjectileDetonate->Destroy();
	if ( DNavAreaDestructor )
		DNavAreaDestructor->Destroy();
	//if ( DEnterBuyState )
		//DEnterBuyState->Destroy();
	//if ( DUpdateBuyState )
		//DUpdateBuyState->Destroy();
}

/*----------------------------------------------------------------------------------------------------------------------------------------------------------*/