// Copyright (C) 1999-2000 Id Software, Inc.
//
#include "g_local.h"
#include "bg_saga.h"
#include "g_adminshared.h"
#include "g_roll.h"

//[SVN]
//rearraigned repository to make it easier to initially compile.
#include "../../ojpenhanced/ui/jamp/menudef.h"
//#include "../../ui/menudef.h"			// for the voice chats
//[/SVN]

//[CoOp]
extern	qboolean		in_camera;
//[/CoOp]

//rww - for getting bot commands...
int AcceptBotCommand(char *cmd, gentity_t *pl);
//end rww

#include "../namespace_begin.h"
void WP_SetSaber( int entNum, saberInfo_t *sabers, int saberNum, const char *saberName );
#include "../namespace_end.h"

void Cmd_NPC_f( gentity_t *ent );
void SP_CreateSnow( gentity_t *ent );
void SP_CreateRain( gentity_t *ent );
void SP_fx_runner( gentity_t *ent );
void fx_runner_think( gentity_t *ent );
void AddSpawnField(char *field, char *value);
void SetTeamQuick(gentity_t *ent, int team, qboolean doBegin);
void M_Svcmd_LockTeam_f(void);
extern void AddIP( char *str );
extern int G_ClientNumberFromArg( char *str);

void uwRename(gentity_t *player, const char *newname) 
{ 
   char userinfo[MAX_INFO_STRING]; 
   int clientNum = player-g_entities;
   trap_GetUserinfo(clientNum, userinfo, sizeof(userinfo)); 
   Info_SetValueForKey(userinfo, "name", newname);
   trap_SetUserinfo(clientNum, userinfo); 
   ClientUserinfoChanged(clientNum); 
   player->client->pers.netnameTime = level.time + 5000; // hmmm... 
}

void uw2Rename(gentity_t *player, const char *newname) 
{ 
   char userinfo[MAX_INFO_STRING]; 
   int clientNum = player-g_entities;
   trap_GetUserinfo(clientNum, userinfo, sizeof(userinfo)); 
   Info_SetValueForKey(userinfo, "name", newname); 
   trap_SetUserinfo(clientNum, userinfo); 
   ClientUserinfoChanged(clientNum); 
   player->client->pers.netnameTime = level.time + Q3_INFINITE;
}

void uw4Rename(gentity_t *player, const char *newname) 
{ 
   char userinfo[MAX_INFO_STRING]; 
   int clientNum = player-g_entities;
   trap_GetUserinfo(clientNum, userinfo, sizeof(userinfo)); 
   Info_SetValueForKey(userinfo, "clan", newname); 
   trap_SetUserinfo(clientNum, userinfo); 
   ClientUserinfoChanged(clientNum); 
   player->client->pers.clannameTime = level.time + Q3_INFINITE;
}

void uw3Rename(gentity_t *player, const char *newname) 
{
   char userinfo[MAX_INFO_STRING]; 
   int clientNum = player-g_entities;
   trap_GetUserinfo(clientNum, userinfo, sizeof(userinfo)); 
   Info_SetValueForKey(userinfo, "clan", newname); 
   trap_SetUserinfo(clientNum, userinfo); 
   ClientUserinfoChanged(clientNum); 
   player->client->pers.clannameTime = level.time + 3000;
}
/*
==================
SanitizeString2

Rich's revised version of SanitizeString
==================
*/
void SanitizeString2( char *in, char *out )
{
	int i = 0;
	int r = 0;

	while (in[i])
	{
		if (i >= MAX_NAME_LENGTH-1)
		{ //the ui truncates the name here..
			break;
		}

		if (in[i] == '^')
		{
			if (in[i+1] >= 48 && //'0'
				in[i+1] <= 57) //'9'
			{ //only skip it if there's a number after it for the color
				i += 2;
				continue;
			}
			else
			{ //just skip the ^
				i++;
				continue;
			}
		}

		if (in[i] < 32)
		{
			i++;
			continue;
		}

		out[r] = in[i];
		r++;
		i++;
	}
	out[r] = 0;
}
//lmo added for ease of admin commands
int G_ClientNumberFromStrippedSubstring ( const char* name )
{
	char		s2[MAX_STRING_CHARS];
	char		n2[MAX_STRING_CHARS];
	int			i, match = -1;
	gclient_t	*cl;

	// check for a name match
	SanitizeString2( (char*)name, s2 );

	for ( i=0 ; i < level.numConnectedClients ; i++ ) 
	{
		cl=&level.clients[level.sortedClients[i]];
		SanitizeString2( cl->pers.netname, n2 );
		if ( strstr( n2, s2 ) ) 
		{
			if( match != -1 )
			{ //found more than one match
				return -2;
			}
			match = level.sortedClients[i];
		}
	}

	return match;
}
int G_ClientNumberFromArg ( char* name)
{
	int client_id = 0;
	char *cp;
	
	cp = name;
	while (*cp)
	{
		if ( *cp >= '0' && *cp <= '9' ) cp++;
		else
		{
			client_id = -1; //mark as alphanumeric
			break;
		}
	}

	if ( client_id == 0 )
	{ // arg is assumed to be client number
		client_id = atoi(name);
	}

	else
	{ // arg is client name
		//client_id = G_ClientNumberFromName( name );
		/*if ( client_id == -1 )
		{
			client_id = G_ClientNumberFromStrippedName( name );
		}*/
		if ( client_id == -1 )
		{
			client_id = G_ClientNumberFromStrippedSubstring(name);
		}
	}
	return client_id;
}

//[AdminSys]
//to allow the /vote command to work for both team votes and normal votes.
void Cmd_TeamVote_f( gentity_t *ent );
//[/AdminSys]

// Required for holocron edits.
//[HolocronFiles]
extern vmCvar_t bot_wp_edit;
//[/HolocronFiles]

/*
==================
DeathmatchScoreboardMessage

==================
*/
void DeathmatchScoreboardMessage( gentity_t *ent ) {
	char		entry[1024];
	char		string[1400];
	int			stringlength;
	int			i, j;
	gclient_t	*cl;
	int			numSorted, scoreFlags, accuracy, perfect;

	// send the latest information on all clients
	string[0] = 0;
	stringlength = 0;
	scoreFlags = 0;

	numSorted = level.numConnectedClients;
	
	if (numSorted > MAX_CLIENT_SCORE_SEND)
	{
		numSorted = MAX_CLIENT_SCORE_SEND;
	}

	for (i=0 ; i < numSorted ; i++) {
		int		ping;

		cl = &level.clients[level.sortedClients[i]];

		if ( cl->pers.connected == CON_CONNECTING ) {
			ping = -1;
		//[BotTweaks] 
		//[ClientNumFix]
		} else if ( g_entities[level.sortedClients[i]].r.svFlags & SVF_BOT )
		//} else if ( g_entities[cl->ps.clientNum]r.svFlags & SVF_BOT )
		//[/ClientNumFix]
		{//make fake pings for bots.
			ping = Q_irand(50, 150);
		//[/BotTweaks]
		} else {
			ping = cl->ps.ping < 999 ? cl->ps.ping : 999;
		}

		if( cl->accuracy_shots ) {
			accuracy = cl->accuracy_hits * 100 / cl->accuracy_shots;
		}
		else {
			accuracy = 0;
		}
		perfect = ( cl->ps.persistant[PERS_RANK] == 0 && cl->ps.persistant[PERS_KILLED] == 0 ) ? 1 : 0;

		Com_sprintf (entry, sizeof(entry),
			//[ExpSys]
			" %i %i %i %i %i %i %i %i %i %i %i %i %i %i %i ", level.sortedClients[i],
			//" %i %i %i %i %i %i %i %i %i %i %i %i %i %i", level.sortedClients[i],
			//[/ExpSys]
			cl->ps.persistant[PERS_SCORE], ping, (level.time - cl->pers.enterTime)/60000,
			scoreFlags, g_entities[level.sortedClients[i]].s.powerups, accuracy, 
			cl->ps.persistant[PERS_IMPRESSIVE_COUNT],
			cl->ps.persistant[PERS_EXCELLENT_COUNT],
			cl->ps.persistant[PERS_GAUNTLET_FRAG_COUNT], 
			cl->ps.persistant[PERS_DEFEND_COUNT], 
			cl->ps.persistant[PERS_ASSIST_COUNT], 
			perfect,
			//[ExpSys]
			cl->ps.persistant[PERS_CAPTURES],
			//cl->ps.persistant[PERS_CAPTURES]);
			(int) cl->sess.skillPoints);
			//[/ExpSys]
		j = strlen(entry);
		if (stringlength + j > 1022)
			break;
		strcpy (string + stringlength, entry);
		stringlength += j;
	}

	//still want to know the total # of clients
	i = level.numConnectedClients;

	trap_SendServerCommand( ent-g_entities, va("scores %i %i %i%s", i, 
		level.teamScores[TEAM_RED], level.teamScores[TEAM_BLUE],
		string ) );
}


/*
==================
Cmd_Score_f

Request current scoreboard information
==================
*/
void Cmd_Score_f( gentity_t *ent ) {
	DeathmatchScoreboardMessage( ent );
}

/* 
================== 
ClanSayOk 

Admin commands
================== 
*/ 
qboolean   ClanSayOk( gentity_t *ent ) {

   if ( ent->r.svFlags & SVF_CLANSAY ) { 
      return qtrue; 
   } 
   else {
   return qfalse;
   }
}

/*
==================
CheatsOk
==================
*/
qboolean	CheatsOk( gentity_t *ent ) {
	if ( !g_cheats.integer ) {
		trap_SendServerCommand( ent-g_entities, va("print \"%s\n\"", G_GetStringEdString("MP_SVGAME", "NOCHEATS")));
		return qfalse;
	}
	if ( ent->health <= 0 ) {
		trap_SendServerCommand( ent-g_entities, va("print \"%s\n\"", G_GetStringEdString("MP_SVGAME", "MUSTBEALIVE")));
		return qfalse;
	}
	return qtrue;
}


/*
==================
ConcatArgs
==================
*/
char	*ConcatArgs( int start ) {
	int		i, c, tlen;
	static char	line[MAX_STRING_CHARS];
	int		len;
	char	arg[MAX_STRING_CHARS];

	len = 0;
	c = trap_Argc();
	for ( i = start ; i < c ; i++ ) {
		trap_Argv( i, arg, sizeof( arg ) );
		tlen = strlen( arg );
		if ( len + tlen >= MAX_STRING_CHARS - 1 ) {
			break;
		}
		memcpy( line + len, arg, tlen );
		len += tlen;
		if ( i != c - 1 ) {
			line[len] = ' ';
			len++;
		}
	}

	line[len] = 0;

	return line;
}

/*
==================
SanitizeString

Remove case and control characters
==================
*/
void SanitizeString( char *in, char *out ) {
	while ( *in ) {
		if ( *in == 27 ) {
			in += 2;		// skip color code
			continue;
		}
		if ( *in < 32 ) {
			in++;
			continue;
		}
		*out++ = tolower( (unsigned char) *in++ );
	}

	*out = 0;
}

/*
==================
ClientNumberFromString

Returns a player number for either a number or name string
Returns -1 if invalid
==================
*/
int ClientNumberFromString( gentity_t *to, char *s ) {
	gclient_t	*cl;
	int			idnum;
	char		s2[MAX_STRING_CHARS];
	char		n2[MAX_STRING_CHARS];

	// numeric values are just slot numbers
	if (s[0] >= '0' && s[0] <= '9') {
		idnum = atoi( s );
		if ( idnum < 0 || idnum >= level.maxclients ) {
			trap_SendServerCommand( to-g_entities, va("print \"Bad client slot: %i\n\"", idnum));
			return -1;
		}

		cl = &level.clients[idnum];
		if ( cl->pers.connected != CON_CONNECTED ) {
			trap_SendServerCommand( to-g_entities, va("print \"Client %i is not active\n\"", idnum));
			return -1;
		}
		return idnum;
	}

	// check for a name match
	SanitizeString( s, s2 );
	for ( idnum=0,cl=level.clients ; idnum < level.maxclients ; idnum++,cl++ ) {
		if ( cl->pers.connected != CON_CONNECTED ) {
			continue;
		}
		SanitizeString( cl->pers.netname, n2 );
		if ( !strcmp( n2, s2 ) ) {
			return idnum;
		}
	}

	trap_SendServerCommand( to-g_entities, va("print \"User %s is not on the server\n\"", s));
	return -1;
}

/*
==================
Cmd_Give_f

Give items to a client
==================
*/
//[VisualWeapons]
extern qboolean OJP_AllPlayersHaveClientPlugin(void);
//[/VisualWeapons]
void Cmd_Give_f (gentity_t *cmdent, int baseArg)
{
	char		name[MAX_TOKEN_CHARS];
	gentity_t	*ent;
	//gitem_t		*it; // ensiform - removed
	int			i;
	qboolean	give_all;
	//gentity_t		*it_ent; // ensiform - removed
	//trace_t		trace; // ensiform - removed
	char		arg[MAX_TOKEN_CHARS];

	if ( !CheatsOk( cmdent ) ) {
		return;
	}

	if (baseArg)
	{
		char otherindex[MAX_TOKEN_CHARS];

		trap_Argv( 1, otherindex, sizeof( otherindex ) );

		if (!otherindex[0])
		{
			Com_Printf("giveother requires that the second argument be a client index number.\n");
			return;
		}

		i = atoi(otherindex);

		if (i < 0 || i >= MAX_CLIENTS)
		{
			Com_Printf("%i is not a client index\n", i);
			return;
		}

		ent = &g_entities[i];

		if (!ent->inuse || !ent->client)
		{
			Com_Printf("%i is not an active client\n", i);
			return;
		}
	}
	else
	{
		ent = cmdent;
	}

	trap_Argv( 1+baseArg, name, sizeof( name ) );

	if (Q_stricmp(name, "all") == 0)
		give_all = qtrue;
	else
		give_all = qfalse;

	//[CoOp]
	/*if (give_all)
	{
		i = 0;
		while (i < HI_NUM_HOLDABLE)
		{
			ent->client->ps.stats[STAT_HOLDABLE_ITEMS] |= (1 << i);
			i++;
		}
		i = 0;
	}*/
	//[/CoOp]

	if (give_all || Q_stricmp( name, "health") == 0)
	{
		if (trap_Argc() == 3+baseArg) {
			trap_Argv( 2+baseArg, arg, sizeof( arg ) );
			ent->health = atoi(arg);
			if (ent->health > ent->client->ps.stats[STAT_MAX_HEALTH]) {
				ent->health = ent->client->ps.stats[STAT_MAX_HEALTH];
			}
		}
		else {
			ent->health = ent->client->ps.stats[STAT_MAX_HEALTH];
		}
		if (!give_all)
			return;
	}

	//[CoOp]
	if (give_all || Q_stricmp( name, "inventory") == 0)
	{
		i = 0;
		for ( i = 0 ; i < HI_NUM_HOLDABLE ; i++ ) {
			ent->client->ps.stats[STAT_HOLDABLE_ITEMS] |= (1 << i);
		}
	}

	if (give_all || Q_stricmp( name, "force") == 0)
	{
		if (trap_Argc() == 3+baseArg) {
			trap_Argv( 2+baseArg, arg, sizeof( arg ) );
			ent->client->ps.fd.forcePower = atoi(arg);
			if (ent->client->ps.fd.forcePower > 100) {
				ent->client->ps.fd.forcePower = 100;
			}
		}
		else {
			ent->client->ps.fd.forcePower = 100;
		}
		if (!give_all)
			return;
	}
	//[/CoOp]

	if (give_all || Q_stricmp(name, "weapons") == 0)
	{
		ent->client->ps.stats[STAT_WEAPONS] = (1 << (LAST_USEABLE_WEAPON+1))  - ( 1 << WP_NONE );
		//[VisualWeapons]
		//update the weapon stats for this player since they have changed.
		if(OJP_AllPlayersHaveClientPlugin())
		{//don't send the weapon updates if someone isn't able to process this new event type (IE anyone without
			//the OJP client plugin)
			G_AddEvent(ent, EV_WEAPINVCHANGE, ent->client->ps.stats[STAT_WEAPONS]);
		}
		//[/VisualWeapons]
		if (!give_all)
			return;
	}
	
	if ( !give_all && Q_stricmp(name, "weaponnum") == 0 )
	{
		trap_Argv( 2+baseArg, arg, sizeof( arg ) );
		ent->client->ps.stats[STAT_WEAPONS] |= (1 << atoi(arg));

		//[VisualWeapons]
		//update the weapon stats for this player since they have changed.
		if(OJP_AllPlayersHaveClientPlugin())
		{//don't send the weapon updates if someone isn't able to process this new event type (IE anyone without
			//the OJP client plugin)
			G_AddEvent(ent, EV_WEAPINVCHANGE, ent->client->ps.stats[STAT_WEAPONS]);
		}
		//[/VisualWeapons]
		return;
	}

	//[CoOp]
	if (give_all || Q_stricmp(name, "ammo") == 0)
	{
		int num = 999;
		if (trap_Argc() == 3+baseArg) {
			trap_Argv( 2+baseArg, arg, sizeof( arg ) );
			num = atoi(arg);
		}
		for ( i = AMMO_BLASTER ; i < AMMO_MAX ; i++ ) {
			if ( num > ammoData[i].max )
				num = ammoData[i].max;
			Add_Ammo( ent, i, num );
		}
		if (!give_all)
			return;
	}

	/*if (give_all || Q_stricmp(name, "ammo") == 0)
	{
		int num = 999;
		if (trap_Argc() == 3+baseArg) {
			trap_Argv( 2+baseArg, arg, sizeof( arg ) );
			num = atoi(arg);
		}
		for ( i = 0 ; i < MAX_WEAPONS ; i++ ) {
			ent->client->ps.ammo[i] = num;
		}
		if (!give_all)
			return;
	}*/
	//[/CoOp]

	if (give_all || Q_stricmp(name, "armor") == 0)
	{
		if (trap_Argc() == 3+baseArg) {
			trap_Argv( 2+baseArg, arg, sizeof( arg ) );
			ent->client->ps.stats[STAT_ARMOR] = atoi(arg);
		} else {
			ent->client->ps.stats[STAT_ARMOR] = ent->client->ps.stats[STAT_MAX_HEALTH];
		}

		if (!give_all)
			return;
	}

	/*
	// ensiform - Not used in basejka or OJP so why keep?
	if (Q_stricmp(name, "excellent") == 0) {
		ent->client->ps.persistant[PERS_EXCELLENT_COUNT]++;
		return;
	}
	if (Q_stricmp(name, "impressive") == 0) {
		ent->client->ps.persistant[PERS_IMPRESSIVE_COUNT]++;
		return;
	}
	if (Q_stricmp(name, "gauntletaward") == 0) {
		ent->client->ps.persistant[PERS_GAUNTLET_FRAG_COUNT]++;
		return;
	}
	if (Q_stricmp(name, "defend") == 0) {
		ent->client->ps.persistant[PERS_DEFEND_COUNT]++;
		return;
	}
	if (Q_stricmp(name, "assist") == 0) {
		ent->client->ps.persistant[PERS_ASSIST_COUNT]++;
		return;
	}

	// spawn a specific item right on the player
	if ( !give_all ) {
		it = BG_FindItem (name);
		if (!it) {
			return;
		}

		it_ent = G_Spawn();
		VectorCopy( ent->r.currentOrigin, it_ent->s.origin );
		it_ent->classname = it->classname;
		G_SpawnItem (it_ent, it);
		FinishSpawningItem(it_ent );
		memset( &trace, 0, sizeof( trace ) );
		Touch_Item (it_ent, ent, &trace);
		if (it_ent->inuse) {
			G_FreeEntity( it_ent );
		}
	}*/
}

/*
==================
Cmd_God_f

Sets client to godmode

argv(0) god
==================
*/
void Cmd_God_f (gentity_t *ent)
{
	char	*msg;

	if ( !CheatsOk( ent ) ) {
		return;
	}

	ent->flags ^= FL_GODMODE;
	if (!(ent->flags & FL_GODMODE) )
		msg = "godmode OFF\n";
	else
		msg = "godmode ON\n";

	trap_SendServerCommand( ent-g_entities, va("print \"%s\"", msg));
}


/*
==================
Cmd_Notarget_f

Sets client to notarget

argv(0) notarget
==================
*/
void Cmd_Notarget_f( gentity_t *ent ) {
	char	*msg;

	if ( !CheatsOk( ent ) ) {
		return;
	}

	ent->flags ^= FL_NOTARGET;
	if (!(ent->flags & FL_NOTARGET) )
		msg = "notarget OFF\n";
	else
		msg = "notarget ON\n";

	trap_SendServerCommand( ent-g_entities, va("print \"%s\"", msg));
}


/*
==================
Cmd_Noclip_f

argv(0) noclip
==================
*/
void Cmd_Noclip_f( gentity_t *ent ) {
	char	*msg;

	if (!CheatsOk(ent) && !((ent->r.svFlags & SVF_ADMIN1) || (ent->r.svFlags & SVF_ADMIN2) || (ent->r.svFlags & SVF_ADMIN3) || (ent->r.svFlags & SVF_ADMIN4) || (ent->r.svFlags & SVF_ADMIN5)))
	{
		return;
	}


	//[CoOp]
	if (in_camera)
		return;
	//[/CoOp]

	if ( ent->client->noclip ) {
		msg = "noclip OFF\n";
	} else {
		msg = "noclip ON\n";
	}
	ent->client->noclip = !ent->client->noclip;

	trap_SendServerCommand( ent-g_entities, va("print \"%s\"", msg));
}


/*
==================
Cmd_LevelShot_f

This is just to help generate the level pictures
for the menus.  It goes to the intermission immediately
and sends over a command to the client to resize the view,
hide the scoreboard, and take a special screenshot
==================
*/
void Cmd_LevelShot_f( gentity_t *ent ) {
	if ( !CheatsOk( ent ) ) {
		return;
	}

	// doesn't work in single player
	if ( g_gametype.integer != 0 ) {
		trap_SendServerCommand( ent-g_entities, 
			"print \"Must be in g_gametype 0 for levelshot\n\"" );
		return;
	}

	BeginIntermission();
	trap_SendServerCommand( ent-g_entities, "clientLevelShot" );
}


/*
==================
Cmd_TeamTask_f

From TA.
==================
*/
void Cmd_TeamTask_f( gentity_t *ent ) {
	char userinfo[MAX_INFO_STRING];
	char		arg[MAX_TOKEN_CHARS];
	int task;
	int client = ent->client - level.clients;

	if ( trap_Argc() != 2 ) {
		return;
	}
	trap_Argv( 1, arg, sizeof( arg ) );
	task = atoi( arg );

	trap_GetUserinfo(client, userinfo, sizeof(userinfo));
	Info_SetValueForKey(userinfo, "teamtask", va("%d", task));
	trap_SetUserinfo(client, userinfo);
	ClientUserinfoChanged(client);
}

//[AdminSys]
extern void AddIP( char *str );
extern vmCvar_t	g_autoKickTKSpammers;
extern vmCvar_t	g_autoBanTKSpammers;
void G_CheckTKAutoKickBan( gentity_t *ent ) 
{
	if ( !ent || !ent->client || ent->s.number >= MAX_CLIENTS )
	{
		return;
	}

	if ( g_autoKickTKSpammers.integer > 0
		|| g_autoBanTKSpammers.integer > 0 )
	{
		ent->client->sess.TKCount++;
		if ( g_autoBanTKSpammers.integer > 0
			&& ent->client->sess.TKCount >= g_autoBanTKSpammers.integer )
		{
			if ( ent->client->sess.IPstring )
			{//ban their IP
				AddIP( ent->client->sess.IPstring );
			}

			trap_SendServerCommand( -1, va("print \"%s %s\n\"", ent->client->pers.netname, G_GetStringEdString("MP_SVGAME_ADMIN", "TKBAN")) );
			//Com_sprintf ( level.voteString, sizeof(level.voteString ), "clientkick %d", ent->s.number );
			//Com_sprintf ( level.voteDisplayString, sizeof(level.voteDisplayString), "kick %s", ent->client->pers.netname );
			//trap_SendConsoleCommand( EXEC_INSERT, va( "banClient %d\n", ent->s.number ) );
			trap_SendConsoleCommand( EXEC_INSERT, va( "clientkick %d\n", ent->s.number ) );
			return;
		}
		if ( g_autoKickTKSpammers.integer > 0
			&& ent->client->sess.TKCount >= g_autoKickTKSpammers.integer )
		{
			trap_SendServerCommand( -1, va("print \"%s %s\n\"", ent->client->pers.netname, G_GetStringEdString("MP_SVGAME_ADMIN", "TKKICK")) );
			//Com_sprintf ( level.voteString, sizeof(level.voteString ), "clientkick %d", ent->s.number );
			//Com_sprintf ( level.voteDisplayString, sizeof(level.voteDisplayString), "kick \"%s\"\n", ent->client->pers.netname );
			trap_SendConsoleCommand( EXEC_INSERT, va( "clientkick %d\n", ent->s.number ) );
			return;
		}
		//okay, not gone (yet), but warn them...
		if ( g_autoBanTKSpammers.integer > 0
			&& (g_autoKickTKSpammers.integer <= 0 || g_autoBanTKSpammers.integer < g_autoKickTKSpammers.integer) )
		{//warn about ban
			trap_SendServerCommand( ent-g_entities, va("print \"%s\n\"", G_GetStringEdString("MP_SVGAME_ADMIN", "WARNINGTKBAN")) );
		}
		else if ( g_autoKickTKSpammers.integer > 0 )
		{//warn about kick
			trap_SendServerCommand( ent-g_entities, va("print \"%s\n\"", G_GetStringEdString("MP_SVGAME_ADMIN", "WARNINGTKKICK")) );
		}
	}
}
//[/AdminSys]


/*
=================
Cmd_Kill_f
=================
*/
//[AdminSys]
extern vmCvar_t	g_autoKickKillSpammers;
extern vmCvar_t	g_autoBanKillSpammers;
//[/AdminSys]
void Cmd_Kill_f( gentity_t *ent ) {
	//[BugFix41]
	//if ( ent->client->sess.sessionTeam == TEAM_SPECTATOR ) {
	if ( ent->client->sess.sessionTeam == TEAM_SPECTATOR || ent->client->tempSpectate >= level.time ) {
	//[/BugFix41]
		return;
	}
	if (ent->health <= 0)
		return;
	//[CoOp]
	if (in_camera)
		return;
	//[/CoOp]

	if ((g_gametype.integer == GT_DUEL || g_gametype.integer == GT_POWERDUEL) &&
		level.numPlayingClients > 1 && !level.warmupTime)
	{
		if (!g_allowDuelSuicide.integer)
		{
			trap_SendServerCommand( ent-g_entities, va("print \"%s\n\"", G_GetStringEdString("MP_SVGAME", "ATTEMPTDUELKILL")) );
			return;
		}
	}

//[AdminSys]
	if ( g_autoKickKillSpammers.integer > 0
		|| g_autoBanKillSpammers.integer > 0 )
	{
		ent->client->sess.killCount++;
		if ( g_autoBanKillSpammers.integer > 0
			&& ent->client->sess.killCount >= g_autoBanKillSpammers.integer )
		{
			if ( ent->client->sess.IPstring )
			{//ban their IP
				AddIP( ent->client->sess.IPstring );
			}

			trap_SendServerCommand( -1, va("print \"%s %s\n\"", ent->client->pers.netname, G_GetStringEdString("MP_SVGAME_ADMIN", "SUICIDEBAN")) );
			//Com_sprintf ( level.voteString, sizeof(level.voteString ), "clientkick %d", ent->s.number );
			//Com_sprintf ( level.voteDisplayString, sizeof(level.voteDisplayString), "kick %s", ent->client->pers.netname );
			//trap_SendConsoleCommand( EXEC_INSERT, va( "banClient %d\n", ent->s.number ) );
			trap_SendConsoleCommand( EXEC_INSERT, va( "clientkick %d\n", ent->s.number ) );
			return;
		}
		if ( g_autoKickKillSpammers.integer > 0
			&& ent->client->sess.killCount >= g_autoKickKillSpammers.integer )
		{
			trap_SendServerCommand( -1, va("print \"%s %s\n\"", ent->client->pers.netname, G_GetStringEdString("MP_SVGAME_ADMIN", "SUICIDEKICK")) );
			//Com_sprintf ( level.voteString, sizeof(level.voteString ), "clientkick %d", ent->s.number );
			//Com_sprintf ( level.voteDisplayString, sizeof(level.voteDisplayString), "kick %s", ent->client->pers.netname );
			trap_SendConsoleCommand( EXEC_INSERT, va( "clientkick %d\n", ent->s.number ) );
			return;
		}
		//okay, not gone (yet), but warn them...
		if ( g_autoBanKillSpammers.integer > 0
			&& (g_autoKickKillSpammers.integer <= 0 || g_autoBanKillSpammers.integer < g_autoKickKillSpammers.integer) )
		{//warn about ban
			trap_SendServerCommand( ent-g_entities, va("print \"%s\n\"", G_GetStringEdString("MP_SVGAME_ADMIN", "WARNINGSUICIDEBAN")) );
		}
		else if ( g_autoKickKillSpammers.integer > 0 )
		{//warn about kick
			trap_SendServerCommand( ent-g_entities, va("print \"%s\n\"", G_GetStringEdString("MP_SVGAME_ADMIN", "WARNINGSUICIDEKICK")) );
		}
	}
//[/AdminSys]
	ent->flags &= ~FL_GODMODE;
	ent->client->ps.stats[STAT_HEALTH] = ent->health = -999;
	player_die (ent, ent, ent, 100000, MOD_SUICIDE);
}

//[ClientNumFix]
gentity_t *G_GetDuelWinner(gclient_t *client)
{
	int i;
	gentity_t *wEnt;

	for ( i = 0 ; i < level.maxclients ; i++ ) {
		wEnt = &g_entities[i];
		
		if (wEnt->client && wEnt->client != client && wEnt->client->pers.connected == CON_CONNECTED && wEnt->client->sess.sessionTeam != TEAM_SPECTATOR)
		{
			return wEnt;
		}
	}

	return NULL;
}
#if 0
gentity_t *G_GetDuelWinner(gclient_t *client)
{
	gclient_t *wCl;
	int i;

	for ( i = 0 ; i < level.maxclients ; i++ ) {
		wCl = &level.clients[i];
		
		if (wCl && wCl != client && /*wCl->ps.clientNum != client->ps.clientNum &&*/
			wCl->pers.connected == CON_CONNECTED && wCl->sess.sessionTeam != TEAM_SPECTATOR)
		{
			return &g_entities[wCl->ps.clientNum];
		}
	}

	return NULL;
}
#endif
//[/ClientNumFix]

/*
=================
BroadCastTeamChange

Let everyone know about a team change
=================
*/
void BroadcastTeamChange( gclient_t *client, int oldTeam )
{
	client->ps.fd.forceDoInit = 1; //every time we change teams make sure our force powers are set right

	if (g_gametype.integer == GT_SIEGE)
	{ //don't announce these things in siege
		return;
	}

	//[CoOp]
	if ( g_gametype.integer == GT_SINGLE_PLAYER ) {
		if ( client->sess.sessionTeam == TEAM_SPECTATOR && oldTeam != TEAM_SPECTATOR ) {
			trap_SendServerCommand( -1, va("cp \"%s" S_COLOR_WHITE " %s\n\"",
			client->pers.netname, G_GetStringEdString("MP_SVGAME", "JOINEDTHESPECTATORS")));
		} else if ( client->sess.sessionTeam == TEAM_FREE ) {
			trap_SendServerCommand( -1, va("cp \"%s" S_COLOR_WHITE " %s\n\"",
			client->pers.netname, G_GetStringEdString("MP_SVGAME", "JOINEDTHEBATTLE")));
		}
	} else {
		if ( client->sess.sessionTeam == TEAM_RED ) {
			trap_SendServerCommand( -1, va("cp \"%s" S_COLOR_WHITE " %s\n\"",
				client->pers.netname, G_GetStringEdString("MP_SVGAME", "JOINEDTHEREDTEAM")) );
		} else if ( client->sess.sessionTeam == TEAM_BLUE ) {
			trap_SendServerCommand( -1, va("cp \"%s" S_COLOR_WHITE " %s\n\"",
			client->pers.netname, G_GetStringEdString("MP_SVGAME", "JOINEDTHEBLUETEAM")));
		} else if ( client->sess.sessionTeam == TEAM_SPECTATOR && oldTeam != TEAM_SPECTATOR ) {
			trap_SendServerCommand( -1, va("cp \"%s" S_COLOR_WHITE " %s\n\"",
			client->pers.netname, G_GetStringEdString("MP_SVGAME", "JOINEDTHESPECTATORS")));
		} else if ( client->sess.sessionTeam == TEAM_FREE ) {
			if (g_gametype.integer == GT_DUEL || g_gametype.integer == GT_POWERDUEL)
			{
				/*
				gentity_t *currentWinner = G_GetDuelWinner(client);

				if (currentWinner && currentWinner->client)
				{
					trap_SendServerCommand( -1, va("cp \"%s" S_COLOR_WHITE " %s %s\n\"",
					currentWinner->client->pers.netname, G_GetStringEdString("MP_SVGAME", "VERSUS"), client->pers.netname));
				}
				else
				{
					trap_SendServerCommand( -1, va("cp \"%s" S_COLOR_WHITE " %s\n\"",
					client->pers.netname, G_GetStringEdString("MP_SVGAME", "JOINEDTHEBATTLE")));
				}
				*/
				//NOTE: Just doing a vs. once it counts two players up
			}
			else
			{
				trap_SendServerCommand( -1, va("cp \"%s" S_COLOR_WHITE " %s\n\"",
				client->pers.netname, G_GetStringEdString("MP_SVGAME", "JOINEDTHEBATTLE")));
			}
		}
	}
	//[/CoOp]

	G_LogPrintf ( "setteam:  %i %s %s\n",
				  client - &level.clients[0],
				  TeamName ( oldTeam ),
				  TeamName ( client->sess.sessionTeam ) );
}

qboolean G_PowerDuelCheckFail(gentity_t *ent)
{
	int			loners = 0;
	int			doubles = 0;

	if (!ent->client || ent->client->sess.duelTeam == DUELTEAM_FREE)
	{
		return qtrue;
	}

	G_PowerDuelCount(&loners, &doubles, qfalse);

	if (ent->client->sess.duelTeam == DUELTEAM_LONE && loners >= 1)
	{
		return qtrue;
	}

	if (ent->client->sess.duelTeam == DUELTEAM_DOUBLE && doubles >= 2)
	{
		return qtrue;
	}

	return qfalse;
}

/*
=================
SetTeam
=================
*/
//[AdminSys]
int G_CountHumanPlayers( int team );
int G_CountBotPlayers( int team );
extern int OJP_PointSpread(void);
//[/AdminSys]
qboolean g_dontPenalizeTeam = qfalse;
qboolean g_preventTeamBegin = qfalse;
void SetTeam( gentity_t *ent, char *s ) {
	int					team, oldTeam;
	gclient_t			*client;
	int					clientNum;
	spectatorState_t	specState;
	int					specClient;
	int					teamLeader;

	//
	// see what change is requested
	//
	client = ent->client;

	clientNum = client - level.clients;
	specClient = 0;
	specState = SPECTATOR_NOT;
	if ( !Q_stricmp( s, "scoreboard" ) || !Q_stricmp( s, "score" )  ) {
		team = TEAM_SPECTATOR;
		specState = SPECTATOR_SCOREBOARD;
	} else if ( !Q_stricmp( s, "follow1" ) ) {
		team = TEAM_SPECTATOR;
		specState = SPECTATOR_FOLLOW;
		specClient = -1;
	} else if ( !Q_stricmp( s, "follow2" ) ) {
		team = TEAM_SPECTATOR;
		specState = SPECTATOR_FOLLOW;
		specClient = -2;
	} else if ( !Q_stricmp( s, "spectator" ) || !Q_stricmp( s, "s" ) ) {
		team = TEAM_SPECTATOR;
		specState = SPECTATOR_FREE;
	//[CoOp]
	} else if ( g_gametype.integer == GT_SINGLE_PLAYER ) 
	{//players spawn on NPCTEAM_PLAYER
		team = NPCTEAM_PLAYER;
	//[/CoOp]
	} else if ( g_gametype.integer >= GT_TEAM ) {
		// if running a team game, assign player to one of the teams
		specState = SPECTATOR_NOT;
		if ( !Q_stricmp( s, "red" ) || !Q_stricmp( s, "r" ) ) {
			team = TEAM_RED;
		} else if ( !Q_stricmp( s, "blue" ) || !Q_stricmp( s, "b" ) ) {
			team = TEAM_BLUE;
		} else {
			// pick the team with the least number of players
			//For now, don't do this. The legalize function will set powers properly now.
			/*
			if (g_forceBasedTeams.integer)
			{
				if (ent->client->ps.fd.forceSide == FORCE_LIGHTSIDE)
				{
					team = TEAM_BLUE;
				}
				else
				{
					team = TEAM_RED;
				}
			}
			else
			{
			*/
				//[AdminSys]
				team = PickTeam( clientNum, (ent->r.svFlags & SVF_BOT) );
				//team = PickTeam( clientNum );
				//[/AdminSys]
			//}
		}

		if ( g_teamForceBalance.integer && !g_trueJedi.integer ) 
		{//racc - override player's choice if the team balancer is in effect.
			int		counts[TEAM_NUM_TEAMS];

			//[ClientNumFix]
			counts[TEAM_BLUE] = TeamCount( ent-g_entities, TEAM_BLUE );
			counts[TEAM_RED] = TeamCount( ent-g_entities, TEAM_RED );
			//counts[TEAM_BLUE] = TeamCount( ent->client->ps.clientNum, TEAM_BLUE );
			//counts[TEAM_RED] = TeamCount( ent->client->ps.clientNUm, TEAM_RED );
			//[/ClientNumFix]

			// We allow a spread of two
			if ( team == TEAM_RED && counts[TEAM_RED] - counts[TEAM_BLUE] > 1 ) {
				//For now, don't do this. The legalize function will set powers properly now.
				/*
				if (g_forceBasedTeams.integer && ent->client->ps.fd.forceSide == FORCE_DARKSIDE)
				{
					//[ClientNumFix]
					trap_SendServerCommand( ent-g_entities, 
					//trap_SendServerCommand( ent->client->ps.clientNum, 
					//[/ClientNumFix]
						va("print \"%s\n\"", G_GetStringEdString("MP_SVGAME", "TOOMANYRED_SWITCH")) );
				}
				else
				*/
				{
					//[ClientNumFix]
					trap_SendServerCommand( ent-g_entities, 
					//trap_SendServerCommand( ent->client->ps.clientNum, 
					//[/ClientNumFix]
						va("print \"%s\n\"", G_GetStringEdString("MP_SVGAME", "TOOMANYRED")) );
				}
				return; // ignore the request
			}
			if ( team == TEAM_BLUE && counts[TEAM_BLUE] - counts[TEAM_RED] > 1 ) {
				//For now, don't do this. The legalize function will set powers properly now.
				/*
				if (g_forceBasedTeams.integer && ent->client->ps.fd.forceSide == FORCE_LIGHTSIDE)
				{
					//[ClientNumFix]
					trap_SendServerCommand( ent-g_entities, 
					//trap_SendServerCommand( ent->client->ps.clientNum, 
					//[/ClientNumFix]
						va("print \"%s\n\"", G_GetStringEdString("MP_SVGAME", "TOOMANYBLUE_SWITCH")) );
				}
				else
				*/
				{
					//[ClientNumFix]
					trap_SendServerCommand( ent-g_entities, 
					//trap_SendServerCommand( ent->client->ps.clientNum, 
					//[/ClientNumFix]
						va("print \"%s\n\"", G_GetStringEdString("MP_SVGAME", "TOOMANYBLUE")) );
				}
				return; // ignore the request
			}
			//[AdminSys]
			//balance based on team score
			if(g_teamForceBalance.integer >= 3 && g_gametype.integer != GT_SIEGE)
			{//check the scores 
				if(level.teamScores[TEAM_BLUE] - OJP_PointSpread() >= level.teamScores[TEAM_RED] 
					&& counts[TEAM_BLUE] >= counts[TEAM_RED] && team == TEAM_BLUE)
				{//blue team is ahead, don't add more players to that team
					//[ClientNumFix]
					trap_SendServerCommand( ent-g_entities, 
					//trap_SendServerCommand( ent->client->ps.clientNum, 
					//[/ClientNumFix]
						va("print \"%s\n\"", G_GetStringEdString("MP_SVGAME", "TOOMANYBLUE")) );
					return;
				}
				else if(level.teamScores[TEAM_RED] - OJP_PointSpread() >= level.teamScores[TEAM_BLUE] 
					&& counts[TEAM_RED] > counts[TEAM_BLUE] && team == TEAM_RED)
				{//red team is ahead, don't add more players to that team
					//[ClientNumFix]
					trap_SendServerCommand( ent-g_entities, 
					//trap_SendServerCommand( ent->client->ps.clientNum, 
					//[/ClientNumFix]
						va("print \"%s\n\"", G_GetStringEdString("MP_SVGAME", "TOOMANYRED")) );
					return;
				}
			}

			//teams have to be balanced in this situation, check for human/bot team balance. 
			if(g_teamForceBalance.integer == 4)
			{//check for human/bot 
				int BotCount[TEAM_NUM_TEAMS];
				int HumanCount = G_CountHumanPlayers( -1 );

				BotCount[TEAM_BLUE] = G_CountBotPlayers( TEAM_BLUE );
				BotCount[TEAM_RED] = G_CountBotPlayers( TEAM_RED );

				if(HumanCount < 2)
				{//don't worry about this check then since there's not enough humans to care.
				}
				else if(BotCount[TEAM_RED] - BotCount[TEAM_BLUE] > 1 
					&& !(ent->r.svFlags & SVF_BOT) && team == TEAM_BLUE)
				{//red team has too many bots, humans can't join blue
					//[ClientNumFix]
					trap_SendServerCommand( ent-g_entities, 
					//trap_SendServerCommand( ent->client->ps.clientNum, 
					//[/ClientNumFix]
						va("print \"%s\n\"", G_GetStringEdString("MP_SVGAME", "TOOMANYBLUE")) );
					return;
				}
				else if(BotCount[TEAM_BLUE] - BotCount[TEAM_RED] > 1
					&& !(ent->r.svFlags & SVF_BOT) && team == TEAM_RED)
				{//blue team has too many bots, humans can't join red
					//[ClientNumFix]
					trap_SendServerCommand( ent-g_entities, 
					//trap_SendServerCommand( ent->client->ps.clientNum, 
					//[/ClientNumFix]
						va("print \"%s\n\"", G_GetStringEdString("MP_SVGAME", "TOOMANYRED")) );
					return;
				}
			}
			//[/AdminSys]

			// It's ok, the team we are switching to has less or same number of players
		}

		//For now, don't do this. The legalize function will set powers properly now.
		/*
		if (g_forceBasedTeams.integer)
		{
			if (team == TEAM_BLUE && ent->client->ps.fd.forceSide != FORCE_LIGHTSIDE)
			{
				trap_SendServerCommand( ent-g_entities, va("print \"%s\n\"", G_GetStringEdString("MP_SVGAME", "MUSTBELIGHT")) );
				return;
			}
			if (team == TEAM_RED && ent->client->ps.fd.forceSide != FORCE_DARKSIDE)
			{
				trap_SendServerCommand( ent-g_entities, va("print \"%s\n\"", G_GetStringEdString("MP_SVGAME", "MUSTBEDARK")) );
				return;
			}
		}
		*/

	} else {
		// force them to spectators if there aren't any spots free
		team = TEAM_FREE;
	}
	
	//[BugFix41]
	oldTeam = client->sess.sessionTeam;
	//[/BugFix41]

	if (g_gametype.integer == GT_SIEGE)
	{
		if (client->tempSpectate >= level.time &&
			team == TEAM_SPECTATOR)
		{ //sorry, can't do that.
			return;
		}
		
		//[BugFix41]
		if ( team == oldTeam && team != TEAM_SPECTATOR ) {
			return;
		}
		//[/BugFix41]

		client->sess.siegeDesiredTeam = team;
		//oh well, just let them go.
		/*
		if (team != TEAM_SPECTATOR)
		{ //can't switch to anything in siege unless you want to switch to being a fulltime spectator
			//fill them in on their objectives for this team now
			trap_SendServerCommand(ent-g_entities, va("sb %i", client->sess.siegeDesiredTeam));

			trap_SendServerCommand( ent-g_entities, va("print \"You will be on the selected team the next time the round begins.\n\"") );
			return;
		}
		*/
		if (client->sess.sessionTeam != TEAM_SPECTATOR &&
			team != TEAM_SPECTATOR)
		{ //not a spectator now, and not switching to spec, so you have to wait til you die.
			//trap_SendServerCommand( ent-g_entities, va("print \"You will be on the selected team the next time you respawn.\n\"") );
			qboolean doBegin;
			if (ent->client->tempSpectate >= level.time)
			{
				doBegin = qfalse;
			}
			else
			{
				doBegin = qtrue;
			}

			if (doBegin)
			{
				// Kill them so they automatically respawn in the team they wanted.
				if (ent->health > 0)
				{
					ent->flags &= ~FL_GODMODE;
					ent->client->ps.stats[STAT_HEALTH] = ent->health = 0;
					player_die( ent, ent, ent, 100000, MOD_TEAM_CHANGE ); 
				}
			}

			if (ent->client->sess.sessionTeam != ent->client->sess.siegeDesiredTeam)
			{
				SetTeamQuick(ent, ent->client->sess.siegeDesiredTeam, qfalse);
			}

			return;
		}
	}

	// override decision if limiting the players
	if ( (g_gametype.integer == GT_DUEL)
		&& level.numNonSpectatorClients >= 2 )
	{
		team = TEAM_SPECTATOR;
	}
	else if ( (g_gametype.integer == GT_POWERDUEL)
		&& (level.numPlayingClients >= 3 || G_PowerDuelCheckFail(ent)) )
	{
		team = TEAM_SPECTATOR;
	}
	else if ( g_maxGameClients.integer > 0 && 
		level.numNonSpectatorClients >= g_maxGameClients.integer )
	{
		team = TEAM_SPECTATOR;
	}

	//
	// decide if we will allow the change
	//
	//[BugFix41]
	// moved this up above the siege check
	//oldTeam = client->sess.sessionTeam;
	//[/BugFix41]
	if ( team == oldTeam && team != TEAM_SPECTATOR ) {
		return;
	}

	//
	// execute the team change
	//

	//If it's siege then show the mission briefing for the team you just joined.
//	if (g_gametype.integer == GT_SIEGE && team != TEAM_SPECTATOR)
//	{
//		trap_SendServerCommand(clientNum, va("sb %i", team));
//	}

	// if the player was dead leave the body
	if ( client->ps.stats[STAT_HEALTH] <= 0 && client->sess.sessionTeam != TEAM_SPECTATOR ) {
		MaintainBodyQueue(ent);
	}

	// he starts at 'base'
	client->pers.teamState.state = TEAM_BEGIN;
	if ( oldTeam != TEAM_SPECTATOR ) {
		// Kill him (makes sure he loses flags, etc)
		ent->flags &= ~FL_GODMODE;
		ent->client->ps.stats[STAT_HEALTH] = ent->health = 0;
		g_dontPenalizeTeam = qtrue;
		player_die (ent, ent, ent, 100000, MOD_SUICIDE);
		g_dontPenalizeTeam = qfalse;

	}
	// they go to the end of the line for tournements
	if ( team == TEAM_SPECTATOR ) {
		if ( (g_gametype.integer != GT_DUEL) || (oldTeam != TEAM_SPECTATOR) )	{//so you don't get dropped to the bottom of the queue for changing skins, etc.
			client->sess.spectatorTime = level.time;
		}
	}

	client->sess.sessionTeam = team;
	client->sess.spectatorState = specState;
	client->sess.spectatorClient = specClient;

	client->sess.teamLeader = qfalse;
	if ( team == TEAM_RED || team == TEAM_BLUE ) {
		teamLeader = TeamLeader( team );
		// if there is no team leader or the team leader is a bot and this client is not a bot
		if ( teamLeader == -1 || ( !(g_entities[clientNum].r.svFlags & SVF_BOT) && (g_entities[teamLeader].r.svFlags & SVF_BOT) ) ) {
			//SetLeader( team, clientNum );
		}
	}
	// make sure there is a team leader on the team the player came from
	if ( oldTeam == TEAM_RED || oldTeam == TEAM_BLUE ) {
		CheckTeamLeader( oldTeam );
	}

	BroadcastTeamChange( client, oldTeam );

	//make a disappearing effect where they were before teleporting them to the appropriate spawn point,
	//if we were not on the spec team
	if (oldTeam != TEAM_SPECTATOR)
	{
		gentity_t *tent = G_TempEntity( client->ps.origin, EV_PLAYER_TELEPORT_OUT );
		tent->s.clientNum = clientNum;
	}

	// get and distribute relevent paramters
	ClientUserinfoChanged( clientNum );

	if (!g_preventTeamBegin)
	{
		ClientBegin( clientNum, qfalse );
	}
}

/*
=================
StopFollowing

If the client being followed leaves the game, or you just want to drop
to free floating spectator mode
=================
*/
//[BugFix38]
extern void G_LeaveVehicle( gentity_t *ent, qboolean ConCheck );
//[/BugFix38]
void StopFollowing( gentity_t *ent ) {
	ent->client->ps.persistant[ PERS_TEAM ] = TEAM_SPECTATOR;	
	ent->client->sess.sessionTeam = TEAM_SPECTATOR;	
	ent->client->sess.spectatorState = SPECTATOR_FREE;
	ent->client->ps.pm_flags &= ~PMF_FOLLOW;
	ent->r.svFlags &= ~SVF_BOT;
	ent->client->ps.clientNum = ent - g_entities;
	ent->client->ps.weapon = WP_NONE;
	//[BugFix38]
	G_LeaveVehicle( ent, qfalse ); // clears m_iVehicleNum as well
	//ent->client->ps.m_iVehicleNum = 0;
	//[/BugFix38]
	ent->client->ps.viewangles[ROLL] = 0.0f;
	ent->client->ps.forceHandExtend = HANDEXTEND_NONE;
	ent->client->ps.forceHandExtendTime = 0;
	ent->client->ps.zoomMode = 0;
	ent->client->ps.zoomLocked = 0;
	ent->client->ps.zoomLockTime = 0;
	ent->client->ps.legsAnim = 0;
	ent->client->ps.legsTimer = 0;
	ent->client->ps.torsoAnim = 0;
	ent->client->ps.torsoTimer = 0;
	//[DuelSys]
	ent->client->ps.duelInProgress = qfalse; // MJN - added to clean it up a bit.
	//[/DuelSys]
	//[BugFix38]
	//[OLDGAMETYPES]
	ent->client->ps.isJediMaster = qfalse; // major exploit if you are spectating somebody and they are JM and you reconnect
	//[/OLDGAMETYPES]
	ent->client->ps.cloakFuel = 100; // so that fuel goes away after stop following them
	ent->client->ps.jetpackFuel = 100; // so that fuel goes away after stop following them
	ent->health = ent->client->ps.stats[STAT_HEALTH] = 100; // so that you don't keep dead angles if you were spectating a dead person
	//[/BugFix38]
}

/*
=================
Cmd_Team_f
=================
*/
void Cmd_Team_f( gentity_t *ent ) {
	int			oldTeam;
	char		s[MAX_TOKEN_CHARS];

	//[ExpSys]
	//changed this so that we can link to this function thru the "forcechanged" behavior with its new design.
	if ( trap_Argc() < 2 ) {
	//if ( trap_Argc() != 2 ) {
	//[/ExpSys]
		oldTeam = ent->client->sess.sessionTeam;
		//[CoOp]
		if ( g_gametype.integer == GT_SINGLE_PLAYER ) {
			switch ( oldTeam ) {
			case NPCTEAM_PLAYER:
				trap_SendServerCommand( ent-g_entities, va("print \"%s\n\"", G_GetStringEdString("MP_SVGAME", "PRINTFREETEAM")) );
				break;
			case TEAM_SPECTATOR:
				trap_SendServerCommand( ent-g_entities, va("print \"%s\n\"", G_GetStringEdString("MP_SVGAME", "PRINTSPECTEAM")) );
				break;
			}
		} else {
			switch ( oldTeam ) {
			case TEAM_BLUE:
				trap_SendServerCommand( ent-g_entities, va("print \"%s\n\"", G_GetStringEdString("MP_SVGAME", "PRINTBLUETEAM")) );
				break;
			case TEAM_RED:
				trap_SendServerCommand( ent-g_entities, va("print \"%s\n\"", G_GetStringEdString("MP_SVGAME", "PRINTREDTEAM")) );
				break;
			case TEAM_FREE:
				trap_SendServerCommand( ent-g_entities, va("print \"%s\n\"", G_GetStringEdString("MP_SVGAME", "PRINTFREETEAM")) );
				break;
			case TEAM_SPECTATOR:
				trap_SendServerCommand( ent-g_entities, va("print \"%s\n\"", G_GetStringEdString("MP_SVGAME", "PRINTSPECTEAM")) );
				break;
			}
		}
		//[/CoOp]
		return;
	}

	if ( ent->client->switchTeamTime > level.time ) {
		trap_SendServerCommand( ent-g_entities, va("print \"%s\n\"", G_GetStringEdString("MP_SVGAME", "NOSWITCH")) );
		return;
	}

	if (gEscaping)
		return;

	//[CoOp]
	if (in_camera)
		return;
	//[/CoOp]

	// if they are playing a tournement game, count as a loss
	if ( g_gametype.integer == GT_DUEL
		&& ent->client->sess.sessionTeam == TEAM_FREE ) {//in a tournament game
		//disallow changing teams
		trap_SendServerCommand( ent-g_entities, "print \"Cannot switch teams in Duel\n\"" );
		return;
		//FIXME: why should this be a loss???
		//ent->client->sess.losses++;
	}

	if (g_gametype.integer == GT_POWERDUEL)
	{ //don't let clients change teams manually at all in powerduel, it will be taken care of through automated stuff
		trap_SendServerCommand( ent-g_entities, "print \"Cannot switch teams in Power Duel\n\"" );
		return;
	}

	trap_Argv( 1, s, sizeof( s ) );

	SetTeam( ent, s );

	ent->client->switchTeamTime = level.time + 5000;

}

/*
=================
Cmd_DuelTeam_f
=================
*/
void Cmd_DuelTeam_f(gentity_t *ent)
{
	int			oldTeam;
	char		s[MAX_TOKEN_CHARS];

	if (g_gametype.integer != GT_POWERDUEL)
	{ //don't bother doing anything if this is not power duel
		return;
	}

	/*
	if (ent->client->sess.sessionTeam != TEAM_SPECTATOR)
	{
		trap_SendServerCommand( ent-g_entities, va("print \"You cannot change your duel team unless you are a spectator.\n\""));
		return;
	}
	*/

	if ( trap_Argc() != 2 )
	{ //No arg so tell what team we're currently on.
		oldTeam = ent->client->sess.duelTeam;
		switch ( oldTeam )
		{
		case DUELTEAM_FREE:
			trap_SendServerCommand( ent-g_entities, va("print \"None\n\"") );
			break;
		case DUELTEAM_LONE:
			trap_SendServerCommand( ent-g_entities, va("print \"Single\n\"") );
			break;
		case DUELTEAM_DOUBLE:
			trap_SendServerCommand( ent-g_entities, va("print \"Double\n\"") );
			break;
		default:
			break;
		}
		return;
	}

	if ( ent->client->switchDuelTeamTime > level.time )
	{ //debounce for changing
		trap_SendServerCommand( ent-g_entities, va("print \"%s\n\"", G_GetStringEdString("MP_SVGAME", "NOSWITCH")) );
		return;
	}

	trap_Argv( 1, s, sizeof( s ) );

	oldTeam = ent->client->sess.duelTeam;

	if (!Q_stricmp(s, "free"))
	{
		ent->client->sess.duelTeam = DUELTEAM_FREE;
	}
	else if (!Q_stricmp(s, "single"))
	{
		ent->client->sess.duelTeam = DUELTEAM_LONE;
	}
	else if (!Q_stricmp(s, "double"))
	{
		ent->client->sess.duelTeam = DUELTEAM_DOUBLE;
	}
	else
	{
		trap_SendServerCommand( ent-g_entities, va("print \"'%s' not a valid duel team.\n\"", s) );
	}

	if (oldTeam == ent->client->sess.duelTeam)
	{ //didn't actually change, so don't care.
		return;
	}

	if (ent->client->sess.sessionTeam != TEAM_SPECTATOR)
	{ //ok..die
		int curTeam = ent->client->sess.duelTeam;
		ent->client->sess.duelTeam = oldTeam;
		G_Damage(ent, ent, ent, NULL, ent->client->ps.origin, 99999, DAMAGE_NO_PROTECTION, MOD_SUICIDE);
		ent->client->sess.duelTeam = curTeam;
	}
	//reset wins and losses
	ent->client->sess.wins = 0;
	ent->client->sess.losses = 0;

	//get and distribute relevent paramters
	ClientUserinfoChanged( ent->s.number );

	ent->client->switchDuelTeamTime = level.time + 5000;
}

int G_TeamForSiegeClass(const char *clName)
{
	int i = 0;
	int team = SIEGETEAM_TEAM1;
	siegeTeam_t *stm = BG_SiegeFindThemeForTeam(team);
	siegeClass_t *scl;

	if (!stm)
	{
		return 0;
	}

	while (team <= SIEGETEAM_TEAM2)
	{
		scl = stm->classes[i];

		if (scl && scl->name[0])
		{
			if (!Q_stricmp(clName, scl->name))
			{
				return team;
			}
		}

		i++;
		if (i >= MAX_SIEGE_CLASSES || i >= stm->numClasses)
		{
			if (team == SIEGETEAM_TEAM2)
			{
				break;
			}
			team = SIEGETEAM_TEAM2;
			stm = BG_SiegeFindThemeForTeam(team);
			i = 0;
		}
	}

	return 0;
}

/*
=================
Cmd_SiegeClass_f
=================
*/
void Cmd_SiegeClass_f( gentity_t *ent )
{
	char className[64];
	int team = 0;
	int preScore;
	qboolean startedAsSpec = qfalse;

	if (g_gametype.integer != GT_SIEGE)
	{ //classes are only valid for this gametype
		return;
	}

	if (!ent->client)
	{
		return;
	}

	if (trap_Argc() < 1)
	{
		return;
	}

	if ( ent->client->switchClassTime > level.time )
	{
		trap_SendServerCommand( ent-g_entities, va("print \"%s\n\"", G_GetStringEdString("MP_SVGAME", "NOCLASSSWITCH")) );
		return;
	}

	if (ent->client->sess.sessionTeam == TEAM_SPECTATOR)
	{
		startedAsSpec = qtrue;
	}

	trap_Argv( 1, className, sizeof( className ) );

	team = G_TeamForSiegeClass(className);

	if (!team)
	{ //not a valid class name
		return;
	}

	if (ent->client->sess.sessionTeam != team)
	{ //try changing it then
		g_preventTeamBegin = qtrue;
		if (team == TEAM_RED)
		{
			SetTeam(ent, "red");
		}
		else if (team == TEAM_BLUE)
		{
			SetTeam(ent, "blue");
		}
		g_preventTeamBegin = qfalse;

		if (ent->client->sess.sessionTeam != team)
		{ //failed, oh well
			if (ent->client->sess.sessionTeam != TEAM_SPECTATOR ||
				ent->client->sess.siegeDesiredTeam != team)
			{
				trap_SendServerCommand( ent-g_entities, va("print \"%s\n\"", G_GetStringEdString("MP_SVGAME", "NOCLASSTEAM")) );
				return;
			}
		}
	}

	//preserve 'is score
	preScore = ent->client->ps.persistant[PERS_SCORE];

	//Make sure the class is valid for the team
	BG_SiegeCheckClassLegality(team, className);

	//Set the session data
	strcpy(ent->client->sess.siegeClass, className);

	// get and distribute relevent paramters
	ClientUserinfoChanged( ent->s.number );

	if (ent->client->tempSpectate < level.time)
	{
		// Kill him (makes sure he loses flags, etc)
		if (ent->health > 0 && !startedAsSpec)
		{
			ent->flags &= ~FL_GODMODE;
			ent->client->ps.stats[STAT_HEALTH] = ent->health = 0;
			player_die (ent, ent, ent, 100000, MOD_SUICIDE);
		}

		if (ent->client->sess.sessionTeam == TEAM_SPECTATOR || startedAsSpec)
		{ //respawn them instantly.
			ClientBegin( ent->s.number, qfalse );
		}
	}
	//set it back after we do all the stuff
	ent->client->ps.persistant[PERS_SCORE] = preScore;

	ent->client->switchClassTime = level.time + 5000;
}

/*
=================
Cmd_ForceChanged_f
=================
*/
void Cmd_ForceChanged_f( gentity_t *ent )
{
	//[ExpSys]
	/* //racc - don't do this stuff anymore since forcepowers are now applied as soon as the client's userinfo updates.
	char fpChStr[1024];
	const char *buf;
//	Cmd_Kill_f(ent);
	if (ent->client->sess.sessionTeam == TEAM_SPECTATOR)
	{ //if it's a spec, just make the changes now
		//trap_SendServerCommand( ent-g_entities, va("print \"%s\n\"", G_GetStringEdString("MP_SVGAME", "FORCEAPPLIED")) );
		//No longer print it, as the UI calls this a lot.
		WP_InitForcePowers( ent );
		goto argCheck;
	}

	buf = G_GetStringEdString("MP_SVGAME", "FORCEPOWERCHANGED");

	strcpy(fpChStr, buf);

	trap_SendServerCommand( ent-g_entities, va("print \"%s%s\n\n\"", S_COLOR_GREEN, fpChStr) );

	ent->client->ps.fd.forceDoInit = 1;

argCheck:

	if (g_gametype.integer == GT_DUEL || g_gametype.integer == GT_POWERDUEL)
	{ //If this is duel, don't even bother changing team in relation to this.
		return;
	}
	*/
	//[/ExpSys]

	if (trap_Argc() > 1)
	{
		char	arg[MAX_TOKEN_CHARS];
		char	userinfo[MAX_INFO_STRING];

		trap_Argv( 2, arg, sizeof( arg ) );
		if (arg[0] && ent->client)
		{//new force power string, update the forcepower string.
			trap_GetUserinfo( ent->s.number, userinfo, sizeof( userinfo ) );
			Info_SetValueForKey( userinfo, "forcepowers", arg );
			trap_SetUserinfo( ent->s.number, userinfo );	

			if (ent->client->sess.sessionTeam == TEAM_SPECTATOR)
			{ //if it's a spec, just make the changes now
				//No longer print it, as the UI calls this a lot.
				WP_InitForcePowers( ent );
			}
			else
			{//wait til respawn and tell the player that.
				trap_SendServerCommand( ent-g_entities, va("print \"%s%s\n\n\"", S_COLOR_GREEN, G_GetStringEdString("MP_SVGAME", "FORCEPOWERCHANGED")) );

				ent->client->ps.fd.forceDoInit = 1;
			}
		}

		trap_Argv( 1, arg, sizeof( arg ) );
		if (arg[0] && arg[0] != 'x' && g_gametype.integer != GT_DUEL && g_gametype.integer != GT_POWERDUEL)
		{ //if there's an arg, assume it's a combo team command from the UI.
			Cmd_Team_f(ent);
		}
	}
}

//[StanceSelection]
qboolean G_ValidSaberStyle(gentity_t *ent, int saberStyle);
//extern qboolean WP_SaberStyleValidForSaber( saberInfo_t *saber1, saberInfo_t *saber2, int saberHolstered, int saberAnimLevel );
//[/StanceSelection]
extern qboolean WP_UseFirstValidSaberStyle( saberInfo_t *saber1, saberInfo_t *saber2, int saberHolstered, int *saberAnimLevel );
qboolean G_SetSaber(gentity_t *ent, int saberNum, char *saberName, qboolean siegeOverride)
{
	char truncSaberName[64];
	int i = 0;

	if (!siegeOverride &&
		g_gametype.integer == GT_SIEGE &&
		ent->client->siegeClass != -1 &&
		(
		 bgSiegeClasses[ent->client->siegeClass].saberStance ||
		 bgSiegeClasses[ent->client->siegeClass].saber1[0] ||
		 bgSiegeClasses[ent->client->siegeClass].saber2[0]
		))
	{ //don't let it be changed if the siege class has forced any saber-related things
        return qfalse;
	}

	while (saberName[i] && i < 64-1)
	{
        truncSaberName[i] = saberName[i];
		i++;
	}
	truncSaberName[i] = 0;

	if ( saberNum == 0 && (Q_stricmp( "none", truncSaberName ) == 0 || Q_stricmp( "remove", truncSaberName ) == 0) )
	{ //can't remove saber 0 like this
        strcpy(truncSaberName, "Kyle");
	}

	//Set the saber with the arg given. If the arg is
	//not a valid sabername defaults will be used.
	WP_SetSaber(ent->s.number, ent->client->saber, saberNum, truncSaberName);

	if (!ent->client->saber[0].model[0])
	{
		assert(0); //should never happen!
		strcpy(ent->client->sess.saberType, "none");
	}
	else
	{
		strcpy(ent->client->sess.saberType, ent->client->saber[0].name);
	}

	if (!ent->client->saber[1].model[0])
	{
		strcpy(ent->client->sess.saber2Type, "none");
	}
	else
	{
		strcpy(ent->client->sess.saber2Type, ent->client->saber[1].name);
	}

	//[StanceSelection]
	if ( !G_ValidSaberStyle(ent, ent->client->ps.fd.saberAnimLevel) )
	{//had an illegal style, revert to default
		ent->client->ps.fd.saberAnimLevel = SS_MEDIUM;
		ent->client->saberCycleQueue = ent->client->ps.fd.saberAnimLevel;
	}

	/*
	if ( !WP_SaberStyleValidForSaber( &ent->client->saber[0], &ent->client->saber[1], ent->client->ps.saberHolstered, ent->client->ps.fd.saberAnimLevel ) )
	{
		WP_UseFirstValidSaberStyle( &ent->client->saber[0], &ent->client->saber[1], ent->client->ps.saberHolstered, &ent->client->ps.fd.saberAnimLevel );
		ent->client->ps.fd.saberAnimLevelBase = ent->client->saberCycleQueue = ent->client->ps.fd.saberAnimLevel;
	}
	*/
	//[/StanceSelection]

	return qtrue;
}

/*
=================
Cmd_Follow_f
=================
*/
void Cmd_Follow_f( gentity_t *ent ) {
	int		i;
	char	arg[MAX_TOKEN_CHARS];

	if ( trap_Argc() != 2 ) {
		if ( ent->client->sess.spectatorState == SPECTATOR_FOLLOW ) {
			StopFollowing( ent );
		}
		return;
	}

	trap_Argv( 1, arg, sizeof( arg ) );
	i = ClientNumberFromString( ent, arg );
	if ( i == -1 ) {
		return;
	}

	// can't follow self
	if ( &level.clients[ i ] == ent->client ) {
		return;
	}

	// can't follow another spectator
	if ( level.clients[ i ].sess.sessionTeam == TEAM_SPECTATOR ) {
		return;
	}
	
	//[BugFix38]
	// can't follow another spectator
	if ( level.clients[ i ].tempSpectate >= level.time ) {
		return;
	}
	//[/BugFix38]

	// if they are playing a tournement game, count as a loss
	if ( (g_gametype.integer == GT_DUEL || g_gametype.integer == GT_POWERDUEL)
		&& ent->client->sess.sessionTeam == TEAM_FREE ) {
		//WTF???
		ent->client->sess.losses++;
	}

	// first set them to spectator
	if ( ent->client->sess.sessionTeam != TEAM_SPECTATOR ) {
		SetTeam( ent, "spectator" );
	}

	ent->client->sess.spectatorState = SPECTATOR_FOLLOW;
	ent->client->sess.spectatorClient = i;
}

/*
=================
Cmd_FollowCycle_f
=================
*/
void Cmd_FollowCycle_f( gentity_t *ent, int dir ) {
	int		clientnum;
	int		original;

	// if they are playing a tournement game, count as a loss
	if ( (g_gametype.integer == GT_DUEL || g_gametype.integer == GT_POWERDUEL)
		&& ent->client->sess.sessionTeam == TEAM_FREE ) {\
		//WTF???
		ent->client->sess.losses++;
	}
	// first set them to spectator
	if ( ent->client->sess.spectatorState == SPECTATOR_NOT ) {
		SetTeam( ent, "spectator" );
	}

	if ( dir != 1 && dir != -1 ) {
		G_Error( "Cmd_FollowCycle_f: bad dir %i", dir );
	}

	clientnum = ent->client->sess.spectatorClient;
	original = clientnum;
	do {
		clientnum += dir;
		if ( clientnum >= level.maxclients ) {
			clientnum = 0;
		}
		if ( clientnum < 0 ) {
			clientnum = level.maxclients - 1;
		}

		// can only follow connected clients
		if ( level.clients[ clientnum ].pers.connected != CON_CONNECTED ) {
			continue;
		}

		// can't follow another spectator
		if ( level.clients[ clientnum ].sess.sessionTeam == TEAM_SPECTATOR ) {
			continue;
		}

		//[BugFix38]
		// can't follow another spectator
		if ( level.clients[ clientnum ].tempSpectate >= level.time ) {
			return;
		}
		//[/BugFix38]

		// this is good, we can use it
		ent->client->sess.spectatorClient = clientnum;
		ent->client->sess.spectatorState = SPECTATOR_FOLLOW;
		return;
	} while ( clientnum != original );

	// leave it where it was
}


/*
==================
G_Say
==================
*/

static void G_SayTo( gentity_t *ent, gentity_t *other, int mode, int color, const char *name, const char *message, char *locMsg )
{
	if (!other) {
		return;
	}
	if (!other->inuse) {
		return;
	}
	if (!other->client) {
		return;
	}
	if ( other->client->pers.connected != CON_CONNECTED ) {
		return;
	}
	if ( mode == SAY_TEAM  && !OnSameTeam(ent, other) ) {
		return;
	}
	if ( mode == SAY_ADMIN && !other->client->pers.iamanadmin ) {
		return;
	}
	if ( mode == SAY_CLAN && !other->client->pers.iamclan ) {
		return;
	}
	/*
	// no chatting to players in tournements
	if ( (g_gametype.integer == GT_DUEL || g_gametype.integer == GT_POWERDUEL)
		&& other->client->sess.sessionTeam == TEAM_FREE
		&& ent->client->sess.sessionTeam != TEAM_FREE ) {
		//Hmm, maybe some option to do so if allowed?  Or at least in developer mode...
		return;
	}
	*/
	//They've requested I take this out.

	if (g_gametype.integer == GT_SIEGE &&
		ent->client && (ent->client->tempSpectate >= level.time || ent->client->sess.sessionTeam == TEAM_SPECTATOR) &&
		other->client->sess.sessionTeam != TEAM_SPECTATOR &&
		other->client->tempSpectate < level.time)
	{ //siege temp spectators should not communicate to ingame players
		return;
	}

	if (locMsg)
	{
		trap_SendServerCommand( other-g_entities, va("%s \"%s\" \"%s\" \"%c\" \"%s\"", 
			mode == SAY_TEAM ? "ltchat" : "lchat",
			name, locMsg, color, message));
	}
	else
	{
		trap_SendServerCommand( other-g_entities, va("%s \"%s%c%c%s\"", 
			mode == SAY_TEAM ? "tchat" : "chat",
			name, Q_COLOR_ESCAPE, color, message));
	}
}

#define EC		"\x19"


//[TABBot]
extern void TAB_BotOrder( gentity_t *orderer, gentity_t *orderee, int order, gentity_t *objective);
//This badboy of a function scans the say command for possible bot orders and then does them
void BotOrderParser(gentity_t *ent, gentity_t *target, int mode, const char *chatText)
{
	int i;
	//int x;
	char tempname[36];
	gclient_t	*cl;
	char *ordereeloc;
	gentity_t *orderee = NULL;
	char *temp;
	char text[MAX_SAY_TEXT];
	int order;
	gentity_t *objective = NULL;

	if(ent->r.svFlags & SVF_BOT)
	{//bots shouldn't give orders.  They were accidently giving orders to each other with some
		//of their taunt chats.
		return;
	}

	Q_strncpyz( text, chatText, sizeof(text) );
	Q_CleanStr(text);
	Q_strlwr(text);

	//place marker at end of chattext
	ordereeloc = text;
	ordereeloc += 8*MAX_SAY_TEXT;
	//ordereeloc = Q_strrchr(text, "\0");

	//ok, first look for a orderee
	for ( i=0 ; i< g_maxclients.integer ; i++ )
	{
		cl = level.clients + i;
		if ( cl->pers.connected != CON_CONNECTED )
		{
			continue;
		}
		//[ClientNumFix]
		if ( !(g_entities[i].r.svFlags & SVF_BOT) )
		//if ( !(g_entities[cl->ps.clientNum].r.svFlags & SVF_BOT) )
		//[/ClientNumFix]
		{
			continue;
		}
		strcpy(tempname, cl->pers.netname);
		Q_CleanStr(tempname);
		Q_strlwr(tempname);

		temp = strstr( text, tempname );	

		if(temp)
		{
			if(temp < ordereeloc)
			{
				ordereeloc = temp;
				//[ClientNumFix]
				orderee = &g_entities[i];
				//orderee = &g_entities[cl->ps.clientNum];
				//[/ClientNumFix]
			}
		}
	}
	
	if(!orderee)
	{//Couldn't find a bot to order
		return;
	}

	if(!OnSameTeam(ent, orderee))
	{//don't take orders from a guy on the other team.
		return;
	}

	G_Printf("%s\n", orderee->client->pers.netname);

	//ok, now determine the order given
	if(strstr(text, "kneel") || strstr(text, "bow"))
	{//BOTORDER_KNEELBEFOREZOD
		order = BOTORDER_KNEELBEFOREZOD;
	}
	else if(strstr(text, "attack") || strstr(text, "destroy"))
	{
		order = BOTORDER_SEARCHANDDESTROY;
	}
	else
	{//no order given.
		return;
	}

	//determine the target entity
	if(!objective)
	{
		if(strstr(text, "me"))
		{
			objective = ent;
		}
		else
		{//troll thru the player names for a possible objective entity.
			temp = NULL;
			for ( i=0 ; i< g_maxclients.integer ; i++ )
			{
				cl = level.clients + i;
				if ( cl->pers.connected != CON_CONNECTED )
				{
					continue;
				}
				//[ClientNumFix]
				if ( i == orderee-g_entities )
				//if ( cl->ps.clientNum == orderee->client->ps.clientNum )
				//[ClientNumFix]
				{//Don't want the orderee to be the target
					continue;
				}
				strcpy(tempname, cl->pers.netname);
				Q_CleanStr(tempname);
				Q_strlwr(tempname);

				temp = strstr( text, tempname );	

				if(temp)
				{
					if(temp > ordereeloc)
					{//Don't parse the orderee again
						//[ClientNumFix]
						objective = &g_entities[i];
						//objective = &g_entities[cl->ps.clientNum];
						//[ClientNumFix]
					}
				}
			}
		}
	}

	TAB_BotOrder(ent, orderee, order, objective);
}
//[/TABBot]

#define DIST_SAY_SPEAK 900
#define DIST_SAY_SHOUT 2000
#define DIST_SAY_LOW 500
#define DIST_SAY_FORCE 0
void G_Say( gentity_t *ent, gentity_t *target, int mode, const char *chatText ) {
	int			i,j,switcher,oldi,oldj;
   int         dice_amount[3];
   int         dice_sides[3];
	gentity_t	*other;
	int			color;
	char		name[64];
	// don't let text be too long for malicious reasons
	char		text[MAX_SAY_TEXT];
	char		location[64];
	char		*locMsg = NULL;
   int distance;
   char *chatMod;

	if ( g_gametype.integer < GT_TEAM && mode == SAY_TEAM ) {
		mode = SAY_ALL;
	}

	//[AdminSys][ChatSpamProtection]
	if(!(ent->r.svFlags & SVF_BOT))
	{//don't chat protect the bots.
		if(ent->client && ent->client->chatDebounceTime > level.time //debounce isn't up
			//and we're not bouncing our message back to our self while using SAY_TELL 
			&& (mode != SAY_TELL || ent != target)) 
		{//prevent players from spamming chat.
			//Warn them.
			if(ojp_chatProtectTime.integer > 0)
			{
				trap_SendServerCommand(ent->s.number, 
					va("cp \""S_COLOR_BLUE"Please Don't Spam.\nWait %.2f Seconds Before Trying Again.\n\"", 
					((float) ojp_chatProtectTime.integer/(float) 1000)));
			}
			return;
		}
		else
		{//we can chat, bump the debouncer
			ent->client->chatDebounceTime = level.time + ojp_chatProtectTime.integer;
		}
	}
	//[/AdminSys][/ChatSpamProtection]

	//[TABBot]
	//Scan for bot orders
	BotOrderParser(ent, target, mode, chatText);
	//[/TABBot]

	switch ( mode ) {
	default:
	case SAY_ALL:
      if(!Q_stricmpn("/shout", chatText, 6) || !Q_stricmpn("!", chatText, 1)) {
         if(!Q_stricmpn("/shout", chatText, 6)) {
            chatMod = (char*)chatText+7;
         } else {
            chatMod = (char*)chatText+1;
         }
         G_LogPrintf( "Shout: %s shouts: %s\n", ent->client->pers.netname, chatMod );
         Com_sprintf (name, sizeof(name), "%s%c%c"EC" shouts: ", ent->client->pers.netname, Q_COLOR_ESCAPE, COLOR_WHITE );
         color = COLOR_RED;
         distance = DIST_SAY_SHOUT;
      } else if(!Q_stricmpn("/me", chatText, 3) || !Q_stricmpn("@", chatText, 1)) {
         if(!Q_stricmpn("/me", chatText, 3)) {
         chatMod = (char*)chatText+4;
         } else {
         chatMod = (char*)chatText+1;
         }
         G_LogPrintf( "Action: %s %s\n", ent->client->pers.netname, chatMod );
         Com_sprintf (name, sizeof(name), "%s%c%c"EC" ", ent->client->pers.netname, Q_COLOR_ESCAPE, COLOR_WHITE );
         color = COLOR_YELLOW;
         distance = DIST_SAY_SHOUT;
	  }
	  else if (!Q_stricmpn("^3rolls", chatText, 7)) {
		  chatMod = (char*)chatText;
		  G_LogPrintf("Roll Action: %s %s\n", ent->client->pers.netname, chatMod);
		  Com_sprintf(name, sizeof(name), "%s%c%c"EC" ", ent->client->pers.netname, Q_COLOR_ESCAPE, COLOR_WHITE);
		  color = COLOR_YELLOW;
		  distance = DIST_SAY_SHOUT;
	  }
	  else if (!Q_stricmpn("/low", chatText, 4) || !Q_stricmpn(".", chatText, 1)) {
		  if (!Q_stricmpn("/low", chatText, 4)) {
        chatMod = (char*)chatText + 5;
        } else {
        chatMod = (char*)chatText + 1;
        }
		  G_LogPrintf("Say (low): %s says (low): %s\n", ent->client->pers.netname, chatMod);
		  Com_sprintf(name, sizeof(name), "%s%c%c"EC" says (low): ", ent->client->pers.netname, Q_COLOR_ESCAPE, COLOR_WHITE);
		  color = COLOR_WHITE;
		  distance = DIST_SAY_LOW;
	  }	
	  else if (!Q_stricmpn("/comm", chatText, 5) || !Q_stricmpn("#", chatText, 1)) {
if (!Q_stricmpn("/comm", chatText, 5) {		
      chatMod = (char*)chatText + 6;
      } else {
      chatMod = (char*)chatText + 1;
      }
		  G_LogPrintf("Comm (Comm) %s: %s\n", ent->client->pers.netname, chatMod);
		  Com_sprintf(name, sizeof(name), "%s%c%c"EC" says (comm): ", ent->client->pers.netname, Q_COLOR_ESCAPE, COLOR_WHITE);
		  color = COLOR_CYAN;
		  distance = 0;
	  }
	  else if(!Q_stricmpn("/ooc", chatText, 4) || !Q_stricmpn("//", chatText, 2)) {
         if(!Q_stricmpn("/ooc", chatText, 4)) {
         chatMod = (char*)chatText+5;
         } else {
         chatMod = (char*)chatText+2;
         }
         G_LogPrintf( "OOC: (OOC) %s: %s\n", ent->client->pers.netname, chatMod );
         Com_sprintf (name, sizeof(name), "(OOC) %s%c%c"EC": ", ent->client->pers.netname, Q_COLOR_ESCAPE, COLOR_WHITE );
         color = COLOR_GREEN;
         distance = 0;
      } else {
         chatMod = (char*)chatText;
         G_LogPrintf( "say: %s says: %s\n", ent->client->pers.netname, chatMod );
         Com_sprintf (name, sizeof(name), "%s%c%c"EC" says: ", ent->client->pers.netname, Q_COLOR_ESCAPE, COLOR_WHITE );
         color = COLOR_WHITE;
         distance = DIST_SAY_SPEAK;
      }
		break;
	case SAY_TEAM:
		G_LogPrintf( "sayteam: %s: %s\n", ent->client->pers.netname, chatText );
		if (Team_GetLocationMsg(ent, location, sizeof(location)))
		{
			Com_sprintf (name, sizeof(name), EC"(%s%c%c"EC")"EC": ", 
				ent->client->pers.netname, Q_COLOR_ESCAPE, COLOR_WHITE );
			locMsg = location;
		}
		else
		{
			Com_sprintf (name, sizeof(name), EC"(%s%c%c"EC")"EC": ", 
				ent->client->pers.netname, Q_COLOR_ESCAPE, COLOR_WHITE );
		}
      chatMod = (char*)chatText;
		color = COLOR_CYAN;
		break;
	case SAY_TELL:
		if (target && g_gametype.integer >= GT_TEAM &&
			target->client->sess.sessionTeam == ent->client->sess.sessionTeam &&
			Team_GetLocationMsg(ent, location, sizeof(location)))
		{
			Com_sprintf (name, sizeof(name), EC"[%s%c%c"EC"]"EC": ", ent->client->pers.netname, Q_COLOR_ESCAPE, COLOR_WHITE );
			locMsg = location;
		}
		else
		{
			Com_sprintf (name, sizeof(name), EC"[%s%c%c"EC"]"EC": ", ent->client->pers.netname, Q_COLOR_ESCAPE, COLOR_WHITE );
		}
      chatMod = (char*)chatText;
		color = COLOR_MAGENTA;
		break;
   case SAY_ADMIN:
		G_LogPrintf( "sayteam: <admin>[%s]: %s\n", ent->client->pers.netname, chatText );
		Com_sprintf (name, sizeof(name), EC"^3<admin>^7[%s^7]: ", 
				ent->client->pers.netname );
		color = COLOR_YELLOW;
		break;
	case SAY_CLAN:
		G_LogPrintf( "sayteam: <clan>[%s]: %s\n", ent->client->pers.netname, chatText );
		Com_sprintf (name, sizeof(name), EC"^1<clan>^7[%s^7]: ", 
				ent->client->pers.netname );
		color = COLOR_RED;
		break;
	}

	Q_strncpyz( text, chatMod, sizeof(text) );

	if ( target ) {
		G_SayTo( ent, target, mode, color, name, text, locMsg );
		return;
	}

	// echo the text to the console
	if ( g_dedicated.integer ) {
		G_Printf( "%s%s\n", name, text);
	}
	if ( strstr( text, "!freezemotd") ){
		trap_SendServerCommand( ent-g_entities, va("print \"^7Freezing MOTD... ^3(TYPE IN !hidemotd OR /hidemotd TO HIDE IT!)\n\"" ) );
		strcpy(ent->client->csMessage, G_NewString(va("^0*^1%s^0*\n\n^7%s\n", GAMEVERSION, roar_motd_line.string )));
		ent->client->csTimeLeft = Q3_INFINITE;
	}

	if ( strstr( text, "!showmotd") ){
		trap_SendServerCommand( ent-g_entities, va("print \"^7Showing MOTD...\n\"" ) );
		strcpy(ent->client->csMessage, G_NewString(va("^0*^1%s^0*\n\n^7%s\n", GAMEVERSION, roar_motd_line.string )));
		ent->client->csTimeLeft = x_cstime.integer;
	}
if ( strstr( text, "!jetpack") ){
if (roar_allow_jetpack_command.integer == 1)
	   {
		   if (ent->client->ps.duelInProgress){
			   trap_SendServerCommand( ent-g_entities, va("print \"Jetpack is not allowed in duels!\n\"" ) );
		   }
		   else if (ent->client->ps.stats[STAT_HOLDABLE_ITEMS] & (1 << HI_JETPACK)) 
   { 
      ent->client->ps.stats[STAT_HOLDABLE_ITEMS] &= ~(1 << HI_JETPACK); 
   } 
   else 
   { 
      ent->client->ps.stats[STAT_HOLDABLE_ITEMS] |= (1 << HI_JETPACK); 
   }
   	if (ent->client->jetPackOn)
	{
		Jetpack_Off(ent);
	}
   }
	else {
		trap_SendServerCommand( ent-g_entities, va("print \"Jetpack is disabled on this server!\n\"" ) );
	}
}

	if ( strstr( text, "!slapme") ){
		if (ent->client->ps.forceHandExtend == HANDEXTEND_KNOCKDOWN)
		{
			return;
		}
		if (roar_allow_KnockMeDown_command.integer == 0)
		{
			trap_SendServerCommand( ent-g_entities, va("print \"^7KnockMeDown is disabled on this server!\n\"" ) );
		}
		else if (ent->health < 1 || (ent->client->ps.eFlags & EF_DEAD))
			{
			}
		else
		{
			ent->client->ps.velocity[2] = 375;
			ent->client->ps.forceHandExtend = HANDEXTEND_KNOCKDOWN;
			ent->client->ps.forceDodgeAnim = 0;
			ent->client->ps.forceHandExtendTime = level.time + 700;
		}
	}

	if (Q_stricmp( text, "!hidemotd") == 0 )
		{
			trap_SendServerCommand( ent-g_entities, va("print \"^7Hiding MOTD...\n\"" ) );
			ent->client->csTimeLeft -= 0;
			strcpy(ent->client->csMessage, G_NewString(va(" \n" )));
		}
	// send it to all the apropriate clients
	for (j = 0; j < level.maxclients; j++) {
		other = &g_entities[j];
      if ( mode == SAY_ALL ) {
         if ( (Distance( ent->client->ps.origin, other->client->ps.origin ) <= distance) || distance == 0) {
            G_SayTo( ent, other, mode, color, name, text, locMsg );
         } else {
            continue;
         }
      } else {
         G_SayTo( ent, other, mode, color, name, text, locMsg );
      }
	}
}


/*
==================
Cmd_Say_f
==================
*/
static void Cmd_Say_f( gentity_t *ent, int mode, qboolean arg0 ) {
	char		*p;

	if ( trap_Argc () < 2 && !arg0 ) {
		return;
	}

	if (arg0)
	{
		p = ConcatArgs( 0 );
	}
	else
	{
		p = ConcatArgs( 1 );
	}

	G_Say( ent, NULL, mode, p );
}

/*
==================
Cmd_Tell_f
==================
*/
static void Cmd_Tell_f( gentity_t *ent ) {
	int			targetNum;
	gentity_t	*target;
	char		*p;
	char		arg[MAX_TOKEN_CHARS];

	if ( trap_Argc () < 2 ) {
		return;
	}

	trap_Argv( 1, arg, sizeof( arg ) );
	targetNum = atoi( arg );
	if ( targetNum < 0 || targetNum >= level.maxclients ) {
		return;
	}

	target = &g_entities[targetNum];
	if ( !target || !target->inuse || !target->client ) {
		return;
	}

	p = ConcatArgs( 2 );

	G_LogPrintf( "tell: %s to %s: %s\n", ent->client->pers.netname, target->client->pers.netname, p );
	G_Say( ent, target, SAY_TELL, p );
	// don't tell to the player self if it was already directed to this player
	// also don't send the chat back to a bot
	if ( ent != target && !(ent->r.svFlags & SVF_BOT)) {
		G_Say( ent, ent, SAY_TELL, p );
	}
}

//siege voice command
static void Cmd_VoiceCommand_f(gentity_t *ent)
{
	gentity_t *te;
	char arg[MAX_TOKEN_CHARS];
	char *s;
	int i = 0;

	if (g_gametype.integer < GT_TEAM)
	{
		return;
	}

	if (trap_Argc() < 2)
	{
		return;
	}

	if (ent->client->sess.sessionTeam == TEAM_SPECTATOR ||
		ent->client->tempSpectate >= level.time)
	{
		trap_SendServerCommand( ent-g_entities, va("print \"%s\n\"", G_GetStringEdString("MP_SVGAME", "NOVOICECHATASSPEC")) );
		return;
	}

	trap_Argv(1, arg, sizeof(arg));

	if (arg[0] == '*')
	{ //hmm.. don't expect a * to be prepended already. maybe someone is trying to be sneaky.
		return;
	}

	s = va("*%s", arg);

	//now, make sure it's a valid sound to be playing like this.. so people can't go around
	//screaming out death sounds or whatever.
	while (i < MAX_CUSTOM_SIEGE_SOUNDS)
	{
		if (!bg_customSiegeSoundNames[i])
		{
			break;
		}
		if (!Q_stricmp(bg_customSiegeSoundNames[i], s))
		{ //it matches this one, so it's ok
			break;
		}
		i++;
	}

	if (i == MAX_CUSTOM_SIEGE_SOUNDS || !bg_customSiegeSoundNames[i])
	{ //didn't find it in the list
		return;
	}

	te = G_TempEntity(vec3_origin, EV_VOICECMD_SOUND);
	te->s.groundEntityNum = ent->s.number;
	te->s.eventParm = G_SoundIndex((char *)bg_customSiegeSoundNames[i]);
	te->r.svFlags |= SVF_BROADCAST;
}


static char	*gc_orders[] = {
	"hold your position",
	"hold this position",
	"come here",
	"cover me",
	"guard location",
	"search and destroy",
	"report"
};

void Cmd_GameCommand_f( gentity_t *ent ) {
	int		player;
	int		order;
	char	str[MAX_TOKEN_CHARS];

	trap_Argv( 1, str, sizeof( str ) );
	player = atoi( str );
	trap_Argv( 2, str, sizeof( str ) );
	order = atoi( str );

	if ( player < 0 || player >= MAX_CLIENTS ) {
		return;
	}
	if ( order < 0 || order > sizeof(gc_orders)/sizeof(char *) ) {
		return;
	}
	G_Say( ent, &g_entities[player], SAY_TELL, gc_orders[order] );
	G_Say( ent, ent, SAY_TELL, gc_orders[order] );
}

/*
==================
Cmd_Where_f
==================
*/
void Cmd_Where_f( gentity_t *ent ) {
	//[BugFix31]
	//This wasn't working for non-spectators since s.origin doesn't update for active players.
	if(ent->client && ent->client->sess.sessionTeam != TEAM_SPECTATOR )
	{//active players use currentOrigin
		trap_SendServerCommand( ent-g_entities, va("print \"%s\n\"", vtos( ent->r.currentOrigin ) ) );
	}
	else
	{
		trap_SendServerCommand( ent-g_entities, va("print \"%s\n\"", vtos( ent->s.origin ) ) );
	}
	//trap_SendServerCommand( ent-g_entities, va("print \"%s\n\"", vtos( ent->s.origin ) ) );
	//[/BugFix31]
}

static const char *gameNames[] = {
	"Free For All",
	"Holocron FFA",
	"Jedi Master",
	"Duel",
	"Power Duel",
	"Single Player",
	"Team FFA",
	"Siege",
	"Capture the Flag",
	"Capture the Ysalamiri"
};

/*
==================
G_ClientNumberFromName

Finds the client number of the client with the given name
==================
*/
int G_ClientNumberFromName ( const char* name )
{
	char		s2[MAX_STRING_CHARS];
	char		n2[MAX_STRING_CHARS];
	int			i;
	gclient_t*	cl;

	// check for a name match
	SanitizeString( (char*)name, s2 );
	for ( i=0, cl=level.clients ; i < level.numConnectedClients ; i++, cl++ ) 
	{
		SanitizeString( cl->pers.netname, n2 );
		if ( !strcmp( n2, s2 ) ) 
		{
			return i;
		}
	}

	return -1;
}

/*
==================
G_ClientNumberFromStrippedName

Same as above, but strips special characters out of the names before comparing.
==================
*/
int G_ClientNumberFromStrippedName ( const char* name )
{
	char		s2[MAX_STRING_CHARS];
	char		n2[MAX_STRING_CHARS];
	int			i;
	gclient_t*	cl;

	// check for a name match
	SanitizeString2( (char*)name, s2 );
	for ( i=0, cl=level.clients ; i < level.numConnectedClients ; i++, cl++ ) 
	{
		SanitizeString2( cl->pers.netname, n2 );
		if ( !strcmp( n2, s2 ) ) 
		{
			return i;
		}
	}

	return -1;
}

/*
==================
Cmd_CallVote_f
==================
*/

//[AdminSys]
void Cmd_CallTeamVote_f( gentity_t *ent );
//[/AdminSys]
extern void SiegeClearSwitchData(void); //g_saga.c
const char *G_GetArenaInfoByMap( const char *map );
void Cmd_CallVote_f( gentity_t *ent ) {
	int		i;
	char	arg1[MAX_STRING_TOKENS];
	char	arg2[MAX_STRING_TOKENS];
//	int		n = 0;
//	char*	type = NULL;
	char*		mapName = 0;
	const char*	arenaInfo;

	if ( !g_allowVote.integer ) {
		//[AdminSys]
		//try teamvote if available.
		trap_Argv( 1, arg1, sizeof( arg1 ) );
		if(g_allowTeamVote.integer && !Q_stricmp( arg1, "kick" ))
		{
			Cmd_CallTeamVote_f( ent );
		}
		else
		{
			trap_SendServerCommand( ent-g_entities, va("print \"%s\n\"", G_GetStringEdString("MP_SVGAME", "NOVOTE")) );
		}
		//trap_SendServerCommand( ent-g_entities, va("print \"%s\n\"", G_GetStringEdString("MP_SVGAME", "NOVOTE")) );
		//[/AdminSys]
		return;
	}

	if ( level.voteTime || level.voteExecuteTime >= level.time ) {
		trap_SendServerCommand( ent-g_entities, va("print \"%s\n\"", G_GetStringEdString("MP_SVGAME", "VOTEINPROGRESS")) );
		return;
	}
	if ( ent->client->pers.voteCount >= MAX_VOTE_COUNT ) {
		trap_SendServerCommand( ent-g_entities, va("print \"%s\n\"", G_GetStringEdString("MP_SVGAME", "MAXVOTES")) );
		return;
	}

	if (g_gametype.integer != GT_DUEL &&
		g_gametype.integer != GT_POWERDUEL)
	{
		if ( ent->client->sess.sessionTeam == TEAM_SPECTATOR ) {
			trap_SendServerCommand( ent-g_entities, va("print \"%s\n\"", G_GetStringEdString("MP_SVGAME", "NOSPECVOTE")) );
			return;
		}
	}

	// make sure it is a valid command to vote on
	trap_Argv( 1, arg1, sizeof( arg1 ) );
	trap_Argv( 2, arg2, sizeof( arg2 ) );

	if( strchr( arg1, ';' ) || strchr( arg2, ';' ) ) {
		trap_SendServerCommand( ent-g_entities, "print \"Invalid vote string.\n\"" );
		return;
	}

	if ( !Q_stricmp( arg1, "map_restart" ) ) {
	} else if ( !Q_stricmp( arg1, "nextmap" ) ) {
	} else if ( !Q_stricmp( arg1, "map" ) ) {
	} else if ( !Q_stricmp( arg1, "g_gametype" ) ) {
	} else if ( !Q_stricmp( arg1, "kick" ) ) {
	} else if ( !Q_stricmp( arg1, "clientkick" ) ) {
	} else if ( !Q_stricmp( arg1, "g_doWarmup" ) ) {
	} else if ( !Q_stricmp( arg1, "timelimit" ) ) {
	} else if ( !Q_stricmp( arg1, "fraglimit" ) ) {
	} else {
		trap_SendServerCommand( ent-g_entities, "print \"Invalid vote string.\n\"" );
		trap_SendServerCommand( ent-g_entities, "print \"Vote commands are: map_restart, nextmap, map <mapname>, g_gametype <n>, kick <player>, clientkick <clientnum>, g_doWarmup, timelimit <time>, fraglimit <frags>.\n\"" );
		return;
	}

	// if there is still a vote to be executed
	if ( level.voteExecuteTime ) {
		level.voteExecuteTime = 0;
		trap_SendConsoleCommand( EXEC_APPEND, va("%s\n", level.voteString ) );
	}

	// special case for g_gametype, check for bad values
	if ( !Q_stricmp( arg1, "g_gametype" ) )
	{
		//[AdminSys]
		if(!g_allowGametypeVote.integer)
		{
			trap_SendServerCommand( ent-g_entities, "print \"Gametype voting is disabled.\n\"" );
			return;
		}
		//[/AdminSys]

		i = atoi( arg2 );
		if( i == GT_SINGLE_PLAYER || i < GT_FFA || i >= GT_MAX_GAME_TYPE) {
			trap_SendServerCommand( ent-g_entities, "print \"Invalid gametype.\n\"" );
			return;
		}

		level.votingGametype = qtrue;
		level.votingGametypeTo = i;

		Com_sprintf( level.voteString, sizeof( level.voteString ), "%s %d", arg1, i );
		Com_sprintf( level.voteDisplayString, sizeof( level.voteDisplayString ), "%s %s", arg1, gameNames[i] );
	}
	else if ( !Q_stricmp( arg1, "map" ) ) 
	{
		// special case for map changes, we want to reset the nextmap setting
		// this allows a player to change maps, but not upset the map rotation
		char	s[MAX_STRING_CHARS];

		//[AdminSys]
		if(g_AllowMapVote.integer != 2)
		{
			if(g_AllowMapVote.integer == 1)
			{
				trap_SendServerCommand( ent-g_entities, "print \"You can only do map restart and nextmap votes while in restricting mode voting mode.\n\"" );
			}
			else
			{
				trap_SendServerCommand( ent-g_entities, "print \"Map voting is disabled.\n\"" );
			}
			return;
		}
		//[/AdminSys]

		if (!G_DoesMapSupportGametype(arg2, trap_Cvar_VariableIntegerValue("g_gametype")))
		{
			//trap_SendServerCommand( ent-g_entities, "print \"You can't vote for this map, it isn't supported by the current gametype.\n\"" );
			trap_SendServerCommand( ent-g_entities, va("print \"%s\n\"", G_GetStringEdString("MP_SVGAME", "NOVOTE_MAPNOTSUPPORTEDBYGAME")) );
			return;
		}

		trap_Cvar_VariableStringBuffer( "nextmap", s, sizeof(s) );
		if (*s) {
			Com_sprintf( level.voteString, sizeof( level.voteString ), "%s %s; set nextmap \"%s\"", arg1, arg2, s );
		} else {
			Com_sprintf( level.voteString, sizeof( level.voteString ), "%s %s", arg1, arg2 );
		}
		
		arenaInfo	= G_GetArenaInfoByMap(arg2);
		if (arenaInfo)
		{
			mapName = Info_ValueForKey(arenaInfo, "longname");
		}

		if (!mapName || !mapName[0])
		{
			mapName = "ERROR";
		}

		Com_sprintf( level.voteDisplayString, sizeof( level.voteDisplayString ), "map %s", mapName);
	}
	else if ( !Q_stricmp ( arg1, "clientkick" ) )
	{
		int n = atoi ( arg2 );

		//[AdminSys]
		if(!g_AllowKickVote.integer)
		{
			trap_SendServerCommand( ent-g_entities, "print \"Kick voting is disabled.\n\"" );
			return;
		}
		//[/AdminSys]

		if ( n < 0 || n >= MAX_CLIENTS )
		{
			trap_SendServerCommand( ent-g_entities, va("print \"invalid client number %d.\n\"", n ) );
			return;
		}

		if ( g_entities[n].client->pers.connected == CON_DISCONNECTED )
		{
			trap_SendServerCommand( ent-g_entities, va("print \"there is no client with the client number %d.\n\"", n ) );
			return;
		}
			
		Com_sprintf ( level.voteString, sizeof(level.voteString ), "%s %s", arg1, arg2 );
		Com_sprintf ( level.voteDisplayString, sizeof(level.voteDisplayString), "kick %s", g_entities[n].client->pers.netname );
	}
	else if ( !Q_stricmp ( arg1, "kick" ) )
	{
		int clientid = G_ClientNumberFromName ( arg2 );

		//[AdminSys]
		if(!g_AllowKickVote.integer)
		{
			trap_SendServerCommand( ent-g_entities, "print \"Kick voting is disabled.\n\"" );
			return;
		}
		//[/AdminSys]

		if ( clientid == -1 )
		{
			clientid = G_ClientNumberFromStrippedName(arg2);

			if (clientid == -1)
			{
				trap_SendServerCommand( ent-g_entities, va("print \"there is no client named '%s' currently on the server.\n\"", arg2 ) );
				return;
			}
		}

		Com_sprintf ( level.voteString, sizeof(level.voteString ), "clientkick %d", clientid );
		Com_sprintf ( level.voteDisplayString, sizeof(level.voteDisplayString), "kick %s", g_entities[clientid].client->pers.netname );
	}
	else if ( !Q_stricmp( arg1, "nextmap" ) ) 
	{
		char	s[MAX_STRING_CHARS];

		//[AdminSys]
		if(!g_AllowMapVote.integer)
		{
			trap_SendServerCommand( ent-g_entities, "print \"Map voting is disabled.\n\"" );
			return;
		}
		//[/AdminSys]

		trap_Cvar_VariableStringBuffer( "nextmap", s, sizeof(s) );
		if (!*s) {
			trap_SendServerCommand( ent-g_entities, "print \"nextmap not set.\n\"" );
			return;
		}
		SiegeClearSwitchData();
		Com_sprintf( level.voteString, sizeof( level.voteString ), "vstr nextmap");
		Com_sprintf( level.voteDisplayString, sizeof( level.voteDisplayString ), "%s", level.voteString );
	} 
	else
	{
		//[AdminSys]
		if(!g_AllowMapVote.integer && !Q_stricmp( arg1, "map_restart" ))
		{
			trap_SendServerCommand( ent-g_entities, "print \"Map voting is disabled.\n\"" );
			return;
		}
		//[/AdminSys]

		Com_sprintf( level.voteString, sizeof( level.voteString ), "%s \"%s\"", arg1, arg2 );
		Com_sprintf( level.voteDisplayString, sizeof( level.voteDisplayString ), "%s", level.voteString );
	}

	trap_SendServerCommand( -1, va("print \"%s^7 %s\n\"", ent->client->pers.netname, G_GetStringEdString("MP_SVGAME", "PLCALLEDVOTE") ) );

	// start the voting, the caller autoamtically votes yes
	level.voteTime = level.time;
	level.voteYes = 1;
	level.voteNo = 0;

	for ( i = 0 ; i < level.maxclients ; i++ ) {
		level.clients[i].mGameFlags &= ~PSG_VOTED;
	}
	ent->client->mGameFlags |= PSG_VOTED;

	trap_SetConfigstring( CS_VOTE_TIME, va("%i", level.voteTime ) );
	trap_SetConfigstring( CS_VOTE_STRING, level.voteDisplayString );	
	trap_SetConfigstring( CS_VOTE_YES, va("%i", level.voteYes ) );
	trap_SetConfigstring( CS_VOTE_NO, va("%i", level.voteNo ) );	
}

/*
==================
Cmd_Vote_f
==================
*/
void Cmd_Vote_f( gentity_t *ent ) {
	char		msg[64];

	//[AdminSys]
	//make it so that the vote command applies to team votes first before normal votes.
	int team = ent->client->sess.sessionTeam;
	int cs_offset = -1;
		
	team = ent->client->sess.sessionTeam;

	if ( team == TEAM_RED )
		cs_offset = 0;
	else if ( team == TEAM_BLUE )
		cs_offset = 1;

	if( cs_offset != -1)
	{//we're on a team
		if ( level.teamVoteTime[cs_offset] && !(ent->client->mGameFlags & PSG_TEAMVOTED) )
		{//team vote is in progress and we haven't voted for it.  Vote for it instead of
			//for the normal vote.
			Cmd_TeamVote_f(ent);
			return;
		}
	}
	//[/AdminSys]

	if ( ent->client->sess.sessionTeam == TEAM_SPECTATOR ) {
		trap_SendServerCommand( ent-g_entities, va("print \"%s\n\"", G_GetStringEdString("MP_SVGAME", "NOVOTEASSPEC")) );
		return;
	}

	if ( !level.voteTime ) {
		trap_SendServerCommand( ent-g_entities, va("print \"%s\n\"", G_GetStringEdString("MP_SVGAME", "NOVOTEINPROG")) );
		return;
	}
	if ( ent->client->mGameFlags & PSG_VOTED ) {
		trap_SendServerCommand( ent-g_entities, va("print \"%s\n\"", G_GetStringEdString("MP_SVGAME", "VOTEALREADY")) );
		return;
	}
	if (g_gametype.integer != GT_DUEL &&
		g_gametype.integer != GT_POWERDUEL)
	{
		if ( ent->client->sess.sessionTeam == TEAM_SPECTATOR ) {
			trap_SendServerCommand( ent-g_entities, va("print \"%s\n\"", G_GetStringEdString("MP_SVGAME", "NOVOTEASSPEC")) );
			return;
		}
	}

	trap_SendServerCommand( ent-g_entities, va("print \"%s\n\"", G_GetStringEdString("MP_SVGAME", "PLVOTECAST")) );

	ent->client->mGameFlags |= PSG_VOTED;

	trap_Argv( 1, msg, sizeof( msg ) );

	if ( msg[0] == 'y' || msg[1] == 'Y' || msg[1] == '1' ) {
		level.voteYes++;
		trap_SetConfigstring( CS_VOTE_YES, va("%i", level.voteYes ) );
	} else {
		level.voteNo++;
		trap_SetConfigstring( CS_VOTE_NO, va("%i", level.voteNo ) );	
	}

	// a majority will be determined in CheckVote, which will also account
	// for players entering or leaving
}

/*
==================
Cmd_CallTeamVote_f
==================
*/
void Cmd_CallTeamVote_f( gentity_t *ent ) {
	//[AdminSys]
	//int		i, team, cs_offset;	
	int		i, targetClientNum=ENTITYNUM_NONE, team, cs_offset;
	//[/AdminSys]
	char	arg1[MAX_STRING_TOKENS];
	char	arg2[MAX_STRING_TOKENS];

	//[AdminSys]
	if ( g_gametype.integer < GT_TEAM )
	{
		trap_SendServerCommand( ent-g_entities, "print \"Cannot call a team vote in a non-team gametype!\n\"" );
		return;
	}
	//[/AdminSys]
	team = ent->client->sess.sessionTeam;
	if ( team == TEAM_RED )
		cs_offset = 0;
	else if ( team == TEAM_BLUE )
		cs_offset = 1;
	else
	//[AdminSys]
	{
		trap_SendServerCommand( ent-g_entities, "print \"Cannot call a team vote if not on a team!\n\"" );
		return;
	}
	

	//if ( !g_allowVote.integer ) {
	if ( !g_allowTeamVote.integer ) {
	//[/AdminSys]
		trap_SendServerCommand( ent-g_entities, va("print \"%s\n\"", G_GetStringEdString("MP_SVGAME", "NOVOTE")) );
		return;
	}

	if ( level.teamVoteTime[cs_offset] ) {
		trap_SendServerCommand( ent-g_entities, va("print \"%s\n\"", G_GetStringEdString("MP_SVGAME", "TEAMVOTEALREADY")) );
		return;
	}
	if ( ent->client->pers.teamVoteCount >= MAX_VOTE_COUNT ) {
		trap_SendServerCommand( ent-g_entities, va("print \"%s\n\"", G_GetStringEdString("MP_SVGAME", "MAXTEAMVOTES")) );
		return;
	}
	if ( ent->client->sess.sessionTeam == TEAM_SPECTATOR ) {
		trap_SendServerCommand( ent-g_entities, va("print \"%s\n\"", G_GetStringEdString("MP_SVGAME", "NOSPECVOTE")) );
		return;
	}

	// make sure it is a valid command to vote on
	trap_Argv( 1, arg1, sizeof( arg1 ) );
	arg2[0] = '\0';
	for ( i = 2; i < trap_Argc(); i++ ) {
		if (i > 2)
			strcat(arg2, " ");
		trap_Argv( i, &arg2[strlen(arg2)], sizeof( arg2 ) - strlen(arg2) );
	}

	if( strchr( arg1, ';' ) || strchr( arg2, ';' ) ) {
		trap_SendServerCommand( ent-g_entities, "print \"Invalid vote string.\n\"" );
		return;
	}

	//[AdminSys]
	//if ( !Q_stricmp( arg1, "leader" ) ) {
	//	char netname[MAX_NETNAME], leader[MAX_NETNAME];
	if ( !Q_stricmp( arg1, "leader" )
		|| !Q_stricmp( arg1, "kick" ) ) {
		char netname[MAX_NETNAME], target[MAX_NETNAME];

		if ( !arg2[0] ) {
			//i = ent->client->ps.clientNum;
			targetClientNum = ent->client->ps.clientNum;
		}
		else {
			// numeric values are just slot numbers
			for (i = 0; i < 3; i++) {
				if ( !arg2[i] || arg2[i] < '0' || arg2[i] > '9' )
					break;
			}
			if ( i >= 3 || !arg2[i]) {
				//i = atoi( arg2 );
				//if ( i < 0 || i >= level.maxclients ) {
				//	trap_SendServerCommand( ent-g_entities, va("print \"Bad client slot: %i\n\"", i) );
				targetClientNum = atoi( arg2 );
				if ( targetClientNum < 0 || targetClientNum >= level.maxclients ) {
					trap_SendServerCommand( ent-g_entities, va("print \"Bad client slot: %i\n\"", targetClientNum) );
					return;
				}

				//if ( !g_entities[i].inuse ) {
				//	trap_SendServerCommand( ent-g_entities, va("print \"Client %i is not active\n\"", i) );
				if ( !g_entities[targetClientNum].inuse ) {
					trap_SendServerCommand( ent-g_entities, va("print \"Client %i is not active\n\"", targetClientNum) );
					return;
				}
			}
			else {
				Q_strncpyz(target, arg2, sizeof(target));
				Q_CleanStr(target);
				//Q_strncpyz(leader, arg2, sizeof(leader));
				//Q_CleanStr(leader);
				for ( i = 0 ; i < level.maxclients ; i++ ) {
					if ( level.clients[i].pers.connected == CON_DISCONNECTED )
						continue;
					if (level.clients[i].sess.sessionTeam != team)
						continue;
					Q_strncpyz(netname, level.clients[i].pers.netname, sizeof(netname));
					Q_CleanStr(netname);
					//if ( !Q_stricmp(netname, leader) ) {
					if ( !Q_stricmp(netname, target) ) 
					{
						targetClientNum = i;
						break;
					}
				}
				if ( targetClientNum >= level.maxclients ) {
				//if ( i >= level.maxclients ) {
					trap_SendServerCommand( ent-g_entities, va("print \"%s is not a valid player on your team.\n\"", arg2) );
					return;
				}
			}
		}
		if ( targetClientNum >= MAX_CLIENTS )
		{//wtf?
			trap_SendServerCommand( ent-g_entities, va("print \"%s is not a valid player on your team.\n\"", arg2) );
			return;
		}
		if ( level.clients[targetClientNum].sess.sessionTeam != ent->client->sess.sessionTeam )
		{//can't call a team vote on someone not on your team!
			trap_SendServerCommand( ent-g_entities, va("print \"Cannot call a team vote on someone not on your team (%s).\n\"", level.clients[targetClientNum].pers.netname) );
			return;
		}
		//just use the client number
		Com_sprintf(arg2, sizeof(arg2), "%d", targetClientNum);
		//Com_sprintf(arg2, sizeof(arg2), "%d", i);
	} else {
		trap_SendServerCommand( ent-g_entities, "print \"Invalid vote string.\n\"" );
		trap_SendServerCommand( ent-g_entities, "print \"Team vote commands are: leader <player on your team> OR kick <player on your team>.\n\"" );
		//trap_SendServerCommand( ent-g_entities, "print \"Team vote commands are: leader <player>.\n\"" );
		return;
	}

	if ( !Q_stricmp( "kick", arg1 ) )
	{//use clientkick and number (so they can't change their name)
		Com_sprintf( level.teamVoteString[cs_offset], sizeof( level.teamVoteString[cs_offset] ), "clientkick %s", arg2 );
	}
	else
	{//just a number
		Com_sprintf( level.teamVoteString[cs_offset], sizeof( level.teamVoteString[cs_offset] ), "%s %s", arg1, arg2 );
	}
	//[/AdminSys]

	for ( i = 0 ; i < level.maxclients ; i++ ) {
		if ( level.clients[i].pers.connected == CON_DISCONNECTED )
			continue;
		if (level.clients[i].sess.sessionTeam == team)
			trap_SendServerCommand( i, va("print \"%s called a team vote.\n\"", ent->client->pers.netname ) );
	}

	// start the voting, the caller autoamtically votes yes
	level.teamVoteTime[cs_offset] = level.time;
	level.teamVoteYes[cs_offset] = 1;
	level.teamVoteNo[cs_offset] = 0;

	for ( i = 0 ; i < level.maxclients ; i++ ) {
		if (level.clients[i].sess.sessionTeam == team)
			level.clients[i].mGameFlags &= ~PSG_TEAMVOTED;
	}
	ent->client->mGameFlags |= PSG_TEAMVOTED;

	trap_SetConfigstring( CS_TEAMVOTE_TIME + cs_offset, va("%i", level.teamVoteTime[cs_offset] ) );
	trap_SetConfigstring( CS_TEAMVOTE_STRING + cs_offset, level.teamVoteString[cs_offset] );
	trap_SetConfigstring( CS_TEAMVOTE_YES + cs_offset, va("%i", level.teamVoteYes[cs_offset] ) );
	trap_SetConfigstring( CS_TEAMVOTE_NO + cs_offset, va("%i", level.teamVoteNo[cs_offset] ) );
}

/*
==================
Cmd_TeamVote_f
==================
*/
void Cmd_TeamVote_f( gentity_t *ent ) {
	int			team, cs_offset;
	char		msg[64];

	team = ent->client->sess.sessionTeam;
	if ( team == TEAM_RED )
		cs_offset = 0;
	else if ( team == TEAM_BLUE )
		cs_offset = 1;
	else
		return;

	if ( !level.teamVoteTime[cs_offset] ) {
		trap_SendServerCommand( ent-g_entities, va("print \"%s\n\"", G_GetStringEdString("MP_SVGAME", "NOTEAMVOTEINPROG")) );
		return;
	}
	if ( ent->client->mGameFlags & PSG_TEAMVOTED ) {
		trap_SendServerCommand( ent-g_entities, va("print \"%s\n\"", G_GetStringEdString("MP_SVGAME", "TEAMVOTEALREADYCAST")) );
		return;
	}
	if ( ent->client->sess.sessionTeam == TEAM_SPECTATOR ) {
		trap_SendServerCommand( ent-g_entities, va("print \"%s\n\"", G_GetStringEdString("MP_SVGAME", "NOVOTEASSPEC")) );
		return;
	}

	trap_SendServerCommand( ent-g_entities, va("print \"%s\n\"", G_GetStringEdString("MP_SVGAME", "PLTEAMVOTECAST")) );

	ent->client->mGameFlags |= PSG_TEAMVOTED;

	trap_Argv( 1, msg, sizeof( msg ) );

	if ( msg[0] == 'y' || msg[1] == 'Y' || msg[1] == '1' ) {
		level.teamVoteYes[cs_offset]++;
		trap_SetConfigstring( CS_TEAMVOTE_YES + cs_offset, va("%i", level.teamVoteYes[cs_offset] ) );
	} else {
		level.teamVoteNo[cs_offset]++;
		trap_SetConfigstring( CS_TEAMVOTE_NO + cs_offset, va("%i", level.teamVoteNo[cs_offset] ) );	
	}

	// a majority will be determined in TeamCheckVote, which will also account
	// for players entering or leaving
}


/*
=================
Cmd_SetViewpos_f
=================
*/
void Cmd_SetViewpos_f( gentity_t *ent ) {
	vec3_t		origin, angles;
	char		buffer[MAX_TOKEN_CHARS];
	int			i;

	if ( !g_cheats.integer ) {
		trap_SendServerCommand( ent-g_entities, va("print \"%s\n\"", G_GetStringEdString("MP_SVGAME", "NOCHEATS")));
		return;
	}
	if ( trap_Argc() != 5 ) {
		trap_SendServerCommand( ent-g_entities, va("print \"usage: setviewpos x y z yaw\n\""));
		return;
	}

	VectorClear( angles );
	for ( i = 0 ; i < 3 ; i++ ) {
		trap_Argv( i + 1, buffer, sizeof( buffer ) );
		origin[i] = atof( buffer );
	}

	trap_Argv( 4, buffer, sizeof( buffer ) );
	angles[YAW] = atof( buffer );

	TeleportPlayer( ent, origin, angles );
}

void Admin_Teleport( gentity_t *ent ) {
	vec3_t		origin;
	char		buffer[MAX_TOKEN_CHARS];
	int			i;

	if ( trap_Argc() != 4 ) {
		trap_SendServerCommand( ent-g_entities, va("print \"usage: tele (X) (Y) (Z)\ntype in /origin OR /origin (name) to find out (X) (Y) (Z)\n\""));
		return;
	}

	for ( i = 0 ; i < 3 ; i++ ) {
		trap_Argv( i + 1, buffer, sizeof( buffer ) );
		origin[i] = atof( buffer );
	}

	TeleportPlayer( ent, origin, ent->client->ps.viewangles );
	//trap_SendServerCommand( -1, va("cp \"%s^7\n%s\n\"", ent->client->pers.netname, roar_teleport_saying.string ) );  
}

/*
=================
Cmd_Stats_f
=================
*/
void Cmd_Stats_f( gentity_t *ent ) {
/*
	int max, n, i;

	max = trap_AAS_PointReachabilityAreaIndex( NULL );

	n = 0;
	for ( i = 0; i < max; i++ ) {
		if ( ent->client->areabits[i >> 3] & (1 << (i & 7)) )
			n++;
	}

	//trap_SendServerCommand( ent-g_entities, va("print \"visited %d of %d areas\n\"", n, max));
	trap_SendServerCommand( ent-g_entities, va("print \"%d%% level coverage\n\"", n * 100 / max));
*/
}

//[BugFix38]
void G_LeaveVehicle( gentity_t* ent, qboolean ConCheck ) {

	if (ent->client->ps.m_iVehicleNum)
	{ //tell it I'm getting off
		gentity_t *veh = &g_entities[ent->client->ps.m_iVehicleNum];

		if (veh->inuse && veh->client && veh->m_pVehicle)
		{
			if ( ConCheck ) { // check connection
				int pCon = ent->client->pers.connected;
				ent->client->pers.connected = 0;
				veh->m_pVehicle->m_pVehicleInfo->Eject(veh->m_pVehicle, (bgEntity_t *)ent, qtrue);
				ent->client->pers.connected = pCon;
			} else { // or not.
				veh->m_pVehicle->m_pVehicleInfo->Eject(veh->m_pVehicle, (bgEntity_t *)ent, qtrue);
			}
		}
	}

	ent->client->ps.m_iVehicleNum = 0;
}
//[/BugFix38]

int G_ItemUsable(playerState_t *ps, int forcedUse)
{
	vec3_t fwd, fwdorg, dest, pos;
	vec3_t yawonly;
	vec3_t mins, maxs;
	vec3_t trtest;
	trace_t tr;

	if (ps->m_iVehicleNum)
	{
		return 0;
	}
	
	if (ps->pm_flags & PMF_USE_ITEM_HELD)
	{ //force to let go first
		return 0;
	}

	if (!forcedUse)
	{
		forcedUse = bg_itemlist[ps->stats[STAT_HOLDABLE_ITEM]].giTag;
	}

	if (!BG_IsItemSelectable(ps, forcedUse))
	{
		return 0;
	}

	switch (forcedUse)
	{
	case HI_MEDPAC:
	case HI_MEDPAC_BIG:
		if (ps->stats[STAT_HEALTH] >= ps->stats[STAT_MAX_HEALTH])
		{
			return 0;
		}

		if (ps->stats[STAT_HEALTH] <= 0)
		{
			return 0;
		}

		return 1;
	case HI_SEEKER:
		if (ps->eFlags & EF_SEEKERDRONE)
		{
			G_AddEvent(&g_entities[ps->clientNum], EV_ITEMUSEFAIL, SEEKER_ALREADYDEPLOYED);
			return 0;
		}

		return 1;
	case HI_SENTRY_GUN:
		//[SentryGun]
		/*
		if (ps->fd.sentryDeployed)
		{
			G_AddEvent(&g_entities[ps->clientNum], EV_ITEMUSEFAIL, SENTRY_ALREADYPLACED);
			return 0;
		}
		*/
		//[/SentryGun]

		yawonly[ROLL] = 0;
		yawonly[PITCH] = 0;
		yawonly[YAW] = ps->viewangles[YAW];

		VectorSet( mins, -8, -8, 0 );
		VectorSet( maxs, 8, 8, 24 );

		AngleVectors(yawonly, fwd, NULL, NULL);

		fwdorg[0] = ps->origin[0] + fwd[0]*64;
		fwdorg[1] = ps->origin[1] + fwd[1]*64;
		fwdorg[2] = ps->origin[2] + fwd[2]*64;

		trtest[0] = fwdorg[0] + fwd[0]*16;
		trtest[1] = fwdorg[1] + fwd[1]*16;
		trtest[2] = fwdorg[2] + fwd[2]*16;

		trap_Trace(&tr, ps->origin, mins, maxs, trtest, ps->clientNum, MASK_PLAYERSOLID);

		if ((tr.fraction != 1 && tr.entityNum != ps->clientNum) || tr.startsolid || tr.allsolid)
		{
			G_AddEvent(&g_entities[ps->clientNum], EV_ITEMUSEFAIL, SENTRY_NOROOM);
			return 0;
		}

		return 1;
	case HI_SHIELD:
		mins[0] = -8;
		mins[1] = -8;
		mins[2] = 0;

		maxs[0] = 8;
		maxs[1] = 8;
		maxs[2] = 8;

		AngleVectors (ps->viewangles, fwd, NULL, NULL);
		fwd[2] = 0;
		VectorMA(ps->origin, 64, fwd, dest);
		trap_Trace(&tr, ps->origin, mins, maxs, dest, ps->clientNum, MASK_SHOT );
		if (tr.fraction > 0.9 && !tr.startsolid && !tr.allsolid)
		{
			VectorCopy(tr.endpos, pos);
			VectorSet( dest, pos[0], pos[1], pos[2] - 4096 );
			trap_Trace( &tr, pos, mins, maxs, dest, ps->clientNum, MASK_SOLID );
			if ( !tr.startsolid && !tr.allsolid )
			{
				return 1;
			}
		}
		G_AddEvent(&g_entities[ps->clientNum], EV_ITEMUSEFAIL, SHIELD_NOROOM);
		return 0;
	case HI_JETPACK: //do something?
		return 1;
	case HI_HEALTHDISP:
		return 1;
	case HI_AMMODISP:
		return 1;
	case HI_EWEB:
		return 1;
	case HI_CLOAK:
		return 1;
	default:
		return 1;
	}
}

void saberKnockDown(gentity_t *saberent, gentity_t *saberOwner, gentity_t *other);

void Cmd_ToggleSaber_f(gentity_t *ent)
{
	//[TAUNTFIX]
	if (ent->client->ps.weapon != WP_SABER) {
		return;
	}

	if (level.intermissiontime) { // not during intermission
		return;
	}

	if (ent->client->sess.sessionTeam == TEAM_SPECTATOR ) { // not when spec
		return;
	}

	if (ent->client->tempSpectate >= level.time ) { // not when tempSpec
		return;
	}

	if (ent->client->ps.emplacedIndex) { //on an emplaced gun
		return;
	}

	if (ent->client->ps.m_iVehicleNum) { //in a vehicle like at-st
		gentity_t *veh = &g_entities[ent->client->ps.m_iVehicleNum];

		if ( veh->m_pVehicle && veh->m_pVehicle->m_pVehicleInfo->type == VH_WALKER )
			return;

		if ( veh->m_pVehicle && veh->m_pVehicle->m_pVehicleInfo->type == VH_FIGHTER )
			return;
	}
	//[/TAUNTFIX]

	if (ent->client->ps.fd.forceGripCripple)
	{ //if they are being gripped, don't let them unholster their saber
		if (ent->client->ps.saberHolstered)
		{
			return;
		}
	}

	if (ent->client->ps.saberInFlight)
	{
		//[SaberThrowSys]
		if(!ent->client->ps.saberEntityNum)
		{//our saber is dead, Try pulling it back.
			ent->client->ps.forceHandExtend = HANDEXTEND_SABERPULL;
			ent->client->ps.forceHandExtendTime = level.time + 300;			
		}
		//Can't use the Force to turn off the saber in midair anymore 
		/* basejka code
		if (ent->client->ps.saberEntityNum)
		{ //turn it off in midair
			saberKnockDown(&g_entities[ent->client->ps.saberEntityNum], ent, ent);
		}
		*/
		//[/SaberThrowSys]
		return;
	}

	if (ent->client->ps.forceHandExtend != HANDEXTEND_NONE)
	{
		return;
	}

	//[TAUNTFIX]
	/* ensiform - moved this up to the top of function
	if (ent->client->ps.weapon != WP_SABER)
	{
		return;
	}
	*/
	//[/TAUNTFIX]

//	if (ent->client->ps.duelInProgress && !ent->client->ps.saberHolstered)
//	{
//		return;
//	}

	if (ent->client->ps.duelTime >= level.time)
	{
		return;
	}

	if (ent->client->ps.saberLockTime >= level.time)
	{
		return;
	}

	if (ent->client && ent->client->ps.weaponTime < 1)
	{
		if (ent->client->ps.saberHolstered == 2)
		{
			ent->client->ps.saberHolstered = 0;

			if (ent->client->saber[0].soundOn)
			{
				G_Sound(ent, CHAN_AUTO, ent->client->saber[0].soundOn);
			}
			if (ent->client->saber[1].soundOn)
			{
				G_Sound(ent, CHAN_AUTO, ent->client->saber[1].soundOn);
			}
		}
		else
		{
			ent->client->ps.saberHolstered = 2;
			if (ent->client->saber[0].soundOff)
			{
				G_Sound(ent, CHAN_AUTO, ent->client->saber[0].soundOff);
			}
			if (ent->client->saber[1].soundOff &&
				ent->client->saber[1].model[0])
			{
				G_Sound(ent, CHAN_AUTO, ent->client->saber[1].soundOff);
			}
			//prevent anything from being done for 400ms after holster
			ent->client->ps.weaponTime = 400;
		}
	}
}


qboolean G_ValidSaberStyle(gentity_t *ent, int saberStyle)
{	
	if(saberStyle == SS_MEDIUM)
	{//SS_YELLOW is the default and always valid
		return qtrue;
	}
	
	//otherwise, check to see if the player has the skill to use this style
	switch (saberStyle)
	{
		case SS_FAST:
			if(ent->client->skillLevel[SK_BLUESTYLE] > 0)
			{
				return qtrue;
			}
			break;
		default:
			if(ent->client->skillLevel[saberStyle+SK_REDSTYLE-SS_STRONG] > 0)
			{//valid style
				return qtrue;
			}
			break;
	};

	return qfalse;
}

extern vmCvar_t		d_saberStanceDebug;

extern qboolean WP_SaberCanTurnOffSomeBlades( saberInfo_t *saber );
void Cmd_SaberAttackCycle_f(gentity_t *ent)
{
	int selectLevel = 0;
	qboolean usingSiegeStyle = qfalse;
	
	//[BugFix15]
	// MJN - Saber Cycle Fix - Thanks Wudan!!
	if ( ent->client->ps.weapon != WP_SABER )
	{
        return;
	}

	/*
	if ( !ent || !ent->client )
	{
		return;
	}
	*/
	/*
	if (ent->client->ps.weaponTime > 0)
	{ //no switching attack level when busy
		return;
	}
	*/	
	//[/BugFix15]

	//[TAUNTFIX]
	if (ent->client->sess.sessionTeam == TEAM_SPECTATOR)
	{ //not for spectators
		return;
	}

	if (ent->client->tempSpectate >= level.time)
	{ //not for spectators
		return;
	}

	if (level.intermissiontime)
	{ //not during intermission
		return;
	}

	if (ent->client->ps.m_iVehicleNum)
	{ //in a vehicle like at-st
		gentity_t *veh = &g_entities[ent->client->ps.m_iVehicleNum];

		if ( veh->m_pVehicle && veh->m_pVehicle->m_pVehicleInfo->type == VH_WALKER )
			return;

		if ( veh->m_pVehicle && veh->m_pVehicle->m_pVehicleInfo->type == VH_FIGHTER )
			return;
	}
	//[/TAUNTFIX]

	/* basejka code
	if (ent->client->saber[0].model[0] && ent->client->saber[1].model[0])
	{ //no cycling for akimbo
		if ( WP_SaberCanTurnOffSomeBlades( &ent->client->saber[1] ) )
		{//can turn second saber off 
			//[SaberThrowSys]
			//can't toggle the other saber while the other saber is in flight.
			if ( ent->client->ps.saberHolstered == 1 && !ent->client->ps.saberInFlight)
			//if ( ent->client->ps.saberHolstered == 1 )
			//[/SaberThrowSys]
			{//have one holstered
				//unholster it
				G_Sound(ent, CHAN_AUTO, ent->client->saber[1].soundOn);
				ent->client->ps.saberHolstered = 0;
				//g_active should take care of this, but...
				ent->client->ps.fd.saberAnimLevel = SS_DUAL;
			}
			else if ( ent->client->ps.saberHolstered == 0 )
			{//have none holstered
				if ( (ent->client->saber[1].saberFlags2&SFL2_NO_MANUAL_DEACTIVATE) )
				{//can't turn it off manually
				}
				else if ( ent->client->saber[1].bladeStyle2Start > 0
					&& (ent->client->saber[1].saberFlags2&SFL2_NO_MANUAL_DEACTIVATE2) )
				{//can't turn it off manually
				}
				else
				{
					//turn it off
					G_Sound(ent, CHAN_AUTO, ent->client->saber[1].soundOff);
					ent->client->ps.saberHolstered = 1;
					//g_active should take care of this, but...
					ent->client->ps.fd.saberAnimLevel = SS_FAST;
				}
			}

			if (d_saberStanceDebug.integer)
			{
				trap_SendServerCommand( ent-g_entities, va("print \"SABERSTANCEDEBUG: Attempted to toggle dual saber blade.\n\"") );
			}
			return;
		}
	}
	else if (ent->client->saber[0].numBlades > 1
		&& WP_SaberCanTurnOffSomeBlades( &ent->client->saber[0] ) )
	{ //use staff stance then.
		if ( ent->client->ps.saberHolstered == 1 )
		{//second blade off
			if ( ent->client->ps.saberInFlight )
			{//can't turn second blade back on if it's in the air, you naughty boy!
				if (d_saberStanceDebug.integer)
				{
					trap_SendServerCommand( ent-g_entities, va("print \"SABERSTANCEDEBUG: Attempted to toggle staff blade in air.\n\"") );
				}
				return;
			}
			//turn it on
			G_Sound(ent, CHAN_AUTO, ent->client->saber[0].soundOn);
			ent->client->ps.saberHolstered = 0;
			//g_active should take care of this, but...
			if ( ent->client->saber[0].stylesForbidden )
			{//have a style we have to use
				WP_UseFirstValidSaberStyle( &ent->client->saber[0], &ent->client->saber[1], ent->client->ps.saberHolstered, &selectLevel );
				if ( ent->client->ps.weaponTime <= 0 )
				{ //not busy, set it now
					ent->client->ps.fd.saberAnimLevel = selectLevel;
				}
				else
				{ //can't set it now or we might cause unexpected chaining, so queue it
					ent->client->saberCycleQueue = selectLevel;
				}
			}
		}
		else if ( ent->client->ps.saberHolstered == 0 )
		{//both blades on
			if ( (ent->client->saber[0].saberFlags2&SFL2_NO_MANUAL_DEACTIVATE) )
			{//can't turn it off manually
			}
			else if ( ent->client->saber[0].bladeStyle2Start > 0
				&& (ent->client->saber[0].saberFlags2&SFL2_NO_MANUAL_DEACTIVATE2) )
			{//can't turn it off manually
			}
			else
			{
				//turn second one off
				G_Sound(ent, CHAN_AUTO, ent->client->saber[0].soundOff);
				ent->client->ps.saberHolstered = 1;
				//g_active should take care of this, but...
				if ( ent->client->saber[0].singleBladeStyle != SS_NONE )
				{
					if ( ent->client->ps.weaponTime <= 0 )
					{ //not busy, set it now
						ent->client->ps.fd.saberAnimLevel = ent->client->saber[0].singleBladeStyle;
					}
					else
					{ //can't set it now or we might cause unexpected chaining, so queue it
						ent->client->saberCycleQueue = ent->client->saber[0].singleBladeStyle;
					}
				}
			}
		}
		if (d_saberStanceDebug.integer)
		{
			trap_SendServerCommand( ent-g_entities, va("print \"SABERSTANCEDEBUG: Attempted to toggle staff blade.\n\"") );
		}
		return;
	}
	*/

	if (ent->client->saberCycleQueue)
	{ //resume off of the queue if we haven't gotten a chance to update it yet
		selectLevel = ent->client->saberCycleQueue;
	}
	else
	{
		selectLevel = ent->client->ps.fd.saberAnimLevel;
	}

	if (g_gametype.integer == GT_SIEGE &&
		ent->client->siegeClass != -1 &&
		bgSiegeClasses[ent->client->siegeClass].saberStance)
	{ //we have a flag of useable stances so cycle through it instead
		int i = selectLevel+1;

		usingSiegeStyle = qtrue;

		while (i != selectLevel)
		{ //cycle around upward til we hit the next style or end up back on this one
			if (i >= SS_NUM_SABER_STYLES)
			{ //loop back around to the first valid
				i = SS_FAST;
			}

			if (bgSiegeClasses[ent->client->siegeClass].saberStance & (1 << i))
			{ //we can use this one, select it and break out.
				selectLevel = i;
				break;
			}
			i++;
		}

		if (d_saberStanceDebug.integer)
		{
			trap_SendServerCommand( ent-g_entities, va("print \"SABERSTANCEDEBUG: Attempted to cycle given class stance.\n\"") );
		}
	}
	else
	{//normal style selection
		int attempts;
		selectLevel++;

		for(attempts = 0; attempts < SS_STAFF; attempts++)
		{
			if(selectLevel > SS_STAFF)
			{
				selectLevel = SS_FAST;
			}

			if(G_ValidSaberStyle(ent, selectLevel))
			{
				break;
			}

			//no dice, keep looking
			selectLevel++;
		}

		//handle saber activation/deactivation based on the style transition
		if (ent->client->saber[0].model[0] && ent->client->saber[1].model[0]
			&& WP_SaberCanTurnOffSomeBlades( &ent->client->saber[1] ) )
		{//using dual sabers
			if(selectLevel != SS_DUAL && ent->client->ps.saberHolstered == 0 && !ent->client->ps.saberInFlight)
			{//not using dual style, turn off the other blade
				G_Sound(ent, CHAN_AUTO, ent->client->saber[1].soundOff);
				ent->client->ps.saberHolstered = 1;
			}
			else if(selectLevel == SS_DUAL && ent->client->ps.saberHolstered == 1 && !ent->client->ps.saberInFlight)
			{
				G_Sound(ent, CHAN_AUTO, ent->client->saber[1].soundOn);
				ent->client->ps.saberHolstered = 0;
			}
		}
		else if (ent->client->saber[0].numBlades > 1
			&& WP_SaberCanTurnOffSomeBlades( &ent->client->saber[0] ) )
		{ //use staff stance then.
			if(selectLevel != SS_STAFF && ent->client->ps.saberHolstered == 0 && !ent->client->ps.saberInFlight)
			{
				G_Sound(ent, CHAN_AUTO, ent->client->saber[0].soundOff);
				ent->client->ps.saberHolstered = 1;
			}
			else if(selectLevel == SS_STAFF && ent->client->ps.saberHolstered == 1 && !ent->client->ps.saberInFlight)
			{
					G_Sound(ent, CHAN_AUTO, ent->client->saber[0].soundOn);
					ent->client->ps.saberHolstered = 0;
			}
		}
		/*
		//[HiddenStances]
		if ( selectLevel > ent->client->ps.fd.forcePowerLevel[FP_SABER_OFFENSE] 
		&& ent->client->ps.fd.forcePowerLevel[FP_SABER_OFFENSE] < FORCE_LEVEL_3
			|| selectLevel > SS_TAVION)
		//if ( selectLevel > ent->client->ps.fd.forcePowerLevel[FP_SABER_OFFENSE] )
		//[/HiddenStances]
		{
			selectLevel = FORCE_LEVEL_1;
		}
		*/
		if (d_saberStanceDebug.integer)
		{
			trap_SendServerCommand( ent-g_entities, va("print \"SABERSTANCEDEBUG: Attempted to cycle stance normally.\n\"") );
		}
	}
/*
#ifndef FINAL_BUILD
	switch ( selectLevel )
	{
	case FORCE_LEVEL_1:
		trap_SendServerCommand( ent-g_entities, va("print \"Lightsaber Combat Style: %sfast\n\"", S_COLOR_BLUE) );
		break;
	case FORCE_LEVEL_2:
		trap_SendServerCommand( ent-g_entities, va("print \"Lightsaber Combat Style: %smedium\n\"", S_COLOR_YELLOW) );
		break;
	case FORCE_LEVEL_3:
		trap_SendServerCommand( ent-g_entities, va("print \"Lightsaber Combat Style: %sstrong\n\"", S_COLOR_RED) );
		break;
	}
#endif
*/
	/*
	if ( !usingSiegeStyle )
	{
		//make sure it's valid, change it if not
		WP_UseFirstValidSaberStyle( &ent->client->saber[0], &ent->client->saber[1], ent->client->ps.saberHolstered, &selectLevel );
	}
	*/

	if (ent->client->ps.weaponTime <= 0)
	{ //not busy, set it now
		ent->client->ps.fd.saberAnimLevel = selectLevel;
	}
	else
	{ //can't set it now or we might cause unexpected chaining, so queue it
		ent->client->saberCycleQueue = selectLevel;
	}
}

qboolean G_OtherPlayersDueling(void)
{
	int i = 0;
	gentity_t *ent;

	while (i < MAX_CLIENTS)
	{
		ent = &g_entities[i];

		if (ent && ent->inuse && ent->client && ent->client->ps.duelInProgress)
		{
			return qtrue;
		}
		i++;
	}

	return qfalse;
}


//[DuelSys]
extern vmCvar_t g_multiDuel;
//[/DuelSys]
//[TABBots]
extern void TAB_BotSaberDuelChallenged(gentity_t *bot, gentity_t *player);
extern int FindBotType(int clientNum);
//[/TABBots]

void Cmd_EngageDuel_f(gentity_t *ent)
{
	trace_t tr;
	vec3_t forward, fwdOrg;

	if (!g_privateDuel.integer)
	{
		return;
	}

	if (g_gametype.integer == GT_DUEL || g_gametype.integer == GT_POWERDUEL)
	{ //rather pointless in this mode..
		trap_SendServerCommand( ent-g_entities, va("print \"%s\n\"", G_GetStringEdString("MP_SVGAME", "NODUEL_GAMETYPE")) );
		return;
	}

	//[DuelSys] 
	//Allow dueling in team games.
	/* basejka code
	//if (g_gametype.integer >= GT_TEAM && g_gametype.integer != GT_SIEGE)
	if (g_gametype.integer >= GT_TEAM)
	{ //no private dueling in team modes
		trap_SendServerCommand( ent-g_entities, va("print \"%s\n\"", G_GetStringEdString("MP_SVGAME", "NODUEL_GAMETYPE")) );
		return;
	}
	*/
	//[/DuelSys]

	if (ent->client->ps.duelTime >= level.time)
	{
		return;
	}

	if (ent->client->ps.weapon != WP_SABER)
	{
		return;
	}

	/*
	if (!ent->client->ps.saberHolstered)
	{ //must have saber holstered at the start of the duel
		return;
	}
	*/
	//NOTE: No longer doing this..

	if (ent->client->ps.saberInFlight)
	{
		return;
	}

	if (ent->client->ps.duelInProgress)
	{
		return;
	}

   if (ent->client->pers.amdemigod)
	{
		return;
	}

	if (ent->client->pers.amfreeze)
	{
		return;
	}
	// New: Don't let a player duel if he just did and hasn't waited 10 seconds yet (note: If someone challenges him, his duel timer will reset so he can accept)
	//[DuelSys]
	// Update - MJN - This uses the new duelTimer cvar to get time, in seconds, before next duel is allowed.
	//[/DuelSys]
	if (ent->client->ps.fd.privateDuelTime > level.time)
	{
		trap_SendServerCommand( ent-g_entities, va("print \"%s\n\"", G_GetStringEdString("MP_SVGAME", "CANTDUEL_JUSTDID")) );
		return;
	}
	//[DuelSys]
	// MJN - cvar g_multiDuel allows more than 1 private duel at a time.
	if (!g_multiDuel.integer && G_OtherPlayersDueling())
	//if (G_OtherPlayersDueling())
	//[/DuelSys]
	{
		trap_SendServerCommand( ent-g_entities, va("print \"%s\n\"", G_GetStringEdString("MP_SVGAME", "CANTDUEL_BUSY")) );
		return;
	}

	AngleVectors( ent->client->ps.viewangles, forward, NULL, NULL );

	fwdOrg[0] = ent->client->ps.origin[0] + forward[0]*256;
	fwdOrg[1] = ent->client->ps.origin[1] + forward[1]*256;
	fwdOrg[2] = (ent->client->ps.origin[2]+ent->client->ps.viewheight) + forward[2]*256;

	trap_Trace(&tr, ent->client->ps.origin, NULL, NULL, fwdOrg, ent->s.number, MASK_PLAYERSOLID);

	if (tr.fraction != 1 && tr.entityNum < MAX_CLIENTS)
	{
		gentity_t *challenged = &g_entities[tr.entityNum];

		if (!challenged || !challenged->client || !challenged->inuse ||
			challenged->health < 1 || challenged->client->ps.stats[STAT_HEALTH] < 1 ||
			challenged->client->ps.weapon != WP_SABER || challenged->client->ps.duelInProgress ||
			challenged->client->ps.saberInFlight)
		{
			return;
		}
		//[DuelSys]
		// MJN - added friendly fire check. Allows private duels in team games where friendly fire is on.
		if (!g_friendlyFire.integer && (g_gametype.integer >= GT_TEAM && OnSameTeam(ent, challenged)))
		//if (g_gametype.integer >= GT_TEAM && OnSameTeam(ent, challenged))
		//[/DuelSys]
		{
			return;
		}
  
		if (challenged->client->ps.duelIndex == ent->s.number && challenged->client->ps.duelTime >= level.time)
		{//racc - our duel target has already challenged us, start the duel.
			//[DuelSys]
			// MJN - added ^7 to clear the color on following text
			trap_SendServerCommand( /*challenged-g_entities*/-1, va("print \"%s ^7%s %s!\n\"", challenged->client->pers.netname, G_GetStringEdString("MP_SVGAME", "PLDUELACCEPT"), ent->client->pers.netname) );
			//trap_SendServerCommand( /*challenged-g_entities*/-1, va("print \"%s %s %s!\n\"", challenged->client->pers.netname, G_GetStringEdString("MP_SVGAME", "PLDUELACCEPT"), ent->client->pers.netname) );
			//[/DuelSys]

			ent->client->ps.duelInProgress = qtrue;
			challenged->client->ps.duelInProgress = qtrue;

			ent->client->ps.duelTime = level.time + 2000;
			challenged->client->ps.duelTime = level.time + 2000;

			G_AddEvent(ent, EV_PRIVATE_DUEL, 1);
			G_AddEvent(challenged, EV_PRIVATE_DUEL, 1);

			//Holster their sabers now, until the duel starts (then they'll get auto-turned on to look cool)

			if (!ent->client->ps.saberHolstered)
			{
				if (ent->client->saber[0].soundOff)
				{
					G_Sound(ent, CHAN_AUTO, ent->client->saber[0].soundOff);
				}
				if (ent->client->saber[1].soundOff &&
					ent->client->saber[1].model[0])
				{
					G_Sound(ent, CHAN_AUTO, ent->client->saber[1].soundOff);
				}
				ent->client->ps.weaponTime = 400;
				ent->client->ps.saberHolstered = 2;
			}
			if (!challenged->client->ps.saberHolstered)
			{
				if (challenged->client->saber[0].soundOff)
				{
					G_Sound(challenged, CHAN_AUTO, challenged->client->saber[0].soundOff);
				}
				if (challenged->client->saber[1].soundOff &&
					challenged->client->saber[1].model[0])
				{
					G_Sound(challenged, CHAN_AUTO, challenged->client->saber[1].soundOff);
				}
				challenged->client->ps.weaponTime = 400;
				challenged->client->ps.saberHolstered = 2;
			}
		}
		else
		{
			//Print the message that a player has been challenged in private, only announce the actual duel initiation in private
			//[DuelSys]
			// MJN - added "\n ^7" to properly align text on screen
			trap_SendServerCommand( challenged-g_entities, va("cp \"%s\n ^7%s\n\"", ent->client->pers.netname, G_GetStringEdString("MP_SVGAME", "PLDUELCHALLENGE")) );
			trap_SendServerCommand( ent-g_entities, va("cp \"%s\n ^7%s\n\"", G_GetStringEdString("MP_SVGAME", "PLDUELCHALLENGED"), challenged->client->pers.netname) );
			//trap_SendServerCommand( challenged-g_entities, va("cp \"%s %s\n\"", ent->client->pers.netname, G_GetStringEdString("MP_SVGAME", "PLDUELCHALLENGE")) );
			//trap_SendServerCommand( ent-g_entities, va("cp \"%s %s\n\"", G_GetStringEdString("MP_SVGAME", "PLDUELCHALLENGED"), challenged->client->pers.netname) );
			//[/DuelSys]
		}

		challenged->client->ps.fd.privateDuelTime = 0; //reset the timer in case this player just got out of a duel. He should still be able to accept the challenge.

		//[TABBots]
		if((challenged->r.svFlags & SVF_BOT) && FindBotType(challenged->s.number) == BOT_TAB)
		{//we just tried to challenge a TABBot, check to see if it's wishes to go for it.			
			TAB_BotSaberDuelChallenged(challenged, ent);
		}
		//[/TABBots]

		ent->client->ps.forceHandExtend = HANDEXTEND_DUELCHALLENGE;
		ent->client->ps.forceHandExtendTime = level.time + 1000;

		ent->client->ps.duelIndex = challenged->s.number;
		ent->client->ps.duelTime = level.time + 5000;
	}
}

#ifndef FINAL_BUILD
extern stringID_table_t animTable[MAX_ANIMATIONS+1];

void Cmd_DebugSetSaberMove_f(gentity_t *self)
{
	int argNum = trap_Argc();
	char arg[MAX_STRING_CHARS];

	if (argNum < 2)
	{
		return;
	}

	trap_Argv( 1, arg, sizeof( arg ) );

	if (!arg[0])
	{
		return;
	}

	self->client->ps.saberMove = atoi(arg);
	self->client->ps.saberBlocked = BLOCKED_BOUNCE_MOVE;

	if (self->client->ps.saberMove >= LS_MOVE_MAX)
	{
		self->client->ps.saberMove = LS_MOVE_MAX-1;
	}

	Com_Printf("Anim for move: %s\n", animTable[saberMoveData[self->client->ps.saberMove].animToUse].name);
}


//[SaberSys]
void Cmd_DebugSetSaberBlock_f(gentity_t *self)
{//This is a simple debugging function for debugging the saberblocked code.
	int argNum = trap_Argc();
	char arg[MAX_STRING_CHARS];

	if (argNum < 2)
	{
		return;
	}

	trap_Argv( 1, arg, sizeof( arg ) );

	if (!arg[0])
	{
		return;
	}

	//self->client->ps.saberMove = atoi(arg);
	//self->client->ps.saberBlocked = BLOCKED_BOUNCE_MOVE;
	self->client->ps.saberBlocked = atoi(arg);

	if (self->client->ps.saberBlocked > BLOCKED_TOP_PROJ)
	{
		self->client->ps.saberBlocked = BLOCKED_TOP_PROJ;
	}
}
//[/SaberSys]


void Cmd_DebugSetBodyAnim_f(gentity_t *self, int flags)
{
	int argNum = trap_Argc();
	char arg[MAX_STRING_CHARS];
	int i = 0;

	if (argNum < 2)
	{
		return;
	}

	trap_Argv( 1, arg, sizeof( arg ) );

	if (!arg[0])
	{
		return;
	}

	while (i < MAX_ANIMATIONS)
	{
		if (!Q_stricmp(arg, animTable[i].name))
		{
			break;
		}
		i++;
	}

	if (i == MAX_ANIMATIONS)
	{
		Com_Printf("Animation '%s' does not exist\n", arg);
		return;
	}

	G_SetAnim(self, NULL, SETANIM_BOTH, i, flags, 0);

	Com_Printf("Set body anim to %s\n", arg);
}
#endif

void StandardSetBodyAnim(gentity_t *self, int anim, int flags)
{
	G_SetAnim(self, NULL, SETANIM_BOTH, anim, flags, 0);
}

void DismembermentTest(gentity_t *self);

void Bot_SetForcedMovement(int bot, int forward, int right, int up);

#ifndef FINAL_BUILD
extern void DismembermentByNum(gentity_t *self, int num);
extern void G_SetVehDamageFlags( gentity_t *veh, int shipSurf, int damageLevel );
#endif

static int G_ClientNumFromNetname(char *name)
{
	int i = 0;
	gentity_t *ent;

	while (i < MAX_CLIENTS)
	{
		ent = &g_entities[i];

		if (ent->inuse && ent->client &&
			!Q_stricmp(ent->client->pers.netname, name))
		{
			return ent->s.number;
		}
		i++;
	}

	return -1;
}

qboolean TryGrapple(gentity_t *ent)
{
	if (ent->client->ps.weaponTime > 0)
	{ //weapon busy
		return qfalse;
	}
	if (ent->client->ps.forceHandExtend != HANDEXTEND_NONE)
	{ //force power or knockdown or something
		return qfalse;
	}
	if (ent->client->grappleState)
	{ //already grappling? but weapontime should be > 0 then..
		return qfalse;
	}

	if (ent->client->ps.weapon != WP_SABER && ent->client->ps.weapon != WP_MELEE)
	{
		return qfalse;
	}

	if (ent->client->ps.weapon == WP_SABER && !ent->client->ps.saberHolstered)
	{
		Cmd_ToggleSaber_f(ent);
		if (!ent->client->ps.saberHolstered)
		{ //must have saber holstered
			return qfalse;
		}
	}

	//G_SetAnim(ent, &ent->client->pers.cmd, SETANIM_BOTH, BOTH_KYLE_PA_1, SETANIM_FLAG_OVERRIDE|SETANIM_FLAG_HOLD, 0);
	G_SetAnim(ent, &ent->client->pers.cmd, SETANIM_BOTH, BOTH_KYLE_GRAB, SETANIM_FLAG_OVERRIDE|SETANIM_FLAG_HOLD, 0);
	if (ent->client->ps.torsoAnim == BOTH_KYLE_GRAB)
	{ //providing the anim set succeeded..
		ent->client->ps.torsoTimer += 500; //make the hand stick out a little longer than it normally would
		if (ent->client->ps.legsAnim == ent->client->ps.torsoAnim)
		{
			ent->client->ps.legsTimer = ent->client->ps.torsoTimer;
		}
		ent->client->ps.weaponTime = ent->client->ps.torsoTimer;

		//[BugFix35]
		ent->client->dangerTime = level.time;
		//[/BugFix35]
		return qtrue;
	}

	return qfalse;
}

//RoAR mod BEGIN
//#ifndef FINAL_BUILD
qboolean saberKnockOutOfHand(gentity_t *saberent, gentity_t *saberOwner, vec3_t velocity);
//#endif

qboolean ScalePlayer( gentity_t *self, int scale );

//ADMIN BITRATES HAVE ARRIVED!!!!
typedef enum
{
	A_ADMINTELE = 0,
	A_FREEZE,
	A_SILENCE,
	A_PROTECT,
	A_ADMINBAN,
	A_ADMINKICK,
	A_NPC,
	A_INSULTSILENCE,
	A_TERMINATOR,
	A_DEMIGOD,
	A_ADMINSIT,
	A_SCALE,
	A_SPLAT,
	A_SLAY,
	A_GRANTADMIN,
	A_CHANGEMAP,
	A_EMPOWER,
	A_RENAME,
	A_LOCKNAME,
	A_CSPRINT,
	A_FORCETEAM,
	A_RENAMECLAN,
	A_HUMAN,
	A_WEATHER,
	A_PLACE,
	A_PUNISH,
	A_SLEEP,
	A_SLAP,
	A_LOCKTEAM
} admin_type_t;
//EMOTE BITRATES HAVE ARRIVED!!!!
typedef enum
{
	E_MYHEAD = 0,
	E_COWER,
	E_SMACK,
	E_ENRAGED,
	E_VICTORY1,
	E_VICTORY2,
	E_VICTORY3,
	E_SWIRL,
	E_DANCE1,
	E_DANCE2,
	E_DANCE3,
	E_SIT1,
	E_SIT2,
	E_SIT3,
	E_SIT4,
	E_SIT5,
	E_KNEEL1,
	E_KNEEL2,
	E_KNEEL3,
	E_SLEEP,
	E_BREAKDANCE,
	E_CHEER,
	E_SURRENDER
} emote_type_t;
//RoAR mod END

//[ROQFILES]
extern qboolean inGameCinematic;
//[/ROQFILES]

/*
=================
ClientCommand
=================
*/
//[CoOpEditor]
extern void Create_Autosave( vec3_t origin, int size, qboolean teleportPlayers );
extern void Save_Autosaves(void);
extern void Delete_Autosaves(gentity_t* ent);
//[/CoOpEditor]
//[KnockdownSys]
extern void G_Knockdown( gentity_t *self, gentity_t *attacker, const vec3_t pushDir, float strength, qboolean breakSaberLock );
//[/KnockdownSys]
void ClientCommand( int clientNum ) {
	gentity_t *ent;
//	gentity_t *targetplayer;
	char	cmd[MAX_TOKEN_CHARS];
	char	cmd2[MAX_TOKEN_CHARS];
	//char	cmd3[MAX_TOKEN_CHARS];
//	float		bounty;
//	int clientid = 0;

	ent = g_entities + clientNum;
	if ( !ent->client ) {
		return;		// not fully in game yet
	}


	trap_Argv( 0, cmd, sizeof( cmd ) );

	//rww - redirect bot commands
	if (strstr(cmd, "bot_") && AcceptBotCommand(cmd, ent))
	{
		return;
	}
	//end rww
	if(!Q_stricmp(cmd,"modelscale"))
	{
		int size;
//		int temp,temp2;
		if(!ojp_modelscaleEnabled.integer)
		{
			trap_SendServerCommand( ent-g_entities, va("print \"Modelscale is disabled!\n\"") );
			return;
		}
		if(trap_Argc()!=2)
		{
			trap_SendServerCommand( ent-g_entities, va("print \"Current modelscale is %i.\n\"", (ent->client->ps.iModelScale ? ent->client->ps.iModelScale : 100)) );
			return;
		}
		trap_Argv(1,cmd2,sizeof(cmd2));
		size=atoi(cmd2);
		ent->client->ps.iModelScale=size;
		//ent->client->ps.stats[STAT_MAX_DODGE] = ((100-size)*(ent->client->ps.stats[STAT_DODGE]/100))/2;
 
		return;
	}

	if (!Q_stricmp(cmd, "roll"))
	{
		Cmd_Roll_F(ent);
		return;
	}

	if (Q_stricmp (cmd, "say") == 0) {
   		if ( ent->client->pers.silent )
		{
			trap_SendServerCommand(ent-g_entities,"print \"You have been silenced by an Admin\n\"");
			trap_SendServerCommand(ent-g_entities,"cp \"You have been silenced by an Admin\n\"");
			return;
		}
		if ( ent->client->pers.ampunish )
		{
			trap_SendServerCommand(ent-g_entities,"print \"You are being punished!\n\"");
			trap_SendServerCommand(ent-g_entities,"cp \"You are being punished!\n\"");
			return;
		}
		if ( ent->client->pers.silent2 )
		{
			int r = rand()%100;

			if (r <= 25)
			{
				G_Say( ent, NULL, SAY_ALL, roar_silence_insult_1.string );
			}
			else if (r <= 50)
			{
				G_Say( ent, NULL, SAY_ALL, roar_silence_insult_2.string );
			}
			else if (r <= 75)
			{
				G_Say( ent, NULL, SAY_ALL, roar_silence_insult_3.string );
			}
			else if (r <= 100)
			{
				G_Say( ent, NULL, SAY_ALL, roar_silence_insult_4.string );
			}
			return;
		}
		Cmd_Say_f (ent, SAY_ALL, qfalse);
		return;
	}

	if (Q_stricmp (cmd, "say_team") == 0) {
		if (g_gametype.integer < GT_TEAM)
		{ //not a team game, just refer to regular say.
			Cmd_Say_f (ent, SAY_ALL, qfalse);
		}
		else
		{
			Cmd_Say_f (ent, SAY_TEAM, qfalse);
		}
		return;
	}
	if (Q_stricmp (cmd, "tell") == 0) {
		Cmd_Tell_f ( ent );
		return;
	}

	//note: these voice_cmds come from the ui/jamp/ingame_voicechat.menu menu file...
	//		the strings are in strings/English/menus.str and all start with "VC_"
	if (Q_stricmp(cmd, "voice_cmd") == 0)
	{
		Cmd_VoiceCommand_f(ent);
		return;
	}

	if (Q_stricmp (cmd, "score") == 0) {
		Cmd_Score_f (ent);
		return;
	}

	//[ROQFILES]
	if (Q_stricmp (cmd, "EndCinematic") == 0)
	{//one of the clients just finished their cutscene, start rendering server frames again.
		inGameCinematic = qfalse;
		return;
	}
	//[/ROQFILES]
	

	// ignore all other commands when at intermission
	if (level.intermissiontime)
	{
		qboolean giveError = qfalse;
		//rwwFIXMEFIXME: This is terrible, write it differently

		if (!Q_stricmp(cmd, "give"))
		{
			giveError = qtrue;
		}
		else if (!Q_stricmp(cmd, "giveother"))
		{
			giveError = qtrue;
		}
		else if (!Q_stricmp(cmd, "god"))
		{
			giveError = qtrue;
		}
		else if (!Q_stricmp(cmd, "notarget"))
		{
			giveError = qtrue;
		}
		else if (!Q_stricmp(cmd, "noclip"))
		{
			giveError = qtrue;
		}
		else if (!Q_stricmp(cmd, "kill"))
		{
			giveError = qtrue;
		}
		else if (!Q_stricmp(cmd, "teamtask"))
		{
			giveError = qtrue;
		}
		else if (!Q_stricmp(cmd, "levelshot"))
		{
			giveError = qtrue;
		}
		else if (!Q_stricmp(cmd, "follow"))
		{
			giveError = qtrue;
		}
		else if (!Q_stricmp(cmd, "follownext"))
		{
			giveError = qtrue;
		}
		else if (!Q_stricmp(cmd, "followprev"))
		{
			giveError = qtrue;
		}
		else if (!Q_stricmp(cmd, "team"))
		{
			giveError = qtrue;
		}
		else if (!Q_stricmp(cmd, "duelteam"))
		{
			giveError = qtrue;
		}
		else if (!Q_stricmp(cmd, "siegeclass"))
		{
			giveError = qtrue;
		}
		else if (!Q_stricmp(cmd, "forcechanged"))
		{ //special case: still update force change
			Cmd_ForceChanged_f (ent);
			return;
		}
		else if (!Q_stricmp(cmd, "where"))
		{
			giveError = qtrue;
		}
		else if (!Q_stricmp(cmd, "callvote"))
		{
			giveError = qtrue;
		}
		else if (!Q_stricmp(cmd, "vote"))
		{
			giveError = qtrue;
		}
		else if (!Q_stricmp(cmd, "callteamvote"))
		{
			giveError = qtrue;
		}
		else if (!Q_stricmp(cmd, "teamvote"))
		{
			giveError = qtrue;
		}
		else if (!Q_stricmp(cmd, "gc"))
		{
			giveError = qtrue;
		}
		else if (!Q_stricmp(cmd, "setviewpos"))
		{
			giveError = qtrue;
		}
		else if (!Q_stricmp(cmd, "stats"))
		{
			giveError = qtrue;
		}

		if (giveError)
		{
			trap_SendServerCommand( clientNum, va("print \"%s (%s) \n\"", G_GetStringEdString("MP_SVGAME", "CANNOT_TASK_INTERMISSION"), cmd ) );
		}
		else
		{
			Cmd_Say_f (ent, qfalse, qtrue);
		}
		return;
	}

	if (Q_stricmp (cmd, "give") == 0)
	{
		Cmd_Give_f (ent, 0);
	}
	else if (Q_stricmp (cmd, "giveother") == 0)
	{ //for debugging pretty much
		Cmd_Give_f (ent, 1);
	}
	else if (Q_stricmp (cmd, "t_use") == 0 && CheatsOk(ent))
	{ //debug use map object
		if (trap_Argc() > 1)
		{
			char sArg[MAX_STRING_CHARS];
			gentity_t *targ;

			trap_Argv( 1, sArg, sizeof( sArg ) );
			targ = G_Find( NULL, FOFS(targetname), sArg );

			while (targ)
			{
				if (targ->use)
				{
					targ->use(targ, ent, ent);
				}
				targ = G_Find( targ, FOFS(targetname), sArg );
			}
		}
	}
	else if (Q_stricmp (cmd, "god") == 0)
		Cmd_God_f (ent);
	else if (Q_stricmp (cmd, "notarget") == 0)
		Cmd_Notarget_f (ent);
	else if (Q_stricmp (cmd, "noclip") == 0)
		Cmd_Noclip_f (ent);
	else if ( Q_stricmp( cmd, "NPC" ) == 0 /*&& CheatsOk(ent)*/ && !in_camera)
	{
		Cmd_NPC_f( ent );
	}
	else if (Q_stricmp (cmd, "kill") == 0)
		Cmd_Kill_f (ent);
	else if (Q_stricmp (cmd, "teamtask") == 0)
		Cmd_TeamTask_f (ent);
	else if (Q_stricmp (cmd, "levelshot") == 0)
		Cmd_LevelShot_f (ent);
	else if (Q_stricmp (cmd, "follow") == 0)
		Cmd_Follow_f (ent);
	else if (Q_stricmp (cmd, "follownext") == 0)
		Cmd_FollowCycle_f (ent, 1);
	else if (Q_stricmp (cmd, "followprev") == 0)
		Cmd_FollowCycle_f (ent, -1);
	else if (Q_stricmp (cmd, "team") == 0)
		Cmd_Team_f (ent);
	else if (Q_stricmp (cmd, "duelteam") == 0)
		Cmd_DuelTeam_f (ent);
	else if (Q_stricmp (cmd, "siegeclass") == 0)
		Cmd_SiegeClass_f (ent);
	else if (Q_stricmp (cmd, "forcechanged") == 0)
		Cmd_ForceChanged_f (ent);
	else if (Q_stricmp (cmd, "where") == 0)
		Cmd_Where_f (ent);
	else if (Q_stricmp (cmd, "callvote") == 0)
		Cmd_CallVote_f (ent);
	else if (Q_stricmp (cmd, "vote") == 0)
		Cmd_Vote_f (ent);
	else if (Q_stricmp (cmd, "callteamvote") == 0)
		Cmd_CallTeamVote_f (ent);
	else if (Q_stricmp (cmd, "teamvote") == 0)
		Cmd_TeamVote_f (ent);
	else if (Q_stricmp (cmd, "gc") == 0)
		Cmd_GameCommand_f( ent );
	else if (Q_stricmp (cmd, "setviewpos") == 0)
		Cmd_SetViewpos_f( ent );
	else if (Q_stricmp (cmd, "stats") == 0)
		Cmd_Stats_f( ent );
	//for convenient powerduel testing in release
	else if (Q_stricmp(cmd, "killother") == 0 && CheatsOk( ent ))
	{
		if (trap_Argc() > 1)
		{
			char sArg[MAX_STRING_CHARS];
			int entNum = 0;

			trap_Argv( 1, sArg, sizeof( sArg ) );

			entNum = G_ClientNumFromNetname(sArg);

			if (entNum >= 0 && entNum < MAX_GENTITIES)
			{
				gentity_t *kEnt = &g_entities[entNum];

				if (kEnt->inuse && kEnt->client)
				{
					kEnt->flags &= ~FL_GODMODE;
					kEnt->client->ps.stats[STAT_HEALTH] = kEnt->health = -999;
					player_die (kEnt, kEnt, kEnt, 100000, MOD_SUICIDE);
				}
			}
		}
	}
	//[Test]
#ifndef FINAL_BUILD
	else if (Q_stricmp(cmd, "testtrace") == 0)
	{
		trace_t tr;
		vec3_t traceTo, traceFrom, traceDir;

		AngleVectors(ent->client->ps.viewangles, traceDir, 0, 0);
		VectorCopy(ent->client->ps.origin, traceFrom);
		VectorMA( traceFrom, 30, traceDir, traceTo );

		trap_Trace( &tr, traceFrom, NULL, NULL, traceTo, ent->s.number, MASK_SHOT );

		if(tr.fraction < 1.0f)
		{
			G_Printf("%i", tr.entityNum);
		}
	}
#endif
	//[/Test]
	else if (Q_stricmp(cmd, "lamercheck") == 0)
	{
		trap_SendServerCommand( -1, va("cp \"This mod is based on code taken from the\nOpen Jedi Project. If the supposed author doesn't\ngive proper credit to OJP,\nplease contact us and we\n will deal with it.\nEmail: razorace@hotmail.com\n\""));
	}
	//[HolocronFiles]
	else if (Q_stricmp(cmd, "!addholocron") == 0 && bot_wp_edit.integer >= 1)
	{// Add a new holocron point. Unique1 added.
		AOTCTC_Holocron_Add ( ent );
	}
	else if (Q_stricmp(cmd, "!saveholocrons") == 0 && bot_wp_edit.integer >= 1)
	{// Save holocron position table. Unique1 added.
		AOTCTC_Holocron_Savepositions();		
	}
	else if (Q_stricmp(cmd, "!spawnholocron") == 0 && bot_wp_edit.integer >= 1)
	{// Spawn a holocron... Unique1 added.
		AOTCTC_Create_Holocron( rand()%18, ent->r.currentOrigin );
	}
	//[/HolocronFiles]
	//[CoOpEditor]
	else if (Q_stricmp (cmd, "autosave_add") == 0 && bot_wp_edit.integer)
	{
		int args = trap_Argc();
		char arg1[MAX_STRING_CHARS];
		char arg2[MAX_STRING_CHARS];

		
		if(args < 1)
		{//no args, use defaults
			Create_Autosave(ent->r.currentOrigin, 0, qfalse);
		}
		else
		{
			trap_Argv(1, arg1, sizeof(arg1));
			if(arg1[0] == 't')
			{//use default size with teleport flag
				Create_Autosave(ent->r.currentOrigin, 0, qtrue);
			}
			else if(args > 1)
			{//size and teleport flag
				trap_Argv(2, arg2, sizeof(arg2));
				Create_Autosave(ent->r.currentOrigin, atoi(arg1), arg2[0] == 't' ? qtrue:qfalse );
			}
			else
			{//just size
				Create_Autosave(ent->r.currentOrigin, atoi(arg1), qfalse );
			}
		}
	}
	else if (Q_stricmp (cmd, "autosave_save") == 0 && bot_wp_edit.integer)
	{
		Save_Autosaves();
	}
	else if (Q_stricmp (cmd, "autosave_delete") == 0 && bot_wp_edit.integer)
	{
		Delete_Autosaves(ent);
	}
	//[/CoOpEditor]
//RoAR mod COMMAND LINES BEGIN
	else if ((Q_stricmp (cmd, "help") == 0) || (Q_stricmp (cmd, "info") == 0)){
		char   arg1[MAX_STRING_CHARS];
		trap_Argv( 1,  arg1, sizeof( arg1 ) );
		//trap_SendServerCommand( ent-g_entities, "print \"^3===^1BLANK COMMAND^3===\n\n^1DESCRIPTION: ^3BLANK\n\n^5Commands\n\n\"" );
		if(( Q_stricmp( arg1, "freeze" ) == 0 ) || (Q_stricmp( arg1, "amfreeze" ) == 0)){
			trap_SendServerCommand( ent-g_entities, "print \"^3===^1FREEZE ADMIN COMMAND^3===\n\n^1DESCRIPTION: ^3This command freezes a player solid and holsters their server. Rendering them unable to move.\n\n^5/Freeze +all  <--- Freeze everyone on server\n/Freeze -all  <--- Unfreeze everyone on server\n/Freeze (client ID)  <--- Freeze a single person by their ID (type in /WHO to see everyones ID)\n/Freeze (client name)  <--- Freeze a single person by their name\n^1To unfreeze a client, you must type in the command again. It's toggled.\nYou can also use... /amfreeze\n\n\"" );
		}
		else if(( Q_stricmp( arg1, "terminator" ) == 0 ) || (Q_stricmp(arg1, "amterminator") == 0) || (Q_stricmp(arg1, "ammerc") == 0)){
			trap_SendServerCommand( ent-g_entities, "print \"^3===^1TERMINATOR ADMIN COMMAND^3===\n\n^1DESCRIPTION: ^3This command makes a player have all weapons and infinite ammo. However, they cannot use force powers.\n\n^5/Terminator  <--- If command is typed in alone, it will perform Terminator on yourself\n/Terminator +all  <--- Terminator everyone on server\n/Terminator -all  <--- Unterminator everyone on server\n/Terminator (client ID)  <--- Terminator a single person by their ID (type in /WHO to see everyones ID)\n/Terminator (client name)  <--- Terminator a single person by their name\n^1To unterminator a client, you must type in the command again. It's toggled.\nYou can also use... /ammerc /amterminator\n\n\"" );
		}
		else if(( Q_stricmp( arg1, "empower" ) == 0 ) || (Q_stricmp(arg1, "amempower") == 0)){
			trap_SendServerCommand( ent-g_entities, "print \"^3===^1EMPOWER ADMIN COMMAND^3===\n\n^1DESCRIPTION: ^3This command makes a player have all the force powers and infinite force. However, they cannot use weapons.\n\n^5/Empower  <--- If command is typed in alone, it will perform Empower on yourself\n/Empower +all  <--- Empower everyone on server\n/Empower -all  <--- UnEmpower everyone on server\n/Empower (client ID)  <--- Empower a single person by their ID (type in /WHO to see everyones ID)\n/Empower (client name)  <--- Empower a single person by their name\n^1To unempower a client, you must type in the command again. It's toggled.\nYou can also use... /amempower\n\n\"" );
		}
		else if(( Q_stricmp( arg1, "origin" ) == 0 ) || (Q_stricmp(arg1, "amorigin") == 0)){
			trap_SendServerCommand( ent-g_entities, "print \"^3===^1ORIGIN COMMAND^3===\n\n^1DESCRIPTION: ^3Type this in to find out your exact X Y and Z coordinates. An admin can teleport to these using the /teleport admin command.\n\n^5/Origin  <--- Find out your X Y and Z coordinates\n/Origin (client ID)  <--- Find out someone elses X Y and Z coordinates by their ID (type in /WHO to see everyones ID)\n/Origin (client name)  <--- Find out someone elses X Y and Z coordinates by their name\nYou can also use... /amorigin\n\n\"" );
		}
		else if(( Q_stricmp( arg1, "monk" ) == 0 ) || (Q_stricmp(arg1, "ammonk") == 0)){
			trap_SendServerCommand( ent-g_entities, "print \"^3===^1MONK ADMIN COMMAND^3===\n\n^1DESCRIPTION: ^3This command makes a player have only melee, but they regenerate +1HP every 1 second. However, they cannot use weapons, or force powers.\n\n^5/Monk  <--- If command is typed in alone, it will perform Monk on yourself\n/Monk +all  <--- Monk everyone on server\n/Monk -all  <--- UnMonk everyone on server\n/Monk (client ID)  <--- Monk a single person by their ID (type in /WHO to see everyones ID)\n/Monk (client name)  <--- Monk a single person by their name\n^1To unmonk a client, you must type in the command again. It's toggled.\nYou can also use... /ammonk\n\n\"" );
		}
		else if(( Q_stricmp( arg1, "silence" ) == 0 ) || (Q_stricmp(arg1, "amsilence") == 0)){
			trap_SendServerCommand( ent-g_entities, "print \"^3===^1SILENCE ADMIN COMMAND^3===\n\n^1DESCRIPTION: ^3Silence a player on a server, making them silent till they leave the server.\n\n^5/Silence +all  <--- Silence everyone on server\n/Silence -all  <--- Unsilence everyone on server\n/Silence (client ID)  <--- Silence a single person by their ID (type in /WHO to see everyones ID)\n/Silence (client name)  <--- Silence a single person by their name\n^1To unsilence a client, you must type in the command again. It's toggled.\nYou can also use... /amsilence\n\n\"" );
		}
		else if(( Q_stricmp( arg1, "protect" ) == 0 ) || (Q_stricmp(arg1, "amprotect") == 0)){
			trap_SendServerCommand( ent-g_entities, "print \"^3===^1PROTECT ADMIN COMMAND^3===\n\n^1DESCRIPTION: ^3Protect a player on a server. If the player attacks, they will become unprotected. You cannot unprotect idle-protected people.\n\n^5/Protect  <--- If command is typed in alone, it will perform Protect on yourself\n/Protect +all  <--- Protect everyone on server\n/Protect -all  <--- Unprotect everyone on server\n/Protect (client ID)  <--- Protect a single person by their ID (type in /WHO to see everyones ID)\n/Protect (client name)  <--- Protect a single person by their name\n^1To unprotect a client, you must type in the command again. It's toggled.\nYou can also use... /amprotect\n\n\"" );
		}
		else if(( Q_stricmp( arg1, "demigod" ) == 0 ) || (Q_stricmp(arg1, "amdemigod") == 0)){
			trap_SendServerCommand( ent-g_entities, "print \"^3===^1DEMIGOD ADMIN COMMAND^3===\n\n^1DESCRIPTION: ^3Turn the character into a demi-god (half god). You can fly through walls, and act as a spectator. You cannot be injured, and cannot attack other people.\n\n^5/DemiGod  <--- If command is typed in alone, it will perform DemiGod on yourself\n/DemiGod +all  <--- DemiGod everyone on server\n/DemiGod -all  <--- UnDemiGod everyone on server\n/DemiGod (client ID)  <--- DemiGod a single person by their ID (type in /WHO to see everyones ID)\n/DemiGod (client name)  <--- DemiGod a single person by their name\n^1To undemigod a client, you must type in the command again. It's toggled.\nYou can also use... /amdemigod\n\n\"" );
		}
		else if(( Q_stricmp( arg1, "splat" ) == 0 ) || (Q_stricmp(arg1, "amsplat") == 0)){
			trap_SendServerCommand( ent-g_entities, "print \"^3===^1SPLAT ADMIN COMMAND^3===\n\n^1DESCRIPTION: ^3Throw a player into the air, and let them fall to their death.\n\n^5/Splat (client ID)  <--- Splat a single person by their ID (type in /WHO to see everyones ID)\n/Splat (client name)  <--- Splat a single person by their name\nYou can also use... /amsplat\n\n\"" );
		}
		else if(( Q_stricmp( arg1, "sleep" ) == 0 ) || (Q_stricmp(arg1, "amsleep") == 0)){
			trap_SendServerCommand( ent-g_entities, "print \"^3===^1SLEEP ADMIN COMMAND^3===\n\n^1DESCRIPTION: ^3Make a character fall on the ground, and unable to get up.\n\n^5/Sleep (client ID)  <--- Sleep a single person by their ID (type in /WHO to see everyones ID)\n/Sleep (client name)  <--- Sleep a single person by their name\nYou can also use... /amsleep\n\n\"" );
		}
		else if(( Q_stricmp( arg1, "punish" ) == 0 ) || (Q_stricmp(arg1, "ampunish") == 0)){
			trap_SendServerCommand( ent-g_entities, "print \"^3===^1PUNISH ADMIN COMMAND^3===\n\n^1DESCRIPTION: ^3Silence a client, and make them unable to move.\n\n^5/Punish (client ID)  <--- Punish a single person by their ID (type in /WHO to see everyones ID)\n/Punish (client name)  <--- Punish a single person by their name\nYou can also use... /ampunish\n\n\"" );
		}
		else if(( Q_stricmp( arg1, "slay" ) == 0 ) || (Q_stricmp(arg1, "amslay") == 0)){
			trap_SendServerCommand( ent-g_entities, "print \"^3===^1SLAY ADMIN COMMAND^3===\n\n^1DESCRIPTION: ^3Force a player to kill themselves.\n\n^5/Slay (client ID)  <--- Slay a single person by their ID (type in /WHO to see everyones ID)\n/Slay (client name)  <--- Slay a single person by their name\nYou can also use... /amslay\n\n\"" );
		}
		else if(( Q_stricmp( arg1, "lockteam" ) == 0 ) || (Q_stricmp(arg1, "amlockteam") == 0)){
			trap_SendServerCommand( ent-g_entities, "print \"^3===^1LOCKTEAM ADMIN COMMAND^3===\n\n^1DESCRIPTION: ^3Lock a team so clients cant join it.\n\n^5/LockTeam (team)\n^3TEAMS = red, blue, spectator, join\nYou can also use... /amLockTeam\n\n\"" );
		}
		else if (( Q_stricmp( arg1, "Teleport" ) == 0 ) || (Q_stricmp( arg1, "Tele") == 0 ) || (Q_stricmp(cmd, "admintele") == 0) || (Q_stricmp(cmd, "amtele") == 0)){
			trap_SendServerCommand( ent-g_entities, "print \"^3===^1TELEPORT COMMAND^3===\n\n^1DESCRIPTION: ^3Teleport a client or yourself to a certain place.\n\n^5/Teleport (X)(Y)(Z)  <---- Teleport yourself to a certain coordinate (type in /origin to see your current coordinates)\n/Teleport (client ID 1) (client ID 2)  <--- Teleport Client 1 to Client 2 (type in /WHO to see everyones' ID)\n/Teleport (client name)  <--- Teleport a single person to you by their name\n^5You can type in /Tele instead of /Teleport.\nYou can also use... /amtele /admintele\n\n\"" );
		}
		else if(( Q_stricmp( arg1, "insultsilence" ) == 0 ) || (Q_stricmp( arg1, "aminsultsilence" ) == 0)){
			trap_SendServerCommand( ent-g_entities, "print \"^3===^1INSULTSILENCE ADMIN COMMAND^3===\n\n^1DESCRIPTION: ^3Silence a person in a different way. If they do speak, they will automatically say 1 of the 4 defined insults. You can define these in the server.cfg file\n\n^5/InsultSilence (client ID)  <--- InsultSilence a single person by their ID (type in /WHO to see everyones ID)\n/InsultSilence (client name)  <--- InsultSilence a single person by their name\n^1To uninstultsilence a client, you must type in the command again. It's toggled.\nYou can also use... /aminsultsilence\n\n\"" );
		}
		else if( Q_stricmp( arg1, "npc" ) == 0 ){
			trap_SendServerCommand( ent-g_entities, "print \"^3===^1NPC ADMIN COMMAND^3===\n\n^1DESCRIPTION: ^3Spawn an NPC or kill one.\n^5/NPC Spawn (ID)\n/NPC Spawn Vehicle (ID)\n/NPC kill all\n\n\"" );
		}
		else if(( Q_stricmp( arg1, "rename" ) == 0 ) || (Q_stricmp( arg1, "amrename" ) == 0)){
			trap_SendServerCommand( ent-g_entities, "print \"^3===^1RENAME ADMIN COMMAND^3===\n\n^1DESCRIPTION: ^3Forcefully rename a player to what you want.\n\n/Rename (client ID or Name) (new name)  <--- Rename a client to a new name.\nYou can also use... /amrename\n\n\"" );
		}
		else if(( Q_stricmp( arg1, "lockname" ) == 0 ) || (Q_stricmp( arg1, "amlockname" ) == 0)){
			trap_SendServerCommand( ent-g_entities, "print \"^3===^1LOCKNAME ADMIN COMMAND^3===\n\n^1DESCRIPTION: ^3Stop a player from renaming by locking their name.\n\n/LockName (client ID or Name)  <--- Lock a clients name.\n^1To unlock a client's name, you must type in the command again. It's toggled.\nYou can also use... /amlockname\n\n\"" );
		}
		else if(( Q_stricmp( arg1, "forceteam" ) == 0 ) || (Q_stricmp( arg1, "amforceteam" ) == 0)){
			trap_SendServerCommand( ent-g_entities, "print \"^3===^1FORCETEAM ADMIN COMMAND^3===\n\n^1DESCRIPTION: ^3Force a client to a team.\n\n/ForceTeam (client ID or Name) (team)  <--- Force a client to a team. TEAMS = free, spectator, blue, red\nYou can also use... /amforceteam\n\n\"" );
		}
		else if(( Q_stricmp( arg1, "adminsit" ) == 0 ) || (Q_stricmp( arg1, "amadminsit" ) == 0)){
			trap_SendServerCommand( ent-g_entities, "print \"^3===^1ADMINSIT ADMIN COMMAND^3===\n\n^1DESCRIPTION: ^3Sit anywhere you want. You can sit in mid-air if you want to watch over lamers.\nYou can also use... /amadminsit\n\n\"" );
		}
		else if(( Q_stricmp( arg1, "scale" ) == 0 ) || (Q_stricmp( arg1, "amscale" ) == 0)){
			trap_SendServerCommand( ent-g_entities, "print \"^3===^1SCALE ADMIN COMMAND^3===\n\n^1DESCRIPTION: ^3Scale a players character model. MIN = 50, MAX = 200\n^5/Scale (client ID or Name) (scale)  <--- Scale a client by their name or ID, to a certain scale\n- or -\n/scale (scale)\nYou can also use... /amscale\n\n\"" );
		}
		else if(( Q_stricmp( arg1, "csprint" ) == 0 ) || (Q_stricmp( arg1, "amcsprint" ) == 0) || (Q_stricmp( arg1, "ampsay" ) == 0)){
			trap_SendServerCommand( ent-g_entities, "print \"^3===^1CSPRINT ADMIN COMMAND^3===\n\n^1DESCRIPTION: ^3CSPrint stands for Center Screen Print. You can center screen print any message you want.\nUSAGE: ^5/CSPrint (message)\nYou can also use... /amcsprint /ampsay\n\n\"" );
		}
		else if(( Q_stricmp( arg1, "adminkick" ) == 0 ) || (Q_stricmp( arg1, "amkick" ) == 0) || (Q_stricmp( arg1, "amadminkick" ) == 0)){
			trap_SendServerCommand( ent-g_entities, "print \"^3===^1ADMINKICK ADMIN COMMAND^3===\n\n^1DESCRIPTION: ^3Kick a client out of the server.\nUSAGE: ^5/AdminKick (client ID or Name)\nYou can also use... /amadminkick /amkick\n\n\"" );
		}
		else if(( Q_stricmp( arg1, "adminban" ) == 0 ) || (Q_stricmp( arg1, "amadminban" ) == 0) || (Q_stricmp( arg1, "amban" ) == 0)){
			trap_SendServerCommand( ent-g_entities, "print \"^3===^1ADMINBAN ADMIN COMMAND^3===\n\n^1DESCRIPTION: ^3Ban a client from the server.\nUSAGE: ^5/AdminBan (client ID or Name)\nYou can also use... /amban\n\n\"" );
		}
		else if(( Q_stricmp( arg1, "changemap" ) == 0 ) || (Q_stricmp( arg1, "amchangemap" ) == 0) || Q_stricmp( arg1, "ammap" ) == 0){
			trap_SendServerCommand( ent-g_entities, "print \"^3===^1CHANGEMAP ADMIN COMMAND^3===\n\n^1DESCRIPTION: ^3Change the map.\nUSAGE: ^5/ChangeMap (map name)\nYou can also use... /amchangemap /ammap\n\n\"" );
		}
		else if(( Q_stricmp( arg1, "weather" ) == 0 ) || (Q_stricmp(arg1, "setweather") == 0) || (Q_stricmp(arg1, "amweather") == 0)){
			trap_SendServerCommand( ent-g_entities, "print \"^3===^1WEATHER ADMIN COMMAND^3===\n\n^1DESCRIPTION: ^3Change the weather on a map.\n\n^5/Weather (weather)\nWeathers = snow, rain\n^1WARNING: Once the weather is set, it requires a map restart to stop the weather!\nYou can also use... /amweather /setweather\n\n\"" );
		}
		else if( Q_stricmp( arg1, "addeffect" ) == 0 ){
			trap_SendServerCommand( ent-g_entities, "print \"^3===^1ADDEFFECT ADMIN COMMAND^3===\n\n^1DESCRIPTION: ^3Place an effect on a map.\n\n^5/Place (fxFile)\n^1WARNING: Once the effect is placed, it requires map restart to stop it!\nEXAMPLE: /place env/small_fire\n\n\"" );
		}
		else {
		trap_SendServerCommand( ent-g_entities, "print \"^3===^1HELP^3===\n\n/AdminCommands <--- see a list of admin commands\n/Sayings <--- list of chat-line commands\n/roll <number of sides> <--- perform a roll, ranging from 3-sided to 100-sided\n/saberdamages <--- show server saber damages\n\n^5For more options type in  /HELP (COMMAND)\n\n\"" );
		}
	}
	else if (Q_stricmp (cmd, "sayings") == 0)
	{
		trap_SendServerCommand( ent-g_entities, "print \"^3===^1SAYINGS^3===\n\n^1DESCRIPTION: ^3Use these commands when you type normally: (/me - emote, /shout - shout, /low - low, /comm - long distance communication, /ooc - OOC chat\n\n\"" );
	}
	else if (Q_stricmp (cmd, "adminguns") == 0)
	{
		if (ent->client->ojpClientPlugIn){
			trap_SendServerCommand( ent-g_entities, "print \"^3===^1ADMIN GUNS^3===\n\n^1DESCRIPTION: ^3Bind these commands to a key /BIND (key) (gun command). Aim your cross hair at a client, and press the button. It will execute the corresponding admin command on that client.\n\n^5gun_freeze, gun_kick, gun_silence, gun_insult, gun_terminator, gun_empower, gun_splat\ngun_slay, gun_monk, gun_sleep, gun_slap, gun_punish\n\n\"" );
		}
		else {
		trap_SendServerCommand( ent-g_entities, va("print \"^1UNAVAILABLE\n\"" ) );
		}
	}
		else if (Q_stricmp (cmd, "emotes") == 0)
	{
		trap_SendServerCommand( ent-g_entities, "print \"^3===^1EMOTES^3===\n\n^5/dance1, /dance2, /dance3, /taunt, /cower, /smack, /swirl\n/kneel1, /kneel2, /kneel3, /breakdance, /laydown, /myhead, /cheer\n/sit1, /sit2, /sit3, /sit4, /sit5, /sit6, /sit7, /surrender, /enraged\n/victory1, /victory2, /victory3, /choke1, /choke3\n/wait1, /wait2, /die1, /die2, /die3, /die4\n\n\"" );
	}
		else if (Q_stricmp (cmd, "admincommands") == 0)
	{
		trap_SendServerCommand( ent-g_entities, "print \"^3===^1ADMIN COMMANDS^3===\n\n/AdminBan, /DemiGod, /GrantAdmin, /Protect, /Terminator, /Slay, /Rename\n/Silence, /InsultSilence, /Splat, /Empower, /Freeze, /NPC, /Scale\n/ChangeMap, /AdminTele, /AdminKick, /AdminSit, /LockName, /CSPrint\n/ForceTeam, /Monk, /Sleep, /Punish, /Slap, /LockTeam\n/Place, /MyAdminCommands\n\n\"" );
	}
		else if (Q_stricmp (cmd, "commands") == 0)
	{
		trap_SendServerCommand( ent-g_entities, "print \"^3===^1COMMANDS^3===\n\n/jetpack <--- put on a jetpack\n/knockmedown <--- knock yourself down\n/dropsaber <--- drop your saber\n/showmotd <--- see the MOTD\n/freezemotd <--- shopw MOTD for infinite time\n/HideMOTD <--- hide the MOTD\n/engage_forceduel <--- duel with force powers\n/engage_meleeduel <--- duel with only melee moves (requires plugin)\n/engage_trainingduel <--- engage in a private training session\n/endduel <--- end training duel session\n/who <--- show all clients + their status\n/chatcolor <--- SAY_ALL in a different color\n/togglechat <--- toggle teamchat mode\n+button12 <--- grappling hook\n/ignore (client) <--- ignore a clients chat text\n\n\"" );
	}
		else if (Q_stricmp (cmd, "playerconfigs") == 0)
	{
		if (ent->client->ojpClientPlugIn){
		trap_SendServerCommand( ent-g_entities, "print \"^3===^1CONFIGURATIONS^3===\n/set c_adminpassword PASSWORD_HERE <--- when entering a server, auto login as admin\n^3/set clan CLAN_NAME <--- scoreboard will show your clan name\n/c_clanpassword PASSWORD_HERE <--- auto login as a clan member\n/set c_chatcolor COLOR_HERE <--- in FFA, speak in any color you want\n^5COLORS = RED GREEN YELLOW BLUE CYAN PURPLE BLACK\n^3/c_chatmode <--- team chat modes\n^5CHAT MODES = TEAM ADMIN CLAN\n\n\"" );
		}
		else {
			trap_SendServerCommand( ent-g_entities, va("print \"^1YOU DO NOT HAVE THE CLAN PLUGIN! WITHOUT IT, YOU CANNOT DO ANY OF THESE CONFIGURATIONS! DOWNLOAD IT AT WWW.CLANMOD.ORG/DOWNLOADS\n\"" ) );
		}
	}
		else if (Q_stricmp (cmd, "myadmincommands") == 0){
		if (!(ent->r.svFlags & SVF_ADMIN1) && !(ent->r.svFlags & SVF_ADMIN2) && !(ent->r.svFlags & SVF_ADMIN3)
			&& !(ent->r.svFlags & SVF_ADMIN4) && !(ent->r.svFlags & SVF_ADMIN5)) {
				trap_SendServerCommand( ent-g_entities, va("print \"^1You are not an administrator on this server!\n\"") );
				return;
			}
		//TELEPORT CHECK
		if ((ent->r.svFlags & SVF_ADMIN1) && (roar_adminControl1.integer & (1 << A_ADMINTELE))) {
			trap_SendServerCommand( ent-g_entities, va("print \"Teleport\n\"") ); }
		if ((ent->r.svFlags & SVF_ADMIN2) && (roar_adminControl2.integer & (1 << A_ADMINTELE))) {
			trap_SendServerCommand( ent-g_entities, va("print \"Teleport\n\"") ); }
		if ((ent->r.svFlags & SVF_ADMIN3) && (roar_adminControl3.integer & (1 << A_ADMINTELE))) {
			trap_SendServerCommand( ent-g_entities, va("print \"Teleport\n\"") ); }
		if ((ent->r.svFlags & SVF_ADMIN4) && (roar_adminControl4.integer & (1 << A_ADMINTELE))) {
			trap_SendServerCommand( ent-g_entities, va("print \"Teleport\n\"") ); }
		if ((ent->r.svFlags & SVF_ADMIN5) && (roar_adminControl5.integer & (1 << A_ADMINTELE))) {
			trap_SendServerCommand( ent-g_entities, va("print \"Teleport\n\"") ); }
		//FREEZE CHECK
		if ((ent->r.svFlags & SVF_ADMIN1) && (roar_adminControl1.integer & (1 << A_FREEZE))) {
			trap_SendServerCommand( ent-g_entities, va("print \"Freeze\n\"") ); }
		if ((ent->r.svFlags & SVF_ADMIN2) && (roar_adminControl2.integer & (1 << A_FREEZE))) {
			trap_SendServerCommand( ent-g_entities, va("print \"Freeze\n\"") ); }
		if ((ent->r.svFlags & SVF_ADMIN3) && (roar_adminControl3.integer & (1 << A_FREEZE))) {
			trap_SendServerCommand( ent-g_entities, va("print \"Freeze\n\"") ); }
		if ((ent->r.svFlags & SVF_ADMIN4) && (roar_adminControl4.integer & (1 << A_FREEZE))) {
			trap_SendServerCommand( ent-g_entities, va("print \"Freeze\n\"") ); }
		if ((ent->r.svFlags & SVF_ADMIN5) && (roar_adminControl5.integer & (1 << A_FREEZE))) {
			trap_SendServerCommand( ent-g_entities, va("print \"Freeze\n\"") ); }
		//SILENCE CHECK
		if ((ent->r.svFlags & SVF_ADMIN1) && (roar_adminControl1.integer & (1 << A_SILENCE))) {
			trap_SendServerCommand( ent-g_entities, va("print \"Silence\n\"") ); }
		if ((ent->r.svFlags & SVF_ADMIN2) && (roar_adminControl2.integer & (1 << A_SILENCE))) {
			trap_SendServerCommand( ent-g_entities, va("print \"Silence\n\"") ); }
		if ((ent->r.svFlags & SVF_ADMIN3) && (roar_adminControl3.integer & (1 << A_SILENCE))) {
			trap_SendServerCommand( ent-g_entities, va("print \"Silence\n\"") ); }
		if ((ent->r.svFlags & SVF_ADMIN4) && (roar_adminControl4.integer & (1 << A_SILENCE))) {
			trap_SendServerCommand( ent-g_entities, va("print \"Silence\n\"") ); }
		if ((ent->r.svFlags & SVF_ADMIN5) && (roar_adminControl5.integer & (1 << A_SILENCE))) {
			trap_SendServerCommand( ent-g_entities, va("print \"Silence\n\"") ); }
		//PROTECT CHECK
		if ((ent->r.svFlags & SVF_ADMIN1) && (roar_adminControl1.integer & (1 << A_PROTECT))) {
			trap_SendServerCommand( ent-g_entities, va("print \"Protect\n\"") ); }
		if ((ent->r.svFlags & SVF_ADMIN2) && (roar_adminControl2.integer & (1 << A_PROTECT))) {
			trap_SendServerCommand( ent-g_entities, va("print \"Protect\n\"") ); }
		if ((ent->r.svFlags & SVF_ADMIN3) && (roar_adminControl3.integer & (1 << A_PROTECT))) {
			trap_SendServerCommand( ent-g_entities, va("print \"Protect\n\"") ); }
		if ((ent->r.svFlags & SVF_ADMIN4) && (roar_adminControl4.integer & (1 << A_PROTECT))) {
			trap_SendServerCommand( ent-g_entities, va("print \"Protect\n\"") ); }
		if ((ent->r.svFlags & SVF_ADMIN5) && (roar_adminControl5.integer & (1 << A_PROTECT))) {
			trap_SendServerCommand( ent-g_entities, va("print \"Protect\n\"") ); }
		//ADMINBAN CHECK
		if ((ent->r.svFlags & SVF_ADMIN1) && (roar_adminControl1.integer & (1 << A_ADMINBAN))) {
			trap_SendServerCommand( ent-g_entities, va("print \"AdminBan\n\"") ); }
		if ((ent->r.svFlags & SVF_ADMIN2) && (roar_adminControl2.integer & (1 << A_ADMINBAN))) {
			trap_SendServerCommand( ent-g_entities, va("print \"AdminBan\n\"") ); }
		if ((ent->r.svFlags & SVF_ADMIN3) && (roar_adminControl3.integer & (1 << A_ADMINBAN))) {
			trap_SendServerCommand( ent-g_entities, va("print \"AdminBan\n\"") ); }
		if ((ent->r.svFlags & SVF_ADMIN4) && (roar_adminControl4.integer & (1 << A_ADMINBAN))) {
			trap_SendServerCommand( ent-g_entities, va("print \"AdminBan\n\"") ); }
		if ((ent->r.svFlags & SVF_ADMIN5) && (roar_adminControl5.integer & (1 << A_ADMINBAN))) {
			trap_SendServerCommand( ent-g_entities, va("print \"AdminBan\n\"") ); }
		//ADMINKICK CHECK
		if ((ent->r.svFlags & SVF_ADMIN1) && (roar_adminControl1.integer & (1 << A_ADMINKICK))) {
			trap_SendServerCommand( ent-g_entities, va("print \"AdminKick\n\"") ); }
		if ((ent->r.svFlags & SVF_ADMIN2) && (roar_adminControl2.integer & (1 << A_ADMINKICK))) {
			trap_SendServerCommand( ent-g_entities, va("print \"AdminKick\n\"") ); }
		if ((ent->r.svFlags & SVF_ADMIN3) && (roar_adminControl3.integer & (1 << A_ADMINKICK))) {
			trap_SendServerCommand( ent-g_entities, va("print \"AdminKick\n\"") ); }
		if ((ent->r.svFlags & SVF_ADMIN4) && (roar_adminControl4.integer & (1 << A_ADMINKICK))) {
			trap_SendServerCommand( ent-g_entities, va("print \"AdminKick\n\"") ); }
		if ((ent->r.svFlags & SVF_ADMIN5) && (roar_adminControl5.integer & (1 << A_ADMINKICK))) {
			trap_SendServerCommand( ent-g_entities, va("print \"AdminKick\n\"") ); }
		//NPC CHECK
		if ((ent->r.svFlags & SVF_ADMIN1) && (roar_adminControl1.integer & (1 << A_NPC))) {
			trap_SendServerCommand( ent-g_entities, va("print \"NPC\n\"") ); }
		if ((ent->r.svFlags & SVF_ADMIN2) && (roar_adminControl2.integer & (1 << A_NPC))) {
			trap_SendServerCommand( ent-g_entities, va("print \"NPC\n\"") ); }
		if ((ent->r.svFlags & SVF_ADMIN3) && (roar_adminControl3.integer & (1 << A_NPC))) {
			trap_SendServerCommand( ent-g_entities, va("print \"NPC\n\"") ); }
		if ((ent->r.svFlags & SVF_ADMIN4) && (roar_adminControl4.integer & (1 << A_NPC))) {
			trap_SendServerCommand( ent-g_entities, va("print \"NPC\n\"") ); }
		if ((ent->r.svFlags & SVF_ADMIN5) && (roar_adminControl5.integer & (1 << A_NPC))) {
			trap_SendServerCommand( ent-g_entities, va("print \"NPC\n\"") ); }
		//INSULTSILENCE CHECK
		if ((ent->r.svFlags & SVF_ADMIN1) && (roar_adminControl1.integer & (1 << A_INSULTSILENCE))) {
			trap_SendServerCommand( ent-g_entities, va("print \"InsultSilence\n\"") ); }
		if ((ent->r.svFlags & SVF_ADMIN2) && (roar_adminControl2.integer & (1 << A_INSULTSILENCE))) {
			trap_SendServerCommand( ent-g_entities, va("print \"InsultSilence\n\"") ); }
		if ((ent->r.svFlags & SVF_ADMIN3) && (roar_adminControl3.integer & (1 << A_INSULTSILENCE))) {
			trap_SendServerCommand( ent-g_entities, va("print \"InsultSilence\n\"") ); }
		if ((ent->r.svFlags & SVF_ADMIN4) && (roar_adminControl4.integer & (1 << A_INSULTSILENCE))) {
			trap_SendServerCommand( ent-g_entities, va("print \"InsultSilence\n\"") ); }
		if ((ent->r.svFlags & SVF_ADMIN5) && (roar_adminControl5.integer & (1 << A_INSULTSILENCE))) {
			trap_SendServerCommand( ent-g_entities, va("print \"InsultSilence\n\"") ); }
		//TERMINATOR CHECK
		if ((ent->r.svFlags & SVF_ADMIN1) && (roar_adminControl1.integer & (1 << A_TERMINATOR))) {
			trap_SendServerCommand( ent-g_entities, va("print \"Terminator\n\"") ); }
		if ((ent->r.svFlags & SVF_ADMIN2) && (roar_adminControl2.integer & (1 << A_TERMINATOR))) {
			trap_SendServerCommand( ent-g_entities, va("print \"Terminator\n\"") ); }
		if ((ent->r.svFlags & SVF_ADMIN3) && (roar_adminControl3.integer & (1 << A_TERMINATOR))) {
			trap_SendServerCommand( ent-g_entities, va("print \"Terminator\n\"") ); }
		if ((ent->r.svFlags & SVF_ADMIN4) && (roar_adminControl4.integer & (1 << A_TERMINATOR))) {
			trap_SendServerCommand( ent-g_entities, va("print \"Terminator\n\"") ); }
		if ((ent->r.svFlags & SVF_ADMIN5) && (roar_adminControl5.integer & (1 << A_TERMINATOR))) {
			trap_SendServerCommand( ent-g_entities, va("print \"Terminator\n\"") ); }
		//DEMIGOD CHECK
		if ((ent->r.svFlags & SVF_ADMIN1) && (roar_adminControl1.integer & (1 << A_DEMIGOD))) {
			trap_SendServerCommand( ent-g_entities, va("print \"DemiGod\n\"") ); }
		if ((ent->r.svFlags & SVF_ADMIN2) && (roar_adminControl2.integer & (1 << A_DEMIGOD))) {
			trap_SendServerCommand( ent-g_entities, va("print \"DemiGod\n\"") ); }
		if ((ent->r.svFlags & SVF_ADMIN3) && (roar_adminControl3.integer & (1 << A_DEMIGOD))) {
			trap_SendServerCommand( ent-g_entities, va("print \"DemiGod\n\"") ); }
		if ((ent->r.svFlags & SVF_ADMIN4) && (roar_adminControl4.integer & (1 << A_DEMIGOD))) {
			trap_SendServerCommand( ent-g_entities, va("print \"DemiGod\n\"") ); }
		if ((ent->r.svFlags & SVF_ADMIN5) && (roar_adminControl5.integer & (1 << A_DEMIGOD))) {
			trap_SendServerCommand( ent-g_entities, va("print \"DemiGod\n\"") ); }
		//ADMINSIT CHECK
		if ((ent->r.svFlags & SVF_ADMIN1) && (roar_adminControl1.integer & (1 << A_ADMINSIT))) {
			trap_SendServerCommand( ent-g_entities, va("print \"AdminSit\n\"") ); }
		if ((ent->r.svFlags & SVF_ADMIN2) && (roar_adminControl2.integer & (1 << A_ADMINSIT))) {
			trap_SendServerCommand( ent-g_entities, va("print \"AdminSit\n\"") ); }
		if ((ent->r.svFlags & SVF_ADMIN3) && (roar_adminControl3.integer & (1 << A_ADMINSIT))) {
			trap_SendServerCommand( ent-g_entities, va("print \"AdminSit\n\"") ); }
		if ((ent->r.svFlags & SVF_ADMIN4) && (roar_adminControl4.integer & (1 << A_ADMINSIT))) {
			trap_SendServerCommand( ent-g_entities, va("print \"AdminSit\n\"") ); }
		if ((ent->r.svFlags & SVF_ADMIN5) && (roar_adminControl5.integer & (1 << A_ADMINSIT))) {
			trap_SendServerCommand( ent-g_entities, va("print \"AdminSit\n\"") ); }
		//SCALE CHECK
		if ((ent->r.svFlags & SVF_ADMIN1) && (roar_adminControl1.integer & (1 << A_SCALE))) {
			trap_SendServerCommand( ent-g_entities, va("print \"Scale\n\"") ); }
		if ((ent->r.svFlags & SVF_ADMIN2) && (roar_adminControl2.integer & (1 << A_SCALE))) {
			trap_SendServerCommand( ent-g_entities, va("print \"Scale\n\"") ); }
		if ((ent->r.svFlags & SVF_ADMIN3) && (roar_adminControl3.integer & (1 << A_SCALE))) {
			trap_SendServerCommand( ent-g_entities, va("print \"Scale\n\"") ); }
		if ((ent->r.svFlags & SVF_ADMIN4) && (roar_adminControl4.integer & (1 << A_SCALE))) {
			trap_SendServerCommand( ent-g_entities, va("print \"Scale\n\"") ); }
		if ((ent->r.svFlags & SVF_ADMIN5) && (roar_adminControl5.integer & (1 << A_SCALE))) {
			trap_SendServerCommand( ent-g_entities, va("print \"Scale\n\"") ); }
		//SPLAT CHECK
		if ((ent->r.svFlags & SVF_ADMIN1) && (roar_adminControl1.integer & (1 << A_SPLAT))) {
			trap_SendServerCommand( ent-g_entities, va("print \"Splat\n\"") ); }
		if ((ent->r.svFlags & SVF_ADMIN2) && (roar_adminControl2.integer & (1 << A_SPLAT))) {
			trap_SendServerCommand( ent-g_entities, va("print \"Splat\n\"") ); }
		if ((ent->r.svFlags & SVF_ADMIN3) && (roar_adminControl3.integer & (1 << A_SPLAT))) {
			trap_SendServerCommand( ent-g_entities, va("print \"Splat\n\"") ); }
		if ((ent->r.svFlags & SVF_ADMIN4) && (roar_adminControl4.integer & (1 << A_SPLAT))) {
			trap_SendServerCommand( ent-g_entities, va("print \"Splat\n\"") ); }
		if ((ent->r.svFlags & SVF_ADMIN5) && (roar_adminControl5.integer & (1 << A_SPLAT))) {
			trap_SendServerCommand( ent-g_entities, va("print \"Splat\n\"") ); }
		//SLAY CHECK
		if ((ent->r.svFlags & SVF_ADMIN1) && (roar_adminControl1.integer & (1 << A_SLAY))) {
			trap_SendServerCommand( ent-g_entities, va("print \"Slay\n\"") ); }
		if ((ent->r.svFlags & SVF_ADMIN2) && (roar_adminControl2.integer & (1 << A_SLAY))) {
			trap_SendServerCommand( ent-g_entities, va("print \"Slay\n\"") ); }
		if ((ent->r.svFlags & SVF_ADMIN3) && (roar_adminControl3.integer & (1 << A_SLAY))) {
			trap_SendServerCommand( ent-g_entities, va("print \"Slay\n\"") ); }
		if ((ent->r.svFlags & SVF_ADMIN4) && (roar_adminControl4.integer & (1 << A_SLAY))) {
			trap_SendServerCommand( ent-g_entities, va("print \"Slay\n\"") ); }
		if ((ent->r.svFlags & SVF_ADMIN5) && (roar_adminControl5.integer & (1 << A_SLAY))) {
			trap_SendServerCommand( ent-g_entities, va("print \"Slay\n\"") ); }
		//GRANTADMIN CHECK
		if ((ent->r.svFlags & SVF_ADMIN1) && (roar_adminControl1.integer & (1 << A_GRANTADMIN))) {
			trap_SendServerCommand( ent-g_entities, va("print \"GrantAdmin\n\"") ); }
		if ((ent->r.svFlags & SVF_ADMIN2) && (roar_adminControl2.integer & (1 << A_GRANTADMIN))) {
			trap_SendServerCommand( ent-g_entities, va("print \"GrantAdmin\n\"") ); }
		if ((ent->r.svFlags & SVF_ADMIN3) && (roar_adminControl3.integer & (1 << A_GRANTADMIN))) {
			trap_SendServerCommand( ent-g_entities, va("print \"GrantAdmin\n\"") ); }
		if ((ent->r.svFlags & SVF_ADMIN4) && (roar_adminControl4.integer & (1 << A_GRANTADMIN))) {
			trap_SendServerCommand( ent-g_entities, va("print \"GrantAdmin\n\"") ); }
		if ((ent->r.svFlags & SVF_ADMIN5) && (roar_adminControl5.integer & (1 << A_GRANTADMIN))) {
			trap_SendServerCommand( ent-g_entities, va("print \"GrantAdmin\n\"") ); }
		//CHANGEMAP CHECK
		if ((ent->r.svFlags & SVF_ADMIN1) && (roar_adminControl1.integer & (1 << A_CHANGEMAP))) {
			trap_SendServerCommand( ent-g_entities, va("print \"ChangeMap\n\"") ); }
		if ((ent->r.svFlags & SVF_ADMIN2) && (roar_adminControl2.integer & (1 << A_CHANGEMAP))) {
			trap_SendServerCommand( ent-g_entities, va("print \"ChangeMap\n\"") ); }
		if ((ent->r.svFlags & SVF_ADMIN3) && (roar_adminControl3.integer & (1 << A_CHANGEMAP))) {
			trap_SendServerCommand( ent-g_entities, va("print \"ChangeMap\n\"") ); }
		if ((ent->r.svFlags & SVF_ADMIN4) && (roar_adminControl4.integer & (1 << A_CHANGEMAP))) {
			trap_SendServerCommand( ent-g_entities, va("print \"ChangeMap\n\"") ); }
		if ((ent->r.svFlags & SVF_ADMIN5) && (roar_adminControl5.integer & (1 << A_CHANGEMAP))) {
			trap_SendServerCommand( ent-g_entities, va("print \"ChangeMap\n\"") ); }
		//EMPOWER CHECK
		if ((ent->r.svFlags & SVF_ADMIN1) && (roar_adminControl1.integer & (1 << A_EMPOWER))) {
			trap_SendServerCommand( ent-g_entities, va("print \"Empower\n\"") ); }
		if ((ent->r.svFlags & SVF_ADMIN2) && (roar_adminControl2.integer & (1 << A_EMPOWER))) {
			trap_SendServerCommand( ent-g_entities, va("print \"Empower\n\"") ); }
		if ((ent->r.svFlags & SVF_ADMIN3) && (roar_adminControl3.integer & (1 << A_EMPOWER))) {
			trap_SendServerCommand( ent-g_entities, va("print \"Empower\n\"") ); }
		if ((ent->r.svFlags & SVF_ADMIN4) && (roar_adminControl4.integer & (1 << A_EMPOWER))) {
			trap_SendServerCommand( ent-g_entities, va("print \"Empower\n\"") ); }
		if ((ent->r.svFlags & SVF_ADMIN5) && (roar_adminControl5.integer & (1 << A_EMPOWER))) {
			trap_SendServerCommand( ent-g_entities, va("print \"Empower\n\"") ); }
		//RENAME CHECK
		if ((ent->r.svFlags & SVF_ADMIN1) && (roar_adminControl1.integer & (1 << A_RENAME))) {
			trap_SendServerCommand( ent-g_entities, va("print \"Rename\n\"") ); }
		if ((ent->r.svFlags & SVF_ADMIN2) && (roar_adminControl2.integer & (1 << A_RENAME))) {
			trap_SendServerCommand( ent-g_entities, va("print \"Rename\n\"") ); }
		if ((ent->r.svFlags & SVF_ADMIN3) && (roar_adminControl3.integer & (1 << A_RENAME))) {
			trap_SendServerCommand( ent-g_entities, va("print \"Rename\n\"") ); }
		if ((ent->r.svFlags & SVF_ADMIN4) && (roar_adminControl4.integer & (1 << A_RENAME))) {
			trap_SendServerCommand( ent-g_entities, va("print \"Rename\n\"") ); }
		if ((ent->r.svFlags & SVF_ADMIN5) && (roar_adminControl5.integer & (1 << A_RENAME))) {
			trap_SendServerCommand( ent-g_entities, va("print \"Rename\n\"") ); }
		//LOCKNAME CHECK
		if ((ent->r.svFlags & SVF_ADMIN1) && (roar_adminControl1.integer & (1 << A_LOCKNAME))) {
			trap_SendServerCommand( ent-g_entities, va("print \"LockName\n\"") ); }
		if ((ent->r.svFlags & SVF_ADMIN2) && (roar_adminControl2.integer & (1 << A_LOCKNAME))) {
			trap_SendServerCommand( ent-g_entities, va("print \"LockName\n\"") ); }
		if ((ent->r.svFlags & SVF_ADMIN3) && (roar_adminControl3.integer & (1 << A_LOCKNAME))) {
			trap_SendServerCommand( ent-g_entities, va("print \"LockName\n\"") ); }
		if ((ent->r.svFlags & SVF_ADMIN4) && (roar_adminControl4.integer & (1 << A_LOCKNAME))) {
			trap_SendServerCommand( ent-g_entities, va("print \"LockName\n\"") ); }
		if ((ent->r.svFlags & SVF_ADMIN5) && (roar_adminControl5.integer & (1 << A_LOCKNAME))) {
			trap_SendServerCommand( ent-g_entities, va("print \"LockName\n\"") ); }
		//CSPRINT CHECK
		if ((ent->r.svFlags & SVF_ADMIN1) && (roar_adminControl1.integer & (1 << A_CSPRINT))) {
			trap_SendServerCommand( ent-g_entities, va("print \"CSPrint\n\"") ); }
		if ((ent->r.svFlags & SVF_ADMIN2) && (roar_adminControl2.integer & (1 << A_CSPRINT))) {
			trap_SendServerCommand( ent-g_entities, va("print \"CSPrint\n\"") ); }
		if ((ent->r.svFlags & SVF_ADMIN3) && (roar_adminControl3.integer & (1 << A_CSPRINT))) {
			trap_SendServerCommand( ent-g_entities, va("print \"CSPrint\n\"") ); }
		if ((ent->r.svFlags & SVF_ADMIN4) && (roar_adminControl4.integer & (1 << A_CSPRINT))) {
			trap_SendServerCommand( ent-g_entities, va("print \"CSPrint\n\"") ); }
		if ((ent->r.svFlags & SVF_ADMIN5) && (roar_adminControl5.integer & (1 << A_CSPRINT))) {
			trap_SendServerCommand( ent-g_entities, va("print \"CSPrint\n\"") ); }
		//FORCETEAM CHECK
		if ((ent->r.svFlags & SVF_ADMIN1) && (roar_adminControl1.integer & (1 << A_FORCETEAM))) {
			trap_SendServerCommand( ent-g_entities, va("print \"ForceTeam\n\"") ); }
		if ((ent->r.svFlags & SVF_ADMIN2) && (roar_adminControl2.integer & (1 << A_FORCETEAM))) {
			trap_SendServerCommand( ent-g_entities, va("print \"ForceTeam\n\"") ); }
		if ((ent->r.svFlags & SVF_ADMIN3) && (roar_adminControl3.integer & (1 << A_FORCETEAM))) {
			trap_SendServerCommand( ent-g_entities, va("print \"ForceTeam\n\"") ); }
		if ((ent->r.svFlags & SVF_ADMIN4) && (roar_adminControl4.integer & (1 << A_FORCETEAM))) {
			trap_SendServerCommand( ent-g_entities, va("print \"ForceTeam\n\"") ); }
		if ((ent->r.svFlags & SVF_ADMIN5) && (roar_adminControl5.integer & (1 << A_FORCETEAM))) {
			trap_SendServerCommand( ent-g_entities, va("print \"ForceTeam\n\"") ); }
		//RENAMECLAN CHECK
		if ((ent->r.svFlags & SVF_ADMIN1) && (roar_adminControl1.integer & (1 << A_RENAMECLAN))) {
			trap_SendServerCommand( ent-g_entities, va("print \"RenameClan\n\"") ); }
		if ((ent->r.svFlags & SVF_ADMIN2) && (roar_adminControl2.integer & (1 << A_RENAMECLAN))) {
			trap_SendServerCommand( ent-g_entities, va("print \"RenameClan\n\"") ); }
		if ((ent->r.svFlags & SVF_ADMIN3) && (roar_adminControl3.integer & (1 << A_RENAMECLAN))) {
			trap_SendServerCommand( ent-g_entities, va("print \"RenameClan\n\"") ); }
		if ((ent->r.svFlags & SVF_ADMIN4) && (roar_adminControl4.integer & (1 << A_RENAMECLAN))) {
			trap_SendServerCommand( ent-g_entities, va("print \"RenameClan\n\"") ); }
		if ((ent->r.svFlags & SVF_ADMIN5) && (roar_adminControl5.integer & (1 << A_RENAMECLAN))) {
			trap_SendServerCommand( ent-g_entities, va("print \"RenameClan\n\"") ); }
		//MONK CHECK
		if ((ent->r.svFlags & SVF_ADMIN1) && (roar_adminControl1.integer & (1 << A_HUMAN))) {
			trap_SendServerCommand( ent-g_entities, va("print \"Monk\n\"") ); }
		if ((ent->r.svFlags & SVF_ADMIN2) && (roar_adminControl2.integer & (1 << A_HUMAN))) {
			trap_SendServerCommand( ent-g_entities, va("print \"Monk\n\"") ); }
		if ((ent->r.svFlags & SVF_ADMIN3) && (roar_adminControl3.integer & (1 << A_HUMAN))) {
			trap_SendServerCommand( ent-g_entities, va("print \"Monk\n\"") ); }
		if ((ent->r.svFlags & SVF_ADMIN4) && (roar_adminControl4.integer & (1 << A_HUMAN))) {
			trap_SendServerCommand( ent-g_entities, va("print \"Monk\n\"") ); }
		if ((ent->r.svFlags & SVF_ADMIN5) && (roar_adminControl5.integer & (1 << A_HUMAN))) {
			trap_SendServerCommand( ent-g_entities, va("print \"Monk\n\"") ); }
		//WEATHER CHECK
		if ((ent->r.svFlags & SVF_ADMIN1) && (roar_adminControl1.integer & (1 << A_WEATHER))) {
			trap_SendServerCommand( ent-g_entities, va("print \"Weather\n\"") ); }
		if ((ent->r.svFlags & SVF_ADMIN2) && (roar_adminControl2.integer & (1 << A_WEATHER))) {
			trap_SendServerCommand( ent-g_entities, va("print \"Weather\n\"") ); }
		if ((ent->r.svFlags & SVF_ADMIN3) && (roar_adminControl3.integer & (1 << A_WEATHER))) {
			trap_SendServerCommand( ent-g_entities, va("print \"Weather\n\"") ); }
		if ((ent->r.svFlags & SVF_ADMIN4) && (roar_adminControl4.integer & (1 << A_WEATHER))) {
			trap_SendServerCommand( ent-g_entities, va("print \"Weather\n\"") ); }
		if ((ent->r.svFlags & SVF_ADMIN5) && (roar_adminControl5.integer & (1 << A_WEATHER))) {
			trap_SendServerCommand( ent-g_entities, va("print \"Weather\n\"") ); }
		//PLACE CHECK
		if ((ent->r.svFlags & SVF_ADMIN1) && (roar_adminControl1.integer & (1 << A_PLACE))) {
			trap_SendServerCommand( ent-g_entities, va("print \"Place\n\"") ); }
		if ((ent->r.svFlags & SVF_ADMIN2) && (roar_adminControl2.integer & (1 << A_PLACE))) {
			trap_SendServerCommand( ent-g_entities, va("print \"Place\n\"") ); }
		if ((ent->r.svFlags & SVF_ADMIN3) && (roar_adminControl3.integer & (1 << A_PLACE))) {
			trap_SendServerCommand( ent-g_entities, va("print \"Place\n\"") ); }
		if ((ent->r.svFlags & SVF_ADMIN4) && (roar_adminControl4.integer & (1 << A_PLACE))) {
			trap_SendServerCommand( ent-g_entities, va("print \"Place\n\"") ); }
		if ((ent->r.svFlags & SVF_ADMIN5) && (roar_adminControl5.integer & (1 << A_PLACE))) {
			trap_SendServerCommand( ent-g_entities, va("print \"Place\n\"") ); }
		//PUNISH CHECK
		if ((ent->r.svFlags & SVF_ADMIN1) && (roar_adminControl1.integer & (1 << A_PUNISH))) {
			trap_SendServerCommand( ent-g_entities, va("print \"Punish\n\"") ); }
		if ((ent->r.svFlags & SVF_ADMIN2) && (roar_adminControl2.integer & (1 << A_PUNISH))) {
			trap_SendServerCommand( ent-g_entities, va("print \"Punish\n\"") ); }
		if ((ent->r.svFlags & SVF_ADMIN3) && (roar_adminControl3.integer & (1 << A_PUNISH))) {
			trap_SendServerCommand( ent-g_entities, va("print \"Punish\n\"") ); }
		if ((ent->r.svFlags & SVF_ADMIN4) && (roar_adminControl4.integer & (1 << A_PUNISH))) {
			trap_SendServerCommand( ent-g_entities, va("print \"Punish\n\"") ); }
		if ((ent->r.svFlags & SVF_ADMIN5) && (roar_adminControl5.integer & (1 << A_PUNISH))) {
			trap_SendServerCommand( ent-g_entities, va("print \"Punish\n\"") ); }
		//SLEEP CHECK
		if ((ent->r.svFlags & SVF_ADMIN1) && (roar_adminControl1.integer & (1 << A_SLEEP))) {
			trap_SendServerCommand( ent-g_entities, va("print \"Sleep\n\"") ); }
		if ((ent->r.svFlags & SVF_ADMIN2) && (roar_adminControl2.integer & (1 << A_SLEEP))) {
			trap_SendServerCommand( ent-g_entities, va("print \"Sleep\n\"") ); }
		if ((ent->r.svFlags & SVF_ADMIN3) && (roar_adminControl3.integer & (1 << A_SLEEP))) {
			trap_SendServerCommand( ent-g_entities, va("print \"Sleep\n\"") ); }
		if ((ent->r.svFlags & SVF_ADMIN4) && (roar_adminControl4.integer & (1 << A_SLEEP))) {
			trap_SendServerCommand( ent-g_entities, va("print \"Sleep\n\"") ); }
		if ((ent->r.svFlags & SVF_ADMIN5) && (roar_adminControl5.integer & (1 << A_SLEEP))) {
			trap_SendServerCommand( ent-g_entities, va("print \"Sleep\n\"") ); }
		//SLAP CHECK
		if ((ent->r.svFlags & SVF_ADMIN1) && (roar_adminControl1.integer & (1 << A_SLAP))) {
			trap_SendServerCommand( ent-g_entities, va("print \"Slap\n\"") ); }
		if ((ent->r.svFlags & SVF_ADMIN2) && (roar_adminControl2.integer & (1 << A_SLAP))) {
			trap_SendServerCommand( ent-g_entities, va("print \"Slap\n\"") ); }
		if ((ent->r.svFlags & SVF_ADMIN3) && (roar_adminControl3.integer & (1 << A_SLAP))) {
			trap_SendServerCommand( ent-g_entities, va("print \"Slap\n\"") ); }
		if ((ent->r.svFlags & SVF_ADMIN4) && (roar_adminControl4.integer & (1 << A_SLAP))) {
			trap_SendServerCommand( ent-g_entities, va("print \"Slap\n\"") ); }
		if ((ent->r.svFlags & SVF_ADMIN5) && (roar_adminControl5.integer & (1 << A_SLAP))) {
			trap_SendServerCommand( ent-g_entities, va("print \"Slap\n\"") ); }
		//LOCKTEAM CHECK
		if ((ent->r.svFlags & SVF_ADMIN1) && (roar_adminControl1.integer & (1 << A_LOCKTEAM))) {
			trap_SendServerCommand( ent-g_entities, va("print \"LockTeam\n\"") ); }
		if ((ent->r.svFlags & SVF_ADMIN2) && (roar_adminControl2.integer & (1 << A_LOCKTEAM))) {
			trap_SendServerCommand( ent-g_entities, va("print \"LockTeam\n\"") ); }
		if ((ent->r.svFlags & SVF_ADMIN3) && (roar_adminControl3.integer & (1 << A_LOCKTEAM))) {
			trap_SendServerCommand( ent-g_entities, va("print \"LockTeam\n\"") ); }
		if ((ent->r.svFlags & SVF_ADMIN4) && (roar_adminControl4.integer & (1 << A_LOCKTEAM))) {
			trap_SendServerCommand( ent-g_entities, va("print \"LockTeam\n\"") ); }
		if ((ent->r.svFlags & SVF_ADMIN5) && (roar_adminControl5.integer & (1 << A_LOCKTEAM))) {
			trap_SendServerCommand( ent-g_entities, va("print \"LockTeam\n\"") ); }
		}
	//END
	else if (Q_stricmp(cmd, "togglechat") == 0){
		if (ent->client->pers.teamchat){
		ent->client->pers.clanchat = qtrue;
		ent->client->pers.teamchat = qfalse;
		ent->client->pers.adminchat = qfalse;
		trap_SendServerCommand( ent-g_entities, va("print \"^7TEAM CHAT MODE: ^1CLAN\n\"" ) );
		}
		else if (ent->client->pers.clanchat){
		ent->client->pers.clanchat = qfalse;
		ent->client->pers.teamchat = qfalse;
		ent->client->pers.adminchat = qtrue;
		trap_SendServerCommand( ent-g_entities, va("print \"^7TEAM CHAT MODE: ^3ADMIN\n\"" ) );
		}
		else if (ent->client->pers.adminchat){
		ent->client->pers.clanchat = qfalse;
		ent->client->pers.teamchat = qtrue;
		ent->client->pers.adminchat = qfalse;
		trap_SendServerCommand( ent-g_entities, va("print \"^7TEAM CHAT MODE: ^5TEAM\n\"" ) );
		}
	}
	//EMOTES BEGIN HERE
	else if (Q_stricmp(cmd, "myhead") == 0)
		{
			if (!(roar_emoteControl.integer & (1 << E_MYHEAD)))
		{
			trap_SendServerCommand( ent-g_entities, va("print \"This emote is not allowed on this server.\n\"") );
			return;
		}
			if (ent->client->ps.duelInProgress)
	{
		trap_SendServerCommand( ent-g_entities, va("print \"^7Emotes not allowed in duel!\n\"" ) );
		return;
	}
			else {
			G_Sound(ent, CHAN_AUTO, G_SoundIndex("sound/chars/humanmerc1/misc/escaping1"));
			StandardSetBodyAnim(ent, BOTH_SONICPAIN_HOLD, SETANIM_FLAG_OVERRIDE|SETANIM_FLAG_HOLD|SETANIM_FLAG_HOLDLESS);
		}
		}
		else if ((Q_stricmp(cmd, "cower") == 0) || (Q_stricmp(cmd, "amcower") == 0))
		{
			if (!(roar_emoteControl.integer & (1 << E_COWER)))
		{
			trap_SendServerCommand( ent-g_entities, va("print \"This emote is not allowed on this server.\n\"") );
			return;
		}
			if (ent->client->ps.groundEntityNum == ENTITYNUM_NONE)
			{
				return;
			}
			if (ent->client->ps.duelInProgress)
	{
		trap_SendServerCommand( ent-g_entities, va("print \"^7Emotes not allowed in duel!\n\"" ) );
		return;
	}
			else {
			StandardSetBodyAnim(ent, BOTH_COWER1, SETANIM_FLAG_OVERRIDE|SETANIM_FLAG_HOLD|SETANIM_FLAG_HOLDLESS);
			}
		}
				else if ((Q_stricmp(cmd, "smack") == 0 ) || (Q_stricmp(cmd, "amsmack") == 0 ))
		{
			if (!(roar_emoteControl.integer & (1 << E_SMACK)))
		{
			trap_SendServerCommand( ent-g_entities, va("print \"This emote is not allowed on this server.\n\"") );
			return;
		}
			if (ent->client->ps.duelInProgress)
	{
		trap_SendServerCommand( ent-g_entities, va("print \"^7Emotes not allowed in duel!\n\"" ) );
		return;
	}
			else {
			StandardSetBodyAnim(ent, BOTH_TOSS1, SETANIM_FLAG_OVERRIDE|SETANIM_FLAG_HOLD|SETANIM_FLAG_HOLDLESS);
			}
		}
		else if ((Q_stricmp(cmd, "enraged") == 0 ) || (Q_stricmp(cmd, "amenraged") == 0 ))
		{
			if (!(roar_emoteControl.integer & (1 << E_ENRAGED)))
		{
			trap_SendServerCommand( ent-g_entities, va("print \"This emote is not allowed on this server.\n\"") );
			return;
		}
			if (ent->client->ps.duelInProgress)
	{
		trap_SendServerCommand( ent-g_entities, va("print \"^7Emotes not allowed in duel!\n\"" ) );
		return;
	}
			else {
			StandardSetBodyAnim(ent, BOTH_FORCE_RAGE, SETANIM_FLAG_OVERRIDE|SETANIM_FLAG_HOLD|SETANIM_FLAG_HOLDLESS);
			}
		}
		else if ((Q_stricmp(cmd, "victory1") == 0 ) || (Q_stricmp(cmd, "amvictory1") == 0 ))
		{
			if (!(roar_emoteControl.integer & (1 << E_VICTORY1)))
		{
			trap_SendServerCommand( ent-g_entities, va("print \"This emote is not allowed on this server.\n\"") );
			return;
		}
			if (ent->client->ps.groundEntityNum == ENTITYNUM_NONE)
			{
				return;
			}
			if (ent->client->ps.duelInProgress)
	{
		trap_SendServerCommand( ent-g_entities, va("print \"^7Emotes not allowed in duel!\n\"" ) );
		return;
	}
			else {
			StandardSetBodyAnim(ent, BOTH_TAVION_SWORDPOWER, SETANIM_FLAG_OVERRIDE|SETANIM_FLAG_HOLD|SETANIM_FLAG_HOLDLESS);
			}
		}
		else if ((Q_stricmp(cmd, "victory2") == 0 ) || (Q_stricmp(cmd, "amvictory2") == 0 ))
		{
			if (!(roar_emoteControl.integer & (1 << E_VICTORY2)))
		{
			trap_SendServerCommand( ent-g_entities, va("print \"This emote is not allowed on this server.\n\"") );
			return;
		}
			if (ent->client->ps.groundEntityNum == ENTITYNUM_NONE)
			{
				return;
			}
			if (ent->client->ps.duelInProgress)
	{
		trap_SendServerCommand( ent-g_entities, va("print \"^7Emotes not allowed in duel!\n\"" ) );
		return;
	}
			else {
			StandardSetBodyAnim(ent, BOTH_TAVION_SCEPTERGROUND, SETANIM_FLAG_OVERRIDE|SETANIM_FLAG_HOLD|SETANIM_FLAG_HOLDLESS);
			}
		}
		else if ((Q_stricmp(cmd, "victory3") == 0 ) || (Q_stricmp(cmd, "amvictory3") == 0 ))
		{
			if (!(roar_emoteControl.integer & (1 << E_VICTORY3)))
		{
			trap_SendServerCommand( ent-g_entities, va("print \"This emote is not allowed on this server.\n\"") );
			return;
		}
			if (ent->client->ps.groundEntityNum == ENTITYNUM_NONE)
			{
				return;
			}
			if (ent->client->ps.duelInProgress)
	{
		trap_SendServerCommand( ent-g_entities, va("print \"^7Emotes not allowed in duel!\n\"" ) );
		return;
	}
			else {
			StandardSetBodyAnim(ent, BOTH_ALORA_TAUNT, SETANIM_FLAG_OVERRIDE|SETANIM_FLAG_HOLD|SETANIM_FLAG_HOLDLESS);
			}
		}
		else if ((Q_stricmp(cmd, "swirl") == 0 ) || (Q_stricmp(cmd, "amswirl") == 0 ))
		{
			if (!(roar_emoteControl.integer & (1 << E_SWIRL)))
		{
			trap_SendServerCommand( ent-g_entities, va("print \"This emote is not allowed on this server.\n\"") );
			return;
		}
			if (ent->client->ps.duelInProgress)
	{
		trap_SendServerCommand( ent-g_entities, va("print \"^7Emotes not allowed in duel!\n\"" ) );
		return;
	}
			else {
			StandardSetBodyAnim(ent, BOTH_CWCIRCLELOCK, SETANIM_FLAG_OVERRIDE|SETANIM_FLAG_HOLD|SETANIM_FLAG_HOLDLESS);
			}
		}
		else if ((Q_stricmp(cmd, "dance1") == 0 ) || (Q_stricmp(cmd, "amdance1") == 0 ))
		{
			if (!(roar_emoteControl.integer & (1 << E_DANCE1)))
		{
			trap_SendServerCommand( ent-g_entities, va("print \"This emote is not allowed on this server.\n\"") );
			return;
		}
			if (ent->client->ps.groundEntityNum == ENTITYNUM_NONE)
			{
				return;
			}
			if (ent->client->ps.duelInProgress)
	{
		trap_SendServerCommand( ent-g_entities, va("print \"^7Emotes not allowed in duel!\n\"" ) );
		return;
	}
			if ( !BG_SaberInAttack(ent->client->ps.saberMove) &&
				!BG_SaberInSpecialAttack(ent->client->ps.saberMove) &&
				!ent->client->ps.saberLockTime )
			{
				StandardSetBodyAnim(ent, BOTH_BUTTERFLY_LEFT, SETANIM_FLAG_OVERRIDE|SETANIM_FLAG_HOLD|SETANIM_FLAG_HOLDLESS);
			}
		}
		else if ((Q_stricmp(cmd, "dance2") == 0 ) || (Q_stricmp(cmd, "amdance2") == 0 ))
		{
			if (!(roar_emoteControl.integer & (1 << E_DANCE2)))
		{
			trap_SendServerCommand( ent-g_entities, va("print \"This emote is not allowed on this server.\n\"") );
			return;
		}
			if (ent->client->ps.groundEntityNum == ENTITYNUM_NONE)
			{
				return;
			}
			if (ent->client->ps.duelInProgress)
	{
		trap_SendServerCommand( ent-g_entities, va("print \"^7Emotes not allowed in duel!\n\"" ) );
		return;
	}
			if ( !BG_SaberInAttack(ent->client->ps.saberMove) &&
				!BG_SaberInSpecialAttack(ent->client->ps.saberMove) &&
				!ent->client->ps.saberLockTime )
			{
				StandardSetBodyAnim(ent, BOTH_BUTTERFLY_RIGHT, SETANIM_FLAG_OVERRIDE|SETANIM_FLAG_HOLD|SETANIM_FLAG_HOLDLESS);
			}
		}
		else if ((Q_stricmp(cmd, "dance3") == 0 ) || (Q_stricmp(cmd, "amdance3") == 0 ))
		{
			if (!(roar_emoteControl.integer & (1 << E_DANCE3)))
		{
			trap_SendServerCommand( ent-g_entities, va("print \"This emote is not allowed on this server.\n\"") );
			return;
		}
			if (ent->client->ps.groundEntityNum == ENTITYNUM_NONE)
			{
				return;
			}
			if (ent->client->ps.duelInProgress)
	{
		trap_SendServerCommand( ent-g_entities, va("print \"^7Emotes not allowed in duel!\n\"" ) );
		return;
	}
			if ( !BG_SaberInAttack(ent->client->ps.saberMove) &&
				!BG_SaberInSpecialAttack(ent->client->ps.saberMove) &&
				!ent->client->ps.saberLockTime )
			{
				StandardSetBodyAnim(ent, BOTH_FJSS_TR_BL, SETANIM_FLAG_OVERRIDE|SETANIM_FLAG_HOLD|SETANIM_FLAG_HOLDLESS);
			}
		}
		else if ((Q_stricmp(cmd, "sit5") == 0 ) || (Q_stricmp(cmd, "amsit5") == 0 ))
		{
			if (!(roar_emoteControl.integer & (1 << E_SIT5)))
		{
			trap_SendServerCommand( ent-g_entities, va("print \"This emote is not allowed on this server.\n\"") );
			return;
		}
			if (ent->client->ps.groundEntityNum == ENTITYNUM_NONE)
			{
				return;
			}
			if (ent->client->ps.duelInProgress)
	{
		trap_SendServerCommand( ent-g_entities, va("print \"^7Emotes not allowed in duel!\n\"" ) );
		return;
	}
			else {
			if (ent->client->ps.legsAnim == BOTH_SLEEP6START)
			{
                        ent->client->ps.forceDodgeAnim = 0;
                        ent->client->ps.forceHandExtendTime = 0;
				ent->client->emote_freeze=0;
				ent->client->ps.saberCanThrow = qtrue;
			}
			else
			{
            ent->client->ps.forceHandExtend = HANDEXTEND_TAUNT;
            ent->client->ps.forceDodgeAnim = BOTH_SLEEP6START;
            ent->client->ps.forceHandExtendTime = level.time + Q3_INFINITE;
				ent->client->ps.saberCanThrow = qfalse;
				ent->client->emote_freeze=1;
				StandardSetBodyAnim(ent, BOTH_SLEEP6START, SETANIM_FLAG_OVERRIDE|SETANIM_FLAG_HOLD|SETANIM_FLAG_HOLDLESS);
				ent->client->ps.saberMove = LS_NONE;
				ent->client->ps.saberBlocked = 0;
				ent->client->ps.saberBlocking = 0;
			}
			}
		}
		else if ((Q_stricmp(cmd, "sit6") == 0) || (Q_stricmp(cmd, "amsit6") == 0))
		{
			if (!(roar_emoteControl.integer & (1 << E_SIT4)))
			{
				trap_SendServerCommand(ent - g_entities, va("print \"This emote is not allowed on this server.\n\""));
				return;
			}
			if (ent->client->ps.groundEntityNum == ENTITYNUM_NONE)
			{
				return;
			}
			if (ent->client->ps.duelInProgress)
			{
				trap_SendServerCommand(ent - g_entities, va("print \"^7Emotes not allowed in duel!\n\""));
				return;
			}
			else {
				if (ent->client->ps.legsAnim == BOTH_SIT5)
				{
					ent->client->ps.forceDodgeAnim = 0;
					ent->client->ps.forceHandExtendTime = 0;
					ent->client->emote_freeze = 0;
					ent->client->ps.saberCanThrow = qtrue;
				}
				else
				{
					ent->client->ps.forceHandExtend = HANDEXTEND_TAUNT;
					ent->client->ps.forceDodgeAnim = BOTH_SIT5;
					ent->client->ps.forceHandExtendTime = level.time + Q3_INFINITE;
					ent->client->ps.saberCanThrow = qfalse;
					ent->client->emote_freeze = 1;
					StandardSetBodyAnim(ent, BOTH_SIT5, SETANIM_FLAG_OVERRIDE | SETANIM_FLAG_HOLD | SETANIM_FLAG_HOLDLESS);
					ent->client->ps.saberMove = LS_NONE;
					ent->client->ps.saberBlocked = 0;
					ent->client->ps.saberBlocking = 0;
			}
			}
		}
		else if ((Q_stricmp(cmd, "sit7") == 0) || (Q_stricmp(cmd, "amsit7") == 0))
		{
			if (!(roar_emoteControl.integer & (1 << E_SIT4)))
			{
				trap_SendServerCommand(ent - g_entities, va("print \"This emote is not allowed on this server.\n\""));
				return;
			}
			if (ent->client->ps.groundEntityNum == ENTITYNUM_NONE)
			{
				return;
			}
			if (ent->client->ps.duelInProgress)
			{
				trap_SendServerCommand(ent - g_entities, va("print \"^7Emotes not allowed in duel!\n\""));
				return;
			}
			else {
				if (ent->client->ps.legsAnim == BOTH_SIT4)
				{
					ent->client->ps.forceDodgeAnim = 0;
					ent->client->ps.forceHandExtendTime = 0;
					ent->client->emote_freeze = 0;
					ent->client->ps.saberCanThrow = qtrue;
				}
				else
				{
					ent->client->ps.forceHandExtend = HANDEXTEND_TAUNT;
					ent->client->ps.forceDodgeAnim = BOTH_SIT4;
					ent->client->ps.forceHandExtendTime = level.time + Q3_INFINITE;
					ent->client->ps.saberCanThrow = qfalse;
					ent->client->emote_freeze = 1;
					StandardSetBodyAnim(ent, BOTH_SIT4, SETANIM_FLAG_OVERRIDE | SETANIM_FLAG_HOLD | SETANIM_FLAG_HOLDLESS);
					ent->client->ps.saberMove = LS_NONE;
					ent->client->ps.saberBlocked = 0;
					ent->client->ps.saberBlocking = 0;
			}
			}
		}
		else if ((Q_stricmp(cmd, "kneel1") == 0) || (Q_stricmp(cmd, "amkneel1") == 0))
		{
			if (!(roar_emoteControl.integer & (1 << E_KNEEL1)))
		{
			trap_SendServerCommand( ent-g_entities, va("print \"This emote is not allowed on this server.\n\"") );
			return;
		}
			if (ent->client->ps.groundEntityNum == ENTITYNUM_NONE)
			{
				return;
			}
			if (ent->client->ps.duelInProgress)
	{
		trap_SendServerCommand( ent-g_entities, va("print \"^7Emotes not allowed in duel!\n\"" ) );
		return;
	}
			else {
			if (ent->client->ps.legsAnim == BOTH_KNEES2)
			{
                        ent->client->ps.forceDodgeAnim = 0;
                        ent->client->ps.forceHandExtendTime = 0;
				ent->client->emote_freeze=0;
				ent->client->ps.saberCanThrow = qtrue;
			}
			else
			{
            ent->client->ps.forceHandExtend = HANDEXTEND_TAUNT;
            ent->client->ps.forceDodgeAnim = BOTH_KNEES2;
            ent->client->ps.forceHandExtendTime = level.time + Q3_INFINITE;
				ent->client->ps.saberCanThrow = qfalse;
				ent->client->emote_freeze=1;
				StandardSetBodyAnim(ent, BOTH_KNEES2, SETANIM_FLAG_OVERRIDE|SETANIM_FLAG_HOLD|SETANIM_FLAG_HOLDLESS);
				ent->client->ps.saberMove = LS_NONE;
				ent->client->ps.saberBlocked = 0;
				ent->client->ps.saberBlocking = 0;
			}
			}
		}
		else if ((Q_stricmp(cmd, "kneel2") == 0) || (Q_stricmp(cmd, "amkneel2") == 0))
		{
			if (!(roar_emoteControl.integer & (1 << E_KNEEL2)))
		{
			trap_SendServerCommand( ent-g_entities, va("print \"This emote is not allowed on this server.\n\"") );
			return;
		}
			if (ent->client->ps.groundEntityNum == ENTITYNUM_NONE)
			{
				return;
			}
			if (ent->client->ps.duelInProgress)
	{
		trap_SendServerCommand( ent-g_entities, va("print \"^7Emotes not allowed in duel!\n\"" ) );
		return;
	}
			else {
			if (ent->client->ps.legsAnim == BOTH_ROSH_PAIN)
			{
                        ent->client->ps.forceDodgeAnim = 0;
                        ent->client->ps.forceHandExtendTime = 0;
				ent->client->emote_freeze=0;
				ent->client->ps.saberCanThrow = qtrue;
			}
			else
			{
            ent->client->ps.forceHandExtend = HANDEXTEND_TAUNT;
            ent->client->ps.forceDodgeAnim = BOTH_ROSH_PAIN;
            ent->client->ps.forceHandExtendTime = level.time + Q3_INFINITE;
				ent->client->ps.saberCanThrow = qfalse;
				ent->client->emote_freeze=1;
				StandardSetBodyAnim(ent, BOTH_ROSH_PAIN, SETANIM_FLAG_OVERRIDE|SETANIM_FLAG_HOLD|SETANIM_FLAG_HOLDLESS);
				ent->client->ps.saberMove = LS_NONE;
				ent->client->ps.saberBlocked = 0;
				ent->client->ps.saberBlocking = 0;
			}
			}
		}
			else if ((Q_stricmp(cmd, "kneel3") == 0) || (Q_stricmp(cmd, "amkneel3") == 0))
		{
			if (!(roar_emoteControl.integer & (1 << E_KNEEL3)))
		{
			trap_SendServerCommand( ent-g_entities, va("print \"This emote is not allowed on this server.\n\"") );
			return;
		}
			if (ent->client->ps.groundEntityNum == ENTITYNUM_NONE)
			{
				return;
			}
			if (ent->client->ps.duelInProgress)
	{
		trap_SendServerCommand( ent-g_entities, va("print \"^7Emotes not allowed in duel!\n\"" ) );
		return;
	}
			else {
			if (ent->client->ps.legsAnim == BOTH_CROUCH3)
			{
                        ent->client->ps.forceDodgeAnim = 0;
                        ent->client->ps.forceHandExtendTime = 0;
				ent->client->emote_freeze=0;
				ent->client->ps.saberCanThrow = qtrue;
			}
			else
			{
            ent->client->ps.forceHandExtend = HANDEXTEND_TAUNT;
            ent->client->ps.forceDodgeAnim = BOTH_CROUCH3;
            ent->client->ps.forceHandExtendTime = level.time + Q3_INFINITE;
				ent->client->ps.saberCanThrow = qfalse;
				ent->client->emote_freeze=1;
				StandardSetBodyAnim(ent, BOTH_CROUCH3, SETANIM_FLAG_OVERRIDE|SETANIM_FLAG_HOLD|SETANIM_FLAG_HOLDLESS);
				ent->client->ps.saberMove = LS_NONE;
				ent->client->ps.saberBlocked = 0;
				ent->client->ps.saberBlocking = 0;
			}
			}
		}
		else if ((Q_stricmp(cmd, "weather") == 0) || (Q_stricmp(cmd, "amweather") == 0) || (Q_stricmp(cmd, "setweather") == 0)){
		char   arg1[MAX_STRING_CHARS];
		trap_Argv( 1,  arg1, sizeof( arg1 ) );
		if (ent->r.svFlags & SVF_ADMIN1)
			{
				if (!(roar_adminControl1.integer & (1 << A_WEATHER)))
		{
			trap_SendServerCommand( ent-g_entities, va("print \"Weather is not allowed at this administration rank.\n\"") );
			return;
		}
			}
			else if (ent->r.svFlags & SVF_ADMIN2)
			{
				if (!(roar_adminControl2.integer & (1 << A_WEATHER)))
		{
			trap_SendServerCommand( ent-g_entities, va("print \"Weather is not allowed at this administration rank.\n\"") );
			return;
		}
			}
			if (ent->r.svFlags & SVF_ADMIN3)
			{
				if (!(roar_adminControl3.integer & (1 << A_WEATHER)))
		{
			trap_SendServerCommand( ent-g_entities, va("print \"Weather is not allowed at this administration rank.\n\"") );
			return;
		}
			}
			if (ent->r.svFlags & SVF_ADMIN4)
			{
				if (!(roar_adminControl4.integer & (1 << A_WEATHER)))
		{
			trap_SendServerCommand( ent-g_entities, va("print \"Weather is not allowed at this administration rank.\n\"") );
			return;
		}
			}
			if (ent->r.svFlags & SVF_ADMIN5)
			{
				if (!(roar_adminControl5.integer & (1 << A_WEATHER)))
		{
			trap_SendServerCommand( ent-g_entities, va("print \"Weather is not allowed at this administration rank.\n\"") );
			return;
		}
			}
			if (!(ent->r.svFlags & SVF_ADMIN1) && !(ent->r.svFlags & SVF_ADMIN2) && !(ent->r.svFlags & SVF_ADMIN3) && !(ent->r.svFlags & SVF_ADMIN4) && !(ent->r.svFlags & SVF_ADMIN5)){
				trap_SendServerCommand( ent-g_entities, "print \"Must login with /adminlogin (password)\n\"" );
				return;
			}
		if ( trap_Argc() != 2 )
         { 
			 trap_SendServerCommand( ent-g_entities, "print \"^5Usage: /weather (weather)\n^3WEATHERS = snow, rain\n^1WARNING: Once the weather is set, it requires a map restart to stop the weather!\n^1EXAMPLE: /weather snow\n\"" ); 
            return; 
         }
			if (Q_stricmp(arg1, "snow") == 0){
			SP_CreateSnow(ent);
			}
			else if (Q_stricmp(arg1, "rain") == 0){
			SP_CreateRain(ent);
			}
		}
		else if (Q_stricmp(cmd, "addeffect") == 0){
		char   arg1[MAX_STRING_CHARS];
		gentity_t *fx_runner = G_Spawn();
		trap_Argv( 1,  arg1, sizeof( arg1 ) );
		if (ent->r.svFlags & SVF_ADMIN1)
			{
				if (!(roar_adminControl1.integer & (1 << A_PLACE)))
		{
			trap_SendServerCommand( ent-g_entities, va("print \"Place is not allowed at this administration rank.\n\"") );
			return;
		}
			}
			else if (ent->r.svFlags & SVF_ADMIN2)
			{
				if (!(roar_adminControl2.integer & (1 << A_PLACE)))
		{
			trap_SendServerCommand( ent-g_entities, va("print \"Place is not allowed at this administration rank.\n\"") );
			return;
		}
			}
			if (ent->r.svFlags & SVF_ADMIN3)
			{
				if (!(roar_adminControl3.integer & (1 << A_PLACE)))
		{
			trap_SendServerCommand( ent-g_entities, va("print \"Place is not allowed at this administration rank.\n\"") );
			return;
		}
			}
			if (ent->r.svFlags & SVF_ADMIN4)
			{
				if (!(roar_adminControl4.integer & (1 << A_PLACE)))
		{
			trap_SendServerCommand( ent-g_entities, va("print \"Place is not allowed at this administration rank.\n\"") );
			return;
		}
			}
			if (ent->r.svFlags & SVF_ADMIN5)
			{
				if (!(roar_adminControl5.integer & (1 << A_PLACE)))
		{
			trap_SendServerCommand( ent-g_entities, va("print \"Place is not allowed at this administration rank.\n\"") );
			return;
		}
			}
			if (!(ent->r.svFlags & SVF_ADMIN1) && !(ent->r.svFlags & SVF_ADMIN2) && !(ent->r.svFlags & SVF_ADMIN3) && !(ent->r.svFlags & SVF_ADMIN4) && !(ent->r.svFlags & SVF_ADMIN5)){
				trap_SendServerCommand( ent-g_entities, "print \"Must login with /adminlogin (password)\n\"" );
				return;
			}
		if ( trap_Argc() != 2 )
         { 
			 trap_SendServerCommand( ent-g_entities, "print \"^5Usage: /addeffect (fxFile)\n^1WARNING: Once the effect is placed, it requires map restart to stop it!\n^1EXAMPLE: /addeffect env/small_fire\n\"" ); 
            return; 
         }
		 if ((ent->r.svFlags & SVF_ADMIN1) || (ent->r.svFlags & SVF_ADMIN2) || (ent->r.svFlags & SVF_ADMIN3) || (ent->r.svFlags & SVF_ADMIN4) || (ent->r.svFlags & SVF_ADMIN5)){
fx_runner->s.origin[2] = (int) ent->client->ps.origin[2];
fx_runner->s.origin[1] = (int) ent->client->ps.origin[1];
fx_runner->s.origin[0] = (int) ent->client->ps.origin[0];
AddSpawnField("fxFile", arg1);
SP_fx_runner(fx_runner);
		 }
			}
		else if ((Q_stricmp(cmd, "slap") == 0) || (Q_stricmp(cmd, "amslap") == 0)){
			int client_id = -1; 
			char   arg1[MAX_STRING_CHARS];
			if (ent->r.svFlags & SVF_ADMIN1)
			{
				if (!(roar_adminControl1.integer & (1 << A_SLAP)))
		{
			trap_SendServerCommand( ent-g_entities, va("print \"Slap is not allowed at this administration rank.\n\"") );
			return;
		}
			}
			else if (ent->r.svFlags & SVF_ADMIN2)
			{
				if (!(roar_adminControl2.integer & (1 << A_SLAP)))
		{
			trap_SendServerCommand( ent-g_entities, va("print \"Slap is not allowed at this administration rank.\n\"") );
			return;
		}
			}
			if (ent->r.svFlags & SVF_ADMIN3)
			{
				if (!(roar_adminControl3.integer & (1 << A_SLAP)))
		{
			trap_SendServerCommand( ent-g_entities, va("print \"Slap is not allowed at this administration rank.\n\"") );
			return;
		}
			}
			if (ent->r.svFlags & SVF_ADMIN4)
			{
				if (!(roar_adminControl4.integer & (1 << A_SLAP)))
		{
			trap_SendServerCommand( ent-g_entities, va("print \"Slap is not allowed at this administration rank.\n\"") );
			return;
		}
			}
			if (ent->r.svFlags & SVF_ADMIN5)
			{
				if (!(roar_adminControl5.integer & (1 << A_SLAP)))
		{
			trap_SendServerCommand( ent-g_entities, va("print \"Slap is not allowed at this administration rank.\n\"") );
			return;
		}
			}
			if (!(ent->r.svFlags & SVF_ADMIN1) && !(ent->r.svFlags & SVF_ADMIN2) && !(ent->r.svFlags & SVF_ADMIN3) && !(ent->r.svFlags & SVF_ADMIN4) && !(ent->r.svFlags & SVF_ADMIN5)){
				trap_SendServerCommand( ent-g_entities, "print \"Must login with /adminlogin (password)\n\"" );
				return;
			}
			if ( trap_Argc() != 2 )
			{
				trap_SendServerCommand( ent-g_entities, "print \"Usage: ^3/slap (client)\n\"" );
				return;
			}
			trap_Argv( 1, arg1, sizeof( arg1 ) );
			client_id = G_ClientNumberFromArg(  arg1 );
			if (client_id == -1) 
         { 
            trap_SendServerCommand( ent-g_entities, va("print \"Can't find client ID for %s\n\"", arg1 ) ); 
            return; 
         } 
         if (client_id == -2) 
         { 
            trap_SendServerCommand( ent-g_entities, va("print \"Ambiguous client ID for %s\n\"", arg1 ) ); 
            return; 
         } 
         if (!g_entities[client_id].inuse) 
         {
            trap_SendServerCommand( ent-g_entities, va("print \"Client %s is not active\n\"", arg1 ) ); 
            return; 
         }
		 if(g_entities[client_id].client->ps.duelInProgress){
			 trap_SendServerCommand( ent-g_entities, va("print \"You cannot slap someone who is currently dueling.\n\"") ); 
			return;
		 }
		g_entities[client_id].client->ps.forceHandExtend = HANDEXTEND_KNOCKDOWN;
		g_entities[client_id].client->ps.forceHandExtendTime = level.time + 3000;
		g_entities[client_id].client->ps.velocity[2] += 500;
		g_entities[client_id].client->ps.forceDodgeAnim = 0;
		g_entities[client_id].client->ps.quickerGetup = qfalse;
		trap_SendServerCommand( -1, va("cp \"%s\n^7%s\n\"", g_entities[client_id].client->pers.netname, roar_slap_saying.string ) );
			trap_SendServerCommand( -1, va("print \"%s ^7%s\n\"", g_entities[client_id].client->pers.netname, roar_slap_saying.string ) );
		}
		else if ((Q_stricmp(cmd, "sleep") == 0) || (Q_stricmp(cmd, "amsleep") == 0)){
			gentity_t * targetplayer;
		  int	i;
			int client_id = -1; 
			char   arg1[MAX_STRING_CHARS];
			if (ent->r.svFlags & SVF_ADMIN1)
			{
				if (!(roar_adminControl1.integer & (1 << A_SLEEP)))
		{
			trap_SendServerCommand( ent-g_entities, va("print \"Sleep is not allowed at this administration rank.\n\"") );
			return;
		}
			}
			else if (ent->r.svFlags & SVF_ADMIN2)
			{
				if (!(roar_adminControl2.integer & (1 << A_SLEEP)))
		{
			trap_SendServerCommand( ent-g_entities, va("print \"Sleep is not allowed at this administration rank.\n\"") );
			return;
		}
			}
			if (ent->r.svFlags & SVF_ADMIN3)
			{
				if (!(roar_adminControl3.integer & (1 << A_SLEEP)))
		{
			trap_SendServerCommand( ent-g_entities, va("print \"Sleep is not allowed at this administration rank.\n\"") );
			return;
		}
			}
			if (ent->r.svFlags & SVF_ADMIN4)
			{
				if (!(roar_adminControl4.integer & (1 << A_SLEEP)))
		{
			trap_SendServerCommand( ent-g_entities, va("print \"Sleep is not allowed at this administration rank.\n\"") );
			return;
		}
			}
			if (ent->r.svFlags & SVF_ADMIN5)
			{
				if (!(roar_adminControl5.integer & (1 << A_SLEEP)))
		{
			trap_SendServerCommand( ent-g_entities, va("print \"Sleep is not allowed at this administration rank.\n\"") );
			return;
		}
			}
			if (!(ent->r.svFlags & SVF_ADMIN1) && !(ent->r.svFlags & SVF_ADMIN2) && !(ent->r.svFlags & SVF_ADMIN3) && !(ent->r.svFlags & SVF_ADMIN4) && !(ent->r.svFlags & SVF_ADMIN5)){
				trap_SendServerCommand( ent-g_entities, "print \"Must login with /adminlogin (password)\n\"" );
				return;
			}
			if ( trap_Argc() != 2 )
			{
				trap_SendServerCommand( ent-g_entities, "print \"Usage: ^3/sleep (client)\n\"" );
				return;
			}
			trap_Argv( 1, arg1, sizeof( arg1 ) );
			 if( Q_stricmp( arg1, "+all" ) == 0 ){
			 for( i = 0; i < level.maxclients; i++ )
		{
			targetplayer = &g_entities[i];

			if( targetplayer->client && targetplayer->client->pers.connected ){
			targetplayer->client->pers.amsleep = 1;
			targetplayer->client->pers.tzone = 0;
			targetplayer->flags |= FL_GODMODE;
			targetplayer->client->ps.forceHandExtendTime = Q3_INFINITE;
			targetplayer->client->ps.forceDodgeAnim = 0;
			}
		}
		trap_SendServerCommand( -1, va("print \"^7%s\n\"", roar_sleep_on_ALL_saying.string ) );
		trap_SendServerCommand( -1, va("cp \"^7%s\n\"", roar_sleep_on_ALL_saying.string ) );
		return;
		 }
		 if( Q_stricmp( arg1, "-all" ) == 0 ){
			 for( i = 0; i < level.maxclients; i++ )
		{
			targetplayer = &g_entities[i];

			if( targetplayer->client && targetplayer->client->pers.connected ){
			targetplayer->client->pers.amsleep = 0;
			targetplayer->takedamage = qtrue;
			targetplayer->flags &= ~FL_GODMODE;
			targetplayer->client->ps.forceHandExtendTime = 0;
			}
		}
		trap_SendServerCommand( -1, va("print \"^7%s\n\"", roar_sleep_off_ALL_saying.string ) );
		trap_SendServerCommand( -1, va("cp \"^7%s\n\"", roar_sleep_off_ALL_saying.string ) );
		return;
		 }
		 else {
			client_id = G_ClientNumberFromArg(  arg1 );
			}
			if (client_id == -1) 
         { 
            trap_SendServerCommand( ent-g_entities, va("print \"Can't find client ID for %s\n\"", arg1 ) ); 
            return; 
         } 
         if (client_id == -2) 
         { 
            trap_SendServerCommand( ent-g_entities, va("print \"Ambiguous client ID for %s\n\"", arg1 ) ); 
            return; 
         } 
         // either we have the client id or the string did not match 
         if (!g_entities[client_id].inuse) 
         { // check to make sure client slot is in use 
            trap_SendServerCommand( ent-g_entities, va("print \"Client %s is not active\n\"", arg1 ) ); 
            return; 
         }
		 if (g_entities[client_id].client->pers.ampunish == 1){
			trap_SendServerCommand( ent-g_entities, va("print \"Client is currently being punished. Please unpunish them before sleeping them.\nTo unpunish them, type in /punish again on the client.\n\"" ) );
			 return;
		 }
		 if (g_entities[client_id].client->pers.amfreeze == 1){
			trap_SendServerCommand( ent-g_entities, va("print \"Client is currently frozen. Please unfreeze them before sleeping them.\nTo unfreeze them, type in /freeze again on the client.\n\"" ) );
			 return;
		 }
		 if (g_entities[client_id].client->pers.amdemigod == 1){
			trap_SendServerCommand( ent-g_entities, va("print \"Client is currently a demigod. Please undemigod them before sleeping them.\nTo undemigod them, type in /demigod again on the client.\n\"" ) );
			 return;
		 }
			if (g_entities[client_id].client->pers.amsleep == 1)
			{
			trap_SendServerCommand( -1, va("cp \"%s\n^7%s\n\"", g_entities[client_id].client->pers.netname, roar_sleep_off_saying.string ) );
			trap_SendServerCommand( -1, va("print \"%s ^7%s\n\"", g_entities[client_id].client->pers.netname, roar_sleep_off_saying.string ) );
			g_entities[client_id].client->pers.amsleep = 0;
			if (g_entities[client_id].client->pers.tzone == 0){
			g_entities[client_id].takedamage = qtrue;
			}
			g_entities[client_id].flags &= ~FL_GODMODE;
			g_entities[client_id].client->ps.forceHandExtendTime = 0;
			}
			else
			{
			trap_SendServerCommand( -1, va("cp \"%s\n^7%s\n\"", g_entities[client_id].client->pers.netname, roar_sleep_on_saying.string ) );
			trap_SendServerCommand( -1, va("print \"%s ^7%s\n\"", g_entities[client_id].client->pers.netname, roar_sleep_on_saying.string ) );
			g_entities[client_id].client->pers.amsleep = 1;
			g_entities[client_id].client->ps.forceHandExtendTime = Q3_INFINITE;
			g_entities[client_id].client->ps.forceDodgeAnim = 0;
			}
		}
		else if ((Q_stricmp(cmd, "punish") == 0) || (Q_stricmp(cmd, "ampunish") == 0)){
		int client_id = -1; 
			char   arg1[MAX_STRING_CHARS];
			if (ent->r.svFlags & SVF_ADMIN1)
			{
				if (!(roar_adminControl1.integer & (1 << A_PUNISH)))
		{
			trap_SendServerCommand( ent-g_entities, va("print \"Punish is not allowed at this administration rank.\n\"") );
			return;
		}
			}
			else if (ent->r.svFlags & SVF_ADMIN2)
			{
				if (!(roar_adminControl2.integer & (1 << A_PUNISH)))
		{
			trap_SendServerCommand( ent-g_entities, va("print \"Punish is not allowed at this administration rank.\n\"") );
			return;
		}
			}
			if (ent->r.svFlags & SVF_ADMIN3)
			{
				if (!(roar_adminControl3.integer & (1 << A_PUNISH)))
		{
			trap_SendServerCommand( ent-g_entities, va("print \"Punish is not allowed at this administration rank.\n\"") );
			return;
		}
			}
			if (ent->r.svFlags & SVF_ADMIN4)
			{
				if (!(roar_adminControl4.integer & (1 << A_PUNISH)))
		{
			trap_SendServerCommand( ent-g_entities, va("print \"Punish is not allowed at this administration rank.\n\"") );
			return;
		}
			}
			if (ent->r.svFlags & SVF_ADMIN5)
			{
				if (!(roar_adminControl5.integer & (1 << A_PUNISH)))
		{
			trap_SendServerCommand( ent-g_entities, va("print \"Punish is not allowed at this administration rank.\n\"") );
			return;
		}
			}
			if (!(ent->r.svFlags & SVF_ADMIN1) && !(ent->r.svFlags & SVF_ADMIN2) && !(ent->r.svFlags & SVF_ADMIN3) && !(ent->r.svFlags & SVF_ADMIN4) && !(ent->r.svFlags & SVF_ADMIN5)){
				trap_SendServerCommand( ent-g_entities, "print \"Must login with /adminlogin (password)\n\"" );
				return;
			}
			if ( trap_Argc() != 2 )
			{
				trap_SendServerCommand( ent-g_entities, "print \"Usage: ^3/punish (client)\n\"" );
				return;
			}
			trap_Argv( 1, arg1, sizeof( arg1 ) );
			client_id = G_ClientNumberFromArg(  arg1 );
			if (client_id == -1) 
         { 
            trap_SendServerCommand( ent-g_entities, va("print \"Can't find client ID for %s\n\"", arg1 ) ); 
            return; 
         } 
         if (client_id == -2) 
         { 
            trap_SendServerCommand( ent-g_entities, va("print \"Ambiguous client ID for %s\n\"", arg1 ) ); 
            return; 
         } 
         // either we have the client id or the string did not match 
         if (!g_entities[client_id].inuse) 
         { // check to make sure client slot is in use 
            trap_SendServerCommand( ent-g_entities, va("print \"Client %s is not active\n\"", arg1 ) ); 
            return; 
         }
		 if (g_entities[client_id].client->pers.amsleep == 1){
			trap_SendServerCommand( ent-g_entities, va("print \"Client is currently sleeping. Please unsleep them before punishing them.\nTo wake them, type in /sleep again on the client.\n\"" ) );
			 return;
		 }
		 if (g_entities[client_id].client->pers.amfreeze == 1){
			trap_SendServerCommand( ent-g_entities, va("print \"Client is currently frozen. Please unfreeze them before punishing them.\nTo unfreeze them, type in /freeze again on the client.\n\"" ) );
			 return;
		 }
		 if (g_entities[client_id].client->pers.amdemigod == 1){
			trap_SendServerCommand( ent-g_entities, va("print \"Client is currently a demigod. Please undemigod them before punishing them.\nTo undemigod them, type in /demigod again on the client.\n\"" ) );
			 return;
		 }
			if (g_entities[client_id].client->pers.ampunish == 1)
			{
			g_entities[client_id].client->pers.ampunish = 0;
			g_entities[client_id].takedamage = qtrue;
			g_entities[client_id].flags &= ~FL_GODMODE;
			trap_SendServerCommand( -1, va("cp \"%s\n^7%s\n\"", g_entities[client_id].client->pers.netname, roar_punish_off_saying.string ) );
			trap_SendServerCommand( -1, va("print \"%s ^7%s\n\"", g_entities[client_id].client->pers.netname, roar_punish_off_saying.string ) );
			}
			else {
			g_entities[client_id].client->pers.ampunish = 1;
			trap_SendServerCommand( -1, va("cp \"%s\n^7%s\n\"", g_entities[client_id].client->pers.netname, roar_punish_on_saying.string ) );
			trap_SendServerCommand( -1, va("print \"%s ^7%s\n\"", g_entities[client_id].client->pers.netname, roar_punish_on_saying.string ) );
			}
		}
		else if ((Q_stricmp(cmd, "laydown") == 0) || (Q_stricmp(cmd, "amlaydown") == 0))
		{
			if (!(roar_emoteControl.integer & (1 << E_SLEEP)))
		{
			trap_SendServerCommand( ent-g_entities, va("print \"This emote is not allowed on this server.\n\"") );
			return;
		}
			if (ent->client->ps.groundEntityNum == ENTITYNUM_NONE)
			{
				return;
			}
			if (ent->client->ps.duelInProgress)
	{
		trap_SendServerCommand( ent-g_entities, va("print \"^7Emotes not allowed in duel!\n\"" ) );
		return;
	}
			else {
			if (ent->client->ps.legsAnim == BOTH_SLEEP1)
			{
                        ent->client->ps.forceDodgeAnim = 0;
                        ent->client->ps.forceHandExtendTime = 0;
				ent->client->emote_freeze=0;
				ent->client->ps.saberCanThrow = qtrue;
			}
			else
			{
            ent->client->ps.forceHandExtend = HANDEXTEND_TAUNT;
            ent->client->ps.forceDodgeAnim = BOTH_SLEEP1;
            ent->client->ps.forceHandExtendTime = level.time + Q3_INFINITE;
				ent->client->ps.saberCanThrow = qfalse;
				ent->client->emote_freeze=1;
				StandardSetBodyAnim(ent, BOTH_SLEEP1, SETANIM_FLAG_OVERRIDE|SETANIM_FLAG_HOLD|SETANIM_FLAG_HOLDLESS);
				ent->client->ps.saberMove = LS_NONE;
				ent->client->ps.saberBlocked = 0;
				ent->client->ps.saberBlocking = 0;
			}
			}
		}
		else if ((Q_stricmp(cmd, "breakdance") == 0 ) || (Q_stricmp(cmd, "ambreakdance") == 0 ))
		{
			if (!(roar_emoteControl.integer & (1 << E_BREAKDANCE)))
		{
			trap_SendServerCommand( ent-g_entities, va("print \"This emote is not allowed on this server.\n\"") );
			return;
		}
			if (ent->client->ps.duelInProgress)
	{
		trap_SendServerCommand( ent-g_entities, va("print \"^7Emotes not allowed in duel!\n\"" ) );
		return;
	}
			else {
			StandardSetBodyAnim(ent, BOTH_BACK_FLIP_UP, SETANIM_FLAG_OVERRIDE|SETANIM_FLAG_HOLD|SETANIM_FLAG_HOLDLESS);
			ent->client->ps.saberMove = LS_NONE;
			ent->client->ps.saberBlocked = 0;
			ent->client->ps.saberBlocking = 0;
			}
		}
		else if ((Q_stricmp(cmd, "cheer") == 0 ) || (Q_stricmp(cmd, "amcheer") == 0 ))
		{
			if (!(roar_emoteControl.integer & (1 << E_CHEER)))
		{
			trap_SendServerCommand( ent-g_entities, va("print \"This emote is not allowed on this server.\n\"") );
			return;
		}
			if (ent->client->ps.duelInProgress)
	{
		trap_SendServerCommand( ent-g_entities, va("print \"^7Emotes not allowed in duel!\n\"" ) );
		return;
	}
			else {
			StandardSetBodyAnim(ent, BOTH_TUSKENTAUNT1, SETANIM_FLAG_OVERRIDE|SETANIM_FLAG_HOLD|SETANIM_FLAG_HOLDLESS);
			ent->client->ps.saberMove = LS_NONE;
			ent->client->ps.saberBlocked = 0;
			ent->client->ps.saberBlocking = 0;
			}
		}
		else if ((Q_stricmp(cmd, "surrender") == 0 ) || (Q_stricmp(cmd, "amsurrender") == 0 ))
		{
			if (!(roar_emoteControl.integer & (1 << E_SURRENDER)))
		{
			trap_SendServerCommand( ent-g_entities, va("print \"This emote is not allowed on this server.\n\"") );
			return;
		}
			if (ent->client->ps.groundEntityNum == ENTITYNUM_NONE)
			{
				return;
			}
			if (ent->client->ps.duelInProgress)
	{
		trap_SendServerCommand( ent-g_entities, va("print \"^7Emotes not allowed in duel!\n\"" ) );
		return;
	}
			else {
			if (ent->client->ps.legsAnim == TORSO_SURRENDER_START)
			{
                        ent->client->ps.forceDodgeAnim = 0;
                        ent->client->ps.forceHandExtendTime = 0;
				ent->client->emote_freeze=0;
				ent->client->ps.saberCanThrow = qtrue;
			}
			else
			{
            ent->client->ps.forceHandExtend = HANDEXTEND_TAUNT;
            ent->client->ps.forceDodgeAnim = TORSO_SURRENDER_START;
            ent->client->ps.forceHandExtendTime = level.time + Q3_INFINITE;
				ent->client->ps.saberCanThrow = qfalse;
				ent->client->emote_freeze=1;
				StandardSetBodyAnim(ent, TORSO_SURRENDER_START, SETANIM_FLAG_OVERRIDE|SETANIM_FLAG_HOLD|SETANIM_FLAG_HOLDLESS);
				ent->client->ps.saberMove = LS_NONE;
				ent->client->ps.saberBlocked = 0;
				ent->client->ps.saberBlocking = 0;
			}
			}
		}
			else if ((Q_stricmp(cmd, "sit1") == 0 ) || (Q_stricmp(cmd, "amsit1") == 0 ))
		{
			if (!(roar_emoteControl.integer & (1 << E_SIT1)))
		{
			trap_SendServerCommand( ent-g_entities, va("print \"This emote is not allowed on this server.\n\"") );
			return;
		}
			if (ent->client->ps.groundEntityNum == ENTITYNUM_NONE)
			{
				return;
			}
			if (ent->client->ps.duelInProgress)
	{
		trap_SendServerCommand( ent-g_entities, va("print \"^7Emotes not allowed in duel!\n\"" ) );
		return;
	}
			else {
			if (ent->client->ps.legsAnim == BOTH_SIT1)
			{
                        ent->client->ps.forceDodgeAnim = 0;
                        ent->client->ps.forceHandExtendTime = 0;
				ent->client->emote_freeze=0;
				ent->client->ps.saberCanThrow = qtrue;
			}
			else
			{
            ent->client->ps.forceHandExtend = HANDEXTEND_TAUNT;
            ent->client->ps.forceDodgeAnim = BOTH_SIT1;
            ent->client->ps.forceHandExtendTime = level.time + Q3_INFINITE;
				ent->client->ps.saberCanThrow = qfalse;
				ent->client->emote_freeze=1;
				StandardSetBodyAnim(ent, BOTH_SIT1, SETANIM_FLAG_OVERRIDE|SETANIM_FLAG_HOLD|SETANIM_FLAG_HOLDLESS);
				ent->client->ps.saberMove = LS_NONE;
				ent->client->ps.saberBlocked = 0;
				ent->client->ps.saberBlocking = 0;
			}
			}
		}

		else if ((Q_stricmp(cmd, "sit2") == 0) || (Q_stricmp(cmd, "amsit2") == 0))
		{
			if (!(roar_emoteControl.integer & (1 << E_SIT2)))
		{
			trap_SendServerCommand( ent-g_entities, va("print \"This emote is not allowed on this server.\n\"") );
			return;
		}
			if (ent->client->ps.groundEntityNum == ENTITYNUM_NONE)
			{
				return;
			}
			if (ent->client->ps.duelInProgress)
	{
		trap_SendServerCommand( ent-g_entities, va("print \"^7Emotes not allowed in duel!\n\"" ) );
		return;
	}
			else {
			if (ent->client->ps.legsAnim == BOTH_SIT2)
			{
                        ent->client->ps.forceDodgeAnim = 0;
                        ent->client->ps.forceHandExtendTime = 0;
				ent->client->emote_freeze=0;
				ent->client->ps.saberCanThrow = qtrue;
			}
			else
			{
            ent->client->ps.forceHandExtend = HANDEXTEND_TAUNT;
            ent->client->ps.forceDodgeAnim = BOTH_SIT2;
            ent->client->ps.forceHandExtendTime = level.time + Q3_INFINITE;
				ent->client->ps.saberCanThrow = qfalse;
				ent->client->emote_freeze=1;
				StandardSetBodyAnim(ent, BOTH_SIT2, SETANIM_FLAG_OVERRIDE|SETANIM_FLAG_HOLD|SETANIM_FLAG_HOLDLESS);
				ent->client->ps.saberMove = LS_NONE;
				ent->client->ps.saberBlocked = 0;
				ent->client->ps.saberBlocking = 0;
			}
			}
		}

		else if ((Q_stricmp(cmd, "sit3") == 0) || (Q_stricmp(cmd, "amsit3") == 0))
		{
			if (!(roar_emoteControl.integer & (1 << E_SIT3)))
		{
			trap_SendServerCommand( ent-g_entities, va("print \"This emote is not allowed on this server.\n\"") );
			return;
		}
			if (ent->client->ps.groundEntityNum == ENTITYNUM_NONE)
			{
				return;
			}
			if (ent->client->ps.duelInProgress)
	{
		trap_SendServerCommand( ent-g_entities, va("print \"^7Emotes not allowed in duel!\n\"" ) );
		return;
	}
			else {
			if (ent->client->ps.legsAnim == BOTH_SIT3)
			{
                        ent->client->ps.forceDodgeAnim = 0;
                        ent->client->ps.forceHandExtendTime = 0;
				ent->client->emote_freeze=0;
				ent->client->ps.saberCanThrow = qtrue;
			}
			else
			{
            ent->client->ps.forceHandExtend = HANDEXTEND_TAUNT;
            ent->client->ps.forceDodgeAnim = BOTH_SIT3;
            ent->client->ps.forceHandExtendTime = level.time + Q3_INFINITE;
				ent->client->ps.saberCanThrow = qfalse;
				ent->client->emote_freeze=1;
				StandardSetBodyAnim(ent, BOTH_SIT3, SETANIM_FLAG_OVERRIDE|SETANIM_FLAG_HOLD|SETANIM_FLAG_HOLDLESS);
				ent->client->ps.saberMove = LS_NONE;
				ent->client->ps.saberBlocked = 0;
				ent->client->ps.saberBlocking = 0;
			}
			}
		}
		else if ((Q_stricmp(cmd, "sit4") == 0) || (Q_stricmp(cmd, "amsit4") == 0))
		{
			if (!(roar_emoteControl.integer & (1 << E_SIT4)))
		{
			trap_SendServerCommand( ent-g_entities, va("print \"This emote is not allowed on this server.\n\"") );
			return;
		}
			if (ent->client->ps.groundEntityNum == ENTITYNUM_NONE)
			{
				return;
			}
			if (ent->client->ps.duelInProgress)
	{
		trap_SendServerCommand( ent-g_entities, va("print \"^7Emotes not allowed in duel!\n\"" ) );
		return;
	}
			else {
			if (ent->client->ps.legsAnim == BOTH_SIT6)
			{
                        ent->client->ps.forceDodgeAnim = 0;
                        ent->client->ps.forceHandExtendTime = 0;
				ent->client->emote_freeze=0;
				ent->client->ps.saberCanThrow = qtrue;
			}
			else
			{
            ent->client->ps.forceHandExtend = HANDEXTEND_TAUNT;
            ent->client->ps.forceDodgeAnim = BOTH_SIT6;
            ent->client->ps.forceHandExtendTime = level.time + Q3_INFINITE;
				ent->client->ps.saberCanThrow = qfalse;
				ent->client->emote_freeze=1;
				StandardSetBodyAnim(ent, BOTH_SIT6, SETANIM_FLAG_OVERRIDE|SETANIM_FLAG_HOLD|SETANIM_FLAG_HOLDLESS);
				ent->client->ps.saberMove = LS_NONE;
				ent->client->ps.saberBlocked = 0;
				ent->client->ps.saberBlocking = 0;
			}
			}
		}
		else if ((Q_stricmp(cmd, "wait1") == 0) || (Q_stricmp(cmd, "amwait1") == 0))
		{
			if (!(roar_emoteControl.integer & (1 << E_SIT4)))
			{
				trap_SendServerCommand(ent - g_entities, va("print \"This emote is not allowed on this server.\n\""));
				return;
			}
			if (ent->client->ps.groundEntityNum == ENTITYNUM_NONE)
			{
				return;
			}
			if (ent->client->ps.duelInProgress)
			{
				trap_SendServerCommand(ent - g_entities, va("print \"^7Emotes not allowed in duel!\n\""));
				return;
			}
			else {
				if (ent->client->ps.legsAnim == BOTH_STAND4)
				{
					ent->client->ps.forceDodgeAnim = 0;
					ent->client->ps.forceHandExtendTime = 0;
					ent->client->emote_freeze = 0;
					ent->client->ps.saberCanThrow = qtrue;
				}
				else
				{
					ent->client->ps.forceHandExtend = HANDEXTEND_TAUNT;
					ent->client->ps.forceDodgeAnim = BOTH_STAND4;
					ent->client->ps.forceHandExtendTime = level.time + Q3_INFINITE;
					ent->client->ps.saberCanThrow = qfalse;
					ent->client->emote_freeze = 1;
					StandardSetBodyAnim(ent, BOTH_STAND4, SETANIM_FLAG_OVERRIDE | SETANIM_FLAG_HOLD | SETANIM_FLAG_HOLDLESS);
					ent->client->ps.saberMove = LS_NONE;
					ent->client->ps.saberBlocked = 0;
					ent->client->ps.saberBlocking = 0;
			}
			}
		}
		else if ((Q_stricmp(cmd, "wait2") == 0) || (Q_stricmp(cmd, "amwait2") == 0))
		{
			if (!(roar_emoteControl.integer & (1 << E_SIT4)))
			{
				trap_SendServerCommand(ent - g_entities, va("print \"This emote is not allowed on this server.\n\""));
				return;
			}
			if (ent->client->ps.groundEntityNum == ENTITYNUM_NONE)
			{
				return;
			}
			if (ent->client->ps.duelInProgress)
			{
				trap_SendServerCommand(ent - g_entities, va("print \"^7Emotes not allowed in duel!\n\""));
				return;
			}
			else {
				if (ent->client->ps.legsAnim == BOTH_STAND10)
				{
					ent->client->ps.forceDodgeAnim = 0;
					ent->client->ps.forceHandExtendTime = 0;
					ent->client->emote_freeze = 0;
					ent->client->ps.saberCanThrow = qtrue;
				}
				else
				{
					ent->client->ps.forceHandExtend = HANDEXTEND_TAUNT;
					ent->client->ps.forceDodgeAnim = BOTH_STAND10;
					ent->client->ps.forceHandExtendTime = level.time + Q3_INFINITE;
					ent->client->ps.saberCanThrow = qfalse;
					ent->client->emote_freeze = 1;
					StandardSetBodyAnim(ent, BOTH_STAND10, SETANIM_FLAG_OVERRIDE | SETANIM_FLAG_HOLD | SETANIM_FLAG_HOLDLESS);
					ent->client->ps.saberMove = LS_NONE;
					ent->client->ps.saberBlocked = 0;
					ent->client->ps.saberBlocking = 0;
				}
			}
		}
		else if ((Q_stricmp(cmd, "die1") == 0) || (Q_stricmp(cmd, "amdie1") == 0))
		{
			if (!(roar_emoteControl.integer & (1 << E_SIT4)))
			{
				trap_SendServerCommand(ent - g_entities, va("print \"This emote is not allowed on this server.\n\""));
				return;
			}
			if (ent->client->ps.groundEntityNum == ENTITYNUM_NONE)
			{
				return;
			}
			if (ent->client->ps.duelInProgress)
			{
				trap_SendServerCommand(ent - g_entities, va("print \"^7Emotes not allowed in duel!\n\""));
				return;
			}
			else {
				if (ent->client->ps.legsAnim == BOTH_DEATH1)
				{
					ent->client->ps.forceDodgeAnim = 0;
					ent->client->ps.forceHandExtendTime = 0;
					ent->client->emote_freeze = 0;
					ent->client->ps.saberCanThrow = qtrue;
				}
				else
				{
					ent->client->ps.forceHandExtend = HANDEXTEND_TAUNT;
					ent->client->ps.forceDodgeAnim = BOTH_DEATH1;
					ent->client->ps.forceHandExtendTime = level.time + Q3_INFINITE;
					ent->client->ps.saberCanThrow = qfalse;
					ent->client->emote_freeze = 1;
					StandardSetBodyAnim(ent, BOTH_DEATH1, SETANIM_FLAG_OVERRIDE | SETANIM_FLAG_HOLD | SETANIM_FLAG_HOLDLESS);
					ent->client->ps.saberMove = LS_NONE;
					ent->client->ps.saberBlocked = 0;
					ent->client->ps.saberBlocking = 0;
				}
			}
		}
		else if ((Q_stricmp(cmd, "die2") == 0) || (Q_stricmp(cmd, "amdie2") == 0))
		{
			if (!(roar_emoteControl.integer & (1 << E_SIT4)))
			{
				trap_SendServerCommand(ent - g_entities, va("print \"This emote is not allowed on this server.\n\""));
				return;
			}
			if (ent->client->ps.groundEntityNum == ENTITYNUM_NONE)
			{
				return;
			}
			if (ent->client->ps.duelInProgress)
			{
				trap_SendServerCommand(ent - g_entities, va("print \"^7Emotes not allowed in duel!\n\""));
				return;
			}
			else {
				if (ent->client->ps.legsAnim == BOTH_DEATH14)
				{
					ent->client->ps.forceDodgeAnim = 0;
					ent->client->ps.forceHandExtendTime = 0;
					ent->client->emote_freeze = 0;
					ent->client->ps.saberCanThrow = qtrue;
				}
				else
				{
					ent->client->ps.forceHandExtend = HANDEXTEND_TAUNT;
					ent->client->ps.forceDodgeAnim = BOTH_DEATH14;
					ent->client->ps.forceHandExtendTime = level.time + Q3_INFINITE;
					ent->client->ps.saberCanThrow = qfalse;
					ent->client->emote_freeze = 1;
					StandardSetBodyAnim(ent, BOTH_DEATH14, SETANIM_FLAG_OVERRIDE | SETANIM_FLAG_HOLD | SETANIM_FLAG_HOLDLESS);
					ent->client->ps.saberMove = LS_NONE;
					ent->client->ps.saberBlocked = 0;
					ent->client->ps.saberBlocking = 0;
			}
			}
		}
		else if ((Q_stricmp(cmd, "die3") == 0) || (Q_stricmp(cmd, "amdie3") == 0))
		{
			if (!(roar_emoteControl.integer & (1 << E_SIT4)))
			{
				trap_SendServerCommand(ent - g_entities, va("print \"This emote is not allowed on this server.\n\""));
				return;
			}
			if (ent->client->ps.groundEntityNum == ENTITYNUM_NONE)
			{
				return;
			}
			if (ent->client->ps.duelInProgress)
			{
				trap_SendServerCommand(ent - g_entities, va("print \"^7Emotes not allowed in duel!\n\""));
				return;
			}
			else {
				if (ent->client->ps.legsAnim == BOTH_DEATH17)
				{
					ent->client->ps.forceDodgeAnim = 0;
					ent->client->ps.forceHandExtendTime = 0;
					ent->client->emote_freeze = 0;
					ent->client->ps.saberCanThrow = qtrue;
				}
				else
				{
					ent->client->ps.forceHandExtend = HANDEXTEND_TAUNT;
					ent->client->ps.forceDodgeAnim = BOTH_DEATH17;
					ent->client->ps.forceHandExtendTime = level.time + Q3_INFINITE;
					ent->client->ps.saberCanThrow = qfalse;
					ent->client->emote_freeze = 1;
					StandardSetBodyAnim(ent, BOTH_DEATH17, SETANIM_FLAG_OVERRIDE | SETANIM_FLAG_HOLD | SETANIM_FLAG_HOLDLESS);
					ent->client->ps.saberMove = LS_NONE;
					ent->client->ps.saberBlocked = 0;
					ent->client->ps.saberBlocking = 0;
			}
			}
		}
		else if ((Q_stricmp(cmd, "die4") == 0) || (Q_stricmp(cmd, "amdie4") == 0))
		{
			if (!(roar_emoteControl.integer & (1 << E_SIT4)))
			{
				trap_SendServerCommand(ent - g_entities, va("print \"This emote is not allowed on this server.\n\""));
				return;
			}
			if (ent->client->ps.groundEntityNum == ENTITYNUM_NONE)
			{
				return;
			}
			if (ent->client->ps.duelInProgress)
			{
				trap_SendServerCommand(ent - g_entities, va("print \"^7Emotes not allowed in duel!\n\""));
				return;
			}
			else {
				if (ent->client->ps.legsAnim == BOTH_DEATH4)
				{
					ent->client->ps.forceDodgeAnim = 0;
					ent->client->ps.forceHandExtendTime = 0;
					ent->client->emote_freeze = 0;
					ent->client->ps.saberCanThrow = qtrue;
				}
				else
				{
					ent->client->ps.forceHandExtend = HANDEXTEND_TAUNT;
					ent->client->ps.forceDodgeAnim = BOTH_DEATH4;
					ent->client->ps.forceHandExtendTime = level.time + Q3_INFINITE;
					ent->client->ps.saberCanThrow = qfalse;
					ent->client->emote_freeze = 1;
					StandardSetBodyAnim(ent, BOTH_DEATH4, SETANIM_FLAG_OVERRIDE | SETANIM_FLAG_HOLD | SETANIM_FLAG_HOLDLESS);
					ent->client->ps.saberMove = LS_NONE;
					ent->client->ps.saberBlocked = 0;
					ent->client->ps.saberBlocking = 0;
				}
			}
		}
		//main
		else if ((Q_stricmp(cmd, "choke3") == 0) || (Q_stricmp(cmd, "amchoke3") == 0))
		{
			if (!(roar_emoteControl.integer & (1 << E_SIT4)))
		{
			trap_SendServerCommand( ent-g_entities, va("print \"This emote is not allowed on this server.\n\"") );
			return;
		}
			if (ent->client->ps.groundEntityNum == ENTITYNUM_NONE)
			{
				return;
			}
			if (ent->client->ps.duelInProgress)
	{
		trap_SendServerCommand( ent-g_entities, va("print \"^7Emotes not allowed in duel!\n\"" ) );
		return;
	}
			else {
			if (ent->client->ps.legsAnim == BOTH_CHOKE3)
			{
                        ent->client->ps.forceDodgeAnim = 0;
                        ent->client->ps.forceHandExtendTime = 0;
				ent->client->emote_freeze=0;
				ent->client->ps.saberCanThrow = qtrue;
			}
			else
			{
            ent->client->ps.forceHandExtend = HANDEXTEND_TAUNT;
            ent->client->ps.forceDodgeAnim = BOTH_CHOKE3;
            ent->client->ps.forceHandExtendTime = level.time + Q3_INFINITE;
				ent->client->ps.saberCanThrow = qfalse;
				ent->client->emote_freeze=1;
				StandardSetBodyAnim(ent, BOTH_CHOKE3, SETANIM_FLAG_OVERRIDE|SETANIM_FLAG_HOLD|SETANIM_FLAG_HOLDLESS);
				ent->client->ps.saberMove = LS_NONE;
				ent->client->ps.saberBlocked = 0;
				ent->client->ps.saberBlocking = 0;
			}
			}
		}
		else if ((Q_stricmp(cmd, "choke1") == 0) || (Q_stricmp(cmd, "amchoke1") == 0))
		{
			if (!(roar_emoteControl.integer & (1 << E_SIT4)))
		{
			trap_SendServerCommand( ent-g_entities, va("print \"This emote is not allowed on this server.\n\"") );
			return;
		}
			if (ent->client->ps.groundEntityNum == ENTITYNUM_NONE)
			{
				return;
			}
			if (ent->client->ps.duelInProgress)
	{
		trap_SendServerCommand( ent-g_entities, va("print \"^7Emotes not allowed in duel!\n\"" ) );
		return;
	}
			else {
			if (ent->client->ps.legsAnim == BOTH_CHOKE1START)
			{
                        ent->client->ps.forceDodgeAnim = 0;
                        ent->client->ps.forceHandExtendTime = 0;
				ent->client->emote_freeze=0;
				ent->client->ps.saberCanThrow = qtrue;
			}
			else
			{
            ent->client->ps.forceHandExtend = HANDEXTEND_TAUNT;
            ent->client->ps.forceDodgeAnim = BOTH_CHOKE1START;
            ent->client->ps.forceHandExtendTime = level.time + Q3_INFINITE;
				ent->client->ps.saberCanThrow = qfalse;
				ent->client->emote_freeze=1;
				StandardSetBodyAnim(ent, BOTH_CHOKE1START, SETANIM_FLAG_OVERRIDE|SETANIM_FLAG_HOLD|SETANIM_FLAG_HOLDLESS);
				ent->client->ps.saberMove = LS_NONE;
				ent->client->ps.saberBlocked = 0;
				ent->client->ps.saberBlocking = 0;
			}
			}
		}
		else if ((Q_stricmp(cmd, "chatcolor") == 0) || (Q_stricmp(cmd, "amchatcolor") == 0)){
		char	cmd[1024];
		trap_Argv( 1, cmd, 1024 );
		if (roar_allow_chatColors.integer == 0){
		trap_SendServerCommand( ent-g_entities, va("print \"^3Chat colors are disabled.\n\"") );
		return;
		}
	else if ( !cmd[0] )
	{
		trap_SendServerCommand( ent-g_entities, va("print \"^3COLORS = RED GREEN YELLOW BLUE CYAN PURPLE WHITE BLACK\n\"") );
	}
	else if ( Q_stricmp( cmd, "red" ) == 0 )
	{
		trap_SendServerCommand( ent-g_entities, va("print \"^3CHAT COLOR CHANGED TO ^1RED\n\"") );
		ent->client->pers.chatred = 1;
		ent->client->pers.chatgreen = 0;
		ent->client->pers.chatyellow = 0;
		ent->client->pers.chatblue = 0;
		ent->client->pers.chatlblue = 0;
		ent->client->pers.chatpurple = 0;
		ent->client->pers.chatwhite = 0;
		ent->client->pers.chatblack = 0;
	}
	else if ( Q_stricmp( cmd, "green" ) == 0 )
	{
		trap_SendServerCommand( ent-g_entities, va("print \"^3CHAT COLOR CHANGED TO ^2GREEN\n\"") );
		ent->client->pers.chatred = 0;
		ent->client->pers.chatgreen = 1;
		ent->client->pers.chatyellow = 0;
		ent->client->pers.chatblue = 0;
		ent->client->pers.chatlblue = 0;
		ent->client->pers.chatpurple = 0;
		ent->client->pers.chatwhite = 0;
		ent->client->pers.chatblack = 0;
	}
	else if ( Q_stricmp( cmd, "yellow" ) == 0 )
	{
		trap_SendServerCommand( ent-g_entities, va("print \"^3CHAT COLOR CHANGED TO ^3YELLOW\n\"") );
		ent->client->pers.chatred = 0;
		ent->client->pers.chatgreen = 0;
		ent->client->pers.chatyellow = 1;
		ent->client->pers.chatblue = 0;
		ent->client->pers.chatlblue = 0;
		ent->client->pers.chatpurple = 0;
		ent->client->pers.chatwhite = 0;
		ent->client->pers.chatblack = 0;
	}
	else if ( Q_stricmp( cmd, "blue" ) == 0 )
	{
		trap_SendServerCommand( ent-g_entities, va("print \"^3CHAT COLOR CHANGED TO ^4BLUE\n\"") );
		ent->client->pers.chatred = 0;
		ent->client->pers.chatgreen = 0;
		ent->client->pers.chatyellow = 0;
		ent->client->pers.chatblue = 1;
		ent->client->pers.chatlblue = 0;
		ent->client->pers.chatpurple = 0;
		ent->client->pers.chatwhite = 0;
		ent->client->pers.chatblack = 0;
	}
	else if ( Q_stricmp( cmd, "cyan" ) == 0 )
	{
		trap_SendServerCommand( ent-g_entities, va("print \"^3CHAT COLOR CHANGED TO ^5CYAN\n\"") );
		ent->client->pers.chatred = 0;
		ent->client->pers.chatgreen = 0;
		ent->client->pers.chatyellow = 0;
		ent->client->pers.chatblue = 0;
		ent->client->pers.chatlblue = 1;
		ent->client->pers.chatpurple = 0;
		ent->client->pers.chatwhite = 0;
		ent->client->pers.chatblack = 0;
	}
	else if ( Q_stricmp( cmd, "purple" ) == 0 )
	{
		trap_SendServerCommand( ent-g_entities, va("print \"^3CHAT COLOR CHANGED TO ^6PURPLE\n\"") );
		ent->client->pers.chatred = 0;
		ent->client->pers.chatgreen = 0;
		ent->client->pers.chatyellow = 0;
		ent->client->pers.chatblue = 0;
		ent->client->pers.chatlblue = 0;
		ent->client->pers.chatpurple = 1;
		ent->client->pers.chatwhite = 0;
		ent->client->pers.chatblack = 0;
	}
	else if ( Q_stricmp( cmd, "white" ) == 0 )
	{
		trap_SendServerCommand( ent-g_entities, va("print \"^3CHAT COLOR CHANGED TO ^7WHITE\n\"") );
		ent->client->pers.chatred = 0;
		ent->client->pers.chatgreen = 0;
		ent->client->pers.chatyellow = 0;
		ent->client->pers.chatblue = 0;
		ent->client->pers.chatlblue = 0;
		ent->client->pers.chatpurple = 0;
		ent->client->pers.chatwhite = 1;
		ent->client->pers.chatblack = 0;
	}
	else if ( Q_stricmp( cmd, "black" ) == 0 )
	{
		trap_SendServerCommand( ent-g_entities, va("print \"^3CHAT COLOR CHANGED TO ^0BLACK\n\"") );
		ent->client->pers.chatred = 0;
		ent->client->pers.chatgreen = 0;
		ent->client->pers.chatyellow = 0;
		ent->client->pers.chatblue = 0;
		ent->client->pers.chatlblue = 0;
		ent->client->pers.chatpurple = 0;
		ent->client->pers.chatwhite = 0;
		ent->client->pers.chatblack = 1;
	}
		}
	else if (Q_stricmp (cmd, "engage_forceduel") == 0 )
		{
			if (roar_enable_ForceDuel.integer == 1)
			{
			Cmd_EngageDuel_f( ent, 1 );
			}
			else {
				trap_SendServerCommand( ent-g_entities, va("print \"^7Force duels disabled on this server!\n\"" ) );
			}
		}
	else if (Q_stricmp (cmd, "engage_trainingduel") == 0 )
		{
			if (roar_enable_TrainingDuel.integer == 1)
			{
			Cmd_EngageDuel_f( ent, 2 );
			}
			else {
				trap_SendServerCommand( ent-g_entities, va("print \"^7Training duels disabled on this server!\n\"" ) );
			}
		}
	else if (Q_stricmp (cmd, "engage_meleeduel") == 0 )
		{
			if (roar_enable_MeleeDuel.integer == 1)
			{
			Cmd_EngageDuel_f( ent, 3 );
			}
			else {
				trap_SendServerCommand( ent-g_entities, va("print \"^7Melee duels disabled on this server!\n\"" ) );
			}
		}
	/*else if (Q_stricmp (cmd, "kylesmash") == 0)
	{
		TryGrapple(ent);
	}*/
			else if ((Q_stricmp(cmd, "silence") == 0) || (Q_stricmp(cmd, "amsilence") == 0))
		{ // silence a player
			gentity_t * targetplayer;
			int i;
			int	client_id = -1; 
			char	arg1[MAX_STRING_CHARS];
			if (ent->r.svFlags & SVF_ADMIN1)
			{
				if (!(roar_adminControl1.integer & (1 << A_SILENCE)))
		{
			trap_SendServerCommand( ent-g_entities, va("print \"Silence is not allowed at this administration rank.\n\"") );
			return;
		}
			}
			else if (ent->r.svFlags & SVF_ADMIN2)
			{
				if (!(roar_adminControl2.integer & (1 << A_SILENCE)))
		{
			trap_SendServerCommand( ent-g_entities, va("print \"Silence is not allowed at this administration rank.\n\"") );
			return;
		}
			}
			if (ent->r.svFlags & SVF_ADMIN3)
			{
				if (!(roar_adminControl3.integer & (1 << A_SILENCE)))
		{
			trap_SendServerCommand( ent-g_entities, va("print \"Silence is not allowed at this administration rank.\n\"") );
			return;
		}
			}
			if (ent->r.svFlags & SVF_ADMIN4)
			{
				if (!(roar_adminControl4.integer & (1 << A_SILENCE)))
		{
			trap_SendServerCommand( ent-g_entities, va("print \"Silence is not allowed at this administration rank.\n\"") );
			return;
		}
			}
			if (ent->r.svFlags & SVF_ADMIN5)
			{
				if (!(roar_adminControl5.integer & (1 << A_SILENCE)))
		{
			trap_SendServerCommand( ent-g_entities, va("print \"Silence is not allowed at this administration rank.\n\"") );
			return;
		}
			}
			if (!(ent->r.svFlags & SVF_ADMIN1) && !(ent->r.svFlags & SVF_ADMIN2) && !(ent->r.svFlags & SVF_ADMIN3) && !(ent->r.svFlags & SVF_ADMIN4) && !(ent->r.svFlags & SVF_ADMIN5)){
				trap_SendServerCommand( ent-g_entities, "print \"Must login with /adminlogin (password)\n\"" );
				return;
			}
			if ( trap_Argc() < 2 )
			{
				trap_SendServerCommand( ent-g_entities, "print \"Type in /help silence if you need help with this command.\n\"" );
				return;
			}
			trap_Argv( 1, arg1, sizeof( arg1 ) );
			if( Q_stricmp( arg1, "+all" ) == 0 ){
			for( i = 0; i < level.maxclients; i++ )
		{
			targetplayer = &g_entities[i];

			if( targetplayer->client && targetplayer->client->pers.connected ){
				if (!targetplayer->client->pers.silent && !targetplayer->client->pers.silent2){
					targetplayer->client->pers.silent = qtrue;
				}
			}
		}
		trap_SendServerCommand( -1, va("cp \"^7%s\n\"", roar_silence_on_ALL_saying.string ) );
		trap_SendServerCommand( -1, va("print \"^7%s\n\"", roar_silence_on_ALL_saying.string ) );
		return;
			}
			else if( Q_stricmp( arg1, "-all" ) == 0 ){
			for( i = 0; i < level.maxclients; i++ )
		{
			targetplayer = &g_entities[i];

			if( targetplayer->client && targetplayer->client->pers.connected ){
				if (targetplayer->client->pers.silent && !targetplayer->client->pers.silent2){
					targetplayer->client->pers.silent = qfalse;
				}
			}
		}
		trap_SendServerCommand( -1, va("cp \"^7%s\n\"", roar_silence_off_ALL_saying.string ) );
		trap_SendServerCommand( -1, va("print \"^7%s\n\"", roar_silence_off_ALL_saying.string ) );
		return;
			}
			else {
			client_id = G_ClientNumberFromArg( arg1 );
			}

			if (client_id == -1)
			{
				trap_SendServerCommand( ent-g_entities, va("print \"Can't find client ID for %s\n\"", arg1 ) );
				return;
			}
			if (client_id == -2)
			{
				trap_SendServerCommand( ent-g_entities, va("print \"Ambiguous client ID for %s\n\"", arg1 ) );
				return;
			}
			// either we have the client id or the string did not match
			if (!g_entities[client_id].inuse)
			{ // check to make sure client slot is in use
				trap_SendServerCommand( ent-g_entities, va("print \"Client %s is not active\n\"", arg1 ) );
				return;
			}
			if (g_entities[client_id].client->pers.ampunish){
			trap_SendServerCommand( ent-g_entities, va("print \"Client is currently being punished. Please unpunish them before silencing them.\nTo unpunish them, type in /punish again on the client.\n\"" ) );
			 return;
		 }
			if (g_entities[client_id].client->pers.silent)
			{
				g_entities[client_id].client->pers.silent = qfalse;
				trap_SendServerCommand( -1, va("cp \"%s^7\n%s\n\"", g_entities[client_id].client->pers.netname, roar_silence_off_saying.string ) );
				trap_SendServerCommand( -1, va("print \"%s ^7%s\n\"", g_entities[client_id].client->pers.netname, roar_silence_off_saying.string ) );
			}
			else{
			g_entities[client_id].client->pers.silent = qtrue;
			trap_SendServerCommand( -1, va("cp \"%s^7\n%s\n\"", g_entities[client_id].client->pers.netname, roar_silence_on_saying.string ) );
			trap_SendServerCommand( -1, va("print \"%s ^7%s\n\"", g_entities[client_id].client->pers.netname, roar_silence_on_saying.string ) );
			G_Sound(&g_entities[client_id], CHAN_AUTO, G_SoundIndex("sound/effects/electric_beam_lp.wav")); 
			}
		}
		else if ((Q_stricmp(cmd, "insultsilence") == 0) || (Q_stricmp(cmd, "aminsultsilence") == 0))
		{ // silence a player
			int	client_id = -1; 
			char	arg1[MAX_STRING_CHARS];
			if (ent->r.svFlags & SVF_ADMIN1)
			{
				if (!(roar_adminControl1.integer & (1 << A_INSULTSILENCE)))
		{
			trap_SendServerCommand( ent-g_entities, va("print \"InsultSilence is not allowed at this administration rank.\n\"") );
			return;
		}
			}
			else if (ent->r.svFlags & SVF_ADMIN2)
			{
				if (!(roar_adminControl2.integer & (1 << A_INSULTSILENCE)))
		{
			trap_SendServerCommand( ent-g_entities, va("print \"InsultSilence is not allowed at this administration rank.\n\"") );
			return;
		}
			}
			if (ent->r.svFlags & SVF_ADMIN3)
			{
				if (!(roar_adminControl3.integer & (1 << A_INSULTSILENCE)))
		{
			trap_SendServerCommand( ent-g_entities, va("print \"InsultSilence is not allowed at this administration rank.\n\"") );
			return;
		}
			}
			if (ent->r.svFlags & SVF_ADMIN4)
			{
				if (!(roar_adminControl4.integer & (1 << A_INSULTSILENCE)))
		{
			trap_SendServerCommand( ent-g_entities, va("print \"InsultSilence is not allowed at this administration rank.\n\"") );
			return;
		}
			}
			if (ent->r.svFlags & SVF_ADMIN5)
			{
				if (!(roar_adminControl5.integer & (1 << A_INSULTSILENCE)))
		{
			trap_SendServerCommand( ent-g_entities, va("print \"InsultSilence is not allowed at this administration rank.\n\"") );
			return;
		}
			}
			if (!(ent->r.svFlags & SVF_ADMIN1) && !(ent->r.svFlags & SVF_ADMIN2) && !(ent->r.svFlags & SVF_ADMIN3) && !(ent->r.svFlags & SVF_ADMIN4) && !(ent->r.svFlags & SVF_ADMIN5)){
				trap_SendServerCommand( ent-g_entities, "print \"Must login with /adminlogin (password)\n\"" );
				return;
			}
			if ( trap_Argc() < 2 )
			{
				trap_SendServerCommand( ent-g_entities, "print \"Type in /help insultsilence if you need help with this command.\n\"" );
				return;
			}
			trap_Argv( 1, arg1, sizeof( arg1 ) );
			client_id = G_ClientNumberFromArg( arg1 );

			if (client_id == -1)
			{
				trap_SendServerCommand( ent-g_entities, va("print \"Can't find client ID for %s\n\"", arg1 ) );
				return;
			}
			if (client_id == -2)
			{
				trap_SendServerCommand( ent-g_entities, va("print \"Ambiguous client ID for %s\n\"", arg1 ) );
				return;
			}
			// either we have the client id or the string did not match
			if (!g_entities[client_id].inuse)
			{ // check to make sure client slot is in use
				trap_SendServerCommand( ent-g_entities, va("print \"Client %s is not active\n\"", arg1 ) );
				return;
			}
			if (g_entities[client_id].client->pers.ampunish){
			trap_SendServerCommand( ent-g_entities, va("print \"Client is currently being punished. Please unpunish them before insult-silencing them.\nTo unpunish them, type in /punish again on the client.\n\"" ) );
			 return;
		 }
			if (g_entities[client_id].client->pers.silent2)
			{
				g_entities[client_id].client->pers.silent2 = qfalse;
				trap_SendServerCommand( -1, va("cp \"%s^7\n%s\n\"", g_entities[client_id].client->pers.netname, roar_insult_silence_off_saying.string ) );
				trap_SendServerCommand( -1, va("print \"%s ^7%s\n\"", g_entities[client_id].client->pers.netname, roar_insult_silence_off_saying.string ) );
				}
			else{
			g_entities[client_id].client->pers.silent2 = qtrue;
			trap_SendServerCommand( -1, va("cp \"%s^7\n%s\n\"", g_entities[client_id].client->pers.netname, roar_insult_silence_on_saying.string ) );
			trap_SendServerCommand( -1, va("print \"%s ^7%s\n\"", g_entities[client_id].client->pers.netname, roar_insult_silence_on_saying.string ) ); 
			G_Sound(&g_entities[client_id], CHAN_AUTO, G_SoundIndex("sound/effects/electric_beam_lp.wav")); 
			}
		}
			else if ((Q_stricmp(cmd, "teleport") == 0) || (Q_stricmp(cmd, "tele") == 0) || (Q_stricmp(cmd, "admintele") == 0) || (Q_stricmp(cmd, "amtele") == 0))
		{ // teleport to specific location
			vec3_t location;
			vec3_t forward;
		if ( trap_Argc() < 2 ) {
		trap_SendServerCommand( ent-g_entities, va("print \"^3Type in ^5/help teleport ^3if you need help with this command.\n\"") );
		return;
		}
			if ( trap_Argc() == 3 )
			{
			int	clId = -1;
         int   clId2 = -1;
			char	arg1[MAX_STRING_CHARS];
         char	arg2[MAX_STRING_CHARS];
			trap_Argv( 1, arg1, sizeof( arg1 ) );
			clId = G_ClientNumberFromArg( arg1 );			
         trap_Argv( 2, arg2, sizeof( arg2 ) );
			clId2 = G_ClientNumberFromArg( arg2 );

			if (clId == -1 || clId2 == -1)
			{
				trap_SendServerCommand( ent-g_entities, va("print \"Can't find client ID for %s\n\"", arg1 ) );
				return;
			}
			if (clId == -2 || clId2 == -2)
			{
				trap_SendServerCommand( ent-g_entities, va("print \"Ambiguous client ID for %s\n\"", arg1 ) );
				return;
			}
			if (clId >= 32 || clId2 >= 32)
			{
				trap_SendServerCommand(ent - g_entities, va("print \"Ambiguous client ID for %s\n\"", arg1));
				return;
			}
			// either we have the client id or the string did not match
			if (!g_entities[clId].inuse && !g_entities[clId2].inuse)
			{ // check to make sure client slot is in use
				trap_SendServerCommand( ent-g_entities, va("print \"Client %s is not active\n\"", arg1 ) );
				return;
			}
			if (g_entities[clId].health <= 0 && g_entities[clId2].health <= 0)
		 {
			return;
		 }
			VectorCopy(g_entities[clId2].client->ps.origin, location);
			AngleVectors(g_entities[clId2].client->ps.viewangles, forward, NULL, NULL);
			// set location out in front of your view
			forward[2] = 0; //no elevation change
			VectorNormalize(forward);
			VectorMA(g_entities[clId2].client->ps.origin, 100, forward, location);
			location[2] += 5; //add just a bit of height???
			// teleport them to you
			TeleportPlayer( &g_entities[clId], location, g_entities[clId2].client->ps.viewangles);
			//trap_SendServerCommand( -1, va("cp \"%s^7\n%s\n\"", g_entities[clId].client->pers.netname, roar_teleport_saying.string ) );  
			}
			if ( trap_Argc() == 4 )
			{
				Admin_Teleport(ent);
			}
		}

				else if ((Q_stricmp(cmd, "origin") == 0) || (Q_stricmp(cmd, "amorigin") == 0))
		{ // teleport to specific location
			//
			int	client_id = -1; 
			char	arg1[MAX_STRING_CHARS];
			trap_Argv( 1, arg1, sizeof( arg1 ) );
			client_id = G_ClientNumberFromArg( arg1 );
			//
			if (client_id)
			{
				trap_SendServerCommand( ent-g_entities, va("print \"^1X:^7%d, ^1Y:^7%d, ^1Z:^7%d\n\"", (int) g_entities[client_id].client->ps.origin[0], (int) g_entities[client_id].client->ps.origin[1], (int) g_entities[client_id].client->ps.origin[2]));
				return;
			}
			else
			{
				trap_SendServerCommand( ent-g_entities, va("print \"^1X:^7%d, ^1Y:^7%d, ^1Z:^7%d\n\"", (int) ent->client->ps.origin[0], (int) ent->client->ps.origin[1], (int) ent->client->ps.origin[2]));
			}
		}
		 else if ((Q_stricmp(cmd, "terminator") == 0) || (Q_stricmp(cmd, "amterminator") == 0) || (Q_stricmp(cmd, "ammerc") == 0))
      {
		  gentity_t * targetplayer;
		  int	i;
		int	num = 0;
         int	client_id = -1; 
         char	arg1[MAX_STRING_CHARS]; 
		 if (ent->r.svFlags & SVF_ADMIN1)
			{
				if (!(roar_adminControl1.integer & (1 << A_TERMINATOR)))
		{
			trap_SendServerCommand( ent-g_entities, va("print \"Terminator is not allowed at this administration rank.\n\"") );
			return;
		}
			}
			else if (ent->r.svFlags & SVF_ADMIN2)
			{
				if (!(roar_adminControl2.integer & (1 << A_TERMINATOR)))
		{
			trap_SendServerCommand( ent-g_entities, va("print \"Terminator is not allowed at this administration rank.\n\"") );
			return;
		}
			}
			if (ent->r.svFlags & SVF_ADMIN3)
			{
				if (!(roar_adminControl3.integer & (1 << A_TERMINATOR)))
		{
			trap_SendServerCommand( ent-g_entities, va("print \"Terminator is not allowed at this administration rank.\n\"") );
			return;
		}
			}
			if (ent->r.svFlags & SVF_ADMIN4)
			{
				if (!(roar_adminControl4.integer & (1 << A_TERMINATOR)))
		{
			trap_SendServerCommand( ent-g_entities, va("print \"Terminator is not allowed at this administration rank.\n\"") );
			return;
		}
			}
			if (ent->r.svFlags & SVF_ADMIN5)
			{
				if (!(roar_adminControl5.integer & (1 << A_TERMINATOR)))
		{
			trap_SendServerCommand( ent-g_entities, va("print \"Terminator is not allowed at this administration rank.\n\"") );
			return;
		}
			}
			if (!(ent->r.svFlags & SVF_ADMIN1) && !(ent->r.svFlags & SVF_ADMIN2) && !(ent->r.svFlags & SVF_ADMIN3) && !(ent->r.svFlags & SVF_ADMIN4) && !(ent->r.svFlags & SVF_ADMIN5)){
				trap_SendServerCommand( ent-g_entities, "print \"Must login with /adminlogin (password)\n\"" );
				return;
			}
         if ( trap_Argc() > 2 )
         { 
            trap_SendServerCommand( ent-g_entities, "print \"Usage: ^3/terminator (client)\n\"" ); 
            return; 
         }
         trap_Argv( 1,  arg1, sizeof(  arg1 ) ); 
		 if( Q_stricmp( arg1, "+all" ) == 0 ){
			for( i = 0; i < level.maxclients; i++ )
		{
			targetplayer = &g_entities[i];

			if( targetplayer->client && targetplayer->client->pers.connected ){
				if (!targetplayer->client->pers.amterminator){
		targetplayer->client->pers.amempower = 0;
		targetplayer->client->ps.eFlags &= ~EF_BODYPUSH;
		 //MJN's code here
				// Restore forcepowers:
		for( i = 0; i < NUM_FORCE_POWERS; i ++ ){
			// Save
			targetplayer->client->ps.fd.forcePowerLevel[i] = targetplayer->client->pers.forcePowerLevelSaved[i];
		}

		// Save and set known powers:
		targetplayer->client->ps.fd.forcePowersKnown = targetplayer->client->pers.forcePowersKnownSaved;
		//
		//targetplayer->client->ps.powerups[PW_FORCE_ENLIGHTENED_LIGHT] = 0;
		//targetplayer->client->ps.powerups[PW_FORCE_ENLIGHTENED_DARK] = 0;
		targetplayer->client->pers.amterminator = 1;
		targetplayer->client->pers.amhuman = 0;
		targetplayer->client->ps.weapon = WP_MELEE;
				}
			}
		}
		trap_SendServerCommand( -1, va("print \"^7%s\n\"", roar_terminator_on_ALL_saying.string ) );
		trap_SendServerCommand( -1, va("cp \"^7%s\n\"", roar_terminator_on_ALL_saying.string ) );
		return;
		 }
		 else if( Q_stricmp( arg1, "-all" ) == 0 ){
			for( i = 0; i < level.maxclients; i++ )
		{
			targetplayer = &g_entities[i];

			if( targetplayer->client && targetplayer->client->pers.connected ){
				if (targetplayer->client->pers.amterminator){
		targetplayer->client->ps.eFlags &= ~EF_SEEKERDRONE;
		targetplayer->client->ps.stats[STAT_HOLDABLE_ITEMS] &= ~(1 << HI_SEEKER) & ~(1 << HI_BINOCULARS) & ~(1 << HI_SENTRY_GUN) & ~(1 << HI_EWEB) & ~(1 << HI_CLOAK);
		targetplayer->client->pers.amterminator = 0;
		targetplayer->client->ps.stats[STAT_WEAPONS] &= ~(1 << WP_STUN_BATON) & ~(1 << WP_BLASTER) & ~(1 << WP_DISRUPTOR) & ~(1 << WP_BOWCASTER)
			& ~(1 << WP_REPEATER) & ~(1 << WP_DEMP2) & ~(1 << WP_FLECHETTE) & ~(1 << WP_ROCKET_LAUNCHER) & ~(1 << WP_THERMAL) & ~(1 << WP_DET_PACK)
			& ~(1 << WP_BRYAR_OLD) & ~(1 << WP_CONCUSSION) & ~(1 << WP_TRIP_MINE);
		if (roar_starting_weapons.integer == 0)
				{
				targetplayer->client->ps.stats[STAT_WEAPONS] |= ( 1 << WP_SABER);
				targetplayer->client->ps.weapon = WP_SABER;
				}
				else if (roar_starting_weapons.integer == 1)
				{			
				targetplayer->client->ps.stats[STAT_WEAPONS] |= (1 << WP_MELEE);
				targetplayer->client->ps.weapon = WP_MELEE;
				}
				else if (roar_starting_weapons.integer == 2)
				{
				targetplayer->client->ps.weapon = WP_SABER;
				targetplayer->client->ps.stats[STAT_WEAPONS] |= (1 << WP_MELEE) | (1 << WP_SABER);
				}
				if (!(g_weaponDisable.integer & (1 << WP_BRYAR_PISTOL)))
			{
				targetplayer->client->ps.stats[STAT_WEAPONS] |= ( 1 << WP_BRYAR_PISTOL );
			}
				}
			}
		}
		Tforce[targetplayer->client->ps.clientNum] = 0;
		trap_SendServerCommand( -1, va("print \"^7%s\n\"", roar_terminator_off_ALL_saying.string ) );
		trap_SendServerCommand( -1, va("cp \"^7%s\n\"", roar_terminator_off_ALL_saying.string ) );
		return;
		 }
		 else {
         client_id = G_ClientNumberFromArg(  arg1 );
		 }
		 if ( trap_Argc() < 2 ) 
         { 
			client_id = ent->client->ps.clientNum;
		}
         if (client_id == -1) 
         { 
            trap_SendServerCommand( ent-g_entities, va("print \"Can't find client ID for %s\n\"", arg1 ) ); 
            return; 
         } 
         if (client_id == -2) 
         { 
            trap_SendServerCommand( ent-g_entities, va("print \"Ambiguous client ID for %s\n\"", arg1 ) ); 
            return; 
         } 
         // either we have the client id or the string did not match 
         if (!g_entities[client_id].inuse) 
         { // check to make sure client slot is in use 
            trap_SendServerCommand( ent-g_entities, va("print \"Client %s is not active\n\"", arg1 ) ); 
            return; 
         }
		 if (g_entities[client_id].client->pers.amterminator)
		 {
			 for ( i = 0 ; i < MAX_WEAPONS ; i++ ) {
			g_entities[client_id].client->ps.ammo[i] = num;
			}
		 g_entities[client_id].client->ps.eFlags &= ~EF_SEEKERDRONE;
		g_entities[client_id].client->ps.stats[STAT_HOLDABLE_ITEMS] &= ~(1 << HI_SEEKER) & ~(1 << HI_BINOCULARS) & ~(1 << HI_SENTRY_GUN) & ~(1 << HI_EWEB) & ~(1 << HI_CLOAK);
		g_entities[client_id].client->ps.stats[STAT_WEAPONS] &= ~(1 << WP_STUN_BATON) & ~(1 << WP_BLASTER) & ~(1 << WP_DISRUPTOR) & ~(1 << WP_BOWCASTER)
			& ~(1 << WP_REPEATER) & ~(1 << WP_DEMP2) & ~(1 << WP_FLECHETTE) & ~(1 << WP_ROCKET_LAUNCHER) & ~(1 << WP_THERMAL) & ~(1 << WP_DET_PACK)
			& ~(1 << WP_BRYAR_OLD) & ~(1 << WP_CONCUSSION) & ~(1 << WP_TRIP_MINE);
       // trap_SendServerCommand( -1, va("amterminator %i %i", client_id, 0)); //FOR CUSTOM EFFECTS!!!
		g_entities[client_id].client->pers.amterminator = 0;
		if (roar_starting_weapons.integer == 0)
				{
				g_entities[client_id].client->ps.stats[STAT_WEAPONS] |= ( 1 << WP_SABER);
				g_entities[client_id].client->ps.weapon = WP_SABER;
				}
				else if (roar_starting_weapons.integer == 1)
				{			
				g_entities[client_id].client->ps.stats[STAT_WEAPONS] |= (1 << WP_MELEE);
				g_entities[client_id].client->ps.weapon = WP_MELEE;
				}
				else if (roar_starting_weapons.integer == 2)
				{
				g_entities[client_id].client->ps.weapon = WP_SABER;
				g_entities[client_id].client->ps.stats[STAT_WEAPONS] |= (1 << WP_MELEE) + (1 << WP_SABER);
				//g_entities[client_id].client->ps.stats[STAT_WEAPONS] |= (1 << WP_SABER);
				}
				if (!(g_weaponDisable.integer & (1 << WP_BRYAR_PISTOL)))
			{
				g_entities[client_id].client->ps.stats[STAT_WEAPONS] |= ( 1 << WP_BRYAR_PISTOL );
			}
		Tforce[g_entities[client_id].client->ps.clientNum] = 0;
         trap_SendServerCommand( -1, va("cp \"%s^7\n%s\n\"", g_entities[client_id].client->pers.netname, roar_terminator_off_saying.string ) );
		 trap_SendServerCommand( -1, va("print \"%s ^7%s\n\"", g_entities[client_id].client->pers.netname, roar_terminator_off_saying.string ) ); 
				}
		 else {
			  //Empowerment takeaway
		//MJN's code here
		// Restore forcepowers:
		for( i = 0; i < NUM_FORCE_POWERS; i ++ ){
			// Save
			g_entities[client_id].client->ps.fd.forcePowerLevel[i] = g_entities[client_id].client->pers.forcePowerLevelSaved[i];
		}

		// Save and set known powers:
		g_entities[client_id].client->ps.fd.forcePowersKnown = g_entities[client_id].client->pers.forcePowersKnownSaved;
		//
		g_entities[client_id].client->pers.amempower = 0;
		g_entities[client_id].client->ps.eFlags &= ~EF_BODYPUSH;
		//g_entities[client_id].client->ps.powerups[PW_FORCE_ENLIGHTENED_LIGHT] = 0;
		//g_entities[client_id].client->ps.powerups[PW_FORCE_ENLIGHTENED_DARK] = 0;
			//
		 g_entities[client_id].client->pers.amterminator = 1;
		 g_entities[client_id].client->pers.amhuman = 0;
		 //trap_SendServerCommand( -1, va("amterminator %i %i", client_id, 1)); //FOR CUSTOM EFFECTS!!!
		 g_entities[client_id].client->ps.weapon = WP_MELEE;
         trap_SendServerCommand( -1, va("cp \"%s^7\n%s\n\"", g_entities[client_id].client->pers.netname, roar_terminator_on_saying.string ) );
		 trap_SendServerCommand( -1, va("print \"%s ^7%s\n\"", g_entities[client_id].client->pers.netname, roar_terminator_on_saying.string ) );
         G_ScreenShake(g_entities[client_id].client->ps.origin, &g_entities[client_id],  3.0f, 2000, qtrue); 
         G_Sound(&g_entities[client_id], CHAN_AUTO, G_SoundIndex("sound/effects/electric_beam_lp.wav")); 
		 }
	  }
	  else if ((Q_stricmp(cmd, "CSPrint") == 0) || (Q_stricmp(cmd, "amCSPrint") == 0) || (Q_stricmp(cmd, "ampsay") == 0))
      { 
		 //RoAR mod NOTE: Blµb's code begins here
		 int pos = 0;
		 char real_msg[MAX_STRING_CHARS];
		 char *msg = ConcatArgs(1); 
		 while(*msg) { 
    if(msg[0] == '\\' && msg[1] == 'n') { 
          msg++;           // \n is 2 chars, so increase by one here. (one, cuz it's increased down there again...) 
          real_msg[pos++] = '\n';  // put in a real \n 
    } else { 
          real_msg[pos++] = *msg;  // otherwise just copy 
    } 
    msg++;                         // increase the msg pointer 
}
		 real_msg[pos] = 0;
		 //RoAR mod NOTE: Blµb's code ends here
		 if (ent->r.svFlags & SVF_ADMIN1)
			{
				if (!(roar_adminControl1.integer & (1 << A_CSPRINT)))
		{
			trap_SendServerCommand( ent-g_entities, va("print \"CSPrint is not allowed at this administration rank.\n\"") );
			return;
		}
			}
			else if (ent->r.svFlags & SVF_ADMIN2)
			{
				if (!(roar_adminControl2.integer & (1 << A_CSPRINT)))
		{
			trap_SendServerCommand( ent-g_entities, va("print \"CSPrint is not allowed at this administration rank.\n\"") );
			return;
		}
			}
			if (ent->r.svFlags & SVF_ADMIN3)
			{
				if (!(roar_adminControl3.integer & (1 << A_CSPRINT)))
		{
			trap_SendServerCommand( ent-g_entities, va("print \"CSPrint is not allowed at this administration rank.\n\"") );
			return;
		}
			}
			if (ent->r.svFlags & SVF_ADMIN4)
			{
				if (!(roar_adminControl4.integer & (1 << A_CSPRINT)))
		{
			trap_SendServerCommand( ent-g_entities, va("print \"CSPrint is not allowed at this administration rank.\n\"") );
			return;
		}
			}
			if (ent->r.svFlags & SVF_ADMIN5)
			{
				if (!(roar_adminControl5.integer & (1 << A_CSPRINT)))
		{
			trap_SendServerCommand( ent-g_entities, va("print \"CSPrint is not allowed at this administration rank.\n\"") );
			return;
		}
			}
			if (!(ent->r.svFlags & SVF_ADMIN1) && !(ent->r.svFlags & SVF_ADMIN2) && !(ent->r.svFlags & SVF_ADMIN3) && !(ent->r.svFlags & SVF_ADMIN4) && !(ent->r.svFlags & SVF_ADMIN5)){
				trap_SendServerCommand( ent-g_entities, "print \"Must login with /adminlogin (password)\n\"" );
				return;
			}
         if ( trap_Argc() < 2 ) 
         { 
            trap_SendServerCommand( ent-g_entities, "print \"Type in /help csprint if you need help with this command.\n\"" ); 
            return; 
         }
		trap_SendServerCommand( -1, va("cp \"%s\"", real_msg) );
	  }
	  		else if ((Q_stricmp(cmd, "demigod") == 0) || (Q_stricmp(cmd, "amdemigod") == 0))
      { //
         int client_id = -1; 
         char   arg1[MAX_STRING_CHARS]; 
		 if (ent->r.svFlags & SVF_ADMIN1)
			{
				if (!(roar_adminControl1.integer & (1 << A_DEMIGOD)))
		{
			trap_SendServerCommand( ent-g_entities, va("print \"DemiGod is not allowed at this administration rank.\n\"") );
			return;
		}
			}
			else if (ent->r.svFlags & SVF_ADMIN2)
			{
				if (!(roar_adminControl2.integer & (1 << A_DEMIGOD)))
		{
			trap_SendServerCommand( ent-g_entities, va("print \"DemiGod is not allowed at this administration rank.\n\"") );
			return;
		}
			}
			if (ent->r.svFlags & SVF_ADMIN3)
			{
				if (!(roar_adminControl3.integer & (1 << A_DEMIGOD)))
		{
			trap_SendServerCommand( ent-g_entities, va("print \"DemiGod is not allowed at this administration rank.\n\"") );
			return;
		}
			}
			if (ent->r.svFlags & SVF_ADMIN4)
			{
				if (!(roar_adminControl4.integer & (1 << A_DEMIGOD)))
		{
			trap_SendServerCommand( ent-g_entities, va("print \"DemiGod is not allowed at this administration rank.\n\"") );
			return;
		}
			}
			if (ent->r.svFlags & SVF_ADMIN5)
			{
				if (!(roar_adminControl5.integer & (1 << A_DEMIGOD)))
		{
			trap_SendServerCommand( ent-g_entities, va("print \"DemiGod is not allowed at this administration rank.\n\"") );
			return;
		}
			}
			if (!(ent->r.svFlags & SVF_ADMIN1) && !(ent->r.svFlags & SVF_ADMIN2) && !(ent->r.svFlags & SVF_ADMIN3) && !(ent->r.svFlags & SVF_ADMIN4) && !(ent->r.svFlags & SVF_ADMIN5)){
				trap_SendServerCommand( ent-g_entities, "print \"Must login with /adminlogin (password)\n\"" );
				return;
			}
         if ( trap_Argc() > 2 ) 
         { 
            trap_SendServerCommand( ent-g_entities, "print \"Usage: ^3/demigod (client)\n\"" ); 
            return; 
         } 
         trap_Argv( 1,  arg1, sizeof(  arg1 ) ); 
         client_id = G_ClientNumberFromArg(  arg1 ); 
		 if ( trap_Argc() < 2 ) 
         { 
			client_id = ent->client->ps.clientNum;
			}
         if (client_id == -1) 
         { 
            trap_SendServerCommand( ent-g_entities, va("print \"Can't find client ID for %s\n\"", arg1 ) ); 
            return; 
         } 
         if (client_id == -2) 
         { 
            trap_SendServerCommand( ent-g_entities, va("print \"Ambiguous client ID for %s\n\"", arg1 ) ); 
            return; 
         } 
         // either we have the client id or the string did not match 
         if (!g_entities[client_id].inuse) 
         { // check to make sure client slot is in use 
            trap_SendServerCommand( ent-g_entities, va("print \"Client %s is not active\n\"", arg1 ) ); 
            return; 
         }
		 if (g_entities[client_id].client->pers.amsleep == 1){
			trap_SendServerCommand( ent-g_entities, va("print \"Client is currently sleeping. Please unsleep them before demigoding them.\nTo wake them, type in /sleep again on the client.\n\"" ) );
			 return;
		 }
		 if (g_entities[client_id].client->pers.amfreeze == 1){
			trap_SendServerCommand( ent-g_entities, va("print \"Client is currently frozen. Please unfreeze them before demigoding them.\nTo unfreeze them, type in /freeze again on the client.\n\"" ) );
			 return;
		 }
		 if (g_entities[client_id].client->pers.ampunish == 1){
			trap_SendServerCommand( ent-g_entities, va("print \"Client is currently being punished. Please unpunish them before demigoding them.\nTo unpunish them, type in /punish again on the client.\n\"" ) );
			 return;
		 }
		 if (g_entities[client_id].client->pers.amsplat == 1){
			trap_SendServerCommand( ent-g_entities, va("print \"Client is currently being splatted. Please wait for them to die.\n\"" ) );
			 return;
		 }
		 if(g_entities[client_id].client->pers.amdemigod)
		 {
			 trap_SendServerCommand( -1, va("cp \"%s^7\n%s\n\"", g_entities[client_id].client->pers.netname, roar_demigod_off_saying.string ) );
			g_entities[client_id].client->noclip = 0;
			g_entities[client_id].client->pers.amdemigod = 0;
			
				}
		 else
		 {
			 //g_entities[client_id].client->noclip = !g_entities[client_id].client->noclip;
			  //trap_SendServerCommand( -1, va("amdemigod %i %i",client_id, 1));
			g_entities[client_id].client->pers.amdemigod = 1;
			 trap_SendServerCommand( -1, va("cp \"%s^7\n%s\n\"", g_entities[client_id].client->pers.netname, roar_demigod_on_saying.string ) ); 

         G_ScreenShake(g_entities[client_id].client->ps.origin, &g_entities[client_id],  3.0f, 2000, qtrue); 
         G_Sound(&g_entities[client_id], CHAN_AUTO, G_SoundIndex("sound/effects/electric_beam_lp.wav"));
				}
	  }
		else if ((Q_stricmp(cmd, "KnockMeDown") == 0) || (Q_stricmp(cmd, "amKnockMeDown") == 0))
	{
		if (ent->client->ps.forceHandExtend == HANDEXTEND_KNOCKDOWN)
		{
			return;
		}
		if (roar_allow_KnockMeDown_command.integer == 0)
		{
			trap_SendServerCommand( ent-g_entities, va("print \"^7KnockMeDown is disabled on this server!\n\"" ) );
		}
		else if (ent->health < 1 || (ent->client->ps.eFlags & EF_DEAD))
			{
			}
		else
		{
			ent->client->ps.velocity[2] = 375;
			ent->client->ps.forceHandExtend = HANDEXTEND_KNOCKDOWN;
			ent->client->ps.forceDodgeAnim = 0;
			ent->client->ps.forceHandExtendTime = level.time + 700;
		}
	}
		else if (Q_stricmp(cmd, "endduel") == 0 )
		{
			gentity_t *duelAgainst = &g_entities[ent->client->ps.duelIndex];
			if (dueltypes[ent->client->ps.clientNum] == 2 && ent->client->ps.duelInProgress)
			{
				ent->client->ps.duelInProgress = 0;
				duelAgainst->client->ps.duelInProgress = 0;

				G_AddEvent(ent, EV_PRIVATE_DUEL, 0);
				G_AddEvent(duelAgainst, EV_PRIVATE_DUEL, 0);
				
				ent->flags &= ~FL_GODMODE;
				duelAgainst->flags &= ~FL_GODMODE;

				trap_SendServerCommand(-1, va("print \"Training duel ended by %s\n\"", ent->client->pers.netname));
				trap_SendServerCommand(-1, va("cp \"Training duel ended by\n%s\n\"", ent->client->pers.netname));
			}
			else {
				trap_SendServerCommand(ent-g_entities, va("print \"^7Must be in a training session to do this command.\n\""));
				return;
			}
		}
		else if (Q_stricmp(cmd, "saberdamages") == 0 ){
			trap_SendServerCommand( ent-g_entities, va("print \"^7g_mSaberDMGTwirl = %s\ng_mSaberDMGKick = %s\ng_mSaberDMGDualKata = %s\ng_mSaberDMGStaffKataMin = %s\ng_mSaberDMGStaffKataMax = %s\ng_mSaberDMGMultiMin = %s\ng_mSaberDMGMultiMax = %s\ng_mSaberDMGSpecialMin = %s\ng_mSaberDMGSpecialMax = %s\ng_mSaberDMGRedNormal = %s\ng_mSaberDMGRedNormalMin = %s\ng_mSaberDMGRedNormalMax = %s\ng_mSaberDMGRedDFAMin = %s\ng_mSaberDMGRedDFAMax = %s\ng_mSaberDMGRedBackMin = %s\ng_mSaberDMGRedBackMax = %s\ng_mSaberDMGYellowNormal = %s\ng_mSaberDMGYellowOverheadMin = %s\ng_mSaberDMGYellowOverheadMax = %s\ng_mSaberDMGYellowBackMin = %s\ng_mSaberDMGYellowBackMax = %s\ng_mSaberDMGBlueNormal = %s\ng_mSaberDMGBlueLungeMin = %s\ng_mSaberDMGBlueLungeMax = %s\ng_mSaberDMGBlueBackMin = %s\ng_mSaberDMGBlueBackMax = %s\n\"", g_mSaberDMGTwirl.string, g_mSaberDMGKick.string, g_mSaberDMGDualKata.string, g_mSaberDMGStaffKataMin.string, g_mSaberDMGStaffKataMax.string, g_mSaberDMGMultiMin.string, g_mSaberDMGMultiMax.string, g_mSaberDMGSpecialMin.string, g_mSaberDMGSpecialMax.string, g_mSaberDMGRedNormal.string, g_mSaberDMGRedNormalMin.string, g_mSaberDMGRedNormalMax.string, g_mSaberDMGRedDFAMin.string, g_mSaberDMGRedDFAMax.string, g_mSaberDMGRedBackMin.string, g_mSaberDMGRedBackMax.string, g_mSaberDMGYellowNormal.string, g_mSaberDMGYellowOverheadMin.string, g_mSaberDMGYellowOverheadMax.string, g_mSaberDMGYellowBackMin.string, g_mSaberDMGYellowBackMax.string, g_mSaberDMGBlueNormal.string, g_mSaberDMGBlueLungeMin.string, g_mSaberDMGBlueLungeMax.string, g_mSaberDMGBlueBackMin.string, g_mSaberDMGBlueBackMax.string ) );
		}
			else if (Q_stricmp(cmd, "hidemotd") == 0 )
		{
			trap_SendServerCommand( ent-g_entities, va("print \"^7Hiding MOTD...\n\"" ) );
			ent->client->csTimeLeft -= 0;
			strcpy(ent->client->csMessage, G_NewString(va(" \n" )));
		}
			else if (Q_stricmp(cmd, "showmotd") == 0 )
			{
				trap_SendServerCommand( ent-g_entities, va("print \"^7Showing MOTD...\n\"" ) );
		strcpy(ent->client->csMessage, G_NewString(va("^0*^1%s^0*\n\n^7%s\n", GAMEVERSION, roar_motd_line.string )));
		ent->client->csTimeLeft = x_cstime.integer;
			}
			else if (Q_stricmp(cmd, "freezemotd") == 0 )
			{
			trap_SendServerCommand( ent-g_entities, va("print \"^7Freezing MOTD... ^3(TYPE IN !hidemotd OR /hidemotd TO HIDE IT!)\n\"" ) );
		strcpy(ent->client->csMessage, G_NewString(va("^0*^1%s^0*\n\n^7%s\n", GAMEVERSION, roar_motd_line.string )));
		ent->client->csTimeLeft = Q3_INFINITE;
			}
				else if (Q_stricmp(cmd, "adminsit") == 0)
		{
			if (ent->r.svFlags & SVF_ADMIN1)
			{
				if (!(roar_adminControl1.integer & (1 << A_ADMINSIT)))
		{
			trap_SendServerCommand( ent-g_entities, va("print \"AdminSit is not allowed at this administration rank.\n\"") );
			return;
		}
			}
			else if (ent->r.svFlags & SVF_ADMIN2)
			{
				if (!(roar_adminControl2.integer & (1 << A_ADMINSIT)))
		{
			trap_SendServerCommand( ent-g_entities, va("print \"AdminSit is not allowed at this administration rank.\n\"") );
			return;
		}
			}
			if (ent->r.svFlags & SVF_ADMIN3)
			{
				if (!(roar_adminControl3.integer & (1 << A_ADMINSIT)))
		{
			trap_SendServerCommand( ent-g_entities, va("print \"AdminSit is not allowed at this administration rank.\n\"") );
			return;
		}
			}
			if (ent->r.svFlags & SVF_ADMIN4)
			{
				if (!(roar_adminControl4.integer & (1 << A_ADMINSIT)))
		{
			trap_SendServerCommand( ent-g_entities, va("print \"AdminSit is not allowed at this administration rank.\n\"") );
			return;
		}
			}
			if (ent->r.svFlags & SVF_ADMIN5)
			{
				if (!(roar_adminControl5.integer & (1 << A_ADMINSIT)))
		{
			trap_SendServerCommand( ent-g_entities, va("print \"AdminSit is not allowed at this administration rank.\n\"") );
			return;
		}
			}
			if (!(ent->r.svFlags & SVF_ADMIN1) && !(ent->r.svFlags & SVF_ADMIN2) && !(ent->r.svFlags & SVF_ADMIN3) && !(ent->r.svFlags & SVF_ADMIN4) && !(ent->r.svFlags & SVF_ADMIN5)){
				trap_SendServerCommand( ent-g_entities, "print \"Must login with /adminlogin (password)\n\"" );
				return;
			}
			if (ent->client->ps.legsAnim == BOTH_SIT2)
			{
				ent->client->emote_freeze=0;
				ent->client->ps.saberCanThrow = qtrue;
			}
			else
			{
				ent->client->ps.saberHolstered;
				ent->client->ps.saberCanThrow = qfalse;
				ent->client->emote_freeze=1;
				StandardSetBodyAnim(ent, BOTH_SIT2, SETANIM_FLAG_OVERRIDE|SETANIM_FLAG_HOLD|SETANIM_FLAG_HOLDLESS);
				ent->client->ps.saberMove = LS_NONE;
				ent->client->ps.saberBlocked = 0;
				ent->client->ps.saberBlocking = 0;
			}
		}
		 else if ((Q_stricmp(cmd, "splat") == 0) || (Q_stricmp(cmd, "amsplat") == 0))
      { // freeze a player 
         int client_id = -1; 
         char   arg1[MAX_STRING_CHARS];
		 if (ent->r.svFlags & SVF_ADMIN1)
		{
				if (!(roar_adminControl1.integer & (1 << A_SPLAT)))
		{
			trap_SendServerCommand( ent-g_entities, va("print \"Splat is not allowed at this administration rank.\n\"") );
			return;
		}
			}
			else if (ent->r.svFlags & SVF_ADMIN2)
			{
				if (!(roar_adminControl2.integer & (1 << A_SPLAT)))
		{
			trap_SendServerCommand( ent-g_entities, va("print \"Splat is not allowed at this administration rank.\n\"") );
			return;
		}
			}
			if (ent->r.svFlags & SVF_ADMIN3)
			{
				if (!(roar_adminControl3.integer & (1 << A_SPLAT)))
		{
			trap_SendServerCommand( ent-g_entities, va("print \"Splat is not allowed at this administration rank.\n\"") );
			return;
		}
			}
			if (ent->r.svFlags & SVF_ADMIN4)
			{
				if (!(roar_adminControl4.integer & (1 << A_SPLAT)))
		{
			trap_SendServerCommand( ent-g_entities, va("print \"Splat is not allowed at this administration rank.\n\"") );
			return;
		}
			}
			if (ent->r.svFlags & SVF_ADMIN5)
			{
				if (!(roar_adminControl5.integer & (1 << A_SPLAT)))
		{
			trap_SendServerCommand( ent-g_entities, va("print \"Splat is not allowed at this administration rank.\n\"") );
			return;
		}
			}
			if (!(ent->r.svFlags & SVF_ADMIN1) && !(ent->r.svFlags & SVF_ADMIN2) && !(ent->r.svFlags & SVF_ADMIN3) && !(ent->r.svFlags & SVF_ADMIN4) && !(ent->r.svFlags & SVF_ADMIN5)){
				trap_SendServerCommand( ent-g_entities, "print \"Must login with /adminlogin (password)\n\"" );
				return;
			}
         if ( trap_Argc() < 2 ) 
         { 
            trap_SendServerCommand( ent-g_entities, "print \"Type in /help splat if you need help with this command.\n\"" ); 
            return; 
         } 
         trap_Argv( 1,  arg1, sizeof(  arg1 ) ); 
         client_id = G_ClientNumberFromArg(  arg1 ); 
         if (client_id == -1) 
         { 
            trap_SendServerCommand( ent-g_entities, va("print \"Can't find client ID for %s\n\"", arg1 ) ); 
            return; 
         } 
         if (client_id == -2) 
         { 
            trap_SendServerCommand( ent-g_entities, va("print \"Ambiguous client ID for %s\n\"", arg1 ) ); 
            return; 
         } 
         // either we have the client id or the string did not match 
         if (!g_entities[client_id].inuse) 
         { // check to make sure client slot is in use 
            trap_SendServerCommand( ent-g_entities, va("print \"Client %s is not active\n\"", arg1 ) ); 
            return; 
         }
		 if (g_entities[client_id].client->pers.tzone == 1){
			 trap_SendServerCommand( ent-g_entities, va("print \"Can't splat a protected client.\n\"", arg1 ) );
			 return;
		 }
		 if (g_entities[client_id].client->pers.amdemigod){
			 trap_SendServerCommand( ent-g_entities, va("print \"Client is currently a demigod. Please un-demigod them.\nTo un-demigod them, type in /demigod on them again.\n\"" ) );
			 return;
		 }
		 if (g_entities[client_id].client->emote_freeze==1)
				{
					g_entities[client_id].client->emote_freeze=0;
				}
		 if (g_entities[client_id].health > 0)
		 {
		G_ScreenShake(g_entities[client_id].client->ps.origin, &g_entities[client_id],  3.0f, 2000, qtrue);
		trap_SendServerCommand( -1, va("cp \"%s^7\n%s\n\"", g_entities[client_id].client->pers.netname, roar_splat_saying.string ) );
		g_entities[client_id].client->ps.velocity[2] += 1000;
		g_entities[client_id].client->pers.amsplat = 1;
		g_entities[client_id].client->ps.forceDodgeAnim = 0;
		g_entities[client_id].client->ps.forceHandExtend = HANDEXTEND_KNOCKDOWN;
		g_entities[client_id].client->ps.forceHandExtendTime = level.time + Q3_INFINITE;
		g_entities[client_id].client->ps.quickerGetup = qfalse;
		 }
		 else
		 {
			return;
		 }
      }
	  		else if ((Q_stricmp(cmd, "protect") == 0) || (Q_stricmp(cmd, "amprotect") == 0))
		{
			gentity_t * targetplayer;
			int i;
			int client_id = -1; 
			char   arg1[MAX_STRING_CHARS];
			if (ent->r.svFlags & SVF_ADMIN1)
			{
				if (!(roar_adminControl1.integer & (1 << A_PROTECT)))
		{
			trap_SendServerCommand( ent-g_entities, va("print \"Protect is not allowed at this administration rank.\n\"") );
			return;
		}
			}
			else if (ent->r.svFlags & SVF_ADMIN2)
			{
				if (!(roar_adminControl2.integer & (1 << A_PROTECT)))
		{
			trap_SendServerCommand( ent-g_entities, va("print \"Protect is not allowed at this administration rank.\n\"") );
			return;
		}
			}
			if (ent->r.svFlags & SVF_ADMIN3)
			{
				if (!(roar_adminControl3.integer & (1 << A_PROTECT)))
		{
			trap_SendServerCommand( ent-g_entities, va("print \"Protect is not allowed at this administration rank.\n\"") );
			return;
		}
			}
			if (ent->r.svFlags & SVF_ADMIN4)
			{
				if (!(roar_adminControl4.integer & (1 << A_PROTECT)))
		{
			trap_SendServerCommand( ent-g_entities, va("print \"Protect is not allowed at this administration rank.\n\"") );
			return;
		}
			}
			if (ent->r.svFlags & SVF_ADMIN5)
			{
				if (!(roar_adminControl5.integer & (1 << A_PROTECT)))
		{
			trap_SendServerCommand( ent-g_entities, va("print \"Protect is not allowed at this administration rank.\n\"") );
			return;
		}
			}
			if (!(ent->r.svFlags & SVF_ADMIN1) && !(ent->r.svFlags & SVF_ADMIN2) && !(ent->r.svFlags & SVF_ADMIN3) && !(ent->r.svFlags & SVF_ADMIN4) && !(ent->r.svFlags & SVF_ADMIN5)){
				trap_SendServerCommand( ent-g_entities, "print \"Must login with /adminlogin (password)\n\"" );
				return;
			}
			if ( trap_Argc() > 2 )
			{
				trap_SendServerCommand( ent-g_entities, "print \"Usage: ^3/protect (client)\n\"" );
				return;
			}
			trap_Argv( 1, arg1, sizeof( arg1 ) );
			if( Q_stricmp( arg1, "+all" ) == 0 ){
			for( i = 0; i < level.maxclients; i++ )
		{
			targetplayer = &g_entities[i];

			if( targetplayer->client && targetplayer->client->pers.connected ){
				if (!targetplayer->client->pers.tzone){
					targetplayer->client->pers.tzone = 1;
					targetplayer->client->pers.autopro = 0;
				}
			}
		}
		trap_SendServerCommand( -1, va("cp \"^7%s\n\"", roar_protect_on_ALL_saying.string ) );
		trap_SendServerCommand( -1, va("print \"^7%s\n\"", roar_protect_on_ALL_saying.string ) );
		return;
			}
			else if( Q_stricmp( arg1, "-all" ) == 0 ){
			for( i = 0; i < level.maxclients; i++ )
		{
			targetplayer = &g_entities[i];

			if( targetplayer->client && targetplayer->client->pers.connected ){
				if (targetplayer->client->pers.tzone){
					targetplayer->client->pers.tzone = 0;
					targetplayer->client->pers.autopro = 0;
				}
			}
		}
		trap_SendServerCommand( -1, va("cp \"^7%s\n\"", roar_protect_off_ALL_saying.string ) );
		trap_SendServerCommand( -1, va("print \"^7%s\n\"", roar_protect_off_ALL_saying.string ) );
		return;
			}
			else {
			client_id = G_ClientNumberFromArg( arg1 );
			}
			if ( trap_Argc() < 2 ) 
         { 
			client_id = ent->client->ps.clientNum;
			}
			if (client_id == -1)
			{
				trap_SendServerCommand( ent-g_entities, va("print \"Can't find client ID for %s\n\"", arg1 ) );
				return;
			}
			if (client_id == -2)
			{
				trap_SendServerCommand( ent-g_entities, va("print \"Ambiguous client ID for %s\n\"", arg1 ) );
				return;
			}
			if (g_entities[client_id].client->ps.eFlags2 == EF2_HELD_BY_MONSTER)
			{
				return;
			}
			if (g_entities[client_id].client->pers.amsleep == 1){
				trap_SendServerCommand( ent-g_entities, va("print \"Client is currently sleeping. Wake him to protect him.\n\"" ) );
				return;
			}
			if (g_entities[client_id].client->pers.ampunish == 1){
				trap_SendServerCommand( ent-g_entities, va("print \"Client is currently being punished. Unpunish him to protect him.\n\"" ) );
				return;
			}
	if (g_entities[client_id].health <= 0)
		 {
			return;
		 }
			// either we have the client id or the string did not match
			if (!g_entities[client_id].inuse)
			{ // check to make sure client slot is in use
				trap_SendServerCommand( ent-g_entities, va("print \"Client %s is not active\n\"", arg1 ) );
				return;
			}
			if (g_entities[client_id].client->pers.tzone == 1)
		 {
		 g_entities[client_id].client->pers.tzone = 0;
		 g_entities[client_id].client->pers.autopro = 0;
		g_entities[client_id].takedamage = qtrue;
		g_entities[client_id].client->ps.eFlags &= ~EF_INVULNERABLE;
         trap_SendServerCommand( -1, va("cp \"%s^7\n%s\n\"", g_entities[client_id].client->pers.netname, roar_protect_off_saying.string ) ); 
		 }
		 else {
		g_entities[client_id].client->pers.tzone = 1;
		trap_SendServerCommand( -1, va("cp \"%s^7\n%s\n\"", g_entities[client_id].client->pers.netname, roar_protect_on_saying.string ) );
		}
		 if (g_entities[client_id].client->pers.autopro == 1)
		 {
		g_entities[client_id].client->pers.tzone = 1;
		g_entities[client_id].client->pers.autopro = 0;
         trap_SendServerCommand( -1, va("cp \"%s^7\n%s\n\"", g_entities[client_id].client->pers.netname, roar_protect_on_saying.string ) ); 
		 }
		}
		else if (( Q_stricmp (cmd, "lockteam") == 0 ) || ( Q_stricmp (cmd, "amlockteam") == 0 )){
	char teamname[MAX_TEAMNAME];
	if (ent->r.svFlags & SVF_ADMIN1)
			{
				if (!(roar_adminControl1.integer & (1 << A_FORCETEAM)))
		{
			trap_SendServerCommand( ent-g_entities, va("print \"LockTeam is not allowed at this administration rank.\n\"") );
			return;
		}
			}
			else if (ent->r.svFlags & SVF_ADMIN2)
			{
				if (!(roar_adminControl2.integer & (1 << A_LOCKTEAM)))
		{
			trap_SendServerCommand( ent-g_entities, va("print \"LockTeam is not allowed at this administration rank.\n\"") );
			return;
		}
			}
			if (ent->r.svFlags & SVF_ADMIN3)
			{
				if (!(roar_adminControl3.integer & (1 << A_LOCKTEAM)))
		{
			trap_SendServerCommand( ent-g_entities, va("print \"LockTeam is not allowed at this administration rank.\n\"") );
			return;
		}
			}
			if (ent->r.svFlags & SVF_ADMIN4)
			{
				if (!(roar_adminControl4.integer & (1 << A_LOCKTEAM)))
		{
			trap_SendServerCommand( ent-g_entities, va("print \"LockTeam is not allowed at this administration rank.\n\"") );
			return;
		}
			}
			if (ent->r.svFlags & SVF_ADMIN5)
			{
				if (!(roar_adminControl5.integer & (1 << A_LOCKTEAM)))
		{
			trap_SendServerCommand( ent-g_entities, va("print \"LockTeam is not allowed at this administration rank.\n\"") );
			return;
		}
			}
			if (!(ent->r.svFlags & SVF_ADMIN1) && !(ent->r.svFlags & SVF_ADMIN2) && !(ent->r.svFlags & SVF_ADMIN3) && !(ent->r.svFlags & SVF_ADMIN4) && !(ent->r.svFlags & SVF_ADMIN5)){
				trap_SendServerCommand( ent-g_entities, "print \"Must login with /adminlogin (password)\n\"" );
				return;
			}
	if ( g_gametype.integer >= GT_TEAM || g_gametype.integer == GT_FFA ) {
		
		if ( trap_Argc() != 2 ){
			trap_SendServerCommand( ent-g_entities, va("print \"Usage: /lockteam (team)\n^3TEAMS = Spectator, Blue, Red, Join\n\""));
			return;
		}
		trap_Argv( 1, teamname, sizeof( teamname ) );
				
		if ( !Q_stricmp( teamname, "red" ) || !Q_stricmp( teamname, "r" ) ) {
			if (level.isLockedred == qfalse){
			level.isLockedred = qtrue;
			trap_SendServerCommand( -1, va("cp \"^7The ^1Red ^7team is now ^1Locked^7.\n\""));
			trap_SendServerCommand( -1, va("print \"^7The ^1Red ^7team is now ^1Locked^7.\n\""));
			}
			else {
			level.isLockedred = qfalse;
			trap_SendServerCommand( -1, va("cp \"^7The ^1Red ^7team is now ^2unLocked^7.\n\""));
			trap_SendServerCommand( -1, va("print \"^7The ^1Red ^7team is now ^2unLocked^7.\n\""));
			}
		}
		else if ( !Q_stricmp( teamname, "blue" ) || !Q_stricmp( teamname, "b" ) ) {
			if (level.isLockedblue == qfalse){
			level.isLockedblue = qtrue;
			trap_SendServerCommand( -1, va("cp \"^7The ^4Blue ^7team is now ^1Locked^7.\n\""));
			trap_SendServerCommand( -1, va("print \"^7The ^4Blue ^7team is now ^1Locked^7.\n\""));
			}
			else {
			level.isLockedblue = qfalse;
			trap_SendServerCommand( -1, va("cp \"^7The ^4Blue ^7team is now ^2unLocked^7.\n\""));
			trap_SendServerCommand( -1, va("print \"^7The ^4Blue ^7team is now ^2unLocked^7.\n\""));
			}
		}
		else if( !Q_stricmp( teamname, "spectator" ) || !Q_stricmp( teamname, "s" ) || !Q_stricmp( teamname, "spec" ) || !Q_stricmp( teamname, "spectate" ) ) {
			if (level.isLockedspec == qfalse){
			level.isLockedspec = qtrue;
			trap_SendServerCommand( -1, va("cp \"^7The ^3Spectator ^7team is now ^1Locked^7.\n\""));
			trap_SendServerCommand( -1, va("print \"^7The ^3Spectator ^7team is now ^1Locked^7.\n\""));
			}
			else {
			level.isLockedspec = qfalse;
			trap_SendServerCommand( -1, va("cp \"^7The ^3Spectator ^7team is now ^2unLocked^7.\n\""));
			trap_SendServerCommand( -1, va("print \"^7The ^3Spectator ^7team is now ^2unLocked^7.\n\""));
			}
		}
		else if( !Q_stricmp( teamname, "join" ) || !Q_stricmp( teamname, "free" ) || !Q_stricmp( teamname, "enter" )
			 || !Q_stricmp( teamname, "f" ) || !Q_stricmp( teamname, "j" )) {
			if (level.isLockedjoin == qfalse){
			level.isLockedjoin = qtrue;
			trap_SendServerCommand( -1, va("cp \"^7The ^2Join ^7team is now ^1Locked^7.\n\""));
			trap_SendServerCommand( -1, va("print \"^7The ^2Join ^7team is now ^1Locked^7.\n\""));
			}
			else {
			level.isLockedjoin = qfalse;
			trap_SendServerCommand( -1, va("cp \"^7The ^2Join ^7team is now ^2unLocked^7.\n\""));
			trap_SendServerCommand( -1, va("print \"^7The ^2Join ^7team is now ^2unLocked^7.\n\""));
			}
		}
		else {
			trap_SendServerCommand( ent-g_entities, va("print \"Usage: /lockteam (team)\n^3TEAMS = Spectator, Blue, Red, Join\n\""));
			return;
		}
	}
	else
	{
		G_Printf("^1Warning^7: You cannot Lock the teams in this gameplay\n");
		return;
	}
	}
	  		else if ((Q_stricmp(cmd, "changemap") == 0) || (Q_stricmp(cmd, "amchangemap") == 0) || (Q_stricmp(cmd, "ammap") == 0))

		{
			char   arg1[MAX_STRING_CHARS]; 
			if (ent->r.svFlags & SVF_ADMIN1)
		{
				if (!(roar_adminControl1.integer & (1 << A_CHANGEMAP)))
		{
			trap_SendServerCommand( ent-g_entities, va("print \"ChangeMap is not allowed at this administration rank.\n\"") );
			return;
		}
			}
			else if (ent->r.svFlags & SVF_ADMIN2)
			{
				if (!(roar_adminControl2.integer & (1 << A_CHANGEMAP)))
		{
			trap_SendServerCommand( ent-g_entities, va("print \"ChangeMap is not allowed at this administration rank.\n\"") );
			return;
		}
			}
			if (ent->r.svFlags & SVF_ADMIN3)
			{
				if (!(roar_adminControl3.integer & (1 << A_CHANGEMAP)))
		{
			trap_SendServerCommand( ent-g_entities, va("print \"ChangeMap is not allowed at this administration rank.\n\"") );
			return;
		}
			}
			if (ent->r.svFlags & SVF_ADMIN4)
			{
				if (!(roar_adminControl4.integer & (1 << A_CHANGEMAP)))
		{
			trap_SendServerCommand( ent-g_entities, va("print \"ChangeMap is not allowed at this administration rank.\n\"") );
			return;
		}
			}
			if (ent->r.svFlags & SVF_ADMIN5)
			{
				if (!(roar_adminControl5.integer & (1 << A_CHANGEMAP)))
		{
			trap_SendServerCommand( ent-g_entities, va("print \"ChangeMap is not allowed at this administration rank.\n\"") );
			return;
		}
			}
			if (!(ent->r.svFlags & SVF_ADMIN1) && !(ent->r.svFlags & SVF_ADMIN2) && !(ent->r.svFlags & SVF_ADMIN3) && !(ent->r.svFlags & SVF_ADMIN4) && !(ent->r.svFlags & SVF_ADMIN5)){
				trap_SendServerCommand( ent-g_entities, "print \"Must login with /adminlogin (password)\n\"" );
				return;
			}
			if ( trap_Argc() < 2 )
			{
				trap_SendServerCommand( ent-g_entities, "print \"Type in /help changemap if you need help with this command.\n\"" );
				return;
			}
			trap_Argv( 1, arg1, sizeof( arg1 ) );
			trap_SendConsoleCommand( EXEC_APPEND, va("map %s\n", arg1));

		}
		// MJN - Ignore // FULL CREDIT TO MJN!
	else if (Q_stricmp ( cmd, "ignore" ) == 0 ){
	int ignoree = -1;
	qboolean ignore;
	char   name[MAX_STRING_CHARS];
	if( trap_Argc() != 2 ){
		trap_SendServerCommand( ent-g_entities, "print \"Usage: ignore <client>\n\"");
		return;
	}
	trap_Argv( 1, name, sizeof( name ) );
	ignoree = G_ClientNumberFromArg( name );
         if (ignoree == -1) 
         { 
            trap_SendServerCommand( ent-g_entities, va("print \"Can't find client ID for %s\n\"", name ) ); 
            return; 
         } 
         if (ignoree == -2) 
         { 
            trap_SendServerCommand( ent-g_entities, va("print \"Ambiguous client ID for %s\n\"", name ) ); 
            return; 
         } 
         if (!g_entities[ignoree].inuse) 
         {
            trap_SendServerCommand( ent-g_entities, va("print \"Client %s is not active\n\"", name ) ); 
            return; 
         }
	ignore = G_IsClientChatIgnored ( ent->client->ps.clientNum, ignoree ) ? qfalse : qtrue;
	if ( ignoree == ent->client->ps.clientNum )
		{
			trap_SendServerCommand( ent-g_entities, va("print \"You cant ignore yourself.\n\""));
			return;
		}
	G_IgnoreClientChat ( ent->client->ps.clientNum, ignoree, ignore);
	if ( ignore )
		{
			trap_SendServerCommand( ent-g_entities, va("print \"%s ^7is now being ignored.\n\"", g_entities[ignoree].client->pers.netname));
			trap_SendServerCommand( ignoree, va("print \"%s ^7is now ignoring you.\n\"", ent->client->pers.netname));
		}
		else
		{
			trap_SendServerCommand( ent-g_entities, va("print \"%s ^7is now unignored.\n\"", g_entities[ignoree].client->pers.netname));
			trap_SendServerCommand( ignoree, va("print \"%s ^7has unignored you.\n\"", ent->client->pers.netname));
		}
	}
		else if (Q_stricmp(cmd, "who") == 0) 
      {
  int i;
	trap_SendServerCommand(ent-g_entities, va("print \"\n^1Current clients connected & client status\n\n^3===================================\n\""));
   for(i = 0; i < level.maxclients; i++) { 
      if(g_entities[i].client->pers.connected == CON_CONNECTED) { 
         trap_SendServerCommand(ent-g_entities, va("print \"%i %s\"", i, g_entities[i].client->pers.netname));
		 if (!g_entities[i].client->pers.amfreeze && !g_entities[i].client->pers.iamclan && !g_entities[i].client->pers.iamanadmin && !g_entities[i].client->ojpClientPlugIn && !g_entities[i].client->pers.silent && !g_entities[i].client->pers.silent2 && !g_entities[i].client->pers.amterminator && !g_entities[i].client->pers.amempower && !(g_entities[i].r.svFlags & SVF_BOT) && !g_entities[i].client->pers.ampunish && !g_entities[i].client->pers.amsleep){
			 trap_SendServerCommand(ent-g_entities, va("print \"\n\""));
		 }
		 if (g_entities[i].client->pers.amfreeze){
			 trap_SendServerCommand(ent-g_entities, va("print \" ^5(frozen)\""));
			 if (!g_entities[i].client->pers.iamclan && !g_entities[i].client->pers.iamanadmin && !g_entities[i].client->ojpClientPlugIn && !g_entities[i].client->pers.silent && !g_entities[i].client->pers.silent2 && !g_entities[i].client->pers.amterminator && !g_entities[i].client->pers.amempower && !(g_entities[i].r.svFlags & SVF_BOT) && !g_entities[i].client->pers.ampunish && !g_entities[i].client->pers.amsleep){
				trap_SendServerCommand(ent-g_entities, va("print \"\n\""));
			 }
		 }
		 if (g_entities[i].client->pers.iamclan){
			 trap_SendServerCommand(ent-g_entities, va("print \" ^1(clan)\""));
			 if (!g_entities[i].client->pers.iamanadmin && !g_entities[i].client->ojpClientPlugIn && !g_entities[i].client->pers.silent && !g_entities[i].client->pers.silent2 && !g_entities[i].client->pers.amterminator && !g_entities[i].client->pers.amempower && !(g_entities[i].r.svFlags & SVF_BOT) && !g_entities[i].client->pers.ampunish && !g_entities[i].client->pers.amsleep){
				trap_SendServerCommand(ent-g_entities, va("print \"\n\""));
			 }
		 }
		 if (g_entities[i].client->pers.silent){
			 trap_SendServerCommand(ent-g_entities, va("print \" ^6(silenced)\""));
			 if (!g_entities[i].client->pers.iamanadmin && !g_entities[i].client->ojpClientPlugIn && !g_entities[i].client->pers.silent2 && !g_entities[i].client->pers.amterminator && !g_entities[i].client->pers.amempower && !(g_entities[i].r.svFlags & SVF_BOT) && !g_entities[i].client->pers.ampunish && !g_entities[i].client->pers.amsleep){
				trap_SendServerCommand(ent-g_entities, va("print \"\n\""));
			 }
		 }
		 if (g_entities[i].client->pers.silent2){
			 trap_SendServerCommand(ent-g_entities, va("print \" ^6(in-silence)\""));
			 if (!g_entities[i].client->pers.iamanadmin && !g_entities[i].client->ojpClientPlugIn && !g_entities[i].client->pers.amterminator && !g_entities[i].client->pers.amempower && !(g_entities[i].r.svFlags & SVF_BOT) && !g_entities[i].client->pers.ampunish && !g_entities[i].client->pers.amsleep){
				trap_SendServerCommand(ent-g_entities, va("print \"\n\""));
			 }
		 }
		 if (g_entities[i].client->pers.amterminator){
			 trap_SendServerCommand(ent-g_entities, va("print \" ^4(terminator)\""));
			 if (!g_entities[i].client->pers.amempower && !g_entities[i].client->pers.iamanadmin && !g_entities[i].client->ojpClientPlugIn && !(g_entities[i].r.svFlags & SVF_BOT) && !g_entities[i].client->pers.ampunish && !g_entities[i].client->pers.amsleep){
				trap_SendServerCommand(ent-g_entities, va("print \"\n\""));
			 }
		 }
		 if (g_entities[i].client->pers.amempower){
			 trap_SendServerCommand(ent-g_entities, va("print \" ^4(empowered)\""));
			 if (!g_entities[i].client->pers.iamanadmin && !g_entities[i].client->ojpClientPlugIn && !(g_entities[i].r.svFlags & SVF_BOT) && !g_entities[i].client->pers.ampunish && !g_entities[i].client->pers.amsleep){
				trap_SendServerCommand(ent-g_entities, va("print \"\n\""));
			 }
		 }
		 if (g_entities[i].client->pers.iamanadmin){
			 trap_SendServerCommand(ent-g_entities, va("print \" ^3(admin)\""));
			 if (!g_entities[i].client->ojpClientPlugIn && !(g_entities[i].r.svFlags & SVF_BOT) && !g_entities[i].client->pers.ampunish && !g_entities[i].client->pers.amsleep){
				trap_SendServerCommand(ent-g_entities, va("print \"\n\""));
			 }
		 }
		 if (g_entities[i].client->pers.ampunish){
			 trap_SendServerCommand(ent-g_entities, va("print \" ^1(punished)\""));
			 if (!g_entities[i].client->ojpClientPlugIn && !(g_entities[i].r.svFlags & SVF_BOT) && !g_entities[i].client->pers.amsleep){
				trap_SendServerCommand(ent-g_entities, va("print \"\n\""));
			 }
		 }
		 if (g_entities[i].client->pers.amsleep){
			 trap_SendServerCommand(ent-g_entities, va("print \" ^5(sleeping)\""));
			 if (!g_entities[i].client->ojpClientPlugIn && !(g_entities[i].r.svFlags & SVF_BOT)){
				trap_SendServerCommand(ent-g_entities, va("print \"\n\""));
			 }
		 }
		 //Bots n Plugins
		 if (g_entities[i].r.svFlags & SVF_BOT){
			 trap_SendServerCommand(ent-g_entities, va("print \" ^7(bot)\""));
			 //if (!g_entities[i].client->pers.amfreeze && !g_entities[i].client->pers.iamclan && !g_entities[i].client->pers.iamanadmin && !g_entities[i].client->ojpClientPlugIn && !g_entities[i].client->pers.silent && !g_entities[i].client->pers.silent2 && !g_entities[i].client->pers.amterminator && !g_entities[i].client->pers.amempower){
				 trap_SendServerCommand(ent-g_entities, va("print \"\n\""));
			 //}
				 //NOTE: End of the road for the bot. Mark it off with \n
		 }
		 else if (g_entities[i].client->ojpClientPlugIn && !(g_entities[i].r.svFlags & SVF_BOT) && !g_entities[i].client->ojpClientPlugIn2 && !g_entities[i].client->ojpClientPlugIn3){
			 trap_SendServerCommand(ent-g_entities, va("print \" ^2(plugin ^1v1.00^2)\""));
			 //if (!g_entities[i].client->pers.amfreeze && !g_entities[i].client->pers.iamclan && !g_entities[i].client->pers.iamanadmin && !g_entities[i].client->pers.silent && !g_entities[i].client->pers.silent2 && !g_entities[i].client->pers.amterminator && !g_entities[i].client->pers.amempower){
				trap_SendServerCommand(ent-g_entities, va("print \"\n\""));
			 //}
		 }
		 else if (g_entities[i].client->ojpClientPlugIn && !(g_entities[i].r.svFlags & SVF_BOT) && g_entities[i].client->ojpClientPlugIn2 && !g_entities[i].client->ojpClientPlugIn3){
			 trap_SendServerCommand(ent-g_entities, va("print \" ^2(plugin ^1v1.05^2)\""));
			 //if (!g_entities[i].client->pers.amfreeze && !g_entities[i].client->pers.iamclan && !g_entities[i].client->pers.iamanadmin && !g_entities[i].client->pers.silent && !g_entities[i].client->pers.silent2 && !g_entities[i].client->pers.amterminator && !g_entities[i].client->pers.amempower){
				trap_SendServerCommand(ent-g_entities, va("print \"\n\""));
			 //}
		 }
		 else if (g_entities[i].client->ojpClientPlugIn && !(g_entities[i].r.svFlags & SVF_BOT) && g_entities[i].client->ojpClientPlugIn2 && g_entities[i].client->ojpClientPlugIn3){
			 trap_SendServerCommand(ent-g_entities, va("print \" ^2(plugin ^1v1.06^2)\""));
			 //if (!g_entities[i].client->pers.amfreeze && !g_entities[i].client->pers.iamclan && !g_entities[i].client->pers.iamanadmin && !g_entities[i].client->pers.silent && !g_entities[i].client->pers.silent2 && !g_entities[i].client->pers.amterminator && !g_entities[i].client->pers.amempower){
				trap_SendServerCommand(ent-g_entities, va("print \"\n\""));
			 //}
		 }
		 //
      }
}
   trap_SendServerCommand(ent-g_entities, va("print \"^3===================================\n\n\""));
	  }
	    else if ((Q_stricmp(cmd, "freeze") == 0) || (Q_stricmp(cmd, "amfreeze") == 0)) 
      { // freeze a player
		gentity_t * targetplayer;
		int i;
        int client_id = -1; 
        char   arg1[MAX_STRING_CHARS]; 

		 if (ent->r.svFlags & SVF_ADMIN1)
			{
				if (!(roar_adminControl1.integer & (1 << A_FREEZE)))
		{
			trap_SendServerCommand( ent-g_entities, va("print \"Freeze is not allowed at this administration rank.\n\"") );
			return;
		}
			}
			else if (ent->r.svFlags & SVF_ADMIN2)
			{
				if (!(roar_adminControl2.integer & (1 << A_FREEZE)))
		{
			trap_SendServerCommand( ent-g_entities, va("print \"Freeze is not allowed at this administration rank.\n\"") );
			return;
		}
			}
			if (ent->r.svFlags & SVF_ADMIN3)
			{
				if (!(roar_adminControl3.integer & (1 << A_FREEZE)))
		{
			trap_SendServerCommand( ent-g_entities, va("print \"Freeze is not allowed at this administration rank.\n\"") );
			return;
		}
			}
			if (ent->r.svFlags & SVF_ADMIN4)
			{
				if (!(roar_adminControl4.integer & (1 << A_FREEZE)))
		{
			trap_SendServerCommand( ent-g_entities, va("print \"Freeze is not allowed at this administration rank.\n\"") );
			return;
		}
			}
			if (ent->r.svFlags & SVF_ADMIN5)
			{
				if (!(roar_adminControl5.integer & (1 << A_FREEZE)))
		{
			trap_SendServerCommand( ent-g_entities, va("print \"Freeze is not allowed at this administration rank.\n\"") );
			return;
		}
			}
			if (!(ent->r.svFlags & SVF_ADMIN1) && !(ent->r.svFlags & SVF_ADMIN2) && !(ent->r.svFlags & SVF_ADMIN3) && !(ent->r.svFlags & SVF_ADMIN4) && !(ent->r.svFlags & SVF_ADMIN5)){
				trap_SendServerCommand( ent-g_entities, "print \"Must login with /adminlogin (password)\n\"" );
				return;
			}
         if ( trap_Argc() != 2 ) 
         { 
            trap_SendServerCommand( ent-g_entities, "print \"Usage: ^3/freeze (client)\n\"" ); 
            return; 
         }
		 trap_Argv( 1,  arg1, sizeof(  arg1 ) );
		 if( Q_stricmp( arg1, "+all" ) == 0 ){
			for( i = 0; i < level.maxclients; i++ )
		{
			targetplayer = &g_entities[i];

			if( targetplayer->client && targetplayer->client->pers.connected ){
				if (!targetplayer->client->pers.amfreeze){
					targetplayer->client->pers.amfreeze = 1;
				}
			}
		}
		trap_SendServerCommand( -1, va("print \"^7%s\n\"", roar_freeze_on_ALL_saying.string ) );
		trap_SendServerCommand( -1, va("cp \"^7%s\n\"", roar_freeze_on_ALL_saying.string ) );
		return;
		 }
		 else if( Q_stricmp( arg1, "-all" ) == 0 ){
			for( i = 0; i < level.maxclients; i++ )
		{
			targetplayer = &g_entities[i];

			if( targetplayer->client && targetplayer->client->pers.connected ){
				if (targetplayer->client->pers.amfreeze){
					targetplayer->client->pers.amfreeze = 0; 
					targetplayer->client->ps.saberCanThrow = qtrue;
					targetplayer->client->ps.forceRestricted = qfalse;
					targetplayer->takedamage = qtrue;
				}
			}
		}
		trap_SendServerCommand( -1, va("print \"^7%s\n\"", roar_freeze_off_ALL_saying.string ) );
		trap_SendServerCommand( -1, va("cp \"^7%s\n\"", roar_freeze_off_ALL_saying.string ) );
		return;
		 }
		 else {
         client_id = G_ClientNumberFromArg(  arg1 );
		 }
         if (client_id == -1) 
         { 
            trap_SendServerCommand( ent-g_entities, va("print \"Can't find client ID for %s\n\"", arg1 ) ); 
            return; 
         } 
         if (client_id == -2) 
         { 
            trap_SendServerCommand( ent-g_entities, va("print \"Ambiguous client ID for %s\n\"", arg1 ) ); 
            return; 
         } 
         if (!g_entities[client_id].inuse) 
         { // check to make sure client slot is in use 
            trap_SendServerCommand( ent-g_entities, va("print \"Client %s is not active\n\"", arg1 ) ); 
            return; 
         }

		 if (g_entities[client_id].client->pers.ampunish){
			trap_SendServerCommand( ent-g_entities, va("print \"Client is currently being punished. Please unpunish them before freezing them.\nTo unpunish them, type in /punish again on the client.\n\"" ) );
			 return;
		 }
		 if (g_entities[client_id].client->pers.amsleep){
			trap_SendServerCommand( ent-g_entities, va("print \"Client is currently sleeping. Please wake them before freezing them.\nTo wake them, type in /sleep again on the client.\n\"" ) );
			 return;
		 }
		 if (g_entities[client_id].client->pers.amdemigod == 1){
			trap_SendServerCommand( ent-g_entities, va("print \"Client is currently a demigod. Please undemigod them before freezing them.\nTo undemigod them, type in /demigod again on the client.\n\"" ) );
			 return;
		 }

		 if (g_entities[client_id].client->pers.amfreeze)
		 {
		 g_entities[client_id].client->pers.amfreeze = 0; 
         g_entities[client_id].client->ps.saberCanThrow = qtrue;
		 g_entities[client_id].client->ps.forceRestricted = qfalse;
		 g_entities[client_id].takedamage = qtrue;
         trap_SendServerCommand( -1, va("cp \"%s^7\n%s\n\"", g_entities[client_id].client->pers.netname, roar_freeze_off_saying.string ) ); 
		 }
		 else{
		 g_entities[client_id].client->pers.amfreeze = 1;
         trap_SendServerCommand( -1, va("cp \"%s^7\n%s\n\"", g_entities[client_id].client->pers.netname, roar_freeze_on_saying.string ) );
		 G_ScreenShake(g_entities[client_id].client->ps.origin, &g_entities[client_id],  3.0f, 2000, qtrue); 
         G_Sound(&g_entities[client_id], CHAN_AUTO, G_SoundIndex("sound/ambience/thunder_close1"));
		 }
	  }
	  else if ((Q_stricmp(cmd, "lockname") == 0) || (Q_stricmp(cmd, "amlockname") == 0))
{
   int client_id = -1;
	char   arg1[MAX_STRING_CHARS];
	if (ent->r.svFlags & SVF_ADMIN1)
			{
				if (!(roar_adminControl1.integer & (1 << A_LOCKNAME)))
		{
			trap_SendServerCommand( ent-g_entities, va("print \"LockName is not allowed at this administration rank.\n\"") );
			return;
		}
			}
			else if (ent->r.svFlags & SVF_ADMIN2)
			{
				if (!(roar_adminControl2.integer & (1 << A_LOCKNAME)))
		{
			trap_SendServerCommand( ent-g_entities, va("print \"LockName is not allowed at this administration rank.\n\"") );
			return;
		}
			}
			if (ent->r.svFlags & SVF_ADMIN3)
			{
				if (!(roar_adminControl3.integer & (1 << A_LOCKNAME)))
		{
			trap_SendServerCommand( ent-g_entities, va("print \"LockName is not allowed at this administration rank.\n\"") );
			return;
		}
			}
			if (ent->r.svFlags & SVF_ADMIN4)
			{
				if (!(roar_adminControl4.integer & (1 << A_LOCKNAME)))
		{
			trap_SendServerCommand( ent-g_entities, va("print \"LockName is not allowed at this administration rank.\n\"") );
			return;
		}
			}
			if (ent->r.svFlags & SVF_ADMIN5)
			{
				if (!(roar_adminControl5.integer & (1 << A_LOCKNAME)))
		{
			trap_SendServerCommand( ent-g_entities, va("print \"LockName is not allowed at this administration rank.\n\"") );
			return;
		}
			}
			if (!(ent->r.svFlags & SVF_ADMIN1) && !(ent->r.svFlags & SVF_ADMIN2) && !(ent->r.svFlags & SVF_ADMIN3) && !(ent->r.svFlags & SVF_ADMIN4) && !(ent->r.svFlags & SVF_ADMIN5)){
				trap_SendServerCommand( ent-g_entities, "print \"Type in /help lockname if you need help with this command.\n\"" );
				return;
			}
   if ( trap_Argc() != 2)
   {
      trap_SendServerCommand( ent-g_entities, "print \"Usage: ^3/lockname (client)\n\"" ); 
      return; 
   }
   trap_Argv( 1,  arg1, sizeof(  arg1 ) );
   client_id = G_ClientNumberFromArg(  arg1 );
   if (client_id == -1) 
         { 
            trap_SendServerCommand( ent-g_entities, va("print \"Can't find client ID for %s\n\"", arg1 ) ); 
            return; 
         } 
         if (client_id == -2) 
         { 
            trap_SendServerCommand( ent-g_entities, va("print \"Ambiguous client ID for %s\n\"", arg1 ) ); 
            return; 
         }// either we have the client id or the string did not match 
         if (!g_entities[client_id].inuse) 
         {// check to make sure client slot is in use 
            trap_SendServerCommand( ent-g_entities, va("print \"Client %s is not active\n\"", arg1 ) ); 
            return; 
         }
   if (g_entities[client_id].client->pers.amlockname == 1){
	   trap_SendServerCommand( -1, va("cp \"%s\nmay change his name & clan now!\n\"", g_entities[client_id].client->pers.netname ) );
	   trap_SendServerCommand( client_id, va("print \"Your name & clan has been unlocked by an admin! You may change it!\n\"" ) ); 
   uwRename(&g_entities[client_id], g_entities[client_id].client->pers.netname);
   uw3Rename(&g_entities[client_id], g_entities[client_id].client->pers.clanname);
   g_entities[client_id].client->pers.amlockname = 0;
   }
   else {
	    g_entities[client_id].client->pers.amlockname = 1; 
 trap_SendServerCommand( -1, va("cp \"%s\nis not allowed to change his name!\n\"", g_entities[client_id].client->pers.netname ) ); 
 trap_SendServerCommand( client_id, va("print \"Your name & clan has been locked by an admin! You may not change it!\n\"" ) ); 
    uw2Rename(&g_entities[client_id], g_entities[client_id].client->pers.netname); 
    uw4Rename(&g_entities[client_id], g_entities[client_id].client->pers.clanname); 
   }
}
	  else if ((Q_stricmp(cmd, "rename") == 0) || (Q_stricmp(cmd, "amrename") == 0)) 
{ 
   int client_id = -1; 
   char   arg1[MAX_STRING_CHARS];
   if (ent->r.svFlags & SVF_ADMIN1)
			{
				if (!(roar_adminControl1.integer & (1 << A_RENAME)))
		{
			trap_SendServerCommand( ent-g_entities, va("print \"Rename is not allowed at this administration rank.\n\"") );
			return;
		}
			}
			else if (ent->r.svFlags & SVF_ADMIN2)
			{
				if (!(roar_adminControl2.integer & (1 << A_RENAME)))
		{
			trap_SendServerCommand( ent-g_entities, va("print \"Rename is not allowed at this administration rank.\n\"") );
			return;
		}
			}
			if (ent->r.svFlags & SVF_ADMIN3)
			{
				if (!(roar_adminControl3.integer & (1 << A_RENAME)))
		{
			trap_SendServerCommand( ent-g_entities, va("print \"Rename is not allowed at this administration rank.\n\"") );
			return;
		}
			}
			if (ent->r.svFlags & SVF_ADMIN4)
			{
				if (!(roar_adminControl4.integer & (1 << A_RENAME)))
		{
			trap_SendServerCommand( ent-g_entities, va("print \"Rename is not allowed at this administration rank.\n\"") );
			return;
		}
			}
			if (ent->r.svFlags & SVF_ADMIN5)
			{
				if (!(roar_adminControl5.integer & (1 << A_RENAME)))
		{
			trap_SendServerCommand( ent-g_entities, va("print \"Rename is not allowed at this administration rank.\n\"") );
			return;
		}
			}
			if (!(ent->r.svFlags & SVF_ADMIN1) && !(ent->r.svFlags & SVF_ADMIN2) && !(ent->r.svFlags & SVF_ADMIN3) && !(ent->r.svFlags & SVF_ADMIN4) && !(ent->r.svFlags & SVF_ADMIN5)){
				trap_SendServerCommand( ent-g_entities, "print \"Must login with /adminlogin (password)\n\"" );
				return;
			}
   if ( trap_Argc() != 3) 
   { 
      trap_SendServerCommand( ent-g_entities, "print \"Type in /help rename if you need help with this command.\n\"" ); 
      return;
   } 
   trap_Argv( 1, arg1, sizeof( arg1 ) ); 
   client_id = G_ClientNumberFromArg( arg1 ); 
   trap_Argv( 2,  arg1, sizeof(  arg1 ) ); 
   uwRename(&g_entities[client_id], arg1); 
}
else if ((Q_stricmp(cmd, "renameClan") == 0) || (Q_stricmp(cmd, "amrenameClan") == 0))
{ 
   int client_id = -1; 
   char   arg1[MAX_STRING_CHARS];
   if (ent->r.svFlags & SVF_ADMIN1)
			{
				if (!(roar_adminControl1.integer & (1 << A_RENAMECLAN)))
		{
			trap_SendServerCommand( ent-g_entities, va("print \"RenameClan is not allowed at this administration rank.\n\"") );
			return;
		}
			}
			else if (ent->r.svFlags & SVF_ADMIN2)
			{
				if (!(roar_adminControl2.integer & (1 << A_RENAMECLAN)))
		{
			trap_SendServerCommand( ent-g_entities, va("print \"RenameClan is not allowed at this administration rank.\n\"") );
			return;
		}
			}
			if (ent->r.svFlags & SVF_ADMIN3)
			{
				if (!(roar_adminControl3.integer & (1 << A_RENAMECLAN)))
		{
			trap_SendServerCommand( ent-g_entities, va("print \"RenameClan is not allowed at this administration rank.\n\"") );
			return;
		}
			}
			if (ent->r.svFlags & SVF_ADMIN4)
			{
				if (!(roar_adminControl4.integer & (1 << A_RENAMECLAN)))
		{
			trap_SendServerCommand( ent-g_entities, va("print \"RenameClan is not allowed at this administration rank.\n\"") );
			return;
		}
			}
			if (ent->r.svFlags & SVF_ADMIN5)
			{
				if (!(roar_adminControl5.integer & (1 << A_RENAMECLAN)))
		{
			trap_SendServerCommand( ent-g_entities, va("print \"RenameClan is not allowed at this administration rank.\n\"") );
			return;
		}
			}
			if (!(ent->r.svFlags & SVF_ADMIN1) && !(ent->r.svFlags & SVF_ADMIN2) && !(ent->r.svFlags & SVF_ADMIN3) && !(ent->r.svFlags & SVF_ADMIN4) && !(ent->r.svFlags & SVF_ADMIN5)){
				trap_SendServerCommand( ent-g_entities, "print \"Must login with /adminlogin (password)\n\"" );
				return;
			}
   if ( trap_Argc() != 3) 
   { 
      trap_SendServerCommand( ent-g_entities, "print \"Usage: ^3/renameClan (client) (new name)\n\"" ); 
      return; 
   }
   trap_Argv( 1, arg1, sizeof( arg1 ) ); 
   client_id = G_ClientNumberFromArg( arg1 ); 
   trap_Argv( 2,  arg1, sizeof(  arg1 ) ); 
   uw3Rename(&g_entities[client_id], arg1); 
    trap_SendServerCommand( -1, va("print \"%s^7%s\n\"", g_entities[client_id].client->pers.netname, roar_renameclan_saying.string ) );
}
	  	    else if ((Q_stricmp(cmd, "adminkick") == 0) || (Q_stricmp(cmd, "amadminkick") == 0) || (Q_stricmp(cmd, "amkick") == 0))
      { // kick a player 
         int client_id = -1; 
         char   arg1[MAX_STRING_CHARS]; 
		 if (ent->r.svFlags & SVF_ADMIN1)
			{
				if (!(roar_adminControl1.integer & (1 << A_ADMINKICK)))
		{
			trap_SendServerCommand( ent-g_entities, va("print \"AdminKick is not allowed at this administration rank.\n\"") );
			return;
		}
			}
			else if (ent->r.svFlags & SVF_ADMIN2)
			{
				if (!(roar_adminControl2.integer & (1 << A_ADMINKICK)))
		{
			trap_SendServerCommand( ent-g_entities, va("print \"AdminKick is not allowed at this administration rank.\n\"") );
			return;
		}
			}
			if (ent->r.svFlags & SVF_ADMIN3)
			{
				if (!(roar_adminControl3.integer & (1 << A_ADMINKICK)))
		{
			trap_SendServerCommand( ent-g_entities, va("print \"AdminKick is not allowed at this administration rank.\n\"") );
			return;
		}
			}
			if (ent->r.svFlags & SVF_ADMIN4)
			{
				if (!(roar_adminControl4.integer & (1 << A_ADMINKICK)))
		{
			trap_SendServerCommand( ent-g_entities, va("print \"AdminKick is not allowed at this administration rank.\n\"") );
			return;
		}
			}
			if (ent->r.svFlags & SVF_ADMIN5)
			{
				if (!(roar_adminControl5.integer & (1 << A_ADMINKICK)))
		{
			trap_SendServerCommand( ent-g_entities, va("print \"AdminKick is not allowed at this administration rank.\n\"") );
			return;
		}
			}
			if (!(ent->r.svFlags & SVF_ADMIN1) && !(ent->r.svFlags & SVF_ADMIN2) && !(ent->r.svFlags & SVF_ADMIN3) && !(ent->r.svFlags & SVF_ADMIN4) && !(ent->r.svFlags & SVF_ADMIN5)){
				trap_SendServerCommand( ent-g_entities, "print \"Must login with /adminlogin (password)\n\"" );
				return;
			}
         if ( trap_Argc() < 2 ) 
         { 
            trap_SendServerCommand( ent-g_entities, "print \"Type in /help adminkick if you need help with this command.\n\"" ); 
            return; 
         }
         trap_Argv( 1,  arg1, sizeof( arg1 ) ); 
         client_id = G_ClientNumberFromArg( arg1 ); 
         if (client_id == -1) 
         { 
            trap_SendServerCommand( ent-g_entities, va("print \"Can't find client ID for %s\n\"", arg1 ) ); 
            return; 
         } 
         if (client_id == -2) 
         { 
            trap_SendServerCommand( ent-g_entities, va("print \"Ambiguous client ID for %s\n\"", arg1 ) ); 
            return; 
         } 
         // either we have the client id or the string did not match 
         if (!g_entities[client_id].inuse) 
         { // check to make sure client slot is in use 
            trap_SendServerCommand( ent-g_entities, va("print \"Client %s is not active\n\"", arg1 ) ); 
            return; 
         }
		 //trap_Argv( 2,  arg1, sizeof( arg1 ) );
		 trap_SendConsoleCommand( EXEC_APPEND, va("clientkick %d", client_id) );
		 //trap_SendServerCommand( -1, va("print \"^3REASON:^2%s\n\"", arg1) );
	  }
	  	 else if ((Q_stricmp(cmd, "adminban") == 0) || (Q_stricmp(cmd, "amadminban") == 0) || (Q_stricmp(cmd, "amban") == 0)) 
      { // kick n ban 
		 int client_id = -1; 
         char   arg1[MAX_STRING_CHARS];
		 if (ent->r.svFlags & SVF_ADMIN1)
			{
				if (!(roar_adminControl1.integer & (1 << A_ADMINBAN)))
		{
			trap_SendServerCommand( ent-g_entities, va("print \"AdminBan is not allowed at this administration rank.\n\"") );
			return;
		}
			}
			else if (ent->r.svFlags & SVF_ADMIN2)
			{
				if (!(roar_adminControl2.integer & (1 << A_ADMINBAN)))
		{
			trap_SendServerCommand( ent-g_entities, va("print \"AdminBan is not allowed at this administration rank.\n\"") );
			return;
		}
			}
			if (ent->r.svFlags & SVF_ADMIN3)
			{
				if (!(roar_adminControl3.integer & (1 << A_ADMINBAN)))
		{
			trap_SendServerCommand( ent-g_entities, va("print \"AdminBan is not allowed at this administration rank.\n\"") );
			return;
		}
			}
			if (ent->r.svFlags & SVF_ADMIN4)
			{
				if (!(roar_adminControl4.integer & (1 << A_ADMINBAN)))
		{
			trap_SendServerCommand( ent-g_entities, va("print \"AdminBan is not allowed at this administration rank.\n\"") );
			return;
		}
			}
			if (ent->r.svFlags & SVF_ADMIN5)
			{
				if (!(roar_adminControl5.integer & (1 << A_ADMINBAN)))
		{
			trap_SendServerCommand( ent-g_entities, va("print \"AdminBan is not allowed at this administration rank.\n\"") );
			return;
		}
			}
			if (!(ent->r.svFlags & SVF_ADMIN1) && !(ent->r.svFlags & SVF_ADMIN2) && !(ent->r.svFlags & SVF_ADMIN3) && !(ent->r.svFlags & SVF_ADMIN4) && !(ent->r.svFlags & SVF_ADMIN5)){
				trap_SendServerCommand( ent-g_entities, "print \"Must login with /adminlogin (password)\n\"" );
				return;
			}
         if ( trap_Argc() < 2 ) 
         { 
            trap_SendServerCommand( ent-g_entities, "print \"Type in /help adminban if you need help with this command.\n\"" ); 
            return; 
         } 
         trap_Argv( 1,  arg1, sizeof(  arg1 ) );
         client_id = G_ClientNumberFromArg(  arg1 ); 
         if (client_id == -1) 
         { 
            trap_SendServerCommand( ent-g_entities, va("print \"Can't find client ID for %s\n\"", arg1 ) ); 
            return; 
         } 
         if (client_id == -2) 
         { 
            trap_SendServerCommand( ent-g_entities, va("print \"Ambiguous client ID for %s\n\"", arg1 ) ); 
            return; 
         } 
         // either we have the client id or the string did not match 
         if (!g_entities[client_id].inuse) 
         { // check to make sure client slot is in use 
            trap_SendServerCommand( ent-g_entities, va("print \"Client %s is not active\n\"", arg1 ) ); 
            return; 
         }
		 //if( g_entities[client_id].client->sess.ip[0] == 0 ){
		//	G_Printf( "Couldn't retrieve IP Address for player %s\n", g_entities[client_id].client->pers.netname );
			//return;
		//}
		//G_LogPrintf("BANNED IP: %i.%i.%i.%i for user %s.\n", g_entities[client_id].client->sess.ip[0], g_entities[client_id].client->sess.ip[1], 
		//g_entities[client_id].client->sess.ip[2], g_entities[client_id].client->sess.ip[3], g_entities[client_id].client->pers.netname);
		trap_SendConsoleCommand( EXEC_NOW, va( "clientkick %d", client_id ) );
		//trap_SendConsoleCommand( EXEC_APPEND, va( "AddIP %i.%i.%i.%i" ) );
		AddIP( g_entities[client_id].client->sess.IPstring ); //RoAR mod NOTE: Ensiform
	  }

	else if ((Q_stricmp(cmd, "scale") == 0) || (Q_stricmp(cmd, "amscale") == 0)) {
	  int TargetNum = -1;
      int TheScale = 0;
	  int i=0;
      char arg1[MAX_STRING_CHARS]; 
	  if (ent->r.svFlags & SVF_ADMIN1)
		{
			if (!(roar_adminControl1.integer & (1 << A_SCALE)))
		{
			trap_SendServerCommand( ent-g_entities, va("print \"Scale is not allowed at this administration rank.\n\"") );
			return;
		}
			}
		if (ent->r.svFlags & SVF_ADMIN2)
			{
				if (!(roar_adminControl2.integer & (1 << A_SCALE)))
		{
			trap_SendServerCommand( ent-g_entities, va("print \"Scale is not allowed at this administration rank.\n\"") );
			return;
		}
			}
			if (ent->r.svFlags & SVF_ADMIN3)
			{
				if (!(roar_adminControl3.integer & (1 << A_SCALE)))
		{
			trap_SendServerCommand( ent-g_entities, va("print \"Scale is not allowed at this administration rank.\n\"") );
			return;
		}
			}
			if (ent->r.svFlags & SVF_ADMIN4)
			{
				if (!(roar_adminControl4.integer & (1 << A_SCALE)))
		{
			trap_SendServerCommand( ent-g_entities, va("print \"Scale is not allowed at this administration rank.\n\"") );
			return;
		}
			}
			if (ent->r.svFlags & SVF_ADMIN5)
			{
				if (!(roar_adminControl5.integer & (1 << A_SCALE)))
		{
			trap_SendServerCommand( ent-g_entities, va("print \"Scale is not allowed at this administration rank.\n\"") );
			return;
		}
			}
			if (!(ent->r.svFlags & SVF_ADMIN1) && !(ent->r.svFlags & SVF_ADMIN2) && !(ent->r.svFlags & SVF_ADMIN3) && !(ent->r.svFlags & SVF_ADMIN4) && !(ent->r.svFlags & SVF_ADMIN5)){
				trap_SendServerCommand( ent-g_entities, "print \"Must login with /adminlogin (password)\n\"" );
				return;
			}
	  trap_Argv(1, arg1, sizeof(arg1)); 
      if(trap_Argc() == 2){
		TargetNum = ent->client->ps.clientNum;
		TheScale = atoi(arg1);
      }
	  else {
		  TargetNum = G_ClientNumberFromArg( arg1 );
		  trap_Argv(2, arg1, sizeof(arg1));
		TheScale = atoi(arg1);
	  }
	  if (trap_Argc() > 3 || trap_Argc() == 1){
		  trap_SendServerCommand( clientNum, "print \"^3Type in ^5/help scale ^3if you need help with this command.\n\""); 
         return;
	  }
		 if (TargetNum == -1) 
         { 
            trap_SendServerCommand( ent-g_entities, va("print \"Can't find client ID for %s\n\"", arg1 ) ); 
            return;
         }
         if (TargetNum == -2) 
         { 
            trap_SendServerCommand( ent-g_entities, va("print \"Ambiguous client ID for %s\n\"", arg1 ) ); 
            return;
         } 
         // either we have the client id or the string did not match 
         if (!g_entities[TargetNum].inuse) 
         { // check to make sure client slot is in use 
            trap_SendServerCommand( ent-g_entities, va("print \"Client %s is not active\n\"", arg1 ) ); 
            return; 
         }
      if(TheScale >= 201 || TheScale <= 49){
         trap_SendServerCommand( clientNum, "print \"Can't scale a player beyond 50 - 200!\n\""); 
         return;
      }
	  trap_SendServerCommand( -1, va("cp \"%s^7\n%s\n\"", g_entities[TargetNum].client->pers.netname, roar_scale_saying.string ) );
	  g_entities[TargetNum].client->pers.ammodelchanged3 = 1;
	  g_entities[TargetNum].client->ps.iModelScale = TheScale;
	}

	else if ((Q_stricmp(cmd, "empower") == 0) || (Q_stricmp(cmd, "amempower") == 0))
   {
	   gentity_t * targetplayer;
		int i;
	   int client_id = -1; 
       char   arg1[MAX_STRING_CHARS]; 
		if (ent->r.svFlags & SVF_ADMIN1)
			{
				if (!(roar_adminControl1.integer & (1 << A_EMPOWER)))
		{
			trap_SendServerCommand( ent-g_entities, va("print \"Empower is not allowed at this administration rank.\n\"") );
			return;
		}
			}
			else if (ent->r.svFlags & SVF_ADMIN2)
			{
				if (!(roar_adminControl2.integer & (1 << A_EMPOWER)))
		{
			trap_SendServerCommand( ent-g_entities, va("print \"Empower is not allowed at this administration rank.\n\"") );
			return;
		}
			}
			if (ent->r.svFlags & SVF_ADMIN3)
			{
				if (!(roar_adminControl3.integer & (1 << A_EMPOWER)))
		{
			trap_SendServerCommand( ent-g_entities, va("print \"Empower is not allowed at this administration rank.\n\"") );
			return;
		}
			}
			if (ent->r.svFlags & SVF_ADMIN4)
			{
				if (!(roar_adminControl4.integer & (1 << A_EMPOWER)))
		{
			trap_SendServerCommand( ent-g_entities, va("print \"Empower is not allowed at this administration rank.\n\"") );
			return;
		}
			}
			if (ent->r.svFlags & SVF_ADMIN5)
			{
				if (!(roar_adminControl5.integer & (1 << A_EMPOWER)))
		{
			trap_SendServerCommand( ent-g_entities, va("print \"Empower is not allowed at this administration rank.\n\"") );
			return;
		}
			}
			if (!(ent->r.svFlags & SVF_ADMIN1) && !(ent->r.svFlags & SVF_ADMIN2) && !(ent->r.svFlags & SVF_ADMIN3) && !(ent->r.svFlags & SVF_ADMIN4) && !(ent->r.svFlags & SVF_ADMIN5)){
				trap_SendServerCommand( ent-g_entities, "print \"Must login with /adminlogin (password)\n\"" );
				return;
			}
         if ( trap_Argc() > 2 ) 
         { 
            trap_SendServerCommand( ent-g_entities, "print \"Usage: ^3/empower (client)\n\"" ); 
            return; 
         }
		 trap_Argv( 1,  arg1, sizeof( arg1 ) );
		 //NOTE: EVERYONE HAS BECOME EMPOWERED!
		 if( Q_stricmp( arg1, "+all" ) == 0 ){
			for( i = 0; i < level.maxclients; i++ )
		{
			targetplayer = &g_entities[i];

			if( targetplayer->client && targetplayer->client->pers.connected ){
				if (!targetplayer->client->pers.amempower){
				targetplayer->client->ps.eFlags &= ~EF_SEEKERDRONE;
				targetplayer->client->pers.amempower = 1;
				targetplayer->client->ps.stats[STAT_HOLDABLE_ITEMS] &= ~(1 << HI_SEEKER) & ~(1 << HI_BINOCULARS) & ~(1 << HI_SENTRY_GUN) & ~(1 << HI_EWEB) & ~(1 << HI_CLOAK);
		targetplayer->client->ps.stats[STAT_WEAPONS] &= ~(1 << WP_STUN_BATON) & ~(1 << WP_BLASTER) & ~(1 << WP_DISRUPTOR) & ~(1 << WP_BOWCASTER)
			& ~(1 << WP_REPEATER) & ~(1 << WP_DEMP2) & ~(1 << WP_FLECHETTE) & ~(1 << WP_ROCKET_LAUNCHER) & ~(1 << WP_THERMAL) & ~(1 << WP_DET_PACK)
			& ~(1 << WP_BRYAR_OLD) & ~(1 << WP_CONCUSSION) & ~(1 << WP_TRIP_MINE) & ~(1 << WP_BRYAR_PISTOL);
		targetplayer->client->pers.amterminator = 0;
		targetplayer->client->pers.amhuman = 0;
		targetplayer->client->ps.stats[STAT_WEAPONS] |= (1 << WP_MELEE) | (1 << WP_SABER);
		targetplayer->client->ps.weapon = WP_SABER;
		//MJN's code here
		for( i = 0; i < NUM_FORCE_POWERS; i ++ ){
					// Save
					targetplayer->client->pers.forcePowerLevelSaved[i] = targetplayer->client->ps.fd.forcePowerLevel[i];

					// Set new:
					targetplayer->client->ps.fd.forcePowerLevel[i] = FORCE_LEVEL_3;
				}
				// Save and set known powers:
				targetplayer->client->pers.forcePowersKnownSaved = targetplayer->client->ps.fd.forcePowersKnown;
				if ( g_gametype.integer >= GT_TEAM) {
					targetplayer->client->ps.fd.forcePowersKnown = ( 1 << FP_HEAL | 1 << FP_SPEED | 1 << FP_PUSH | 1 << FP_PULL | 
																	 1 << FP_TELEPATHY | 1 << FP_GRIP | 1 << FP_LIGHTNING | 1 << FP_RAGE | 
																	 1 << FP_PROTECT | 1 << FP_ABSORB | 1 << FP_TEAM_HEAL | 1 << FP_TEAM_FORCE | 
																	 1 << FP_DRAIN | 1 << FP_SEE);
				}
				else{
					targetplayer->client->ps.fd.forcePowersKnown = ( 1 << FP_HEAL | 1 << FP_SPEED | 1 << FP_PUSH | 1 << FP_PULL | 
																 1 << FP_TELEPATHY | 1 << FP_GRIP | 1 << FP_LIGHTNING | 1 << FP_RAGE | 
																 1 << FP_PROTECT | 1 << FP_ABSORB | 1 << FP_DRAIN | 1 << FP_SEE);
				}
				}
			}
		}
		Tforce[targetplayer->client->ps.clientNum] = 0;
		trap_SendServerCommand( -1, va("cp \"^7%s\n\"", roar_empower_on_ALL_saying.string ) );
		trap_SendServerCommand( -1, va("print \"^7%s\n\"", roar_empower_on_ALL_saying.string ) );
		return;
		 }
		 //NOTE: EVERYONE IS NOW UNEMPOWERED!
		else if( Q_stricmp( arg1, "-all" ) == 0 ){
			for( i = 0; i < level.maxclients; i++ )
		{
			targetplayer = &g_entities[i];

			if( targetplayer->client && targetplayer->client->pers.connected ){
				if (targetplayer->client->pers.amempower){
				targetplayer->client->pers.amempower = 0;
				targetplayer->client->ps.eFlags &= ~EF_BODYPUSH;
				//targetplayer->client->ps.powerups[PW_FORCE_ENLIGHTENED_LIGHT] = 0;
				//targetplayer->client->ps.powerups[PW_FORCE_ENLIGHTENED_DARK] = 0;
				//MJN's code here
				// Restore forcepowers:
		for( i = 0; i < NUM_FORCE_POWERS; i ++ ){
			// Save
			targetplayer->client->ps.fd.forcePowerLevel[i] = targetplayer->client->pers.forcePowerLevelSaved[i];
		}

		// Save and set known powers:
		targetplayer->client->ps.fd.forcePowersKnown = targetplayer->client->pers.forcePowersKnownSaved;
				if (!(g_weaponDisable.integer & (1 << WP_BRYAR_PISTOL)))
			{
				targetplayer->client->ps.stats[STAT_WEAPONS] |= ( 1 << WP_BRYAR_PISTOL );
			}
			if (roar_starting_weapons.integer == 0)
				{
				targetplayer->client->ps.stats[STAT_WEAPONS] |= ( 1 << WP_SABER );
				targetplayer->client->ps.weapon = WP_SABER;
				}
				else if (roar_starting_weapons.integer == 1)
				{
				targetplayer->client->ps.stats[STAT_WEAPONS] |= (1 << WP_MELEE);
				targetplayer->client->ps.weapon = WP_MELEE;
				}
				else if (roar_starting_weapons.integer == 2)
				{
				targetplayer->client->ps.stats[STAT_WEAPONS] |= ( 1 << WP_SABER ) | (1 << WP_MELEE);
				targetplayer->client->ps.weapon = WP_SABER;
				}
				}
			}
		}
		trap_SendServerCommand( -1, va("cp \"^7%s\n\"", roar_empower_off_ALL_saying.string ) );
		trap_SendServerCommand( -1, va("print \"^7%s\n\"", roar_empower_off_ALL_saying.string ) );
		return;
		 }
		//
		 else{
         client_id = G_ClientNumberFromArg( arg1 );
		 }
		 if ( trap_Argc() < 2 ) 
         {
			client_id = ent->client->ps.clientNum;
		}
         if (client_id == -1) 
         { 
            trap_SendServerCommand( ent-g_entities, va("print \"Can't find client ID for %s\n\"", arg1 ) ); 
            return;
         }
         if (client_id == -2) 
         { 
            trap_SendServerCommand( ent-g_entities, va("print \"Ambiguous client ID for %s\n\"", arg1 ) ); 
            return;
         }
         if (!g_entities[client_id].inuse) 
         {
            trap_SendServerCommand( ent-g_entities, va("print \"Client %s is not active\n\"", arg1 ) ); 
            return; 
         }
		 if (g_entities[client_id].client->pers.amempower)
		 {
			 //MJN's code here
				// Restore forcepowers:
		for( i = 0; i < NUM_FORCE_POWERS; i ++ ){
			// Save
			g_entities[client_id].client->ps.fd.forcePowerLevel[i] = g_entities[client_id].client->pers.forcePowerLevelSaved[i];
		}

		// Save and set known powers:
		g_entities[client_id].client->ps.fd.forcePowersKnown = g_entities[client_id].client->pers.forcePowersKnownSaved;
		//
         trap_SendServerCommand( -1, va("cp \"%s^7\n%s\n\"", g_entities[client_id].client->pers.netname, roar_empower_off_saying.string ) );
		 trap_SendServerCommand( -1, va("print \"%s ^7%s\n\"", g_entities[client_id].client->pers.netname, roar_empower_off_saying.string ) );
		g_entities[client_id].client->pers.amempower = 0;
		g_entities[client_id].client->ps.eFlags &= ~EF_BODYPUSH;
		g_entities[client_id].client->ps.powerups[PW_FORCE_ENLIGHTENED_LIGHT] = 0;
		g_entities[client_id].client->ps.powerups[PW_FORCE_ENLIGHTENED_DARK] = 0;
		if (!(g_weaponDisable.integer & (1 << WP_BRYAR_PISTOL)))
			{
				g_entities[client_id].client->ps.stats[STAT_WEAPONS] |= ( 1 << WP_BRYAR_PISTOL );
			}
			if (roar_starting_weapons.integer == 0)
				{
				g_entities[client_id].client->ps.stats[STAT_WEAPONS] |= ( 1 << WP_SABER );
				g_entities[client_id].client->ps.weapon = WP_SABER;
				}
				else if (roar_starting_weapons.integer == 1)
				{
				g_entities[client_id].client->ps.stats[STAT_WEAPONS] |= (1 << WP_MELEE);
				g_entities[client_id].client->ps.weapon = WP_MELEE;
				}
				else if (roar_starting_weapons.integer == 2)
				{
				g_entities[client_id].client->ps.stats[STAT_WEAPONS] |= ( 1 << WP_SABER ) | (1 << WP_MELEE);
				g_entities[client_id].client->ps.weapon = WP_SABER;
				}
		 }
		 else {
			 //Terminator takeaway
//
		g_entities[client_id].client->ps.eFlags &= ~EF_SEEKERDRONE;
		g_entities[client_id].client->ps.stats[STAT_HOLDABLE_ITEMS] &= ~(1 << HI_SEEKER) & ~(1 << HI_BINOCULARS) & ~(1 << HI_SENTRY_GUN) & ~(1 << HI_EWEB) & ~(1 << HI_CLOAK);
		g_entities[client_id].client->ps.stats[STAT_WEAPONS] &= ~(1 << WP_STUN_BATON) & ~(1 << WP_BLASTER) & ~(1 << WP_DISRUPTOR) & ~(1 << WP_BOWCASTER)
			& ~(1 << WP_REPEATER) & ~(1 << WP_DEMP2) & ~(1 << WP_FLECHETTE) & ~(1 << WP_ROCKET_LAUNCHER) & ~(1 << WP_THERMAL) & ~(1 << WP_DET_PACK)
			& ~(1 << WP_BRYAR_OLD) & ~(1 << WP_CONCUSSION) & ~(1 << WP_TRIP_MINE) & ~(1 << WP_BRYAR_PISTOL);
//
		//MJN's code here
		for( i = 0; i < NUM_FORCE_POWERS; i ++ ){
					// Save
					g_entities[client_id].client->pers.forcePowerLevelSaved[i] = g_entities[client_id].client->ps.fd.forcePowerLevel[i];

					// Set new:
					g_entities[client_id].client->ps.fd.forcePowerLevel[i] = FORCE_LEVEL_3;
				}
				// Save and set known powers:
				g_entities[client_id].client->pers.forcePowersKnownSaved = g_entities[client_id].client->ps.fd.forcePowersKnown;
				if ( g_gametype.integer >= GT_TEAM) {
					g_entities[client_id].client->ps.fd.forcePowersKnown = ( 1 << FP_HEAL | 1 << FP_SPEED | 1 << FP_PUSH | 1 << FP_PULL | 
																	 1 << FP_TELEPATHY | 1 << FP_GRIP | 1 << FP_LIGHTNING | 1 << FP_RAGE | 
																	 1 << FP_PROTECT | 1 << FP_ABSORB | 1 << FP_TEAM_HEAL | 1 << FP_TEAM_FORCE | 
																	 1 << FP_DRAIN | 1 << FP_SEE);
				}
				else{
					g_entities[client_id].client->ps.fd.forcePowersKnown = ( 1 << FP_HEAL | 1 << FP_SPEED | 1 << FP_PUSH | 1 << FP_PULL | 
																 1 << FP_TELEPATHY | 1 << FP_GRIP | 1 << FP_LIGHTNING | 1 << FP_RAGE | 
																 1 << FP_PROTECT | 1 << FP_ABSORB | 1 << FP_DRAIN | 1 << FP_SEE);
				}
		//END MJN's code here.
		Tforce[g_entities[client_id].client->ps.clientNum] = 0;
		g_entities[client_id].client->pers.amterminator = 0;
		g_entities[client_id].client->ps.stats[STAT_WEAPONS] |= ( 1 << WP_SABER ) | ( 1 << WP_MELEE );
		g_entities[client_id].client->ps.weapon = WP_SABER;
         trap_SendServerCommand( -1, va("cp \"%s^7\n%s\n\"", g_entities[client_id].client->pers.netname, roar_empower_on_saying.string ) );
		 trap_SendServerCommand( -1, va("print \"%s ^7%s\n\"", g_entities[client_id].client->pers.netname, roar_empower_on_saying.string ) ); 
		g_entities[client_id].client->pers.amempower = 1;
		g_entities[client_id].client->pers.amhuman = 0;
		G_ScreenShake(g_entities[client_id].client->ps.origin, &g_entities[client_id],  3.0f, 2000, qtrue); 
         G_Sound(&g_entities[client_id], CHAN_AUTO, G_SoundIndex("sound/effects/electric_beam_lp.wav"));
		 }
   }
else if (Q_stricmp(cmd, "Vaporize") == 0){
	Cmd_Kill_f (ent);
	ent->client->ps.eFlags |= EF_DISINTEGRATION;
}
/*
	else if ((Q_stricmp(cmd, "jetpack") == 0) || (Q_stricmp(cmd, "amjetpack") == 0))
   {
	   if (ent->client->pers.amhuman == 1){
		   trap_SendServerCommand( ent-g_entities, va("print \"Monk's can't have jetpacks!\n\"" ) );
		   return;
	   }
	   if (roar_allow_jetpack_command.integer == 1)
	   {
		   if (ent->client->ps.duelInProgress){
			   trap_SendServerCommand( ent-g_entities, va("print \"Jetpack is not allowed in duels!\n\"" ) );
		   }
		   else if (ent->client->ps.stats[STAT_HOLDABLE_ITEMS] & (1 << HI_JETPACK)) 
   { 
      ent->client->ps.stats[STAT_HOLDABLE_ITEMS] &= ~(1 << HI_JETPACK); 
   } 
   else 
   { 
      ent->client->ps.stats[STAT_HOLDABLE_ITEMS] |= (1 << HI_JETPACK); 
   }
   	if (ent->client->jetPackOn)
	{
		Jetpack_Off(ent);
	}
   }
	else {
		trap_SendServerCommand( ent-g_entities, va("print \"Jetpack is disabled on this server!\n\"" ) );
	}
   }*/
		else if ((Q_stricmp(cmd, "DropSaber") == 0) || (Q_stricmp(cmd, "amDropSaber") == 0))
	{
		if (roar_allow_jetpack_command.integer == 0){
			trap_SendServerCommand( ent-g_entities, va("print \"Jetpack is disabled on this server!\n\"" ) );
			return;
		}
		else if (ent->client->ps.weapon == WP_SABER &&
			ent->client->ps.saberEntityNum &&
			!ent->client->ps.saberInFlight)
		{
			saberKnockOutOfHand(&g_entities[ent->client->ps.saberEntityNum], ent, vec3_origin);
		}
	}
			else if (Q_stricmp(cmd, "taunt") == 0 )
		{
			int tauntNum;
			char	arg1[MAX_STRING_CHARS]; // target of command

			trap_Argv( 1, arg1, sizeof( arg1 ) );
			tauntNum = atoi(arg1);
			
			if (ent->client->ps.torsoTimer < 1 && ent->client->ps.forceHandExtend == HANDEXTEND_NONE &&
			ent->client->ps.legsTimer < 1 && ent->client->ps.weaponTime < 1 && 
			ent->client->ps.saberLockTime < ent->client->pers.cmd.serverTime) 
			{

				ent->client->ps.forceHandExtend = HANDEXTEND_TAUNT;

				ent->client->ps.forceDodgeAnim = BOTH_ENGAGETAUNT;

				ent->client->ps.forceHandExtendTime = level.time + 1000;
				
				ent->client->ps.weaponTime = 100;

				if (tauntNum > 11)
				{
					tauntNum = 11;
				}
				G_AddEvent(ent, EV_TAUNT, tauntNum);
			}
		}
	else if ((Q_stricmp(cmd, "slay") == 0) || (Q_stricmp(cmd, "amslay") == 0))
	{
		int client_id = -1;
		char	arg1[MAX_STRING_CHARS];
		trap_Argv( 1, arg1, sizeof( arg1 ) );
		client_id = G_ClientNumberFromArg( arg1 );

		if (ent->r.svFlags & SVF_ADMIN1)
		{
				if (!(roar_adminControl1.integer & (1 << A_SLAY)))
		{
			trap_SendServerCommand( ent-g_entities, va("print \"Slay is not allowed at this administration rank.\n\"") );
			return;
		}
			}
			else if (ent->r.svFlags & SVF_ADMIN2)
			{
				if (!(roar_adminControl2.integer & (1 << A_SLAY)))
		{
			trap_SendServerCommand( ent-g_entities, va("print \"Slay is not allowed at this administration rank.\n\"") );
			return;
		}
			}
			if (ent->r.svFlags & SVF_ADMIN3)
			{
				if (!(roar_adminControl3.integer & (1 << A_SLAY)))
		{
			trap_SendServerCommand( ent-g_entities, va("print \"Slay is not allowed at this administration rank.\n\"") );
			return;
		}
			}
			if (ent->r.svFlags & SVF_ADMIN4)
			{
				if (!(roar_adminControl4.integer & (1 << A_SLAY)))
		{
			trap_SendServerCommand( ent-g_entities, va("print \"Slay is not allowed at this administration rank.\n\"") );
			return;
		}
			}
			if (ent->r.svFlags & SVF_ADMIN5)
			{
				if (!(roar_adminControl5.integer & (1 << A_SLAY)))
		{
			trap_SendServerCommand( ent-g_entities, va("print \"Slay is not allowed at this administration rank.\n\"") );
			return;
		}
			}
			if (!(ent->r.svFlags & SVF_ADMIN1) && !(ent->r.svFlags & SVF_ADMIN2) && !(ent->r.svFlags & SVF_ADMIN3) && !(ent->r.svFlags & SVF_ADMIN4) && !(ent->r.svFlags & SVF_ADMIN5)){
				trap_SendServerCommand( ent-g_entities, "print \"Must login with /adminlogin (password)\n\"" );
				return;
			}
			if ( trap_Argc() != 2 ) 
         { 
            trap_SendServerCommand( ent-g_entities, "print \"Type in /help slay if you need help with this command.\n\"" ); 
            return; 
         }
			if (client_id == -1)
			{
				trap_SendServerCommand( ent-g_entities, va("print \"Can't find client ID for %s\n\"", arg1 ) );
				return;
			}
			if (client_id >= 0 && client_id < MAX_GENTITIES)
			{
				gentity_t *kEnt = &g_entities[client_id];

				if (kEnt->inuse && kEnt->client)
				{
					g_entities[client_id].flags &= ~FL_GODMODE;
					g_entities[client_id].client->ps.stats[STAT_HEALTH] = kEnt->health = -999;
					player_die (kEnt, kEnt, kEnt, 100000, MOD_SUICIDE);
					DismembermentTest(ent);

					trap_SendServerCommand( -1, va("cp \"%s\n%s\n\"", g_entities[client_id].client->pers.netname, roar_slay_saying.string ) );
				}
			}
			}
	else if ((Q_stricmp(cmd, "monk") == 0) || (Q_stricmp(cmd, "ammonk") == 0))
	{
		gentity_t * targetplayer;
		int	i;
		int client_id = -1;
		char	arg1[MAX_STRING_CHARS];
		if (ent->r.svFlags & SVF_ADMIN1)
		{
				if (!(roar_adminControl1.integer & (1 << A_HUMAN)))
		{
			trap_SendServerCommand( ent-g_entities, va("print \"Monk is not allowed at this administration rank.\n\"") );
			return;
		}
			}
			else if (ent->r.svFlags & SVF_ADMIN2)
			{
				if (!(roar_adminControl2.integer & (1 << A_HUMAN)))
		{
			trap_SendServerCommand( ent-g_entities, va("print \"Monk is not allowed at this administration rank.\n\"") );
			return;
		}
			}
			if (ent->r.svFlags & SVF_ADMIN3)
			{
				if (!(roar_adminControl3.integer & (1 << A_HUMAN)))
		{
			trap_SendServerCommand( ent-g_entities, va("print \"Monk is not allowed at this administration rank.\n\"") );
			return;
		}
			}
			if (ent->r.svFlags & SVF_ADMIN4)
			{
				if (!(roar_adminControl4.integer & (1 << A_HUMAN)))
		{
			trap_SendServerCommand( ent-g_entities, va("print \"Monk is not allowed at this administration rank.\n\"") );
			return;
		}
			}
			if (ent->r.svFlags & SVF_ADMIN5)
			{
				if (!(roar_adminControl5.integer & (1 << A_HUMAN)))
		{
			trap_SendServerCommand( ent-g_entities, va("print \"Monk is not allowed at this administration rank.\n\"") );
			return;
		}
			}
			if (!(ent->r.svFlags & SVF_ADMIN1) && !(ent->r.svFlags & SVF_ADMIN2) && !(ent->r.svFlags & SVF_ADMIN3) && !(ent->r.svFlags & SVF_ADMIN4) && !(ent->r.svFlags & SVF_ADMIN5)){
				trap_SendServerCommand( ent-g_entities, "print \"Must login with /adminlogin (password)\n\"" );
				return;
			}
			if ( trap_Argc() > 2 ) 
         { 
            trap_SendServerCommand( ent-g_entities, "print \"Usage: ^3/monk (client)\n\"" ); 
            return; 
         }
		 trap_Argv( 1, arg1, sizeof( arg1 ) );
		 if ( Q_stricmp( arg1, "+all" ) == 0 ){
			for( i = 0; i < level.maxclients; i++ )
		{
			targetplayer = &g_entities[i];

			if( targetplayer->client && targetplayer->client->pers.connected ){
				if (!targetplayer->client->pers.amhuman){
				targetplayer->client->ps.eFlags &= ~EF_SEEKERDRONE;
				targetplayer->client->ps.stats[STAT_HOLDABLE_ITEMS] &= ~(1 << HI_SEEKER) & ~(1 << HI_BINOCULARS) & ~(1 << HI_SENTRY_GUN) & ~(1 << HI_EWEB) & ~(1 << HI_CLOAK);
		targetplayer->client->ps.stats[STAT_WEAPONS] &= ~(1 << WP_STUN_BATON) & ~(1 << WP_BLASTER) & ~(1 << WP_DISRUPTOR) & ~(1 << WP_BOWCASTER)
			& ~(1 << WP_REPEATER) & ~(1 << WP_DEMP2) & ~(1 << WP_FLECHETTE) & ~(1 << WP_ROCKET_LAUNCHER) & ~(1 << WP_THERMAL) & ~(1 << WP_DET_PACK)
			& ~(1 << WP_BRYAR_OLD) & ~(1 << WP_CONCUSSION) & ~(1 << WP_TRIP_MINE);
		targetplayer->client->pers.amterminator = 0;
		targetplayer->client->ps.weapon = WP_MELEE;
		targetplayer->client->pers.amhuman = 1;
		if (!(g_weaponDisable.integer & (1 << WP_BRYAR_PISTOL)))
			{
				targetplayer->client->ps.stats[STAT_WEAPONS] &= ~( 1 << WP_BRYAR_PISTOL );
			}
		targetplayer->client->pers.amempower = 0;
		targetplayer->client->ps.eFlags &= ~EF_BODYPUSH;
		//MJN's code here
				// Restore forcepowers:
		for( i = 0; i < NUM_FORCE_POWERS; i ++ ){
			// Save
			targetplayer->client->ps.fd.forcePowerLevel[i] = targetplayer->client->pers.forcePowerLevelSaved[i];
		}

		// Save and set known powers:
		targetplayer->client->ps.fd.forcePowersKnown = targetplayer->client->pers.forcePowersKnownSaved;
		//
		//targetplayer->client->ps.powerups[PW_FORCE_ENLIGHTENED_LIGHT] = 0;
		//targetplayer->client->ps.powerups[PW_FORCE_ENLIGHTENED_DARK] = 0;
				}
			}
		}
		trap_SendServerCommand( -1, va("print \"^7%s\n\"", roar_monk_on_ALL_saying.string ) );
		trap_SendServerCommand( -1, va("cp \"^7%s\n\"", roar_monk_on_ALL_saying.string ) );
		return;
		 }
		 else if ( Q_stricmp( arg1, "-all" ) == 0 ){
			for ( i = 0; i < level.maxclients; i++ )
		{
			targetplayer = &g_entities[i];

			if( targetplayer->client && targetplayer->client->pers.connected ){
				if (targetplayer->client->pers.amhuman){
				if (roar_starting_weapons.integer == 0)
				{
				targetplayer->client->ps.stats[STAT_WEAPONS] |= ( 1 << WP_SABER);
				targetplayer->client->ps.weapon = WP_SABER;
				}
				else if (roar_starting_weapons.integer == 1)
				{			
				targetplayer->client->ps.stats[STAT_WEAPONS] |= (1 << WP_MELEE);
				targetplayer->client->ps.weapon = WP_MELEE;
				}
				else if (roar_starting_weapons.integer == 2)
				{
				targetplayer->client->ps.weapon = WP_SABER;
				targetplayer->client->ps.stats[STAT_WEAPONS] |= (1 << WP_MELEE) | (1 << WP_SABER);
				}
				targetplayer->client->pers.amhuman = 0;
				if (!(g_weaponDisable.integer & (1 << WP_BRYAR_PISTOL)))
			{
				targetplayer->client->ps.stats[STAT_WEAPONS] |= ( 1 << WP_BRYAR_PISTOL );
			}
				}
			}
		}
		Tforce[targetplayer->client->ps.clientNum] = 0;
		trap_SendServerCommand( -1, va("print \"^7%s\n\"", roar_monk_off_ALL_saying.string ) );
		trap_SendServerCommand( -1, va("cp \"^7%s\n\"", roar_monk_off_ALL_saying.string ) );
		return;
		 }
		 else {
		client_id = G_ClientNumberFromArg( arg1 );
		 }
		 if ( trap_Argc() < 2 ) 
         { 
			client_id = ent->client->ps.clientNum;
		}
			if (client_id == -1)
			{
				trap_SendServerCommand( ent-g_entities, va("print \"Can't find client ID for %s\n\"", arg1 ) );
				return;
			}
if (g_entities[client_id].client->pers.amhuman)
{
			 if (roar_starting_weapons.integer == 0)
				{
				g_entities[client_id].client->ps.stats[STAT_WEAPONS] |= ( 1 << WP_SABER);
				g_entities[client_id].client->ps.weapon = WP_SABER;
				}
				else if (roar_starting_weapons.integer == 1)
				{			
				g_entities[client_id].client->ps.stats[STAT_WEAPONS] |= (1 << WP_MELEE);
				g_entities[client_id].client->ps.weapon = WP_MELEE;
				}
				else if (roar_starting_weapons.integer == 2)
				{
				g_entities[client_id].client->ps.weapon = WP_SABER;
				g_entities[client_id].client->ps.stats[STAT_WEAPONS] |= (1 << WP_MELEE) | (1 << WP_SABER);
				}
				trap_SendServerCommand( -1, va("cp \"%s^7\n%s\n\"", g_entities[client_id].client->pers.netname, roar_human_off_saying.string ) );
				trap_SendServerCommand( -1, va("print \"%s ^7%s\n\"", g_entities[client_id].client->pers.netname, roar_human_off_saying.string ) );
				g_entities[client_id].client->pers.amhuman = 0;
				Tforce[g_entities[client_id].client->ps.clientNum] = 0;
				if (!(g_weaponDisable.integer & (1 << WP_BRYAR_PISTOL)))
			{
				g_entities[client_id].client->ps.stats[STAT_WEAPONS] |= ( 1 << WP_BRYAR_PISTOL );
			}
}
else {
		g_entities[client_id].client->ps.eFlags &= ~EF_SEEKERDRONE;
		g_entities[client_id].client->ps.stats[STAT_HOLDABLE_ITEMS] &= ~(1 << HI_SEEKER) & ~(1 << HI_BINOCULARS) & ~(1 << HI_SENTRY_GUN) & ~(1 << HI_EWEB) & ~(1 << HI_CLOAK);
		g_entities[client_id].client->ps.stats[STAT_WEAPONS] &= ~(1 << WP_STUN_BATON) & ~(1 << WP_BLASTER) & ~(1 << WP_DISRUPTOR) & ~(1 << WP_BOWCASTER)
			& ~(1 << WP_REPEATER) & ~(1 << WP_DEMP2) & ~(1 << WP_FLECHETTE) & ~(1 << WP_ROCKET_LAUNCHER) & ~(1 << WP_THERMAL) & ~(1 << WP_DET_PACK)
			& ~(1 << WP_BRYAR_OLD) & ~(1 << WP_CONCUSSION) & ~(1 << WP_TRIP_MINE);
		g_entities[client_id].client->pers.amterminator = 0;
		g_entities[client_id].client->ps.weapon = WP_MELEE;
		g_entities[client_id].client->pers.amhuman = 1;
		if (!(g_weaponDisable.integer & (1 << WP_BRYAR_PISTOL)))
			{
				g_entities[client_id].client->ps.stats[STAT_WEAPONS] &= ~( 1 << WP_BRYAR_PISTOL );
			}
		g_entities[client_id].client->pers.amempower = 0;
		g_entities[client_id].client->ps.eFlags &= ~EF_BODYPUSH;
		//MJN's code here
				// Restore forcepowers:
		for( i = 0; i < NUM_FORCE_POWERS; i ++ ){
			// Save
			g_entities[client_id].client->ps.fd.forcePowerLevel[i] = g_entities[client_id].client->pers.forcePowerLevelSaved[i];
		}

		// Save and set known powers:
		g_entities[client_id].client->ps.fd.forcePowersKnown = g_entities[client_id].client->pers.forcePowersKnownSaved;
		//
		//g_entities[client_id].client->ps.powerups[PW_FORCE_ENLIGHTENED_LIGHT] = 0;
		//g_entities[client_id].client->ps.powerups[PW_FORCE_ENLIGHTENED_DARK] = 0;
		G_ScreenShake(g_entities[client_id].client->ps.origin, &g_entities[client_id],  3.0f, 2000, qtrue); 
         G_Sound(&g_entities[client_id], CHAN_AUTO, G_SoundIndex("sound/effects/electric_beam_lp.wav")); 
		 trap_SendServerCommand( -1, va("cp \"%s^7\n%s\n\"", g_entities[client_id].client->pers.netname, roar_human_on_saying.string ) );
		 trap_SendServerCommand( -1, va("print \"%s ^7%s\n\"", g_entities[client_id].client->pers.netname, roar_human_on_saying.string ) );
}
				}
	//for convenient powerduel testing in release
	else if (Q_stricmp(cmd, "killother") == 0 && CheatsOk( ent ))
	{
		if (trap_Argc() > 1)
		{
			char sArg[MAX_STRING_CHARS];
			int entNum = 0;

			trap_Argv( 1, sArg, sizeof( sArg ) );

			entNum = G_ClientNumFromNetname(sArg);

			if (entNum >= 0 && entNum < MAX_GENTITIES)
			{
				gentity_t *kEnt = &g_entities[entNum];

				if (kEnt->inuse && kEnt->client)
				{
					kEnt->flags &= ~FL_GODMODE;
					kEnt->client->ps.stats[STAT_HEALTH] = kEnt->health = -999;
					player_die (kEnt, kEnt, kEnt, 100000, MOD_SUICIDE);
				}
			}
		}
	}
#ifdef _DEBUG
	else if (Q_stricmp(cmd, "relax") == 0 && CheatsOk( ent ))
	{
		if (ent->client->ps.eFlags & EF_RAG)
		{
			ent->client->ps.eFlags &= ~EF_RAG;
		}
		else
		{
			ent->client->ps.eFlags |= EF_RAG;
		}
	}
	else if (Q_stricmp(cmd, "holdme") == 0 && CheatsOk( ent ))
	{
		if (trap_Argc() > 1)
		{
			char sArg[MAX_STRING_CHARS];
			int entNum = 0;

			trap_Argv( 1, sArg, sizeof( sArg ) );

			entNum = atoi(sArg);

			if (entNum >= 0 &&
				entNum < MAX_GENTITIES)
			{
				gentity_t *grabber = &g_entities[entNum];

				if (grabber->inuse && grabber->client && grabber->ghoul2)
				{
					if (!grabber->s.number)
					{ //switch cl 0 and entitynum_none, so we can operate on the "if non-0" concept
						ent->client->ps.ragAttach = ENTITYNUM_NONE;
					}
					else
					{
						ent->client->ps.ragAttach = grabber->s.number;
					}
				}
			}
		}
		else
		{
			ent->client->ps.ragAttach = 0;
		}
	}
	else if (Q_stricmp(cmd, "limb_break") == 0 && CheatsOk( ent ))
	{
		if (trap_Argc() > 1)
		{
			char sArg[MAX_STRING_CHARS];
			int breakLimb = 0;

			trap_Argv( 1, sArg, sizeof( sArg ) );
			if (!Q_stricmp(sArg, "right"))
			{
				breakLimb = BROKENLIMB_RARM;
			}
			else if (!Q_stricmp(sArg, "left"))
			{
				breakLimb = BROKENLIMB_LARM;
			}

			G_BreakArm(ent, breakLimb);
		}
	}
	else if (Q_stricmp(cmd, "headexplodey") == 0 && CheatsOk( ent ))
	{
		Cmd_Kill_f (ent);
		if (ent->health < 1)
		{
			DismembermentTest(ent);
		}
	}
	else if (Q_stricmp(cmd, "debugstupidthing") == 0 && CheatsOk( ent ))
	{
		int i = 0;
		gentity_t *blah;
		while (i < MAX_GENTITIES)
		{
			blah = &g_entities[i];
			if (blah->inuse && blah->classname && blah->classname[0] && !Q_stricmp(blah->classname, "NPC_Vehicle"))
			{
				Com_Printf("Found it.\n");
			}
			i++;
		}
	}
	else if (Q_stricmp(cmd, "arbitraryprint") == 0 && CheatsOk( ent ))
	{
		trap_SendServerCommand( -1, va("cp \"Blah blah blah\n\""));
	}
	else if (Q_stricmp(cmd, "handcut") == 0 && CheatsOk( ent ))
	{
		int bCl = 0;
		char sarg[MAX_STRING_CHARS];

		if (trap_Argc() > 1)
		{
			trap_Argv( 1, sarg, sizeof( sarg ) );

			if (sarg[0])
			{
				bCl = atoi(sarg);

				if (bCl >= 0 && bCl < MAX_GENTITIES)
				{
					gentity_t *hEnt = &g_entities[bCl];

					if (hEnt->client)
					{
						if (hEnt->health > 0)
						{
							gGAvoidDismember = 1;
							hEnt->flags &= ~FL_GODMODE;
							hEnt->client->ps.stats[STAT_HEALTH] = hEnt->health = -999;
							player_die (hEnt, hEnt, hEnt, 100000, MOD_SUICIDE);
						}
						gGAvoidDismember = 2;
						G_CheckForDismemberment(hEnt, ent, hEnt->client->ps.origin, 999, hEnt->client->ps.legsAnim, qfalse);
						gGAvoidDismember = 0;
					}
				}
			}
		}
	}
	else if (Q_stricmp(cmd, "loveandpeace") == 0 && CheatsOk( ent ))
	{
		trace_t tr;
		vec3_t fPos;

		AngleVectors(ent->client->ps.viewangles, fPos, 0, 0);

		fPos[0] = ent->client->ps.origin[0] + fPos[0]*40;
		fPos[1] = ent->client->ps.origin[1] + fPos[1]*40;
		fPos[2] = ent->client->ps.origin[2] + fPos[2]*40;

		trap_Trace(&tr, ent->client->ps.origin, 0, 0, fPos, ent->s.number, ent->clipmask);

		if (tr.entityNum < MAX_CLIENTS && tr.entityNum != ent->s.number)
		{
			gentity_t *other = &g_entities[tr.entityNum];

			if (other && other->inuse && other->client)
			{
				vec3_t entDir;
				vec3_t otherDir;
				vec3_t entAngles;
				vec3_t otherAngles;

				if (ent->client->ps.weapon == WP_SABER && !ent->client->ps.saberHolstered)
				{
					Cmd_ToggleSaber_f(ent);
				}

				if (other->client->ps.weapon == WP_SABER && !other->client->ps.saberHolstered)
				{
					Cmd_ToggleSaber_f(other);
				}

				if ((ent->client->ps.weapon != WP_SABER || ent->client->ps.saberHolstered) &&
					(other->client->ps.weapon != WP_SABER || other->client->ps.saberHolstered))
				{
					VectorSubtract( other->client->ps.origin, ent->client->ps.origin, otherDir );
					VectorCopy( ent->client->ps.viewangles, entAngles );
					entAngles[YAW] = vectoyaw( otherDir );
					SetClientViewAngle( ent, entAngles );

					StandardSetBodyAnim(ent, /*BOTH_KISSER1LOOP*/BOTH_STAND1, SETANIM_FLAG_OVERRIDE|SETANIM_FLAG_HOLD|SETANIM_FLAG_HOLDLESS);
					ent->client->ps.saberMove = LS_NONE;
					ent->client->ps.saberBlocked = 0;
					ent->client->ps.saberBlocking = 0;

					VectorSubtract( ent->client->ps.origin, other->client->ps.origin, entDir );
					VectorCopy( other->client->ps.viewangles, otherAngles );
					otherAngles[YAW] = vectoyaw( entDir );
					SetClientViewAngle( other, otherAngles );

					StandardSetBodyAnim(other, /*BOTH_KISSEE1LOOP*/BOTH_STAND1, SETANIM_FLAG_OVERRIDE|SETANIM_FLAG_HOLD|SETANIM_FLAG_HOLDLESS);
					other->client->ps.saberMove = LS_NONE;
					other->client->ps.saberBlocked = 0;
					other->client->ps.saberBlocking = 0;
				}
			}
		}
	}
#endif
	//[MELEE]
	else if (Q_stricmp(cmd, "togglesaber") == 0)
	{
		Cmd_ToggleSaber_f(ent);
	}
	/* racc - This cheat code isn't used anymore.
	else if (Q_stricmp(cmd, "thedestroyer") == 0 && CheatsOk( ent ) && ent && ent->client && ent->client->ps.saberHolstered && ent->client->ps.weapon == WP_SABER)
	{
		Cmd_ToggleSaber_f(ent);

		if (!ent->client->ps.saberHolstered)
		{
		}
	}
	*/
	//begin bot debug cmds
	else if (Q_stricmp(cmd, "debugBMove_Forward") == 0 && CheatsOk(ent))
	{
		int arg = 4000;
		int bCl = 0;
		char sarg[MAX_STRING_CHARS];

		assert(trap_Argc() > 1);
		trap_Argv( 1, sarg, sizeof( sarg ) );

		assert(sarg[0]);
		bCl = atoi(sarg);
		Bot_SetForcedMovement(bCl, arg, -1, -1);
	}
	else if (Q_stricmp(cmd, "debugBMove_Back") == 0 && CheatsOk(ent))
	{
		int arg = -4000;
		int bCl = 0;
		char sarg[MAX_STRING_CHARS];

		assert(trap_Argc() > 1);
		trap_Argv( 1, sarg, sizeof( sarg ) );

		assert(sarg[0]);
		bCl = atoi(sarg);
		Bot_SetForcedMovement(bCl, arg, -1, -1);
	}
	else if (Q_stricmp(cmd, "debugBMove_Right") == 0 && CheatsOk(ent))
	{
		int arg = 4000;
		int bCl = 0;
		char sarg[MAX_STRING_CHARS];

		assert(trap_Argc() > 1);
		trap_Argv( 1, sarg, sizeof( sarg ) );

		assert(sarg[0]);
		bCl = atoi(sarg);
		Bot_SetForcedMovement(bCl, -1, arg, -1);
	}
	else if (Q_stricmp(cmd, "debugBMove_Left") == 0 && CheatsOk(ent))
	{
		int arg = -4000;
		int bCl = 0;
		char sarg[MAX_STRING_CHARS];

		assert(trap_Argc() > 1);
		trap_Argv( 1, sarg, sizeof( sarg ) );

		assert(sarg[0]);
		bCl = atoi(sarg);
		Bot_SetForcedMovement(bCl, -1, arg, -1);
	}
	else if (Q_stricmp(cmd, "debugBMove_Up") == 0 && CheatsOk(ent))
	{
		int arg = 4000;
		int bCl = 0;
		char sarg[MAX_STRING_CHARS];

		assert(trap_Argc() > 1);
		trap_Argv( 1, sarg, sizeof( sarg ) );

		assert(sarg[0]);
		bCl = atoi(sarg);
		Bot_SetForcedMovement(bCl, -1, -1, arg);
	}
	//end bot debug cmds
#ifndef FINAL_BUILD
	else if (Q_stricmp(cmd, "debugSetSaberMove") == 0)
	{
		Cmd_DebugSetSaberMove_f(ent);
	}
	//[SaberSys]
	//This command forces the player into a given saberblocked state which is determined by the inputed numberical value.
	else if (Q_stricmp(cmd, "debugSetSaberBlock") == 0)
	{
		Cmd_DebugSetSaberBlock_f(ent);
	}
	//[/SaberSys]
	else if (Q_stricmp(cmd, "debugSetBodyAnim") == 0)
	{
		Cmd_DebugSetBodyAnim_f(ent, SETANIM_FLAG_OVERRIDE|SETANIM_FLAG_HOLD);
	}
	else if (Q_stricmp(cmd, "debugDismemberment") == 0)
	{
		Cmd_Kill_f (ent);
		if (ent->health < 1)
		{
			char	arg[MAX_STRING_CHARS];
			int		iArg = 0;

			if (trap_Argc() > 1)
			{
				trap_Argv( 1, arg, sizeof( arg ) );

				if (arg[0])
				{
					iArg = atoi(arg);
				}
			}

			DismembermentByNum(ent, iArg);
		}
	}
	else if (Q_stricmp(cmd, "debugDropSaber") == 0)
	{
		if (ent->client->ps.weapon == WP_SABER &&
			ent->client->ps.saberEntityNum &&
			!ent->client->ps.saberInFlight)
		{
			saberKnockOutOfHand(&g_entities[ent->client->ps.saberEntityNum], ent, vec3_origin);
		}
	}
	else if (Q_stricmp(cmd, "debugKnockMeDown") == 0)
	{
		//[KnockdownSys]
		G_Knockdown(ent, NULL, vec3_origin, 300, qtrue);
		/*
		if (BG_KnockDownable(&ent->client->ps))
		{
			ent->client->ps.forceHandExtend = HANDEXTEND_KNOCKDOWN;
			ent->client->ps.forceDodgeAnim = 0;
			if (trap_Argc() > 1)
			{
				ent->client->ps.forceHandExtendTime = level.time + 1100;
				ent->client->ps.quickerGetup = qfalse;
			}
			else
			{
				ent->client->ps.forceHandExtendTime = level.time + 700;
				ent->client->ps.quickerGetup = qtrue;
			}
		}
		*/
		//[/KnockdownSys]
	}
	else if (Q_stricmp(cmd, "debugSaberSwitch") == 0)
	{
		gentity_t *targ = NULL;

		if (trap_Argc() > 1)
		{
			char	arg[MAX_STRING_CHARS];

			trap_Argv( 1, arg, sizeof( arg ) );

			if (arg[0])
			{
				int x = atoi(arg);
				
				if (x >= 0 && x < MAX_CLIENTS)
				{
					targ = &g_entities[x];
				}
			}
		}

		if (targ && targ->inuse && targ->client)
		{
			Cmd_ToggleSaber_f(targ);
		}
	}
	else if (Q_stricmp(cmd, "debugIKGrab") == 0)
	{
		gentity_t *targ = NULL;

		if (trap_Argc() > 1)
		{
			char	arg[MAX_STRING_CHARS];

			trap_Argv( 1, arg, sizeof( arg ) );

			if (arg[0])
			{
				int x = atoi(arg);
				
				if (x >= 0 && x < MAX_CLIENTS)
				{
					targ = &g_entities[x];
				}
			}
		}

		if (targ && targ->inuse && targ->client && ent->s.number != targ->s.number)
		{
			targ->client->ps.heldByClient = ent->s.number+1;
		}
	}
	else if (Q_stricmp(cmd, "debugIKBeGrabbedBy") == 0)
	{
		gentity_t *targ = NULL;

		if (trap_Argc() > 1)
		{
			char	arg[MAX_STRING_CHARS];

			trap_Argv( 1, arg, sizeof( arg ) );

			if (arg[0])
			{
				int x = atoi(arg);
				
				if (x >= 0 && x < MAX_CLIENTS)
				{
					targ = &g_entities[x];
				}
			}
		}

		if (targ && targ->inuse && targ->client && ent->s.number != targ->s.number)
		{
			ent->client->ps.heldByClient = targ->s.number+1;
		}
	}
	else if (Q_stricmp(cmd, "debugIKRelease") == 0)
	{
		gentity_t *targ = NULL;

		if (trap_Argc() > 1)
		{
			char	arg[MAX_STRING_CHARS];

			trap_Argv( 1, arg, sizeof( arg ) );

			if (arg[0])
			{
				int x = atoi(arg);
				
				if (x >= 0 && x < MAX_CLIENTS)
				{
					targ = &g_entities[x];
				}
			}
		}

		if (targ && targ->inuse && targ->client)
		{
			targ->client->ps.heldByClient = 0;
		}
	}
	else if (Q_stricmp(cmd, "debugThrow") == 0)
	{
		trace_t tr;
		vec3_t tTo, fwd;

		if (ent->client->ps.weaponTime > 0 || ent->client->ps.forceHandExtend != HANDEXTEND_NONE ||
			ent->client->ps.groundEntityNum == ENTITYNUM_NONE || ent->health < 1)
		{
			return;
		}

		AngleVectors(ent->client->ps.viewangles, fwd, 0, 0);
		tTo[0] = ent->client->ps.origin[0] + fwd[0]*32;
		tTo[1] = ent->client->ps.origin[1] + fwd[1]*32;
		tTo[2] = ent->client->ps.origin[2] + fwd[2]*32;

		trap_Trace(&tr, ent->client->ps.origin, 0, 0, tTo, ent->s.number, MASK_PLAYERSOLID);

		if (tr.fraction != 1)
		{
			gentity_t *other = &g_entities[tr.entityNum];

			if (other->inuse && other->client && other->client->ps.forceHandExtend == HANDEXTEND_NONE &&
				other->client->ps.groundEntityNum != ENTITYNUM_NONE && other->health > 0 &&
				(int)ent->client->ps.origin[2] == (int)other->client->ps.origin[2])
			{
				float pDif = 40.0f;
				vec3_t entAngles, entDir;
				vec3_t otherAngles, otherDir;
				vec3_t intendedOrigin;
				vec3_t boltOrg, pBoltOrg;
				vec3_t tAngles, vDif;
				vec3_t fwd, right;
				trace_t tr;
				trace_t tr2;

				VectorSubtract( other->client->ps.origin, ent->client->ps.origin, otherDir );
				VectorCopy( ent->client->ps.viewangles, entAngles );
				entAngles[YAW] = vectoyaw( otherDir );
				SetClientViewAngle( ent, entAngles );

				ent->client->ps.forceHandExtend = HANDEXTEND_PRETHROW;
				ent->client->ps.forceHandExtendTime = level.time + 5000;

				ent->client->throwingIndex = other->s.number;
				ent->client->doingThrow = level.time + 5000;
				ent->client->beingThrown = 0;

				VectorSubtract( ent->client->ps.origin, other->client->ps.origin, entDir );
				VectorCopy( other->client->ps.viewangles, otherAngles );
				otherAngles[YAW] = vectoyaw( entDir );
				SetClientViewAngle( other, otherAngles );

				other->client->ps.forceHandExtend = HANDEXTEND_PRETHROWN;
				other->client->ps.forceHandExtendTime = level.time + 5000;

				other->client->throwingIndex = ent->s.number;
				other->client->beingThrown = level.time + 5000;
				other->client->doingThrow = 0;

				//Doing this now at a stage in the throw, isntead of initially.
				//other->client->ps.heldByClient = ent->s.number+1;

				G_EntitySound( other, CHAN_VOICE, G_SoundIndex("*pain100.wav") );
				G_EntitySound( ent, CHAN_VOICE, G_SoundIndex("*jump1.wav") );
				G_Sound(other, CHAN_AUTO, G_SoundIndex( "sound/movers/objects/objectHit.wav" ));

				//see if we can move to be next to the hand.. if it's not clear, break the throw.
				VectorClear(tAngles);
				tAngles[YAW] = ent->client->ps.viewangles[YAW];
				VectorCopy(ent->client->ps.origin, pBoltOrg);
				AngleVectors(tAngles, fwd, right, 0);
				boltOrg[0] = pBoltOrg[0] + fwd[0]*8 + right[0]*pDif;
				boltOrg[1] = pBoltOrg[1] + fwd[1]*8 + right[1]*pDif;
				boltOrg[2] = pBoltOrg[2];

				VectorSubtract(boltOrg, pBoltOrg, vDif);
				VectorNormalize(vDif);

				VectorClear(other->client->ps.velocity);
				intendedOrigin[0] = pBoltOrg[0] + vDif[0]*pDif;
				intendedOrigin[1] = pBoltOrg[1] + vDif[1]*pDif;
				intendedOrigin[2] = other->client->ps.origin[2];

				trap_Trace(&tr, intendedOrigin, other->r.mins, other->r.maxs, intendedOrigin, other->s.number, other->clipmask);
				trap_Trace(&tr2, ent->client->ps.origin, ent->r.mins, ent->r.maxs, intendedOrigin, ent->s.number, CONTENTS_SOLID);

				if (tr.fraction == 1.0 && !tr.startsolid && tr2.fraction == 1.0 && !tr2.startsolid)
				{
					VectorCopy(intendedOrigin, other->client->ps.origin);
				}
				else
				{ //if the guy can't be put here then it's time to break the throw off.
					vec3_t oppDir;
					int strength = 4;

					other->client->ps.heldByClient = 0;
					other->client->beingThrown = 0;
					ent->client->doingThrow = 0;

					ent->client->ps.forceHandExtend = HANDEXTEND_NONE;
					G_EntitySound( ent, CHAN_VOICE, G_SoundIndex("*pain25.wav") );

					other->client->ps.forceHandExtend = HANDEXTEND_NONE;
					VectorSubtract(other->client->ps.origin, ent->client->ps.origin, oppDir);
					VectorNormalize(oppDir);
					other->client->ps.velocity[0] = oppDir[0]*(strength*40);
					other->client->ps.velocity[1] = oppDir[1]*(strength*40);
					other->client->ps.velocity[2] = 150;

					VectorSubtract(ent->client->ps.origin, other->client->ps.origin, oppDir);
					VectorNormalize(oppDir);
					ent->client->ps.velocity[0] = oppDir[0]*(strength*40);
					ent->client->ps.velocity[1] = oppDir[1]*(strength*40);
					ent->client->ps.velocity[2] = 150;
				}
			}
		}
	}
#endif
#ifdef VM_MEMALLOC_DEBUG
	else if (Q_stricmp(cmd, "debugTestAlloc") == 0)
	{ //rww - small routine to stress the malloc trap stuff and make sure nothing bad is happening.
		char *blah;
		int i = 1;
		int x;

		//stress it. Yes, this will take a while. If it doesn't explode miserably in the process.
		while (i < 32768)
		{
			x = 0;

			trap_TrueMalloc((void **)&blah, i);
			if (!blah)
			{ //pointer is returned null if allocation failed
				trap_SendServerCommand( -1, va("print \"Failed to alloc at %i!\n\"", i));
				break;
			}
			while (x < i)
			{ //fill the allocated memory up to the edge
				if (x+1 == i)
				{
					blah[x] = 0;
				}
				else
				{
					blah[x] = 'A';
				}
				x++;
			}
			trap_TrueFree((void **)&blah);
			if (blah)
			{ //should be nullified in the engine after being freed
				trap_SendServerCommand( -1, va("print \"Failed to free at %i!\n\"", i));
				break;
			}

			i++;
		}

		trap_SendServerCommand( -1, "print \"Finished allocation test\n\"");
	}
#endif
#ifndef FINAL_BUILD
	else if (Q_stricmp(cmd, "debugShipDamage") == 0)
	{
		char	arg[MAX_STRING_CHARS];
		char	arg2[MAX_STRING_CHARS];
		int		shipSurf, damageLevel;

		trap_Argv( 1, arg, sizeof( arg ) );
		trap_Argv( 2, arg2, sizeof( arg2 ) );
		shipSurf = SHIPSURF_FRONT+atoi(arg);
		damageLevel = atoi(arg2);

		G_SetVehDamageFlags( &g_entities[ent->s.m_iVehicleNum], shipSurf, damageLevel );
	}
#endif
	else if (Q_stricmp(cmd, "grantadmin" ) == 0)
	{
		int client_id = -1; 
         char   arg1[MAX_STRING_CHARS]; 
		 if (ent->r.svFlags & SVF_ADMIN1)
		{
				if (!(roar_adminControl1.integer & (1 << A_GRANTADMIN)))
		{
			trap_SendServerCommand( ent-g_entities, va("print \"GrantAdmin is not allowed at this administration rank.\n\"") );
			return;
		}
			}
			else if (ent->r.svFlags & SVF_ADMIN2)
			{
				if (!(roar_adminControl2.integer & (1 << A_GRANTADMIN)))
		{
			trap_SendServerCommand( ent-g_entities, va("print \"GrantAdmin is not allowed at this administration rank.\n\"") );
			return;
		}
			}
			if (ent->r.svFlags & SVF_ADMIN3)
			{
				if (!(roar_adminControl3.integer & (1 << A_GRANTADMIN)))
		{
			trap_SendServerCommand( ent-g_entities, va("print \"GrantAdmin is not allowed at this administration rank.\n\"") );
			return;
		}
			}
			if (ent->r.svFlags & SVF_ADMIN4)
			{
				if (!(roar_adminControl4.integer & (1 << A_GRANTADMIN)))
		{
			trap_SendServerCommand( ent-g_entities, va("print \"GrantAdmin is not allowed at this administration rank.\n\"") );
			return;
		}
			}
			if (ent->r.svFlags & SVF_ADMIN5)
			{
				if (!(roar_adminControl5.integer & (1 << A_GRANTADMIN)))
		{
			trap_SendServerCommand( ent-g_entities, va("print \"GrantAdmin is not allowed at this administration rank.\n\"") );
			return;
		}
			}
			if (!(ent->r.svFlags & SVF_ADMIN1) && !(ent->r.svFlags & SVF_ADMIN2) && !(ent->r.svFlags & SVF_ADMIN3) && !(ent->r.svFlags & SVF_ADMIN4) && !(ent->r.svFlags & SVF_ADMIN5)){
				trap_SendServerCommand( ent-g_entities, "print \"Must login with /adminlogin (password)\n\"" );
				return;
			}
         if ( trap_Argc() < 3 ) 
         { 
            trap_SendServerCommand( ent-g_entities, "print \"^3Usage: ^5/grantadmin (client) (level)\n^3Admin Levels 1-5\n\"" ); 
            return; 
         }
         trap_Argv( 1,  arg1, sizeof( arg1 ) );
		 client_id = G_ClientNumberFromArg(  arg1 ); 
		 trap_Argv( 2,  arg1, sizeof( arg1 ) );
		 if( Q_stricmp( arg1, "1" ) == 0 ){
			g_entities[client_id].r.svFlags |= SVF_ADMIN1;
			g_entities[client_id].r.svFlags &= ~SVF_ADMIN2;
			g_entities[client_id].r.svFlags &= ~SVF_ADMIN3;
			g_entities[client_id].r.svFlags &= ~SVF_ADMIN4;
			g_entities[client_id].r.svFlags &= ~SVF_ADMIN5;
			 g_entities[client_id].client->pers.iamanadmin = qtrue;
			 trap_SendServerCommand( -1, va("print \"%s %s\n\"", g_entities[client_id].client->pers.netname, roar_AdminLogin1_saying.string ));
		 }
		 else if( Q_stricmp( arg1, "2" ) == 0 ){
			 if (!(ent->r.svFlags & SVF_ADMIN2)){
				 trap_SendServerCommand( ent-g_entities, va("print \"You are not admin level 2. Therefore you cannot grant it to other clients.\n\"" ));
				 return;
			 }
			 g_entities[client_id].r.svFlags |= SVF_ADMIN2;
			 g_entities[client_id].r.svFlags &= ~SVF_ADMIN1;
			g_entities[client_id].r.svFlags &= ~SVF_ADMIN3;
			g_entities[client_id].r.svFlags &= ~SVF_ADMIN4;
			g_entities[client_id].r.svFlags &= ~SVF_ADMIN5;
			 g_entities[client_id].client->pers.iamanadmin = qtrue;
			 trap_SendServerCommand( -1, va("print \"%s %s\n\"", g_entities[client_id].client->pers.netname, roar_AdminLogin2_saying.string ));
		 }
		 else if( Q_stricmp( arg1, "3" ) == 0 ){
			 if (!(ent->r.svFlags & SVF_ADMIN3)){
				 trap_SendServerCommand( ent-g_entities, va("print \"You are not admin level 3. Therefore you cannot grant it to other clients.\n\"" ));
				 return;
			 }
			 g_entities[client_id].r.svFlags |= SVF_ADMIN3;
			 g_entities[client_id].r.svFlags &= ~SVF_ADMIN2;
			g_entities[client_id].r.svFlags &= ~SVF_ADMIN1;
			g_entities[client_id].r.svFlags &= ~SVF_ADMIN4;
			g_entities[client_id].r.svFlags &= ~SVF_ADMIN5;
			 g_entities[client_id].client->pers.iamanadmin = qtrue;
			 trap_SendServerCommand( -1, va("print \"%s %s\n\"", g_entities[client_id].client->pers.netname, roar_AdminLogin3_saying.string ));
		 }
		 else if( Q_stricmp( arg1, "4" ) == 0 ){
			 if (!(ent->r.svFlags & SVF_ADMIN4)){
				 trap_SendServerCommand( ent-g_entities, va("print \"You are not admin level 4. Therefore you cannot grant it to other clients.\n\"" ));
				 return;
			 }
			 g_entities[client_id].r.svFlags |= SVF_ADMIN4;
			 g_entities[client_id].r.svFlags &= ~SVF_ADMIN2;
			g_entities[client_id].r.svFlags &= ~SVF_ADMIN3;
			g_entities[client_id].r.svFlags &= ~SVF_ADMIN1;
			g_entities[client_id].r.svFlags &= ~SVF_ADMIN5;
			 g_entities[client_id].client->pers.iamanadmin = qtrue;
			 trap_SendServerCommand( -1, va("print \"%s %s\n\"", g_entities[client_id].client->pers.netname, roar_AdminLogin4_saying.string ));
		 }
		 else if( Q_stricmp( arg1, "5" ) == 0 ){
			 if (!(ent->r.svFlags & SVF_ADMIN5)){
				 trap_SendServerCommand( ent-g_entities, va("print \"You are not admin level 5. Therefore you cannot grant it to other clients.\n\"" ));
				 return;
			 }
			 g_entities[client_id].r.svFlags |= SVF_ADMIN5;
			 g_entities[client_id].r.svFlags &= ~SVF_ADMIN2;
			g_entities[client_id].r.svFlags &= ~SVF_ADMIN3;
			g_entities[client_id].r.svFlags &= ~SVF_ADMIN4;
			g_entities[client_id].r.svFlags &= ~SVF_ADMIN1;
			 g_entities[client_id].client->pers.iamanadmin = qtrue;
			 trap_SendServerCommand( -1, va("print \"%s %s\n\"", g_entities[client_id].client->pers.netname, roar_AdminLogin5_saying.string ));
		 }
	}

	else if (Q_stricmp(cmd, "clanlogin" ) == 0) // client command: clanlogin <password>
      { 
         char   pass[MAX_STRING_CHARS]; 

         trap_Argv( 1, pass, sizeof( pass ) ); // password

		 if ( trap_Argc() != 2 ){
            trap_SendServerCommand( clientNum, "print \"Usage: clanlogin <password>\n\"" ); 
            return; 
			}

		if ( !Q_stricmp( pass, g_clanPassword.string ) ) {
         ent->r.svFlags |= SVF_CLANSAY;
		 ent->client->pers.iamclan = qtrue;
         trap_SendServerCommand( -1, va("print \"%s %s\n\"", ent->client->pers.netname, roar_ClanLogin_saying.string ));
         return; 
		}
		else if ( ent->r.svFlags & SVF_CLANSAY ) {
            trap_SendServerCommand( clientNum, "print \"You are already logged in. Type in /clanlogout to remove clan status.\n\"" ); 
            return; 
         }
		else {
		 trap_SendServerCommand( ent-g_entities, va("print \"Clan password is incorrect!\n\"", ent->client->pers.netname ));
		 }
	  }

	else if (Q_stricmp(cmd, "adminlogin" ) == 0) // client command: adminlogin <password>
      { 
         char   pass[MAX_STRING_CHARS]; 

         gentity_t *recipient; 

         trap_Argv( 1, pass, sizeof( pass ) ); // password

         if ( trap_Argc() == 2 ) 
         { 
            recipient = ent; 
         }
         else 
         { 
            trap_SendServerCommand( clientNum, "print \"Usage: adminlogin <password>\n\"" ); 
            return; 
         }

         if ( recipient->r.svFlags & SVF_ADMIN1 ) {
            trap_SendServerCommand( clientNum, "print \"You are already logged in. Type in /adminlogout to remove admin status.\n\"" ); 
            return; 
         }
		 if ( recipient->r.svFlags & SVF_ADMIN2 ) {
            trap_SendServerCommand( clientNum, "print \"You are already logged in. Type in /adminlogout to remove admin status.\n\"" ); 
            return; 
         } 
		 if ( recipient->r.svFlags & SVF_ADMIN3 ) {
            trap_SendServerCommand( clientNum, "print \"You are already logged in. Type in /adminlogout to remove admin status.\n\"" ); 
            return; 
         } 
		 if ( recipient->r.svFlags & SVF_ADMIN4 ) {
            trap_SendServerCommand( clientNum, "print \"You are already logged in. Type in /adminlogout to remove admin status.\n\"" ); 
            return; 
         } 
		 if ( recipient->r.svFlags & SVF_ADMIN5 ) {
            trap_SendServerCommand( clientNum, "print \"You are already logged in. Type in /adminlogout to remove admin status.\n\"" ); 
            return; 
         }

		 if ( !Q_stricmp( pass, "" ) ) { //Blank? Don't log in!
			 return;
		 }
		 if ( !Q_stricmp( pass, g_adminPassword1.string ) ) {
         recipient->r.svFlags |= SVF_ADMIN1;
		 recipient->r.svFlags |= SVF_ADMINSAY;
		 ent->client->pers.iamanadmin = qtrue;
         trap_SendServerCommand( -1, va("print \"%s %s\n\"", ent->client->pers.netname, roar_AdminLogin1_saying.string ));
         return; 
		 }
		 if ( !Q_stricmp( pass, g_adminPassword2.string ) ) {
         recipient->r.svFlags |= SVF_ADMIN2;
		 recipient->r.svFlags |= SVF_ADMINSAY;
		 ent->client->pers.iamanadmin = qtrue;
         trap_SendServerCommand( -1, va("print \"%s %s\n\"", ent->client->pers.netname, roar_AdminLogin2_saying.string ));
         return; 
		 }
		 if ( !Q_stricmp( pass, g_adminPassword3.string ) ) {
         recipient->r.svFlags |= SVF_ADMIN3;
		 recipient->r.svFlags |= SVF_ADMINSAY;
		 ent->client->pers.iamanadmin = qtrue;
         trap_SendServerCommand( -1, va("print \"%s %s\n\"", ent->client->pers.netname, roar_AdminLogin3_saying.string ));
         return; 
		 }
		 if ( !Q_stricmp( pass, g_adminPassword4.string ) ) {
         recipient->r.svFlags |= SVF_ADMIN4;
		 recipient->r.svFlags |= SVF_ADMINSAY;
		 ent->client->pers.iamanadmin = qtrue;
         trap_SendServerCommand( -1, va("print \"%s %s\n\"", ent->client->pers.netname, roar_AdminLogin4_saying.string ));
         return; 
		 }
		 if ( !Q_stricmp( pass, g_adminPassword5.string ) ) {
         recipient->r.svFlags |= SVF_ADMIN5;
		 recipient->r.svFlags |= SVF_ADMINSAY;
		 ent->client->pers.iamanadmin = qtrue;
         trap_SendServerCommand( -1, va("print \"%s %s\n\"", ent->client->pers.netname, roar_AdminLogin5_saying.string ));
         return;
		 }
		 else {
		 trap_SendServerCommand( ent-g_entities, va("print \"Admin password is incorrect!\n\"", ent->client->pers.netname ));
		 }
      }
	  else if (Q_stricmp(cmd, "clanlogout") == 0)  // client command: adminlogout 
      { 
         if ( ent->r.svFlags & SVF_CLANSAY )
         { 
			ent->client->pers.iamclan = qfalse;
            ent->r.svFlags &= ~SVF_CLANSAY;
			trap_SendServerCommand( -1, va("print \"%s %s\n\"", ent->client->pers.netname, roar_ClanLogout_saying.string ));             
         }
	  }
            else if (Q_stricmp(cmd, "adminlogout") == 0)  // client command: adminlogout 
      { 
         if ( ent->r.svFlags & SVF_ADMIN1 )
         { 
			ent->client->pers.iamanadmin = qfalse;
            ent->r.svFlags &= ~SVF_ADMIN1;
			ent->r.svFlags &= ~SVF_ADMINSAY;
			trap_SendServerCommand( -1, va("print \"%s %s\n\"", ent->client->pers.netname, roar_AdminLogout1_saying.string ));             
         }
		 if ( ent->r.svFlags & SVF_ADMIN2 )
         { 
			ent->client->pers.iamanadmin = qfalse;
            ent->r.svFlags &= ~SVF_ADMIN2;
			ent->r.svFlags &= ~SVF_ADMINSAY;
			trap_SendServerCommand( -1, va("print \"%s %s\n\"", ent->client->pers.netname, roar_AdminLogout2_saying.string ));             
         }
		 if ( ent->r.svFlags & SVF_ADMIN3 )
         { 
			ent->client->pers.iamanadmin = qfalse;
            ent->r.svFlags &= ~SVF_ADMIN3;
			ent->r.svFlags &= ~SVF_ADMINSAY;
			trap_SendServerCommand( -1, va("print \"%s %s\n\"", ent->client->pers.netname, roar_AdminLogout3_saying.string ));             
         } 
		 if ( ent->r.svFlags & SVF_ADMIN4 )
         { 
			ent->client->pers.iamanadmin = qfalse;
            ent->r.svFlags &= ~SVF_ADMIN4;
			ent->r.svFlags &= ~SVF_ADMINSAY;
			trap_SendServerCommand( -1, va("print \"%s %s\n\"", ent->client->pers.netname, roar_AdminLogout4_saying.string ));             
         } 
		 if ( ent->r.svFlags & SVF_ADMIN5 )
         { 
			ent->client->pers.iamanadmin = qfalse;
            ent->r.svFlags &= ~SVF_ADMIN5;
			ent->r.svFlags &= ~SVF_ADMINSAY;
			trap_SendServerCommand( -1, va("print \"%s %s\n\"", ent->client->pers.netname, roar_AdminLogout5_saying.string ));             
         } 
      }
	else
	{
		if (Q_stricmp(cmd, "addbot") == 0)
		{ //because addbot isn't a recognized command unless you're the server, but it is in the menus regardless
//			trap_SendServerCommand( clientNum, va("print \"You can only add bots as the server.\n\"" ) );
			trap_SendServerCommand( clientNum, va("print \"%s.\n\"", G_GetStringEdString("MP_SVGAME", "ONLY_ADD_BOTS_AS_SERVER")));
		}
		else
		{
			trap_SendServerCommand( clientNum, va("print \"unknown cmd %s\n\"", cmd ) );
		}
	}
}
