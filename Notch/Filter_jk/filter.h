#define W_FRAC 10
#define W_INT  22
#define W_INPUT 12

#define SAMP_FREQ 200.0

int getNotchFilterCoeffs(double nu_s, double r, int xtraPipe, int *a, int *b, int *aLen, int *bLen);
unsigned short iirNotchFixed(int reset, unsigned short in, double nu, double r);
unsigned short iirNotchPipe2(int reset, unsigned short in, double nu, double r);
unsigned short iirNotchPipe4(int reset, unsigned short in, double nu, double r);
unsigned short iirNotchType2(int reset, unsigned short in, double nu, double r);
