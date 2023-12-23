#include "../vban_common/pipewire_backend.h"
#include "../vban_common/vban_functions.h"
#include "../vban_common/udpsocket.h"

#include <sys/timerfd.h>

#define PIPEWIRE

// uint32_t rbwspc_old = 0, rbwspc_new;
// uint8_t disconnect_cnt = 0;

int tfd; // timer file descriptor
struct itimerspec ts; // time mark
struct pollfd tds[1]; // poll file descriptor for timer
int tlen;
uint64_t tval;
uint8_t no_data_cnt = 0;


void* timerThread(void* param)
{
    int ret;
    while(1)
    {
        ret = poll(tds, tlen, -1);
        read(tfd, &tval, sizeof(tval));
        if (no_data_cnt<100) no_data_cnt+=25;
        else
        {
            if (((streamConfig_t*)param)->flags&CONNECTED)
                ((streamConfig_t*)param)->flags&=~CONNECTED;
        }
    }
}


void* rxThread(void* param)
{
    streamConfig_t* stream = (streamConfig_t*)param;
    int packetlen;
    char* dstptr;
    char* srcptr;
    float* srcfptr;
    int32_t nframes;
    int32_t frame;
    int32_t nchannels;// = stream->header.format_nbc + 1;
    int32_t channel;
    size_t framesize;// = VBanBitResolutionSize[stream->header.format_bit&VBAN_BIT_RESOLUTION_MASK]*nchannels;
    int datalen;
    int32_t tmp;
    char* samplebuffer;

    while (1)
    {
        while (poll(&stream->polldesc, 1, 500))
        {
            if (stream->port)   //SOCKET
            {
                packetlen = UDP_recv(stream->vban_sd, &stream->vban_si_other, (uint8_t*)&stream->packet, VBAN_PROTOCOL_MAX_SIZE);
                datalen = packetlen - VBAN_HEADER_SIZE;
            }
            else                // PIPE
            {
                packetlen = read(stream->polldesc.fd, &stream->packet.header, VBAN_HEADER_SIZE);
                datalen = VBanBitResolutionSize[stream->packet.header.format_bit&VBAN_BIT_RESOLUTION_MASK]*(stream->packet.header.format_nbc+1)*(stream->packet.header.format_nbs+1);
                if (datalen==read(stream->polldesc.fd, stream->packet.data, datalen)) packetlen+= datalen;
                //packetlen = read(stream->polldesc.fd, &stream->packet, VBAN_DATA_MAX_SIZE);
            }
            if (packetlen>=VBAN_HEADER_SIZE) // packet header section is not empty
            {
                if (stream->port) stream->iprx = stream->vban_si_other.sin_addr.s_addr; // get senrder's ip

                if (stream->packet.header.vban==VBAN_HEADER_FOURC) // check for VBAN packet
                {
                    switch (stream->packet.header.format_SR&VBAN_PROTOCOL_MASK)
                    {
                    case VBAN_PROTOCOL_AUDIO:
                        if ((strncmp(stream->packet.header.streamname, stream->header.streamname, VBAN_STREAM_NAME_SIZE)==0)&&(stream->ip == stream->iprx)&&
                            (stream->packet.header.format_SR  == stream->header.format_SR)&&
                            (stream->packet.header.format_nbc == stream->header.format_nbc)&&
                            (stream->packet.header.nuFrame    != stream->header.nuFrame))//&&(stream->flags&CONNECTED))
                        {
                            nchannels = stream->header.format_nbc + 1;
                            nframes = ((uint32_t)stream->packet.header.format_nbs+1);
                            framesize = VBanBitResolutionSize[stream->header.format_bit&VBAN_BIT_RESOLUTION_MASK]*nchannels;
                            samplebuffer = (char*)malloc(framesize);
                            srcptr = &((char*)&stream->packet)[VBAN_HEADER_SIZE];

                            if (((stream->packet.header.format_bit&VBAN_BIT_RESOLUTION_MASK)==(stream->header.format_bit&VBAN_BIT_RESOLUTION_MASK)))
                            {
                                {
                                    for (frame=0; frame<nframes; frame++)
                                    {
                                        if (jack_ringbuffer_write_space(stream->ringbuffer)>=framesize) jack_ringbuffer_write(stream->ringbuffer, srcptr, framesize);
                                        srcptr+= framesize;
                                    }
                                }
                            }
                            else
                            {
                                switch (stream->packet.header.format_bit&VBAN_BIT_RESOLUTION_MASK)
                                {
                                case VBAN_BITFMT_16_INT:
                                    switch (stream->header.format_bit&VBAN_BIT_RESOLUTION_MASK)
                                    {
                                    case VBAN_BITFMT_24_INT:
                                        for (frame=0; frame<nframes; frame++)
                                        {
                                            dstptr = samplebuffer;
                                            for (channel=0; channel<nchannels; channel++)
                                            {
                                                dstptr[0] = 0;
                                                dstptr[1] = srcptr[0];
                                                dstptr[2] = srcptr[1];
                                                srcptr+= 2; // 16 bit = 2 bytes
                                                dstptr+= 3; // 24 bit = 3 bytes
                                            }
                                            if (jack_ringbuffer_write_space(stream->ringbuffer)>=framesize) jack_ringbuffer_write(stream->ringbuffer, samplebuffer, framesize);                                        }
                                        break;
                                    case VBAN_BITFMT_32_FLOAT:
                                        for (frame=0; frame<nframes; frame++)
                                        {
                                            dstptr = samplebuffer;
                                            for (channel=0; channel<nchannels; channel++)
                                            {
                                                ((float*)dstptr)[0] = (float)(*((int16_t const*)srcptr))/(float)(1 << 15);
                                                srcptr+= 2; // 16 bit = 2 bytes
                                                dstptr+= 4; // 32 bit = 4 bytes
                                            }
                                            if (jack_ringbuffer_write_space(stream->ringbuffer)>=framesize) jack_ringbuffer_write(stream->ringbuffer, samplebuffer, framesize);
                                        }
                                        break;
                                    default:
                                        break;
                                    }
                                    break;
                                case VBAN_BITFMT_24_INT:
                                    switch (stream->header.format_bit&VBAN_BIT_RESOLUTION_MASK)
                                    {
                                    case VBAN_BITFMT_16_INT:
                                        for (frame=0; frame<nframes; frame++)
                                        {
                                            dstptr = samplebuffer;
                                            for (channel=0; channel<nchannels; channel++)
                                            {
                                                dstptr[0] = srcptr[1];
                                                dstptr[1] = srcptr[2];
                                                srcptr+= 3; // 24 bit = 3 bytes
                                                dstptr+= 2; // 16 bit = 2 bytes
                                            }
                                            if (jack_ringbuffer_write_space(stream->ringbuffer)>=framesize) jack_ringbuffer_write(stream->ringbuffer, samplebuffer, framesize);
                                        }
                                        break;
                                    case VBAN_BITFMT_32_FLOAT:
                                        for (frame=0; frame<nframes; frame++)
                                        {
                                            dstptr = samplebuffer;
                                            for (channel=0; channel<nchannels; channel++)
                                            {
                                                ((float*)dstptr)[0] = (float)(((srcptr[2])<<16) + ((uint8_t)srcptr[1]<<8) + (uint8_t)srcptr[0])/(float)(1 << 23);
                                                srcptr+= 3; // 24 bit = 3 bytes
                                                dstptr+= 4; // 32 bit = 4 bytes
                                            }
                                            if (jack_ringbuffer_write_space(stream->ringbuffer)>=framesize) jack_ringbuffer_write(stream->ringbuffer, samplebuffer, framesize);
                                        }
                                        break;
                                    default:
                                        break;
                                    }
                                    break;
                                case VBAN_BITFMT_32_FLOAT:
                                    srcfptr = (float*)&stream->packet.data;
                                    switch (stream->header.format_bit&VBAN_BIT_RESOLUTION_MASK)
                                    {
                                    case VBAN_BITFMT_16_INT:
                                        for (frame=0; frame<nframes; frame++)
                                        {
                                            dstptr = samplebuffer;
                                            for (channel=0; channel<nchannels; channel++)
                                            {
                                                tmp = (int16_t)roundf(32767.0f*srcfptr[0]);
                                                dstptr[0] = tmp&0xFF;
                                                dstptr[1] = (tmp>>8)&0xFF;
                                                dstptr+= 2; // 16 bit = 2 bytes
                                                srcptr+= 4; // 32 bit = 4 bytes
                                                srcfptr++;
                                            }
                                            if (jack_ringbuffer_write_space(stream->ringbuffer)>=framesize) jack_ringbuffer_write(stream->ringbuffer, samplebuffer, framesize);
                                        }
                                        break;
                                    case VBAN_BITFMT_24_INT:
                                        for (frame=0; frame<nframes; frame++)
                                        {
                                            dstptr = samplebuffer;
                                            for (channel=0; channel<nchannels; channel++)
                                            {
                                                tmp = (int32_t)roundf(8388607.0f*srcfptr[0]);
                                                dstptr[0] = tmp&0xFF;
                                                dstptr[1] = (tmp>>8)&0xFF;
                                                dstptr[2] = (tmp>>16)&0xFF;
                                                dstptr+= 3; // 24 bit = 3 bytes
                                                srcptr+= 4; // 32 bit = 4 bytes
                                                srcfptr++;
                                            }
                                            if (jack_ringbuffer_write_space(stream->ringbuffer)>=framesize) jack_ringbuffer_write(stream->ringbuffer, samplebuffer, framesize);
                                        }
                                        break;
                                    default:
                                        break;
                                    }
                                    break;
                                default:
                                    break;
                                }
                            }

                            stream->header.nuFrame = stream->packet.header.nuFrame;
                        }
                        else
                        {
                            // Creating new stream
                            if (((stream->flags&RESAMPLER)!=0)||(stream->packet.header.format_SR==stream->header.format_SR))
                            {
                                if ((strncmp(stream->packet.header.streamname, stream->header.streamname, VBAN_STREAM_NAME_SIZE)==0)||(stream->header.streamname[0]==0))
                                {
                                    if (stream->port) stream->ip = stream->iprx;
                                    else
                                    {
                                        stream->ip = 0;
                                        stream->iprx = 0;
                                    }
                                    // memcpy(&stream->header, &stream->packet.header, VBAN_HEADER_SIZE-4); // Save packet header
                                    stream->header = stream->packet.header;
                                    stream->samplerate = (uint32_t)VBanSRList[stream->packet.header.format_SR&VBAN_SR_MASK];
                                    if (stream->samplerate==0)
                                    {
                                        fprintf(stderr, "ERROR by SR INIT!!!\n");
                                        exit(1);
                                    }
                                    stream->nbchannels = stream->packet.header.format_nbc + 1; // vban_format_nbc = nbchannels - 1
                                    stream->format_vban = stream->packet.header.format_bit&VBAN_BIT_RESOLUTION_MASK;
                                    stream->format = format_vban_to_spa((enum VBanBitResolution)stream->format_vban);
                                    compute_packets_and_bufs(stream);
                                    if ((stream->flags&RECEIVING)==0) fprintf(stderr, "Receiving from %s:%d\n", inet_ntoa(stream->vban_si_other.sin_addr), htons(stream->vban_si_other.sin_port));
                                    stream->flags|= RECEIVING;
                                }
                            }
                        }
                        break;
                    case VBAN_PROTOCOL_SERIAL:
                        break;
                    case VBAN_PROTOCOL_TXT:
                        break;
                    case VBAN_PROTOCOL_USER:
                        break;
                    default:
                        break;
                    }
                }
            }
        }
        if (stream->flags&CONNECTED) fprintf(stderr, "500 ms no signal!\n");
        if (stream->port)
        {
            stream->iprx = 0;
            while (poll(&stream->polldesc, 1, 0))
            {
                packetlen = UDP_recv(stream->vban_sd, &stream->vban_si_other, NULL, VBAN_PROTOCOL_MAX_SIZE);
            }
        }
        /*jack_ringbuffer_reset(stream->ringbuffer);
        char* const zeros = (char*)calloc(1, stream->ringbuffersize);
        jack_ringbuffer_write(stream->ringbuffer, zeros, stream->ringbuffersize);
        free(zeros);//*/
    }
}


static void on_process(void *userdata)
{
    struct data *data = (struct data*)userdata;
    struct pw_buffer *b;
    struct spa_buffer *buf;
    int stride, n_frames;
    char* samples_ptr;
    char* dstptr;
    uint16_t frame;
    uint16_t lost = 0;

    if ((b = pw_stream_dequeue_buffer(data->stream)) == NULL) {
        pw_log_warn("out of buffers: %m");
        return;
    }

    buf = b->buffer;
    samples_ptr = (char*)(b->buffer->datas[0].data);
    if (samples_ptr == NULL)
        return;

    stride = data->config->nbchannels*VBanBitResolutionSize[data->config->format_vban]; //sizeof(float) * DEFAULT_CHANNELS;
    n_frames = buf->datas[0].maxsize / stride;
    if (b->requested) n_frames = SPA_MIN((int)b->requested, n_frames);

    if (n_frames!=data->config->nframes)
    {
        data->config->nframes = n_frames;
        compute_packets_and_bufs(data->config);
        fprintf(stderr, "Warning: buffer size is changed to %d!\n", n_frames);
    }

    dstptr = data->config->buf;
    for (frame=0; frame<n_frames; frame++)
    {
        if (jack_ringbuffer_read_space(data->config->ringbuffer)>=stride)
        {
            jack_ringbuffer_read(data->config->ringbuffer, dstptr, stride);
            dstptr+= stride;
        }
        else
        {
            if (frame!=0)
            {
                memcpy(dstptr, dstptr-stride, stride);
                dstptr+= stride;
            }
            lost++;
        }
    }
    //memcpy(samples_ptr, data->config->buf, data->config->buflen);
    if (lost==0)
    {
        data->config->flags|= CONNECTED;
        data->config->lostpaccnt = 0;
        no_data_cnt = 0;
    }
    else
    {
        if (data->config->lostpaccnt<9) fprintf(stderr, "%d samples lost\n", lost);
        if (lost==n_frames)
        {
            if (data->config->lostpaccnt<10) data->config->lostpaccnt++;
            if (data->config->lostpaccnt==9)
            {
                memset(data->config->buf, 0, data->config->buflen);
            }
        }
    }
    if (data->config->lostpaccnt<8) memcpy(samples_ptr, data->config->buf, data->config->buflen);
    else memset(samples_ptr, 0, data->config->buflen);

    buf->datas[0].chunk->offset = 0;
    buf->datas[0].chunk->stride = stride;
    buf->datas[0].chunk->size = n_frames * stride;//*/

    pw_stream_queue_buffer(data->stream, b);
}


static const struct pw_stream_events stream_events = {
    PW_VERSION_STREAM_EVENTS,
    .process = on_process,
};


static void do_quit(void *userdata, int signal_number)
{
    struct data *data = (struct data*)userdata;
    pw_main_loop_quit(data->loop);
}


int main(int argc, char *argv[])
{
    struct data data = { 0, };
    const struct spa_pod *params[1];
    uint8_t buffer[16384];
    struct pw_properties *props;
    struct spa_pod_builder b;
    struct spa_audio_info_raw audio_info;
    streamConfig_t config;
    char tmpstring[16];
    mutexcond rxmutex;
    mutexcond timermutex;

    rxmutex.threadlock = PTHREAD_MUTEX_INITIALIZER;
    rxmutex.dataready  = PTHREAD_COND_INITIALIZER;

    timermutex.threadlock = PTHREAD_MUTEX_INITIALIZER;
    timermutex.dataready = PTHREAD_COND_INITIALIZER;

    memset(tmpstring, 0, 16);
    data.config = (streamConfig_t*)&config;
    memset((char*)&config, 0, sizeof(streamConfig_t));
    //config.additionals = &rxmutex;
    config.additionals = &no_data_cnt;
    config.flags|= PLAYBACK;

    config.nframes = 128;
    config.flags|= RESAMPLER; // Resampler is built-in on Pipewire
    strcpy(config.header.streamname, "Stream1");
    strcpy(config.ipaddr, "pipe");
    strcpy(config.pipename, "stdin");

    get_options(&config, argc, argv);
    if (config.redundancy==0) config.redundancy = 1; // for Pipewire backend

    if (strncmp(config.ipaddr, "pipe", 4)) // using socket
    {
        config.vban_sd = UDP_init(&config.vban_sd, &config.vban_si, &config.vban_si_other, config.ipaddr, config.port, 's', 1, 6);
        //config.vban_sd = UDP_init(&config.vban_sd, &config.vban_si_other, config.ipaddr, config.port, 's', 1, 6);
        if (config.vban_sd<0)
        {
            fprintf(stderr, "Can't bind the socket! Maybe, port is busy?\n");
            return 1;
        }
        printf("Socket is successfully created! Port: %d, priority: 6\n", config.port);
        config.polldesc.fd = config.vban_sd;
        config.polldesc.events = POLLIN;

        while (poll(&config.polldesc, 1, 0)) UDP_recv(config.vban_sd, &config.vban_si_other, NULL, VBAN_PROTOCOL_MAX_SIZE); //flush socket
    }
    else // using pipe
    {
        config.port = 0;
        if (strncmp(config.pipename, "stdin", 6)) // named pipe
        {
            config.pipedesc = open(config.pipename, O_RDONLY);
            mkfifo(config.pipename, 0666);
        }
        else config.pipedesc = 0; // stdin
        config.polldesc.fd = config.pipedesc;
        config.polldesc.events = POLLIN;
    }
    pthread_attr_init(&rxmutex.attr);
    pthread_create(&rxmutex.tid, &rxmutex.attr, rxThread, (void*)&config);
    //pthread_setcanceltype (PTHREAD_CANCEL_ASYNCHRONOUS, NULL);
    //pthread_mutex_lock (&rxmutex.threadlock);
    while ((config.flags&RECEIVING)==0) usleep(100000);//pthread_cond_wait(&rxmutex.dataready, &rxmutex.threadlock);
    //pthread_mutex_unlock(&rxmutex.threadlock);
    usleep(10000);
    config.format = format_vban_to_spa((enum VBanBitResolution)config.format_vban);

    tfd = timerfd_create(CLOCK_REALTIME,  0);
    ts.it_interval.tv_sec = 0;
    ts.it_interval.tv_nsec = 100000000;
    ts.it_value.tv_sec = 1;
    ts.it_value.tv_nsec = 100000000;
    tlen = sizeof(tds) / sizeof(tds[0]);
    tds[0].fd = tfd;
    tds[0].events = POLLIN;
    timerfd_settime(tfd, 0, &ts, NULL);

    pthread_attr_init(&timermutex.attr);
    pthread_create(&timermutex.tid, &timermutex.attr, timerThread, (void*)&config);

    // INIT PIPEWIRE NODE
    spa_pod_builder_init(&b, buffer, sizeof(buffer));
    pw_init(NULL, NULL); //(&argc, &argv);

    data.loop = pw_main_loop_new(NULL);

    pw_loop_add_signal(pw_main_loop_get_loop(data.loop), SIGINT, do_quit, &data);
    pw_loop_add_signal(pw_main_loop_get_loop(data.loop), SIGTERM, do_quit, &data);

    sprintf(tmpstring, "%d/%d", config.nframes, config.samplerate);
    props = pw_properties_new(PW_KEY_MEDIA_TYPE, "Audio",
                              PW_KEY_MEDIA_CATEGORY, "Playback",
                              PW_KEY_MEDIA_ROLE, "Music",
                              PW_KEY_NODE_LATENCY, tmpstring, // nframes/samplerate
                              NULL);
    fprintf(stderr, "PIPEWIRE_LATENCY=%s\n", tmpstring);
    //if (argc > 1) pw_properties_set(props, PW_KEY_TARGET_OBJECT, argv[1]);

    data.stream = pw_stream_new_simple(
        pw_main_loop_get_loop(data.loop),
        config.header.streamname,
        props,
        &stream_events,
        &data);

    audio_info.channels = config.nbchannels;
    audio_info.format = format_vban_to_spa((enum VBanBitResolution)config.format_vban);
    //audio_info.rate = config.samplerate;
    params[0] = spa_format_audio_raw_build(&b, SPA_PARAM_EnumFormat, &audio_info);

    pw_stream_connect(data.stream,
                      PW_DIRECTION_OUTPUT,
                      PW_ID_ANY,
                      //PW_STREAM_FLAG_AUTOCONNECT | //(1 << 0)
                      //PW_STREAM_FLAG_MAP_BUFFERS | PW_STREAM_FLAG_RT_PROCESS,
                      (pw_stream_flags)((1 << 2)|(1 << 4)),
                      params, 1);

    // GOOOOOO!!!!!!1111
    pw_main_loop_run(data.loop);

    pw_stream_destroy(data.stream);
    pw_main_loop_destroy(data.loop);
    pw_deinit();

    return 0;
}
