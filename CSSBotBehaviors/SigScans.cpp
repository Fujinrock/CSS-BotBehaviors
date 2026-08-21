#include "SigScans.h"
#include "BotPath.h"
#include "BotHiding.h"
#include "util.h"
#include "sourcehook.h"
#include "sh_memory.h"

extern bool *g_bNavMeshIsBeingDestroyed;

CSigScan CCSBot_ComputePath_Sig;
CSigScan CCSBot_NoticeLooseBomb_Sig;
CSigScan CCSBot_Hide_Sig;
CSigScan CCSBot_TryToHide_Sig;
CSigScan CCSBot_Hunt_Sig;
CSigScan CCSBot_Blind_Sig;
CSigScan CCSBot_Upkeep_Sig;
CSigScan CCSBot_UpdateLookAngles_Sig;
CSigScan CCSBot_GetPartPosition_Sig;
CSigScan CCSBotManager_GetRandomZone_Sig;
CSigScan CCSBotManager_GetRandomAreaInZone_Sig;
CSigScan CBotDoorEnumerator_EnumElement_Sig;
CSigScan HideState_OnUpdate_Sig;
CSigScan HuntState_OnUpdate_Sig;
CSigScan BuyState_OnEnter_Sig;
CSigScan BuyState_OnUpdate_Sig;
CSigScan FlashbangProjectileDetonate_Sig;
CSigScan GetWeaponInfo_Sig;
CSigScan CNavArea_ComputeSpotEncounters_Sig;
CSigScan CNavArea_ComputeApproachAreas_Sig;
CSigScan CNavArea_Destructor_Sig;
CSigScan CNavArea_ComputeEarliestOccupyTimes_Sig;
CSigScan CNavMesh_GetNearestNavArea_Sig;
CSigScan CNavMesh_GetNavArea_Sig;
CSigScan CNavMesh_IncreaseDangerNearby_Sig;
CSigScan CNavMesh_DestroyHidingSpots_Sig;
CSigScan CNavMesh_StripNavigationAreas_Sig;
CSigScan NavAreaTravelDistance_ShortestPathCost_Sig;
//CSigScan HidingSpot_PostLoad_Sig;

//=======================================================================================================================

bool GetGlobals( void )
{
	// ===== TheNavAreaList ============================================================
	{
		CSigScan TheNavAreaList_Sig;

		if ( TheNavAreaList_Sig.Scan(
			"\x83\xEC\x08\x53\x8B\x5C\x24\x10\x55\x8B\x6C\x24\x18",
			"xxxxxxxxxxxxx",
			CSigScan::SERVER) != CSigScan::SCAN_OK )
			return false;

		const int offset = 177;
		unsigned char *theAddress = (unsigned char*)TheNavAreaList_Sig.Address() + offset;
		TheNavAreaList = *reinterpret_cast<NavAreaList **>( theAddress );
	}

	// ===== TheNavMesh ================================================================
	{
		CSigScan TheNavMesh_Sig;

		if ( TheNavMesh_Sig.Scan(
			"\x83\xEC\x3C\x56\x8B\x74\x24\x44\x83\xBE\x18\x17\x00\x00\x00",
			"xxxxxxxxxxxxxxx",
			CSigScan::SERVER) != CSigScan::SCAN_OK )
			return false;

		const int offset = 1402;
		unsigned char *theAddress = (unsigned char*)TheNavMesh_Sig.Address() + offset;
		TheNavMesh = **reinterpret_cast<CNavMesh ***>( theAddress );
	}
	
	// ===== TheHidingSpotList =========================================================
	{
		// Address is m_Head of the list minus sizeof(CUtlMemory<ListElem_t>) which is 12
		const int offset1 = 51;
		const int offset2 = 12;
		unsigned char *theAddress = (unsigned char*)CNavMesh_DestroyHidingSpots_Sig.Address() + offset1;
		TheHidingSpotList = (HidingSpotList*)(((char*)(*reinterpret_cast<void**>( theAddress ))) - offset2);
	}

	// ===== TheBots ===================================================================
	{
		const int offset = 1;
		unsigned char *theAddress = (unsigned char*)CCSBot_NoticeLooseBomb_Sig.Address() + offset;
		TheBots = **reinterpret_cast<CCSBotManager ***>( theAddress );
	}

	// ===== CSGameRules ===============================================================
	{
		CSigScan CreateGameRulesObject_Sig;

		if ( CreateGameRulesObject_Sig.Scan(
			"\x8B\x0D\x2A\x2A\x2A\x2A\x85\xC9\x74\x2A\x8B\x01\x6A\x01\xFF\x50\x2C\x53",
			"xx????xxx?xxxxxxxx",
			CSigScan::SERVER) != CSigScan::SCAN_OK )
			return false;

		const int offset = 2;
		unsigned char *theAddress = (unsigned char*)CreateGameRulesObject_Sig.Address() + offset;
		CSGameRules = *reinterpret_cast<CCSGameRules ***>( theAddress );
	}

	// ===== CNavArea m_isReset ========================================================
	{
		CSigScan isReset_Sig;

		if ( isReset_Sig.Scan(
			"\x53\x55\x56\x8B\x35\x2A\x2A\x2A\x2A\x33\xDB",
			"xxxxx????xx",
			CSigScan::SERVER) != CSigScan::SCAN_OK )
			return false;

		const int offset = 25;
		unsigned char *theAddress = (unsigned char*)isReset_Sig.Address() + offset;
		g_bNavMeshIsBeingDestroyed = *reinterpret_cast<bool**>( theAddress );
	}

	return true;
}

//=======================================================================================================================

bool ApplyPatches( void )
{
	// Patch hunt area computation away since we'll be doing it ourselves
	{
		// Patch first jz
		const int offset1 = 825;
		const int offset2 = 828;
		const int offset3 = 838;
		unsigned char *theAddress = (unsigned char*)HuntState_OnUpdate_Sig.Address() + offset1;

		if ( SourceHook::SetMemAccess( theAddress, sizeof(unsigned char) * 2, SH_MEM_READ|SH_MEM_WRITE|SH_MEM_EXEC ) )
		{
			for ( int i = 0; i < 2; ++i )
			{
				*theAddress++ = 0x90;
			}
		}
		else
			return false;

		// Patch 'allowSpeedChange' to false too
		theAddress = (unsigned char*)HuntState_OnUpdate_Sig.Address() + offset2;
		if ( SourceHook::SetMemAccess( theAddress, sizeof(unsigned char), SH_MEM_READ|SH_MEM_WRITE|SH_MEM_EXEC ) )
		{
			*theAddress = 0x00;
		}
		else
			return false;

		// Patch second jz
		theAddress = (unsigned char*)HuntState_OnUpdate_Sig.Address() + offset3;
		const unsigned char patchBytes[6] = {'\xE9', '\x6C', '\x01', '\x00', '\x00', '\x90'};

		if ( SourceHook::SetMemAccess( theAddress, sizeof(patchBytes), SH_MEM_READ|SH_MEM_WRITE|SH_MEM_EXEC ) )
		{
			for ( int i = 0; i < 6; ++i )
			{
				*theAddress++ = patchBytes[ i ];
			}
		}
		else
			return false;
	}

	return true;
}

//=======================================================================================================================

BOOL SigScan( void )
{
	static int result = -1;

	if ( result != -1 )
		return !!result;

	if ( CCSBot_ComputePath_Sig.Scan(
		"\x83\xEC\x24\x53\x56\x57\x6A\x04",
		"xxxxxxxx",
		CSigScan::SERVER) != CSigScan::SCAN_OK )
		goto scanFailed;

	if ( CNavMesh_GetNearestNavArea_Sig.Scan(
		"\x81\xEC\xD4\x00\x00\x00\x56\x8B\xF1\x83\x7E\x04\x00",
		"xxxxxxxxxxxxx",
		CSigScan::SERVER) != CSigScan::SCAN_OK )
		goto scanFailed;

	if ( CNavMesh_GetNavArea_Sig.Scan(
		"\x83\xEC\x20\x57\x8B\x79\x04\x85\xFF\x75",
		"xxxxxxxxxx",
		CSigScan::SERVER) != CSigScan::SCAN_OK )
		goto scanFailed;

	if ( CNavMesh_IncreaseDangerNearby_Sig.Scan(
		"\x83\xEC\x1C\x56\x8B\x74\x24\x2C",
		"xxxxxxxx",
		CSigScan::SERVER) != CSigScan::SCAN_OK )
		goto scanFailed;

	if ( CCSBot_NoticeLooseBomb_Sig.Scan(
		"\xA1\x2A\x2A\x2A\x2A\x83\xB8\x48\x18\x00\x00\x01\x75",
		"x????xxxxxxxx",
		CSigScan::SERVER) != CSigScan::SCAN_OK )
		goto scanFailed;

	if ( CNavMesh_DestroyHidingSpots_Sig.Scan(
		"\x56\x8B\x35\x2A\x2A\x2A\x2A\x83\xFE\xFF\x74\x2A\xA1\x2A\x2A\x2A\x2A\x8D\x34\x76\x03\xF6\x03\xF6\x8B\x0C\x06\x83\xC1\x54",
		"xxx????xxxx?x????xxxxxxxxxxxxx",
		CSigScan::SERVER) != CSigScan::SCAN_OK )
		goto scanFailed;

	if ( CCSBot_Hide_Sig.Scan(
		"\x53\x56\x57\x8B\x7C\x24\x10\x33\xDB\x53",
		"xxxxxxxxxx",
		CSigScan::SERVER) != CSigScan::SCAN_OK )
		goto scanFailed;

	if ( CCSBot_TryToHide_Sig.Scan(
		"\x83\xEC\x18\x56\x57\x8B\x7C\x24\x24\x85\xFF",
		"xxxxxxxxxxx",
		CSigScan::SERVER) != CSigScan::SCAN_OK )
		goto scanFailed;

	if ( CCSBot_Blind_Sig.Scan(
		"\xD9\x44\x24\x0C\x83\xEC\x34\x56\x57\x83\xEC\x18",
		"xxxxxxxxxxxx",
		CSigScan::SERVER) != CSigScan::SCAN_OK )
		goto scanFailed;
	/*
	if ( HidingSpot_PostLoad_Sig.Scan(
		"\x83\xEC\x18\x56\x8B\xF1\xD9\x46\x0C",
		"xxxxxxxxx",
		CSigScan::SERVER) != CSigScan::SCAN_OK )
		goto scanFailed;
	*/
	if ( HideState_OnUpdate_Sig.Scan(
		"\x81\xEC\xBC\x00\x00\x00\x53\x55\x56\x8B",
		"xxxxxxxxxx",
		CSigScan::SERVER) != CSigScan::SCAN_OK )
		goto scanFailed;

	if ( HuntState_OnUpdate_Sig.Scan(
		"\x83\xEC\x10\xA1\x2A\x2A\x2A\x2A\xD9\x40\x0C",
		"xxxx????xxx",
		CSigScan::SERVER) != CSigScan::SCAN_OK )
		goto scanFailed;

	if ( CNavArea_ComputeSpotEncounters_Sig.Scan(
		"\x83\xEC\x10\x56\x8B\xF1\x8D\x4E\x78",
		"xxxxxxxxx",
		CSigScan::SERVER) != CSigScan::SCAN_OK )
		goto scanFailed;

	if ( CCSBot_Upkeep_Sig.Scan(
		"\x83\xEC\x28\x53\x56\x6A\x04\x33\xDB\x53\x68\x5C\x70",
		"xxxxxxxxxxxxx",
		CSigScan::SERVER) != CSigScan::SCAN_OK )
		goto scanFailed;

	if ( CCSBot_UpdateLookAngles_Sig.Scan(
		"\x83\xEC\x20\x56\x6A\x04\x6A\x00\x68\x5C\x70",
		"xxxxxxxxxxx",
		CSigScan::SERVER) != CSigScan::SCAN_OK )
		goto scanFailed;

	if ( CCSBot_GetPartPosition_Sig.Scan(
		"\x53\x56\x57\x6A\x04\x6A\x00\x68\x2A\x2A\x2A\x2A\x6A\x00\x8B\xF9",
		"xxxxxxxx????xxxx",
		CSigScan::SERVER) != CSigScan::SCAN_OK )
		goto scanFailed;
	
	if ( CCSBotManager_GetRandomZone_Sig.Scan(
		"\x83\xEC\x18\x53\x55\x8B\xE9\x8B\x85\x2C\x1A\x00\x00",
		"xxxxxxxxxxxxx",
		CSigScan::SERVER) != CSigScan::SCAN_OK )
		goto scanFailed;

	if ( CCSBotManager_GetRandomAreaInZone_Sig.Scan(
		"\x8B\x44\x24\x04\x53\x8B\x58\x44",
		"xxxxxxxx",
		CSigScan::SERVER) != CSigScan::SCAN_OK )
		goto scanFailed;

	if ( FlashbangProjectileDetonate_Sig.Scan(
		"\x53\x55\x56\x8B\xF1\x8B\x86\xA8\x00\x00\x00\xC1\xE8\x0B\xA8\x01\x57\x74\x2A\xE8\x2A\x2A\x2A\x2A\x6A\x40",
		"xxxxxxxxxxxxxxxxxx?x????xx",
		CSigScan::SERVER) != CSigScan::SCAN_OK )
		goto scanFailed;

	if ( CNavArea_ComputeApproachAreas_Sig.Scan(
		"\x81\xEC\x78\x04\x00\x00\x57\x8B\xF9\xC6\x87",
		"xxxxxxxxxxx",
		CSigScan::SERVER) != CSigScan::SCAN_OK )
		goto scanFailed;

	if ( CNavMesh_StripNavigationAreas_Sig.Scan(
		"\x56\x8B\x35\x2A\x2A\x2A\x2A\x83\xFE\xFF\x74\x2A\xA1\x2A\x2A\x2A\x2A\x8D\x34\x76\x03\xF6\x03\xF6\x8B\x0C\x06\xE8",
		"xxx????xxxx?x????xxxxxxxxxxx",
		CSigScan::SERVER) != CSigScan::SCAN_OK )
		goto scanFailed;

	if ( CNavArea_Destructor_Sig.Scan(
		"\x51\x53\x55\x56\x57\x8B\xF9\x8D\x4F\x78",
		"xxxxxxxxxx",
		CSigScan::SERVER) != CSigScan::SCAN_OK )
		goto scanFailed;

	if ( CNavArea_ComputeEarliestOccupyTimes_Sig.Scan(
		"\x51\xB8\x00\x00\xF0\x42",
		"xxxxxx",
		CSigScan::SERVER) != CSigScan::SCAN_OK )
		goto scanFailed;

	if ( CCSBot_Hunt_Sig.Scan(
		"\x8D\x81\x94\x15\x00\x00\x50\xE8\x64\xF9\xFF\xFF",
		"xxxxxxxxxxxx",
		CSigScan::SERVER) != CSigScan::SCAN_OK )
		goto scanFailed;

	if ( CBotDoorEnumerator_EnumElement_Sig.Scan(
		"\x56\x57\x8B\xF9\x8B\x4C\x24\x0C\x8B\x01\xFF\x50\x08\x8B\x00",
		"xxxxxxxxxxxxxxx",
		CSigScan::SERVER) != CSigScan::SCAN_OK )
		goto scanFailed;

	if ( NavAreaTravelDistance_ShortestPathCost_Sig.Scan(
		"\x83\xEC\x40\x55\x56\x8B\x74\x24\x4C\x33\xED\x3B\xF5\x75\x2A\xD9\x05\x2A\x2A\x2A\x2A\x5E\x5D\x83\xC4\x40\xC3\x57\x8B\x7C\x24\x54\x3B\xFD\x75\x2A\xD9\x05\x2A\x2A\x2A\x2A\x5F\x5E\x5D\x83\xC4\x40\xC3\x3B\xF7\x75\x2A\xD9\x05\x2A\x2A\x2A\x2A\x5F\x5E\x5D\x83\xC4\x40\xC3\x80\x7F\x30\x00\x53\x0F\x85\x2A\x2A\x2A\x2A\x89\xAE\xF0\x01\x00\x00\xC7\x86\xF4\x01\x00\x00\x07\x00\x00\x00\x8B\x47\x1C\x8B\x4F\x20\x8B\x57\x24\x89\x44\x24\x38\x89\x4C\x24\x3C\x89\x54\x24\x40\xE8\x2A\x2A\x2A\x2A\xD9\x46\x1C\xD8\x64\x24\x38\xD9\x5C\x24\x44\x8B\x44\x24\x44\xD9\x46\x20\x89\x44\x24\x2C\xD8\x64\x24\x3C\xD9\x5C\x24\x48\x8B\x4C\x24\x48\xD9\x46\x24\x89\x4C\x24\x30\xD8\x64\x24\x40\x51\xD9\x5C\x24\x50\x8B\x54\x24\x50\x89\x54\x24\x38\xD9\x44\x24\x38\xD8\x4C\x24\x38\xD9\x44\x24\x34\xD8\x4C\x24\x34\xDE\xC1\xD9\x44\x24\x30\xD8\x4C\x24\x30\xDE\xC1\xD9\x1C\x24\xFF\x15\x2A\x2A\x2A\x2A\x83\xC4\x04",
		"xxxxxxxxxxxxxx?xx????xxxxxxxxxxxxxx?xx????xxxxxxxxxx?xx????xxxxxxxxxxxxxx????xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx????xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx????xxx",
		CSigScan::SERVER) != CSigScan::SCAN_OK )
		goto scanFailed;

	/*
	if ( BuyState_OnEnter_Sig.Scan(
		"\x53\x33\xDB\x56\x8B\xF1\x89\x5E\x10",
		"xxxxxxxxx",
		CSigScan::SERVER) != CSigScan::SCAN_OK )
		goto scanFailed;

	if ( BuyState_OnUpdate_Sig.Scan(
		"\x81\xEC\x3C\x01\x00\x00\xA1\x2A\x2A\x2A\x2A\x80\x78",
		"xxxxxxx????xx",
		CSigScan::SERVER) != CSigScan::SCAN_OK )
		goto scanFailed;

	if ( GetWeaponInfo_Sig.Scan(
		"\x8B\x44\x24\x04\x85\xC0\x75\x2A\xC3\x83\xF8\x1F",
		"xxxxxxx?xxxx",
		CSigScan::SERVER) != CSigScan::SCAN_OK )
		goto scanFailed;
	*/

	if ( !GetGlobals() )
		goto scanFailed;

	if ( !ApplyPatches() )
		goto scanFailed;

	result = 1;
	return 1;

scanFailed:
	result = 0;
	return 0;
}

//=======================================================================================================================