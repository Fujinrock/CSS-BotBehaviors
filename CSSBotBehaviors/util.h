#ifndef _UTIL_H_
#define _UTIL_H_

#include "vector.h"

#define SLOT_PRIMARY	(0)

#define MAXPLAYERS		(64)

#define TEAM_BOTH		(1)
#define TEAM_T			(2)
#define TEAM_CT			(3)
#define NUM_TEAMS		(2)

enum CSWeaponID
{
	WEAPON_NONE = 0,

	WEAPON_P228,
	WEAPON_GLOCK,
	WEAPON_SCOUT,
	WEAPON_HEGRENADE,
	WEAPON_XM1014,
	WEAPON_C4,
	WEAPON_MAC10,
	WEAPON_AUG,
	WEAPON_SMOKEGRENADE,
	WEAPON_ELITE,
	WEAPON_FIVESEVEN,
	WEAPON_UMP45,
	WEAPON_SG550,

	WEAPON_GALIL,
	WEAPON_FAMAS,
	WEAPON_USP,
	WEAPON_AWP,
	WEAPON_MP5NAVY,
	WEAPON_M249,
	WEAPON_M3,
	WEAPON_M4A1,
	WEAPON_TMP,
	WEAPON_G3SG1,
	WEAPON_FLASHBANG,
	WEAPON_DEAGLE,
	WEAPON_SG552,
	WEAPON_AK47,
	WEAPON_KNIFE,
	WEAPON_P90,

	WEAPON_SHIELDGUN,

	WEAPON_KEVLAR,
	WEAPON_ASSAULTSUIT,
	WEAPON_NVG,

	WEAPON_MAX,
};

enum NavErrorType
{
	NAV_OK,
	NAV_CANT_ACCESS_FILE,
	NAV_INVALID_FILE,
	NAV_BAD_FILE_VERSION,
	NAV_FILE_OUT_OF_DATE,
	NAV_CORRUPT_DATA,
};

// The call class for member function pointers
class FnEmptyClass { };

class IHandleEntity;
class CBaseEntity;
class CCSBot;
typedef CCSBot CCSPlayer;
class CCSBotManager;
class CCSGameRules;
struct edict_t;

extern CCSBotManager *TheBots;
extern CCSGameRules **CSGameRules;

bool IsAlive( void *pEntity );
const Vector &GetAbsOrigin( void *pEntity );
int GetTeamNumber( void *pEntity );
const QAngle &GetEyeAngles( edict_t *pEdict );
bool PlayerHasWeaponInSlot( CBaseEntity *player, int slot );
CBaseEntity *GetActiveWeapon( CCSPlayer *player );
void *GetPlayerLocalData( CCSPlayer *player );
const QAngle &GetPunchAngle( CCSPlayer *player );
bool IsPlayerDucking( CCSPlayer *player );

int GetOppositeTeamNumber( int teamID );
int GetNumRoundsPlayed( void );

void PrintToLocal( const char *fmt, ... );

CCSPlayer *GetBotEnemy( CCSBot *bot );
float GetBotSkill( CCSBot *bot );

CSWeaponID GetWeaponID( CBaseEntity *weapon );

#endif // _UTIL_H_