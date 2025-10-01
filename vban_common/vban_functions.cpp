#include "vban_functions.h"
#include <signal.h>


void tune_tx_packets(vban_stream_context_t* stream)
{
    stream->pacnum = 1;
    stream->vban_nframes_pac = stream->nframes;
    stream->txbuflen = stream->nframes * stream->nbinputs * VBanBitResolutionSize[stream->vban_output_format];
    if (stream->txbuf!=NULL) free(stream->txbuf);
    stream->txbuf = (char*)malloc(stream->txbuflen);
    memset(stream->txbuf, 0, stream->txbuflen);
    stream->pacdatalen = stream->txbuflen;
    while((stream->pacdatalen > VBAN_DATA_MAX_SIZE)||(stream->vban_nframes_pac > VBAN_SAMPLES_MAX_NB))
    {
        stream->pacnum = stream->pacnum * 2;
        stream->vban_nframes_pac = stream->vban_nframes_pac / 2;
        stream->pacdatalen = stream->pacdatalen / 2;
    }
    stream->txpacket.header.format_nbs = stream->vban_nframes_pac - 1;
}


void vban_fill_receptor_info(vban_stream_context_t* context)
{
    memset(&context->info, 0, VBAN_PROTOCOL_MAX_SIZE);
    context->info.header.vban = VBAN_HEADER_FOURC;
    context->info.header.format_SR = VBAN_PROTOCOL_TXT;
    strcpy(context->info.header.streamname, "INFO");
    if (context->flags&MULTISTREAM)
    {
        sprintf(context->info.data, "servername=%s ", context->rx_streamname);
    }
    else
    {
        sprintf(context->info.data, "streamnamerx=%s nbchannels=%d ", context->rx_streamname, context->nboutputs);
    }
    sprintf(context->info.data+strlen(context->info.data), "samplerate=%d format=%d flags=%d", context->samplerate, context->vban_output_format, context->flags);
}


void* timerThread(void* arg)
{
    vban_multistream_context_t* context = (vban_multistream_context_t*)arg;
    //client_id_t* clients = (client_id_t*)arg;
    client_id_t* client = NULL;
    // client_id_t* next = NULL;
    pid_t pidtokill;
    uint index;

    while((*context->flags&RECEIVING)==RECEIVING) // Thread is enabled cond
    {
        usleep(50000);
        client = context->clients;
        if (context->active_clients_num)
            for (index=0; index<context->active_clients_num; index++)
            {
                if (client->timer==4)
                {
                    if (index==0) // Remove 1-st
                    {
                        pidtokill = context->clients->pid;
                        pop(&context->clients);
                    }
                    else
                    {
                        pidtokill = client->pid;
                        if (index==context->active_clients_num) remove_last(context->clients);
                        else remove_by_index(&context->clients, index);
                    }
                    kill(pidtokill, SIGINT);
                    pclose2(pidtokill);
                    context->active_clients_num--;
                    break;
                }
                else client->timer++;
                //if (active_clients_num!=0)
                client = client->next;
            }
    }
}


void* rxThread(void* arg)
{
    vban_stream_context_t* stream = (vban_stream_context_t*)arg;
    VBanPacket packet;
    int packetlen;
    int datalen;
    uint32_t ip_in = 0;
    //uint16_t port_in = 0;

    fprintf(stderr, "rxThread started\r\n");

    while((stream->flags&RECEIVING)==RECEIVING)
    {
        while ((poll(stream->pd, 1, 100))&&((stream->flags&RECEIVING)==RECEIVING))
        {
            if (stream->rxport!=0) // UDP
            {
                packetlen = udp_recv(stream->rxsock, &packet, VBAN_PROTOCOL_MAX_SIZE);
                ip_in = stream->rxsock->c_addr.sin_addr.s_addr;
                stream->ansport = htons(stream->rxsock->c_addr.sin_port);
                if (((packet.header.format_SR&VBAN_PROTOCOL_MASK)==VBAN_PROTOCOL_AUDIO)||((packet.header.format_SR&VBAN_PROTOCOL_MASK)==VBAN_PROTOCOL_SERIAL))
                {
                    if (stream->iprx==0) //stream->iprx = ip_in;
                    {
                        if (strncmp(stream->rx_streamname, packet.header.streamname, VBAN_STREAM_NAME_SIZE)==0) stream->iprx = ip_in;
                        else if (stream->rx_streamname[0]==0)
                        {
                            strncpy(stream->rx_streamname, packet.header.streamname, VBAN_STREAM_NAME_SIZE);
                            stream->iprx = ip_in;
                        }
                    }
                    else if ((stream->iprx==ip_in)&&(stream->rx_streamname[0]==0)) strncpy(stream->rx_streamname, packet.header.streamname, VBAN_STREAM_NAME_SIZE);
                }
            }
            else
            {
                packetlen = read(stream->pd[0].fd, &packet, VBAN_HEADER_SIZE);
                if (((packet.header.format_SR&VBAN_PROTOCOL_MASK)==VBAN_PROTOCOL_AUDIO)||((packet.header.format_SR&VBAN_PROTOCOL_MASK)==VBAN_PROTOCOL_TXT))
                {
                    datalen = VBanBitResolutionSize[packet.header.format_bit&VBAN_BIT_RESOLUTION_MASK]*(packet.header.format_nbc+1)*(packet.header.format_nbs+1);
                    if (datalen==read(stream->pd[0].fd, packet.data, datalen)) packetlen+= datalen;
                    if (stream->rx_streamname[0]==0) strncpy(stream->rx_streamname, packet.header.streamname, VBAN_STREAM_NAME_SIZE);
                }
            }
            if ((packetlen>=VBAN_HEADER_SIZE)&&(packet.header.vban==VBAN_HEADER_FOURC))
            {
                vban_rx_handle_packet(&packet, packetlen, stream, ip_in, stream->ansport);
                memset(&packet, 0, sizeof(VBanPacket));
            }
        }
    }

    fprintf(stderr, "RX thread stopped\r\n");
    return NULL;
}
