// Copyright (C) 2003 - 2006 - Michael J. Nohai
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of agreement written in the JAE Mod Source.doc.
// See JKA Game Source License.htm for legal information with Raven Software.
// Use this code at your own risk.

#include "g_local.h"
#include "g_adminshared.h"
void runEmote( animNumber_t emote, gentity_t *ent,int freeze) {
   if(freeze) {
      if(ent->client->ps.legsAnim == emote) {
                              ent->client->ps.forceDodgeAnim = 0;
                        ent->client->ps.forceHandExtendTime = 0;
				ent->client->emote_freeze=0;
				ent->client->ps.saberCanThrow = qtrue;
      }
   } else {
               ent->client->ps.forceHandExtend = HANDEXTEND_TAUNT;
            ent->client->ps.forceDodgeAnim = emote;
            ent->client->ps.forceHandExtendTime = level.time + Q3_INFINITE;
				ent->client->ps.saberCanThrow = qfalse;
				ent->client->emote_freeze=1;
				StandardSetBodyAnim(ent, emote, SETANIM_FLAG_OVERRIDE|SETANIM_FLAG_HOLD|SETANIM_FLAG_HOLDLESS);
				ent->client->ps.saberMove = LS_NONE;
				ent->client->ps.saberBlocked = 0;
				ent->client->ps.saberBlocking = 0;
   }
}
/*
===========
G_IgnoreClientChat

Instructs all chat to be ignored by the given 
============
*/
void G_IgnoreClientChat ( int ignorer, int ignoree, qboolean ignore )
{
	// Cant ignore yourself
	if ( ignorer == ignoree )
	{
		return;
	}

	// If there is no client connected then dont bother
	if ( g_entities[ignoree].client->pers.connected != CON_CONNECTED )
	{
		return;
	}

	if ( ignore )
	{
		g_entities[ignoree].client->sess.chatIgnoreClients[ignorer/32] |= (1<<(ignorer%32));
	}
	else
	{
		g_entities[ignoree].client->sess.chatIgnoreClients[ignorer/32] &= ~(1<<(ignorer%32));
	}
}

/*
===========
G_IsClientChatIgnored

Checks to see if the given client is being ignored by a specific client
============
*/
qboolean G_IsClientChatIgnored ( int ignorer, int ignoree )
{
	if ( g_entities[ignoree].client->sess.chatIgnoreClients[ignorer/32] & (1<<(ignorer%32)) )
	{
		return qtrue;
	}

	return qfalse;
}
/*
===========
G_RemoveFromAllIgnoreLists

Clears any possible ignore flags that were set and not reset.
============
*/
void G_RemoveFromAllIgnoreLists( int ignorer ) 
{
	int i;

	for( i = 0; i < level.maxclients; i++) {
		g_entities[i].client->sess.chatIgnoreClients[ignorer/32] &= ~(1 << ( ignorer%32 ));
	}
}

// Fix: MasterHex //RoAR mod NOTE: Causes a crash with Q3Fill. Therefore, this mod must be built in DEBUG again...
/*void FR_NormalizeForcePowers(char *powerOut, int powerLen)
{
	char powerBuf[128];
	char readBuf[2];
	int finalPowers[21] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
	int i, c;

	if (powerLen >= 128 || powerLen < 1)
	{ //This should not happen. If it does, this is obviously a bogus string.
		//They can have this string. Because I said so.
		Q_strncpyz(powerBuf, "7-1-032330000000001333", sizeof(powerBuf)); //Fix: Ensiform
	}
	else
	{
		Q_strncpyz(powerBuf, powerOut, sizeof(powerBuf)); //copy it as the original
	}

	c = 0;
	i = 0;
	while (i < powerLen && i < 128 && powerBuf[i] && powerBuf[i] != '\n' && powerBuf[i] != '\r' && powerBuf != '\0'  //standard sanity checks
		&& c < NUM_FORCE_POWERS+2) //Fix: Ensiform
	{
		if (powerBuf[i] != '-')
		{
			readBuf[0] = powerBuf[i];
			readBuf[1] = 0;
			finalPowers[c] = atoi(readBuf);
			c++;
		}
		i++;
	}

	strcpy(powerOut, va("%i-%i-%i%i%i%i%i%i%i%i%i%i%i%i%i%i%i%i%i%i\0",
						finalPowers[0], finalPowers[1], finalPowers[2], 
						finalPowers[3], finalPowers[4], finalPowers[5], 
						finalPowers[6], finalPowers[7], finalPowers[8], 
						finalPowers[9], finalPowers[10], finalPowers[11], 
						finalPowers[12], finalPowers[13], finalPowers[14], 
						finalPowers[15], finalPowers[16], finalPowers[17], 
						finalPowers[18], finalPowers[19], finalPowers[20]));
}*/
/*
==================

M_HolsterThoseSabers - MJN

Something like Cmd_ToggleSaber, 
but stripped down and for holster only.
==================
*/
void M_HolsterThoseSabers( gentity_t *ent ){

	// MJN - Check to see if that is the weapon of choice...
	if (ent->client->ps.weapon != WP_SABER)
	{
		return;
	}
	// MJN - Cannot holster it in flight or we're screwed!
	if (ent->client->ps.saberInFlight)
	{
		return;
	}
	// MJN - Cannot holster in saber lock.
	if (ent->client->ps.saberLockTime >= level.time)
	{
		return;
	}
	// MJN - Holster Sabers
	if ( ent->client->ps.saberHolstered < 2 ){
		if (ent->client->saber[0].soundOff){
			G_Sound(ent, CHAN_AUTO, ent->client->saber[0].soundOff);
		}
		if (ent->client->saber[1].soundOff && ent->client->saber[1].model[0]){
			G_Sound(ent, CHAN_AUTO, ent->client->saber[1].soundOff);
		}
		ent->client->ps.saberHolstered = 2;
		ent->client->ps.weaponTime = 400;
	}
}