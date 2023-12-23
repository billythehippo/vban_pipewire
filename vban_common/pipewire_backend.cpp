#include "pipewire_backend.h"


enum spa_audio_format format_vban_to_spa(enum VBanBitResolution format_vban)
{
    switch (format_vban)
    {
    case VBAN_BITFMT_8_INT:
        return SPA_AUDIO_FORMAT_S8;
    case VBAN_BITFMT_16_INT:
        return SPA_AUDIO_FORMAT_S16_LE;
    case VBAN_BITFMT_24_INT:
        return SPA_AUDIO_FORMAT_S24_LE;
    case VBAN_BITFMT_32_INT:
        return SPA_AUDIO_FORMAT_S32_LE;
    case VBAN_BITFMT_32_FLOAT:
        return SPA_AUDIO_FORMAT_F32_LE;
    case VBAN_BITFMT_64_FLOAT:
        return SPA_AUDIO_FORMAT_F64_LE;
    default:
        return SPA_AUDIO_FORMAT_F32_LE;
    }
}


void help(void)
{
    fprintf(stderr, "VBAN Pipewire clients for network and pipes/fifos\n\nBy Billy the Hippo\n\nusage: <exec_name> <args>\nexec_name is pw_vban_emitter or pw_vban_receptor\n\n");
    //fprintf(stderr, "-m - mode: rx - net/fifo to audio, tx (default) - audio to net/fifo\n");
    //fprintf(stderr, "-d - device name (for ALSA without \"hw:\", for others - see manuals)\n");
    fprintf(stderr, "-s - samplerate (default 48000)\n");
    fprintf(stderr, "-q - quantum, buffer size (Attention!!! Default is 128!!! Made for musicians.)\n");
    fprintf(stderr, "-c - number of channels\n");
    fprintf(stderr, "-n - redundancy 1 to 10 (in rx mode - as \"net quality\", it tx mode not implemented yet)\n");
    fprintf(stderr, "-f - format: 16, 24, 32f\n");
    fprintf(stderr, "-i - ip address or pipe name\n");
    fprintf(stderr, "-p - ip port (if 0 - pipe)\n");
    fprintf(stderr, "-s - Stream name, up to symbols\n");
    fprintf(stderr, "-h - show this help\n");
    exit(0);
}


// int get_options(streamConfig_t* stream, int argc, char *argv[])
// {
//     int index;
//     char c;
//     static const struct option options[] =
//         {
//             {"device",      required_argument,  0, 'd'},
//             {"samplerate",  required_argument,  0, 'r'},
//             {"bufsize",     required_argument,  0, 'q'},
//             {"nbchannels",  required_argument,  0, 'c'},
//             {"redundancy",  required_argument,  0, 'n'},
//             {"format",      required_argument,  0, 'f'},
//             {"ipaddr",      required_argument,  0, 'i'},
//             {"port",        required_argument,  0, 'p'},
//             {"streamname",  required_argument,  0, 's'},
//             {"help",        no_argument,        0, 'h'},
//             {0,             0,                  0,  0 }
//         };
//     int ipnums[4];

//     c = getopt_long(argc, argv, "d:r:q:c:n:f:i:p:s:h", options, 0);
//     if (c==-1)
//     {
//         help();
//         exit(0);
//     }
//     while(c!=-1)
//     {
//         switch (c)
//         {
//         case 'd':
//             // ???
//             break;

//         case 'r':
//             stream->samplerate = atoi(optarg);
//             break;
//         case 'q':
//             stream->nframes = atoi(optarg);
//             break;
//         case 'c':
//             stream->nbchannels = atoi(optarg);
//             //packet.header.format_nbc = captureHandle.nbchannels - 1;
//             break;
//         case 'n':
//             stream->redundancy = atoi(optarg);
//             break;
//         case 'f':
//             if ((strncmp(optarg, "16I", 3)==0)|(strncmp(optarg, "16i", 3)==0)) stream->format_vban = VBAN_BITFMT_16_INT;
//             else if ((strncmp(optarg, "24I", 3)==0)|(strncmp(optarg, "24i", 3)==0)) stream->format_vban = VBAN_BITFMT_24_INT;
//             else if ((strncmp(optarg, "32F", 3)==0)|(strncmp(optarg, "32f", 3)==0)) stream->format_vban = VBAN_BITFMT_32_FLOAT;
//             stream->format = format_vban_to_spa((enum VBanBitResolution)stream->format_vban);
//             break;
//         case 'i':
//             memset(stream->ipaddr, 0, 16);
//             if (strlen(optarg)<16) strcpy(stream->ipaddr, optarg);
//             else strncpy(stream->ipaddr, optarg, 16);
//             if(strncmp(stream->ipaddr, "pipe", 4))
//             {
//                 sscanf(stream->ipaddr, "%d.%d.%d.%d", &ipnums[0], &ipnums[1], &ipnums[2], &ipnums[3]);
//                 if ((ipnums[0]<0)|(ipnums[1]<0)|(ipnums[2]<0)|(ipnums[3]<0)|(ipnums[0]>255)|(ipnums[1]>255)|(ipnums[2]>255)|(ipnums[3]>255))
//                 {
//                     fprintf(stderr, "Error: incorrect IP address!\n");
//                     return 1;
//                 }
//                 else
//                 {
//                     //for(index=0; index<4; index++) ((uint8_t*)&stream->ip)[index] = (uint8_t)ipnums[index];
//                     if (stream->port==0) stream->port = 6980;
//                 }
//             }
//             else fprintf(stderr, "Using pipe!\n");
//             break;
//         case 'p':
//             if(strncmp(stream->ipaddr, "pipe", 4)) stream->port = atoi(optarg);
//             else
//             {
//                 memset(stream->pipename, 0, 64);
//                 if (strlen(optarg)<64) strcpy(stream->pipename, optarg);
//                 else strncpy(stream->pipename, optarg, 64);
//             }
//             break;
//         case 's':
//             if (optarg[0]==0)
//             {
//                 fprintf(stderr, "No stream name given!\n");
//                 return 1;
//             }
//             else
//             {
//                 if (stream->flags&CAPTURE) // CAPTURE
//                 {
//                     memset(stream->packet.header.streamname, 0, VBAN_STREAM_NAME_SIZE);
//                     if (strlen(optarg)<VBAN_STREAM_NAME_SIZE) strcpy(stream->packet.header.streamname, optarg);
//                     else strncpy(stream->packet.header.streamname, optarg, VBAN_STREAM_NAME_SIZE-1);
//                 }
//                 else if (stream->flags&PLAYBACK) // PLAYBACK
//                 {
//                     memset(stream->header.streamname, 0, VBAN_STREAM_NAME_SIZE);
//                     if (strlen(optarg)<VBAN_STREAM_NAME_SIZE) strcpy(stream->header.streamname, optarg);
//                     else strncpy(stream->header.streamname, optarg, VBAN_STREAM_NAME_SIZE-1);
//                 }
//             }//*/
//             break;
//         }
//         c = getopt_long(argc, argv, "d:r:q:c:n:f:i:p:s:h", options, 0);
//     }
//     return 0;
// }

