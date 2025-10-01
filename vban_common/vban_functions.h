#ifndef VBAN_FUNCTIONS_H
#define VBAN_FUNCTIONS_H

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <poll.h>
#include <arpa/inet.h>
#include <errno.h>
//#include <signal.h>
#include <getopt.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <pthread.h>
#include <unistd.h>

#include "vban.h"
#include "vban_client_list.h"
#include "ringbuffer.h"
#include "zita-resampler/vresampler.h"
#include "udp.h"
#include "popen2.h"

// TYPE DEFS

#define CMDLEN_MAX 600

typedef struct
{
    pthread_t tid;
    pthread_attr_t attr;
    pthread_mutex_t threadlock;// = PTHREAD_MUTEX_INITIALIZER;
    pthread_cond_t  dataready;// = PTHREAD_COND_INITIALIZER;
} mutexcond_t;

typedef struct
{
#ifdef __linux__
    timer_t t;
    int tfd;
    struct itimerspec ts;
#endif
    struct pollfd tds[1];
    uint64_t tval;
    int tlen;
    mutexcond_t timmutex;
} timer_simple_t;

// typedef struct
// {
//     udpc_t* udpsocket;
//     mutexcond_t* mutexcond;
//     ringbuffer_t* ringbuffer;
// } rxcontext_t;

// CONTEXT VBAN

typedef struct vban_stream_context_t {
    char rx_streamname[VBAN_STREAM_NAME_SIZE];
    char tx_streamname[VBAN_STREAM_NAME_SIZE];
    char servername[VBAN_STREAM_NAME_SIZE * 2];
    uint16_t nbinputs = 0;
    uint16_t nboutputs = 0;
    uint16_t nframes;
    uint8_t nperiods;
    int samplerate;
    int samplerate_resampler;
    char* portbuf;
    int portbuflen;
    char* txbuf;
    int txbuflen;
    float* rxbuf;
    int rxbuflen;
    ringbuffer_t* ringbuffer;
    ringbuffer_t* ringbuffer_midi;
    char iptxaddr[16];
    union
    {
        uint32_t iptx;
        uint8_t iptx_bytes[4];
    };
    uint16_t txport;
    union
    {
        uint32_t iprx;
        uint8_t iprx_bytes[4];
    };
    union
    {
        uint32_t iplocal;
        uint8_t iplocal_bytes[4];
    };
    uint16_t rxport;
    uint16_t ansport;
    char pipename[32];
    int pipedesc;
    pollfd pd[1];
    uint32_t nu_frame;
    uint8_t vban_input_format;
    uint8_t vban_output_format;
    uint8_t redundancy;
    int vban_nframes_pac;
    uint16_t pacnum;
    uint16_t pacdatalen;
    uint8_t lost_pac_cnt;
    udpc_t* rxsock;
    udpc_t* txsock;
    mutexcond_t rxmutex;
    mutexcond_t cmdmutex;
    uint16_t flags;
    char* command;
    char* message;
    VBanPacket txpacket;
    VBanPacket txmidipac;
    VBanPacket info;
    char* jack_server_name;
    VResampler* resampler;
    float* resampler_inbuf;
    int resampler_inbuflen;
//    int resampler_infrag;
    float* resampler_outbuf;
    int resampler_outbuflen;
} vban_stream_context_t;
//FLAGS COMMON
// flags defines
#define RECEIVER        0x0001
#define TRANSMITTER     0x0002
#define RECEIVING       0x0004
#define SENDING         0x0008
#define CMD_PRESENT     0x0010
#define CONNECTED       0x0020
#define PLUCKING_EN     0x0040
#define PLUCKING_ON     0x0080
#define MULTISTREAM     0x0100
#define DEVICE_MODE     0x0200
#define AUTOCONNECT     0x0400
#define RESAMPLER       0x0800
#define MSG_PRESENT     0x1000
#define HALT            0x8000

typedef struct vban_multistream_context_t
{
    timer_simple_t offtimer;
    uint8_t vban_clients_min = 0;
    uint8_t vban_clients_max = 0;
    uint8_t active_clients_num = 0;
    uint8_t active_clients_ind = 0;
    client_id_t* clients;
    client_id_t* client;
    uint16_t* flags;
} vban_multistream_context_t;

#define CARD_NAME_LENGTH 32
//#ifdef __linux__
//#endif

#define CMD_SIZE 300

void vban_fill_receptor_info(vban_stream_context_t* context);
void tune_tx_packets(vban_stream_context_t* stream);
void* timerThread(void* arg);
void* rxThread(void* arg);


inline uint16_t int16betole(u_int16_t input)
{
    return ((((uint8_t*)&input)[0])<<8) + ((uint8_t*)&input)[1];
}


inline uint32_t int32betole(uint32_t input)
{
    return (((((((uint8_t*)&input)[0]<<8)+((uint8_t*)&input)[1])<<8)+((uint8_t*)&input)[2])<<8)+((uint8_t*)&input)[3];
}


inline void vban_inc_nuFrame(VBanHeader* header)
{
    header->nuFrame++;
}


inline int vban_sample_convert(void* dstptr, uint8_t format_bit_dst, void* srcptr, uint8_t format_bit_src, int num)
{
    int ret = 0;
    uint8_t* dptr;
    uint8_t* sptr;
    int32_t tmp;

    int dst_sample_size;
    int src_sample_size;

    if (format_bit_dst==format_bit_src)
    {
        memcpy(dstptr, srcptr, VBanBitResolutionSize[format_bit_dst]*num);
        return 0;
    }

    dptr = (uint8_t*)dstptr;
    sptr = (uint8_t*)srcptr;

    dst_sample_size = VBanBitResolutionSize[format_bit_dst];
    src_sample_size = VBanBitResolutionSize[format_bit_src];

    switch (format_bit_dst)
    {
    case VBAN_BITFMT_8_INT:
        switch (format_bit_src)
        {
        case VBAN_BITFMT_16_INT:
            for (int sample=0; sample<num; sample++)
            {
                dptr[sample*dst_sample_size] = sptr[1 + sample*src_sample_size];
            }
            break;
        case VBAN_BITFMT_24_INT:
            for (int sample=0; sample<num; sample++)
            {
                dptr[sample*dst_sample_size] = sptr[2 + sample*src_sample_size];
            }
            break;
        case VBAN_BITFMT_32_INT:
            for (int sample=0; sample<num; sample++)
            {
                dptr[sample*dst_sample_size] = sptr[3 + sample*src_sample_size];
            }
            break;
        case VBAN_BITFMT_32_FLOAT:
            for (int sample=0; sample<num; sample++)
            {
                dptr[sample*dst_sample_size] = (int8_t)roundf((float)(1<<7)*((float*)sptr)[sample]);
            }
            break;
        default:
            fprintf(stderr, "Convert Error! Unsuppotred source format!%d\n", format_bit_src);
            ret = 1;
        }
        break;
    case VBAN_BITFMT_16_INT:
        switch (format_bit_src)
        {
        case VBAN_BITFMT_8_INT:
            for (int sample=0; sample<num; sample++)
            {
                dptr[0 + sample*dst_sample_size] = 0;
                dptr[1 + sample*dst_sample_size] = sptr[0 + sample*src_sample_size];
            }
            break;
        case VBAN_BITFMT_24_INT:
            for (int sample=0; sample<num; sample++)
            {
                dptr[0 + sample*dst_sample_size] = sptr[1 + sample*src_sample_size];
                dptr[1 + sample*dst_sample_size] = sptr[2 + sample*src_sample_size];
            }
            break;
        case VBAN_BITFMT_32_INT:
            for (int sample=0; sample<num; sample++)
            {
                dptr[0 + sample*dst_sample_size] = sptr[2 + sample*src_sample_size];
                dptr[1 + sample*dst_sample_size] = sptr[3 + sample*src_sample_size];
            }
            break;
        case VBAN_BITFMT_32_FLOAT:
            for (int sample=0; sample<num; sample++)
            {
                //((int16_t*)dptr)[sample*dst_sample_size] = (int16_t)roundf((float)(1<<15)*((float*)sptr)[sample]);
                //((int16_t*)dptr)[sample] = (int16_t)roundf((float)(1<<15)*((float*)sptr)[sample]);
                tmp = (int32_t)roundf((float)(1<<31)*((float*)sptr)[sample]);
                memcpy(&dptr[sample*dst_sample_size], (uint8_t*)&tmp + 2, dst_sample_size);
            }
            break;
        default:
            fprintf(stderr, "Convert Error! Unsuppotred source format!%d\n", format_bit_src);
            ret = 1;
        }
        break;
    case VBAN_BITFMT_24_INT:
        switch (format_bit_src)
        {
        case VBAN_BITFMT_8_INT:
            for (int sample=0; sample<num; sample++)
            {
                dptr[0 + sample*dst_sample_size] = 0;
                dptr[1 + sample*dst_sample_size] = 0;
                dptr[2 + sample*dst_sample_size] = sptr[0 + sample*src_sample_size];
            }
            break;
        case VBAN_BITFMT_16_INT:
            for (int sample=0; sample<num; sample++)
            {
                dptr[0 + sample*dst_sample_size] = 0;
                dptr[1 + sample*dst_sample_size] = sptr[0 + sample*src_sample_size];
                dptr[2 + sample*dst_sample_size] = sptr[1 + sample*src_sample_size];
            }
            break;
        case VBAN_BITFMT_32_INT:
            for (int sample=0; sample<num; sample++)
            {
                dptr[0 + sample*dst_sample_size] = sptr[1 + sample*src_sample_size];
                dptr[1 + sample*dst_sample_size] = sptr[2 + sample*src_sample_size];
                dptr[2 + sample*dst_sample_size] = sptr[3 + sample*src_sample_size];
            }
            break;
        case VBAN_BITFMT_32_FLOAT:
            for (int sample=0; sample<num; sample++)
            {
                //((int32_t*)dptr)[sample*dst_sample_size] = (int32_t)roundf((float)(1<<23)*((float*)sptr)[sample]);
                tmp = (int32_t)roundf((float)(1<<31)*((float*)sptr)[sample]);
                memcpy(&dptr[sample*dst_sample_size], (uint8_t*)&tmp + 1, dst_sample_size);
            }
            break;
        default:
            fprintf(stderr, "Convert Error! Unsuppotred source format!%d\n", format_bit_src);
            ret = 1;
        }
        break;
    case VBAN_BITFMT_32_INT:
        switch (format_bit_src)
        {
        case VBAN_BITFMT_8_INT:
            for (int sample=0; sample<num; sample++)
            {
                dptr[0 + sample*dst_sample_size] = 0;
                dptr[1 + sample*dst_sample_size] = 0;
                dptr[2 + sample*dst_sample_size] = 0;
                dptr[3 + sample*dst_sample_size] = sptr[0 + sample*src_sample_size];
            }
            break;
        case VBAN_BITFMT_16_INT:
            for (int sample=0; sample<num; sample++)
            {
                dptr[0 + sample*dst_sample_size] = 0;
                dptr[1 + sample*dst_sample_size] = 0;
                dptr[2 + sample*dst_sample_size] = sptr[0 + sample*src_sample_size];
                dptr[3 + sample*dst_sample_size] = sptr[1 + sample*src_sample_size];
            }
            break;
        case VBAN_BITFMT_24_INT:
            for (int sample=0; sample<num; sample++)
            {
                dptr[0 + sample*dst_sample_size] = 0;
                dptr[1 + sample*dst_sample_size] = sptr[0 + sample*src_sample_size];
                dptr[2 + sample*dst_sample_size] = sptr[1 + sample*src_sample_size];
                dptr[3 + sample*dst_sample_size] = sptr[2 + sample*src_sample_size];
            }
            break;
        case VBAN_BITFMT_32_FLOAT:
            for (int sample=0; sample<num; sample++)
            {
                ((int32_t*)dptr)[sample] = (int32_t)roundf((float)(1<<31)*((float*)sptr)[sample]);
            }
            break;
        default:
            fprintf(stderr, "Convert Error! Unsuppotred source format!%d\n", format_bit_src);
            ret = 1;
        }
        break;
    case VBAN_BITFMT_32_FLOAT:
        switch (format_bit_src)
        {
        case VBAN_BITFMT_8_INT:
            for (int sample=0; sample<num; sample++)
            {
                ((float*)dptr)[sample] = (float)sptr[sample]/(float)(1<<7);
            }
            break;
        case VBAN_BITFMT_16_INT:
            for (int sample=0; sample<num; sample++)
            {
                ((float*)dptr)[sample] = (float)(((int16_t*)sptr)[sample])/(float)(1<<15);
            }
            break;
        case VBAN_BITFMT_24_INT:
            for (int sample=0; sample<num; sample++)
            {
                ((float*)dptr)[sample] = (float)((((int8_t)sptr[2 + sample*src_sample_size])<<16)+(sptr[1 + sample*src_sample_size]<<8)+sptr[0 + sample*src_sample_size])/(float)(1<<23);
            }
            break;
        case VBAN_BITFMT_32_INT:
            for (int sample=0; sample<num; sample++)
            {
                ((float*)dptr)[sample] = (float)(((int32_t*)sptr)[sample])/(float)(1<<31);
            }
            break;
        default:
            fprintf(stderr, "Convert Error! Unsuppotred source format!%d\n", format_bit_src);
            ret = 1;
        }
        break;
    default:
        fprintf(stderr, "Convert Error! Unsuppotred destination format!%d\n", format_bit_dst);
        ret = 1;
    }
    return ret;
}


inline int vban_get_format_SR(long host_samplerate)
{
    int i;
    for (i=0; i<VBAN_SR_MAXNUMBER; i++) if (host_samplerate==VBanSRList[i]) return i;
    return -1;
}


inline uint vban_strip_vban_packet(uint8_t format_bit, uint16_t nbchannels)
{
    uint framesize = VBanBitResolutionSize[format_bit]*nbchannels;
    uint nframes = VBAN_DATA_MAX_SIZE/framesize;
    if (nframes>VBAN_SAMPLES_MAX_NB) nframes = VBAN_SAMPLES_MAX_NB;
    return nframes*framesize;
}


inline uint vban_strip_vban_data(uint datasize, uint8_t format_bit, uint16_t nbchannels)
{
    uint framesize = VBanBitResolutionSize[format_bit]*nbchannels;
    uint nframes = datasize/framesize;
    if (nframes>VBAN_SAMPLES_MAX_NB) nframes = VBAN_SAMPLES_MAX_NB;
    return nframes*framesize;
}


inline uint vban_calc_nbs(uint datasize, uint8_t resolution, uint16_t nbchannels)
{
    return datasize/VBanBitResolutionSize[resolution]*nbchannels;
}


inline uint vban_packet_to_float_buffer(uint pktdatalen, uint8_t resolution)
{
    return sizeof(float)*pktdatalen/VBanBitResolutionSize[resolution];
}


inline int file_exists(const char* __restrict filename)
{
    if (access(filename, F_OK)==0) return 1;
    return 0;
}


inline void vban_free_rx_ringbuffer(ringbuffer_t* ringbuffer)
{
    if (ringbuffer!= NULL) ringbuffer_free(ringbuffer);
}


inline void vban_compute_rx_ringbuffer(int nframes, int nframes_pac, int nbchannels, int redundancy, ringbuffer_t** ringbuffer)
{
    char* zeros;
    char div = 1;
    nframes = (nframes>nframes_pac ? nframes : nframes_pac);
    *ringbuffer = ringbuffer_create(2 * nframes * nbchannels * (redundancy + 1) * sizeof(float));
    memset((*ringbuffer)->buf, 0, (*ringbuffer)->size);
    if (redundancy>0) // TODO : REWORK THIS!!!
    {
        if (redundancy<2) div = 2;
        zeros = (char*)calloc(1, (*ringbuffer)->size>>div);
        ringbuffer_write(*ringbuffer, zeros, (*ringbuffer)->size>>div);
        free(zeros);
    }
}


inline void vban_free_line_buffer(void* buffer, int* bufsize)
{
    if (buffer!= NULL) free(buffer);
    bufsize = 0;
}


inline void vban_compute_rx_buffer(int nframes, int nbchannels, float** rxbuffer, int* rxbuflen)
{
    vban_free_line_buffer(*rxbuffer, rxbuflen);
    *rxbuflen = nbchannels*nframes;
    *rxbuffer = (float*)malloc(*rxbuflen*sizeof(float));
}


inline int vban_compute_line_buffer(char* buffer, int nframes, int nbchannels, int bitres)
{
    int size = nframes*nbchannels*bitres;
    vban_free_line_buffer(buffer, NULL);
    buffer = (char*)malloc(size);
    return size;
}


inline uint8_t vban_compute_tx_packets(uint16_t* pacdatalen, uint16_t* pacnum, int nframes, int nbchannels, int bitres)
{
    *pacdatalen = nframes*nbchannels*bitres;
    *pacnum = 1;
    while((*pacdatalen>VBAN_DATA_MAX_SIZE)||((*pacdatalen/(bitres*nbchannels))>256))
    {
        *pacdatalen = *pacdatalen>>1;
        *pacnum = *pacnum<<1;
    }
    return *pacdatalen/(bitres*nbchannels) - 1;
}


inline int vban_read_frame_from_ringbuffer(float* dst, ringbuffer_t* ringbuffer, int num)
{
    size_t size = num*sizeof(float);
    if (ringbuffer_read_space(ringbuffer)>=size)
    {
        ringbuffer_read(ringbuffer, (char*)dst, size);
        return 0;
    }
    return 1;
}


inline int vban_add_frame_from_ringbuffer(float* dst, ringbuffer_t* ringbuffer, int num)
{
    float fsamples[256];
    size_t size = num*sizeof(float);
    if (ringbuffer_read_space(ringbuffer)>=size)
    {
        ringbuffer_read(ringbuffer, (char*)fsamples, size);
        for (int i=0; i<num; i++) dst[i] = (dst[i] + fsamples[i])/2;
        return 0;
    }
    return 1;
}


inline int vban_send_txbuffer(vban_stream_context_t* context, in_addr_t txip = 0, uint8_t attempts = 2)
{
    int ret = 0;
    for (uint16_t pac=0; pac<context->pacnum; pac++)
    {
        memcpy(context->txpacket.data, context->txbuf + pac*context->pacdatalen, context->pacdatalen);

        for (uint8_t red=0; red<=context->redundancy+1; red++)
            if (context->txport!= 0)
            {
                udp_send(context->txsock, context->txport, (char*)&context->txpacket, VBAN_HEADER_SIZE+context->pacdatalen, txip);

                int icnt = 0;
                usleep(20);
                while((check_icmp_status(context->txsock->fd)==111)&&(icnt<2))
                {
                    udp_send(context->txsock, context->txport, (char*)&context->txpacket, VBAN_HEADER_SIZE+context->pacdatalen, txip);
                    usleep(20);
                    icnt++;
                }
                if (icnt==attempts) ret = 1; // ICMP PORT UNREACHABLE
            }
            else write(context->pipedesc, (uint8_t*)&context->txpacket, VBAN_HEADER_SIZE+context->pacdatalen);
        context->txpacket.header.nuFrame++;
    }
    return ret;
}


inline void do_async_plucking(vban_stream_context_t* context)
{
    int bufreadspace;
    if (context->flags&PLUCKING_EN)
    {
        //while(pthread_mutex_trylock(&context->rxmutex.threadlock)!= 0);
        pthread_mutex_lock(&context->rxmutex.threadlock);
        bufreadspace = ringbuffer_read_space(context->ringbuffer);
        pthread_mutex_unlock(&context->rxmutex.threadlock);
        if (bufreadspace>(context->ringbuffer->size*3/4)) context->flags|= PLUCKING_ON;
        else if (bufreadspace<(context->ringbuffer->size*1/2)) context->flags&=~PLUCKING_ON;
    }
}


inline void read_from_ringbuffer_async(vban_stream_context_t* context)
{
    int lost = 0;

    for (int frame=0; frame<context->nframes; frame++)
    {
        //while(pthread_mutex_trylock(&context->rxmutex.threadlock));
        pthread_mutex_lock(&context->rxmutex.threadlock);
        if (vban_read_frame_from_ringbuffer(&context->rxbuf[frame*context->nboutputs], context->ringbuffer, context->nboutputs))
        {
            lost++;
            if (frame!=0) memcpy(&context->rxbuf[frame*context->nboutputs], &context->rxbuf[(frame - 1)*context->nboutputs], sizeof(float)*context->nboutputs);
        }
        pthread_mutex_unlock(&context->rxmutex.threadlock);
    }

    if (lost==0)
    {
        //while(pthread_mutex_trylock(&context->rxmutex.threadlock));
        pthread_mutex_lock(&context->rxmutex.threadlock);
        context->lost_pac_cnt = 0;
        if ((context->flags&(PLUCKING_EN|PLUCKING_ON))!=0)
        {
            vban_add_frame_from_ringbuffer(&context->rxbuf[(context->nframes - 1)*context->nboutputs], context->ringbuffer, context->nboutputs);
            //fprintf(stderr, "%d bytes in buffer. Sample added!\r\n",bufreadspace );
        }
        pthread_mutex_unlock(&context->rxmutex.threadlock);
    }
    else
    {
        if (context->lost_pac_cnt<9) fprintf(stderr, "%d samples lost\n", lost);
        if (lost==context->nframes)
        {
            if (context->lost_pac_cnt<10) context->lost_pac_cnt++;
            if (context->lost_pac_cnt==9)
                memset(context->rxbuf, 0, context->rxbuflen*sizeof(float));
        }
    }
}


inline void read_from_ringbuffer_async_non_interleaved(vban_stream_context_t* context, float** buffers)
{
    int lost = 0;
    int rb_readspace;

    for (int frame=0; frame<context->nframes; frame++)
    {
        //while(pthread_mutex_trylock(&context->rxmutex.threadlock));
        //pthread_mutex_lock(&context->rxmutex.threadlock);
        rb_readspace = ringbuffer_read_space(context->ringbuffer);
        //pthread_mutex_unlock(&context->rxmutex.threadlock);
        if (rb_readspace < sizeof(float)*context->nboutputs)
        {
            lost++;
            context->flags&=~PLUCKING_ON;
        }
        else
        {
            //while(pthread_mutex_trylock(&context->rxmutex.threadlock));
            pthread_mutex_lock(&context->rxmutex.threadlock);
            vban_read_frame_from_ringbuffer(context->rxbuf, context->ringbuffer, context->nboutputs);
            if ((frame==(context->nframes - 1))&&(context->flags&PLUCKING_ON)) vban_add_frame_from_ringbuffer(context->rxbuf, context->ringbuffer, context->nboutputs);
            pthread_mutex_unlock(&context->rxmutex.threadlock);
            for (int channel=0; channel<context->nboutputs; channel++) buffers[channel][frame] = context->rxbuf[channel];
        }
    }
    if (lost)
    {
        if (context->lost_pac_cnt<9) fprintf(stderr, "%d samples lost\n", lost);
        if (lost==context->nframes)
        {
            if (context->lost_pac_cnt<10) context->lost_pac_cnt++;
            if (context->lost_pac_cnt>=9)
            {
                for (int channel = 0; channel < context->nboutputs; channel++)
                    for (int frame=0; frame<context->nframes; frame++)
                        buffers[channel][frame] = 0;
            }
        }
    }
}


inline static int vban_rx_handle_packet(VBanPacket* vban_packet, int packetlen, vban_stream_context_t* context, uint32_t ip_in, u_int16_t port_in)
{
    uint16_t nbc;
    static float fsamples[VBAN_CHANNELS_MAX_NB];
    size_t framesize;
    size_t outframesize;
    char* srcptr;
    uint16_t eventptr;
    uint32_t iplocal = context->iplocal;

    switch (vban_packet->header.format_SR&VBAN_PROTOCOL_MASK)
    {
    case VBAN_PROTOCOL_AUDIO:
        if (context->ringbuffer==nullptr)  // ringbuffer is not created that means client is not created too
        {
            if ((ip_in!= context->iplocal)||(context->rxport == 0))
            {
                // Init backend
                if (context->nboutputs==0) context->nboutputs = vban_packet->header.format_nbc + 1;
                context->samplerate_resampler = VBanSRList[vban_packet->header.format_SR];

                // Let main loop continue
                if (pthread_mutex_trylock(&context->cmdmutex.threadlock)==0)
                {
                    pthread_cond_signal(&context->cmdmutex.dataready);
                    pthread_mutex_unlock(&context->cmdmutex.threadlock);
                }
                return 1;
            }
        }
        if ((strncmp(vban_packet->header.streamname, context->rx_streamname, VBAN_STREAM_NAME_SIZE)==0)&& // stream name matches
            //(vban_packet->header.format_SR  == vban_get_format_SR(context->samplerate))&& // will be deprecated after resampler
            (vban_packet->header.nuFrame!= context->nu_frame)&& // number of packet is not same
            (context->iprx==ip_in))//&&(ip_in!= iplocal))
        {
            nbc = ((vban_packet->header.format_nbc + 1) < context->nboutputs ? (vban_packet->header.format_nbc + 1) : context->nboutputs);
            framesize = (vban_packet->header.format_nbc + 1)*VBanBitResolutionSize[vban_packet->header.format_bit];
            outframesize = context->nboutputs*sizeof(float);
            srcptr = vban_packet->data;
            if (context->samplerate==context->samplerate_resampler)
            {
                for (int frame=0; frame<=vban_packet->header.format_nbs; frame++)
                {
                    // while(pthread_mutex_trylock(&context->rxmutex.threadlock));
                    pthread_mutex_lock(&context->rxmutex.threadlock);
                    vban_sample_convert(fsamples, VBAN_BITFMT_32_FLOAT, srcptr, vban_packet->header.format_bit, nbc);
                    if (ringbuffer_write_space(context->ringbuffer)>=outframesize) ringbuffer_write(context->ringbuffer, (char*)fsamples, outframesize);
                    srcptr+= framesize;
                    pthread_mutex_unlock(&context->rxmutex.threadlock);
                }
            }
            else
            {
                if (context->resampler_inbuflen < (vban_packet->header.format_nbs + 1))
                {
                    if (context->resampler_inbuf!=nullptr) free(context->resampler_inbuf);
                    if (context->resampler_outbuf!=nullptr) free(context->resampler_outbuf);
                    context->resampler_inbuflen = vban_packet->header.format_nbs + 1;
                    context->resampler_inbuf = (float*)calloc(context->resampler_inbuflen*context->nboutputs, sizeof(float));
                    context->resampler_outbuflen = context->resampler_inbuflen * context->samplerate/context->samplerate_resampler + 1;
                    context->resampler_outbuf = (float*)calloc(context->resampler_outbuflen*context->nboutputs, sizeof(float));
                }
                else
                {
                    context->resampler_inbuflen = vban_packet->header.format_nbs + 1;
                    context->resampler_outbuflen = context->resampler_inbuflen * context->samplerate/context->samplerate_resampler + 1;
                }

                for (int frame = 0; frame<=vban_packet->header.format_nbs; frame++)
                {
                    vban_sample_convert(&context->resampler_inbuf[frame*context->nboutputs], VBAN_BITFMT_32_FLOAT, srcptr, vban_packet->header.format_bit, nbc);
                    srcptr+= framesize;
                }

                context->resampler->inp_count = context->resampler_inbuflen;
                context->resampler->inp_data = context->resampler_inbuf;
                context->resampler->out_count = context->resampler_outbuflen;
                context->resampler->out_data = context->resampler_outbuf;
                context->resampler->process();

                for (int frame = 0; frame < (context->resampler_outbuflen - context->resampler->out_count); frame++)
                {
                    // while(pthread_mutex_trylock(&context->rxmutex.threadlock));
                    pthread_mutex_lock(&context->rxmutex.threadlock);
                    if (ringbuffer_write_space(context->ringbuffer)>=outframesize)
                        ringbuffer_write(context->ringbuffer, (const char*)&context->resampler_outbuf[frame*nbc], outframesize);
                    pthread_mutex_unlock(&context->rxmutex.threadlock);
                }
            }

            context->nu_frame = vban_packet->header.nuFrame;
            return 0;
        }
        break;
    case VBAN_PROTOCOL_SERIAL:
        if ((vban_packet->header.vban==VBAN_HEADER_FOURC)&&
            (vban_packet->header.format_SR==0x2E)&&
            ((strcmp(vban_packet->header.streamname, context->rx_streamname)==0)||
            (strcmp(vban_packet->header.streamname, "MIDI1")==0)))
        {
            if (context->ringbuffer_midi!=0)
            {
                eventptr = 0;
                while((eventptr+VBAN_HEADER_SIZE)<packetlen)
                {
                    if (ringbuffer_write_space(context->ringbuffer_midi)>=3)
                    {
                        ringbuffer_write(context->ringbuffer_midi, &vban_packet->data[eventptr], 3);
                        eventptr+= 3;
                    }
                }
            }
        }
        break;
    case VBAN_PROTOCOL_TXT:
        if ((memcmp(vban_packet->header.streamname, "info", 4)==0)||(memcmp(vban_packet->header.streamname, "INFO", 4)==0))
        {
            if (((packetlen - VBAN_HEADER_SIZE) <= 5)&&(ip_in!= iplocal)) // INFO REQUEST
            {
                fprintf(stderr, "Info request from %d.%d.%d.%d:%d\n", ((uint8_t*)&ip_in)[0], ((uint8_t*)&ip_in)[1], ((uint8_t*)&ip_in)[2], ((uint8_t*)&ip_in)[3], port_in);
                udp_send(context->rxsock, port_in, (char*)&context->info, VBAN_HEADER_SIZE + strlen(context->info.data), ip_in);
                //vban_fill_receptor_info(context);
                // context->flags|= CMD_PRESENT;
                // if (pthread_mutex_trylock(&context->cmdmutex.threadlock)==0)
                // {
                //     pthread_cond_signal(&context->cmdmutex.dataready);
                //     pthread_mutex_unlock(&context->cmdmutex.threadlock);
                // }
            }
            else if ((packetlen - VBAN_HEADER_SIZE) > 5) // PROCESS INCOMING INFO
            {
                if (context->command!= nullptr)
                {
                    memset(context->command, 0, strlen(context->command));
                    strncat(context->command, "info ", 5);
                    memcpy(&context->command[5], vban_packet->data, strlen(vban_packet->data));
                    memset(vban_packet->data, 0, VBAN_DATA_MAX_SIZE);
                    if (sizeof(context->command) - strlen(context->command) > 25)
                    {
                        strncat(context->command, " ipother=", 9);
                        strncat(context->command, inet_ntoa(*(in_addr*)&ip_in), strlen(inet_ntoa(*(in_addr*)&ip_in)));
                        context->flags|= CMD_PRESENT;
                        if (pthread_mutex_trylock(&context->cmdmutex.threadlock)==0)
                        {
                            pthread_cond_signal(&context->cmdmutex.dataready);
                            pthread_mutex_unlock(&context->cmdmutex.threadlock);
                        }
                    }
                }
            }
        }
        else if ((memcmp(vban_packet->header.streamname, "command", 7)==0)||(memcmp(vban_packet->header.streamname, "COMMAND", 7)==0))
        {
            if (context->command!= nullptr)
            {
                memcpy(context->command, vban_packet->data, strlen(vban_packet->data));
                memset(vban_packet->data, 0, VBAN_DATA_MAX_SIZE);
                // pthread_mutex_lock(&context->cmdmutex.threadlock);
                // context->flags|= CMD_PRESENT;
                // pthread_cond_signal(&context->cmdmutex.dataready);
                // pthread_mutex_unlock(&context->cmdmutex.threadlock);
                context->flags|= CMD_PRESENT;
                if (pthread_mutex_trylock(&context->cmdmutex.threadlock)==0)
                {
                    pthread_cond_signal(&context->cmdmutex.dataready);
                    pthread_mutex_unlock(&context->cmdmutex.threadlock);
                }
            }
        }
        else if ((memcmp(vban_packet->header.streamname, "message", 7)==0)||(memcmp(vban_packet->header.streamname, "MESSAGE", 7)==0))
        {
            fprintf(stderr, "Message from %d.%d.%d.%d:%d\r\n%s\r\n", ((uint8_t*)&ip_in)[0], ((uint8_t*)&ip_in)[1], ((uint8_t*)&ip_in)[2], ((uint8_t*)&ip_in)[3], port_in, vban_packet->data);
        }
        break;
    case VBAN_PROTOCOL_USER:
        break;
    default:
        break;
    }
    return 0;
}

#endif
