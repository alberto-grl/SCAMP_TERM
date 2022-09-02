/*
 * TestSCAMP.h
 *
 *  Created on: 10 feb 2022
 *      Author: Alberto Garlassi I4NZX
 */

#ifndef TESTSCAMP_H_
#define TESTSCAMP_H_



/* #define SAMPLE_RATE  (17932) // Test failure to open with this value. */
//was 44100
#define SAMPLE_RATE  (48000)
//was 512 or 4096, but gave clicking noises only with simultaneous in/out callback. On my system FRAMES_PER_BUFFER = 0 sets buffer at 128, which is OK
#define FRAMES_PER_BUFFER (4096)
#define NUM_CHANNELS    (2)
#define NUM_WRITES_PER_BUFFER   (16)
/* #define DITHER_FLAG     (paDitherOff) */
#define DITHER_FLAG     (0) /**/
/** Set to 1 if you want to capture the recording to a file. */
#define WRITE_TO_FILE   (0)

#define N_OVERSAMPLING 	(16) /* brings sample F down to 12 Khz. Tones at 1000 and 900 Hz */


/* Select sample format. */
#if 1
#define PA_SAMPLE_TYPE  paFloat32
typedef float SAMPLE;
#define SAMPLE_SILENCE  (0.0f)
#define PRINTF_S_FORMAT "%.8f"
#elif 0
#define PA_SAMPLE_TYPE  paInt16
typedef short SAMPLE;
#define SAMPLE_SILENCE  (0)
#define PRINTF_S_FORMAT "%d"
#elif 0
#define PA_SAMPLE_TYPE  paInt8
typedef char SAMPLE;
#define SAMPLE_SILENCE  (0)
#define PRINTF_S_FORMAT "%d"
#else
#define PA_SAMPLE_TYPE  paUInt8
typedef unsigned char SAMPLE;
#define SAMPLE_SILENCE  (128)
#define PRINTF_S_FORMAT "%d"
#endif

#define NO_TX_DATA (255)
#define FREQ_MARK (1000.f)
//#define FREQ_MARK (900.f)
#define FREQ_SPACE (900.f)
#define N_SAMPLES_PER_BIT ((long)(SAMPLE_RATE * 0.040))

typedef struct
{
	unsigned            RXframeIndex;
	int                 RXthreadSyncFlag;
	SAMPLE             *RXringBufferData;
	PaUtilRingBuffer    RXringBuffer;
	unsigned            TXframeIndex;
	int                 TXthreadSyncFlag;
	SAMPLE             *TXringBufferData;
	PaUtilRingBuffer    TXringBuffer;
	FILE               *file;
	void               *threadHandle;
}
paTestData;





void TestCompleteChain(void);
void test_dsp_sample(void);
void test_cwmod_decode(void);



#endif /* TESTSCAMP_H_ */
