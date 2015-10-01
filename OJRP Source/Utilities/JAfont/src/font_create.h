#ifndef WU_FREETYPE_H
#define WU_FREETYPE_H

#include <ft2build.h>
#include <freetype/freetype.h>
#include <freetype/ftglyph.h>
#include <freetype/ftoutln.h>
#include <freetype/fttrigon.h>

//wu_FreeType.h

typedef struct {
	int phWidth;
	int phHeight;
	int hAdv;
	int hOfs;
	int baseline;
	float st1[2];
	float st2[2];
} wJAg_t;

typedef struct {
	wJAg_t glyph[256];
	int mPointSize;
	int mHeight;
	int mAscender;
	int mDescender;
	int mKoreanHack;
	int baselineOFs;
	float fscale;
	float h;
} wJAf_t;

#endif
