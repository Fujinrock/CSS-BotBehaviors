#ifndef _BOTBEHAVIORS_PLUGIN_H_
#define _BOTBEHAVIORS_PLUGIN_H_

#include "ISmmPlugin.h"

#define PLUGIN_VERSION	"1.6.3"

PLUGIN_GLOBALVARS();

class BotBehaviorsPlugin : public ISmmPlugin
{
	virtual bool Load( PluginId id, ISmmAPI *ismm, char *error, size_t maxlen, bool late );
	virtual bool Unload( char *error, size_t maxlen );

	virtual const char *GetAuthor( void )			{ return "Fujinrock"; }
	virtual const char *GetName( void )				{ return "Bot Behaviors"; }
	virtual const char *GetDescription( void )		{ return "Modified behavior for CS:S bots"; }
	virtual const char *GetURL( void )				{ return "https://github.com/Fujinrock/CSS-BotBehaviors"; }
	virtual const char *GetLicense( void )			{ return ""; }
	virtual const char *GetVersion( void )			{ return PLUGIN_VERSION; }
	virtual const char *GetDate( void )				{ return __DATE__; }
	virtual const char *GetLogTag( void )			{ return "BotBehaviors"; }
};

#endif // _BOTBEHAVIORS_PLUGIN_H_