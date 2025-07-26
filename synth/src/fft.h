#ifndef _fft_h_
#define _fft_h_

#include <stddef.h>

// A basic FFT operation scales the overall magnitude of elements by the
// square root of the length of the array, √N. Performing a forward FFT and
// then an inverse FFT results in the original array, but multiplied by N.
// This helper function can be used to compensate for that. 
void fft_scale_array(float *array, size_t length, double factor);

// Computes the inverse Fourier transform from a specially formatted array of
// scalar values. Elements 0 through N/2 are expected to be the real values of
// the corresponding complex elements, representing cosine wave amplitudes in
// ascending frequency, and elements N/2+1 through N-1 correspond to the
// imaginary values, representing sine wave amplitudes in descending frequency.
// Generates real-valued time-domain samples. Overwrites the input array.
void fft_inverse_real_fourier_transform(float *array, unsigned int array_length);

#endif