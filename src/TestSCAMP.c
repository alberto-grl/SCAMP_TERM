/*
 * TestSCAMP.c
 *
 *  Created on: 10 feb 2022
 *      Author: alberto
 *
 *
 *
 *
 */
 /*
 * Copyright (c) 2021 Daniel Marks

This software is provided 'as-is', without any express or implied
warranty. In no event will the authors be held liable for any damages
arising from the use of this software.

Permission is granted to anyone to use this software for any purpose,
including commercial applications, and to alter it and redistribute it
freely, subject to the following restrictions:

1. The origin of this software must not be misrepresented; you must not
   claim that you wrote the original software. If you use this software
   in a product, an acknowledgment in the product documentation would be
   appreciated but is not required.
2. Altered source versions must be plainly marked as such, and must not be
   misrepresented as being the original software.
3. This notice may not be removed or altered from any source distribution.
 */


#include <ncurses.h>

#define DSPINT_DEBUG


#ifdef DSPINT_DEBUG
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <math.h>
#include <pthread.h>
#endif

#include "portaudio.h"
#include "pa_ringbuffer.h"
#include "pa_util.h"

#include "scamp.h"
#include "dspint.h"

#include "eccfr.h"
#include "golay.h"

#include "fftw3.h"
#include "TestSCAMP.h"


WINDOW *RXWin, *TXWin, *BitErrWin;

float TXLevel;
static long TXSamplesLeft;
float ToneFrequency;
uint8_t TXBit;

uint8_t TXString[255];
OutData_t TXMessage;

float *in;
float *in2;
int nc;
fftwf_complex *out;
fftwf_plan plan_backward;
fftwf_plan plan_forward;

static ring_buffer_size_t rbs_min(ring_buffer_size_t a, ring_buffer_size_t b)
{
    return (a < b) ? a : b;
}


static unsigned NextPowerOf2(unsigned val)
{
    val--;
    val = (val >> 1) | val;
    val = (val >> 2) | val;
    val = (val >> 4) | val;
    val = (val >> 8) | val;
    val = (val >> 16) | val;
    return ++val;
}

/*thread function definition*/
void* RXThread(void* ptr)
{
    paTestData* pData = (paTestData*)ptr;
    static float SumSampleValue;
    while(1)
    {
        ring_buffer_size_t elementsInBuffer = PaUtil_GetRingBufferReadAvailable(&pData->RXringBuffer);
        if ( (elementsInBuffer >= pData->RXringBuffer.bufferSize / NUM_WRITES_PER_BUFFER))
        {
            void* ptr[2] = {0};
            ring_buffer_size_t sizes[2] = {0};

            /* By using PaUtil_GetRingBufferReadRegions, we can read directly from the ring buffer */
            ring_buffer_size_t elementsRead = PaUtil_GetRingBufferReadRegions(&pData->RXringBuffer, elementsInBuffer, ptr + 0, sizes + 0, ptr + 1, sizes + 1);
            if (elementsRead > 0)
            {
                int i, j, k;
                for (i = 0; i < 2 && ptr[i] != NULL; ++i)
                {
                    for (j = 0; j < sizes[i]; j+=2)  //only left
                    {
                        SumSampleValue +=  ( * (float *)((uint8_t *)ptr[i] +  pData->RXringBuffer.elementSizeBytes * j));

                        if (k++ == N_OVERSAMPLING - 1)
                        {
                            DecodeSample(SumSampleValue / N_OVERSAMPLING);
                            //			printf("%f\n", SumSampleValue);
                            k = 0;
                            SumSampleValue = 0;
                        }
                    }
                }
                //	                    fwrite(ptr[i], pData->ringBuffer.elementSizeBytes, sizes[i], pData->file);
            }
            PaUtil_AdvanceRingBufferReadIndex(&pData->RXringBuffer, elementsRead);
        }
        Pa_Sleep(10);
    }
}

uint8_t GetTXBit(OutData_t* TXMessage, uint32_t n)
{
    uint8_t i;
    uint32_t Frame;
    Frame = TXMessage->OutCodes[n / 30];

    if (Frame & (1 << (29 -(n % 30))))
        return 1;
    else
        return 0;

}


/*thread function definition*/
void* TXThread(void* ptr)
{
    volatile static double AudioPhase;
    paTestData* pData = (paTestData*)ptr;
    ring_buffer_size_t elementsInBuffer;
    static uint32_t i, j, k, TXBitN;

    uint8_t s[255];
    //	OutData_t TXMessage;
    printf("enter string: ");
    //    My_gets(s);

    //	strncpy((char*) s, "qz3456789abcdefghijklmnopqr\0", 29);
    //strncpy((char*) s, "1", 1);
    //	PrepareBits(s, & TXMessage);

    /*	TXMessage.OutLength = 4;
    TXMessage.OutCodes[0]=0x1;
    TXMessage.OutCodes[1]=0x1;
    TXMessage.OutCodes[2]=0x1;
    TXMessage.OutCodes[3]=0x1;
     */

    i = j = k = TXBitN = 0;

    Pa_Sleep(2000);

    while(1)
    {

        if ((strlen(TXString) > 0) && (TXMessage.OutLength == 0))
        {
            //	PrepareBits(TXString, & TXMessage);
        }

        elementsInBuffer = PaUtil_GetRingBufferWriteAvailable(&pData->TXringBuffer);
        if (elementsInBuffer >= pData->TXringBuffer.bufferSize / NUM_WRITES_PER_BUFFER)
        {

            void* ptr[2] = {0};
            ring_buffer_size_t sizes[2] = {0};

            /* By using PaUtil_GetRingBufferWriteRegions, we can write directly into the ring buffer */
            PaUtil_GetRingBufferWriteRegions(&pData->TXringBuffer, elementsInBuffer, ptr + 0, sizes + 0, ptr + 1, sizes + 1);


            ring_buffer_size_t itemsReadFromFile = 0;
            //			int i, j;
            for (i = 0; i < 2 && ptr[i] != NULL; ++i)
            {
                for (j=0; j < (sizes[i] ); j+=2) //only left
                {
                    if (TXSamplesLeft == 0)
                    {
                        TXBit = GetTXBit(& TXMessage, TXBitN);
                        if (TXBit == 1)
                        {
                            ToneFrequency = FREQ_MARK;
                            TXLevel = 1;
                        }
                        if (TXBit == 0)
                        {
                            ToneFrequency = FREQ_SPACE;
                            TXLevel = 1.0;
                        }
                        TXSamplesLeft = N_SAMPLES_PER_BIT;
                        //						printf("%d \n", TXBit);
                        //TXLevel = 0.2; //TODO: ramping

                        if (TXBitN < TXMessage.OutLength * 30) //N. bits in SCAMP frame
                        {
                            TXBitN++;
                            TXSamplesLeft = N_SAMPLES_PER_BIT;
                        }
                        else
                        {
                            TXMessage.OutLength = 0; //TX finished
                            TXBitN = 0;
                            TXSamplesLeft = N_SAMPLES_PER_BIT;
                            TXLevel = 0.0;
                        }
                    }

                    AudioPhase = 2.f * M_PI * TXSamplesLeft * ToneFrequency / SAMPLE_RATE;
                    *(float *) ((uint8_t *)ptr[i] + pData->TXringBuffer.elementSizeBytes * j) = TXLevel * sin(AudioPhase);
                    *(float *) ((uint8_t *)ptr[i] + pData->TXringBuffer.elementSizeBytes * (j + 1)) = 0;
                    if(TXSamplesLeft > 0)
                        TXSamplesLeft--;

                }
                itemsReadFromFile += sizes[i];
            }
            PaUtil_AdvanceRingBufferWriteIndex(&pData->TXringBuffer, itemsReadFromFile);
        }
        Pa_Sleep(10);
    }
}

/* This routine will be called by the PortAudio engine when audio is needed.
 ** It may be called at interrupt level on some machines so don't do anything
 ** that could mess up the system like calling malloc() or free().
 */
static int AudioCallback( void *inputBuffer, void *outputBuffer,
                          unsigned long framesPerBuffer,
                          const PaStreamCallbackTimeInfo* timeInfo,
                          PaStreamCallbackFlags statusFlags,
                          void *userData )
{
    paTestData *data = (paTestData*)userData;
    ring_buffer_size_t elementsWriteable = PaUtil_GetRingBufferWriteAvailable(&data->RXringBuffer);
    ring_buffer_size_t elementsToWrite = rbs_min(elementsWriteable, (ring_buffer_size_t)(framesPerBuffer * NUM_CHANNELS));
    SAMPLE *rptr = (SAMPLE*)inputBuffer;

    (void) outputBuffer; /* Prevent unused variable warnings. */
    (void) timeInfo;
    (void) statusFlags;
    (void) userData;

    data->RXframeIndex += PaUtil_WriteRingBuffer(&data->RXringBuffer, rptr, elementsToWrite);
    ring_buffer_size_t elementsToPlay = PaUtil_GetRingBufferReadAvailable(&data->TXringBuffer);
    ring_buffer_size_t elementsToRead = rbs_min(elementsToPlay, (ring_buffer_size_t)(framesPerBuffer * NUM_CHANNELS));
    SAMPLE* wptr =  (SAMPLE*)outputBuffer;

    data->TXframeIndex += PaUtil_ReadRingBuffer(&data->TXringBuffer, wptr, elementsToRead);
    return paContinue;

}



void TestAudioRX(void)
{
    PaStreamParameters  inputParameters,
                        outputParameters;
    PaStream*           stream;
    PaError             err = paNoError;
    paTestData          data = {0};
    unsigned            numSamples;
    unsigned            numBytes;

    pthread_t RXid, TXid;
    int ret;

    ret=pthread_create(&RXid,NULL,&RXThread,&data);
    if(ret==0)
    {
        printf("RXThread  created successfully.\n");
    }
    else
    {
        printf("RXThread  not created.\n");
        //		return ; /*return from main*/
    }

    ret=pthread_create(&TXid,NULL,&TXThread,&data);
    if(ret==0)
    {
        printf("TXThread  created successfully.\n");
    }
    else
    {
        printf("TXThread  not created.\n");
        return ; /*return from main*/
    }

    dsp_initialize_scamp(MOD_TYPE);

    printf("patest_record.c\n");
    fflush(stdout);

    /* We set the ring buffer size to about 500 ms */
    numSamples = NextPowerOf2((unsigned)(SAMPLE_RATE * 1.0 * NUM_CHANNELS));
    numBytes = numSamples * sizeof(SAMPLE);
    data.RXringBufferData = (SAMPLE *) malloc( numBytes );
    if( data.RXringBufferData == NULL )
    {
        printf("Could not allocate ring buffer data.\n");
        goto done;
    }

    data.TXringBufferData = (SAMPLE *) malloc( numBytes );
    if( data.TXringBufferData == NULL )
    {
        printf("Could not allocate ring buffer data.\n");
        goto done;
    }

    if (PaUtil_InitializeRingBuffer(&data.RXringBuffer, sizeof(SAMPLE), numSamples, data.RXringBufferData) < 0)
    {
        printf("Failed to initialize ring buffer. Size is not power of 2 ??\n");
        goto done;
    }
    if (PaUtil_InitializeRingBuffer(&data.TXringBuffer, sizeof(SAMPLE), numSamples, data.TXringBufferData) < 0)
    {
        printf("Failed to initialize ring buffer. Size is not power of 2 ??\n");
        goto done;
    }

    err = Pa_Initialize();
    if( err != paNoError ) goto done;

    inputParameters.device = Pa_GetDefaultInputDevice(); /* default input device */
    if (inputParameters.device == paNoDevice)
    {
        fprintf(stderr,"Error: No default input device.\n");
        goto done;
    }
    inputParameters.channelCount = 2;                    /* stereo input */
    inputParameters.sampleFormat = PA_SAMPLE_TYPE;
    inputParameters.suggestedLatency = Pa_GetDeviceInfo( inputParameters.device )->defaultLowInputLatency;
    inputParameters.hostApiSpecificStreamInfo = NULL;

    outputParameters.device = Pa_GetDefaultOutputDevice(); /* default output device */
    if (outputParameters.device == paNoDevice)
    {
        fprintf(stderr,"Error: No default input device.\n");
        goto done;
    }
    outputParameters.channelCount = 2;                    /* stereo output */
    outputParameters.sampleFormat = PA_SAMPLE_TYPE;
    outputParameters.suggestedLatency = Pa_GetDeviceInfo( outputParameters.device )->defaultLowOutputLatency;
    outputParameters.hostApiSpecificStreamInfo = NULL;

    nc = ( FRAMES_PER_BUFFER / 2 ) + 1;

    out = fftwf_malloc ( sizeof ( fftwf_complex ) * nc );

    err = Pa_OpenStream(
              &stream,
              &inputParameters,
              &outputParameters,                  /* &outputParameters, */
              SAMPLE_RATE,
              FRAMES_PER_BUFFER,
              paClipOff,      /* we won't output out of range samples so don't bother clipping them */
              AudioCallback,
              &data );
    if( err != paNoError ) goto done;

    err = Pa_StartStream( stream );
    if( err != paNoError ) goto done;
    printf("\n=== Now receiving ===\n");
    fflush(stdout);

    initscr();			/* Start curses mode 		*/
    raw();			/* Line buffering disabled, Pass on everty thing to me 		*/
    noecho();
    RXWin = newwin(6,80,0,0);     //(height, width, starty, startx);
    BitErrWin = newwin(6,80,7,0);     //(height, width, starty, startx);
    TXWin = newwin(5,80,15,0);     //(height, width, starty, startx);
    wtimeout(TXWin, 100);
    scrollok(RXWin,TRUE);
    scrollok(TXWin,TRUE);
    scrollok(BitErrWin,TRUE);
    mvprintw(6,0,"_______________________________________________________________________________________________________");
    mvprintw(14,0,"_______________________________________________________________________________________________________");
    refresh();

    while (1)
    {
        if (TXMessage.OutLength==0)
        {
//			printf("------------------------------------------Input String-------------------------\n");
            //	My_gets(TXString);
            char c = wgetch(TXWin);
            if (c > 0)
            {
                wprintw(TXWin, "%c", c);
                strcat(TXString, (char[2]){(char) c, '\0'});
            }
            wrefresh(BitErrWin);
            wrefresh(TXWin);
//strcpy(TXString, "abcd");
            if (c== '\n')
            {
                PrepareBits(TXString, & TXMessage);
                strcpy(TXString, "\0");
            }
        }

        Pa_Sleep(10);
    }
done:
    Pa_Terminate();

    if( err != paNoError )
    {
        fprintf( stderr, "An error occurred while using the portaudio stream \n" );
        fprintf( stderr, "Error number: %d\n", err );
        fprintf( stderr, "Error message: %s\n", Pa_GetErrorText( err ) );
        err = 1;          /* Always return 0 or 1, but no other return codes. */
    }
}

int main(void)
{


    TestAudioRX();
    //TestCompleteChain();
    //  test_dsp_sample();
    //test_cwmod_decode();
    // printf("size=%d\n",sizeof(ds)+sizeof(df)+sizeof(ps));
}
