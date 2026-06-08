/*
 * af_aiextract - Audio filter to extract PCM data for AI subtitle pipeline.
 *
 * This filter intercepts audio frames as they flow through mpv's audio chain,
 * copies the PCM data (converted to float32 interleaved), invokes a registered
 * callback, then passes the original frame unmodified to the next filter
 * (ultimately the audio output driver).
 *
 * This file is part of mpv-ai-fork (LGPL).
 */

#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "audio/aframe.h"
#include "audio/format.h"
#include "common/common.h"
#include "filters/filter_internal.h"
#include "filters/user_filters.h"

#include "af_aiextract.h"

// Logging removed to avoid liblog dependency

static aiextract_audio_callback_t g_callback = NULL;

void aiextract_set_callback(aiextract_audio_callback_t callback)
{
    g_callback = callback;
}

aiextract_audio_callback_t aiextract_get_callback(void)
{
    return g_callback;
}

struct priv {
    // No persistent state needed.
};

/* Convert arbitrary mpv audio format to float32 interleaved temporary buffer.
 * Returns NULL if allocation fails. Caller must talloc_free(). */
static float *convert_to_float_interleaved(struct mp_aframe *aframe, void *ta_ctx)
{
    int format = mp_aframe_get_format(aframe);
    int channels = mp_aframe_get_channels(aframe);
    int size = mp_aframe_get_size(aframe); // frames (samples per channel)
    int planes = mp_aframe_get_planes(aframe);

    if (channels <= 0 || size <= 0)
        return NULL;

    float *out = talloc_array(ta_ctx, float, size * channels);
    if (!out)
        return NULL;

    uint8_t **data = mp_aframe_get_data_ro(aframe);

    if (af_fmt_is_planar(format)) {
        // Planar: each channel is a separate plane.
        // Interleave into out[].
        if (planes != channels) {
            talloc_free(out);
            return NULL;
        }
        // Determine sample size
        int bytes = af_fmt_to_bytes(format);
        for (int i = 0; i < size; i++) {
            for (int ch = 0; ch < channels; ch++) {
                switch (format) {
                case AF_FORMAT_S16P:
                case AF_FORMAT_S16: {
                    int16_t val = ((int16_t *)data[ch])[i];
                    out[i * channels + ch] = val / 32768.0f;
                    break;
                }
                case AF_FORMAT_S32P:
                case AF_FORMAT_S32: {
                    int32_t val = ((int32_t *)data[ch])[i];
                    out[i * channels + ch] = val / 2147483648.0f;
                    break;
                }
                case AF_FORMAT_FLOATP:
                case AF_FORMAT_FLOAT: {
                    out[i * channels + ch] = ((float *)data[ch])[i];
                    break;
                }
                case AF_FORMAT_DOUBLEP:
                case AF_FORMAT_DOUBLE: {
                    out[i * channels + ch] = (float)((double *)data[ch])[i];
                    break;
                }
                default:
                    out[i * channels + ch] = 0.0f;
                    break;
                }
            }
        }
    } else {
        // Interleaved: single plane, channels interleaved.
        for (int i = 0; i < size * channels; i++) {
            switch (format) {
            case AF_FORMAT_S16:
                out[i] = ((int16_t *)data[0])[i] / 32768.0f;
                break;
            case AF_FORMAT_S32:
                out[i] = ((int32_t *)data[0])[i] / 2147483648.0f;
                break;
            case AF_FORMAT_FLOAT:
                out[i] = ((float *)data[0])[i];
                break;
            case AF_FORMAT_DOUBLE:
                out[i] = (float)((double *)data[0])[i];
                break;
            default:
                out[i] = 0.0f;
                break;
            }
        }
    }

    return out;
}

static void af_aiextract_process(struct mp_filter *f)
{
    struct priv *p = f->priv;
    (void)p;

    if (!mp_pin_in_needs_data(f->ppins[1]))
        return;

    struct mp_frame frame = mp_pin_out_read(f->ppins[0]);

    if (frame.type == MP_FRAME_AUDIO) {
        struct mp_aframe *aframe = frame.data;
        aiextract_audio_callback_t cb = g_callback;
        if (cb) {
            void *ta_ctx = talloc_new(NULL);
            float *float_buf = convert_to_float_interleaved(aframe, ta_ctx);
            if (float_buf) {
                int size = mp_aframe_get_size(aframe);
                int channels = mp_aframe_get_channels(aframe);
                int rate = mp_aframe_get_rate(aframe);
                double pts = mp_aframe_get_pts(aframe);
                cb(float_buf, size, channels, rate, pts);
            }
            talloc_free(ta_ctx);
        }
    }

    mp_pin_in_write(f->ppins[1], frame);
}

static const struct mp_filter_info af_aiextract_filter = {
    .name = "aiextract",
    .priv_size = sizeof(struct priv),
    .process = af_aiextract_process,
};

static struct mp_filter *af_aiextract_create(struct mp_filter *parent, void *options)
{
    struct mp_filter *f = mp_filter_create(parent, &af_aiextract_filter);
    if (!f) {
        talloc_free(options);
        return NULL;
    }

    mp_filter_add_pin(f, MP_PIN_IN, "in");
    mp_filter_add_pin(f, MP_PIN_OUT, "out");

    return f;
}

const struct mp_user_filter_entry af_aiextract = {
    .desc = {
        .description = "Extract audio PCM for AI subtitle pipeline",
        .name = "aiextract",
        .priv_size = sizeof(struct priv),
    },
    .create = af_aiextract_create,
};
