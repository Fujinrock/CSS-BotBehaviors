#ifndef _BOT_LOADOUT_H_
#define _BOT_LOADOUT_H_

class CCSBot;

// Return false from these if original buy function should be called
bool OnEnterBuyState( CCSBot *me );
bool OnUpdateBuyState( CCSBot *me );

#endif // _BOT_LOADOUT_H_