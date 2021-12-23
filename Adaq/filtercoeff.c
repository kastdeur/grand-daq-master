// filtercoeff.c

#include <stdio.h>
#include <math.h>

#define W_INPUT 14      //14b: adcbits:   14   //12b: 12
#define W_FRAC  12      //14b: w_input-2: 12   //12b: 10
#define W_INT   20      //14b: 32-w_frac: 20   //12b: 22
#define SAMP_FREQ 500.0

//-------------------------------------------------------------------
// Float to fixed-point conversion with rounding
//

int float2fixed(float x, int len_int, int len_frac) {

  int f;

  if (len_int + len_frac > 32) {
    printf("ERROR: too many bits!  Must fit in 32b int!\n");
    return 0;
  }

  // Round-to-nearest
  f = (int)(x*(1 << len_frac) + (x < 0 ? -1.0 : 1.0)*0.5);
  f &= 0xffffffff >> (32-len_int-len_frac);
  
  return f;
}

//----------------------------------------------------------
//
// Get IIR notch filter coefficients in fixed point
//
// Number and calculation of coefficients depends on pipeline
// stages of feedback loop, so the calling method still 
// needs to keep track of the coefficient indices.  
//
// We encapsulate this here just for use in programs that, for
// example, only need to calculate the coefficients for DAQ use.
//
int getNotchFilterCoeffs(double nu_s, double r, int xtraPipe
    , int *a, int *b, int *aLen, int *bLen) {

    double a_dbl[10], b_dbl[10];
    int i;

    *aLen = 3;

    switch (xtraPipe) {
    case 0:
        *bLen = 3;
        a_dbl[1] = -2 * r * cos(2*3.1416*nu_s);
        a_dbl[2] = r*r;
        
        b_dbl[0] = 1;
        b_dbl[1] = -2 * cos(2*3.1416*nu_s);
        b_dbl[2] = 1;
        break;

    case 2:
        *bLen = 5;
        a_dbl[1] = 2 * r * r * cos(2*2*3.1416*nu_s);
        a_dbl[2] = -r*r*r*r;
        
        b_dbl[0] = 1;
        b_dbl[1] = -2 * cos(2*3.1416*nu_s);
        b_dbl[2] = 1;
        b_dbl[3] = 2 * r * cos(2*3.1416*nu_s);
        b_dbl[4] = r*r;
        break;

    case 4:
        *bLen = 7;
        a_dbl[1] = 2 * r * r * r * r * cos(4*2*3.1416*nu_s);
        a_dbl[2] = -r*r*r*r*r*r*r*r;
        
        b_dbl[0] = 1;
        b_dbl[1] = -2 * cos(2*3.1416*nu_s);
        b_dbl[2] = 1;
        b_dbl[3] = 2 * r * cos(2*3.1416*nu_s);
        b_dbl[4] = r*r;
        b_dbl[5] = 2 * r * r* cos(2*2*3.1416*nu_s);
        b_dbl[6] = r*r*r*r;
        break;

    default:
        printf("ERROR: extra pipeline stage = %d not supported!", xtraPipe);
        return -1;
    }

    a[1] = float2fixed(a_dbl[1], W_INT, W_FRAC);
    a[2] = float2fixed(a_dbl[2], W_INT, W_FRAC);

    for (i = 0; i < *bLen; i++) {
        b[i] = float2fixed(b_dbl[i], W_INT, W_FRAC);    
    }
    //printf("    A\t\t\tB\n");
    //printf("0    \t\t\t%.3f (0x%03x)\n", b_dbl[0], b[0] & 0xfff);
    //printf("1   %.3f (0x%03x)\t%.3f (0x%03x)\n", a_dbl[1], a[1] & 0xfff, b_dbl[1], b[1] & 0xfff);
    //printf("2   %.3f (0x%03x)\t%.3f (0x%03x)\n", a_dbl[2], a[2] & 0xfff, b_dbl[2], b[2] & 0xfff);
    //for (i = *aLen; i < *bLen; i++)
    //    printf("%d    \t\t\t%.3f (0x%03x)\n", i, b_dbl[i], b[i] & 0xfff);
    //printf("\n");
    return 0;
}

//----------------------------------------------------------
