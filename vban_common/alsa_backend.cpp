#include "alsa_backend.h"

// ALSA formats array table according to VBAN formats table
#define VBAN_ALSA_FORMATS_NUMBER 6
snd_pcm_format_t vban_to_alsa_format_table[VBAN_ALSA_FORMATS_NUMBER] =
    {
        SND_PCM_FORMAT_S8,
        SND_PCM_FORMAT_S16_LE,
        SND_PCM_FORMAT_S24_LE,
        SND_PCM_FORMAT_S32_LE,
        SND_PCM_FORMAT_FLOAT_LE,
        SND_PCM_FORMAT_FLOAT64_LE,
};


void stop_streams(alsa_context_t* context, int exit_code)
{
    snd_pcm_close(context->capture_handle);
    snd_pcm_close(context->playback_handle);
    if (context->capture_hw_params!= NULL) free(context->capture_hw_params);
    if (context->playback_hw_params!= NULL) free(context->playback_hw_params);
    if (context->capture_sw_params!= NULL) free(context->capture_sw_params);
    if (context->playback_sw_params!= NULL) free(context->playback_sw_params);
    if (context->capture_handler!= NULL) free(context->capture_handler);
    if (context->playback_handler!= NULL) free(context->playback_handler);
    if (context->capture_handle!= NULL) free(context->capture_handle);
    if (context->playback_handle!= NULL) free(context->playback_handle);
    if (context->capbuf!= NULL) free(context->capbuf);
    if (context->playbuf!= NULL) free(context->playbuf);
    exit(exit_code);
}


int find_card_by_name(const char *card_name)
{
    int card = -1;
    snd_ctl_t *handle;
    char *name;

    for (int i = 0;; ++i)
    {
        if (snd_card_get_name(i, &name) != 0) break;
        if (strcmp(name, card_name) == 0)
        {
            card = i;
            break;
        }
    }
    return card;
}


void available_formats(snd_pcm_hw_params_t* params)
{
    snd_pcm_format_mask_t *fmtmask;
    snd_pcm_format_mask_alloca(&fmtmask);
    snd_pcm_hw_params_get_format_mask(params, fmtmask);
    printf("Available formats:\n");
    if (snd_pcm_format_mask_test(fmtmask, SND_PCM_FORMAT_U8))
        printf("\tU8\n");
    if (snd_pcm_format_mask_test(fmtmask, SND_PCM_FORMAT_S16_LE))
        printf("\tS16_LE\n");
    if (snd_pcm_format_mask_test(fmtmask, SND_PCM_FORMAT_S24_LE))
        printf("\tS24_LE\n");
    if (snd_pcm_format_mask_test(fmtmask, SND_PCM_FORMAT_FLOAT))
        printf("\tFLOAT\n");
    if (snd_pcm_format_mask_test(fmtmask, SND_PCM_FORMAT_MU_LAW))
        printf("\tMU_LAW\n");
    if (snd_pcm_format_mask_test(fmtmask, SND_PCM_FORMAT_A_LAW))
        printf("\tA_LAW\n");
    if (snd_pcm_format_mask_test(fmtmask, SND_PCM_FORMAT_IMA_ADPCM))
        printf("\tIMA_ADPCM\n");
    if (snd_pcm_format_mask_test(fmtmask, SND_PCM_FORMAT_GSM))
        printf("\tGSM\n");
    if (snd_pcm_format_mask_test(fmtmask, SND_PCM_FORMAT_S24_3LE))
        printf("\tS24_3LE\n");
    if (snd_pcm_format_mask_test(fmtmask, SND_PCM_FORMAT_S32_LE))
        printf("\tS32_LE\n");
    fflush(stdout);
}


int check_format(snd_pcm_hw_params_t* params, snd_pcm_format_t format)
{
    snd_pcm_format_mask_t *fmtmask;
    snd_pcm_format_mask_alloca(&fmtmask);
    snd_pcm_hw_params_get_format_mask(params, fmtmask);
    if (snd_pcm_format_mask_test(fmtmask, format))
    {
        fprintf(stderr, "Format OK\r\n");
        return 0;
    }
    fprintf(stderr, "Format mismatch\r\n");
    return 1;
}


snd_pcm_format_t max_supported_alsa_format(snd_pcm_hw_params_t* params, snd_pcm_format_t format)
{
    int fmt = format;
    snd_pcm_format_mask_t *fmtmask;
    snd_pcm_format_mask_alloca(&fmtmask);
    snd_pcm_hw_params_get_format_mask(params, fmtmask);
    while(!snd_pcm_format_mask_test(fmtmask, (snd_pcm_format_t)fmt))
    {
        fmt--;
        if (fmt == SND_PCM_FORMAT_S32_LE) fmt--;
    }
    if (fmt < (int)format) fprintf(stderr, "Maximum supported format is %d\r\n", fmt);
    return (snd_pcm_format_t)fmt;
}


int set_hwparams(snd_pcm_t *handle,
                        snd_pcm_hw_params_t *params,
                        snd_pcm_access_t access,
                        snd_pcm_stream_t direction,
                        vban_stream_context_t *vban_context,
                        uint resampling_value)
{
    int err;
    int dir = direction;
    char dir_string[9] = {0,0,0,0,0,0,0,0,0};
    if (dir) strcpy(dir_string, "capture");
    else strcpy(dir_string, "playback");

    if (params==NULL) snd_pcm_hw_params_alloca(&params);

    /* choose all parameters */
    err = snd_pcm_hw_params_any(handle, params);
    if (err < 0)
    {
        fprintf(stderr, "Broken configuration for %s: no configurations available: %s\n", dir_string, snd_strerror(err));
        return err;
    }
    /* set hardware resampling */
    if (resampling_value) err = snd_pcm_hw_params_set_rate_resample(handle, params, resampling_value);
    if (err < 0)
    {
        fprintf(stderr, "Resampling setup failed for %s: %s\n", dir_string, snd_strerror(err));
        return err;
    }
    /* set the interleaved read/write format */
    err = snd_pcm_hw_params_set_access(handle, params, access);
    if (err < 0)
    {
        fprintf(stderr, "Access type not available for %s: %s\n", dir_string, snd_strerror(err));
        return err;
    }

    /* set the count of channels */
    uint nbchannels_min;
    uint nbchannels_max;
    err = snd_pcm_hw_params_get_channels_min(params, &nbchannels_min);
    if (err < 0)
    {
        fprintf(stderr, "Cannot get minimum %s channels number: %s\n", dir_string, snd_strerror(err));
        return err;
    }
    err = snd_pcm_hw_params_get_channels_min(params, &nbchannels_max);
    if (err < 0)
    {
        fprintf(stderr, "Cannot get maximum %s channels number: %s\n", dir_string, snd_strerror(err));
        return err;
    }
    switch (direction)
    {
    case SND_PCM_STREAM_CAPTURE:
        if (vban_context->nbinputs < nbchannels_min)
        {
            fprintf(stderr, "Real minimum %s channels number (%d) is bigger than ordered (%d), correcting to real...\n", dir_string, nbchannels_min, vban_context->nbinputs);
            vban_context->nbinputs = nbchannels_min;
        }
        if (vban_context->nbinputs > nbchannels_max)
        {
            fprintf(stderr, "Real maximum %s channels number (%d) is smaller than ordered (%d), correcting to real...\n", dir_string, nbchannels_max, vban_context->nbinputs);
            vban_context->nbinputs = nbchannels_max;
        }
        err = snd_pcm_hw_params_set_channels(handle, params, vban_context->nbinputs);
        if (err < 0)
        {
            fprintf(stderr, "Channels count (%u) not available for %s: %s\n", vban_context->nbinputs, dir_string, snd_strerror(err));
            return err;
        }
        break;
    case SND_PCM_STREAM_PLAYBACK:
    default:
        if (vban_context->nboutputs < nbchannels_min)
        {
            fprintf(stderr, "Real minimum %s channels number (%d) is bigger than ordered (%d), correcting to real...\n", dir_string, nbchannels_min, vban_context->nboutputs);
            vban_context->nboutputs = nbchannels_min;
        }
        if (vban_context->nboutputs > nbchannels_max)
        {
            fprintf(stderr, "Real maximum %s channels number (%d) is smaller than ordered (%d), correcting to real...\n", dir_string, nbchannels_max, vban_context->nboutputs);
            vban_context->nboutputs = nbchannels_max;
        }
        err = snd_pcm_hw_params_set_channels(handle, params, vban_context->nboutputs);
        if (err < 0)
        {
            fprintf(stderr, "Channels count (%u) not available for %s: %s\n", vban_context->nboutputs, dir_string, snd_strerror(err));
            return err;
        }
        break;
    }

    /* set the sample format */
    available_formats(params);
    snd_pcm_format_t fmt;
    switch (direction)
    {
    case SND_PCM_STREAM_CAPTURE:
        fmt = max_supported_alsa_format(params, vban_to_alsa_format_table[vban_context->vban_output_format]);

        vban_context->vban_output_format = VBAN_BITFMT_8_INT;
        while((vban_to_alsa_format_table[vban_context->vban_output_format]!= fmt)&&(vban_context->vban_output_format<=VBAN_BITFMT_64_FLOAT)) vban_context->vban_output_format++;

        break;
    case SND_PCM_STREAM_PLAYBACK:
    default:
        fmt = max_supported_alsa_format(params, vban_to_alsa_format_table[vban_context->vban_input_format]);

        vban_context->vban_input_format = VBAN_BITFMT_8_INT;
        while((vban_to_alsa_format_table[vban_context->vban_input_format]!= fmt)&&(vban_context->vban_input_format<=VBAN_BITFMT_64_FLOAT)) vban_context->vban_input_format++;

        break;
    }
    err = snd_pcm_hw_params_set_format(handle, params, fmt);
    if (err < 0)
    {
        fprintf(stderr, "Sample format not available for %s: %s\n", dir_string, snd_strerror(err));
        return err;
    }

    /* set the stream rate */
    uint samplerate_min;
    uint samplerate_max;
    err = snd_pcm_hw_params_get_rate_min(params, &samplerate_min, &dir);
    if (err < 0)
    {
        fprintf(stderr, "Cannot get minimum %s samplerate\r\n", dir_string);
        return err;
    }
    err = snd_pcm_hw_params_get_rate_max(params, &samplerate_max, &dir);
    if (err < 0)
    {
        fprintf(stderr, "Cannot get maximum %s samplerate\r\n", dir_string);
        return err;
    }
    if (vban_context->samplerate > samplerate_max)
    {
        fprintf(stderr, "Ordered samplerate (%d) is bigger than maximum real (%d), correcting...", vban_context->samplerate, samplerate_max);
        vban_context->samplerate = samplerate_max;
    }
    if (vban_context->samplerate < samplerate_min)
    {
        fprintf(stderr, "Ordered samplerate (%d) is smaller than minimum real (%d), correcting...", vban_context->samplerate, samplerate_min);
        vban_context->samplerate = samplerate_min;
    }
    uint rrate = vban_context->samplerate;
    err = snd_pcm_hw_params_set_rate_near(handle, params, &rrate, &dir);
    if (err < 0)
    {
        fprintf(stderr, "Rate %uHz not available for %s: %s\n", rrate, dir_string, snd_strerror(err));
        return err;
    }
    if (rrate != (uint)vban_context->samplerate)
    {
        fprintf(stderr, "Rate doesn't match (requested %uHz, get %iHz)\n", rrate, err);
        return -EINVAL;
    }
    /* set the buffer time */
    // err = snd_pcm_hw_params_set_buffer_time_near(handle, params, &buffer_time, &dir);
    // if (err < 0)
    // {
    //     fprintf(stderr, "Unable to set buffer time %u for %s: %s\n", buffer_time, dir_string, snd_strerror(err));
    //     return err;
    // }
    uint nper = vban_context->nperiods;
    uint persize = vban_context->nframes;
    snd_pcm_uframes_t bufsize = persize * nper;
    err = snd_pcm_hw_params_set_buffer_size_near(handle, params, &bufsize);
    if (err < 0)
    {
        fprintf(stderr, "Unable to set buffer size for %s: %s\n", dir_string, snd_strerror(err));
        return err;
    }
    /* set the period time */
    // err = snd_pcm_hw_params_set_period_time_near(handle, params, &period_time, &dir);
    // if (err < 0)
    // {
    //     fprintf(stderr, "Unable to set period time %u for %s: %s\n", period_time, dir_string, snd_strerror(err));
    //     return err;
    // }
    err = snd_pcm_hw_params_set_period_size(handle, params, vban_context->nframes, dir);
    if (err < 0)
    {
        fprintf(stderr, "Unable to get period size for %s: %s\n", dir_string, snd_strerror(err));
        return err;
    }
    err = snd_pcm_hw_params_set_periods_near(handle, params, &persize, &dir);
    if (err < 0)
    {
        fprintf(stderr, "Unable to get periods num for %s: %s\n", dir_string, snd_strerror(err));
        return err;
    }
    /* write the parameters to device */
    err = snd_pcm_hw_params(handle, params);
    if (err < 0)
    {
        fprintf(stderr, "Unable to set hw params for %s: %s\n", dir_string, snd_strerror(err));
        return err;
    }
    return 0;
}

int set_swparams(snd_pcm_t *handle,
                        snd_pcm_sw_params_t *params,
                        snd_pcm_stream_t direction,
                        vban_stream_context_t *vban_context)
{
    int err;
    int dir = direction;
    char dir_string[9] = {0,0,0,0,0,0,0,0,0};
    if (dir) strcpy(dir_string, "capture");
    else strcpy(dir_string, "playback");

    if (params==NULL) snd_pcm_sw_params_alloca(&params);

    /* get the current swparams */
    err = snd_pcm_sw_params_current(handle, params);
    if (err < 0)
    {
        fprintf(stderr, "Unable to determine current swparams for %s: %s\n", dir_string, snd_strerror(err));
        return err;
    }
    /* start the transfer when the buffer is almost full: */
    /* (buffer_size / avail_min) * avail_min */
    err = snd_pcm_sw_params_set_start_threshold(handle, params, vban_context->nframes);//(buffer_size / period_size) * period_size);
    if (err < 0)
    {
        fprintf(stderr, "Unable to set start threshold mode for %s: %s\n", dir_string, snd_strerror(err));
        return err;
    }
    /* allow the transfer when at least period_size samples can be processed */
    /* or disable this mechanism when period event is enabled (aka interrupt like style processing) */
    err = snd_pcm_sw_params_set_avail_min(handle, params, vban_context->nframes);//period_event ? buffer_size : period_size);
    if (err < 0)
    {
        fprintf(stderr, "Unable to set avail min for %s: %s\n", dir_string, snd_strerror(err));
        return err;
    }
    /* enable period events when requested */
    if (vban_context->nframes)
    {
        err = snd_pcm_sw_params_set_period_event(handle, params, 1);
        if (err < 0)
        {
            fprintf(stderr, "Unable to set period event: %s\n", snd_strerror(err));
            return err;
        }
    }

    err = snd_pcm_sw_params_set_silence_threshold(handle, params, vban_context->nframes);
    if (err < 0)
    {
        fprintf(stderr, "Unable to set silence threshold: %s\n", snd_strerror(err));
        return err;
    }
    /* write the parameters to the device */
    err = snd_pcm_sw_params(handle, params);
    if (err < 0)
    {
        fprintf(stderr, "Unable to set sw params for %s: %s\n", dir_string, snd_strerror(err));
        return err;
    }
    return 0;
}


int xrun_recovery(snd_pcm_t *handle, int err, int verbose)
{
    if (verbose) printf("stream recovery\n");
    if (err == -EPIPE) // Playback Underrun
    {
        err = snd_pcm_prepare(handle);
        if (err < 0) printf("Can't recovery from underrun, prepare failed: %s\n", snd_strerror(err));
        return 0;
    }
    else if (err == -ESTRPIPE) // Capture Overrun
    {
        while ((err = snd_pcm_resume(handle)) == -EAGAIN) sleep(1);   /* wait until the suspend flag is released */
        if (err < 0)
        {
            err = snd_pcm_prepare(handle);
            if (err < 0) printf("Can't recovery from suspend, prepare failed: %s\n", snd_strerror(err));
        }
        return 0;
    }
    return err;
}


void capture_callback(snd_async_handler_t* handler)
{
    snd_pcm_t *handle = snd_async_handler_get_pcm(handler);
    alsa_context_t* context = (alsa_context_t*)snd_async_handler_get_callback_private(handler);
    vban_stream_context_t* vban_context = context->vban_stream_context;
    udpc_t* txsock = vban_context->txsock;
    uint16_t txport = vban_context->txport;
    int nframes = vban_context->nframes;
    snd_pcm_sframes_t readframes;
    uint16_t capbufsize = vban_context->txbuflen;

    if (context->capbuf == nullptr) context->capbuf = (char*)malloc(capbufsize);
    readframes = snd_pcm_readi(handle, context->capbuf, nframes);
    if (readframes<0)
    {
        readframes = snd_pcm_recover(handle, readframes, 0);
        fprintf(stderr, "XRUN, trying to recover...\r\n");
    }
    if (readframes<0)
    {
        fprintf(stderr, "snd_pcm_readi failed: %s\n", snd_strerror(readframes));
        snd_pcm_reset(handle);
        //snd_pcm_prepare(handle);
    }
    else if (readframes<nframes)
    {
        fprintf(stderr, "Short read, %ld!\n", readframes);
        //snd_pcm_reset(handle);
        if (readframes == 0) snd_pcm_start(handle);
        //snd_pcm_prepare(handle);
    }
    if (readframes == vban_context->nframes) memcpy(vban_context->txbuf, context->capbuf, capbufsize);
    for (uint16_t pac=0; pac<vban_context->pacnum; pac++)
    {
        memcpy(vban_context->txpacket.data, vban_context->txbuf + pac*vban_context->pacdatalen, vban_context->pacdatalen);

        for (uint8_t red=0; red<=vban_context->redundancy+1; red++)
            if (vban_context->txport)
            {
                if (vban_context->iptx!= 0)
                {
                    udp_send(txsock, txport, (char*)&vban_context->txpacket, VBAN_HEADER_SIZE+vban_context->pacdatalen);

                    int icnt = 0;
                    usleep(20);
                    while((check_icmp_status(vban_context->txsock->fd)==111)&&(icnt<2))
                    {
                        udp_send(txsock, txport, (char*)&vban_context->txpacket, VBAN_HEADER_SIZE+vban_context->pacdatalen);
                        usleep(20);
                        icnt++;
                    }
                    if (icnt==2) fprintf(stderr, "Получено ICMP уведомление 'Port Unreachable'\n");
                    // int ec = check_icmp_status(context->txsock->fd);
                    // if (ec==111) fprintf(stderr, "Получено ICMP уведомление 'Port Unreachable'\n");
                }
            }
            else write(vban_context->pipedesc, (uint8_t*)&vban_context->txpacket, VBAN_HEADER_SIZE+vban_context->pacdatalen);
        vban_context->txpacket.header.nuFrame++;
    }
    vban_context->txpacket.header.nuFrame++;
}


void playback_callback(snd_async_handler_t* handler)
{
    snd_pcm_t *handle = snd_async_handler_get_pcm(handler);
    alsa_context_t* context = (alsa_context_t*)snd_async_handler_get_callback_private(handler);
    vban_stream_context_t* vban_context = context->vban_stream_context;
    int nframes = vban_context->nframes;
    snd_pcm_sframes_t wroteframes;
    char* playbuf = context->playbuf;
    int playbuflen = context->playbuflen;
    int framesize = vban_context->nboutputs * VBanBitResolutionSize[vban_context->vban_input_format];
    float samplebuf[VBAN_CHANNELS_MAX_NB];

    if (vban_context->ringbuffer)
    {
        for (int frame = 0; frame<nframes; frame++)
        {
            if (ringbuffer_read_space(vban_context->ringbuffer)>=vban_context->nboutputs*sizeof(float))
            {
                ringbuffer_read(vban_context->ringbuffer, (char*)samplebuf, vban_context->nboutputs*sizeof(float));
                vban_sample_convert(playbuf + frame*framesize, vban_context->vban_input_format, samplebuf, VBAN_BITFMT_32_FLOAT, vban_context->nboutputs);
            }
        }
    }

    wroteframes = snd_pcm_writei(handle, playbuf, nframes);
    if (wroteframes < 0)
    {
        wroteframes = snd_pcm_recover(handle, wroteframes, 0);
        fprintf(stderr, "XRUN, trying to recover...\r\n");
    }
    if (wroteframes < 0)
    {
        fprintf(stderr, "snd_pcm_writei failed: %s\n", snd_strerror(wroteframes));
        snd_pcm_reset(handle);
        //snd_pcm_prepare(handle);
    }
    else if (wroteframes<nframes)
    {
        fprintf(stderr, "Short write, %ld!\n", wroteframes);
        //snd_pcm_reset(handle);
        if (wroteframes == 0) snd_pcm_start(handle);
        //snd_pcm_prepare(handle);
    }
}
