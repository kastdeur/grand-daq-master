ROOTSYS ?= "/usr"
ROOT_INCLUDES = "${ROOTSYS}/include/root"
ROOT_LIBS = "${ROOTSYS}/lib/root" # Might be lib64 on system-wide install

GRAND_UTILS="../../c_grand_utils"
GRAND_ANTENNA="../../c-antenna-test/src"


Traces: Traces.c Traces.h
	gcc  Traces.c -I../DU -o Traces

Hist: Hist.c Traces.h Makefile
	g++ Hist.c \
	-std=c++11 \
	-ltbb -lfftw3 \
	-I../DU \
	-I${GRAND_UTILS} -L${GRAND_UTILS} -lgrand_utillib  \
	-I${GRAND_ANTENNA} \
	-I${ROOT_INCLUDES} \
	-L${ROOT_LIBS} -lCore -lRIO -lHist -lMathCore -lGpad \
	-o Hist
