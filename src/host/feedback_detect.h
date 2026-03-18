/*
 * Feedback Protection — Global Safety Net
 *
 * Per-slot output limiter is in chain_host.c (feedback_risk_active slots only).
 * This file provides the global output safety net:
 *   - Monitors total AUDIO_OUT RMS pre-ioctl on hardware buffer
 *   - When detection_active (feedback loop possible): -6 dBFS / 58ms hold
 *   - When not active: -1 dBFS / 145ms hold (lenient, edge cases only)
 *   - Covers Move's native sampler feedback (sampler=ME source)
 *   - Context-aware floor: -20dB when active, -6dB otherwise
 *
 * Also in the shim: CC 114 jack detect, Line In load gating, TTS bypass.
 */

#ifndef FEEDBACK_DETECT_H
#define FEEDBACK_DETECT_H

#include <stdint.h>

typedef struct {
    int enabled;            /* Master enable (from feedback_config) */
    int detection_active;   /* 1 = feedback loop possible (tighter thresholds) */

    /* Layer 3: Global safety net */
    float safety_gain;      /* 1.0 = unity, min 0.5 (-6dB) */
    int safety_high_count;  /* Consecutive blocks above threshold */

    /* Passive: jack/mic state */
    int jack_plugged;       /* From CC 114: 1=cable in jack */
    int mic_warning_given;  /* Avoid repeating mic warning */
} feedback_detect_state_t;

void feedback_detect_init(feedback_detect_state_t *st);

/* Apply global safety net to AUDIO_OUT (PRE-ioctl on hardware buffer).
 * Context-aware thresholds based on detection_active. */
void feedback_detect_suppress(feedback_detect_state_t *st,
                              int16_t *audio_out, int frames);

void feedback_detect_set_jack(feedback_detect_state_t *st, int plugged);

#endif /* FEEDBACK_DETECT_H */
