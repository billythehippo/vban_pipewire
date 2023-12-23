#ifndef VBAN_FUNCTIONS_H
#define VBAN_FUNCTIONS_H

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <poll.h>
#include <arpa/inet.h>
#include <errno.h>
#include <signal.h>
#include <getopt.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <pthread.h>

#include "vban.h"
#include "ringbuffer.h"

#define CARD_NAME_LENGTH 32
//#ifdef __linux__
//#endif

typedef struct
{
    uint32_t samplerate;
    uint16_t nframes;
    uint16_t nbchannels;
    uint16_t hwnchannels;
    uint8_t redundancy;
    uint8_t format; //enum spa_audio_format
    uint8_t format_vban;
    char ipaddr[16];
    uint32_t ip;
    uint32_t iprx;
    uint16_t port;
    char pipename[64];
    VBanPacket packet; // here is also format and nbchannels
    VBanHeader header;
    uint16_t packetdatalen;
    uint16_t packetnum;
    int pipedesc;
    int vban_sd;
    struct sockaddr_in vban_si;
    struct sockaddr_in vban_si_other;
    struct pollfd polldesc;
    jack_ringbuffer_t* ringbuffer;
    size_t ringbuffersize;
    char* buf;
    uint32_t buflen;
    uint8_t flags;
    uint8_t lostpaccnt;
    uint8_t apiindex;
    char cardname[32];
    void* additionals;
} streamConfig_t;
// flags defines
#define CAPTURE         0x01
#define PLAYBACK        0x02
#define DUPLEX          0x03
#define RESAMPLER       0x04 // 1 - Available, 0 - No
#define CONNECTED       0x08
#define RECEIVING       0x10

typedef struct
{
    pthread_t tid;
    pthread_attr_t attr;
    pthread_mutex_t threadlock;// = PTHREAD_MUTEX_INITIALIZER;
    pthread_cond_t  dataready;// = PTHREAD_COND_INITIALIZER;
} mutexcond;


inline uint16_t int16betole(u_int16_t input)
{
    return ((((uint8_t*)&input)[0])<<8) + ((uint8_t*)&input)[1];
}


inline uint32_t int32betole(uint32_t input)
{
    return (((((((uint8_t*)&input)[0]<<8)+((uint8_t*)&input)[1])<<8)+((uint8_t*)&input)[2])<<8)+((uint8_t*)&input)[3];
}


inline void inc_nuFrame(VBanHeader* header)
{
    header->nuFrame++;
}


inline void convertSampleTX(uint8_t* ptr, float sample, uint8_t bit_fmt)
{
    int32_t tmp = 0;
    uint32_t* tp;

    switch (bit_fmt)
    {
    case 0: //VBAN_BITFMT_8_INT:
        tmp = (int8_t)roundf((float)(1<<7)*sample); //((float)(1<<7)*sample);//
        ptr[0] = tmp&0xFF;
        break;
    case 1: //VBAN_BITFMT_16_INT:
        tmp = (int16_t)roundf((float)(1<<15)*sample); //((float)(1<<15)*sample);//
        ptr[0] = tmp&0xFF;
        ptr[1] = (tmp>>8)&0xFF;
        break;
    case 2: //VBAN_BITFMT_24_INT:
        tmp = (int32_t)roundf((float)(1<<23)*sample); //((float)(1<<23)*sample);//
        ptr[0] = tmp&0xFF;
        ptr[1] = (tmp>>8)&0xFF;
        ptr[2] = (tmp>>16)&0xFF;
        break;
    case 3: //VBAN_BITFMT_32_INT:
        ptr[0] = tmp&0xFF;
        ptr[1] = (tmp>>8)&0xFF;
        ptr[2] = (tmp>>16)&0xFF;
        ptr[3] = (tmp>>24)&0xFF;
        break;
    case 4: //VBAN_BITFMT_32_FLOAT:
        tp = (uint32_t*)(&sample);
        tmp = *tp;
        ptr[0] = tmp&0xFF;
        ptr[1] = (tmp>>8)&0xFF;
        ptr[2] = (tmp>>16)&0xFF;
        ptr[3] = (tmp>>24)&0xFF;
        break;
    case 5: //VBAN_BITFMT_64_FLOAT:
    default:
        break;
    }
}


inline float convertSampleRX(uint8_t* ptr, uint8_t bit_fmt)
{
    int value = 0;

    switch (bit_fmt)
    {
        case 0: //VBAN_BITFMT_8_INT:
        return (float)(*((int8_t const*)ptr)) / (float)(1 << 7);

        case 1: //VBAN_BITFMT_16_INT:
        return (float)(*((int16_t const*)ptr)) / (float)(1 << 15);

        case 2: //VBAN_BITFMT_24_INT:
        value = (((int8_t)ptr[2])<<16) + (ptr[1]<<8) + ptr[0];
        return (float)(value) / (float)(1 << 23);

        case 3: //VBAN_BITFMT_32_INT:
        return (float)*((int32_t const*)ptr) / (float)(1 << 31);

        case 4: //VBAN_BITFMT_32_FLOAT:
        return *(float const*)ptr;

        //case 5: //VBAN_BITFMT_64_FLOAT:
        default:
        fprintf(stderr, "Convert Error! %d\n", bit_fmt);
        return 0.0;
    }
}


inline int getSampleRateIndex(long host_samplerate)
{
    int index = -1;
    uint8_t i;
    for (i=0; i<VBAN_SR_MAXNUMBER; i++) if (host_samplerate==VBanSRList[i]) index = i;
    return index;
}


inline uint stripPacket(uint8_t resolution, uint16_t nchannels)
{
    uint framesize = VBanBitResolutionSize[resolution]*nchannels;
    uint nframes = VBAN_DATA_MAX_SIZE/framesize;
    if (nframes>VBAN_SAMPLES_MAX_NB) nframes = VBAN_SAMPLES_MAX_NB;
    return nframes*framesize;
}


inline uint stripData(uint datasize, uint8_t resolution, uint16_t nchannels)
{
    uint framesize = VBanBitResolutionSize[resolution]*nchannels;
    uint nframes = datasize/framesize;
    if (nframes>VBAN_SAMPLES_MAX_NB) nframes = VBAN_SAMPLES_MAX_NB;
    return nframes*framesize;
}

inline uint calcNFrames(uint datasize, uint8_t resolution, uint16_t nchannels)
{
    uint framesize = VBanBitResolutionSize[resolution]*nchannels;
    uint nframes = datasize/framesize;
    return nframes;
}


inline uint pktToFloatBuf(uint pktlen, uint8_t resolution)
{
    return (pktlen/VBanBitResolutionSize[resolution])*4;
}


void compute_packets_and_bufs(streamConfig_t* config);
int get_options(streamConfig_t* stream, int argc, char *argv[]);

#endif
