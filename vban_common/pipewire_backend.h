#ifndef PIPEWIRE_BACKEND_H_
#define PIPEWIRE_BACKEND_H_

#include <spa/param/audio/format-utils.h>
#include <pipewire/pipewire.h>
#include <getopt.h>

#include "vban_functions.h"


struct data
{
    struct pw_main_loop *loop;
    struct pw_stream *stream;
    struct spa_audio_info format;
    streamConfig_t* config;
};

enum spa_audio_format format_vban_to_spa(enum VBanBitResolution format_vban);
void help(void);

#endif
