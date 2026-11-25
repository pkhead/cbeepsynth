// constants/defines shared between supersaw and pwm
#ifndef SYNTH_PWM_H
#define SYNTH_PWM_H

// It's actually closer to half of this, the synthesized pulse amplitude range
// is only .5 to -.5, but also note that the fundamental sine partial amplitude
// of a square wave is 4/π times the measured square wave amplitude.
#define PWM_BASE_EXPRESSION 0.04725

#define pulse_width_ratio(x) ((double)(x) / (BBXS_PULSE_WIDTH_RANGE * 2))

#endif