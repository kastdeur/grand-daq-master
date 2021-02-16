#include<math.h>

void fft_init(int len);
void fft_forward(float input[][2], float output[][2]);
void fft_backward(float input[][2], float output[][2]);
void envelope(float *in,float *out);
