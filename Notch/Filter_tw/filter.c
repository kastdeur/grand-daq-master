// filter.c     11-Jun-2013 (14 bit corrections in debug statements)
// filter.c     27-Feb-2012 (12/14 bit)

#include <stdio.h>
#include <math.h>

#include "filter.h"

// no longer use NotchType2 filter (turned off by #if NotchType2)

//-------------------------------------------------------------------
// Float to fixed-point conversion with rounding
//

int float2fixed(float x, int len_int, int len_frac)
{
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
int getNotchFilterCoeffs(double nu_s, double r, int xtraPipe, int *a, int *b, int *aLen, int *bLen) {

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

    printf("# Filter: %g MHz on sampling frequency of %g MHz (r = %g)\n", nu_s*SAMP_FREQ, SAMP_FREQ, r);
    printf("# \n");
    printf("#     A\t\t\tB\n");
    printf("# 0    \t\t\t%.3f\t(0x%04x)\n", b_dbl[0], b[0] & 0x3fff);
    printf("# 1   %.3f (0x%04x)\t%.3f\t(0x%04x)\n", a_dbl[1], a[1] & 0x3fff, b_dbl[1], b[1] & 0x3fff);
    printf("# 2   %.3f (0x%04x)\t%.3f\t(0x%04x)\n", a_dbl[2], a[2] & 0x3fff, b_dbl[2], b[2] & 0x3fff);
    for (i = *aLen; i < *bLen; i++) {
        printf("# %d    \t\t\t%.3f\t(0x%04x)\n", i, b_dbl[i], b[i] & 0x3fff);
    }
    printf("#\n");

    return 0;
}


//----------------------------------------------------------
//
// IIR notch filter implementation in fixed point
//
// See e.g. C. Tseng, IEEE Trans. Sig. Process. 49, 2673 (2001).
// Implementation of Type I single-frequency notch filter.
//
// Retains state from one call to next to implement feedforward 
// and feedback.
//

unsigned short iirNotchFixed(int reset, unsigned short in, double nu, double r, int debug)
{
    short in_signed;
    int mul_b1, mul_b2, mul_a1, mul_a2;
    int add_a1, add_a2, add_a3;
    int out_int;

    static int a[3];
    static int b[3];
    static int aLen, bLen;

    // Output and saved samples for feedback / feedforward    
    static short in_dly1 = 0;
    static short in_dly2 = 0;

    static short out = 0;
    static short out_dly1 = 0;
    static short out_dly2 = 0;

    //------------------------------------------------------
    // Calculated coefficients if the filter is not 
    // initialized
    
    if (reset) {
        getNotchFilterCoeffs(nu/SAMP_FREQ, r, 0, a, b, &aLen, &bLen);

        if (debug == 1) { //debugthei
          printf("    in_sig mul_a1 mul_b1 mul_a2 mul_a2 add_a1 add_a2 add_a3 outint out outc\n");
          printf("\n");
        }

        in_dly1 = in_dly2 = out = out_dly1 = out_dly2 = 0;
    }

    //------------------------------------------------------
    // Rescale input to 12b signed
    in_signed = (short)in - (short)(1 << (W_INPUT-1));

    //------------------------------------------------------
    // 12b * 12b
    mul_a2 = a[2]*out_dly2;
    mul_b2 = (int)in_dly2 << W_FRAC;
    
    // 12b * 12b
    mul_a1 = a[1]*out_dly1;
    mul_b1 = b[1]*in_dly1;
    
    // 24b + 24b
    add_a2 = mul_b2-mul_a2;
    add_a1 = mul_b1-mul_a1;

    add_a3 = add_a2+add_a1;

    out_int = ((int)in_signed << W_FRAC) + add_a3;

    out = (short)(out_int >> W_FRAC);

#if ADC12
    //DEBUG
    //printf("mul_a2 %d  mul_b2 %d  mul_a1 %d  mul_b1 %d   out_int %d  out %d\n", mul_a2, mul_b2, mul_a1, mul_b1, out_int, out);
    //
    if (debug == 1) { //debugthei
      printf("int. %03X %03X %06X %06X %06X %06X %06X %06X %06X %06X %03X"
             , in, in_signed&0x0fff, mul_a1&0xffffff, mul_b1&0xffffff, mul_a2&0xffffff
             , mul_a2&0xffffff, add_a1&0xffffff, add_a2&0xffffff
             , add_a3&0xffffff, out_int&0xffffff, out&0x0fff);
    }
    if (debug != 0) {
      printf("iir1 %03X %03X %03X\n", in, in_signed&0x0fff, out&0xfff);
    }
#endif
#if ADC14
    //DEBUG
    //printf("mul_a2 %d  mul_b2 %d  mul_a1 %d  mul_b1 %d   out_int %d  out %d\n", mul_a2, mul_b2, mul_a1, mul_b1, out_int, out);
    //
    if (debug == 1) { //debugthei
      printf("int. %04X %04X %06X %06X %06X %06X %06X %06X %06X %06X %04X"
             , in, in_signed&0x3fff, mul_a1&0xffffff, mul_b1&0xffffff, mul_a2&0xffffff
             , mul_a2&0xffffff, add_a1&0xffffff, add_a2&0xffffff
             , add_a3&0xffffff, out_int&0xffffff, out&0x3fff);
    }
    if (debug != 0) {
      printf("iir1 %04X %04X %04X\n", in, in_signed&0x3fff, out&0x3fff);
    }
#endif

    //------------------------------------------------------
    // Range checks

    if (((add_a1 >> (W_INPUT*2)) != 0) && ((add_a1 >> (W_INPUT*2)) != -1))
        fprintf(stderr, "WARNING! add_a1 OVERFLOW (0x%0x)\n", add_a1);

    if (((add_a2 >> (W_INPUT*2)) != 0) && ((add_a2 >> (W_INPUT*2)) != -1))
        fprintf(stderr, "WARNING! add_a2 OVERFLOW! (0x%0x)\n", add_a2);

    if (((add_a3 >> (W_INPUT*2)) != 0) && ((add_a3 >> (W_INPUT*2)) != -1))
        fprintf(stderr, "WARNING! add_a3 OVERFLOW! (0x%0x)\n", add_a3);

    //if ((out >= (1 << (W_INPUT-1))) || (out < -(1 << (W_INPUT-1))))
    //    fprintf(stderr, "WARNING! Output will be clamped! (%d = 0x%0x)\n", out, out);
   
    // Clamp output to correct range
    out = (out < -(1 << (W_INPUT-1))) ? -(1 << (W_INPUT-1))   : out;
    out = (out >= (1 << (W_INPUT-1))) ?  (1 << (W_INPUT-1))-1 : out;
        
    //------------------------------------------------------
    // Stage for next calculation

    in_dly2 = in_dly1;
    in_dly1 = in_signed;
    
    out_dly2 = out_dly1;
    out_dly1 = out;
    
    // Rescale output
    out += (1 << (W_INPUT-1));
    
    return (unsigned short)out;
}

//----------------------------------------------------------
//
// Pipelined IIR notch filter
//
// Additional pipeline stages added in feedback path via scattered-lookahead
// decomposition (see Parhi & Messerschmitt, IEEE Trans. on Acoustics, Speech,
// and Sig. Proc. 37, 1099 (1989).
//
unsigned short iirNotchPipe2(int reset, unsigned short in, double nu, double r, int debug)
{
    short in_signed;
    int out_int;
    int fw1_1, fw1_2, fw2_1, fw2_2;
    int fb1_2;

    static int a[3];
    static int b[5];
    static int aLen, bLen;
    
    // Output and saved samples for feedback / feedforward    
    static short in_dly1 = 0;
    static short in_dly2 = 0;

    static short out = 0;
    static short out_dly1 = 0;
    static short out_dly2 = 0;
    static short out_dly3 = 0;

    static short fw1_out = 0;
    static short fw1_dly1 = 0;
    static short fw1_dly2 = 0;

    static int fb1_1 = 0;
    static int fb2_1 = 0;
    static int fb1_1_dly1 = 0;
    static int fb2_1_dly1 = 0;

    //------------------------------------------------------
    // Calculated coefficients if the filter is not 
    // initialized
    
    if (reset) {
        getNotchFilterCoeffs(nu/SAMP_FREQ, r, 2, a, b, &aLen, &bLen);

        if (debug >= 2) { //debugthei
          printf("<ir2  in ins out ");
          printf("in_sig  fw1_1   fw1_2   fw1o  fw2_1  fw2_2   fb1_1   fb2_1   fb1_2   out\n");
          printf("\n");
        }

        in_dly1 = in_dly2 = 0;
        out = out_dly1 = out_dly2 = out_dly3 = 0;
        fw1_out = fw1_dly1 = fw1_dly2 = 0;
        fb1_1 = fb2_1 = fb1_1_dly1 = fb2_1_dly1 = 0;
    }
    
    //------------------------------------------------------
    // Rescale input to 12b signed
    in_signed = (short)in - (1 << (W_INPUT-1));

    // First feed-forward stage
    // 12bx12b signed multiply
    // 24b output OK
    fw1_1 = b[1]*in_dly1 + b[0]*in_signed;
    fw1_2 = b[2]*in_dly2 + fw1_1;

    //--- REGISTER ---//

    // Need 14b here
    fw1_out = (short)(fw1_2 >> W_FRAC);

    // Second feed-forward stage
    // 14bx12b signed multiply... truncate output to 24b????
    // 24b output of adder OK
    fw2_1 = b[3]*fw1_dly1 + fw1_2;
    fw2_2 = b[4]*fw1_dly2 + fw2_1;

    //--- REGISTER ---//

    // Feedback, pipelined
    // 14bx12b signed multiply... truncate output to 24b????
    // 24b output of adder OK
    fb1_1 = a[1]*out_dly1;
    fb2_1 = a[2]*out_dly3;
    fb1_2 = fb1_1_dly1 + fb2_1_dly1;

    out_int = fw2_2 + fb1_2;

    out = (short)(out_int >> W_FRAC);

#if ADC12
    // DEBUG
    //printf("fw1_1 %0x  fw1_2 %0x  fw2_1 %0x  fw2_2 %0x  fb1_1 %0x  fb2_1 %0x  fb1_2 %0x  out_int %d\n",
    //       fw1_1, fw1_2, fw2_1, fw2_2, fb1_1, fb2_1, fb1_2, out_int);
    //
    if (debug >= 2) { //debugthei
      printf("iir2 %03X %03X %03X %07X %07X %07X %04x %07X %07X %07X %07X %07X %03X\n"
           , in, in_signed&0x0fff, out&0x0fff, in_signed&0xfffffff
           , fw1_1&0xfffffff, fw1_2&0xfffffff, fw1_out&0xffff
           , fw2_1&0xfffffff, fw2_2&0xfffffff
           , fb1_1&0xfffffff, fb2_1&0xfffffff, fb1_2&0xfffffff
           , out&0x0fff);
    }
#endif
#if ADC14
    // DEBUG
    //printf("fw1_1 %0x  fw1_2 %0x  fw2_1 %0x  fw2_2 %0x  fb1_1 %0x  fb2_1 %0x  fb1_2 %0x  out_int %d\n",
    //       fw1_1, fw1_2, fw2_1, fw2_2, fb1_1, fb2_1, fb1_2, out_int);
    //
    if (debug >= 2) { //debugthei
      printf("iir2 %04X %04X %04X %07X %07X %07X %04x %07X %07X %07X %07X %07X %04X\n"
           , in, in_signed&0x3fff, out&0x3fff, in_signed&0xfffffff
           , fw1_1&0xfffffff, fw1_2&0xfffffff, fw1_out&0xffff
           , fw2_1&0xfffffff, fw2_2&0xfffffff
           , fb1_1&0xfffffff, fb2_1&0xfffffff, fb1_2&0xfffffff
           , out&0x3fff);
    }
#endif

    //------------------------------------------------------
    // Range checks
    if (((fw1_1 >> (W_INPUT*2)) != 0) && ((fw1_1 >> (W_INPUT*2)) != -1))
        fprintf(stderr, "WARNING! fw1_1 OVERFLOW (0x%0x)\n", fw1_1);

    if (((fw1_2 >> (W_INPUT*2)) != 0) && ((fw1_2 >> (W_INPUT*2)) != -1))
        fprintf(stderr, "WARNING! fw1_2 OVERFLOW (0x%0x)\n", fw1_2);

    if (((fw2_1 >> (W_INPUT*2)) != 0) && ((fw2_1 >> (W_INPUT*2)) != -1))
        fprintf(stderr, "WARNING! fw2_1 OVERFLOW (0x%0x)\n", fw2_1);

    if (((fw2_2 >> (W_INPUT*2)) != 0) && ((fw2_2 >> (W_INPUT*2)) != -1))
        fprintf(stderr, "WARNING! fw2_2 OVERFLOW (0x%0x)\n", fw2_2);

    if (((fb1_1 >> (W_INPUT*2)) != 0) && ((fb1_1 >> (W_INPUT*2)) != -1))
        fprintf(stderr, "WARNING! fb1_1 OVERFLOW (0x%0x)\n", fb1_1);

    if (((fb1_2 >> (W_INPUT*2)) != 0) && ((fb1_2 >> (W_INPUT*2)) != -1))
        fprintf(stderr, "WARNING! fb1_2 OVERFLOW (0x%0x)\n", fb1_2);

    if (((out_int >> (W_INPUT*2)) != 0) && ((out_int >> (W_INPUT*2)) != -1))
        fprintf(stderr, "WARNING! out_int OVERFLOW (0x%0x)\n", out_int);

    if (((fw1_out >> (W_INPUT+1)) & 0x1) != ((fw1_out >> (W_INPUT+2)) & 0x1))
        fprintf(stderr, "WARNING! fw1_out OVERFLOW (0x%0x)\n", fw1_out);    

    //------------------------------------------------------
    // Stage for next calculation

    // 12b
    in_dly2 = in_dly1;
    in_dly1 = in_signed;

    // 14b
    fw1_dly2 = fw1_dly1;
    fw1_dly1 = fw1_out;

    fb1_1_dly1 = fb1_1;
    fb2_1_dly1 = fb2_1;

    // 14b
    out_dly3 = out_dly2;
    out_dly2 = out_dly1;
    out_dly1 = out;

    // Clamp final output to correct range
    out = (out < -(1 << (W_INPUT-1))) ? -(1 << (W_INPUT-1))   : out;
    out = (out >= (1 << (W_INPUT-1))) ?  (1 << (W_INPUT-1))-1 : out;
    
    // Rescale output
    out += (1 << (W_INPUT-1));
    
    return (unsigned short)out;
}

/*
 * 4-stage version of pipelined notch filter 
 */
unsigned short iirNotchPipe4(int reset, unsigned short in, double nu, double r, int debug)
{
    short in_signed;
    int out_int;
    int fw1_1, fw1_2, fw2_1, fw2_2, fw3_1, fw3_2;
    int fb1_2;

    static int a[3];
    static int b[7];
    static int aLen, bLen;
    
    // Output and saved samples for feedback / feedforward    
    static short in_dly1 = 0;
    static short in_dly2 = 0;

    static short out = 0;
    static short out_dly1 = 0;
    static short out_dly2 = 0;
    static short out_dly3 = 0;
    static short out_dly4 = 0;
    static short out_dly5 = 0;
    static short out_dly6 = 0;

    static short fw1_out = 0;
    static short fw1_dly1 = 0;
    static short fw1_dly2 = 0;

    static short fw2_out = 0;
    static short fw2_dly1 = 0;
    static short fw2_dly2 = 0;
    static short fw2_dly3 = 0;
    static short fw2_dly4 = 0;

    static int fb1_1 = 0;
    static int fb2_1 = 0;
    static int fb1_1_dly1 = 0;
    static int fb2_1_dly1 = 0;
    static int fb1_2_dly1 = 0;

    //------------------------------------------------------
    // Calculated coefficients if the filter is not 
    // initialized
    
    if (reset) {
        getNotchFilterCoeffs(nu/SAMP_FREQ, r, 4, a, b, &aLen, &bLen);

        if (debug == 3) { //debugthei
          printf("<ir3  in ins out ");
          printf("in_sig  fw1_1   fw1_2   fw1o  fw2_1  fw2_2   fw3_1   fw3_2   fb1_1   fb2_1   fb1_2    out\n");
          printf("\n");
        }

        in_dly1 = in_dly2 = 0;
        out = out_dly1 = out_dly2 = out_dly3 = out_dly4 = out_dly5 = out_dly6 = 0;
        fw1_out = fw1_dly1 = fw1_dly2 = 0;
        fw2_out = fw2_dly1 = fw2_dly2 = fw2_dly3 = fw2_dly4 = 0;
        fb1_1 = fb2_1 = fb1_1_dly1 = fb2_1_dly1 = fb1_2_dly1 = 0;
    }
    
    //------------------------------------------------------
    // Rescale input to 12b signed
    in_signed = (short)in - (1 << (W_INPUT-1));

    // First feed-forward stage
    // 12bx12b signed multiply
    // 24b output OK
    fw1_1 = b[1]*in_dly1 + b[0]*in_signed;
    fw1_2 = b[2]*in_dly2 + fw1_1;

    //--- REGISTER ---//

    // Need 14b here
    fw1_out = (short)(fw1_2 >> W_FRAC);

    // Second feed-forward stage
    // 14bx12b signed multiply... truncate output to 24b????
    // 24b output of adder OK
    fw2_1 = b[3]*fw1_dly1 + fw1_2;
    fw2_2 = b[4]*fw1_dly2 + fw2_1;

    // Probably need 14b here
    fw2_out = (short)(fw2_2 >> W_FRAC);

    //--- REGISTER ---//

    // Third feed-forward stage
    // 14bx12b signed multiply
    // 24b output of adder OK

    fw3_1 = b[5]*fw2_dly2 + fw2_2;
    fw3_2 = b[6]*fw2_dly4 + fw3_1;

    //--- REGISTER ---//

    // Feedback, pipelined
    // 14bx12b signed multiply... truncate output to 24b????
    // 24b output of adder OK
    fb1_1 = a[1]*out_dly2;
    fb2_1 = a[2]*out_dly6;
    fb1_2 = fb1_1_dly1 + fb2_1_dly1;

    out_int = fw3_2 + fb1_2_dly1;

    out = (short)(out_int >> W_FRAC);

#if ADC12
    // DEBUG
    //printf("fw1_1 %0x  fw1_2 %0x  fw2_1 %0x  fw2_2 %0x  fb1_1 %0x  fb2_1 %0x  fb1_2 %0x  out_int %d\n",
    //       fw1_1, fw1_2, fw2_1, fw2_2, fb1_1, fb2_1, fb1_2, out_int);
    //
    if (debug == 3) { //debugthei
      printf("iir3 %03X %03X %03X %07X %07X %07X %04x %07X %07X %07X %07X %07X %07X %07X %03X "
           , in, in_signed&0x0fff, out&0x0fff, in_signed&0xfffffff
           , fw1_1&0xfffffff, fw1_2&0xfffffff, fw1_out&0xffff
           , fw2_1&0xfffffff, fw2_2&0xfffffff
           , fw3_1&0xfffffff, fw3_2&0xfffffff
           , fb1_1&0xfffffff, fb2_1&0xfffffff, fb1_2&0xfffffff
           , out&0x0fff);
    }
#endif
#if ADC14
    // DEBUG
    //printf("fw1_1 %0x  fw1_2 %0x  fw2_1 %0x  fw2_2 %0x  fb1_1 %0x  fb2_1 %0x  fb1_2 %0x  out_int %d\n",
    //       fw1_1, fw1_2, fw2_1, fw2_2, fb1_1, fb2_1, fb1_2, out_int);
    //
    if (debug == 3) { //debugthei
      printf("iir3 %04X %04X %04X %07X %07X %07X %04x %07X %07X %07X %07X %07X %07X %07X %04X "
           , in, in_signed&0x3fff, out&0x3fff, in_signed&0xfffffff
           , fw1_1&0xfffffff, fw1_2&0xfffffff, fw1_out&0xffff
           , fw2_1&0xfffffff, fw2_2&0xfffffff
           , fw3_1&0xfffffff, fw3_2&0xfffffff
           , fb1_1&0xfffffff, fb2_1&0xfffffff, fb1_2&0xfffffff
           , out&0x3fff);
    }
#endif

    //------------------------------------------------------
    // Range checks
    if (((fw1_1 >> (W_INPUT*2)) != 0) && ((fw1_1 >> (W_INPUT*2)) != -1))
        fprintf(stderr, "WARNING! fw1_1 OVERFLOW (0x%0x)\n", fw1_1);

    if (((fw1_2 >> (W_INPUT*2)) != 0) && ((fw1_2 >> (W_INPUT*2)) != -1))
        fprintf(stderr, "WARNING! fw1_2 OVERFLOW (0x%0x)\n", fw1_2);

    if (((fw2_1 >> (W_INPUT*2)) != 0) && ((fw2_1 >> (W_INPUT*2)) != -1))
        fprintf(stderr, "WARNING! fw2_1 OVERFLOW (0x%0x)\n", fw2_1);

    if (((fw2_2 >> (W_INPUT*2)) != 0) && ((fw2_2 >> (W_INPUT*2)) != -1))
        fprintf(stderr, "WARNING! fw2_2 OVERFLOW (0x%0x)\n", fw2_2);

    if (((fw3_1 >> (W_INPUT*2)) != 0) && ((fw3_1 >> (W_INPUT*2)) != -1))
        fprintf(stderr, "WARNING! fw3_1 OVERFLOW (0x%0x)\n", fw3_1);

    if (((fw3_2 >> (W_INPUT*2)) != 0) && ((fw3_2 >> (W_INPUT*2)) != -1))
        fprintf(stderr, "WARNING! fw3_2 OVERFLOW (0x%0x)\n", fw3_2);

    if (((fb1_1 >> (W_INPUT*2)) != 0) && ((fb1_1 >> (W_INPUT*2)) != -1))
        fprintf(stderr, "WARNING! fb1_1 OVERFLOW (0x%0x)\n", fb1_1);

    if (((fb1_2 >> (W_INPUT*2)) != 0) && ((fb1_2 >> (W_INPUT*2)) != -1))
        fprintf(stderr, "WARNING! fb1_2 OVERFLOW (0x%0x)\n", fb1_2);

    if (((out_int >> (W_INPUT*2)) != 0) && ((out_int >> (W_INPUT*2)) != -1))
        fprintf(stderr, "WARNING! out_int OVERFLOW (0x%0x)\n", out_int);

    if (((fw1_out >> (W_INPUT+1)) & 0x1) != ((fw1_out >> (W_INPUT+2)) & 0x1))
        fprintf(stderr, "WARNING! fw1_out OVERFLOW (0x%0x)\n", fw1_out);    

    if (((fw2_out >> (W_INPUT+1)) & 0x1) != ((fw2_out >> (W_INPUT+2)) & 0x1))
        fprintf(stderr, "WARNING! fw2_out OVERFLOW (0x%0x)\n", fw2_out);    

    //------------------------------------------------------
    // Stage for next calculation

    // 12b
    in_dly2 = in_dly1;
    in_dly1 = in_signed;

    // 14b
    fw1_dly2 = fw1_dly1;
    fw1_dly1 = fw1_out;

    fw2_dly4 = fw2_dly3;
    fw2_dly3 = fw2_dly2;
    fw2_dly2 = fw2_dly1;
    fw2_dly1 = fw2_out;

    fb1_1_dly1 = fb1_1;
    fb2_1_dly1 = fb2_1;
    fb1_2_dly1 = fb1_2;

    // 14b
    out_dly6 = out_dly5;
    out_dly5 = out_dly4;
    out_dly4 = out_dly3;
    out_dly3 = out_dly2;
    out_dly2 = out_dly1;
    out_dly1 = out;

    // Clamp final output to correct range
    out = (out < -(1 << (W_INPUT-1))) ? -(1 << (W_INPUT-1))   : out;
    out = (out >= (1 << (W_INPUT-1))) ?  (1 << (W_INPUT-1))-1 : out;
    
    // Rescale output
    out += (1 << (W_INPUT-1));
    
    return (unsigned short)out;
}

#if NotchType2
//----------------------------------------------------------
//
// Type 2 IIR notch filter implementation in fixed point
//
// See e.g. C. Tseng, IEEE Trans. Sig. Process. 49, 2673 (2001).
// Implementation of Type II single-frequency notch filter, based
// on an allpass filter.
//
// Note: this filter seems to have a much wider notch for a given r.
// This could be a bug in my implementation.
//
// Retains state from one call to next to implement feedforward 
// and feedback.
//
unsigned short iirNotchType2(int reset, unsigned short in, double nu, double r) {

    int i;
    short in_signed;
    int mul_b0, mul_b1, mul_b2, mul_a1, mul_a2;
    int add_a1, add_a2, add_a3;
    int out_int;

    static int a[3];
    static int b[3];

    static double a_dbl[3];
    static double b_dbl[3];
    static double nu_s;
    
    // Output and saved samples for feedback / feedforward    
    static short in_dly1 = 0;
    static short in_dly2 = 0;

    static short out = 0;
    static short out_dly1 = 0;
    static short out_dly2 = 0;

    //------------------------------------------------------
    // Calculated coefficients if the filter is not 
    // initialized
    
    if (reset) {

        nu_s = nu / SAMP_FREQ;
        
        a_dbl[1] = -(1 + r*r) * cos(2*3.1416*nu_s);
        a_dbl[2] = r*r;
        
        b_dbl[0] = r*r;
        b_dbl[1] = -(1 + r*r)*cos(2*3.1416*nu_s);
        b_dbl[2] = 1;
        
        //------------------------------------------------------
        // Fixed-point IIR coefficients
        
        for (i = 0; i < 3; i++) {
            a[i] = float2fixed(a_dbl[i], W_INT, W_FRAC);
            b[i] = float2fixed(b_dbl[i], W_INT, W_FRAC);
        }
     
        printf("# Filter: %g MHz on sampling frequency of %g MHz (r = %g)\n", nu, SAMP_FREQ, r);
        printf("#\n");
        printf("#     A\t\t\tB\n");
        printf("# 0    \t\t\t%.3f (0x%03x)\n", b_dbl[0], b[0] & 0xfff);
        printf("# 1   %.3f (0x%03x)\t%.3f (0x%03x)\n"
               , a_dbl[1], a[1] & 0xfff, b_dbl[1], b[1] & 0xfff);
        printf("# 2   %.3f (0x%03x)\t%.3f (0x%03x)\n"
               , a_dbl[2], a[2] & 0xfff, b_dbl[2], b[2] & 0xfff);
        printf("#\n");

        in_dly1 = in_dly2 = out = out_dly1 = out_dly2 = 0;
    }
    
    //------------------------------------------------------
    // Rescale input to 12b signed
    in_signed = (short)in - (short)(1 << (W_INPUT-1));

    //------------------------------------------------------
    mul_a2 = a[2]*out_dly2;
    mul_b2 = (int)in_dly2 << W_FRAC;
    
    mul_a1 = a[1]*out_dly1;
    mul_b1 = b[1]*in_dly1;
    
    mul_b0 = b[0]*in_signed;

    add_a2 = mul_b2-mul_a2;
    add_a1 = mul_b1-mul_a1;

    add_a3 = add_a2+add_a1;

    out_int = ((int)in_signed + mul_b0 + add_a3) >> 1;

    out = (short)(out_int >> W_FRAC);
   
    // Clamp output to correct range
    out = (out < -(1 << (W_INPUT-1))) ? -(1 << (W_INPUT-1))   : out;
    out = (out >= (1 << (W_INPUT-1))) ?  (1 << (W_INPUT-1))-1 : out;
        
    //------------------------------------------------------
    // Stage for next calculation

    in_dly2 = in_dly1;
    in_dly1 = in_signed;
    
    out_dly2 = out_dly1;
    out_dly1 = out;
    
    // Rescale output
    out += (1 << (W_INPUT-1));
    
    return (unsigned short)out;
}
#endif

