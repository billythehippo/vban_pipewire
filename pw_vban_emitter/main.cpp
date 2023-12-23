#include "../vban_common/pipewire_backend.h"
#include "../vban_common/vban_functions.h"
#include "../vban_common/udpsocket.h"
#include "../vban_common/pipewire_backend.h"
//#include "../vban_common/ringbuffer.h"

// КОСТЫЛИ
// {


// }


static void on_process(void *userdata)
{
    struct data *data = (struct data*)userdata;
    struct pw_buffer *b;
    struct spa_buffer *buf;
    char* samples_ptr;
    uint16_t n;
    uint16_t n_channels;
    uint16_t n_samples;
    uint32_t n_frames;

    if ((b = pw_stream_dequeue_buffer(data->stream)) == NULL)
    {
        pw_log_warn("out of buffers: %m");
        return;
    }

    buf = b->buffer;
    samples_ptr = (char*)(b->buffer->datas[0].data);
    if (samples_ptr == NULL)
        return;

    n_channels = data->format.info.raw.channels;
    n_samples = buf->datas[0].chunk->size / VBanBitResolutionSize[data->config->format_vban];
//    memcpy(data->config->buf, samples_ptr, n_channels*buf->datas[0].chunk->size);

    n_frames = n_samples/n_channels;
    //fprintf(stderr, "captured %d samples\n", n_frames);
    if (n_frames!=data->config->nframes)
    {
        data->config->nframes = n_frames;
        compute_packets_and_bufs(data->config);
        fprintf(stderr, "Warning: buffer size is changed to %d!\n", n_frames);
    }

    for(n=0; n<data->config->packetnum; n++)
    {
        memcpy(data->config->packet.data, samples_ptr+n*data->config->packetdatalen, data->config->packetdatalen);
        for (uint8_t r=0; r<=data->config->redundancy; r++)
        {
            if (data->config->port) UDP_send(data->config->vban_sd, &data->config->vban_si_other, (uint8_t*)&data->config->packet, data->config->packetdatalen + VBAN_HEADER_SIZE);
            else write(data->config->pipedesc, &data->config->packet, data->config->packetdatalen + VBAN_HEADER_SIZE);
            data->config->packet.header.nuFrame++;
        }
    }

    pw_stream_queue_buffer(data->stream, b);
}
 
/* Be notified when the stream param changes. We're only looking at the
 * format changes.
 */
static void on_stream_param_changed(void *_data, uint32_t id, const struct spa_pod *param)
{
        struct data *data = (struct data *)_data;
 
        /* NULL means to clear the format */
        if (param == NULL || id != SPA_PARAM_Format)
                return;
 
        if (spa_format_parse(param, &data->format.media_type, &data->format.media_subtype) < 0)
                return;
 
        /* only accept raw audio */
        if (data->format.media_type != SPA_MEDIA_TYPE_audio ||
            data->format.media_subtype != SPA_MEDIA_SUBTYPE_raw)
                return;
 
        /* call a helper function to parse the format for us. */
        spa_format_audio_raw_parse(param, &data->format.info.raw);
 
        fprintf(stderr, "capturing rate:%d channels:%d\n",
                        data->format.info.raw.rate, data->format.info.raw.channels);
 
}


static const struct pw_stream_events stream_events = {
        PW_VERSION_STREAM_EVENTS,
        .param_changed = on_stream_param_changed,
        .process = on_process,
};


static void do_quit(void *userdata, int signal_number)
{
        struct data *data = (struct data *)userdata;
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

        memset(tmpstring, 0, 16);
        data.config = &config;
        memset((char*)&config, 0, sizeof(streamConfig_t));
        config.nframes = 128;
        config.samplerate = 48000;
        config.nbchannels = 2;
        config.format_vban = 1;// S16

        config.packet.header.vban = VBAN_HEADER_FOURC;
        config.packet.header.format_bit = config.format_vban;
        config.packet.header.format_SR = getSampleRateIndex(config.samplerate);
        config.packet.header.format_nbc = config.nbchannels - 1;
        config.packet.header.format_nbs = 0;
        strcpy(config.packet.header.streamname, "Stream1");
        strcpy(config.ipaddr, "pipe");
        strcpy(config.pipename, "stdout");

        get_options(&config, argc, argv);

        config.format = format_vban_to_spa((enum VBanBitResolution)config.format_vban);
        config.packet.header.format_bit = config.format_vban;
        config.packet.header.format_SR = getSampleRateIndex(config.samplerate);
        config.packet.header.format_nbc = config.nbchannels - 1;

        spa_pod_builder_init(&b, buffer, sizeof(buffer));
        pw_init(NULL, NULL); //pw_init(&argc, &argv);
 
        data.loop = pw_main_loop_new(NULL);
 
        pw_loop_add_signal(pw_main_loop_get_loop(data.loop), SIGINT, do_quit, &data);
        pw_loop_add_signal(pw_main_loop_get_loop(data.loop), SIGTERM, do_quit, &data);

        sprintf(tmpstring, "%d/%d", config.nframes, config.samplerate);
        props = pw_properties_new(PW_KEY_MEDIA_TYPE, "Audio",
                                  PW_KEY_MEDIA_CATEGORY, "Capture",
                                  PW_KEY_MEDIA_ROLE, "Music",
                                  PW_KEY_NODE_LATENCY, tmpstring, // nframes/samplerate
                                  NULL);


        //char* prop = (char*)pw_properties_get(props, PW_KEY_NODE_LATENCY);
        //if ((prop==NULL)&&(config.nframes!=0)) pw_properties_set(props, PW_KEY_NODE_LATENCY, tmpstring);//*/
 
        data.stream =   pw_stream_new_simple(
                        pw_main_loop_get_loop(data.loop),
                        config.packet.header.streamname, // Name of client
                        props,
                        &stream_events,
                        &data);

        audio_info.format = format_vban_to_spa((enum VBanBitResolution)config.format_vban);//(enum spa_audio_format)config.format;
        audio_info.channels = config.packet.header.format_nbc + 1;
        params[0] = spa_format_audio_raw_build(&b, SPA_PARAM_EnumFormat, &audio_info);
 
        pw_stream_connect(data.stream,
                          PW_DIRECTION_INPUT,
                          PW_ID_ANY,
                          //PW_STREAM_FLAG_AUTOCONNECT |
                          //PW_STREAM_FLAG_MAP_BUFFERS | PW_STREAM_FLAG_RT_PROCESS,
                          (pw_stream_flags)((1 << 2)|(1 << 4)),
                          params, 1);

        //compute_packets_and_bufs(&config);

        if (strcmp(config.ipaddr, "pipe")) // socket mode
        {
            config.vban_sd = UDP_init(&config.vban_sd, &config.vban_si, &config.vban_si_other, config.ipaddr, config.port, 'c', 1, 6);
            if (config.vban_sd<0)
            {
                fprintf(stderr, "Can't bind the socket! Maybe, port is busy?\n");
                return 1;
            }
            printf("Socket is successfully created! Port: %d, priority: 6\n", config.port);
        }
        else // pipe mode
        {
            config.port = 0;
            if (strncmp(config.pipename, "stdout", 6)) // named pipe
            {
                config.pipedesc = open(config.pipename, O_WRONLY);
                mkfifo(config.pipename, 0666);
            }
            else config.pipedesc = 1; // stdout
        }

        pw_main_loop_run(data.loop);
 
        pw_stream_destroy(data.stream);
        pw_main_loop_destroy(data.loop);
        pw_deinit();
 
        return 0;
}
