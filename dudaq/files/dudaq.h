/// @file DU.h
/// @brief few definitions to be used for the main program
/// @author C. Timmermans, Nikhef/RU

#include<sys/time.h>

#define MAX_INP_MSG 20000 //!< maximum size (in shorts) of the input data (should be big enough for the configuation data)
#define MAX_OUT_MSG 80000 //!< maximum size (in shorts) of the output data (should be big enough for the T2 data/ a complete event)
#define MAX_T2 1000 //! max size for T2's, corresponds to the Adaq

#ifdef Fake
#define MSGSTOR 10 //!< number of messages in shared memory
#else
#define MSGSTOR 100 //!< number of messages in shared memory
#endif
#define MAXMSGSIZE 1000 //!< max number of shorts to be sent from socket to the scope at once

#define DU_PORT 5001 //!< socket port number of the detector unit software


