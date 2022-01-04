/***
DAQ Main project definitions
Version:1.0
Date: 17/2/2020
Author: Charles Timmermans, Nikhef/Radboud University

Altering the code without explicit consent of the author is forbidden
 ***/
#include <sys/socket.h>
#include <netinet/tcp.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "ad_shm.h"

#define MAXDU 5 //max number of Detector Units
#define ERROR -1
#define NORMAL 1
#define DEFAULT_CONFIGFILE "conf/Adaq.conf"

typedef struct{
  int DUid; // station number
  char DUip[20]; // IP address
  int DUport;   // port to connect to
  int DUsock;
  time_t LSTconnect;
  struct sockaddr_in  DUaddress;
  socklen_t DUalength;
}DUInfo;

#define NT2BUF (8*MAXDU) //8 per DU
#define T2SIZE 1000 //Max. size (in shorts) for T2 info in 1 message 

#define NT3BUF 500 // max 500 T3 buffers (small messages anyway)
#define T3SIZE (6+3*MAXDU) //Max. size (in shorts) for T3 info in 1 message

#define NEVBUF 10 // maximal 10 event buffers
#define EVSIZE 40000 //Max. size (in shorts) for evsize for each DU

#define CMDBUF 20 // leave 20 command buffers
#define CMDSIZE 5000 //Max. size (in shorts) for command (should be able to hold config file)

#define MONBUF 1 // only 1 entry for monitoring


#ifdef _MAINDAQ
DUInfo DUinfo[MAXDU];
int tot_du;
shm_struct shm_t2;
shm_struct shm_t3;
shm_struct shm_cmd;
shm_struct shm_eb;
shm_struct shm_mon;
//next EB parameters
int eb_run = 1;
int eb_run_mode = 0;
int eb_max_evts = 10;
char eb_dir[80];
//T3 parameters
int t3_rand = 0; 
#else
extern DUInfo DUinfo[MAXDU];
extern int tot_du;
extern shm_struct shm_t2;
extern shm_struct shm_t3;
extern shm_struct shm_eb;
extern shm_struct shm_cmd;
extern shm_struct shm_mon;
extern int eb_run ;
extern int eb_run_mode;
extern int eb_max_evts ;
extern char eb_dir[80]; 
extern int t3_rand;
#endif
