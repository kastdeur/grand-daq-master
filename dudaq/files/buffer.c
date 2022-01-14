/// @file
/// @brief routines for handling of the memory buffers
/// @author C. Timmermans, Nikhef/RU
/************************************
File: buffer.c
Author: C. Timmermans
Version 1.0

This file handles the buffers needed for the DAQ of the DU_NL Software.
The following buffers are present:

External buffers:
timestampbuf: holds the events read-out from the digitizer
        evread: index of location of next event read-out
        t2prev: index of location after previous T2 stamp sent

monbuf: holds monitor information (really parameter requests) from the digitizer
        evmon: index of location of next monitor data read out
        monprev: index of location of next monitor data sent to DAQ

Routines:
buffer_add_t2(short id) moves the timestamps of the events readout since last T2request into a new T2-request
buffer_add_t3(short id) moves the T3-buffers to the DAQ when they are ready
buffer_add_monitor(short id) moves monitor info to the daq. (this is really all scope parameters)
print_message(AMSG *msg) prints the complete content of a message (hex and dec)!

 ************************************/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "dudaq.h"
#include "amsg.h"
#include "ad_shm.h"
#include "scope.h"


extern shm_struct shm_ev;         //!< shared memory containing all event info, including read/write pointers
extern shm_struct shm_ts;
extern shm_struct shm_gps;        //!< shared memory containing all GPS info, including read/write pointers

extern TS_DATA *timestampbuf;
extern int evread;              //!< pointer to the last event read

extern GPS_DATA *gpsbuf;
extern int32_t evgps;                      //!< pointer to next GPS info


extern int32_t firmware_version;                             

extern float *ch_volt;
extern float *ch_cur;

/*!
* \fn int buffer_add_t2(unsigned short *bf,int bfsize,short id)
* \brief copies timestamps from all-event buffer into t2-list for DAQ.
* \param bf buffer that holds the output message to sent to DAQ
* \param bfsize maximum length of the information to be stored in bf
* \param id        local station identifier
* \retval n number of T2-timestamps in the list
* - If all timestamps are sent, do nothing
* - Create a message in buffer "bf"
* - For all events in memory, until the second marker changes or until the output buffer is full
*   - get the subseconds
*   - get the trigger mask
*   - fill the T2-message with subseconds and trigger info
*   - increase the total buffer size used
*   - go to next position in message buffer
*   - go to next event in memory
* - add 1 to the GPS second counter
* - write message length in first word of message
* \author C. Timmermans

 */
int buffer_add_t2(unsigned short *bf,int bfsize,short id) {
  
  unsigned int *s=(unsigned int *)(&(bf[3]));                     // pointer to where the seconds will be stored
  unsigned int ss;
  unsigned int ssp;
  T2SSEC *ssec;                                                   // pointer to where nanosec. are stored
  int n_t2=0;
  int iten;
  int bffil = 0;
  int next_read = *(shm_ts.next_read);
  int next_write = *(shm_ts.next_write);
  
  if(next_read == next_write) {
    bf[0] = 0;
    return(0);                                 // no T2 to be sent
  }
  bf[1] = DU_T2;                                                  // tag (message header)
  bf[2] = id;
  *s = (unsigned int)timestampbuf[next_read].ts_seconds;
  bffil=5;                                                        // the T2 body
    
  while(next_read != next_write && timestampbuf[next_read].ts_seconds == *s     // until all timestamps are done (or we move into next second)
        && bffil<(bfsize-2) ){                                     // maximally the whole socket-buffer
    ss = (unsigned int)(timestampbuf[next_read].ts_nanoseconds>>6);      // shift nanosec 6 bits
    ssec = (T2SSEC *)(&(bf[bffil]));
    iten = 0;
    if((timestampbuf[next_read].trigmask&0x20)!=0) iten +=4; //10 sec
    if((timestampbuf[next_read].trigmask&0xf)!=0) iten +=2; //ant
    T2FILL(ssec,ss,
           ((timestampbuf[next_read].ts_nanoseconds>>2)&0xf)+
           ((iten&0xf)<<4));               // fill subsec bytes appropriately
    bffil +=2;                                                    // update buffer counter
    n_t2++;
    next_read++;                                                     // update circular event counter
    if(next_read >= BUFSIZE) next_read = 0;
  }
  if(n_t2 == 0) bffil = 0;
  *(shm_ts.next_read) = next_read;
  bf[0] = bffil;                                                  // set total message size
  return(n_t2);
}

/*!
 * \fn int buffer_add_t3(unsigned short *bf,int bfsize,short id)
 * \brief copies events that are ready into DU_EVENT messages for the DAQ
 * \param bf buffer that holds the output message to sent to DAQ
 * \param bfsize maximum length of the information to be stored in bf
 * \param id        local station identifier
 * \retval n number of events copied
 * - If all requested T3 events are sent, do nothing
 * - Add event
 * - Update the pointer in the T3 ringbuffer
 * - Add 1 to the number of T3 events sent
 * \author C. Timmermans
*/
int buffer_add_t3(unsigned short *bf,int bfsize,short id) {
  uint16_t *t3buf = (uint16_t *)shm_ev.Ubuf;
  if(t3buf == NULL) {
    bf[0] = 0;
    return(0);
  }
  int next_read = *(shm_ev.next_read) ;
  int next_write = *(shm_ev.next_write) ;
  uint16_t *msg_start = bf;
  
  bf[0] = 0;
  msg_start[AMSG_OFFSET_TAG] = DU_EVENT;
  if(next_read == next_write)  return(0); // nothing needed
  if(t3buf[EVT_LENGTH]<bfsize) return(0); //nothing can be done
  memcpy(&msg_start[AMSG_OFFSET_BODY],&t3buf[next_read*t3buf[EVT_LENGTH]],t3buf[EVT_LENGTH]*sizeof(uint16_t));
  //printf("Copying %d words\n",t3buf[EVT_LENGTH]);
  bf[0] = t3buf[EVT_LENGTH]+2;
  next_read++;
  if(next_read>=MAXT3) next_read = 0;
  *(shm_ev.next_read) = next_read;
  return(1);
}

/*!
 * \fn int buffer_add_monitor(unsigned short *bf,int bfsize,short id)
 * \brief add monitor events into the output buffer
 * \param bf buffer that holds the output message to sent to DAQ
 * \param bfsize maximum length of the information to be stored in bf
 * \param id        local station identifier
 * \retval n number of monitoring blocks copied
 * - If there is monitoring data available
 *   - create message header
 *   - add scope id, and firmware version
 *   - add second counter
 *   - add total trigger rate and trigger rate for each of the channels
 *   - add temperature (GPS)
 *   - add voltage and current (Charge controller)
 * \author C. Timmermans
 */
int buffer_add_monitor(unsigned short *bf,int bfsize,short id) {

  int len;                              // total length of the message body
  int i;
  int n_send = 0;
  static int gpssent=0;
  int bffil = 0;
  float tmp;

  bf[0] = 0;
  if(gpssent != *(shm_gps.next_read)) {
    gpssent = *(shm_gps.next_read);
    bf[bffil] = 19;                                                // length
    bf[bffil+1] = DU_MONITOR;                                           // tag
      bf[bffil+2] =  id;//((id&0xff) | ((DU_HWNL&0xf)<<8) | ((FIRMWARE_VERSION(firmware_version&0x1f))<<11)); // id of the scope+HWtype +HWvers
    memcpy((void *)&bf[bffil+3],(void *)&(firmware_version),4);
    memcpy((void *)&bf[bffil+5],(void *)&(gpsbuf[gpssent].ts_seconds),4);
    bf[bffil+7]= *(unsigned short *)&(gpsbuf[gpssent].data[PPS_TRIG_RATE]);
    //memcpy((void *)&bf[bffil+14],ch_volt,4);
    //memcpy((void *)&bf[bffil+16],ch_cur,4);
    bf[bffil+18] = gpsbuf[gpssent].data[PPS_STATSEC];
    //printf("CC: %g %g\n",*ch_volt,*ch_cur);
    //printf("Send monitor: %d %d %d %d\n",gpsbuf[gpssent].ts_seconds,bf[bffil+5],bf[bffil+6],bf[bffil+7]);
    bffil+=19;
    n_send++;
  }
  return(n_send);
}


/*!
  \fn int print_message(AMSG *msg)
  \brief prints message in hex and dec (debugging purposes)
  \param msg pointer to message
  \retval 1
  \author C. Timmermans
 */
int print_message(AMSG *msg){
  int i,ilen;
  uint16_t *buf = (uint16_t *)msg;
  ilen = msg->length;
  for(i=0;i<ilen;i++){
    printf("%d %x %d\n",i,buf[i],buf[i]);
  }
  return(1);
}
