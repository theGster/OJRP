#include <stdio.h>
#include "q3jafont.h"
//#include "wubasegl.h"

//extern GLuint texture[2];

void ReadQ3Font( char *filename, q3font_t font[256] )
{
	FILE *fp;
	fp = fopen(filename, "rb");
	int i;
	i = 0;
	while( i < 255 )
	{
		font[i].height = getw( fp );
		font[i].top = getw( fp );
		font[i].bottom = getw( fp );
		font[i].pitch = getw( fp );
		font[i].xskip = getw( fp );
		font[i].phWidth = getw( fp );
		font[i].phHeight = getw( fp );
		font[i].st1[0] = getf( fp );
		font[i].st1[1] = getf( fp );
		font[i].st2[0] = getf( fp );
		font[i].st2[1] = getf( fp );
		font[i].glyph = getw( fp );
		fread( font[i].texname, 32, 1, fp );
		i++;
	}
}

void Q3FontOut( char *filename, q3font_t font[256] )
{
	int i;
	FILE *outfile;
	outfile = fopen(filename, "w");
	i = 0;
	while( i < 255 )
	{
		//fprintf( outfile, "%d: height: %d \n", i, font[i].height );
		fprintf( outfile, "%d: tex: %s \n", i, font[i].texname );
		i++;
	}
	fclose( outfile );
}

void ConvertQ3FonttoJAFont( q3font_t q3f[256], jafont_t jaf[256] )
{
	int i;
	i = 0;
	while( i < 255 )
	{
		jaf[i].phWidth  = q3f[i].phWidth;
		jaf[i].phHeight = q3f[i].phHeight;
		jaf[i].hAdv     = q3f[i].xskip;
		jaf[i].hOfs     = 0;
		jaf[i].baseline = q3f[i].top;
		jaf[i].st1[0] = q3f[i].st1[0];
		jaf[i].st1[1] = q3f[i].st1[1];
		jaf[i].st2[0] = q3f[i].st2[0];
		jaf[i].st2[1] = q3f[i].st2[1];
		i++;
	}
}

void WriteJAFont( char *filename, jafont_t font[256] )
{
	FILE *fp;
	char magic[4];
	magic[0] = 0;
	magic[1] = 0;
	magic[2] = 0;
	magic[3] = 0;
	int i;
	int def = 0;
	fp = fopen(filename, "wb");
	fseek( fp, 0, SEEK_SET );
	i = 0;
	while( i < 256 )
	{
		puts( font[i].phWidth, fp );
		puts( font[i].phHeight, fp );
		puts( font[i].hAdv, fp );
		puts( font[i].hOfs, fp );
		putw( font[i].baseline, fp );
		putf( font[i].st1[0], fp );
		putf( font[i].st1[1], fp );
		putf( font[i].st2[0], fp );
		putf( font[i].st2[1], fp );
		i++;
	}
	puts( 16, fp );
	puts( 18, fp );
	puts( 16, fp );
	puts( 12, fp );
	puts( 0, fp );
	puts( 0, fp );
	fclose( fp );
}

void ReadJAFont( char *filename, jafont_t font[256], jahead_t head )
{
	FILE *fp;
	fp = fopen(filename, "rb");
	int i;
	i = 32;
	fseek( fp, 896, SEEK_SET );
	while( i < 255 )
	{
		font[i].phWidth = gets( fp );
		font[i].phHeight = gets( fp );
		font[i].hAdv = gets( fp );
		font[i].hOfs = gets( fp );
		font[i].baseline = getw( fp );
		font[i].st1[0] = getf( fp );
		font[i].st1[1] = getf( fp );
		font[i].st2[0] = getf( fp );
		font[i].st2[1] = getf( fp );
		i++;
	}
	head.mPointSize = gets( fp );
	head.mHeight = gets( fp );
	head.mAscender = gets( fp );
	head.mDescender = gets( fp );
	head.mKoreanHack = gets( fp );
	fclose( fp );
}

void JAFontOut( char *filename, jafont_t font[256] )
{
	int i;
	FILE *outfile;
	outfile = fopen(filename, "w");
	i = 32;
	while( i < 255 )
	{
		fprintf( outfile, "%d:\n", i );
		fprintf( outfile, "\tphWidth : %d\n", font[i].phWidth );
		fprintf( outfile, "\tphHeight: %d\n", font[i].phHeight );
		fprintf( outfile, "\thAdv    : %d\n", font[i].hAdv );
		fprintf( outfile, "\thOfs    : %d\n", font[i].hOfs );
		fprintf( outfile, "\tbaseline: %d\n\n", font[i].baseline );
		i++;
	}
	fclose( outfile );
}
