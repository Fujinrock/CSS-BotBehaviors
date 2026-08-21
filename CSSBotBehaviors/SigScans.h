#ifndef _SIGSCANS_H_
#define _SIGSCANS_H_

#include "SigScan.h"
#include "basetypes.h"

extern CSigScan CCSBot_ComputePath_Sig;
extern CSigScan CCSBot_NoticeLooseBomb_Sig;
extern CSigScan CCSBot_Hide_Sig;
extern CSigScan CCSBot_TryToHide_Sig;
extern CSigScan CCSBot_Hunt_Sig;
extern CSigScan CCSBot_Blind_Sig;
extern CSigScan CCSBot_Upkeep_Sig;
extern CSigScan CCSBot_UpdateLookAngles_Sig;
extern CSigScan CCSBot_GetPartPosition_Sig;
extern CSigScan CCSBotManager_GetRandomZone_Sig;
extern CSigScan CCSBotManager_GetRandomAreaInZone_Sig;
extern CSigScan CBotDoorEnumerator_EnumElement_Sig;
extern CSigScan HideState_OnUpdate_Sig;
extern CSigScan HuntState_OnUpdate_Sig;
extern CSigScan BuyState_OnEnter_Sig;
extern CSigScan BuyState_OnUpdate_Sig;
extern CSigScan FlashbangProjectileDetonate_Sig;
extern CSigScan GetWeaponInfo_Sig;
extern CSigScan CNavArea_ComputeSpotEncounters_Sig;
extern CSigScan CNavArea_ComputeApproachAreas_Sig;
extern CSigScan CNavArea_Destructor_Sig;
extern CSigScan CNavArea_ComputeEarliestOccupyTimes_Sig;
extern CSigScan CNavMesh_GetNearestNavArea_Sig;
extern CSigScan CNavMesh_GetNavArea_Sig;
extern CSigScan CNavMesh_IncreaseDangerNearby_Sig;
extern CSigScan CNavMesh_DestroyHidingSpots_Sig;
extern CSigScan CNavMesh_StripNavigationAreas_Sig;
extern CSigScan NavAreaTravelDistance_ShortestPathCost_Sig;
//extern CSigScan HidingSpot_PostLoad_Sig;

BOOL SigScan( void );

#endif // _SIGSCANS_H_