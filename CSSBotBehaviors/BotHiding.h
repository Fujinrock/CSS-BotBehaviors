#ifndef _BOT_HIDING_H_
#define _BOT_HIDING_H_

#include "vector.h"
#include "utlvector.h"
#include "utllinkedlist.h"

class HidingSpot;
class CCSBot;
class CNavArea;
struct edict_t;
enum NavErrorType;

struct HidingSpotLookInfo
{
	Vector	target;		///< World position to look at
	uint	spotID;		///< ID of the hiding spot to look from
};

extern char g_szNavIdentifier[];
extern bool g_bDoNavSaveComputations;

typedef CUtlLinkedList< HidingSpot *, int > HidingSpotList;
extern HidingSpotList *TheHidingSpotList;

extern CUtlVector< HidingSpotLookInfo > TheLookSpots;
extern CUtlVector< HidingSpot * > DisconnectedHidingSpots;

HidingSpot *CreateHidingSpot( void );
void DestroyHidingSpots( void );
void OnNavAreaRemoved( CNavArea *area );

bool PreNavMeshSave( void );
bool PostNavMeshSave( void );
NavErrorType PreNavMeshLoad( void );
NavErrorType PostNavMeshLoad( void );

bool ShouldBlockHide( CCSBot *me );
bool OnBotReachHidingSpot( CCSBot *me, void *pHideState );
bool CheckBotHidingLookSpot( CCSBot *me, void *pHideState );
bool BotIsHiding( CCSBot *bot );
bool BotTryToHide( CCSBot *bot, CNavArea *searchFromArea = NULL, float duration = -1.0f, float hideRange = 750.0f, bool holdPosition = false, bool useNearest = false );

bool HandleHidingSpotCmd( edict_t *pEntity );
void DrawActiveHidingSpotLookSpot( void );

#endif // _BOT_HIDING_H_