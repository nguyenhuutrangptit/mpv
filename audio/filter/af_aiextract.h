/*
 * af_aiextract - Audio filter to extract PCM data for AI subtitle pipeline.
 *
 * This file is part of mpv-ai-fork (LGPL).
 */

#ifndef AF_AIEXTRACT_H
#define AF_AIEXTRACT_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#if defined(__GNUC__) || defined(__clang__)
#define AIEXTRACT_EXPORT __attribute__((visibility("default")))
#else
#define AIEXTRACT_EXPORT
#endif

/*
 * Audio extraction callback.
 * @param samples    Interleaved float32 PCM data (allocated temporarily,
 *                   callee should copy if needed across calls).
 * @param num_frames Number of audio frames (samples per channel).
 * @param channels   Number of channels.
 * @param sample_rate Sample rate in Hz.
 * @param pts        Presentation timestamp in seconds.
 */
typedef void (*aiextract_audio_callback_t)(const float *samples,
                                            int num_frames,
                                            int channels,
                                            int sample_rate,
                                            double pts);

/* Set the global callback invoked by af_aiextract on each audio frame.
 * Must be called before the filter processes any data.
 * Thread-safe: should be set once during initialization. */
AIEXTRACT_EXPORT void aiextract_set_callback(aiextract_audio_callback_t callback);

/* Get current callback (for internal use). */
AIEXTRACT_EXPORT aiextract_audio_callback_t aiextract_get_callback(void);

#ifdef __cplusplus
}
#endif

#endif
