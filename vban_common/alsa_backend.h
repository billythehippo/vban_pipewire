#ifndef ALSA_BACKEND_H_
#define ALSA_BACKEND_H_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <alsa/asoundlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <getopt.h>
#include <pthread.h>

//#include "../vban_common/vban.h"
#include "../vban_common/vban_functions.h"
#include "../vban_common/udpsocket.h"
#include "../vban_common/ringbuffer.h"

#define CARD_NAME_LENGTH 32

typedef struct
{
    snd_pcm_t* handle;
    snd_pcm_hw_params_t* hwParams;
    snd_pcm_format_t format;
    snd_pcm_access_t access;
    snd_pcm_stream_t streamDirection;
    uint32_t nbchannels;
    uint32_t nbchannelsmin;
    uint32_t nbchannelsmax;
    uint32_t samplerate;
    uint32_t samplerateMin;
    uint32_t samplerateMax;
    snd_pcm_uframes_t nframes;
    snd_pcm_uframes_t nframesMin;
    snd_pcm_uframes_t nframesMax;
    char cardName[CARD_NAME_LENGTH];
}
alsaHandle_t;

// typedef struct
// {
//     uint32_t ip;
//     uint32_t iprx;
//     char ipaddr[16];
//     uint16_t port;
//     char pipename[64];
//     uint8_t redundancy;
//     uint32_t format_vban;
//     VBanPacket packet;
//     VBanHeader header;
//     char* rxbuf;
//     int32_t rxbuflen;
//     int pipedesc;
//     int vban_sd;
//     struct sockaddr_in vban_si;
//     struct sockaddr_in vban_si_other;
//     struct pollfd polldesc;
//     jack_ringbuffer_t* ringbuffer;
//     size_t ringbuffersize;
//     int nframes;

//     // temporary
//     alsaHandle_t* handle;
// }
// streamConfig_t;

const uint8_t availableSamplerates = 11;
const uint32_t samplerates[availableSamplerates] = {22050, 24000, 32000, 44100, 48000, 88200, 96000, 176400, 192000, 352800, 384000};
const uint8_t availableBuffersizes = 14;
const snd_pcm_uframes_t buffersizes[availableBuffersizes] = {16, 32, 64, 96, 128, 160, 192, 224, 256, 512, 1024, 2048, 4096, 8192};


inline snd_pcm_format_t vban_to_alsa_format(enum VBanBitResolution bit_resolution)
{
    switch (bit_resolution)
    {
    case VBAN_BITFMT_8_INT:
        return SND_PCM_FORMAT_S8;

    case VBAN_BITFMT_16_INT:
        return SND_PCM_FORMAT_S16;

    case VBAN_BITFMT_24_INT:
        return SND_PCM_FORMAT_S24;

    case VBAN_BITFMT_32_INT:
        return SND_PCM_FORMAT_S32;

    case VBAN_BITFMT_32_FLOAT:
        return SND_PCM_FORMAT_FLOAT;

    case VBAN_BITFMT_64_FLOAT:
        return SND_PCM_FORMAT_FLOAT64;

    default:
        return SND_PCM_FORMAT_UNKNOWN;
    }
}


extern void* rxThread(void* param);
void pbCallback(snd_async_handler_t *pcm_callback); // playback callback
extern int get_options(alsaHandle_t* handle, streamConfig_t* stream, int argc, char *argv[]);
extern int alsa2pipe(int argc, char *argv[]);
extern int pipe2alsa(int argc, char *argv[]);

#endif // ALSA_BACKEND_H_
