#include <arpa/inet.h>
#include "../vban_common/jack_backend.h"
#include "../vban_common/udp.h"
#include "../vban_common/vban_client_list.h"

int main(int argc, char *argv[])
{
    vban_stream_context_t stream;

    memset(&stream, 0, sizeof(vban_stream_context_t));

    stream.txport = 6980;
    stream.vban_output_format = VBAN_BITFMT_32_FLOAT;
    stream.samplerate = 48000;
    stream.nbinputs = 2;

    if (get_emitter_options(&stream, argc, argv)) return 1;

    if (stream.txport!=0) // UDP tx mode
    {
        stream.txsock = udp_init(0, stream.txport, NULL, stream.iptxaddr, 0, 6, 1);
        if (stream.txsock==NULL)
        {
            fprintf(stderr, "Cannot init UDP socket!\r\n");
            return 1;
        }
        set_recverr(stream.txsock->fd);
        stream.pd[0].fd = stream.txsock->fd;
    }
    else // PIPE tx mode
    {
        if (strncmp(stream.pipename, "stdin", 6)) // named pipe
        {
            stream.pipedesc = open(stream.pipename, O_WRONLY);
            mkfifo(stream.pipename, 0666);
        }
        else stream.pipedesc = 0; // stdin
        stream.pd[0].fd = stream.pipedesc;
    }

    //Create stream
    stream.txpacket.header.vban = VBAN_HEADER_FOURC;
    stream.txpacket.header.format_SR = VBAN_SR_MASK&vban_get_format_SR(stream.samplerate);
    stream.txpacket.header.format_bit = stream.vban_output_format;
    stream.txpacket.header.format_nbc = stream.nbinputs - 1;
    stream.txpacket.header.format_nbs = stream.vban_nframes_pac - 1;
    strncpy(stream.txpacket.header.streamname, stream.tx_streamname, (strlen(stream.tx_streamname)>16 ? 16 : strlen(stream.tx_streamname)));

    stream.txmidipac.header.vban = VBAN_HEADER_FOURC;
    stream.txmidipac.header.format_SR = VBAN_PROTOCOL_SERIAL&11;
    strncpy(stream.txmidipac.header.streamname, stream.tx_streamname, (strlen(stream.tx_streamname)>16 ? 16 : strlen(stream.tx_streamname)));

    stream.flags|= TRANSMITTER;
    jack_stream_data_t jack_stream;
    memset(&jack_stream, 0, sizeof(jack_stream_data_t));
    jack_stream.user_data = (void*)&stream;

    jack_init_tx_stream(&jack_stream);

    jack_run_tx_stream(&jack_stream);

    while(1) sleep(1);

    return 0;
}
