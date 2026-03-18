/*
 * Feedback Protection — Global Safety Net (Layer 3)
 *
 * Context-aware output limiter:
 *   detection_active=1: -6 dBFS threshold, 20-block hold (feedback possible)
 *   detection_active=0: -1 dBFS threshold, 50-block hold (edge cases only)
 *
 * Per-slot limiting (Layer 1) is in chain_host.c.
 */

#include "feedback_detect.h"
#include "unified_log.h"
#include <string.h>
#include <math.h>

#define SAFETY_GAIN_MIN     0.5f    /* -6dB max reduction */
#define SAFETY_GAIN_DECAY   0.95f   /* Per-block decay when triggered */
#define SAFETY_GAIN_RECOVERY 0.001f /* Per-block recovery */

void feedback_detect_init(feedback_detect_state_t *st) {
    memset(st, 0, sizeof(*st));
    st->safety_gain = 1.0f;
    st->enabled = 1;
    st->jack_plugged = 0;  /* Assume no cable (safer) */
}

void feedback_detect_set_jack(feedback_detect_state_t *st, int plugged) {
    if (st) st->jack_plugged = plugged;
}

void feedback_detect_suppress(feedback_detect_state_t *st,
                              int16_t *audio_out, int frames) {
    if (!st || !st->enabled || !audio_out || frames <= 0)
        return;

    /* Compute total output RMS */
    float sum = 0.0f;
    for (int i = 0; i < frames * 2; i++) {
        float s = (float)audio_out[i];
        sum += s * s;
    }
    float rms = sqrtf(sum / (float)(frames * 2));
    float rms_db = (rms > 0.001f) ? 20.0f * log10f(rms / 32768.0f) : -96.0f;

    /* Context-aware thresholds */
    float threshold_db = st->detection_active ? -6.0f : -1.0f;
    int hold_blocks = st->detection_active ? 20 : 50;

    if (rms_db > threshold_db) {
        st->safety_high_count++;
    } else {
        st->safety_high_count = 0;
    }

    /* Sustained loud output: apply global gain reduction */
    if (st->safety_high_count >= hold_blocks) {
        if (st->safety_gain > 0.999f) {
            unified_log("feedback", LOG_LEVEL_INFO,
                       "Global safety triggered: rms=%.1fdB threshold=%.1fdB active=%d",
                       rms_db, threshold_db, st->detection_active);
        }
        st->safety_gain *= SAFETY_GAIN_DECAY;
        /* Context-aware floor: -20dB when feedback possible, -6dB otherwise */
        float gain_floor = st->detection_active ? 0.1f : SAFETY_GAIN_MIN;
        if (st->safety_gain < gain_floor)
            st->safety_gain = gain_floor;
    } else if (rms_db < -6.0f && st->safety_gain < 1.0f) {
        st->safety_gain += SAFETY_GAIN_RECOVERY;
        if (st->safety_gain > 1.0f) st->safety_gain = 1.0f;
    }

    /* Apply safety gain */
    if (st->safety_gain < 0.999f) {
        for (int i = 0; i < frames * 2; i++) {
            int32_t s = (int32_t)((float)audio_out[i] * st->safety_gain);
            if (s > 32767) s = 32767;
            if (s < -32768) s = -32768;
            audio_out[i] = (int16_t)s;
        }
    }
}
