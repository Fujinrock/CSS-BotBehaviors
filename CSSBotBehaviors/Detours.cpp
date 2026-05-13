#include "SigScans.h"
#include "CDetour\detours.h"
#include "BotPath.h"
#include "BotHiding.h"
#include "BotAiming.h"
#include "util.h"

CDetour *DComputePath;
CDetour *DNoticeLooseBomb;
CDetour *DHide;
CDetour *DBlind;
//CDetour *DHidingSpotPostLoad;
CDetour *DEnumElement;
CDetour *DOnUpdateHideState;
CDetour *DUpkeep;
CDetour *DFlashbangProjectileDetonate;
CDetour *DNavAreaDestructor;

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
		return;

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
	const int isAtSpotOffs = 25;

	bool isAtSpotPreUpdate = *(bool*)((char*)this + isAtSpotOffs);

	DETOUR_MEMBER_CALL(OnUpdateHideState)(me);

	bool isAtSpotPostUpdate = *(bool*)((char*)this + isAtSpotOffs);

	// Did the bot reach the hiding spot on this update?
	if( !isAtSpotPreUpdate && isAtSpotPostUpdate )
	{
		OnBotReachHidingSpot( me, this );
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
	ENABLE_MEMBER_DETOUR(DUpkeep, Upkeep, CCSBot_Upkeep_Sig.Address());
	ENABLE_MEMBER_DETOUR(DFlashbangProjectileDetonate, FlashbangProjectileDetonate, FlashbangProjectileDetonate_Sig.Address());
	ENABLE_MEMBER_DETOUR(DNavAreaDestructor, NavAreaDestructor, CNavArea_Destructor_Sig.Address());
	ENABLE_MEMBER_DETOUR(DEnumElement, EnumElement, CBotDoorEnumerator_EnumElement_Sig.Address());

	created = true;
}

/*----------------------------------------------------------------------------------------------------------------------------------------------------------*/

void RemoveDetours( void )
{
	DComputePath->Destroy();
	DNoticeLooseBomb->Destroy();
	DHide->Destroy();
	DBlind->Destroy();
	//DHidingSpotPostLoad->Destroy();
	DEnumElement->Destroy();
	DOnUpdateHideState->Destroy();
	DUpkeep->Destroy();
	DFlashbangProjectileDetonate->Destroy();
	DNavAreaDestructor->Destroy();
}

/*----------------------------------------------------------------------------------------------------------------------------------------------------------*/