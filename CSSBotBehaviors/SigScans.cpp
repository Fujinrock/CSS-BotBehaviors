#include "SigScans.h"

CSigScan CCSBot_ComputePath_Sig;
CSigScan CCSBot_NoticeLooseBomb_Sig;
CSigScan CCSBot_Hide_Sig;
CSigScan CCSBot_Hunt_Sig;
CSigScan CCSBot_Blind_Sig;
CSigScan CCSBot_Upkeep_Sig;
CSigScan CCSBot_UpdateLookAngles_Sig;
CSigScan CCSBot_GetPartPosition_Sig;
CSigScan CBotDoorEnumerator_EnumElement_Sig;
CSigScan HideState_OnUpdate_Sig;
CSigScan FlashbangProjectileDetonate_Sig;
CSigScan CNavArea_ComputeSpotEncounters_Sig;
CSigScan CNavArea_ComputeApproachAreas_Sig;
CSigScan CNavArea_Destructor_Sig;
CSigScan CNavArea_ComputeEarliestOccupyTimes_Sig;
CSigScan CNavMesh_GetNearestNavArea_Sig;
CSigScan CNavMesh_GetNavArea_Sig;
CSigScan CNavMesh_IncreaseDangerNearby_Sig;
CSigScan CNavMesh_DestroyHidingSpots_Sig;
CSigScan CNavMesh_StripNavigationAreas_Sig;
CSigScan HidingSpot_PostLoad_Sig;

void SigScan( void )
{
	CCSBot_ComputePath_Sig.Scan((unsigned char*)
		"\x83\xEC\x24\x53\x56\x57\x6A\x04",
		"xxxxxxxx",
		8,
		CSigScan::SERVER);

	CNavMesh_GetNearestNavArea_Sig.Scan((unsigned char*)
		"\x81\xEC\xD4\x00\x00\x00\x56\x8B\xF1\x83\x7E\x04\x00",
		"xxxxxxxxxxxxx",
		13,
		CSigScan::SERVER);

	CNavMesh_GetNavArea_Sig.Scan((unsigned char*)
		"\x83\xEC\x20\x57\x8B\x79\x04\x85\xFF\x75",
		"xxxxxxxxxx",
		10,
		CSigScan::SERVER);

	CNavMesh_IncreaseDangerNearby_Sig.Scan((unsigned char*)
		"\x83\xEC\x1C\x56\x8B\x74\x24\x2C",
		"xxxxxxxx",
		8,
		CSigScan::SERVER);

	CCSBot_NoticeLooseBomb_Sig.Scan((unsigned char*)
		"\xA1\x2A\x2A\x2A\x2A\x83\xB8\x48\x18\x00\x00\x01\x75",
		"x????xxxxxxxx",
		13,
		CSigScan::SERVER);

	CNavMesh_DestroyHidingSpots_Sig.Scan((unsigned char*)
		"\x56\x8B\x35\x2A\x2A\x2A\x2A\x83\xFE\xFF\x74\x2A\xA1\x2A\x2A\x2A\x2A\x8D\x34\x76\x03\xF6\x03\xF6\x8B\x0C\x06\x83\xC1\x54",
		"xxx????xxxx?x????xxxxxxxxxxxxx",
		30,
		CSigScan::SERVER);

	CCSBot_Hide_Sig.Scan((unsigned char*)
		"\x53\x56\x57\x8B\x7C\x24\x10\x33\xDB\x53",
		"xxxxxxxxxx",
		10,
		CSigScan::SERVER);

	CCSBot_Blind_Sig.Scan((unsigned char*)
		"\xD9\x44\x24\x0C\x83\xEC\x34\x56\x57\x83\xEC\x18",
		"xxxxxxxxxxxx",
		12,
		CSigScan::SERVER);
	/*
	HidingSpot_PostLoad_Sig.Scan((unsigned char*)
		"\x83\xEC\x18\x56\x8B\xF1\xD9\x46\x0C",
		"xxxxxxxxx",
		9,
		CSigScan::SERVER);
	*/
	HideState_OnUpdate_Sig.Scan((unsigned char*)
		"\x81\xEC\xBC\x00\x00\x00\x53\x55\x56\x8B",
		"xxxxxxxxxx",
		10,
		CSigScan::SERVER);

	CNavArea_ComputeSpotEncounters_Sig.Scan((unsigned char*)
		"\x83\xEC\x10\x56\x8B\xF1\x8D\x4E\x78",
		"xxxxxxxxx",
		9,
		CSigScan::SERVER);

	CCSBot_Upkeep_Sig.Scan((unsigned char*)
		"\x83\xEC\x28\x53\x56\x6A\x04\x33\xDB\x53\x68\x5C\x70",
		"xxxxxxxxxxxxx",
		13,
		CSigScan::SERVER);

	CCSBot_UpdateLookAngles_Sig.Scan((unsigned char*)
		"\x83\xEC\x20\x56\x6A\x04\x6A\x00\x68\x5C\x70",
		"xxxxxxxxxxx",
		11,
		CSigScan::SERVER);

	CCSBot_GetPartPosition_Sig.Scan((unsigned char*)
		"\x53\x56\x57\x6A\x04\x6A\x00\x68\x2A\x2A\x2A\x2A\x6A\x00\x8B\xF9",
		"xxxxxxxx????xxxx",
		16,
		CSigScan::SERVER);
	
	FlashbangProjectileDetonate_Sig.Scan((unsigned char*)
		"\x53\x55\x56\x8B\xF1\x8B\x86\xA8\x00\x00\x00\xC1\xE8\x0B\xA8\x01\x57\x74\x2A\xE8\x2A\x2A\x2A\x2A\x6A\x40",
		"xxxxxxxxxxxxxxxxxx?x????xx",
		26,
		CSigScan::SERVER);

	CNavArea_ComputeApproachAreas_Sig.Scan((unsigned char*)
		"\x81\xEC\x78\x04\x00\x00\x57\x8B\xF9\xC6\x87",
		"xxxxxxxxxxx",
		11,
		CSigScan::SERVER);

	CNavMesh_StripNavigationAreas_Sig.Scan((unsigned char*)
		"\x56\x8B\x35\x2A\x2A\x2A\x2A\x83\xFE\xFF\x74\x2A\xA1\x2A\x2A\x2A\x2A\x8D\x34\x76\x03\xF6\x03\xF6\x8B\x0C\x06\xE8\x2A\x2A\x2A\x2A",
		"xxx????xxxx?x????xxxxxxxxxxx????",
		32,
		CSigScan::SERVER);

	CNavArea_Destructor_Sig.Scan((unsigned char*)
		"\x51\x53\x55\x56\x57\x8B\xF9\x8D\x4F\x78",
		"xxxxxxxxxx",
		10,
		CSigScan::SERVER);

	CNavArea_ComputeEarliestOccupyTimes_Sig.Scan((unsigned char*)
		"\x51\xB8\x00\x00\xF0\x42",
		"xxxxxx",
		6,
		CSigScan::SERVER);

	CCSBot_Hunt_Sig.Scan((unsigned char*)
		"\x8D\x81\x94\x15\x00\x00\x50\xE8\x64\xF9\xFF\xFF",
		"xxxxxxxxxxxx",
		12,
		CSigScan::SERVER);

	CBotDoorEnumerator_EnumElement_Sig.Scan((unsigned char*)
		"\x56\x57\x8B\xF9\x8B\x4C\x24\x0C\x8B\x01\xFF\x50\x08\x8B\x00",
		"xxxxxxxxxxxxxxx",
		15,
		CSigScan::SERVER);
}