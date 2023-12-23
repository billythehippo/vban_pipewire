#include "vban_functions.h"


void compute_packets_and_bufs(streamConfig_t* config)
{
    uint8_t samplesize = VBanBitResolutionSize[config->format_vban];
    uint16_t framesize = samplesize*config->nbchannels;
    uint32_t buflen = framesize*config->nframes;
    uint32_t packetdatalen = framesize*(config->header.format_nbs + 1);
    free(config->buf);
    config->buf = (char*)malloc(buflen);
    if (config->flags&PLAYBACK)
    {
        if (config->ringbuffer!=0) jack_ringbuffer_free(config->ringbuffer);
        config->ringbuffersize = (packetdatalen > buflen ? packetdatalen : buflen)*2*(1+config->redundancy);
        config->ringbuffer = jack_ringbuffer_create(config->ringbuffersize);
        if (config->redundancy)
        {
            char* zeros = (char*)calloc(1, config->ringbuffersize>>2);
            jack_ringbuffer_write(config->ringbuffer, zeros, config->ringbuffersize>>2);
            free(zeros);
        }
        // else
        // {
        //     char* zeros = (char*)calloc(1, config->ringbuffersize>>1);
        //     jack_ringbuffer_write(config->ringbuffer, zeros, config->ringbuffersize>>1);
        //     free(zeros);
        // }
    }
    config->buflen = buflen;
    config->packetdatalen = (packetdatalen > buflen ? packetdatalen : buflen);
    config->packetnum = 1;
    while((config->packetdatalen>VBAN_DATA_MAX_SIZE)||((config->packetdatalen/framesize)>256))
    {
        config->packetdatalen = config->packetdatalen>>1;
        config->packetnum = config->packetnum<<1;
    }
    config->packet.header.format_nbs = config->packetdatalen/framesize - 1;
}


int get_options(streamConfig_t* stream, int argc, char *argv[])
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

    c = getopt_long(argc, argv, "d:r:q:c:n:f:i:p:s:b:h", options, 0);
    if (c==-1)
    {
        //help();
        return 1;
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
                memset(stream->cardname, 0, CARD_NAME_LENGTH);
                if (strlen(optarg)<CARD_NAME_LENGTH) strcpy(stream->cardname, optarg);
                else strncpy(stream->cardname, optarg, CARD_NAME_LENGTH-1);
            }
            break;

        case 'r':
            stream->samplerate = atoi(optarg);
            break;
        case 'q':
            stream->nframes = atoi(optarg);
            break;
        case 'c':
            stream->nbchannels = atoi(optarg);
            break;
        case 'n':
            stream->redundancy = atoi(optarg);
            break;
        case 'f':
            if ((strncmp(optarg, "16I", 3)==0)|(strncmp(optarg, "16i", 3)==0)) stream->format_vban = VBAN_BITFMT_16_INT;
            else if ((strncmp(optarg, "24I", 3)==0)|(strncmp(optarg, "24i", 3)==0)) stream->format_vban = VBAN_BITFMT_24_INT;
            else if ((strncmp(optarg, "32F", 3)==0)|(strncmp(optarg, "32f", 3)==0)) stream->format_vban = VBAN_BITFMT_32_FLOAT;
            //stream->format = format_vban_to_spa((enum VBanBitResolution)stream->format_vban);
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
                    //for(index=0; index<4; index++) ((uint8_t*)&stream->ip)[index] = (uint8_t)ipnums[index];
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
                if (stream->flags&CAPTURE) // CAPTURE
                {
                    memset(stream->packet.header.streamname, 0, VBAN_STREAM_NAME_SIZE);
                    if (strlen(optarg)<VBAN_STREAM_NAME_SIZE) strcpy(stream->packet.header.streamname, optarg);
                    else strncpy(stream->packet.header.streamname, optarg, VBAN_STREAM_NAME_SIZE-1);
                }
                else if (stream->flags&PLAYBACK) // PLAYBACK
                {
                    memset(stream->header.streamname, 0, VBAN_STREAM_NAME_SIZE);
                    if (strlen(optarg)<VBAN_STREAM_NAME_SIZE) strcpy(stream->header.streamname, optarg);
                    else strncpy(stream->header.streamname, optarg, VBAN_STREAM_NAME_SIZE-1);
                }
            }//*/
            break;
        case 'b':
            if ((strncmp(optarg, "alsa", 4)==0)||(strncmp(optarg, "ALSA", 4)==0))
            {
                stream->apiindex = 1;
            }
            else if ((strncmp(optarg, "pulse", 5)==0)||(strncmp(optarg, "PULSE", 5)==0))
            {
                stream->apiindex = 2;
            }
            else if ((strncmp(optarg, "jack", 4)==0)||(strncmp(optarg, "JACK", 4)==0))
            {
                stream->apiindex = 4;
            }
            else if ((strncmp(optarg, "coreaudio", 9)==0)||(strncmp(optarg, "COREAUDIO", 9)==0))
            {
                stream->apiindex = 5;
            }
            else if ((strncmp(optarg, "wasapi", 6)==0)||(strncmp(optarg, "WASAPI", 6)==0))
            {
                stream->apiindex = 6;
            }
            else if ((strncmp(optarg, "asio", 4)==0)||(strncmp(optarg, "ASIO", 4)==0))
            {
                stream->apiindex = 7;
            }
            break;
        }
        c = getopt_long(argc, argv, "d:r:q:c:n:f:i:p:s:b:h", options, 0);
    }
    return 0;
}
