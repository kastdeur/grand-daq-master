// filter_settings.c    27-Feb-2012

#include <stdio.h>
#include <math.h>
#include <stdlib.h>

#include "filter.h"

//
// Number of extra pipeline stages implemented in FPGA
//
#define XTRA_PIPE 4

//----------------------------------------------------------
//
// Get IIR notch filter coefficients for a given 
// frequency and filter width.  The input frequency is in 
// MHz, and the width must be between 0 and 1 (larger is narrower).  
// Recommended value for very narrow notch: r = 0.99.
//
// J. Kelley, June 2010
// j.kelley@astro.ru.nl
//
//----------------------------------------------------------

int main(int argv, char **argc)
{
    float freq;
    float width = 0.99;
    int a[10], b[10];
    int aLen, bLen, i;

    //-----------------------------------------
    // Check command-line arguments

    if (argv <= 1) {
        printf("Usage: %s <frequency> [ <width> ]\n", argc[0]);
        printf("   frequency:  notch filter central frequency in MHz\n");
        printf("       width:  filter width parameter (higher is narrower, default = 0.99)\n");
        return 0;
    }

    freq = atof(argc[1]);
    if (argv == 3)
        width = atof(argc[2]);

    if ((freq <= 0) || (freq >= SAMP_FREQ/2)) {
        printf("Error: filter frequency must be between 0 and %g MHz\n", SAMP_FREQ/2);
        return -1;
    }

    if ((width < 0) || (width > 1)) {
        printf("Error: width parameter must be between 0 and 1 (try 0.99)\n");
        return -1;
    }

    getNotchFilterCoeffs(freq/SAMP_FREQ, width, XTRA_PIPE, a, b, &aLen, &bLen);

    printf("FPGA hex values:\n");
    for (i = 1; i < aLen; i++) {
        printf("%02x %02x ", (a[i] >> 8) & 0xff, a[i] & 0xff);
    }
    for (i = 1; i < bLen; i++) {
        printf("%02x %02x ", (b[i] >> 8) & 0xff, b[i] & 0xff);
    }
    printf("\n");
    for (i = 1; i < aLen; i++) {
        printf("0x%04x ", a[i]&0xffff);
    }
    for (i = 1; i < bLen; i++) {
        printf("0x%04x ", b[i]&0xffff );
    }
    printf("\n");

    return 0;

}

