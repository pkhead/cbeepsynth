#include <assert.h>
#include <string.h>
#include "../util.h"
#include "../param_util.h"
#include "../wavetables.h"
#include "../filtering.h"
#include "../alloc.h"
#include "../log.h"

#include "synth.h"

#define PICKED_BASE_EXPRESSION 0.025 // Same as harmonics.
#define STRING_DECAY_RATE 0.12
#define IMPULSE_WAVE_LENGTH ((HARMONICS_WAVE_LENGTH + 1))
#define DELAY_LINE_COUNT ((BPBXSYN_SYNTH_MAX_VOICES * UNISON_MAX_VOICES))

// Picked strings have an all-pass filter with a corner frequency based on the
// tone fundamental frequency, in order to add a slight inharmonicity. (Which is
// important for distortion.)

// The tone fundamental freq is pulled toward this freq for computing the
// all-pass corner freq.
#define STRING_DISPERSION_CENTER_FREQ 6000.0

// The tone fundamental freq freq moves this much toward the center freq for
// computing the all-pass corner freq.
#define STRING_DISPERSION_FREQ_SCALE 0.3
                                        
// The all-pass corner freq is based on this times the adjusted tone fundamental
// freq. 
#define STRING_DISPERSION_FREQ_MULT 4.0

// The cutoff freq of the shelf filter that is used to decay the high frequency
// energy in the picked string.
#define STRING_SHELF_HZ 4000.0

// i think string decay types are a scrapped feature
typedef enum string_sustain {
    STRING_SUSTAIN_BRIGHT,
    STRING_SUSTAIN_ACOUSTIC,
    STRING_SUSTAIN_LENGTH,
} string_sustain_e;

typedef struct pstring {
    float *delay_line;
    int delay_index;
    double all_pass_sample;
    double all_pass_prev_input;
    double sustain_filter_sample;
	double sustain_filter_prev_output_2;
	double sustain_filter_prev_input_1;
	double sustain_filter_prev_input_2;
    double fractional_delay_sample;
    double prev_delay_length;
    double delay_length_delta;
    int delay_reset_offset;

    double all_pass_g;
    double all_pass_g_delta;
    double sustain_filter_a1;
	double sustain_filter_a1_delta;
	double sustain_filter_a2;
	double sustain_filter_a2_delta;
	double sustain_filter_b0;
	double sustain_filter_b0_delta;
	double sustain_filter_b1;
	double sustain_filter_b1_delta;
	double sustain_filter_b2;
	double sustain_filter_b2_delta;
} pstring_s;

typedef struct picked_voice {
    inst_base_voice_s base;

    double phase[UNISON_MAX_VOICES];
    double phase_delta[UNISON_MAX_VOICES];
    double phase_delta_scale[UNISON_MAX_VOICES];

    double sustain_start, sustain_end;

    double prev_pitch_expression;
    double prev_string_decay;

    bool has_prev_pitch_expression;
    bool has_prev_string_decay;
    bool at_note_start;

    pstring_s strings[UNISON_MAX_VOICES];
} picked_voice_s;

typedef struct picked_inst {
    bpbxsyn_synth_s base;

    uint8_t unison_type;
    uint8_t harmonics[BPBXSYN_HARMONICS_CONTROL_COUNT];
    uint8_t last_harmonics[BPBXSYN_HARMONICS_CONTROL_COUNT];

    // [0] = prev
    // [1] = current
    double sustain[2];

    float impulse_wave[IMPULSE_WAVE_LENGTH];
    
    // TODO: what is a good size for the delay line?
    picked_voice_s voices[BPBXSYN_SYNTH_MAX_VOICES];
    float *delay_line_alloc; // a continguous list of delay lines, of size
                             // delay_line_size
    int delay_line_size;
} picked_inst_s;

static void pstring_reset(pstring_s *self)
{
    self->delay_index = -1;
    self->all_pass_sample = 0.0;
    self->all_pass_prev_input = 0.0;
    self->sustain_filter_sample = 0.0;
    self->sustain_filter_prev_output_2 = 0.0;
    self->sustain_filter_prev_input_1 = 0.0;
    self->sustain_filter_prev_input_2 = 0.0;
    self->fractional_delay_sample = 0.0;
    self->prev_delay_length = -1.0;
    self->delay_reset_offset = 0;
}

static void pstring_update(pstring_s *self, const picked_inst_s *inst,
                           picked_voice_s *voice, int string_index,
                           double rounded_samples_per_tick,
                           double string_decay_start, double string_decay_end,
                           string_sustain_e sustain_type)
{
    const double samples_per_second = inst->base.sample_rate;
    double allPassCenter = PI2 * STRING_DISPERSION_CENTER_FREQ / samples_per_second;

    double prevDelayLength = self->prev_delay_length;

    double phaseDeltaStart = voice->phase_delta[string_index];
    double phaseDeltaScale = voice->phase_delta_scale[string_index];
    double phaseDeltaEnd = phaseDeltaStart * pow(phaseDeltaScale, rounded_samples_per_tick);

    double radiansPerSampleStart = PI2 * phaseDeltaStart;
    double radiansPerSampleEnd = PI2 * phaseDeltaEnd;

    double centerHarmonicStart = radiansPerSampleStart * 2.0;
    double centerHarmonicEnd = radiansPerSampleEnd * 2.0;

    double allPassRadiansStart = min(PI, radiansPerSampleStart * STRING_DISPERSION_FREQ_MULT * pow(allPassCenter / radiansPerSampleStart, STRING_DISPERSION_FREQ_SCALE));
    double allPassRadiansEnd = min(PI, radiansPerSampleEnd * STRING_DISPERSION_FREQ_MULT * pow(allPassCenter / radiansPerSampleEnd, STRING_DISPERSION_FREQ_SCALE));
    double shelfRadians = PI2 * STRING_SHELF_HZ / samples_per_second;
    double decayCurveStart = (pow(100.0, string_decay_start) - 1.0) / 99.0;
    double decayCurveEnd   = (pow(100.0, string_decay_end  ) - 1.0) / 99.0;
    double registerAmt = sustain_type = STRING_SUSTAIN_ACOUSTIC ? 0.25 : 0.0;
    double registerShelfCenter = 15.6;
    double registerLowpassCenter = 3.0 * samples_per_second / 48000.0;
    //const decayRateStart: number = Math.pow(0.5, decayCurveStart * shelfRadians / radiansPerSampleStart);
    //const decayRateEnd: number   = Math.pow(0.5, decayCurveEnd   * shelfRadians / radiansPerSampleEnd);
    double decayRateStart = pow(0.5, decayCurveStart * pow(shelfRadians / (radiansPerSampleStart * registerShelfCenter), (1.0 + 2.0 * registerAmt)) * registerShelfCenter);
    double decayRateEnd = pow(0.5, decayCurveEnd   * pow(shelfRadians / (radiansPerSampleEnd   * registerShelfCenter), (1.0 + 2.0 * registerAmt)) * registerShelfCenter);
    
    double expressionDecayStart = pow(decayRateStart, 0.002);
    double expressionDecayEnd = pow(decayRateEnd, 0.002);

    filter_coefs_s tempFilterStartCoefficients;
    filter_coefs_s tempFilterEndCoefficients;

    bbsyn_filter_ap1ipa(&tempFilterStartCoefficients, allPassRadiansStart);
    bpbxsyn_complex_s tempFrequencyResponse = bbsyn_filter_analyze(tempFilterStartCoefficients, centerHarmonicStart);
    double allPassGStart = tempFilterStartCoefficients.b[0]; /* same as a[1] */
    double allPassPhaseDelayStart = -atan2(tempFrequencyResponse.imag, tempFrequencyResponse.real) / centerHarmonicStart;

    bbsyn_filter_ap1ipa(&tempFilterEndCoefficients, allPassRadiansEnd);
    tempFrequencyResponse = bbsyn_filter_analyze(tempFilterEndCoefficients, centerHarmonicEnd);
    double allPassGEnd = tempFilterEndCoefficients.b[0]; /* same as a[1] */
    double allPassPhaseDelayEnd = -atan2(tempFrequencyResponse.imag, tempFrequencyResponse.real) / centerHarmonicEnd;

    // 1st order shelf filters and 2nd order lowpass filters have differently shaped frequency
    // responses, as well as adjustable shapes. I originally picked a 1st order shelf filter,
    // but I kinda prefer 2nd order lowpass filters now and I designed a couple settings:
    typedef enum pstring_brightness {
        STRING_BRIGHTNESS_BRIGHT,   // 1st order shelf
        STRING_BRIGHTNESS_NORMAL,   // 2nd order lowpass, rounded corner
        STRING_BRIGHTNESS_RESONANT, // 3rd order lowpass, harder corner
    } pstring_brightness_e;

    pstring_brightness_e brightnessType = sustain_type == STRING_SUSTAIN_BRIGHT ? STRING_BRIGHTNESS_BRIGHT : STRING_BRIGHTNESS_NORMAL;
    if (brightnessType == STRING_BRIGHTNESS_BRIGHT) {
        const double shelfGainStart = pow(decayRateStart, STRING_DECAY_RATE);
        const double shelfGainEnd   = pow(decayRateEnd,   STRING_DECAY_RATE);

        bbsyn_filter_hshelf2(&tempFilterStartCoefficients, shelfRadians, shelfGainStart, 0.5);
        bbsyn_filter_hshelf2(&tempFilterEndCoefficients, shelfRadians, shelfGainEnd, 0.5);
    } else {
        double cornerHardness = pow(brightnessType == STRING_BRIGHTNESS_NORMAL ? 0.0 : 1.0, 0.25);
        double lowpass1stOrderCutoffRadiansStart = pow(registerLowpassCenter * registerLowpassCenter * radiansPerSampleStart * 3.3 * 48000 / samples_per_second, 0.5 + registerAmt) / registerLowpassCenter / pow(decayCurveStart, .5);
        double lowpass1stOrderCutoffRadiansEnd = pow(registerLowpassCenter * registerLowpassCenter * radiansPerSampleEnd   * 3.3 * 48000 / samples_per_second, 0.5 + registerAmt) / registerLowpassCenter / pow(decayCurveEnd,   .5);
        double lowpass2ndOrderCutoffRadiansStart = lowpass1stOrderCutoffRadiansStart * pow(2.0, 0.5 - 1.75 * (1.0 - pow(1.0 - cornerHardness, 0.85)));
        double lowpass2ndOrderCutoffRadiansEnd = lowpass1stOrderCutoffRadiansEnd   * pow(2.0, 0.5 - 1.75 * (1.0 - pow(1.0 - cornerHardness, 0.85)));
        double lowpass2ndOrderGainStart = pow(2.0, -pow(2.0, -pow(cornerHardness, 0.9)));
        double lowpass2ndOrderGainEnd = pow(2.0, -pow(2.0, -pow(cornerHardness, 0.9)));

        bbsyn_filter_lp2bw(&tempFilterStartCoefficients,
                           bbsyn_warp_inf_to_nyquist(lowpass2ndOrderCutoffRadiansStart),
                           lowpass2ndOrderGainStart);
        bbsyn_filter_lp2bw(&tempFilterEndCoefficients,
                           bbsyn_warp_inf_to_nyquist(lowpass2ndOrderCutoffRadiansEnd),
                           lowpass2ndOrderGainEnd);
    }
    
    tempFrequencyResponse = bbsyn_filter_analyze(tempFilterStartCoefficients, centerHarmonicStart);
    const double sustainFilterA1Start = tempFilterStartCoefficients.a[1];
    const double sustainFilterA2Start = tempFilterStartCoefficients.a[2];
    const double sustainFilterB0Start = tempFilterStartCoefficients.b[0] * expressionDecayStart;
    const double sustainFilterB1Start = tempFilterStartCoefficients.b[1] * expressionDecayStart;
    const double sustainFilterB2Start = tempFilterStartCoefficients.b[2] * expressionDecayStart;
    const double sustainFilterPhaseDelayStart = -atan2(tempFrequencyResponse.imag, tempFrequencyResponse.real) / centerHarmonicStart;
    
    tempFrequencyResponse = bbsyn_filter_analyze(tempFilterEndCoefficients, centerHarmonicEnd);
    const double sustainFilterA1End = tempFilterEndCoefficients.a[1];
    const double sustainFilterA2End = tempFilterEndCoefficients.a[2];
    const double sustainFilterB0End = tempFilterEndCoefficients.b[0] * expressionDecayEnd;
    const double sustainFilterB1End = tempFilterEndCoefficients.b[1] * expressionDecayEnd;
    const double sustainFilterB2End = tempFilterEndCoefficients.b[2] * expressionDecayEnd;
    const double sustainFilterPhaseDelayEnd = -atan2(tempFrequencyResponse.imag, tempFrequencyResponse.real) / centerHarmonicEnd;
    
    const double periodLengthStart = 1.0 / phaseDeltaStart;
    const double periodLengthEnd = 1.0 / phaseDeltaEnd;
    const double minBufferLength = ceil(max(periodLengthStart, periodLengthEnd) * 2);
    const double delayLength = periodLengthStart - allPassPhaseDelayStart - sustainFilterPhaseDelayStart;
    const double delayLengthEnd = periodLengthEnd - allPassPhaseDelayEnd - sustainFilterPhaseDelayEnd;
    
    self->prev_delay_length = delayLength;
    self->delay_length_delta = (delayLengthEnd - delayLength) / rounded_samples_per_tick;
    self->all_pass_g = allPassGStart;
    self->sustain_filter_a1 = sustainFilterA1Start;
    self->sustain_filter_a2 = sustainFilterA2Start;
    self->sustain_filter_b0 = sustainFilterB0Start;
    self->sustain_filter_b1 = sustainFilterB1Start;
    self->sustain_filter_b2 = sustainFilterB2Start;
    self->all_pass_g_delta = (allPassGEnd - allPassGStart) / rounded_samples_per_tick;
    self->sustain_filter_a1_delta = (sustainFilterA1End - sustainFilterA1Start) / rounded_samples_per_tick;
    self->sustain_filter_a2_delta = (sustainFilterA2End - sustainFilterA2Start) / rounded_samples_per_tick;
    self->sustain_filter_b0_delta = (sustainFilterB0End - sustainFilterB0Start) / rounded_samples_per_tick;
    self->sustain_filter_b1_delta = (sustainFilterB1End - sustainFilterB1Start) / rounded_samples_per_tick;
    self->sustain_filter_b2_delta = (sustainFilterB2End - sustainFilterB2Start) / rounded_samples_per_tick;

    const bool pitchChanged = fabs(log2(delayLength / prevDelayLength)) > 0.01;

    const bool reinitializeImpulse = (self->delay_index == -1 || pitchChanged);
    // TODO: picked string delay line allocation
    // if (this.delayLine == null || this.delayLine.length <= minBufferLength) {
    //     // The delay line buffer will get reused for other tones so might as well
    //     // start off with a buffer size that is big enough for most notes.
    //     const likelyMaximumLength: number = Math.ceil(2 * synth.samplesPerSecond / Instrument.frequencyFromPitch(12));
    //     const newDelayLine: Float32Array = new Float32Array(Synth.fittingPowerOfTwo(Math.max(likelyMaximumLength, minBufferLength)));
    //     if (!reinitializeImpulse && this.delayLine != null) {
    //         // If the tone has already started but the buffer needs to be reallocated,
    //         // transfer the old data to the new buffer.
    //         const oldDelayBufferMask: number = (this.delayLine.length - 1) >> 0;
    //         const startCopyingFromIndex: number = this.delayIndex + this.delayResetOffset;
    //         this.delayIndex = this.delayLine.length - this.delayResetOffset;
    //         for (let i: number = 0; i < this.delayLine.length; i++) {
    //             newDelayLine[i] = this.delayLine[(startCopyingFromIndex + i) & oldDelayBufferMask];
    //         }
    //     }
    //     this.delayLine = newDelayLine;
    // }

    float *const delayLine = self->delay_line;
    const int delayBufferMask = (inst->delay_line_size - 1);
    assert(((delayBufferMask + 1) & delayBufferMask) == 0);

    if (reinitializeImpulse) {
        // -1 delay index means the tone was reset.
        // Also, if the pitch changed suddenly (e.g. from seamless or arpeggio) then reset the wave.

        self->delay_index = 0;
        self->all_pass_sample = 0.0;
        self->all_pass_prev_input = 0.0;
        self->sustain_filter_sample = 0.0;
        self->sustain_filter_prev_output_2 = 0.0;
        self->sustain_filter_prev_input_1 = 0.0;
        self->sustain_filter_prev_input_2 = 0.0;
        self->fractional_delay_sample = 0.0;

        // Clear away a region of the delay buffer for the new impulse.
        const double startImpulseFrom = -delayLength;
        const int startZerosFrom = (int) floor(startImpulseFrom - periodLengthStart / 2);
        const int stopZerosAt = (int) ceil(startZerosFrom + periodLengthStart * 2);
        self->delay_reset_offset = stopZerosAt; // And continue clearing the area in front of the delay line.
        for (int i = startZerosFrom; i <= stopZerosAt; i++) {
            delayLine[i & delayBufferMask] = 0.0f;
        }

        const float *const impulseWave = inst->impulse_wave;
        const int impulseWaveLength = IMPULSE_WAVE_LENGTH - 1; // The first sample is duplicated at the end, don't double-count it.
        const double impulsePhaseDelta = (double) impulseWaveLength / periodLengthStart;

        const double fadeDuration = min(periodLengthStart * 0.2, samples_per_second * 0.003);
        const double startImpulseFromSample = ceil(startImpulseFrom);
        const double stopImpulseAt = startImpulseFrom + periodLengthStart + fadeDuration;
        const double stopImpulseAtSample = stopImpulseAt; // TODO: is this an int?
        double impulsePhase = (startImpulseFromSample - startImpulseFrom) * impulsePhaseDelta;
        double prevWaveIntegral = 0.0;
        for (double i = startImpulseFromSample; i <= stopImpulseAtSample; i++) {
            const int impulsePhaseInt = (int) impulsePhase;
            const int index = impulsePhaseInt % impulseWaveLength;
            double nextWaveIntegral = (double) impulseWave[index];
            const double phaseRatio = impulsePhase - impulsePhaseInt;
            nextWaveIntegral += (impulseWave[index + 1] - nextWaveIntegral) * phaseRatio;
            const double sample = (nextWaveIntegral - prevWaveIntegral) / impulsePhaseDelta;
            const double fadeIn = min(1.0, (i - startImpulseFrom) / fadeDuration);
            const double fadeOut = min(1.0, (stopImpulseAt - i) / fadeDuration);
            const double combinedFade = fadeIn * fadeOut;
            const double curvedFade = combinedFade * combinedFade * (3.0 - 2.0 * combinedFade); // A cubic sigmoid from 0 to 1.
            delayLine[(int)i & delayBufferMask] += (float)(sample * curvedFade);
            prevWaveIntegral = nextWaveIntegral;
            impulsePhase += impulsePhaseDelta;
        }
    }
}

static void picked_init(bpbxsyn_context_s *ctx, bpbxsyn_synth_s *p_inst) {
    picked_inst_s *inst = (picked_inst_s*)p_inst;
    *inst = (picked_inst_s){0};
    bbsyn_inst_init(ctx, &inst->base, BPBXSYN_SYNTH_PICKED_STRING);

    inst->sustain[0] = BPBXSYN_PICKED_STRING_SUSTAIN_MAX;
    inst->sustain[1] = inst->sustain[0];
}

static bpbxsyn_voice_id picked_note_on(bpbxsyn_synth_s *p_inst, int key,
                                    double velocity, int32_t length) {
    assert(p_inst);
    assert(p_inst->type == BPBXSYN_SYNTH_PICKED_STRING);
    picked_inst_s *inst = (picked_inst_s*)p_inst;

    bool continuation;
    bpbxsyn_voice_id id =
        bbsyn_trigger_voice(p_inst, GENERIC_LIST(inst->voices), key, velocity,
                            length, &continuation);

    if (!continuation) {
        picked_voice_s *voice = &inst->voices[id];
        *voice = (picked_voice_s) {
            .base = voice->base
        };
    }

    return id;
}

static void picked_note_off(bpbxsyn_synth_s *p_inst, bpbxsyn_voice_id id) {
    assert(p_inst);
    assert(p_inst->type == BPBXSYN_SYNTH_PICKED_STRING);
    picked_inst_s *inst = (picked_inst_s*)p_inst;

    bbsyn_release_voice(p_inst, GENERIC_LIST(inst->voices), id);
}

static void picked_note_all_off(bpbxsyn_synth_s *p_inst) {
    assert(p_inst);
    assert(p_inst->type == BPBXSYN_SYNTH_PICKED_STRING);
    picked_inst_s *inst = (picked_inst_s*)p_inst;

    bbsyn_release_all_voices(p_inst, GENERIC_LIST(inst->voices));
}

static void picked_sample_rate_changed(bpbxsyn_synth_s *p_inst, double old,
                                       double new)
{
    if (new == old) return;

    assert(p_inst);
    assert(p_inst->type == BPBXSYN_SYNTH_PICKED_STRING);
    picked_inst_s *inst = (picked_inst_s*)p_inst;
    const bpbxsyn_context_s *const ctx = inst->base.ctx;

    if (inst->delay_line_alloc)
        bpbxsyn_free(ctx, inst->delay_line_alloc);

    // The delay line buffer will get reused for other tones so might as well
    // start off with a buffer size that is big enough for most notes.
    const int likely_maximum_length = (int) ceil(4 * new / key_to_hz_d(12.0));
    int dl_size = bbsyn_fitting_power_of_two(likely_maximum_length);

    float *dl_alloc = bpbxsyn_malloc(ctx, DELAY_LINE_COUNT * dl_size * sizeof(float));
    if (!dl_alloc)
    {
        bbsyn_logmsgf(ctx, BPBXSYN_LOG_ERROR,
                      "picked string could not allocate delay lines!");
        
        // umm i don't know how the program would recover from this. uhhh.
        inst->delay_line_alloc = NULL;
        inst->delay_line_size = 0;
        return;
    }

    bbsyn_logmsgf(ctx, BPBXSYN_LOG_DEBUG,
                  "allocated %llu bytes of delay line buffers",
                  (size_t)dl_size * DELAY_LINE_COUNT * sizeof(float));

    inst->delay_line_size = dl_size;
    inst->delay_line_alloc = dl_alloc;
    for (int i = 0; i < BPBXSYN_SYNTH_MAX_VOICES; ++i)
    {
        for (int j = 0; j < UNISON_MAX_VOICES; ++j)
        {
            inst->voices[i].strings[j].delay_line = dl_alloc;
            dl_alloc += dl_size;
        }
    }
}

static void compute_voice(
    const bpbxsyn_synth_s *const base_inst, inst_base_voice_s *base_voice,
    voice_compute_s *compute_data
) {
    const picked_inst_s *const inst = (picked_inst_s*) base_inst;
    picked_voice_s *const voice = (picked_voice_s*) base_voice;

    const double sample_len = compute_data->varying.sample_len;
    const double rounded_samples_per_tick = compute_data->varying.rounded_samples_per_tick;

    voice_compute_varying_s *const varying = &compute_data->varying;

    const unison_desc_s unison = bbsyn_unison_info[inst->unison_type];

    double settings_expression_mult = PICKED_BASE_EXPRESSION;
    settings_expression_mult *= unison.expression * unison.voices / 2.0;

    const double interval_start = compute_data->varying.interval_start;
    const double interval_end = compute_data->varying.interval_end;
    const double start_pitch = voice->base.current_key + interval_start;
    const double end_pitch = voice->base.current_key + interval_end;

    // pitch expression
    double pitch_expression_start;
    if (voice->has_prev_pitch_expression) {
        pitch_expression_start = voice->prev_pitch_expression;
    } else {
        pitch_expression_start = calc_pitch_expression(start_pitch);
    }
    const double pitch_expression_end = calc_pitch_expression(end_pitch);
    voice->has_prev_pitch_expression = TRUE;
    voice->prev_pitch_expression = pitch_expression_end;

    double use_sustain_start = inst->sustain[0];
    double use_sustain_end = inst->sustain[1];
    voice->sustain_start = use_sustain_start;
    voice->sustain_end = use_sustain_end;

    // Increase expression to compensate for string decay.
    settings_expression_mult *= pow(2.0, 0.7 * (1.0 - use_sustain_start / BPBXSYN_PICKED_STRING_SUSTAIN_MAX));

    // calculate final expression
    const double expr_start = varying->expr_start * settings_expression_mult * pitch_expression_start;
    const double expr_end = varying->expr_end * settings_expression_mult * pitch_expression_end;
    
    const double unison_env_start = voice->base.env_computer.envelope_starts[BPBXSYN_ENV_INDEX_UNISON];
    const double unison_env_end = voice->base.env_computer.envelope_ends[BPBXSYN_ENV_INDEX_UNISON];

    const double freq_end_ratio = pow(2.0, (interval_end - interval_start) * 1.0 / 12.0);
    const double base_phase_delta_scale = pow(freq_end_ratio, 1.0 / rounded_samples_per_tick);

    const double start_freq = key_to_hz_d(start_pitch);

    // TODO: specialIntervalMult has to do with arpeggios/custom interval
    // But if there is none it will always be 1.0
    assert(UNISON_MAX_VOICES == 2);
    double unison_starts[UNISON_MAX_VOICES];
    double unison_ends[UNISON_MAX_VOICES];

    unison_starts[0] = pow(2.0, (unison.offset + unison.spread) * unison_env_start / 12.0);
    unison_ends[0] = pow(2.0, (unison.offset + unison.spread) * unison_env_end / 12.0);
    unison_starts[1] = pow(2.0, (unison.offset - unison.spread) * unison_env_start / 12.0)/* * specialIntervalMult*/;
    unison_ends[1] = pow(2.0, (unison.offset - unison.spread) * unison_env_end / 12.0)/* * specialIntervalMult*/;

    for (int i = 0; i < UNISON_MAX_VOICES; i++) {
        voice->phase_delta[i] = start_freq * sample_len * unison_starts[i];
        voice->phase_delta_scale[i] =
            base_phase_delta_scale * pow(unison_ends[i] / unison_starts[i], 1.0 / rounded_samples_per_tick);
    }
    
    voice->base.expression = expr_start;
    voice->base.expression_delta = (expr_end - expr_start) / rounded_samples_per_tick;

    double string_decay_start;
    if (voice->has_prev_string_decay) {
        string_decay_start = voice->prev_string_decay;
    } else {
        double sustain_envelope_start = voice->base.env_computer.envelope_starts[BPBXSYN_ENV_INDEX_STRING_SUSTAIN];
        string_decay_start =
            1.0 - min(1.0, sustain_envelope_start * voice->sustain_start / BPBXSYN_PICKED_STRING_SUSTAIN_MAX);
    }
    double sustain_envelope_end = voice->base.env_computer.envelope_ends[BPBXSYN_ENV_INDEX_STRING_SUSTAIN];
    double string_decay_end = 1.0 - min(1.0, sustain_envelope_end * voice->sustain_end / BPBXSYN_PICKED_STRING_SUSTAIN_MAX);
    voice->prev_string_decay = string_decay_end;

    // for (int i: number = tone.pickedStrings.length; i < unison.voices; i++) {
    //     tone.pickedStrings[i] = new PickedString();
    // }

    if (voice->at_note_start/* && !transition.continues && !tone.forceContinueAtStart*/) {
        for (int i = 0; i < UNISON_MAX_VOICES; ++i)
        {
            voice->strings[i].delay_index = -1;
        }
    }

    for (int i = 0; i < unison.voices; ++i)
    {
        pstring_update(&voice->strings[i], inst, voice, i,
                       rounded_samples_per_tick, string_decay_start,
                       string_decay_end, STRING_SUSTAIN_BRIGHT);
    }
}

static void picked_tick(bpbxsyn_synth_s *p_inst,
                     const bpbxsyn_tick_ctx_s *tick_ctx) {
    assert(p_inst);
    assert(p_inst->type == BPBXSYN_SYNTH_PICKED_STRING);
    picked_inst_s *inst = (picked_inst_s*)p_inst;

    bbsyn_inst_tick(p_inst, tick_ctx, &(audio_compute_s) {
        .voice_list = inst->voices,
        .sizeof_voice = sizeof(*inst->voices),
        .compute_voice = compute_voice,
        .userdata = NULL
    });

    inst->sustain[0] = inst->sustain[1];
}

static void picked_run(bpbxsyn_synth_s *p_inst, float *samples, size_t frame_count) {
    assert(p_inst);
    assert(p_inst->type == BPBXSYN_SYNTH_PICKED_STRING);
    picked_inst_s *inst = (picked_inst_s*)p_inst;
    (void)inst;

    memset(samples, 0, frame_count * sizeof(float));

    // for (int i = 0; i < BPBXSYN_SYNTH_MAX_VOICES; ++i) {
    //     pwm_voice_s *voice = &inst->voices[i];
    //     if (!voice_is_computing(&voice->base)) continue;

    //     // pre
    //     double phase_delta = voice->phase_delta;
    //     const double phase_delta_scale = voice->phase_delta_scale;
    //     double expression = voice->base.expression;
    //     const double expression_delta = voice->base.expression_delta;

    //     double phase = fmod(voice->phase, 1.0);
    //     double pulse_width = voice->pulse_width;
    //     const double pulse_width_delta = voice->pulse_width_delta;

    //     double x1 = voice->base.note_filter_input[0];
    //     double x2 = voice->base.note_filter_input[1];

    //     for (size_t smp = 0; smp < frame_count; ++smp) {
    //         const double saw_phase_a = fmod(phase, 1.0);
    //         const double saw_phase_b = fmod(phase + pulse_width, 1);

    //         double pulse_wave = saw_phase_b - saw_phase_a;
            
    //         // This is a PolyBLEP, which smooths out discontinuities at any
    //         // frequency to reduce aliasing.
    //         if (!inst->aliases) {
    //             if (saw_phase_a < phase_delta) {
    //                 double t = saw_phase_a / phase_delta;
    //                 pulse_wave += (t + t - t * t - 1) * 0.5;
    //             } else if (saw_phase_a > 1.0 - phase_delta) {
    //                 double t = (saw_phase_a - 1.0) / phase_delta;
    //                 pulse_wave += (t + t + t * t + 1) * 0.5;
    //             }
    //             if (saw_phase_b < phase_delta) {
    //                 double t = saw_phase_b / phase_delta;
    //                 pulse_wave -= (t + t - t * t - 1) * 0.5;
    //             } else if (saw_phase_b > 1.0 - phase_delta) {
    //                 double t = (saw_phase_b - 1.0) / phase_delta;
    //                 pulse_wave -= (t + t + t * t + 1) * 0.5;
    //             }
    //         }

    //         const double x0 = pulse_wave;
    //         double sample =
    //             bbsyn_apply_filters(x0, x1, x2, voice->base.note_filters);
    //         x2 = x1;
    //         x1 = x0;

    //         phase += phase_delta;
    //         phase_delta *= phase_delta_scale;
    //         pulse_width += pulse_width_delta;

    //         const double output = sample * expression;
    //         expression += expression_delta;

    //         samples[smp] += (float)output;
    //     }

    //     // post
    //     voice->phase = phase;
    //     voice->phase_delta = phase_delta;
    //     voice->base.expression = expression;
    //     voice->pulse_width = pulse_width;

    //     bbsyn_sanitize_filters(voice->base.note_filters, FILTER_GROUP_COUNT);
    //     voice->base.note_filter_input[0] = x1;
    //     voice->base.note_filter_input[1] = x2;
    // }
}






//////////
// DATA //
//////////

static const bpbxsyn_param_info_s picked_param_info[] = {
    {
        .type = BPBXSYN_PARAM_UINT8,
        .flags = BPBXSYN_PARAM_FLAG_NO_AUTOMATION,

        .id = "psUnison",
        .name = "Unison",
        .group = "Picked String",
        .min_value = 0,
        .max_value = BPBXSYN_UNISON_COUNT - 1,
        .default_value = BPBXSYN_UNISON_NONE,

        .enum_values = bbsyn_unison_enum_values
    },

    {
        .type = BPBXSYN_PARAM_DOUBLE,

        .id = "psSustai",
        .name = "Sustain",
        .group = "Picked String",
        .min_value = 0,
        .max_value = BPBXSYN_PICKED_STRING_SUSTAIN_MAX,
        .default_value = 10.0,
    },
    
    {
        .type = BPBXSYN_PARAM_UINT8,
        .flags = BPBXSYN_PARAM_FLAG_NO_AUTOMATION,

        .id = "psHarm01",
        .name = "Harmonics 1x",
        .group = "Picked String",
        .min_value = 0,
        .max_value = BPBXSYN_HARMONICS_CONTROL_MAX,
        .default_value = 0.0,
    },
    {
        .type = BPBXSYN_PARAM_UINT8,
        .flags = BPBXSYN_PARAM_FLAG_NO_AUTOMATION,

        .id = "psHarm02",
        .name = "Harmonics 2x",
        .group = "Picked String",
        .min_value = 0,
        .max_value = BPBXSYN_HARMONICS_CONTROL_MAX,
        .default_value = 0.0,
    },
    {
        .type = BPBXSYN_PARAM_UINT8,
        .flags = BPBXSYN_PARAM_FLAG_NO_AUTOMATION,

        .id = "psHarm03",
        .name = "Harmonics 3x",
        .group = "Picked String",
        .min_value = 0,
        .max_value = BPBXSYN_HARMONICS_CONTROL_MAX,
        .default_value = 0.0,
    },
    {
        .type = BPBXSYN_PARAM_UINT8,
        .flags = BPBXSYN_PARAM_FLAG_NO_AUTOMATION,

        .id = "psHarm04",
        .name = "Harmonics 4x",
        .group = "Picked String",
        .min_value = 0,
        .max_value = BPBXSYN_HARMONICS_CONTROL_MAX,
        .default_value = 0.0,
    },
    {
        .type = BPBXSYN_PARAM_UINT8,
        .flags = BPBXSYN_PARAM_FLAG_NO_AUTOMATION,

        .id = "psHarm05",
        .name = "Harmonics 5x",
        .group = "Picked String",
        .min_value = 0,
        .max_value = BPBXSYN_HARMONICS_CONTROL_MAX,
        .default_value = 0.0,
    },
    {
        .type = BPBXSYN_PARAM_UINT8,
        .flags = BPBXSYN_PARAM_FLAG_NO_AUTOMATION,

        .id = "psHarm06",
        .name = "Harmonics 6x",
        .group = "Picked String",
        .min_value = 0,
        .max_value = BPBXSYN_HARMONICS_CONTROL_MAX,
        .default_value = 0.0,
    },
    {
        .type = BPBXSYN_PARAM_UINT8,
        .flags = BPBXSYN_PARAM_FLAG_NO_AUTOMATION,

        .id = "psHarm07",
        .name = "Harmonics 7x",
        .group = "Picked String",
        .min_value = 0,
        .max_value = BPBXSYN_HARMONICS_CONTROL_MAX,
        .default_value = 0.0,
    },
    {
        .type = BPBXSYN_PARAM_UINT8,
        .flags = BPBXSYN_PARAM_FLAG_NO_AUTOMATION,

        .id = "psHarm08",
        .name = "Harmonics 8x",
        .group = "Picked String",
        .min_value = 0,
        .max_value = BPBXSYN_HARMONICS_CONTROL_MAX,
        .default_value = 0.0,
    },
    {
        .type = BPBXSYN_PARAM_UINT8,
        .flags = BPBXSYN_PARAM_FLAG_NO_AUTOMATION,

        .id = "psHarm09",
        .name = "Harmonics 9x",
        .group = "Picked String",
        .min_value = 0,
        .max_value = BPBXSYN_HARMONICS_CONTROL_MAX,
        .default_value = 0.0,
    },
    {
        .type = BPBXSYN_PARAM_UINT8,
        .flags = BPBXSYN_PARAM_FLAG_NO_AUTOMATION,

        .id = "psHarm10",
        .name = "Harmonics 10x",
        .group = "Picked String",
        .min_value = 0,
        .max_value = BPBXSYN_HARMONICS_CONTROL_MAX,
        .default_value = 0.0,
    },
    {
        .type = BPBXSYN_PARAM_UINT8,
        .flags = BPBXSYN_PARAM_FLAG_NO_AUTOMATION,

        .id = "psHarm11",
        .name = "Harmonics 11x",
        .group = "Picked String",
        .min_value = 0,
        .max_value = BPBXSYN_HARMONICS_CONTROL_MAX,
        .default_value = 0.0,
    },
    {
        .type = BPBXSYN_PARAM_UINT8,
        .flags = BPBXSYN_PARAM_FLAG_NO_AUTOMATION,

        .id = "psHarm12",
        .name = "Harmonics 12x",
        .group = "Picked String",
        .min_value = 0,
        .max_value = BPBXSYN_HARMONICS_CONTROL_MAX,
        .default_value = 0.0,
    },
    {
        .type = BPBXSYN_PARAM_UINT8,
        .flags = BPBXSYN_PARAM_FLAG_NO_AUTOMATION,

        .id = "psHarm13",
        .name = "Harmonics 13x",
        .group = "Picked String",
        .min_value = 0,
        .max_value = BPBXSYN_HARMONICS_CONTROL_MAX,
        .default_value = 0.0,
    },
    {
        .type = BPBXSYN_PARAM_UINT8,
        .flags = BPBXSYN_PARAM_FLAG_NO_AUTOMATION,

        .id = "psHarm14",
        .name = "Harmonics 14x",
        .group = "Picked String",
        .min_value = 0,
        .max_value = BPBXSYN_HARMONICS_CONTROL_MAX,
        .default_value = 0.0,
    },
    {
        .type = BPBXSYN_PARAM_UINT8,
        .flags = BPBXSYN_PARAM_FLAG_NO_AUTOMATION,

        .id = "psHarm15",
        .name = "Harmonics 15x",
        .group = "Picked String",
        .min_value = 0,
        .max_value = BPBXSYN_HARMONICS_CONTROL_MAX,
        .default_value = 0.0,
    },
    {
        .type = BPBXSYN_PARAM_UINT8,
        .flags = BPBXSYN_PARAM_FLAG_NO_AUTOMATION,

        .id = "psHarm16",
        .name = "Harmonics 16x",
        .group = "Picked String",
        .min_value = 0,
        .max_value = BPBXSYN_HARMONICS_CONTROL_MAX,
        .default_value = 0.0,
    },
    {
        .type = BPBXSYN_PARAM_UINT8,
        .flags = BPBXSYN_PARAM_FLAG_NO_AUTOMATION,

        .id = "psHarm17",
        .name = "Harmonics 17x",
        .group = "Picked String",
        .min_value = 0,
        .max_value = BPBXSYN_HARMONICS_CONTROL_MAX,
        .default_value = 0.0,
    },
    {
        .type = BPBXSYN_PARAM_UINT8,
        .flags = BPBXSYN_PARAM_FLAG_NO_AUTOMATION,

        .id = "psHarm18",
        .name = "Harmonics 18x",
        .group = "Picked String",
        .min_value = 0,
        .max_value = BPBXSYN_HARMONICS_CONTROL_MAX,
        .default_value = 0.0,
    },
    {
        .type = BPBXSYN_PARAM_UINT8,
        .flags = BPBXSYN_PARAM_FLAG_NO_AUTOMATION,

        .id = "psHarm19",
        .name = "Harmonics 19x",
        .group = "Picked String",
        .min_value = 0,
        .max_value = BPBXSYN_HARMONICS_CONTROL_MAX,
        .default_value = 0.0,
    },
    {
        .type = BPBXSYN_PARAM_UINT8,
        .flags = BPBXSYN_PARAM_FLAG_NO_AUTOMATION,

        .id = "psHarm20",
        .name = "Harmonics 20x",
        .group = "Picked String",
        .min_value = 0,
        .max_value = BPBXSYN_HARMONICS_CONTROL_MAX,
        .default_value = 0.0,
    },
    {
        .type = BPBXSYN_PARAM_UINT8,
        .flags = BPBXSYN_PARAM_FLAG_NO_AUTOMATION,

        .id = "psHarm21",
        .name = "Harmonics 21x",
        .group = "Picked String",
        .min_value = 0,
        .max_value = BPBXSYN_HARMONICS_CONTROL_MAX,
        .default_value = 0.0,
    },
    {
        .type = BPBXSYN_PARAM_UINT8,
        .flags = BPBXSYN_PARAM_FLAG_NO_AUTOMATION,

        .id = "psHarm22",
        .name = "Harmonics 22x",
        .group = "Picked String",
        .min_value = 0,
        .max_value = BPBXSYN_HARMONICS_CONTROL_MAX,
        .default_value = 0.0,
    },
    {
        .type = BPBXSYN_PARAM_UINT8,
        .flags = BPBXSYN_PARAM_FLAG_NO_AUTOMATION,

        .id = "psHarm23",
        .name = "Harmonics 23x",
        .group = "Picked String",
        .min_value = 0,
        .max_value = BPBXSYN_HARMONICS_CONTROL_MAX,
        .default_value = 0.0,
    },
    {
        .type = BPBXSYN_PARAM_UINT8,
        .flags = BPBXSYN_PARAM_FLAG_NO_AUTOMATION,

        .id = "psHarm24",
        .name = "Harmonics 24x",
        .group = "Picked String",
        .min_value = 0,
        .max_value = BPBXSYN_HARMONICS_CONTROL_MAX,
        .default_value = 0.0,
    },
    {
        .type = BPBXSYN_PARAM_UINT8,
        .flags = BPBXSYN_PARAM_FLAG_NO_AUTOMATION,

        .id = "psHarm25",
        .name = "Harmonics 25x",
        .group = "Picked String",
        .min_value = 0,
        .max_value = BPBXSYN_HARMONICS_CONTROL_MAX,
        .default_value = 0.0,
    },
    {
        .type = BPBXSYN_PARAM_UINT8,
        .flags = BPBXSYN_PARAM_FLAG_NO_AUTOMATION,

        .id = "psHarm26",
        .name = "Harmonics 26x",
        .group = "Picked String",
        .min_value = 0,
        .max_value = BPBXSYN_HARMONICS_CONTROL_MAX,
        .default_value = 0.0,
    },
    {
        .type = BPBXSYN_PARAM_UINT8,
        .flags = BPBXSYN_PARAM_FLAG_NO_AUTOMATION,

        .id = "psHarm27",
        .name = "Harmonics 27x",
        .group = "Picked String",
        .min_value = 0,
        .max_value = BPBXSYN_HARMONICS_CONTROL_MAX,
        .default_value = 0.0,
    },
    {
        .type = BPBXSYN_PARAM_UINT8,
        .flags = BPBXSYN_PARAM_FLAG_NO_AUTOMATION,

        .id = "psHarm28",
        .name = "Harmonics 28x",
        .group = "Picked String",
        .min_value = 0,
        .max_value = BPBXSYN_HARMONICS_CONTROL_MAX,
        .default_value = 0.0,
    },
};

static const size_t picked_param_addresses[] = {
    offsetof(picked_inst_s, unison_type),
    offsetof(picked_inst_s, sustain[1]),
    offsetof(picked_inst_s, harmonics[0]),
    offsetof(picked_inst_s, harmonics[1]),
    offsetof(picked_inst_s, harmonics[2]),
    offsetof(picked_inst_s, harmonics[3]),
    offsetof(picked_inst_s, harmonics[4]),
    offsetof(picked_inst_s, harmonics[5]),
    offsetof(picked_inst_s, harmonics[6]),
    offsetof(picked_inst_s, harmonics[7]),
    offsetof(picked_inst_s, harmonics[8]),
    offsetof(picked_inst_s, harmonics[9]),
    offsetof(picked_inst_s, harmonics[10]),
    offsetof(picked_inst_s, harmonics[11]),
    offsetof(picked_inst_s, harmonics[12]),
    offsetof(picked_inst_s, harmonics[13]),
    offsetof(picked_inst_s, harmonics[14]),
    offsetof(picked_inst_s, harmonics[15]),
    offsetof(picked_inst_s, harmonics[16]),
    offsetof(picked_inst_s, harmonics[17]),
    offsetof(picked_inst_s, harmonics[18]),
    offsetof(picked_inst_s, harmonics[19]),
    offsetof(picked_inst_s, harmonics[20]),
    offsetof(picked_inst_s, harmonics[21]),
    offsetof(picked_inst_s, harmonics[22]),
    offsetof(picked_inst_s, harmonics[23]),
    offsetof(picked_inst_s, harmonics[24]),
    offsetof(picked_inst_s, harmonics[25]),
    offsetof(picked_inst_s, harmonics[26]),
    offsetof(picked_inst_s, harmonics[27]),
};

static const bpbxsyn_envelope_compute_index_e picked_env_targets[] = {
    BPBXSYN_ENV_INDEX_UNISON,
    BPBXSYN_ENV_INDEX_STRING_SUSTAIN,
};

const inst_vtable_s bbsyn_inst_picked_vtable = {
    .struct_size = sizeof(picked_inst_s),

    .param_count = BPBXSYN_PULSE_WIDTH_PARAM_COUNT,
    .param_info = picked_param_info,
    .param_addresses = picked_param_addresses,

    .envelope_target_count = 2,
    .envelope_targets = picked_env_targets,

    .inst_init = picked_init,
    .inst_note_on = picked_note_on,
    .inst_note_off = picked_note_off,
    .inst_note_all_off = picked_note_all_off,
    .inst_sample_rate_changed = picked_sample_rate_changed,

    .inst_tick = picked_tick,
    .inst_run = picked_run
};