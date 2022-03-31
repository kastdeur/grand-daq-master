/// @file
/// @brief steering file for the Detector Unit software
/// @author C. Timmermans, Nikhef/RU

#include <stdio.h>
#include <stdlib.h>
#include<signal.h>
#include<sys/wait.h>
#include<sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/tcp.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <signal.h>
#include<errno.h>
#include"dudaq.h"
#include "amsg.h"
#include "scope.h"
#include "ad_shm.h"
#include "du_monitor.h"

int buffer_add_t2(unsigned short *bf,int bfsize,short id);
int buffer_add_t3(unsigned short *bf,int bfsize,short id);
int buffer_add_monitor(unsigned short *bf,int bfsize,short id);
void scope_flush();

shm_struct shm_ev; //!< shared memory containing all event info, including read/write pointers
shm_struct shm_gps; //!< shared memory containing all GPS info, including read/write pointers
shm_struct shm_ts; //!< shared memory containing all timestamp info, including read/write pointers
shm_struct shm_cmd; //!< shared memory containing all command info, including read/write pointers
shm_struct shm_mon; //!< shared memory containing monitoring info
TS_DATA *timestampbuf;
GPS_DATA *gpsbuf; //!< buffer to hold GPS information
extern int errno; //!< the number of the error encountered
extern uint32_t shadowlist[Reg_End>>2];


#define SOCKETS_BUFFER_SIZE  1048576
#define SOCKETS_TIMEOUT      100

int du_port;       //!<port number on which to connect to the central daq

int run=0;                //!< current run number

int station_id;           //!< id of the LS, obtained from the ip address

fd_set sockset;           //!< socket series to be manipulated
int32_t DU_socket = -1;   //!< main socket for accepting connections
int32_t DU_comms = -1;    //!< socket for the accepted connection
struct sockaddr_in  DU_address; //!< structure containing address information of socket
socklen_t DU_alength;           //!< length of sending socket address
socklen_t RD_alength;           //!< length of receiving socket address

uint16_t DU_input[MAX_INP_MSG];   //!< memory in which to receive the socket data from the central DAQ
uint16_t DU_output[MAX_OUT_MSG];  //!< memory used to store the data to be sent to the central DAQ


unsigned int prev_mask; //!< old signal mask
unsigned int mask;      //!< signal mask

pid_t pid_scope;        //!< process id of the process reading/writing the fpga
pid_t pid_socket;       //!< process id of the process reading/writing to the central DAQ
pid_t pid_monitor;       //!< process id of the process reading/writing the monitor info
uint8_t stop_process=0; //!< after an interrupt this flag is set so that all forked processes are killed


void chc_read();


void remove_shared_memory()
{
    ad_shm_delete(&shm_ev);
    ad_shm_delete(&shm_ts);
    ad_shm_delete(&shm_gps);
    ad_shm_delete(&shm_cmd);
    ad_shm_delete(&shm_mon);
}

/*!
\fn void clean_stop(int signum)
* \brief kill processes
* \param signum input signal (not used)
* kill child processes and perform a clean stop to clean up shared memory
* \author C. Timmermans
*/
void clean_stop (int signum)
{
    remove_shared_memory();
    stop_process = 1;
    kill(pid_scope,9);
    kill(pid_monitor,9);
    kill(pid_socket,9);
}


/*!
 \fn void block_signals(int iblock)
 * \brief block sigio/sigpipe
 * \retval 0 ok
 * \param iblock When 1, block signals. Otherwise release block
 * \author C. Timmermans
 */

void block_signals(int iblock)
{
    if(iblock == 1){
        mask = ((1<<SIGIO)+ (1<<SIGPIPE));
        sigprocmask(SIG_BLOCK,(sigset_t *)&mask,(sigset_t *)&prev_mask);
        //    printf("Blocking %x %x\n",mask,prev_mask);
    }else{
        mask = prev_mask;
        sigprocmask(SIG_SETMASK,(sigset_t *)&mask,(sigset_t *)&prev_mask);
    }
}

/*!
 \fn int set_socketoptions(int sock)
 * \brief set default socket options
 * \retval 0 ok
 * \param sock socket id
 * - Default options:
 *   - re-use address
 *   - not keepalive
 *   - no delay
 *   - receive buffer size 1048576
 *   - send buffer size 1048576
 *   - receive timeout 100 msec
 *   - send timeout 100 msec
 * \author C. Timmermans
 */
int set_socketoptions(int sock)
{
    int option;
    struct timeval timeout;
    // set default socket options
    option = 1;
    if(setsockopt (sock,SOL_SOCKET, SO_REUSEADDR,&option,sizeof (int) )<0) return(-1);
    option = 0;
    if(setsockopt (sock,SOL_SOCKET, SO_KEEPALIVE,&option,sizeof (int) ) < 0) return(-1);
    option = 1;
    if(setsockopt (sock,IPPROTO_TCP, TCP_NODELAY, &option,sizeof (int) )<0) return(-1);
    option = SOCKETS_BUFFER_SIZE;
    if(setsockopt (sock,SOL_SOCKET, SO_SNDBUF,&option,sizeof (int) )<0) return(-1);
    if(setsockopt (sock,SOL_SOCKET, SO_RCVBUF,&option,sizeof (int) )<0) return(-1);
    timeout.tv_usec = SOCKETS_TIMEOUT;
    timeout.tv_sec = 0;
    if(setsockopt (sock,SOL_SOCKET, SO_RCVTIMEO,&timeout,sizeof (struct timeval) )<0) return(-1);
    timeout.tv_usec = SOCKETS_TIMEOUT;
    timeout.tv_sec = 0;
    if(setsockopt (sock,SOL_SOCKET, SO_SNDTIMEO,&timeout,sizeof (struct timeval) )<0) return(-1);
    return(0);
}

/*!
 \fn int make_server_connection(int port)
 * \brief connect to central DAQ
 * \retval -1 Error
 * \retval 0 ok
 * \param port socket port id
 * - Create a socket
 * - Bind this socket
 * - Listen on this socket
 * - Accept a connection on this socket
 * - set default socket options
 * \author C. Timmermans
 */
int make_server_connection(int port)
{
  int ntry;
  
  printf("Trying to connect to the server\n");
  // Create the socket
  //DU_socket =  socket ( PF_INET, SOCK_DGRAM, 0 );
  DU_socket =  socket ( PF_INET, SOCK_STREAM, 0 );
  if(DU_socket < 0 ) {
    printf("make server connection: Cannot create a socket\n");
    return(-1);
  }
  set_socketoptions(DU_socket);
  DU_address.sin_family       = AF_INET;
  DU_address.sin_port         = htons (port);
  DU_address.sin_addr.s_addr  = htonl ( INADDR_ANY );
  DU_alength = sizeof(DU_address);
  if (bind ( DU_socket, (struct sockaddr*)&DU_address,DU_alength) < 0)  {
    shutdown(DU_socket,SHUT_RDWR);
    close(DU_socket);
    DU_socket = -1;
    printf("make server connection: bind does not work\n");
    return(-1);
  }
  if (listen (DU_socket, 1 ) <0) {
    shutdown(DU_socket,SHUT_RDWR);
    close(DU_socket);
    DU_socket = -1;
    printf("make server connection: listen does not work\n");
    return(-1);
  }
  DU_comms = -1;
  ntry = 0;
  while(DU_comms < 0 && ntry <10000){
    DU_comms = accept(DU_socket, (struct sockaddr*)&DU_address,&DU_alength);
    if(DU_comms <0){
      if(errno != EWOULDBLOCK && errno != EAGAIN) {
	printf("Return an error\n");
	return(-1); // an error if socket is not non-blocking
      }
    }else{
      FD_ZERO(&sockset);
      FD_SET(DU_comms, &sockset);
    }
    ntry++;
  }
  if(DU_comms < 0) return(-1);
  set_socketoptions(DU_comms);
  return(0); // all ok
}

/*!
\fn int check_server_data()
* \brief check and execute commands from central DAQ
* \retval -1 Error
* \retval 0 No new message
* \retval >0 Length of message
* - Checks if there is a message on the socket
*   - on error: shut down the connection
* - read data until all is read or there is an error
*   - on error shut down socket connection
* - interpret messages:
*   - DU_RESET, DU_INITIALIZE, DU_START, DU_STOP, DU_BOOT are stored in shared memory for interpretation by scope_check_commands()
*   - DU_GETEVENT moves event into the T3-buffer
*   - ALIVE responds with ACK_ALIVE to central DAQ
*
* \author C. Timmermans
*/

int check_server_data()
{
  uint16_t *msg_start;
  uint16_t msg_tag, msg_len;
  uint16_t ackalive[6]={5,3,ALIVE_ACK,station_id,GRND1,GRND2};
  int32_t port,i,il,isize;
  uint16_t isec;
  uint32_t ssec;
  
  int32_t bytesRead,recvRet;
  int32_t length;
  unsigned char *bf = (unsigned char *)DU_input;
  struct timeval timeout;
  
  //printf("Checking Server data\n");
  timeout.tv_sec = 0;
  timeout.tv_usec=10;
  if(DU_comms>=0){
    FD_ZERO(&sockset);
    FD_SET(DU_comms, &sockset);
    i= select(DU_comms+1, &sockset, NULL, NULL, &timeout);
    if (i < 0){
      // You have an error
      shutdown(DU_comms,SHUT_RDWR);
      close(DU_comms);
      DU_comms = -1;
    }
  }
  if(i ==0) return(0);
  if(DU_comms<0){
    if(DU_socket < 0){
      if(make_server_connection(du_port)<0)
        return(-1);
    }
    DU_comms = accept(DU_socket, (struct sockaddr*)&DU_address,&DU_alength);
    if(DU_comms <0){
      if(errno != EWOULDBLOCK && errno != EAGAIN) return(-1); // an error if socket is not non-blocking
      return(0);
    }
    usleep(1000);
    FD_ZERO(&sockset);
    FD_SET(DU_comms, &sockset);
    set_socketoptions(DU_comms);
    usleep(1000);
  }
  //printf("There is data\n");
  DU_input[0] = 0;
  while(DU_input[0] <= 1){
    DU_input[0] = -1;
    RD_alength = DU_alength;
    if((recvRet = recvfrom(DU_comms, DU_input,2,0,(struct sockaddr*)&DU_address,&RD_alength))==2){ //read the buffer size
      if ((DU_input[0] < 0) || (DU_input[0]>MAX_INP_MSG)) {
        printf("Check Server Data: bad buffer size when receiving socket data (%d shorts)\n", DU_input[0]);
        shutdown(DU_comms,SHUT_RDWR);
        close(DU_comms);
        DU_comms = -1;
        return(-1);
      }
      bytesRead = 2;
      while (bytesRead < (2*DU_input[0]+2)) {
        RD_alength = DU_alength;
        recvRet = recvfrom(DU_comms,&bf[bytesRead],
                           2*DU_input[0]+2-bytesRead,0,(struct sockaddr*)&DU_address,&RD_alength);
        if(errno == EAGAIN && recvRet<0) continue;
        if (recvRet <= 0) {
          printf("Check Server Data: connection died when receiving socket data\n");
          shutdown(DU_comms,SHUT_RDWR);
          close(DU_comms);
          DU_comms = -1;
          return(-1);
        }
        bytesRead += recvRet;
      } // while read data
    }else{
      if(errno == EAGAIN) { // no data
        DU_input[0] = 0;
        break;
      }else{
        printf("Check Server Data: connection died before getting data\n");
        shutdown(DU_comms,SHUT_RDWR);
        close(DU_comms);
        DU_comms = -1;
        return(-1);
      }
    }
  }
  
  i = 1;
  //printf("Message length = %d\n",DU_input[0]);
  while(i<DU_input[0]-1){
    msg_start = &(DU_input[i]);
    msg_len = msg_start[AMSG_OFFSET_LENGTH];
    msg_tag = msg_start[AMSG_OFFSET_TAG];
    //printf("%d %d\n",msg_len,msg_tag);
    if (msg_len < 2) {
      printf("Error: message is too short (no tag field)!\n");
      break;
    }
    if (msg_len > MAXMSGSIZE) {
      printf("Error: message is too long: %d\n",msg_len);
      break;
    }
    //printf("Received message %d\n",msg_tag);
    switch(msg_tag){
      case DU_RESET:
      case DU_INITIALIZE:
      case DU_START:
      case DU_STOP:
      case DU_BOOT:
        while(shm_cmd.Ubuf[(*shm_cmd.size)*(*shm_cmd.next_write)] == 1) {
          usleep(1000); // wait for the scope to read the shm
        }
        memcpy((void *)&(shm_cmd.Ubuf[(*shm_cmd.size)*(*shm_cmd.next_write)+1]),(void *)msg_start,2*msg_start[AMSG_OFFSET_LENGTH]);
        shm_cmd.Ubuf[(*shm_cmd.size)*(*shm_cmd.next_write)] = 1;
        *shm_cmd.next_write = *shm_cmd.next_write + 1;
        if(*shm_cmd.next_write >= *shm_cmd.nbuf) *shm_cmd.next_write = 0;
        break;
      case DU_GETEVENT:                    // calculate sec and subsec, move the event to the t3 buffer
      case DU_GET_MINBIAS_EVENT:                    // calculate sec and subsec, move the event to the t3 buffer
      case DU_GET_RANDOM_EVENT:                    // calculate sec and subsec, move the event to the t3 buffer
        if (msg_len == AMSG_OFFSET_BODY + 3 + 1) {
          memcpy((void *)&(shm_cmd.Ubuf[(*shm_cmd.size)*(*shm_cmd.next_write)+1]),(void *)msg_start,2*msg_start[AMSG_OFFSET_LENGTH]);
          shm_cmd.Ubuf[(*shm_cmd.size)*(*shm_cmd.next_write)] = 1;
          *shm_cmd.next_write = *shm_cmd.next_write + 1;
          if(*shm_cmd.next_write >= *shm_cmd.nbuf) *shm_cmd.next_write = 0;
        }
        else
          printf("Error: message DU_GETEVENT was wrong length (%d)\n", msg_len);
        break;
      case ALIVE:
        sendto(DU_comms, ackalive,sizeof(ackalive), 0,(struct sockaddr*)&DU_address,DU_alength);
        break;
      default:
        printf("Received unknown message %d\n",msg_tag);
    }
    i+=msg_len;
  } // End loop over input buffer
  if(msg_len<1) return(1);
  return(msg_len);
}

/*!
 \fn int send_server_data()
 * \brief send t2 and monitoring data to the central DAQ
 * \retval -1 Error
 * \retval 0 No connection to DAQ
 * \retval 1 All ok
 * - If there is no connection to the central DAQ, try to connect
 * - Add t2 data to the output buffer
 * - send output buffer to the central DAQ
 * - If this failed, close the connection to the central DAQ
 * - Add monitoring data to the output buffer
 * - send output buffer to the central DAQ
 *
 * \author C. Timmermans
 */
int send_server_data(){
  int32_t sentBytes;
  int32_t rsend;
  char *bf = (char *)DU_output;
  int32_t length,bsent;
  DU_alength = sizeof(DU_address);
  
  if(DU_comms< 0){
    if(DU_socket < 0){
      if(make_server_connection(du_port)<0)
        return(-1);
    }
    DU_comms = accept(DU_socket, (struct sockaddr*)&DU_address,&DU_alength);
    if(DU_comms <0){
      if(errno != EWOULDBLOCK && errno != EAGAIN) return(-1); // an error if socket is not non-blocking
      return(0);
    }
    FD_ZERO(&sockset);
    FD_SET(DU_comms, &sockset);
    set_socketoptions(DU_comms);
  }
  buffer_add_t2(&(DU_output[1]),MAX_T2-1,station_id); // first add the T2 information
  rsend = 0;
  if(DU_output[1] > 0) {
    DU_output[0] = DU_output[1]+2;
    //printf("Sending T2's %d\n",DU_output[0]);
    DU_output[DU_output[0]-1] = GRND1;
    DU_output[DU_output[0]] = GRND2;
    length = 2*DU_output[0]+2;
    sentBytes = 0;
    while(sentBytes<length){
      if(length-sentBytes>64) bsent=64;
      else 
	bsent = length-sentBytes;
      rsend = sendto(DU_comms, &(bf[sentBytes]),bsent, 0,
                     (struct sockaddr*)&DU_address,DU_alength);
      if(rsend<0 && errno != EAGAIN) {
        printf("Sending T2 did not really work\n");
        sentBytes = rsend;
        break;
      }
      if(rsend>0) sentBytes +=rsend;
      if(sentBytes<length) usleep(1000);
    } //while sentbytes
    if(sentBytes <= 0) { // it did not work
      printf("Sending T2 did not work for socket %d %d\n",DU_comms,errno);
      shutdown(DU_comms,SHUT_RDWR);
      close(DU_comms);
      DU_comms = -1;
      return(-1);
    }
    usleep(200);
  }
  buffer_add_monitor(&(DU_output[1]),MAX_OUT_MSG-1,station_id); // Next: monitor information
  if(DU_output[1] == 0) return(rsend); // nothing to do
  //printf("Sending T2's %d\n",DU_output[0]);
  DU_output[0] = DU_output[1]+2;
  DU_output[DU_output[0]-1] = GRND1;
  DU_output[DU_output[0]] = GRND2;
  length = 2*DU_output[0]+2;
  sentBytes = 0;
  while(sentBytes<length){
    if(length-sentBytes>500) bsent=500;
    else 
	bsent = length-sentBytes;
    rsend = sendto(DU_comms, &(bf[sentBytes]),bsent, 0,
                   (struct sockaddr*)&DU_address,DU_alength);
    if(rsend<0 && errno != EAGAIN) {
      printf("Sending T2 did not really work\n");
      sentBytes = rsend;
      break;
    }
    if(rsend>0) sentBytes +=rsend;
    usleep(200);
  } //while sentbytes
  
  return(1);
}

/*!
 \fn int send_t3_event()
 * \brief send stored T3 event to the central DAQ
 * \retval -1 Error
 * \retval 0 No connection to DAQ
 * \retval 1 All ok
 * - If there is no connection to the central DAQ, try to connect
 * - Add requested event to the output buffer
 * - send output buffer to the central DAQ
 * - If this failed, close the connection to the central DAQ
 * 
 * \author C. Timmermans
 */

int send_t3_event()
{
  int32_t sentBytes;
  int32_t rsend;
  char *bf = (char *)DU_output;
  int32_t length,bsent;
  DU_alength= sizeof(DU_address);
  
  if(DU_comms< 0){
    if(DU_socket < 0){
      if(make_server_connection(du_port)<0)
        return(-1);
    }
    DU_comms = accept(DU_socket, (struct sockaddr*)&DU_address,&DU_alength);
    if(DU_comms <=0){
      if(errno != EWOULDBLOCK && errno != EAGAIN) return(-1); // an error if socket is not non-blocking
      return(0);
    }
    FD_ZERO(&sockset);
    FD_SET(DU_comms, &sockset);
    set_socketoptions(DU_comms);
  }
  buffer_add_t3(&(DU_output[1]),MAX_OUT_MSG-3,station_id);
  if(DU_output[1] == 0) return(0); // nothing to do
  //printf("Sending T3 event %d\n",DU_output[1]);
  DU_output[0] = DU_output[1]+2;
  DU_output[DU_output[0]-1] = GRND1;
  DU_output[DU_output[0]] = GRND2;
  //for(int i=0;i<10;i++) printf("%d ",DU_output[i]);
  //printf("\n");
  length = 2*DU_output[0]+2;
  sentBytes = 0;
  while(sentBytes<length){
    if(length-sentBytes>64) bsent=64;
    else 
      bsent = length-sentBytes;
    rsend = sendto(DU_comms, &(bf[sentBytes]),bsent, 0,
                   (struct sockaddr*)&DU_address,DU_alength);
    if(rsend<0 && errno != EAGAIN) {
      printf("Sending event failed %d %d %s\n",length,sentBytes,strerror(errno));
      sentBytes = rsend;
      break;
    }
    if(rsend>0) sentBytes +=rsend;
    usleep(200);
  } //while sentbytes
  if(sentBytes < length) { // it did not work
    printf("Sending event failed %d\n",length-sentBytes);
    shutdown(DU_comms,SHUT_RDWR);
    close(DU_comms);
    DU_comms = -1;
    return(-1);
  }
  //printf("Sending event succeeded\n");
  return(1);
}

/*!
 \fn void du_scope_check_commands()
 * \brief interpreting DAQ commands for fpga
 * - while there is a command in shared memory
 *    - get the command tag
 *      - on DU_BOOT, DU_RESET and DU_INITIALIZE initialize the scope and set parameters
 *      - on DU_START start run
 *      - on DU_STOP stop run
 *      - on DU_CALIBRATE perform calibration
 *      - no other commands are implemented
 *    - remove "to be executed" mark for this command
 *    - go to next command in shared memory
 *
 * \author C. Timmermans
 */

void du_scope_check_commands()
{
  uint16_t *msg_start;
  uint16_t msg_tag,msg_len,addr;
  uint32_t il;
  uint16_t *sl = (uint16_t *)shadowlist;
  du_geteventbody *getevt;
  uint32_t ssec;
  uint16_t trflag;

  //printf("Check cmds %d\n",*shm_cmd.next_read);
  while(((shm_cmd.Ubuf[(*shm_cmd.size)*(*shm_cmd.next_read)]) &1) ==  1){ // loop over the T3 input
    //printf("Received command\n");
    msg_start = (uint16_t *)(&(shm_cmd.Ubuf[(*shm_cmd.size)*(*shm_cmd.next_read)+1]));
    msg_len = msg_start[AMSG_OFFSET_LENGTH];
    msg_tag = msg_start[AMSG_OFFSET_TAG];
    
    switch(msg_tag){
      case DU_BOOT:
      case DU_RESET:
      case DU_INITIALIZE:
        printf("Initializing scope\n");
        scope_initialize(&station_id); // resets and initialize scope
        il = 3;
        while(il<msg_len){ //word swapping
	  addr = msg_start[il+1]>>1;
	  //if((addr&1))addr -=1;
	  //else addr+=1;
          if(msg_start[il+1]<Reg_End) sl[addr] = msg_start[il+2];
          il+=3;
	}  
        scope_copy_shadow();
        scope_create_memory();
        break;
      case DU_START:
        printf("Trying to start a run\n");
        scope_start_run();                  // start the run
        run = 1;
        break;
      case DU_STOP:
        scope_stop_run();                  // stop the run
        run = 0;
        break;
      case DU_GETEVENT:                 // request event
      case DU_GET_MINBIAS_EVENT:                 // request event
      case DU_GET_RANDOM_EVENT:                 // request event
	getevt = (du_geteventbody *)&msg_start[AMSG_OFFSET_BODY];
	trflag = 0;
	if(msg_tag == DU_GET_MINBIAS_EVENT)trflag = TRIGGER_T3_MINBIAS;
	if(msg_tag == DU_GET_RANDOM_EVENT)trflag = TRIGGER_T3_RANDOM;
	ssec = (getevt->NS3+(getevt->NS2<<8)+(getevt->NS1<<16));
	printf("Requesting Event %d %d %d %d\n",getevt->event_nr,msg_tag,getevt->sec,ssec);
	scope_event_to_shm(getevt->event_nr,trflag,getevt->sec,ssec);
        break;
      default:
        printf("Received unimplemented message %d\n",msg_tag);
    }
    shm_cmd.Ubuf[(*shm_cmd.size)*(*shm_cmd.next_read)] &= ~1;
    *shm_cmd.next_read = (*shm_cmd.next_read) + 1;
    if( *shm_cmd.next_read >= *shm_cmd.nbuf) *shm_cmd.next_read = 0;
  }
}

/*!
 \fn void du_scope_main()
 * \brief main fpga handling routine
 * - opens connection to fpga
 * - enables the pps and sending of data
 * - stop the run
 * - continuous while loop
 *    - when running:
 *      - read data, on error flush the connection
 *      - if there is no data for 20 sec, give start run command to fpga
 *    - when not running
 *      - read data from fpga, do not update the ringbuffer
 *    - check if there has been a command from the DAQ
 *
 * \author C. Timmermans
 */

void du_scope_main()
{
  int i;
  
  scope_open();           // open connection to the scope
  scope_stop_run(); // we will not start in running mode for now
  run = 0; // this is because of recovery from crashes
  while(stop_process == 0){
    if(run){
      if((i =scope_run_read(&station_id)) < 0){
        scope_flush();
        printf("Error reading scope %d\n",i);  // read out the scope
      }
    }else{
      scope_no_run_read();                   // read out scope without updating ringbuffer
    }
    du_scope_check_commands();
  }
}

void du_monitor_main()
{
  monitor_open();
  while(1){
    monitor_read();
  }
}

/*!
 \fn void du_get_station_id()
 * Obtains station id from the ip address of the machine.
 * The ip address can be found in /etc/network/interfaces
 *
 * \author C. Timmermans
 */
void du_get_station_id()
{
    FILE *fpn;
    char line[100],wrd[20];
    int n1,n2,n3,n4;
    
    station_id = -1;
    fpn = fopen("/etc/network/interfaces","r");
    if(fpn == NULL) return;
    while(line == fgets(line,199,fpn)){
        if(sscanf(line,"%s %d.%d.%d.%d",wrd,&n1,&n2,&n3,&n4) == 5){
            if(strncmp(wrd,"address",7) == 0){
                station_id = n4;
                break;
            }
        }
    }
    fclose(fpn);
    printf("Read station %d\n",station_id);
}

/*!
 \fn void du_socket_main(int argc,char **argv)
 * \brief Handles all communication with the IP socket.
 * - Opens a connection to the central DAQ
 *  - In an infinite loop
 *     - Every 0.3 seconds
 *       - Send requested events
 *       - Check commands from the central DAQ
 *       - send data to the central DAQ
 *     - If there has not been contact for 20 seconds
 *       - reboot the PC
 *     - If there has not been contact for 13 seconds
 *       - close and reopen the socket
 *
 * \author C. Timmermans
 */
void du_socket_main(int argc,char **argv)
{
  int i=0,du_port=DU_PORT;
  struct timeval tprev,tnow,tcontact;
  struct timezone tzone;
  float tdif,tdifc;
  int prev_msg = 0;
  int prevgps = -1;
  uint32_t tgpsprev=0;
#ifdef Fake
    if(argc >= 2) {
        sscanf(argv[1],"%d",&du_port);
        if(du_port == -1) du_port = DU_PORT;
    }
#endif
  printf("Opening Connection %d\n",du_port);
  if(make_server_connection(du_port) < 0) {  // connect to DAQ
    printf("Cannot open sockets\n");
    exit(-1);
  }
  printf("Connection opened\n");
  gettimeofday(&tprev,&tzone);
  tcontact.tv_sec = tprev.tv_sec;
  tcontact.tv_usec = tprev.tv_usec;
  while(stop_process == 0){
    //printf("In the main loop\n");
    gettimeofday(&tnow,&tzone);
    tdif = (float)(tnow.tv_sec-tprev.tv_sec)+(float)( tnow.tv_usec-tprev.tv_usec)/1000000.;
    if(tdif>=0.3 || prev_msg == 1 ){                          // every 0.3 seconds, this is really only needed for phase2
      while(send_t3_event() > 0) usleep(10);
      prev_msg = 0;
      //printf("Check server data\n");
      if(check_server_data() > 0){  // check on an AERA command
        //printf("handled server data\n");
        tcontact.tv_sec = tnow.tv_sec;
        tcontact.tv_usec = tnow.tv_usec;
        prev_msg = 1;
      }
      if(send_server_data() > 0){                  // send data to AERA
        tcontact.tv_sec = tnow.tv_sec;
        tcontact.tv_usec = tnow.tv_usec;
      }
      tprev.tv_sec = tnow.tv_sec;
      tprev.tv_usec = tnow.tv_usec;
    }
    if(tgpsprev == 0) tgpsprev = tnow.tv_sec;
    if((tnow.tv_sec-tgpsprev)>20){
      if(*(shm_gps.next_read) != *(shm_gps.next_write)){
        if(*(shm_gps.next_write) == prevgps){
          printf("There used to be a reboot due to timeout on socket\n");
        //  system("/sbin/reboot");
        }
        prevgps = *(shm_gps.next_write);
      }
      tgpsprev = tnow.tv_sec;
    }
    tdifc = (float)(tnow.tv_sec-tcontact.tv_sec)+(float)( tnow.tv_usec-tcontact.tv_usec)/1000000.;
    if(tdifc>13.){ // no contact for 13 seconds!
      if(DU_comms >=0){
        shutdown(DU_comms,SHUT_RDWR);
        close(DU_comms);
        DU_comms = -1;
      }
      if(DU_socket >=0){
        shutdown(DU_socket,SHUT_RDWR);
        close(DU_socket);
        DU_socket = -1;
      }
      sleep(5);
      make_server_connection(du_port);
      tcontact.tv_sec = tnow.tv_sec;
    }
    usleep(10000);
  }
}

/*! 
 \fn int main(int argc, char **argv)
 * main program:
 * - Initializes responses to signals
 * - read station ID from hosts file
 * - Create shared memories
 * - Create child processes for:
 *   - Reading/writing IP-sockets
 *   - Reading/writing the scope
 * - Restart these processes when needed
 * - After a stop is requested through an interrupt, clean up shared memories
 *
 \param argc Number of arguments when starting the program (not used)
 \param argv character string of arguments (not used)
 \author C. Timmermans
 */
int main(int argc, char **argv)
{
    pid_t pid;
    int32_t status;
    
    signal(SIGHUP,clean_stop);
    signal(SIGINT,clean_stop);
    signal(SIGTERM,clean_stop);
    signal(SIGABRT,clean_stop);
    signal(SIGKILL,clean_stop);
    
#ifndef Fake
    du_get_station_id();
#else
    if(argc < 2) station_id = DU_PORT;
    else sscanf(argv[1],"%d",&station_id);
#endif
    if(ad_shm_create(&shm_ev,MAXT3,MAX_READOUT) <0){ //ad_shm_create is in shorts!
      printf("Cannot create T3  shared memory !!\n");
      exit(-1);
    }
    *(shm_ev.next_read) = 0;
    *(shm_ev.next_write) = 0;
    if(ad_shm_create(&shm_ts,BUFSIZE,sizeof(TS_DATA)/sizeof(uint16_t)) <0){ //ad_shm_create is in shorts!
      printf("Cannot create Timestamp shared memory !!\n");
      exit(-1);
    }
    *(shm_ts.next_read) = 0;
    *(shm_ts.next_write) = 0;
    timestampbuf = (TS_DATA *)shm_ts.Ubuf;
    if(ad_shm_create(&shm_gps,GPSSIZE,sizeof(GPS_DATA)/sizeof(uint16_t)) <0){ //ad_shm_create is in shorts!
        printf("Cannot create GPS shared memory !!\n");
        exit(-1);
    }
    *(shm_gps.next_read) = 0;
    *(shm_gps.next_write) = 0;
    gpsbuf = (GPS_DATA *) shm_gps.Ubuf;
    if(ad_shm_create(&shm_cmd,MSGSTOR,MAXMSGSIZE) <0){
        printf("Cannot create CMD shared memory !!\n");
        exit(-1);
    }
    if(ad_shm_create(&shm_mon,MONBUF,N_MON) <0){
        printf("Cannot create Monitor shared memory !!\n");
        exit(-1);
    }
    if((pid_scope = fork()) == 0) du_scope_main();
    if((pid_monitor = fork()) == 0) du_monitor_main(argc,argv);
    if((pid_socket = fork()) == 0) du_socket_main(argc,argv);
    while(stop_process == 0){
      pid = waitpid (WAIT_ANY, &status, 0);
      if(pid == pid_scope && stop_process == 0) {
	if((pid_scope = fork()) == 0) du_scope_main();
      }
      if(pid == pid_monitor && stop_process == 0) {
	if((pid_monitor = fork()) == 0) du_monitor_main();
      }
      if(pid == pid_socket && stop_process == 0) {
	if((pid_socket = fork()) == 0) du_socket_main(argc,argv);
      }
      sleep(1);
    }
    remove_shared_memory();
}
