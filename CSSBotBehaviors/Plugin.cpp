#include "Plugin.h"
#include "Events.h"
#include "SigScans.h"
#include "BotPath.h"
#include "BotHiding.h"
#include "util.h"
#include "SigScans.h"
#include "eiface.h"
#include "edict.h"
#include "playerinfomanager.h"
#include "igameevents.h"
#include "filesystem.h"
#include "convar.h"
#include "ivdebugoverlay.h"

#define snprintf _snprintf

BotBehaviorsPlugin g_Plugin;

PLUGIN_EXPOSE(BotBehaviorsPlugin, g_Plugin);

IVEngineServer *engine = NULL;
CGlobalVars *gpGlobals = NULL;
IServerGameDLL *gamedll = NULL;
IServerGameClients *gameclients = NULL;
IServerGameEnts *gameents = NULL;
IPlayerInfoManager *playerinfomgr = NULL;
IGameEventManager2 *gameeventmgr = NULL;
IFileSystem *filesystem = NULL;
IVDebugOverlay *debugoverlay = NULL;

SH_DECL_HOOK3_void( IServerGameDLL, ServerActivate, SH_NOATTRIB, 0, edict_t *, int, int );
SH_DECL_HOOK1_void( IServerGameClients, ClientCommand, SH_NOATTRIB, 0, edict_t * );
SH_DECL_HOOK6( IServerGameDLL, LevelInit, SH_NOATTRIB, 0, bool, char const*, char const*, char const*, char const*, bool, bool );
SH_DECL_HOOK1_void( IServerGameDLL, GameFrame, SH_NOATTRIB, 0, bool );
SH_DECL_MANUALHOOK0( Hook_PreNavMeshLoad, 4, 0, 0, NavErrorType );
SH_DECL_MANUALHOOK0( Hook_PostNavMeshLoad, 4, 0, 0, NavErrorType );
SH_DECL_MANUALHOOK0( Hook_PreNavMeshSave, 5, 0, 0, bool );
SH_DECL_MANUALHOOK0( Hook_PostNavMeshSave, 5, 0, 0, bool );

const ConVar *g_pCvNavEdit = NULL;
const ConVar *g_pCvNavQuicksave = NULL;
const ConVar *g_pCvBotFlipout = NULL;
const ConVar *g_pCvFreezetime = NULL;
const ConVar *g_pCvBotLoadout = NULL;
const ConVar *g_pCvStartMoney = NULL;

//=======================================================================================================================

class ConCommandBaseAccessor : public IConCommandBaseAccessor
{
public:
	bool RegisterConCommandBase( ConCommandBase *pVar )
	{
		return META_REGCVAR( pVar );
	}
}
s_CCBaseAccessor;

//=======================================================================================================================

bool BotBehaviorsPlugin::Load( PluginId id, ISmmAPI *ismm, char *error, size_t maxlen, bool late )
{
	PLUGIN_SAVEVARS();

	ConCommandBaseMgr::OneTimeInit( &s_CCBaseAccessor );

	CSigScan::Initialize( ismm->serverFactory( false ), ismm->engineFactory( false ) );
	if ( !SigScan() )
	{
		if ( error && maxlen )
			snprintf( error, maxlen, "Failed to find required signatures" );
		return false;
	}
	
	g_pCvNavEdit = dynamic_cast<const ConVar *>( ConCommandBase::FindCommand( "nav_edit" ) );
	g_pCvNavQuicksave = dynamic_cast<const ConVar *>( ConCommandBase::FindCommand( "nav_quicksave" ) );
	g_pCvBotFlipout = dynamic_cast<const ConVar *>( ConCommandBase::FindCommand( "bot_flipout" ) );
	g_pCvFreezetime = dynamic_cast<const ConVar *>( ConCommandBase::FindCommand( "mp_freezetime" ) );
	g_pCvBotLoadout = dynamic_cast<const ConVar *>( ConCommandBase::FindCommand( "bot_loadout" ) );
	g_pCvStartMoney = dynamic_cast<const ConVar *>( ConCommandBase::FindCommand( "mp_startmoney" ) );

	gpGlobals = ismm->pGlobals();
	GET_V_IFACE_CURRENT( engineFactory, engine, IVEngineServer, INTERFACEVERSION_VENGINESERVER );
	GET_V_IFACE_CURRENT( serverFactory, gamedll, IServerGameDLL, INTERFACEVERSION_SERVERGAMEDLL );
	GET_V_IFACE_CURRENT( serverFactory, gameclients, IServerGameClients, INTERFACEVERSION_SERVERGAMECLIENTS );
	GET_V_IFACE_CURRENT( serverFactory, gameents, IServerGameEnts, INTERFACEVERSION_SERVERGAMEENTS );
	GET_V_IFACE_CURRENT( serverFactory, playerinfomgr, IPlayerInfoManager, INTERFACEVERSION_PLAYERINFOMANAGER );
	GET_V_IFACE_CURRENT( engineFactory, gameeventmgr, IGameEventManager2, INTERFACEVERSION_GAMEEVENTSMANAGER2 );
	GET_V_IFACE_CURRENT( fileSystemFactory, filesystem, IFileSystem, FILESYSTEM_INTERFACE_VERSION );
	GET_V_IFACE_CURRENT( engineFactory, debugoverlay, IVDebugOverlay, VDEBUG_OVERLAY_INTERFACE_VERSION );

	SH_ADD_HOOK( IServerGameDLL, ServerActivate, gamedll, SH_STATIC(OnServerActivate), true );
	SH_ADD_HOOK( IServerGameClients, ClientCommand, gameclients, SH_STATIC(OnClientCommand), false );
	SH_ADD_HOOK( IServerGameDLL, LevelInit, gamedll, SH_STATIC(OnLevelInit), true );
	SH_ADD_HOOK( IServerGameDLL, GameFrame, gamedll, SH_STATIC(OnGameFrame), true );
	SH_ADD_MANUALHOOK( Hook_PreNavMeshLoad, TheNavMesh, SH_STATIC(PreNavMeshLoad), false );
	SH_ADD_MANUALHOOK( Hook_PostNavMeshLoad, TheNavMesh, SH_STATIC(PostNavMeshLoad), true );
	SH_ADD_MANUALHOOK( Hook_PreNavMeshSave, TheNavMesh, SH_STATIC(PreNavMeshSave), false );
	SH_ADD_MANUALHOOK( Hook_PostNavMeshSave, TheNavMesh, SH_STATIC(PostNavMeshSave), true );
	
	gameeventmgr->AddListener( &g_OnRoundStart, "round_start", true );
	
	return true;
}

//=======================================================================================================================

void RemoveDetours( void );

bool BotBehaviorsPlugin::Unload( char *error, size_t maxlen )
{
	RemoveDetours();

	SH_REMOVE_HOOK( IServerGameDLL, ServerActivate, gamedll, SH_STATIC(OnServerActivate), true );
	SH_REMOVE_HOOK( IServerGameClients, ClientCommand, gameclients, SH_STATIC(OnClientCommand), false );
	SH_REMOVE_HOOK( IServerGameDLL, LevelInit, gamedll, SH_STATIC(OnLevelInit), true );
	SH_REMOVE_HOOK( IServerGameDLL, GameFrame, gamedll, SH_STATIC(OnGameFrame), true );
	SH_REMOVE_MANUALHOOK( Hook_PreNavMeshLoad, TheNavMesh, SH_STATIC(PreNavMeshLoad), false );
	SH_REMOVE_MANUALHOOK( Hook_PostNavMeshLoad, TheNavMesh, SH_STATIC(PostNavMeshLoad), true );
	SH_REMOVE_MANUALHOOK( Hook_PreNavMeshSave, TheNavMesh, SH_STATIC(PreNavMeshSave), false );
	SH_REMOVE_MANUALHOOK( Hook_PostNavMeshSave, TheNavMesh, SH_STATIC(PostNavMeshSave), true );

	gameeventmgr->RemoveListener( &g_OnRoundStart );

	return true;
}

//=======================================================================================================================