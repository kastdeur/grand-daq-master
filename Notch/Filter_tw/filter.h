// filter.h     27-Feb-2012

// Thei: choose between 12 bit or 14 bit ADC

#define ADC12 1
//#define ADC14 1

#ifdef ADC12
#define W_INPUT 12  //14b: adcbits:    14   //12b: 12
#define W_FRAC  10  //14b: w_input-2:  12   //12b: 10
#define W_INT   22  //14b: 32-w_frac:  20   //12b: 22
#endif

#ifdef ADC14
#define W_INPUT 14  //14b: adcbits:    14   //12b: 12
#define W_FRAC  12  //14b: w_input-2:  12   //12b: 10
#define W_INT   20  //14b: 32-w_frac:  20   //12b: 22
#endif

#define SAMP_FREQ 200.0

int getNotchFilterCoeffs(double nu_s, double r, int xtraPipe, int *a, int *b, int *aLen, int *bLen);

unsigned short iirNotchFixed(int reset, unsigned short in, double nu, double r, int debug);
unsigned short iirNotchPipe2(int reset, unsigned short in, double nu, double r, int debug);
unsigned short iirNotchPipe4(int reset, unsigned short in, double nu, double r, int debug);
unsigned short iirNotchType2(int reset, unsigned short in, double nu, double r);

