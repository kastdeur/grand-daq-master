#include<string.h>
#include "fftw3.h"
#include "fftdata.h"


fftw_plan fftpf,fftpb;

fftw_complex *fftin=NULL,*fftout=NULL;

int fft_len=0;


int iswap=0;
short *datbuf=NULL;


void fft_init(int len)
{
  if(fftin != NULL) fftw_free(fftin);
  if(fftout != NULL) fftw_free(fftout);
  if(fftpf != NULL) fftw_destroy_plan(fftpf);
  if(fftpb != NULL) fftw_destroy_plan(fftpb);
  fftin=(fftw_complex *)fftw_malloc(sizeof(fftw_complex)*len);
  fftout=(fftw_complex *)fftw_malloc(sizeof(fftw_complex)*len);
  fftpf=fftw_plan_dft_1d(len,fftin,fftout,FFTW_FORWARD,FFTW_MEASURE);
  fftpb=fftw_plan_dft_1d(len,fftout,fftin,FFTW_BACKWARD,FFTW_MEASURE);
  fft_len = len;
}

void fft_forward(float input[][2], float output[][2])
{int i;
  for(i=0;i<fft_len;i++){
    fftin[i][0] = input[i][0];
    fftin[i][1] = input[i][1];
  }
  fftw_execute(fftpf);
  for(i=0;i<fft_len;i++){
    output[i][0] = fftout[i][0];
    output[i][1] = fftout[i][1];
  }
}

void fft_backward(float input[][2],float output[][2])
{
  int i;

  for(i=0;i<fft_len;i++){
    fftout[i][0] = input[i][0];
    fftout[i][1] = input[i][1];
  }
  fftw_execute(fftpb);
  for(i=0;i<fft_len;i++){
    output[i][0] = fftin[i][0]/fft_len;
    output[i][1] = fftin[i][1]/fft_len;
  }
}

void envelope(float *in, float *out)
{
  int i;
  float tmp;
  for(i=0;i<fft_len;i++){
    fftin[i][0] = in[i];
    fftin[i][1] = 0.;
  }
  fftw_execute(fftpf);
  for(i=0;i<fft_len;i++){
    if(i<(fft_len/2)) {
      tmp = fftout[i][1];
      fftout[i][1] = -fftout[i][0];
    }
    else {
      tmp = -fftout[i][1];
      fftout[i][1] = fftout[i][0];
    }
    fftout[i][0] = tmp;
  }
  fftw_execute(fftpb);
  for(i=0;i<fft_len;i++){
    out[i] = fftin[i][0]*fftin[i][0]+fftin[i][1]*fftin[i][1];
    out[i] = sqrt(out[i]/(fft_len*fft_len)+in[i]*in[i]);
  }
}
