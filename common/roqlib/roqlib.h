//=======================================================================
//			Copyright XashXT Group 2007 �
//			  roqlib.h - roq video maker
//=======================================================================
#ifndef ROQLIB_H
#define ROQLIB_H

#include "platform.h"
#include "utils.h"

typedef struct { byte *rgb; } videoFrame_t;
typedef struct { int x,y; } motionVector_t;
typedef struct { byte yuv[6]; } yuvBlock2_t;
typedef struct { byte indexes[4]; } yuvDiskBlock4_t;
typedef struct { yuvBlock2_t block[4]; } yuvBlock4_t;
typedef struct { byte rgb[8*8*3]; } rgbBlock8_t;
typedef struct { byte rgb[4*4*3]; } rgbBlock4_t;
typedef struct { byte rgb[2*2*3]; } rgbBlock2_t;

enum
{
	ROQENC_PREF_KEYFRAME,
	ROQENC_PREF_WIDTH,
	ROQENC_PREF_HEIGHT,
	ROQENC_PREF_GOAL_SIZE_BITS,
	ROQENC_PREF_REFINEMENT_PASSES,
	ROQENC_PREF_CULL_THRESHOLD,
	ROQENC_PREF_COUNT,
};

typedef struct roq_stats_s
{
	byte	codebookResults4[256*4*4*3];
	byte	codebookResults2[256*2*2*3];
	uint	vectorsProvided4;
	uint	vectorsQuantized4;
	int	bytesConsumed;
	int	bytesMax;
	byte	*reconstructResult;

} roq_stats_t;

typedef struct possibilityList_s
{
	double	bestDownscaleRate;
	int	mse[259];
	int	currentMSE;
	dword	currentSize;
	word	currentChoice;
	word	bestDownscaleChoice;
	byte	downscalePossible;

} possibilityList_t;

typedef struct roq_compressor_s
{
	int		initialized;
	int		wroteInfo;
	int		frameNum;		// hack to fix RoQ bug
	dword		preferences[ROQENC_PREF_COUNT];
	videoFrame_t	frames[3];
	videoFrame_t	*frameCycle[3];

	// local data
	uint		*devLUT8;
	uint		*devLUT4;
	uint		*devLUT2;
	uint		*devMotion8;
	uint		*devMotion4;
	uint		*devSkip4;	// Skip8 can be calculated from this

	motionVector_t	*motionVectors8;
	motionVector_t	*motionVectors4;
	byte		*lut8;
	byte		*lut4;
	byte		*lut2;
	byte		*recon;		// frame reconstruction after compression
	byte		*preWrite;

	possibilityList_t	*pList;
	uint		*listSort;
	int		*optimizedOut4;
	int		framesSinceKeyframe;

} roq_compressor_t;

extern roq_compressor_t *comp;
extern dword vidWidth;
extern dword vidHeight;
extern dword vidScanWidth;
extern byte *rgbFrame0;
extern byte *rgbFrame1;
extern byte *rgbFrame2;
extern uint refinementPasses;
extern rgbBlock8_t codebook8[256];
extern rgbBlock4_t codebook4[256];
extern rgbBlock2_t codebook2[256];
extern uint quickDiff[256*256];
extern yuvBlock2_t yuvCodebook2[256];
extern yuvDiskBlock4_t yuvDisk4[256];
extern int *optimizedOut4;
extern roq_stats_t stats;

#define QUICKDIFF(a,b)		quickDiff[((a)<<8)+(b)]
#define PERTURBATION_BASE_POWER	6

void ROQ_InitQuickdiff(void);
int  ROQ_GetCodebooks(void);
void ROQ_GenerateBlocks(void);
void ROQ_InitYUV(void);
void YUV2toRGB2(byte *yuv, byte *rgb);
void RGB2toYUV2(byte *rgb, byte *yuv);
void YUVPerturb2(yuvBlock2_t *a, yuvBlock2_t *b, uint step);
uint YUVDifference2(yuvBlock2_t *a, yuvBlock2_t *b);
int  YUVGenerateCodebooks4(yuvBlock4_t *input, uint inputCount, uint goalCells, uint *resultCount, yuvBlock4_t **resultElements);
int  YUVGenerateCodebooks2(yuvBlock2_t *input, uint inputCount, uint goalCells, uint *resultCount, yuvBlock2_t **resultElements);
void ROQ_RGBtoYUV(byte r, byte g, byte b, byte *yp, byte *up, byte *vp);
void ROQ_YUVtoRGB(byte y, byte u, byte v, byte *rp, byte *gp, byte *bp);
dword ROQ_CompressRGB( roq_compressor_t *compressor, void *file, byte *rgb);

#define CHUNKMARK_INFO          0x1001
#define CHUNKMARK_CODEBOOK      0x1002
#define CHUNKMARK_VIDEO         0x1011
#define CHUNKMARK_AUDIO_MONO    0x1020
#define CHUNKMARK_AUDIO_STEREO  0x1021

extern byte *roqpool;
#define RQalloc( size )	Mem_Alloc(roqpool, size )
#define RFree(x) if(x)	Mem_Free(x)

// utils
void ROQ_Blit(byte *source, byte *dest, dword scanWidth, dword sourceSkip, dword destSkip, dword rows);
void ROQ_DoubleSize(byte *source, byte *dest, dword dim);

#endif//ROQLIB_H