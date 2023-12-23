#include "alsa_backend.h"

// КОСТЫЛИ
uint32_t nodatacnt = 0;


void* rxThread(void* param)
{
    streamConfig_t* stream = (streamConfig_t*)param;
    int packetlen;
    char* dstptr;
    char* srcptr;
    float* srcfptr;
    int32_t nframes;
    int32_t frame;
    int32_t nchannels = stream->header.format_nbc + 1;
    int32_t channel;
    size_t framesize = VBanBitResolutionSize[stream->header.format_bit&VBAN_BIT_RESOLUTION_MASK]*nchannels;
    int datalen;
    int32_t tmp;
    char* samplebuffer = (char*)malloc(framesize);


    while (1)
    {
        while (poll(&stream->polldesc, 1, 500))
        {
            if (stream->port)
            {
                packetlen = UDP_recv(stream->vban_sd, &stream->vban_si, (uint8_t*)&stream->packet, VBAN_PROTOCOL_MAX_SIZE);
                datalen = packetlen - VBAN_HEADER_SIZE;
            }
            else
            {
                packetlen = read(stream->polldesc.fd, &stream->packet.header, VBAN_HEADER_SIZE);
            }
            if (packetlen)
            {
                stream->iprx = stream->vban_si.sin_addr.s_addr; // get senrder's ip
                if (stream->packet.header.vban==VBAN_HEADER_FOURC) // check for VBAN packet
                {
                    switch (stream->packet.header.format_SR&VBAN_PROTOCOL_MASK)
                    {
                    case VBAN_PROTOCOL_AUDIO:
                        if (stream->port)
                        {
                            if (stream->ip == 0) // still not started
                            {
                                if (stream->packet.header.format_SR!=stream->header.format_SR)
                                {
                                    fprintf(stderr, "Error: Samplerate mismatch!");
                                    free(samplebuffer);
                                    exit(1);
                                    //return 1;
                                    // TODO : REWORK TO RESAMPLER!
                                }
                                if ((strncmp(stream->packet.header.streamname, stream->header.streamname, VBAN_STREAM_NAME_SIZE)==0)&&(stream->packet.header.format_nbc==stream->header.format_nbc)) stream->ip = stream->iprx;
                            }
                        }
                        else
                        {
                            datalen = VBanBitResolutionSize[stream->packet.header.format_bit&VBAN_BIT_RESOLUTION_MASK]*(stream->packet.header.format_nbc+1)*(stream->packet.header.format_nbs+1);
                            if (datalen==read(stream->polldesc.fd, stream->packet.data, datalen))
                            {
                                    packetlen+= datalen;
                            }
                        }

                        if ((strncmp(stream->packet.header.streamname, stream->header.streamname, VBAN_STREAM_NAME_SIZE)==0)&&(stream->ip == stream->iprx)&&
                                (stream->packet.header.format_SR  == stream->header.format_SR)&&
                                (stream->packet.header.format_nbc == stream->header.format_nbc)&&
                                (stream->packet.header.nuFrame    != stream->header.nuFrame))
                        {
                            nframes = ((uint32_t)stream->packet.header.format_nbs+1);
                            srcptr = &((char*)&stream->packet)[VBAN_HEADER_SIZE];

                            if ((stream->packet.header.format_bit&VBAN_BIT_RESOLUTION_MASK)==(stream->header.format_bit&VBAN_BIT_RESOLUTION_MASK))
                            {
                                for (frame=0; frame<nframes; frame++)
                                {
                                    if (jack_ringbuffer_write_space(stream->ringbuffer)>=framesize) jack_ringbuffer_write(stream->ringbuffer, srcptr, framesize);
                                    srcptr+= framesize;
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
        fprintf(stderr, "500 ms no signal!\n");
        if (stream->port)
        {
            stream->iprx = 0;
            memset(stream->buf, 0, stream->buflen);
            while (poll(&stream->polldesc, 1, 0))
            {
                packetlen = UDP_recv(stream->vban_sd, &stream->vban_si, NULL, VBAN_PROTOCOL_MAX_SIZE);
            }
        }//*/
    }
}


void pbCallback(snd_async_handler_t *pcm_callback)  // playback callback
{
    snd_pcm_t *handle = snd_async_handler_get_pcm(pcm_callback);
    streamConfig_t *pcm_handle = (streamConfig_t*)snd_async_handler_get_callback_private(pcm_callback);

    int nframes = pcm_handle->nframes;
    int frame;
    int nbchannels = pcm_handle->header.format_nbc + 1;
    size_t framesize = VBanBitResolutionSize[pcm_handle->format_vban]*nbchannels;
    char* ptr;
    int wroteframes;
    jack_ringbuffer_t* ringbuf = pcm_handle->ringbuffer;
    snd_pcm_sframes_t avail;

    ptr = pcm_handle->buf;
    size_t rb_avail;

    /*if (jack_ringbuffer_read_space(ringbuf)==0) if (nodatacnt<215) nodatacnt+=10;
    else if (nodatacnt) nodatacnt--;
    if ((nodatacnt>200)&&(nodatacnt<211))
    {
        fprintf(stderr, "No signal!\n");
    }//*/

    for (frame=0; frame<nframes; frame++)
    {
        if (jack_ringbuffer_read_space(ringbuf)>=framesize) jack_ringbuffer_read(ringbuf, ptr, framesize);
        ptr+= framesize;
    }

    avail = snd_pcm_avail_update(handle);
//    fprintf(stderr, "Available %ld\n", avail);
    if (avail>=nframes)
    {
        wroteframes = snd_pcm_writei(handle, (void*)pcm_handle->buf, nframes);
        if (wroteframes<0) wroteframes = snd_pcm_recover(handle, wroteframes, 0);
        if (wroteframes<0) snd_pcm_prepare(handle);//fprintf(stderr, "Error: writei failed!\n");
        else avail = snd_pcm_avail_update(handle);
    }

}


int get_options(alsaHandle_t* handle, streamConfig_t* stream, int argc, char *argv[])
{
    int index;
    char c;
    static const struct option options[] =
    {
        {"device",      required_argument,  0, 'd'},
        {"samplerate",  required_argument,  0, 'r'},
        {"bufsize",     required_argument,  0, 'q'},
        {"nbchannels",  required_argument,  0, 'c'},
        {"redundancy",  required_argument,  0, 'n'},
        {"format",      required_argument,  0, 'f'},
        {"ipaddr",      required_argument,  0, 'i'},
        {"port",        required_argument,  0, 'p'},
        {"streamname",  required_argument,  0, 's'},
        {"help",        no_argument,        0, 'h'},
        {0,             0,                  0,  0 }
    };
    int ipnums[4];

    c = getopt_long(argc, argv, "d:r:q:c:n:f:i:p:s:h", options, 0);
    if (c==-1)
    {
        fprintf(stderr, "No params!\n");
        // TODO : Insert help here
        return 0;
    }
    while(c!=-1)
    {
        switch (c)
        {
        case 'd':
            if (optarg[0]==0)
            {
                fprintf(stderr, "No audio device selected!\n");
                return 1;
            }
            else
            {
                memset(handle->cardName, 0, CARD_NAME_LENGTH);
                if (strlen(optarg)<CARD_NAME_LENGTH) strcpy(handle->cardName, optarg);
                else strncpy(handle->cardName, optarg, CARD_NAME_LENGTH-1);
            }
            break;

        case 'r':
            handle->samplerate = atoi(optarg);
            break;
        case 'q':
            handle->nframes = atoi(optarg);
            break;
        case 'c':
            handle->nbchannels = atoi(optarg);
            //packet.header.format_nbc = captureHandle.nbchannels - 1;
            break;
        case 'n':
            stream->redundancy = atoi(optarg);
            break;
        case 'f':
            if ((strncmp(optarg, "16I", 3)==0)|(strncmp(optarg, "16i", 3)==0)) stream->format_vban = VBAN_BITFMT_16_INT;
            else if ((strncmp(optarg, "24I", 3)==0)|(strncmp(optarg, "24i", 3)==0)) stream->format_vban = VBAN_BITFMT_24_INT;
            else if ((strncmp(optarg, "32F", 3)==0)|(strncmp(optarg, "32F", 3)==0)) stream->format_vban = VBAN_BITFMT_32_FLOAT;
            handle->format = vban_to_alsa_format((VBanBitResolution)stream->format_vban);
            break;
        case 'i':
            memset(stream->ipaddr, 0, 16);
            if (strlen(optarg)<16) strcpy(stream->ipaddr, optarg);
            else strncpy(stream->ipaddr, optarg, 16);
            if(strncmp(stream->ipaddr, "pipe", 4))
            {
                sscanf(stream->ipaddr, "%d.%d.%d.%d", &ipnums[0], &ipnums[1], &ipnums[2], &ipnums[3]);
                if ((ipnums[0]<0)|(ipnums[1]<0)|(ipnums[2]<0)|(ipnums[3]<0)|(ipnums[0]>255)|(ipnums[1]>255)|(ipnums[2]>255)|(ipnums[3]>255))
                {
                    fprintf(stderr, "Error: incorrect IP address!\n");
                    return 1;
                }
                else
                {
                    for(index=0; index<4; index++) ((uint8_t*)&stream->ip)[index] = (uint8_t)ipnums[index];
                    if (stream->port==0) stream->port = 6980;
                }
            }
            else fprintf(stderr, "Using pipe!\n");
            break;
        case 'p':
            if(strncmp(stream->ipaddr, "pipe", 4)) stream->port = atoi(optarg);
            else
            {
                memset(stream->pipename, 0, 64);
                if (strlen(optarg)<64) strcpy(stream->pipename, optarg);
                else strncpy(stream->pipename, optarg, 64);
            }
            break;
        case 's':
            if (optarg[0]==0)
            {
                fprintf(stderr, "No stream name given!\n");
                return 1;
            }
            else
            {
                if (handle->streamDirection) // CAPTURE
                {
                    memset(stream->packet.header.streamname, 0, VBAN_STREAM_NAME_SIZE);
                    if (strlen(optarg)<VBAN_STREAM_NAME_SIZE) strcpy(stream->packet.header.streamname, optarg);
                    else strncpy(stream->packet.header.streamname, optarg, VBAN_STREAM_NAME_SIZE-1);
                }
                else // PLAYBACK
                {
                    memset(stream->header.streamname, 0, VBAN_STREAM_NAME_SIZE);
                    if (strlen(optarg)<VBAN_STREAM_NAME_SIZE) strcpy(stream->header.streamname, optarg);
                    else strncpy(stream->header.streamname, optarg, VBAN_STREAM_NAME_SIZE-1);
                }
            }
            break;
        }
        c = getopt_long(argc, argv, "d:r:q:c:n:f:i:p:s:h", options, 0);
    }
    return 0;
}


int alsa2pipe(int argc, char *argv[])
{

    alsaHandle_t captureHandle;
    streamConfig_t stream;
    int index;
    uint32_t latency;
    int32_t readframes;
    uint8_t packets;
    uint32_t samplesize;
    uint32_t framesize;
    uint32_t packetDataLen;
    snd_pcm_uframes_t packetFrames;


    // Input vars
    stream.ip = 0x7f000001;
    stream.redundancy = 0;
    stream.format_vban = VBAN_BITFMT_16_INT;

    captureHandle.samplerate = 48000;
    captureHandle.nframes = 128;
    memset(captureHandle.cardName, 0, CARD_NAME_LENGTH);
    strcpy(captureHandle.cardName, "default");
    captureHandle.format = vban_to_alsa_format((VBanBitResolution)stream.format_vban);
    captureHandle.access = SND_PCM_ACCESS_RW_INTERLEAVED;
    captureHandle.streamDirection = SND_PCM_STREAM_CAPTURE;
    captureHandle.nbchannels = 2;

    // const uint8_t availableSamplerates = 11;
    // const uint32_t samplerates[availableSamplerates] = {22050, 24000, 32000, 44100, 48000, 88200, 96000, 176400, 192000, 352800, 384000};
    // const uint8_t availableBuffersizes = 14;
    // const snd_pcm_uframes_t buffersizes[availableBuffersizes] = {16, 32, 64, 96, 128, 160, 192, 224, 256, 512, 1024, 2048, 4096, 8192};

    stream.pipedesc = 1;

    memset(stream.ipaddr, 0, 16);
    memset(stream.pipename, 0, 64);
    strncpy(stream.ipaddr, "pipe", 4);
    strncpy(stream.pipename, "stdout", 6);
    stream.port = 0;

    get_options(&captureHandle, &stream, argc, argv);

    if (snd_pcm_open(&captureHandle.handle, captureHandle.cardName, captureHandle.streamDirection, SND_PCM_ASYNC)<0) // SND_PCM_NONBLOCK SND_PCM_ASYNC
    {
        fprintf(stderr, "Error: cannot open audio device %s!\n", captureHandle.cardName);
        snd_pcm_close(captureHandle.handle);
        return 1;
    }
    fprintf(stderr, "Audio device successfully opened\n");
    if (snd_pcm_hw_params_malloc(&captureHandle.hwParams)<0)
    {
        fprintf (stderr, "Error: cannot allocate hardware parameter structure!\n");
        snd_pcm_close(captureHandle.handle);
        return 1;
    }
    if (snd_pcm_hw_params_any(captureHandle.handle, captureHandle.hwParams)<0)
    {
        fprintf(stderr, "Error: cannot initialize hw parameters!\n");
        snd_pcm_close(captureHandle.handle);
        return 1;
    }

    // Get parameters of choosen card
    // Channels number
    if (snd_pcm_hw_params_get_channels_min(captureHandle.hwParams, &captureHandle.nbchannelsmin)<0)
    {
        fprintf (stderr, "Error: cannot get minimum inputs number!\n");
        snd_pcm_close(captureHandle.handle);
        return 1;
    }
    if (snd_pcm_hw_params_get_channels_max(captureHandle.hwParams, &captureHandle.nbchannelsmax)<0)
    {
        fprintf (stderr, "Error: cannot get maximum inputs number!\n");
        snd_pcm_close(captureHandle.handle);
        return 1;
    }
    fprintf(stderr, "Number of inputs available: from %d to %d\n", captureHandle.nbchannelsmin, captureHandle.nbchannelsmax);

    // Samplerate
    if (snd_pcm_hw_params_get_rate_min(captureHandle.hwParams, &captureHandle.samplerateMin, 0)<0)
    {
        fprintf(stderr, "Error: cannot get minimum sample rate!\n");
        snd_pcm_close(captureHandle.handle);
        return 1;
    }
    if (snd_pcm_hw_params_get_rate_max(captureHandle.hwParams, &captureHandle.samplerateMax, 0)<0)
    {
        fprintf(stderr, "Error: cannot get maximum sample rate!\n");
        snd_pcm_close(captureHandle.handle);
        return 1;
    }

    // Buffersize
    if (snd_pcm_hw_params_get_buffer_size_min(captureHandle.hwParams, &captureHandle.nframesMin)<0)
    {
        fprintf(stderr, "Minimum buffer size is unavailable!\n");
        snd_pcm_close(captureHandle.handle);
        return 1;
    }
    if (snd_pcm_hw_params_get_buffer_size_max(captureHandle.hwParams, &captureHandle.nframesMax)<0)
    {
        fprintf(stderr, "Maximum buffer size is unavailable!\n");
        snd_pcm_close(captureHandle.handle);
        return 1;
    }
    fprintf(stderr, "Minimum samplerate: %d, maximum: %d\n", captureHandle.samplerateMin, captureHandle.samplerateMax);
    fprintf(stderr, "Minimum buffer size: %lu, maximum: %lu\n", captureHandle.nframesMin, captureHandle.nframesMax);


    // Apply parameters from options
    // Channels number
    if (captureHandle.nbchannels>captureHandle.nbchannelsmax)
    {
        fprintf(stderr, "Warning: Number of requested channels is more than available!\nSetting number of channels %d\n", captureHandle.nbchannelsmax);
        captureHandle.nbchannels = captureHandle.nbchannelsmax;
        //snd_pcm_close(captureHandle.handle);
        //return 1;
    }
    if (captureHandle.nbchannels<captureHandle.nbchannelsmin)
    {
        fprintf(stderr, "Warning: Number of requested channels is less than available!\nSetting number of channels %d\n", captureHandle.nbchannelsmin);
        captureHandle.nbchannels = captureHandle.nbchannelsmin;
        //snd_pcm_close(captureHandle.handle);
        //return 1;
    }
    // Samplerate
    if (captureHandle.samplerate<captureHandle.samplerateMin)
    {
        for (index=0; index<availableSamplerates; index++)
            if (samplerates[index]>captureHandle.samplerateMin)
            {
                captureHandle.samplerate = samplerates[index];
                fprintf(stderr, "Warning: requested samplerate is lower than minimum available! Trying to set %d...\n", captureHandle.samplerate);
                break;
            }
    }
    else if (captureHandle.samplerate>captureHandle.samplerateMax)
    {
        for (index=availableSamplerates-1; index<=0; index--)
            if (samplerates[index]<captureHandle.samplerateMax)
            {
                captureHandle.samplerate = samplerates[index];
                fprintf(stderr, "Warning: requested samplerate is higher than minimum available! Trying to set %d...\n", captureHandle.samplerate);
                break;
            }
    }

    // Buffersize
    if (captureHandle.nframes<captureHandle.nframesMin)
    {
        for (index=0; index<availableBuffersizes; index++)
            if (buffersizes[index]>captureHandle.nframesMin)
            {
                captureHandle.nframes = buffersizes[index];
                fprintf(stderr, "Warning: requested buffer size is lower than minimum available! Trying to set %lu...\n", captureHandle.nframes);
                break;
            }
    }
    else if (captureHandle.nframes>captureHandle.nframesMax)
    {
        for (index=availableBuffersizes-1; index>=0; index--)
            if (buffersizes[index]<captureHandle.nframesMax)
            {
                captureHandle.nframes = buffersizes[index];
                fprintf(stderr, "Warning: requested buffer size is higher than maximum available! Trying to set %lu...\n", captureHandle.nframes);
                break;
            }
    }

    // Final Init
    if (snd_pcm_hw_params_set_buffer_size(captureHandle.handle, captureHandle.hwParams, captureHandle.nframes)<0)
    {
        fprintf(stderr, "Error: cannot set buffer size!\n");
        snd_pcm_close(captureHandle.handle);
        return 1;
    }
    fprintf(stderr, "Buffer size: %lu\n", captureHandle.nframes);

    latency = 1000000*captureHandle.nframes/captureHandle.samplerate;
    if (snd_pcm_set_params(captureHandle.handle,
                           captureHandle.format,
                           captureHandle.access,
                           captureHandle.nbchannels,
                           captureHandle.samplerate,
                           1, // Resampler enabled
                           latency)<0)
    {
        fprintf(stderr, "Error: cannot initialize stream!\n");
        snd_pcm_close(captureHandle.handle);
        return 1;
    }
    fprintf(stderr, "Stream successfully initialized!\n");

    if (snd_pcm_prepare(captureHandle.handle)<0)
    {
        fprintf(stderr, "Error: PCM Preparation failed!\n");
        snd_pcm_close(captureHandle.handle);
        return 1;
    }

    // Inint Packet
    index = getSampleRateIndex(captureHandle.samplerate);
    if (index<0)
    {
        fprintf(stderr, "Error: samplerate is not supported by protocol!\n");
        return 1;
    }
    packetDataLen = VBAN_DATA_MAX_SIZE;

    //memset((char*)&stream.packet, 0, sizeof(VBanPacket));
    stream.packet.header.vban = VBAN_HEADER_FOURC;
    stream.packet.header.format_SR = index;
    stream.packet.header.format_bit = stream.format_vban;
    samplesize = VBanBitResolutionSize[stream.packet.header.format_bit];
    framesize = samplesize*captureHandle.nbchannels;
    packetDataLen = stripPacket(stream.packet.header.format_bit, captureHandle.nbchannels);
    packetFrames = packetDataLen/framesize;
    if (packetFrames>captureHandle.nframes)
    {
        packetFrames = captureHandle.nframes;
        packetDataLen = captureHandle.nframes*framesize;
    }
    stream.packet.header.format_nbc = captureHandle.nbchannels - 1;
    stream.packet.header.format_nbs = packetFrames - 1;
    stream.packet.header.nuFrame = 0;
    if (stream.packet.header.streamname[0]==0) strcpy(stream.packet.header.streamname, "Stream1");

    if (strncmp(stream.ipaddr, "pipe", 4)) // using socket
    {
        stream.vban_sd = UDP_init(&stream.vban_sd, &stream.vban_si, &stream.vban_si_other, stream.ipaddr, stream.port, 'c', 1, 6);
        if (stream.vban_sd<0)
        {
            fprintf(stderr, "Can't bind the socket! Maybe, port is busy?\n");
            return 1;
        }
        printf("Socket is successfully created! Port: %d, priority: 6\n", stream.port);
    }
    else // using pipe
    {
        stream.port = 0;
        if (strncmp(stream.pipename, "stdout", 6)) // named pipe
        {
            stream.pipedesc = open(stream.pipename, O_WRONLY);
            mkfifo(stream.pipename, 0666);
        }
        else stream.pipedesc = 1; // stdout
    }

    while(1)
    {
        readframes = snd_pcm_readi(captureHandle.handle, stream.packet.data, packetFrames);
        if (readframes<0)
        {
            snd_pcm_recover(captureHandle.handle, readframes, 0);
            //snd_pcm_prepare(captureHandle.handle);
        }
        else if (readframes<packetFrames)
        {
            fprintf(stderr, "Short read, %d!\n", readframes);
        }
        if (stream.port) // using socket
        {
            packets = stream.redundancy + 1;
            while(packets)
            {
                UDP_send(stream.vban_sd, &stream.vban_si_other, (uint8_t*)&stream.packet, packetDataLen+VBAN_HEADER_SIZE);
                packets--;
            }
        }
        else // using pipe
        {
            write(stream.pipedesc, (char*)&stream.packet, packetDataLen+VBAN_HEADER_SIZE);
            if (stream.pipedesc==STDOUT_FILENO) fflush(stdout);
        }
        inc_nuFrame(&stream.packet.header);
    }

    return 0;
}


int pipe2alsa(int argc, char *argv[])
{
    int index;

    alsaHandle_t playbackHandle;
    streamConfig_t stream;
    uint32_t latency;

    pthread_t rxtid;
    pthread_attr_t attr;
    pthread_mutex_t read_thread_lock = PTHREAD_MUTEX_INITIALIZER;
    pthread_cond_t  data_ready = PTHREAD_COND_INITIALIZER;

    stream.format_vban = VBAN_BITFMT_16_INT;
    stream.redundancy = 2;
    stream.ip = 0;
    stream.port = 0;
    stream.buf = 0;
    stream.buflen = 0;

    playbackHandle.samplerate = 48000;
    playbackHandle.nframes = 128;
    memset(playbackHandle.cardName, 0, CARD_NAME_LENGTH);
    strcpy(playbackHandle.cardName, "default");
    playbackHandle.format = vban_to_alsa_format((VBanBitResolution)stream.format_vban);
    playbackHandle.access = SND_PCM_ACCESS_RW_INTERLEAVED;
    playbackHandle.streamDirection = SND_PCM_STREAM_PLAYBACK;
    playbackHandle.nbchannels = 2;

    // const uint8_t availableSamplerates = 11;
    // const uint32_t samplerates[availableSamplerates] = {22050, 24000, 32000, 44100, 48000, 88200, 96000, 176400, 192000, 352800, 384000};
    // const uint8_t availableBuffersizes = 14;
    // const snd_pcm_uframes_t buffersizes[availableBuffersizes] = {16, 32, 64, 96, 128, 160, 192, 224, 256, 512, 1024, 2048, 4096, 8192};

    memset(stream.ipaddr, 0, 16);
    memset(stream.pipename, 0, 64);
    strncpy(stream.ipaddr, "pipe", 4);
    strncpy(stream.pipename, "stdin", 6);
    stream.port = 0;

    memset(&stream.header, 0, VBAN_HEADER_SIZE);
    stream.header.vban = VBAN_HEADER_FOURC;
    stream.header.format_bit = stream.format_vban;
    stream.header.format_SR = (uint8_t)getSampleRateIndex(playbackHandle.samplerate);
    strcpy(stream.header.streamname, "Stream1");

    get_options(&playbackHandle, &stream, argc, argv);

    stream.header.format_nbc = playbackHandle.nbchannels - 1;

    if (snd_pcm_open(&playbackHandle.handle, playbackHandle.cardName, playbackHandle.streamDirection, 0)<0) // SND_PCM_NONBLOCK SND_PCM_ASYNC
        {
            fprintf(stderr, "Error: cannot open audio device %s!\n", playbackHandle.cardName);
            //snd_pcm_close(playbackHandle);
            return 1;
        }
    fprintf(stderr, "Audio device successfully opened\n");
    if (snd_pcm_hw_params_malloc(&playbackHandle.hwParams)<0)
    {
        fprintf (stderr, "Error: cannot allocate hardware parameter structure!\n");
        snd_pcm_close(playbackHandle.handle);
        return 1;
    }
    if (snd_pcm_hw_params_any(playbackHandle.handle, playbackHandle.hwParams)<0)
    {
        fprintf(stderr, "Error: cannot initialize hw parameters!\n");
        snd_pcm_close(playbackHandle.handle);
        return 1;
    }

    // Get parameters of choosen card
    // Channels number
    if (snd_pcm_hw_params_get_channels_min(playbackHandle.hwParams, &playbackHandle.nbchannelsmin)<0)
    {
        fprintf (stderr, "Error: cannot get minimum outputs number!\n");
        snd_pcm_close(playbackHandle.handle);
        return 1;
    }
    if (snd_pcm_hw_params_get_channels_max(playbackHandle.hwParams, &playbackHandle.nbchannelsmax)<0)
    {
        fprintf (stderr, "Error: cannot get maximum outputs number!\n");
        snd_pcm_close(playbackHandle.handle);
        return 1;
    }
    fprintf(stderr, "Number of outputs available: from %d to %d\n", playbackHandle.nbchannelsmin, playbackHandle.nbchannelsmax);

    // Samplerate
    if (snd_pcm_hw_params_get_rate_min(playbackHandle.hwParams, &playbackHandle.samplerateMin, 0)<0)
    {
        fprintf(stderr, "Error: cannot get minimum sample rate!\n");
        snd_pcm_close(playbackHandle.handle);
        return 1;
    }
    if (snd_pcm_hw_params_get_rate_max(playbackHandle.hwParams, &playbackHandle.samplerateMax, 0)<0)
    {
        fprintf(stderr, "Error: cannot get maximum sample rate!\n");
        snd_pcm_close(playbackHandle.handle);
        return 1;
    }

    // Buffersize
    if (snd_pcm_hw_params_get_buffer_size_min(playbackHandle.hwParams, &playbackHandle.nframesMin)<0)
    {
        fprintf(stderr, "Minimum buffer size is unavailable!\n");
        snd_pcm_close(playbackHandle.handle);
        return 1;
    }
    if (snd_pcm_hw_params_get_buffer_size_max(playbackHandle.hwParams, &playbackHandle.nframesMax)<0)
    {
        fprintf(stderr, "Maximum buffer size is unavailable!\n");
        snd_pcm_close(playbackHandle.handle);
        return 1;
    }
    fprintf(stderr, "Minimum samplerate: %d, maximum: %d\n", playbackHandle.samplerateMin, playbackHandle.samplerateMax);
    fprintf(stderr, "Minimum buffer size: %lu, maximum: %lu\n", playbackHandle.nframesMin, playbackHandle.nframesMax);


    // Apply parameters
    // Channels number
    if (playbackHandle.nbchannels>playbackHandle.nbchannelsmax)
    {
        fprintf(stderr, "Error: Number of requested channels is more than available!\n");
        snd_pcm_close(playbackHandle.handle);
        return 1;
    }
    if (playbackHandle.nbchannels<playbackHandle.nbchannelsmin)
    {
        fprintf(stderr, "Error: Number of requested channels is less than available!\n");
        snd_pcm_close(playbackHandle.handle);
        return 1;
    }

    // Samplerate
    if (playbackHandle.samplerate<playbackHandle.samplerateMin)
    {
        for (index=0; index<availableSamplerates; index++)
            if (samplerates[index]>playbackHandle.samplerateMin)
            {
                playbackHandle.samplerate = samplerates[index];
                fprintf(stderr, "Warning: requested samplerate is lower than minimum available! Trying to set %d...\n", playbackHandle.samplerate);
                break;
            }
    }
    else if (playbackHandle.samplerate>playbackHandle.samplerateMax)
    {
        for (index=availableSamplerates-1; index<=0; index--)
            if (samplerates[index]<playbackHandle.samplerateMax)
            {
                playbackHandle.samplerate = samplerates[index];
                fprintf(stderr, "Warning: requested samplerate is higher than minimum available! Trying to set %d...\n", playbackHandle.samplerate);
                break;
            }
    }
    else fprintf(stderr, "Samplerate choosen: %d...\n", playbackHandle.samplerate);

    if (stream.redundancy<2)
    {
        fprintf(stderr, "Warning: number of periods %d is incorrect! Using value 2!\n", stream.redundancy);
        stream.redundancy = 2;
    }
    if ((playbackHandle.nframes*stream.redundancy)<playbackHandle.nframesMin)
    {
        for (index=0; index<availableBuffersizes; index++)
            if (buffersizes[index]>playbackHandle.nframesMin)
            {
                playbackHandle.nframes = buffersizes[index]/stream.redundancy;
                fprintf(stderr, "Warning: requested buffer size is lower than minimum available! Trying to set %lu...\n", playbackHandle.nframes);
                break;
            }
    }
    else if ((playbackHandle.nframes*stream.redundancy)>playbackHandle.nframesMax)
    {
        for (index=availableBuffersizes-1; index>=0; index--)
            if (buffersizes[index]<playbackHandle.nframesMax)
            {
                playbackHandle.nframes = buffersizes[index]/stream.redundancy;
                fprintf(stderr, "Warning: requested buffer size is higher than maximum available! Trying to set %lu...\n", playbackHandle.nframes);
                break;
            }
    }

    if (snd_pcm_hw_params_set_period_size(playbackHandle.handle, playbackHandle.hwParams, playbackHandle.nframes, playbackHandle.streamDirection)<0)
    {
        fprintf(stderr, "Error: cannot set size of periods!\n");
        snd_pcm_close(playbackHandle.handle);
        return 1;
    }
    fprintf(stderr, "Period size: %lu\n", playbackHandle.nframes);
    if (snd_pcm_hw_params_set_periods(playbackHandle.handle, playbackHandle.hwParams, stream.redundancy, playbackHandle.streamDirection)<0)
    {
        fprintf(stderr, "Error: cannot set number of periods!\n");
        snd_pcm_close(playbackHandle.handle);
        return 1;
    }
    fprintf(stderr, "Number of periods: %u\nTotal buffer size: %lu\n", stream.redundancy, playbackHandle.nframes*stream.redundancy);

    // Final Init
    latency = 1000000*playbackHandle.nframes*stream.redundancy/playbackHandle.samplerate;
    if (snd_pcm_set_params(playbackHandle.handle,
                           playbackHandle.format,
                           playbackHandle.access,
                           playbackHandle.nbchannels,
                           playbackHandle.samplerate,
                           1, // Resampler enabled
                           latency)<0)
    {
        fprintf(stderr, "Error: cannot initialize stream!\n");
        snd_pcm_close(playbackHandle.handle);
        return 1;
    }
    fprintf(stderr, "Stream successfully initialized!\n");

    stream.nframes = playbackHandle.nframes;
    stream.ringbuffersize = playbackHandle.nframes*playbackHandle.nbchannels*VBanBitResolutionSize[(VBanBitResolution)stream.format_vban];
    if (stream.ringbuffersize<stripPacket(stream.format_vban, playbackHandle.nbchannels)) stream.ringbuffersize = stripPacket(stream.format_vban, playbackHandle.nbchannels);
    stream.ringbuffersize*= 2; // TODO: MUST BE PARAMETER INSTEAD OF CONST 2 !!!
    stream.ringbuffer = jack_ringbuffer_create(stream.ringbuffersize);

    if (strncmp(stream.ipaddr, "pipe", 4)) // Network
    {
        stream.vban_sd = UDP_init(&stream.vban_sd, &stream.vban_si, &stream.vban_si_other, stream.ipaddr, stream.port, 's', 1, 6);
        if (stream.vban_sd<0)
        {
            fprintf(stderr, "Error: cannot bind socket!");
            return 1;
        }
        fprintf(stderr, "Socket created, %d\n", stream.vban_sd);
        stream.polldesc.fd = stream.vban_sd;
        stream.polldesc.events = POLLIN;

        while (poll(&stream.polldesc, 1, 0)) UDP_recv(stream.vban_sd, &stream.vban_si, NULL, VBAN_PROTOCOL_MAX_SIZE); //flush socket
    }
    else // Pipe
    {
        stream.port = 0;
        if (strncmp(stream.pipename, "stdin", 6)>0) // named pipe
        {
            stream.pipedesc = open(stream.pipename, O_RDONLY);
            mkfifo(stream.pipename, 0666);
        }
        else stream.pipedesc = 0; // stdin
        stream.polldesc.fd = stream.pipedesc;
        stream.polldesc.events = POLLIN;
    }


    if (snd_pcm_prepare(playbackHandle.handle)<0)
    {
        fprintf(stderr, "Error: PCM Preparation failed!\n");
        snd_pcm_close(playbackHandle.handle);
        return 1;
    }

    stream.buflen = playbackHandle.nframes*playbackHandle.nbchannels*VBanBitResolutionSize[stream.format_vban];
    stream.buf = (char*)malloc(stream.buflen*2);
    memset(stream.buf, 0, stream.buflen);

    snd_async_handler_t *ahandler;
    if (snd_async_add_pcm_handler(&ahandler, playbackHandle.handle, pbCallback, (void*)&stream) < 0) //
    {
        printf("Unable to register async handler\n");
        exit(EXIT_FAILURE);
    }//*/

    for (index=0; index<stream.redundancy; index++)
    {
        if (snd_pcm_writei(playbackHandle.handle, stream.buf, stream.buflen) < 0) //playbackHandle.nframes
        {
            printf("Initial write error\n");
            exit(EXIT_FAILURE);
        }
    }

    if (snd_pcm_state(playbackHandle.handle) == SND_PCM_STATE_PREPARED)
    {
        if (snd_pcm_start(playbackHandle.handle) < 0)
        {
            printf("Start error\n");
            exit(EXIT_FAILURE);
        }
    }

    stream.additionals = &playbackHandle; // TEMPORARY!!!
    pthread_attr_init(&attr);
    pthread_create(&rxtid, &attr, rxThread, (void*)&stream);

    while(1) sleep(1);

    return 0;
}
