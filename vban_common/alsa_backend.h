#ifndef ALSA_BACKEND_H
#define ALSA_BACKEND_H

#include <alsa/asoundlib.h>
#include "../vban_common/vban_functions.h"
#include "udp.h"


extern snd_pcm_format_t vban_to_alsa_format_table[];

typedef struct {
    char capture_card_name[CARD_NAME_LENGTH];
    char playback_card_name[CARD_NAME_LENGTH];
    uint16_t capbuflen;
    uint16_t playbuflen;
    snd_pcm_t *capture_handle;
    snd_pcm_t *playback_handle;
    snd_pcm_hw_params_t *capture_hw_params;
    snd_pcm_sw_params_t *capture_sw_params;
    snd_pcm_hw_params_t *playback_hw_params;
    snd_pcm_sw_params_t *playback_sw_params;
    snd_async_handler_t *capture_handler;
    snd_async_handler_t *playback_handler;
    char *capbuf;
    char *playbuf;
    vban_stream_context_t* vban_stream_context;
} alsa_context_t;


void stop_streams(alsa_context_t* context, int exit_code);
int find_card_by_name(const char *card_name);
void available_formats(snd_pcm_hw_params_t* params);
int check_format(snd_pcm_hw_params_t* params, snd_pcm_format_t format);
snd_pcm_format_t max_supported_alsa_format(snd_pcm_hw_params_t* params, snd_pcm_format_t format);
int set_hwparams(snd_pcm_t *handle, snd_pcm_hw_params_t *params, snd_pcm_access_t access, snd_pcm_stream_t direction, vban_stream_context_t *vban_context, uint resampling_value);
int set_swparams(snd_pcm_t *handle, snd_pcm_sw_params_t *params, snd_pcm_stream_t direction, vban_stream_context_t *vban_context);
int xrun_recovery(snd_pcm_t *handle, int err, int verbose);
void capture_callback(snd_async_handler_t* handler);
void playback_callback(snd_async_handler_t* handler);

#endif // ALSA_BACKEND_H
