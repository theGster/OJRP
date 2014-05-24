#include "g_local.h"
#include "g_roll.h"


void Cmd_Roll_F(gentity_t *ent)
{
	char maxNumTemp[10] = { 0 };
	int maxNum = 0;

	if (trap_Argc() < 2)
	{
		trap_SendServerCommand(ent - g_entities, "print \"^2Command Usage: /roll <numberOfSidesOnDice> (Example: /roll 50 results in 'Name rolls a(n) x on a 50 sided dice.')\n\"");
		return;
	}

	trap_Argv(1, maxNumTemp, sizeof(maxNumTemp));
	maxNum = atoi(maxNumTemp);

	if (maxNum < 3 || maxNum > 100)
	{
		trap_SendServerCommand(ent - g_entities, "print \"^1The dice must have at least 3 sides and no more than 100 sides.\n\"");
		return;
	}

	G_Say(ent, NULL, SAY_ALL, va("^3rolls %i on a %i sided dice.", Q_irand(1, maxNum), maxNum));
	return;
}