#ifndef _UTIL_H_
#define _UTIL_H_

#include "vector.h"
#include "weaponinfo.h"
#include "irecipientfilter.h"
#include "tier1\utlvector.h"

#define SLOT_PRIMARY	(0)

#define MAXPLAYERS		(64)

#define TEAM_BOTH		(1)
#define TEAM_T			(2)
#define TEAM_CT			(3)
#define NUM_TEAMS		(2)

enum NavErrorType
{
	NAV_OK,
	NAV_CANT_ACCESS_FILE,
	NAV_INVALID_FILE,
	NAV_BAD_FILE_VERSION,
	NAV_FILE_OUT_OF_DATE,
	NAV_CORRUPT_DATA,
};

class IHandleEntity;
class CBaseEntity;
class CCSBot;
typedef CCSBot CCSPlayer;
class CCSBotManager;
class CCSGameRules;
struct edict_t;
class CNavArea;

extern CCSBotManager *TheBots;
extern CCSGameRules **CSGameRules;

int GetEntityIndex( void *pEntity );
CBaseEntity *GetIndexEntity( int index );

bool IsAlive( void *pEntity );
const Vector &GetAbsOrigin( void *pEntity );
int GetTeamNumber( void *pEntity );
bool IsBot( void *pEntity );
const QAngle &GetEyeAngles( edict_t *pEdict );
CBaseEntity *GetPlayerWeaponInSlot( CCSPlayer *player, int slot );
bool PlayerHasWeaponInSlot( CCSPlayer *player, int slot );
CBaseEntity *GetActiveWeapon( CCSPlayer *player );
void *GetPlayerLocalData( CCSPlayer *player );
const QAngle &GetPunchAngle( CCSPlayer *player );
bool IsPlayerDucking( CCSPlayer *player );

int GetOppositeTeamNumber( int teamID );
int GetNumRoundsPlayed( void );
void GetTeamWins( int &CTWins, int &TWins );
int GetTeamConsecutiveLosses( int team );
bool MapHasBombTarget( void );

CCSPlayer *GetListenServerHost( void );
void PrintToListenServerHostConsole( const char *fmt, ... );
void PrintCenterMessage( CCSPlayer *player, const char *szMsg );

CCSPlayer *GetBotEnemy( CCSBot *bot );
float GetBotSkill( CCSBot *bot );
CNavArea *GetBotLastKnownArea( CCSBot *bot );
bool BotsOnTheServer( void );

CSWeaponID GetWeaponID( CBaseEntity *weapon );
CCSWeaponInfo *GetWeaponInfo( CSWeaponID weaponID );
int GetWeaponPrice( CSWeaponID weaponID );
void InitWeaponPrices( void );

// Simple recipient filter for sending user messages
class CRecipientFilter : public IRecipientFilter
{
public:
	bool AddRecipient( CCSPlayer *player )
	{
		int index = GetEntityIndex( player );

		if( m_Recipients.Find( index ) == m_Recipients.InvalidIndex() )
		{
			m_Recipients.AddToTail( index );
			return true;
		}

		return false;
	}

	virtual bool	IsReliable( void ) const					{ return true; }
	virtual bool	IsInitMessage( void ) const					{ return false; }

	virtual int		GetRecipientCount( void ) const				{ return m_Recipients.Count(); }
	virtual int		GetRecipientIndex( int slot ) const
	{
		if( slot < 0 || slot >= GetRecipientCount() )
			return -1;

		return m_Recipients[ slot ];
	}

private:
	CUtlVector< int > m_Recipients;
};

#endif // _UTIL_H_