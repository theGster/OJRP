#include <iostream>
#include <stdlib.h>
#include <string.h>
//#include "q3jafont.h"
#include "font_create.h"

#define STEP_1x 256
#define STEP_1y 256
#define STEP_2x 512
#define STEP_2y 256
#define STEP_3x 512
#define STEP_3y 512
#define STEP_4x 1024
#define STEP_4y 512
#define STEP_5x 1024
#define STEP_5y 1024

int ftSize;
int blitsizex;
int blitsizey;

//using namespace std;
//int wuWriteFontDat( wJAf_t *wfont, char *filename );
int wuFileSuffixSet( char *ftName, char *suffix, char *endName );
int wuCreateFont( wJAf_t *wfont, char *filename, int ptSize, char *fontname );
wJAf_t wuFont;
extern int can_not_add;

int main(int argc, char *argv[])
{
	int step;
	char ftName[64];
	char endName[64];
	printf( " =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=\n");
	printf( " JAfont by Wudan, OJP et al, built with help from Bejitt and Fracmann\n" );
	printf( " A Tool for creating fonts for Jedi Academy from TrueType font files.\n" );
	printf( " Version 0.0.1a\n" );
	printf( " =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=\n");
	
	int comp_check;
	if( argc == 1 )
	{
		printf( " usage: JAfont <command> <fontfile> <size> <JAfontname>\n" );
		printf( " example: jafont -create verdana.ttf 16 anewhope\n\n" );
		
		//system("PAUSE");
		return 0;
	}
	if( !strcasecmp( argv[1], "-create" ) )
	{
		//printf( "Create Option:\n\n" );
		strcpy( ftName, argv[2] );
		ftSize = atoi( argv[3] );
		strcpy( endName, argv[4] );
		
		step = 1;
		while( step < 6 )
		{
			can_not_add = 0;
			switch(step)
			{
				case 1:
				blitsizex = STEP_1x;
				blitsizey = STEP_1y;
				break;
				case 2:
				blitsizex = STEP_2x;
				blitsizey = STEP_2y;
				break;
				case 3:
				blitsizex = STEP_3x;
				blitsizey = STEP_3y;
				break;
				case 4:
				blitsizex = STEP_4x;
				blitsizey = STEP_4y;
				break;
				case 5:
				blitsizex = STEP_5x;
				blitsizey = STEP_5y;
				break;
			}
			wuCreateFont( &wuFont, ftName, ftSize, endName );
			if( can_not_add )
			{
				//printf( " --- step %d: unsuccessful (%d), couldn't make %d by %d image.\n", step, can_not_add, blitsizex, blitsizey );
				step++;
			}
			else
			{
				printf( " --- (%d) All glyphs blitted to image successfully to a %d by %d image.\n", ftSize, blitsizex, blitsizey );
				step = 6;
			}
		}
		return 0;
	}
	//else
	//{
		printf( " Hello out there?\n\n" );
		//printf( " usage: JAfont <command> <fontfile> <size> <JAfontname>\n\n" );
		//return 1;
	//}
	/*else
	{
	ftSize = 20;
	step = 1;
	while( step < 6 )
	{
		can_not_add = 0;
		switch(step)
		{
			case 1:
			blitsizex = STEP_1x;
			blitsizey = STEP_1y;
			break;
			case 2:
			blitsizex = STEP_2x;
			blitsizey = STEP_2y;
			break;
			case 3:
			blitsizex = STEP_3x;
			blitsizey = STEP_3y;
			break;
			case 4:
			blitsizex = STEP_4x;
			blitsizey = STEP_4y;
			break;
			case 5:
			blitsizex = STEP_5x;
			blitsizey = STEP_5y;
			break;
		}
		wuCreateFont( &wuFont, "TT0590M_.TTF", ftSize, "sizetest" );
		if( can_not_add )
		{
			printf( " --- step %d: unsuccessful (%d), couldn't make %d by %d image.\n", step, can_not_add, blitsizex, blitsizey );
			step++;
		}
		else
		{
			printf( " --- (%d) All glyphs blitted to image successfully to a %d by %d image.\n", ftSize, blitsizex, blitsizey );
			step = 6;
		}
	}
	}*/
	/*
	while( ftSize > 10 )
	{
		can_not_add = 0;
		wuCreateFont( &wuFont, "TT0590M_.TTF", ftSize, "sizetest" );
		if( can_not_add )
		{
			printf( " --- (%d) %d glyphs could not fit on the image.\n", ftSize, can_not_add );
		}
		else
		{
			printf( " --- (%d) All glyphs blitted to image successfully.\n", ftSize );
			ftSize = 0;

		}
		ftSize--;
	}*/

	//sufchk = wuFileSuffixSet( "boogey.goo", ".png", picName );
	//printf( "%d %s\n\n", sufchk, picName );
	//wuWriteFontDat( &wuFont, "TT0590M_.TTF", "superfly" );
	
	return 1;
}
