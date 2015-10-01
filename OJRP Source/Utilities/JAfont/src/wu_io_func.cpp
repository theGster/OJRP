#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include "wu_io_func.h"
//wu_io_func.cpp

void StringSet( char *in, const char *msg, ... )
{
	int i;
	va_list         argptr;
	char            text[1024];
	
	va_start (argptr, msg);
	vsprintf (text, msg, argptr);
	va_end (argptr);
	
	i = 0;
	while( text[i] != '\0' )
	{
		in[i] = text[i];
		i++;
	}
	
	if( text[i] == '\0' )
	{
		in[i] = '\0';
	}
	return;
}

float getf( FILE *file )
{
	float_t *ieee;
	float number;

	ieee = NULL;
	ieee = new float_t;
	if( ieee == NULL )
	{
		return 0.0;
	}
	fread( ieee, 4, 1, file );
	number = (float)(ieee->num);
	delete ieee;
	return number;
}

int gets( FILE *file )
{
	short_t *smalley;
	int number;

	smalley = NULL;
	smalley = new short_t;
	if( smalley == NULL )
	{
		return 0;
	}
	fread( smalley, 2, 1, file );
	number = (int)(smalley->num);
	delete smalley;
	return number;
}

void putf( float number, FILE *file )
{
	float_t *ieee;
	//float number;

	ieee = NULL;
	ieee = new float_t;
	if( ieee == NULL )
	{
		return;
	}
	ieee->num = number;
	fwrite( ieee, 4, 1, file );
	delete ieee;
}

void puts( int number, FILE *file )
{
	short_t *smalley;
	//int number;

	smalley = NULL;
	smalley = new short_t;
	if( smalley == NULL )
	{
		return;
	}
	smalley->num = number;
	fwrite( smalley, 2, 1, file );
	delete smalley;
}
