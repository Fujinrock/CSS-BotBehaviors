#ifndef _BOT_PATH_H_
#define _BOT_PATH_H_

#include "util.h"
#include "tier1/utllinkedlist.h"
#include "tier1/utlvector.h"
#include "vector.h"

class CNavMesh;
class CNavArea;

extern float g_fPathRandomizedTimestamp[MAXPLAYERS];
extern int g_iSafePathIndex[MAXPLAYERS];
extern int g_iTeamSafePathIndex[NUM_TEAMS];
extern int g_iLastSafePathIndex[MAXPLAYERS];
extern bool g_bHaveDoneRoundMoneyCheck;
extern float g_fRoundStartTimestamp;

typedef CUtlLinkedList< CNavArea*, int > NavAreaList;
extern NavAreaList *TheNavAreaList;
extern CNavMesh *TheNavMesh;

struct DangerArea
{
	Vector vecPosition;
	CNavArea *pNearestArea;
	float fMaxDistance;
	float fChance;
	enum { MAX_NAME_LENGTH = 24 };
	char szAreaName[MAX_NAME_LENGTH];
	int iTeam;
	bool bUsableAsTeamRoute;

	enum { MAX_LINKED_AREAS = 6 };

	DangerArea *pLinkedAreas[ MAX_LINKED_AREAS ];
	int iNumLinkedAreas;

	~DangerArea()
	{
		for(int i = 0; i < iNumLinkedAreas; ++i )
			delete pLinkedAreas[i];
	}
};

extern CUtlVector< DangerArea * > TheDangerAreas;

enum RouteType
{
	FASTEST_ROUTE,
	SAFEST_ROUTE,
};

void DoRoundMoneyCheck( void );

bool NavMeshIsGenerating( void );

void OnBotComputePath( CCSBot *me, const Vector & goal, RouteType route );

bool NoticeLooseBomb( CCSBot *me );
void BotHunt( CCSBot *pBot );
bool CBotDoorEnumerator_EnumElement( IHandleEntity *pHandleEntity );

#define FOR_TEAM	true
int GetRandomDangerAreaIdxForTeam( int teamID, bool forTeam = false );
int GetRandomSubRouteIdxForArea( const DangerArea *area );
int GetAreaIdxFromString( const char *pString );

CNavArea *GetNearestNavArea( const Vector &pos, bool anyZ = false, float maxDist = 512.f, bool checkLOS = false );
CNavArea *GetNavArea( const Vector &pos, float beneathLimit = 120.f );
void IncreaseDangerNearby( int teamID, float amount, CNavArea *startArea, const Vector &pos, float maxRadius );

bool HandleDangerAreaCmd( edict_t *pEntity );

bool LoadDangerAreasFromFile();

#endif //_BOT_PATH_H_