#include "fft.h"

#include <math.h>
#include <assert.h>
#include <stdbool.h>
#include "util.h"

static int count_bits(unsigned int n) {
    assert(n != 0);
    assert(is_power_of_two(n) && "FFT array length must be a power of 2!");

    // return Math.round(Math.log(n) / Math.log(2));
    // I feel like use the math log for this is a bit weird.
    // so i'm just iteratively shifting it to count the bits.
    // using ctz here instead would be Cool...
    // (or clz, which i heard is more widely supported wrt cpu architectures)
    // But... eh. Not like the One microsecond of performance matters.
    int ret;
    for (ret = 0; n != 1; n >>= 1, ret++);
    return ret;
}

void bbsyn_fft_scale_array(float *array, size_t length, double factor) {
	const float factor_f = (float)factor;

    // i wonder if the compiler will vectorize this
    for (size_t i = 0; i < length; i++) {
        array[i] *= factor_f;
    }
}

// Rearranges the elements of the array, swapping the element at an index
// with an element at an index that is the bitwise reverse of the first
// index in base 2. Useful for computing the FFT.
static void reverse_index_bits(float *array, unsigned int array_length) {
    const int bit_count = count_bits(array_length);
    assert(bit_count <= 16 && "FFT array length must not be greater than 2^16");
    const unsigned int final_shift = 16 - bit_count;
    for (unsigned int i = 0; i < array_length; i++) {
        // TODO: bit order reversal intrinsics?
        unsigned int j;
		j = ((i & 0xaaaa) >> 1) | ((i & 0x5555) << 1);
		j = ((j & 0xcccc) >> 2) | ((j & 0x3333) << 2);
		j = ((j & 0xf0f0) >> 4) | ((j & 0x0f0f) << 4);
        j = ((j           >> 8) | ((j &   0xff) << 8)) >> final_shift;
		
		if (j > i) {
			float temp = array[i];
			array[i] = array[j];
			array[j] = temp;
		}
    }
}

void bbsyn_fft_inverse_real_fourier_transform(float *array,
											  unsigned int array_length) {
    assert(array_length >= 4 && "FFT array length must be at least 4.");
	const int total_passes = count_bits(array_length);

	// Perform all but the last few passes in reverse.
	for (int pass = total_passes - 1; pass >= 2; pass--) {
		const int sub_stride = 1 << pass;
		const int mid_sub_stride = sub_stride >> 1;
		const int stride = sub_stride << 1;
		const double radians_increment = PI2 / stride;
		const double cos_increment = cos(radians_increment);
		const double sin_increment = sin(radians_increment);
		const double oscillator_multiplier = 2.0 * cos_increment;
			
		for (unsigned int startIndex = 0; startIndex < array_length; startIndex += stride) {
			const int start_index_a = startIndex;
			const int mid_index_a = start_index_a + mid_sub_stride;
			const int start_index_b = start_index_a + sub_stride;
			const int mid_index_b = start_index_b + mid_sub_stride;
			const int stop_index = start_index_b + sub_stride;
			const double real_start_a = (double)array[start_index_a];
			const double imag_start_b = (double)array[start_index_b];
			array[start_index_a] = (float)(real_start_a + imag_start_b);
			array[mid_index_a] *= 2;
			array[start_index_b] = (float)(real_start_a - imag_start_b);
			array[mid_index_b] *= 2;
			double c = cos_increment;
			double s = -sin_increment;
			double c_prev = 1.0;
			double s_prev = 0.0;
			for (int index = 1; index < mid_sub_stride; index++) {
				const int index_a0 = start_index_a + index;
				const int index_a1 = start_index_b - index;
				const int index_b0 = start_index_b + index;
                const int index_b1 = stop_index   - index;
				const double real0 = (double)array[index_a0];
				const double real1 = (double)array[index_a1];
				const double imag0 = (double)array[index_b0];
				const double imag1 = (double)array[index_b1];
				const double tempA = real0 - real1;
				const double tempB = imag0 + imag1;
				array[index_a0] = (float)(real0 + real1);
				array[index_a1] = (float)(imag1 - imag0);
				array[index_b0] = (float)(tempA * c - tempB * s);
				array[index_b1] = (float)(tempB * c + tempA * s);
				const double c_temp = oscillator_multiplier * c - c_prev;
				const double s_temp = oscillator_multiplier * s - s_prev;
				c_prev = c;
				s_prev = s;
				c = c_temp;
				s = s_temp;
			}
		}
	}
	/*
	// Commented out this block (and compensated with an extra pass above)
	// because it's slower in my testing so far.
	// Pass with stride 8.
	const sqrt2over2: number = Math.sqrt(2.0) / 2.0;
	for (let index: number = 0; index < fullArrayLength; index += 8) {
		const index1: number = index + 1;
		const index2: number = index + 2;
		const index3: number = index + 3;
		const index4: number = index + 4;
		const index5: number = index + 5;
		const index6: number = index + 6;
		const index7: number = index + 7;
		const real0: number = array[index ];
		const real1: number = array[index1];
		const real2: number = array[index2];
		const real3: number = array[index3];
		const imag4: number = array[index4];
		const imag5: number = array[index5];
		const imag6: number = array[index6];
		const imag7: number = array[index7];
		const tempA: number = real1 - real3;
		const tempB: number = imag5 + imag7;
		array[index ] = real0 + imag4;
		array[index1] = real1 + real3;
		array[index2] = real2 * 2;
		array[index3] = imag7 - imag5;
		array[index4] = real0 - imag4;
		array[index5] = (tempA + tempB) * sqrt2over2;
		array[index6] = imag6 * 2;
		array[index7] = (tempB - tempA) * sqrt2over2;
	}
	*/
	// The final passes with strides 4 and 2, combined into one loop.
	for (unsigned int index = 0; index < array_length; index += 4) {
		const int index1 = index + 1;
		const int index2 = index + 2;
		const int index3 = index + 3;
        const double real0 = (double)array[index ];
		const double real1 = (double)array[index1] * 2;
		const double imag2 = (double)array[index2];
		const double imag3 = (double)array[index3] * 2;
		const double tempA = real0 + imag2;
		const double tempB = real0 - imag2;
        array[index ] = (float)(tempA + real1);
		array[index1] = (float)(tempA - real1);
		array[index2] = (float)(tempB + imag3);
		array[index3] = (float)(tempB - imag3);
	}
		
	reverse_index_bits(array, array_length);
}