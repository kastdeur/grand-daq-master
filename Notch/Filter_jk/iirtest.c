#include <stdio.h>
#include <math.h>
#include <stdlib.h>

#include "filter.h"

//----------------------------------------------------------

int main(int argv, char **argc) {

    FILE *fin;
    char *filename;
    float freq;
    float width;
    int in;
    short out;
    short out_pipe2;
    short out_pipe4;
    short out_type2;
    int reset = 1;

    //-----------------------------------------
    // Check command-line arguments

    if (argv < 3) {
        printf("Usage: %s <input file> <frequency> <width>\n", argc[0]);
        return 0;
    }

    filename = argc[1];
    freq = atof(argc[2]);
    width = atof(argc[3]);

    if ((freq <= 0) || (freq >= SAMP_FREQ/2)) {
        printf("Error: filter frequency must be between 0 and %g MHz\n", SAMP_FREQ/2);
        return -1;
    }

    if ((width < 0) || (width > 1)) {
        printf("Error: width parameter must be between 0 and 1 (try 0.99)\n");
        return -1;
    }

    //-----------------------------------------
    // Read input file

    if ((fin = fopen(filename, "r")) == NULL) {
        printf("Error; couldn't open data file %s for reading.\n", filename);
        return -1;
    }

    while (fscanf(fin, "%d", &in) > 0) {
        out = iirNotchFixed(reset, (short)in, (double)freq, (double)width);
        out_pipe2 = iirNotchPipe2(reset, (short)in, (double)freq, (double)width);
        out_pipe4 = iirNotchPipe4(reset, (short)in, (double)freq, (double)width);
        out_type2 = iirNotchType2(reset, (short)in, (double)freq, (double)width);
        printf("%d %d %d %d %d\n", in, out, out_pipe2, out_pipe4, out_type2);
        reset = 0;
    }

    fclose(fin);

    return 0;

}

