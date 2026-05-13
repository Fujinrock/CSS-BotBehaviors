#ifndef _BOTBEHAVIORS_EVENTS_H_
#define _BOTBEHAVIORS_EVENTS_H_

#include "igameevents.h"

struct edict_t;

// ===== Hook callbacks =======================================================
void OnServerActivate( edict_t *pEdictList, int edictCount, int clientMax );
void OnClientCommand( edict_t *pEntity );
bool OnLevelInit( char const *pMapName, char const *pMapEntities, char const *pOldLevel, char const *pLandmarkName, bool loadGame, bool background );

// ===== Event listeners ======================================================
class CEvent_OnRoundStart : public IGameEventListener2
{
	virtual void FireGameEvent( IGameEvent *event );
};

extern CEvent_OnRoundStart g_OnRoundStart;

#endif // _BOTBEHAVIORS_EVENTS_H_