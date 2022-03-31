Traces: Traces.c Traces.h
	gcc  Traces.c -I../DU -o Traces

Hist: Hist.c Traces.h Makefile
	clang++ Hist.c  \
	-L/usr/local/lib -lfftw3 \
	-I /Users/timmer/Work/GRAND/Program/c_grand_utils -L /Users/timmer/Work/GRAND/Program/c_grand_utils -lgrand_utillib \
	-I /Users/timmer/Work/GRAND/Simu/GRAND/c-antenna-test-master/src \
	-I ../DU \
	-I${ROOTSYS}/include/root -L${ROOTSYS}/lib/root -lCore -lRIO -lhist -lMathCore -lGpad -std=c++11 \
	-o Hist
	
