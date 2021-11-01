/// @file
/// @brief routines for handling of the memory buffers
/// @author C. Timmermans, Nikhef/RU
/************************************
File: buffer.c
Author: C. Timmermans
Version 1.0

This file handles the buffers needed for the DAQ of the DU_NL Software.
The following buffers are present:

*bf : the socket output buffer
t3_buffer: ringbuffer holding at most MAXT3 events to be sent to DAQ
        t3_wait: index of location of next event written int buffer
        t3_send: index of location of next event sent to DAQ

External buffers:
eventbuf: holds the events read-out from the digitizer
        evread: index of location of next event read-out
        t2prev: index of location after previous T2 stamp sent

monbuf: holds monitor information (really parameter requests) from the digitizer
        evmon: index of location of next monitor data read out
        monprev: index of location of next monitor data sent to DAQ

Routines:
buffer_to_t3(unsigned short event_nr, unsigned short isec,unsigned int ssec)  writes event to T3-buffer
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


#define MAXDT 1.e-5                                         //!< max 10 musec difference with T3 time

EV_DATA t3_buffer[MAXT3];              //!< buffer of T3 events that need to be transferred to the DAQ

int32_t t3_wait=0;                                              //!< the pointer to the last event waiting to be sent
int32_t t3_send=0;                                              //!< the pointer to the last event that was sent

extern shm_struct shm_ev;         //!< shared memory containing all event info, including read/write pointers
extern shm_struct shm_gps;        //!< shared memory containing all GPS info, including read/write pointers
//extern EV_DATA eventbuf[BUFSIZE];
extern EV_DATA *eventbuf;       //!< buffer that holds all triggered events (points to shared memory)
extern int evread;              //!< pointer to the last event read

extern GPS_DATA *gpsbuf;
extern int32_t evgps;                      //!< pointer to next GPS info


extern int32_t firmware_version;                             

extern float *ch_volt;
extern float *ch_cur;
/*!
 * \fn int buffer_to_t3(unsigned short event_nr, unsigned short isec,unsigned int ssec)
 * \brief copy buffer from all-event buffer into t3-buffer
 * \param event_nr  event number from T3Maker
 * \param isec      (seconds&0xff) from T3Maker
 * \param ssec      (nanoseconds>>6) from T3maker
 * \retval 0 fails
 * \retval        1 success
 * - loop over all event buffers to find the requested one based upon time
 * - copy the event buffer into the t3buffer
 * - reset the time of the event buffer, this will not be used anymore
 * - add the requested event number to the t3 buffer
 * - update the pointer in the t3 ringbuffer
  \author C. Timmermans
 */
int buffer_to_t3(unsigned short event_nr, unsigned short isec,unsigned int ssec,uint16_t trflag)  {
  int i,ibuf,prevgps;
  int tmin,tdif;
  
  ibuf = -1;
  for(i=0;i<BUFSIZE &&ibuf==-1;i++){                                 // loop to find the correct event
       if(((eventbuf[i].ts_seconds+1) &0xff) == isec &&                 // dutch electronics is slow with GPS
       ssec == (eventbuf[i].t2_nanoseconds>>6)
       && eventbuf[i].ts_seconds != 0) ibuf = i;
  }
  if(ibuf == -1) {
    tmin = 10000;
    for(i=0;i<BUFSIZE;i++){                                 // loop to find the correct event
      if(((eventbuf[i].ts_seconds+1) &0xff) == isec){
	tdif = eventbuf[i].t2_nanoseconds-(ssec<<6);
	if(tdif<0) tdif = -tdif;
	if(tdif<tmin) {
	  tmin = tdif;
	  ibuf = i;
	}
      }
    }
  }
  if(ibuf == -1){
    printf("Did not find the requested buffer isec %u ssec %u (0x%x)\n", isec, ssec,ssec);
    return(0);
  }
  //printf("Copy the buffer to the output %d\n",event_nr);
  memcpy(&(t3_buffer[t3_wait]),&(eventbuf[ibuf]),sizeof(EV_DATA)); // found it, now copy the data
  eventbuf[ibuf].ts_seconds = 0;                                   // reset the time, so this buffer is never used
  t3_buffer[t3_wait].event_nr = event_nr;                          // put the event number to the data
  t3_buffer[t3_wait].trig_flag = trflag;                          // put the trigger flag

  //if(t3_buffer[t3_wait].t3calc == 1) {
  //  printf("T3 nsec has been calculated\n");
  //}
  prevgps = evgps-1;
  if(prevgps<0)prevgps = GPSSIZE-1;
  //if(t3_buffer[t3_wait].ts_seconds <(gpsbuf[prevgps].ts_seconds-5)) reset_socket();
  t3_wait++;                                                       // update the pointer of events waiting to be sent
  if(t3_wait >=MAXT3) t3_wait = 0;                                 // it is a ringbuffer!
  return(1);
}

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
  int next_read = *(shm_ev.next_read);
  int next_write = *(shm_ev.next_write);
  
  if(next_read == next_write) {
    bf[0] = 0;
    return(0);                                 // no T2 to be sent
  }
    bf[1] = DU_T2;                                                  // tag (message header)
    bf[2] = id;//we have many more stations!
    //bf[2] = ((id&0xff) |((DU_HWNL&0xf)<<8) | ((FIRMWARE_VERSION(firmware_version&0x1f))<<11));         // Local Station ID (message header)
    *s = (unsigned int)eventbuf[next_read].ts_seconds;                 // store the seconds
    bffil=5;                                                        // the T2 body
    
  ssp = 1000000001; //impossible
  while(next_read != next_write && eventbuf[next_read].ts_seconds == *s     // until all timestamps are done (or we move into next second)
        && bffil<(bfsize-2) ){                                     // maximally the whole socket-buffer
    ss = (unsigned int)(eventbuf[next_read].t2_nanoseconds>>6);      // shift nanosec 6 bits
    ssec = (T2SSEC *)(&(bf[bffil]));
    iten = 0;
    if((eventbuf[next_read].buf[EVENT_TRIGMASK]&0x20)!=0) iten +=4;
#ifdef STAGE1
    if((eventbuf[next_read].buf[EVENT_TRIGMASK+1]&0xf)!=0) iten +=2; //ant
#else
    if((eventbuf[next_read].buf[EVENT_TRIGMASK+1]&0xc) !=0) iten +=1; //sc
    if((eventbuf[next_read].buf[EVENT_TRIGMASK+1]&0x3)!=0) iten +=2; //ant
#endif
    if((iten &4) != 0) printf("A 10sec trigger\n");
    //printf("%d %d %x %x %x\n",*s,ss,eventbuf[next_read].buf[EVENT_TRIGMASK],eventbuf[next_read].buf[EVENT_TRIGMASK+1],iten);
    T2FILL(ssec,ss,
           ((eventbuf[next_read].t2_nanoseconds>>2)&0xf)+
           ((iten&0xf)<<4));               // fill subsec bytes appropriately
    if(eventbuf[next_read].t2_nanoseconds == ssp){
      printf("TWICE THE SAME EVENT %d \n",ssp);
    }
    ssp =  eventbuf[next_read].t2_nanoseconds;
    bffil +=2;                                                    // update buffer counter
    n_t2++;
    next_read++;                                                     // update circular event counter
    if(next_read == BUFSIZE) next_read = 0;
  }
  *s = (*s)+1;                                                    // Dutch electronics reads GPS slow
  if(n_t2 == 0) bffil = 0;
  *(shm_ev.next_read) = next_read;
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
 * - Loop over all waiting events
 *   - When the nanoseconds are not yet precisely calculated, perform this calculation
 * - Skip events older than 5 seconds for which the time has not been calculated properly
 * - Calculate the size of the next event and check if it fits in the output message
 * - Fill the AERA standard event header
 * - Add the hardware supplied event header
 * - Add the GPS quant. corrections belonging to the event
 * - Add the hardware settings from the PPS message
 * - Add the ADC values
 * - Update the pointer in the T3 ringbuffer
 * - Add 1 to the number of T3 events sent
 * \author C. Timmermans
*/
int buffer_add_t3(unsigned short *bf,int bfsize,short id) {
  int n_event = 0;
  unsigned int gpsSeconds, gpsNanoseconds;
  int i,sz;
  int16_t nl_hlen;
  int prevgps;
  uint16_t *msg_start;
  uint16_t trace_length;
  uint16_t chm,cha;
  int bffil = 0;
  
  bf[0] = 0;
  if(t3_send ==t3_wait)  return(0); // nothing needed
  i = t3_send;
  prevgps = evgps-1;
  if(prevgps<0) prevgps = GPSSIZE-1;
  while(i!= t3_wait) {
    if(t3_buffer[i].t3calc == 0) {
      scope_calc_t3nsec(&(t3_buffer[i]));
    }
    i++;
    if(i>=MAXT3) i = 0;
  }
  while( (t3_buffer[t3_send].ts_seconds < (gpsbuf[prevgps].ts_seconds-5)) && t3_buffer[t3_send].t3calc == 0) {
    t3_send++;
    if(t3_send>=MAXT3) t3_send = 0;
    if(t3_send == t3_wait)return(0);
  }
  chm = 0;
  trace_length = 0;
  for(i=0;i<4;i++){
    cha = *(uint16_t *)(&t3_buffer[t3_send].buf[EVENT_LENCH1+2*i]);
    if(cha>0) chm |=(1<<i);
    trace_length +=cha;
  }
  nl_hlen =  14+(EVENT_CTRL-EVENT_TRIGMASK) ; //(38+292 = 330)
  sz =nl_hlen+2*trace_length;
  int len = ((sz)/sizeof(uint16_t));   // length of ADC information
  if(len >= (bfsize-bffil-(5+MIN_EVHEADER_LENGTH))){
    printf("Cannot add a buffer of length %d (%d %d)\n",len,bfsize,bffil);
    return(0);                                                           // event does not fit
  }
  if(!t3_buffer[t3_send].t3calc) printf("Timing of buffer %d still needs to be done\n",t3_send);
  //printf("Send %d Wait %d\n",t3_send,t3_wait);
  if((t3_send != t3_wait)           // events to send, used to be a while loop
     && (len < (bfsize-bffil-(5+MIN_EVHEADER_LENGTH)))      // fit in buffer
     && (t3_buffer[t3_send].t3calc == 1)
     ){                            // nanoseconds are known
    
    /* Note: casting the bf array pointer as pointers to various structs (such as AMSG and
     EVENTBODY is not reliable).  Instead of assuming the structs are packed, we use halfword
     (16b) offsets into the message and event body to write the data.  */
    
    msg_start = &bf[bffil];
    
    msg_start[AMSG_OFFSET_TAG] = DU_EVENT;
    msg_start[AMSG_OFFSET_BODY + EB_OFFSET_EVENT_NR] = t3_buffer[t3_send].event_nr;
      msg_start[AMSG_OFFSET_BODY + EB_OFFSET_DU_ID] = id;//((id&0xff) | ((DU_HWNL&0xf)<<8) |
                                                     //((FIRMWARE_VERSION(firmware_version))<<11)); // id of the scope+HWtype +HWvers
    //printf("The firmware = %d, board = %d resolution=%d\n",FIRMWARE_VERSION(firmware_version),SERIAL_NUMBER(firmware_version),ADC_RESOLUTION(SERIAL_NUMBER(firmware_version)));
    msg_start[AMSG_OFFSET_BODY + EB_OFFSET_HDR_LENGTH] = MIN_EVHEADER_LENGTH+(nl_hlen/2);  // we add the thresholds to the data
    
    
    gpsSeconds = t3_buffer[t3_send].ts_seconds+1;                       // dutch electronics reads gps seconds slow
    gpsNanoseconds = t3_buffer[t3_send].t3_nanoseconds;
    
    /* Convert unsigned ints to two unsigned shorts */
    msg_start[AMSG_OFFSET_BODY + EB_OFFSET_GPSSEC] = gpsSeconds & 0xffff;
    msg_start[AMSG_OFFSET_BODY + EB_OFFSET_GPSSEC + 1] = gpsSeconds >> 16;
    
    msg_start[AMSG_OFFSET_BODY + EB_OFFSET_GPSNSEC] = gpsNanoseconds & 0xffff;
    msg_start[AMSG_OFFSET_BODY + EB_OFFSET_GPSNSEC + 1] = gpsNanoseconds >> 16;
    
    cha = *(uint16_t *)(&t3_buffer[t3_send].buf[EVENT_TRIGMASK]);
    msg_start[AMSG_OFFSET_BODY + EB_OFFSET_TRIG_FLAG]  = 0;
    if(cha &TRIG_10SEC) msg_start[AMSG_OFFSET_BODY + EB_OFFSET_TRIG_FLAG] |= RANDOM_TRIGGER;
    if(cha &TRIG_CAL) msg_start[AMSG_OFFSET_BODY + EB_OFFSET_TRIG_FLAG] |= CALIB_TRIGGER;
    if(msg_start[AMSG_OFFSET_BODY + EB_OFFSET_TRIG_FLAG]==0)msg_start[AMSG_OFFSET_BODY + EB_OFFSET_TRIG_FLAG] = SELF_TRIGGERED;
    msg_start[AMSG_OFFSET_BODY + EB_OFFSET_TRIG_FLAG] |= t3_buffer[t3_send].trig_flag&0xffff;
    if((cha & TRIG_10SEC) || cha & TRIG_CAL)
      msg_start[AMSG_OFFSET_BODY + EB_OFFSET_TRIG_POS] =
      *(uint16_t *) &t3_buffer[t3_send].buf[EVENT_CTRL+6]; // use ch1 pre-trigger
    else
      msg_start[AMSG_OFFSET_BODY + EB_OFFSET_TRIG_POS] =
      *(uint16_t *) &t3_buffer[t3_send].buf[EVENT_WINDOWS]; // use ch1 pre-trigger
    msg_start[AMSG_OFFSET_BODY + EB_OFFSET_SAMP_FREQ] = SAMPLING_FREQ;
    msg_start[AMSG_OFFSET_BODY + EB_OFFSET_CHAN_MASK] = chm;
    msg_start[AMSG_OFFSET_BODY + EB_OFFSET_ADC_RES] = ADC_RESOLUTION;
    msg_start[AMSG_OFFSET_BODY + EB_OFFSET_TRACELENGTH] = trace_length; //sum of all channels
#ifdef USE_EVENTBODY_VERSION
    msg_start[AMSG_OFFSET_BODY + EB_OFFSET_VERSION] = EVENTBODY_VERSION;
#endif
    msg_start[AMSG_OFFSET_BODY + EB_OFFSET_LENGTH] = msg_start[AMSG_OFFSET_BODY + EB_OFFSET_HDR_LENGTH] + trace_length + 3;
    msg_start[AMSG_OFFSET_LENGTH] =  msg_start[AMSG_OFFSET_BODY + EB_OFFSET_LENGTH] + 2;
    sz = EVENT_CTRL-EVENT_TRIGMASK;
    memcpy(&(msg_start[AMSG_OFFSET_BODY + EB_OFFSET_ADCBUFFER]),
           &(t3_buffer[t3_send].buf[EVENT_TRIGMASK]),sz); // event header
    memcpy(&(msg_start[AMSG_OFFSET_BODY + EB_OFFSET_ADCBUFFER+(sz/2)]),
           &(t3_buffer[t3_send].quant1),14); // 2 floats + 1 32bit int + 1 16bit int
    sz+=14;
    memcpy(&(msg_start[AMSG_OFFSET_BODY + EB_OFFSET_ADCBUFFER+(sz/2)]),
           &(t3_buffer[t3_send].buf[EVENT_ADC]),2*trace_length); // event header
    sz+=trace_length;
    bffil += (msg_start[AMSG_OFFSET_LENGTH]);
    //printf("Moved T3 buffer %d\n",msg_start[AMSG_OFFSET_BODY + EB_OFFSET_EVENT_NR]);
    n_event++;
    t3_send++;                                                           // update the t3_send index in the t3_buffer ringbuffer
    if(t3_send >= MAXT3)t3_send = 0;
  }
  return(n_event);
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
