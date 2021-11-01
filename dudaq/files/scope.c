/// @file scope.c
/// @brief routines interfacing to the fpga
/// @author C. Timmermans, Nikhef/RU

/************************************
 File: scope.c
 Author: C. Timmermans
 
 
 Buffers:
 eventbuf: holds the events read-out from the digitizer
 evread: index of location of next event read-out
 gpsbuf: holds the 1s GPS information, is needed to create proper timing
 evgps: index of location of next GPS data read out
 ************************************/

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <time.h>
#ifdef Fake
#include <math.h>
#include <sys/time.h>
#else
#include <sys/mman.h>
#endif
#include <string.h>
#include<errno.h>

#include "ad_shm.h"
#include "scope.h"


#ifdef Fake
#define MAXRAND 0x7FFFFFFF
#endif
#define DEVFILE "/dev/mem" //!< Device for talking to the FPGA
#define DEV int32_t //!< the type of the device id is really just a 32 bit integer

DEV dev = 0;                    //!< Device id
void *axi_ptr;
uint32_t page_offset;

extern shm_struct shm_ev,shm_gps;
extern EV_DATA *eventbuf;      // buffer to hold the events
extern GPS_DATA *gpsbuf;
uint16_t *evtbuf=NULL;
uint16_t ppsbuf[WCNT_PPS*GPSSIZE];
int n_evt = 0;

int32_t evgps=0;                //!< pointer to next GPS info
int32_t prevgps = 0;
int32_t seczero=0;                //!< seczero keeps track of the number of seconds no data is read out

int16_t cal_type=CAL_END;       //!< what to calibrate (END = nothing)
int32_t firmware_version;       //!< version of the firmware of the scope

int leap_sec = 0;               //!< Number of leap seconds in UTC; read from GPS unit

#define MAXTRY 50               //!< maximal number of loops to complete reading from the FPGA
#define UPDATESEC 100           //!< time interval between succesive rate checks. Only used in dynamic monitoring of rate.
#define MINRRATE (100*UPDATESEC)       //!< minimal radio rate
#define MEANRRATE (200*UPDATESEC)      //!< mean radio rate
#define MAXRRATE (600*UPDATESEC)       //!< maximal radio rate
#define MINSRATE (40*UPDATESEC)        //!< minimal scintillator rate
#define MEANSRATE (50*UPDATESEC)       //!< mean scintillator rate
#define MAXSRATE (80*UPDATESEC)        //!< max scintillator rate
#define HVMAX 0xc0                     //!< maximum HV for PMT's. Only used in dynamic monitoring of rate

uint32_t shadowlist[Reg_End>>2];  //!< all parameters to set in FPGA
uint32_t shadowlistR[Reg_End>>2]; //!< all parameters read from FPGA
int32_t shadow_filled = 0;                               //!< the shadow list is not filled

int32_t tenrate[4]={0,0,0,0};  //!< rate of all channels, to be checked every "UPDATESEC" seconds
int32_t pheight[4]={0,0,0,0};  //!< summed pulseheight of all channels
int32_t n_events[4]={0,0,0,0}; //!< number of events contributing to summed pulse height

int16_t setsystime=0;          //!< check if system time is set
uint16_t evtlen;

/*!
 \func void scope_raw_write(unsigned int *buf, int len)
 \brief writes a buffer to the digitizer
 \param buf pointer to the data to send
 \param           len number of bytes to send
 */
void scope_raw_write(uint32_t reg_addr, uint32_t value)
{
  *((unsigned int *)(axi_ptr+page_offset+reg_addr)) = value;
}

/*!
 \func int scope_raw_read(unsigned char *bf)
 \brief reads data from digitizer and stores it in a buffer
 \param bf pointer to location where data can be stored
 \retval number of bytes read
 */
int32_t scope_raw_read(uint32_t reg_addr, uint32_t *value) //new, reading from AXI
{
  *value = *((unsigned int *)(axi_ptr+page_offset+reg_addr));
  return(1);
}
/*!
 \func void scope_flush()
 \brief empty routine
 */
void scope_flush()
{
  scope_raw_write(Reg_GenControl,0x08000000); // clear DAQ Fifo's
  scope_raw_write(Reg_GenControl,0x00000000);
}

/*!
 \func int scope_open()
 \brief opens connection to digitizer
 \retval -1 failure
 \retval         1 succes
 */
int scope_open()        // Is this needed?
{
  unsigned int addr, page_addr;
  unsigned int page_size=sysconf(_SC_PAGESIZE);

#ifndef Fake
  printf("Trying to open !%s!\n",DEVFILE);
  if ((dev = open(DEVFILE, O_RDWR)) == -1) {
    fprintf(stderr, "Error opening scope device file %s for read/write\n", DEVFILE);
    return(-1);
  }
  addr = (unsigned int) TDAQ_BASE;
  page_addr = (addr & ~(page_size-1));
  page_offset = addr - page_addr;
  
  axi_ptr=mmap(NULL,page_size,PROT_READ|PROT_WRITE, MAP_SHARED,dev,page_addr);
  if ((long)axi_ptr == -1) {
    perror("opening scope\n");
    exit(-1);
  }
  
  printf("Done opening dev = %d\n",(int)dev);
#endif
  sleep(1);
  return(1);
}

/*!
 \func void scope_close()
 \brief closes scope communication
 */
void scope_close() 
{
#ifndef Fake
  close(dev);
#endif
}

/*!
 \func void scope_get_parameterlist(char list)
 \brief request scope parameters (not reading them!)
 \param list
 */
void scope_get_parameterlist(uint8_t list)
{
}

/*!
 \func void scope_reset()
 \brief performs a soft reset on the scope
 */
void scope_reset()
{
  scope_flush(); // no soft reset implemented...
}

/*!
 \func void scope_start_run()
 \brief starts the run
 */
void scope_start_run()
{
  scope_flush();
  scope_set_parameters(Reg_Dig_Control,shadowlist[Reg_Dig_Control>>2] |(CTRL_PPS_EN | CTRL_SEND_EN ),1);
  seczero = 0;
}

/*!
 \func  void scope_stop_run()
 \brief disables output
 */
void scope_stop_run()
{
  scope_set_parameters(Reg_Dig_Control,shadowlist[Reg_Dig_Control>>2] & (~CTRL_PPS_EN & ~CTRL_SEND_EN ),1);
  seczero = 0;
  scope_flush();
}

/*!
 \func  scope_set_parameters(unsigned short int *data,int to_shadow)
 \brief writes a parameter list to the scope (and the shadowlist)
 \param data contains the parameter and its value
 \param to_shadow  If 1 writes to the shadowlist, otherwise do not.
 */
void scope_set_parameters(uint32_t reg_addr, uint32_t value,uint32_t to_shadow)
{
  if(to_shadow == 1) shadowlist[reg_addr>>2] = value;
  scope_raw_write(reg_addr,value);
  usleep(1000);
}

/*!
 \func void scope_reboot()
 \brief resets the fpga
 */
void scope_reboot()
{
  scope_reset();                // for now the same as a reset
}

/*!
 \func void scope_print_parameters(int list)
 \brief dummy routine
 \param list number
 */
void scope_print_parameters(int32_t list) //to be checked
{
}
/*!
 \func void scope_copy_shadow()
 \brief copy every parameter from the shadowlist into the fpga
 */
void scope_copy_shadow()
{
  for(int i=0;i<Reg_End;i+=4){
    scope_set_parameters(i,shadowlist[i>>2],0);
  }
  
}
/*!
 \func void scope_init_shadow()
 \brief initializes the shadow list
 */
void scope_init_shadow()
{
  int32_t list;
  
  shadow_filled = 0;
  memset(shadowlist,0,sizeof(shadowlist));
}

/*!
 \func void scope_initialize()
 \brief initializes shadow memory, resets the digitizer and stops the run
 */
void scope_initialize() //tested 24/7/2012
{
  scope_init_shadow();
  scope_reset();    // reset the scope
  scope_stop_run(); //disable the output
}

void scope_create_memory(){
  evtlen = HEADER_EVT;
  uint16_t *sl = (uint16_t *)shadowlist;
  for(int ich=0;ich<4;ich++){
    if(shadowlist[Reg_TestPulse_ChRead>>2]&(1<<ich)){
      evtlen +=(sl[(Reg_Time1_Pre>>1)+2*ich]+sl[(Reg_Time1_Post>>1)+2*ich]+sl[Reg_Time_Common>>1]);
    }
  }
  if(evtbuf != NULL){
    free(evtbuf);
  }
  printf("Creating a buffer of size %d\n",BUFSIZE*evtlen);
  evtbuf = (uint16_t *)malloc(BUFSIZE*evtlen*sizeof(uint16_t));
}

/*!
 \func void scope_print_pps(uint8_t *buf)
 \brief print all parameters available in a PPS message
 */
void scope_print_pps(uint8_t *buf)
{
  
}

/*!
 \func void scope_print_event(uint8_t *buf)
 \brief print all information from an event read from the fpga
 */
void scope_print_event(uint8_t *buf)  //ok 26/7/2012
{
  
}

/*!
 \func void scope_fill_ph(uint8_t *buf)
 \brief for each channel add to the summed pulse height, also add to the number of events
 */
void scope_fill_ph(uint8_t *buf)
{
  
}

#ifdef Fake
int scope_fake_gps()
{
  int offset = evgps*WCNT_PPS;
  int tmp=0;
  struct timeval tv;
  struct tm *now;
  int next_write = *(shm_ev.next_write);
  
  uint16_t *sl = (uint16_t *)shadowlist;
  ppsbuf[offset] = 0;
  ppsbuf[offset+PPS_MAGIC] = MAGIC_PPS;
  ppsbuf[offset+PPS_TRIG_PAT] = sl[Reg_Trig_Enable>>1];
  ppsbuf[offset+PPS_TRIG_RATE] = n_evt;
  memcpy(&ppsbuf[offset+PPS_CTD],&evtbuf[next_write*evtlen+EVT_CTD],4);
  tmp = SAMPLING_FREQ*1000000; //sampling in MSPS
  memcpy(&ppsbuf[offset+PPS_CTP],&tmp,4);
  tmp = 0; // 0 ns offset between pps
  memcpy(&ppsbuf[offset+PPS_OFFSET],&tmp,4);
  ppsbuf[offset+PPS_LEAP] = 37;
  gettimeofday(&tv,NULL);
  now = localtime(&tv.tv_sec);
  ppsbuf[offset+PPS_YEAR] = 1900+ now->tm_year;
  ppsbuf[offset+PPS_DAYMONTH] = 100*now->tm_mday+now->tm_mon+1;
  ppsbuf[offset+PPS_MINHOUR] = 100*now->tm_min+now->tm_hour;
  ppsbuf[offset+PPS_STATSEC] = now->tm_sec;
  printf("PPS %d %d %d %d\n",evgps,ppsbuf[offset+PPS_TRIG_RATE] ,ppsbuf[offset+PPS_MINHOUR],ppsbuf[offset+PPS_STATSEC]);
  prevgps = evgps;
  evgps++;
  if(evgps>=GPSSIZE)evgps = 0;
  return(0);
}

int scope_fake_event(int32_t ioff)
{
  struct timeval tv;
  static struct timeval tvFake;
  short rate;
  int t_next;
  static int Send_10 = 0;
  int next_write = *(shm_ev.next_write);
  int offset = next_write*evtlen;
  unsigned int nanoseconds,tbuf;
  uint16_t *sl = (uint16_t *)shadowlist;
  
  if(ioff == 0) {
    gettimeofday(&tvFake,NULL);
    usleep(10);
    return(0);
  }
  gettimeofday(&tv,NULL);
  if(Send_10 !=0 || (tv.tv_sec%10)!=0){
    if((tv.tv_sec < tvFake.tv_sec) ||
       ((tv.tv_sec == tvFake.tv_sec)&&(tv.tv_usec < tvFake.tv_usec))) return(0);
  }
  n_evt++;
  eventbuf[next_write].buf[0] = MSG_START;
  eventbuf[next_write].buf[1] = ID_PARAM_EVENT;
  eventbuf[next_write].buf[EVENT_TRIGMASK] =0;
  eventbuf[next_write].ts_seconds = tvFake.tv_sec;
  nanoseconds = 1000*tvFake.tv_usec;
  if(Send_10 == 0){
    eventbuf[next_write].buf[EVENT_TRIGMASK] |=0x20;
    nanoseconds = 100*(((double)(random())/(double)MAXRAND));
  }else
    nanoseconds += 1000*(((double)(random())/(double)MAXRAND)); //add random ns
  eventbuf[next_write].t2_nanoseconds = nanoseconds;
  eventbuf[next_write].t3_nanoseconds =     eventbuf[next_write].t2_nanoseconds;
  eventbuf[next_write].t3calc =     1;
  eventbuf[next_write].CTD = eventbuf[next_write].t2_nanoseconds/5; //clock tick
  eventbuf[next_write].CTP = 200000000; //clock freq.
  eventbuf[next_write].quant1 = 0.;
  eventbuf[next_write].quant2 = 0.;
  eventbuf[next_write].sync = 0;
  eventbuf[next_write].evsize = 70; //only header for now (in bytes)
  evtbuf[offset+EVT_LENGTH] = evtlen;
  evtbuf[offset+EVT_ID] = MAGIC_EVT;
  evtbuf[offset+EVT_HARDWARE] = 1;
  evtbuf[offset+EVT_HDRLEN] = HEADER_EVT;
  memcpy(&evtbuf[offset+EVT_HDRLEN],&tvFake.tv_sec,4);
  memcpy(&evtbuf[offset+EVT_HDRLEN],&nanoseconds,4);
  evtbuf[offset+EVT_TRIGGERPOS] = sl[Reg_Time1_Pre>>1]+sl[Reg_Time_Common>>1];
  evtbuf[offset+EVT_VERSION] = FORMAT_EVT;
  evtbuf[offset+EVT_MSPS] = SAMPLING_FREQ;
  evtbuf[offset+EVT_ADC_RES] = ADC_RESOLUTION;
  evtbuf[offset+EVT_INP_SELECT] = sl[Reg_Inp_Select>>1];
  evtbuf[offset+EVT_CH_ENABLE] = shadowlist[Reg_TestPulse_ChRead]&0xf;
  evtbuf[offset+EVT_TOT_SAMPLES] = evtlen-HEADER_EVT;
  if(evtbuf[offset+EVT_CH_ENABLE]&1)
    evtbuf[offset+EVT_CH1_SAMPLES] = sl[(Reg_Time1_Pre>>1)]+sl[(Reg_Time1_Post>>1)]+sl[Reg_Time_Common>>1];
  else
    evtbuf[offset+EVT_CH1_SAMPLES] = 0;
  if(evtbuf[offset+EVT_CH_ENABLE]&2)
    evtbuf[offset+EVT_CH2_SAMPLES] = sl[(Reg_Time2_Pre>>1)]+sl[(Reg_Time2_Post>>1)]+sl[Reg_Time_Common>>1];
  else
    evtbuf[offset+EVT_CH2_SAMPLES] = 0;
  if(evtbuf[offset+EVT_CH_ENABLE]&4)
    evtbuf[offset+EVT_CH3_SAMPLES] = sl[(Reg_Time3_Pre>>1)]+sl[(Reg_Time3_Post>>1)]+sl[Reg_Time_Common>>1];
  else
    evtbuf[offset+EVT_CH3_SAMPLES] = 0;
  if(evtbuf[offset+EVT_CH_ENABLE]&8)
    evtbuf[offset+EVT_CH4_SAMPLES] = sl[(Reg_Time4_Pre>>1)]+sl[(Reg_Time4_Post>>1)]+sl[Reg_Time_Common>>1];
  else
    evtbuf[offset+EVT_CH4_SAMPLES] = 0;
  evtbuf[offset+EVT_TRIG_PAT] = 0;
  evtbuf[offset+EVT_TRIG_RATE] = ppsbuf[prevgps*WCNT_PPS+PPS_TRIG_RATE]; // from PPS!
  tbuf =nanoseconds/(1000/SAMPLING_FREQ);
  memcpy(&evtbuf[offset+EVT_CTD],&tbuf,4);
  memcpy(&evtbuf[offset+EVT_CTP],&ppsbuf[prevgps*WCNT_PPS+PPS_CTP],2*(WCNT_PPS-PPS_CTP));
  memcpy(&evtbuf[offset+EVT_CTRL],&sl[Reg_Dig_Control>>1],8*sizeof(uint16_t));
  memcpy(&evtbuf[offset+EVT_WINDOWS],&sl[Reg_Time1_Pre>>1],8*sizeof(uint16_t));
  memcpy(&evtbuf[offset+EVT_CHANNEL],&sl[Reg_ADC1_Gain>>1],4*6*sizeof(uint16_t));
  memcpy(&evtbuf[offset+EVT_TRIGGER],&sl[Reg_Trig1_ThresA>>1],4*6*sizeof(uint16_t));
  memcpy(&evtbuf[offset+EVT_FILTER1],&sl[Reg_FltA1_A1>>1],4*4*8*sizeof(uint16_t));
  //followed by ADC....
  if(Send_10 ==0){
    Send_10 = 1;
  }else{
    rate = *(short *)(&shadowlist[Reg_Rate]); // in principle the rate can change
    // take a 3 musec deadtime into account!
    t_next = 3+(int)(-1000000*log((double)(random())/(double)MAXRAND)/rate);
    //t_next = 2000000;
    tvFake.tv_usec+=t_next;
    while(tvFake.tv_usec >=1000000){
      tvFake.tv_usec -=1000000;
      tvFake.tv_sec+=1;
      scope_fake_gps();
      n_evt = 0;
      if((tvFake.tv_sec%10) == 0) {
        Send_10 = 0;
      }
    }
  }
  next_write +=ioff;
  if(next_write>=BUFSIZE) next_write = 0; // remember: circular buffer
  *(shm_ev.next_write) = next_write;
  if(t_next>30){
    if((tvFake.tv_sec%10) == 0 && Send_10 == 0) usleep(10);
    else{
      if(t_next<1030)usleep(t_next-30);
      else usleep(1000);
    }
  }
  return(SCOPE_EVENT);
}
#endif

/*!
 \func int scope_read_event(int32_t ioff)
 \brief read an event from the fpga
 \param ioff 1 to update the event buffer. When 0 no update is made
 \retval -10 too much data in the event
 \retval -11 not all data is read
 \retval SCOPE_EVENT succesfully read event
 */
int scope_read_event(int32_t ioff)
{
  int32_t rread,nread,ntry;
  struct tm tt;
  uint16_t length;
  int next_write = *(shm_ev.next_write);
  eventbuf[next_write].buf[0] = MSG_START;
  eventbuf[next_write].buf[1] = ID_PARAM_EVENT;
  //scope_raw_read(&(eventbuf[next_write].buf[2]),2);
  nread = 4; // length andA4 words
  length = *(unsigned short *)&(eventbuf[next_write].buf[2]);
  if(length>MAX_READOUT) return(-10); // too long
  ntry = 0;
  do{            //while absolutely needed as blocks are read out
    //rread = scope_raw_read(&(eventbuf[next_write].buf[nread]),length-nread);
    if(!rread) { usleep(10); ntry++; }
    else {ntry = 0; nread+=rread;}
  }while(nread <length &&ntry<MAXTRY);              // until the end or timeout
  if(nread < length) {
    printf("nread = %d length = %d rread = %d errno = %d\n",nread,length,rread,errno);
    return(-11); // an error if not all is read
  }
  eventbuf[next_write].evsize = (eventbuf[next_write].buf[EVENT_BCNT]+(eventbuf[next_write].buf[EVENT_BCNT+1]<<8)); //the total length in bytes from scope
  //scope_print_event(eventbuf[next_write].buf);
  scope_fill_ph(eventbuf[next_write].buf);
  tt.tm_sec = eventbuf[next_write].buf[EVENT_GPS+6];    // Convert GPS in a number of seconds
  tt.tm_min = eventbuf[next_write].buf[EVENT_GPS+5];
  tt.tm_hour = eventbuf[next_write].buf[EVENT_GPS+4];
  tt.tm_mday = eventbuf[next_write].buf[EVENT_GPS+3];
  tt.tm_mon = eventbuf[next_write].buf[EVENT_GPS+2]-1;
  tt.tm_year = *(short *)&(eventbuf[next_write].buf[EVENT_GPS]) - 1900;
  eventbuf[next_write].ts_seconds = (unsigned int)timegm(&tt);
  // Timestamp in Unix format
  // Convert UNIX time to GPS time in v3
  // NOTE: difftime() is apparently broken in this uclibc
  eventbuf[next_write].ts_seconds -= (unsigned int)GPS_EPOCH_UNIX;
  //eventbuf[next_write].ts_seconds -= leap_sec;
  // time in GPS epoch CT 20110630 fixed number!
  eventbuf[next_write].CTD = *(int *)&(eventbuf[next_write].buf[EVENT_CTD]);
  // fill the clock tick of the event
  eventbuf[next_write].t2_nanoseconds = (int)(1.E9*eventbuf[next_write].CTD/(*(uint32_t *)&gpsbuf[prevgps].data[PPS_CTP]));
  eventbuf[next_write].t3_nanoseconds = 0;  // the real time is not (yet) known
  eventbuf[next_write].t3calc = 0;          // and has not yet been calculated
  // and the trigger setting in gpsdata
  //
  if(gpsbuf[prevgps].data[PPS_CTP] != 0 &&
     eventbuf[next_write].t2_nanoseconds != -1) {
    next_write +=ioff;   // update the buffer counter if needed
  }
  if(next_write>=BUFSIZE) next_write = 0;       // remember: eventbuf is a circular buffer
  *(shm_ev.next_write) = next_write;
  return(SCOPE_EVENT);                  // success!
}



/*!
 \func int32_t scope_read_pps()
 \brief read pps, convert timestamp to GPS time, update circular GPS buffer
 \retval -7 error in reading the PPS
 \retval SCOPE_GPS OK
 */
int32_t scope_read_pps()  //27/7/2012 ok
{
  
  //# if defined(CALFIRST)
  //if(*(short *)&(gpsbuf[evgps].buf[PPS_RATE]) == 0)   scope_print_pps(gpsbuf[evgps].buf);
  //#endif
  //ct 20140928 scope_fill_shadow(gpsbuf[evgps].buf);         // fill all the shadow config. lists
  /*  tt.tm_sec = gpsbuf[evgps].buf[PPS_TIME+6];    // convert GPS into a number of seconds
   tt.tm_min = gpsbuf[evgps].buf[PPS_TIME+5];
   tt.tm_hour = gpsbuf[evgps].buf[PPS_TIME+4];
   tt.tm_mday = gpsbuf[evgps].buf[PPS_TIME+3];
   tt.tm_mon = gpsbuf[evgps].buf[PPS_TIME+2]-1;
   tt.tm_year = *(short *)(&gpsbuf[evgps].buf[PPS_TIME])-1900;
   gpsbuf[evgps].ts_seconds = (unsigned int)timegm(&tt);
   if(setsystime == 0){
   tp.tv_sec = gpsbuf[evgps].ts_seconds;
   settimeofday(&tp,NULL);
   setsystime = 1;
   }
   // Timestamp in Unix format
   // Convert UNIX time to GPS time in v3
   // NOTE: difftime() is apparently broken in this uclibc
   gpsbuf[evgps].ts_seconds -= (unsigned int)GPS_EPOCH_UNIX;*/
  //gpsbuf[evgps].ts_seconds -= leap_sec;
  //printf("PPS Time stamp = %d (%d)\n",gpsbuf[evgps].ts_seconds,GPS_EPOCH_UNIX);
  // time in GPS epoch CT 20110630 FIXED Number
  
  return(SCOPE_GPS);
}


/*!
 \func int scope_read(int ioff)
 \brief reads data from a scope in the appropriate buffer.
 \param ioff after reading the pointers to the buffers
 (event/monitor) are increased by ioff
 \retval  0 No Data
 \retval        -1 First word not a header
 \retval        -2 Only start-of-message read
 \retval        -3 Bad data identifier
 \retval -4 Error reading scope parameters
 \retval -5 Cannot read event header
 \retval -6 Error reading ADC values
 \retval -7 Error reading GPS parameters
 \retval SCOPE_PARAM successfully read scope parameters
 \retval SCOPE_EVENT successfully read scope event
 \retval SCOPE_GPS succesfully read GPS PPS info
 */

int scope_read(int ioff)
{
  unsigned short int totlen;
  int rread,nread,ntry;
  int ir;
  unsigned char rawbuf[5]={0,0,0,0,0};
  uint32_t Is_Data;
  
#ifdef Fake
  return(scope_fake_event(ioff));
#else
  scope_raw_read(Reg_Data,&Is_Data);
  if((Is_Data&(GENSTAT_PPSFIFO>>16)) == 0){
    scope_raw_write(Reg_GenControl,0);
    scope_read_pps();
  }

  // move data in the shadowlist
/*  if(rawbuf[1]<PARAM_NUM_LIST){
    // move the parameters in the correct shadow list.
    // TODO: they should first be compared!
    //scope_raw_read((unsigned char *)(&rawbuf[3]),2);
    if(rawbuf[3] == MSG_END || rawbuf[4] == MSG_END) return(-4);
    totlen = (rawbuf[4]<<8)+rawbuf[3];
  }
  else if(rawbuf[1] == ID_PARAM_EVENT) return(scope_read_event(ioff));
  else if(rawbuf[1] == ID_PARAM_ERROR) return(scope_read_error());
  printf("ERROR Identifier = %x\n",rawbuf[1]);
  return(-3);                               // bad identifier read
 */
#endif
  return(0);
}

/*!
 \func int scope_no_run_read()
 \brief reads data from scope, meant to use if there is no run
 \retval scope_read(0)
 */
int scope_no_run_read()
{
  return scope_read(0);
}

/*!
 \func int scope_run_read()
 \brief reads data from scope during a run. If there has not been data for more than 11 sec, reset the scope and restart the run.
 \retval scope_read(1)
 */
int scope_run_read()
{
  int iret;
  int szerop = seczero;
  
  iret = scope_read(1);
  if((seczero !=szerop && seczero>11)) {
    //system("/sbin/reboot");
    scope_reset();
    //scope_stop_run(); // stop run before flushing
    //scope_flush(); //flush BEFORE starting the output
#ifdef DEBUG_SCOPE
    printf("scope_run_read: re-Starting the run\n");
#endif
    //scope_close();
    //sleep(1);
    //scope_open();
    scope_copy_shadow(); //make sure that the settings are ok again */
    scope_start_run();
    seczero = 0;
  }
  return iret;
}

/*!
 \func int scope_cal_read()
 \brief reads data from scope, meant to use during calibration
 \retval scope_read(0)
 */
int scope_cal_read()
{
  return scope_read(0);
}

/*!
 \func int scope_calc_evnsec()
 \brief calculates exact timing for all events recorded in the
 appropriate second
 \param ibuf index of event for which to calculate the time
 \retval 1 ok
 \retval       -1 error
 */
int scope_calc_evnsec() //probably no longer needed
{
  
  return(1);
}

/*!
 \func int scope_calc_t3nsec(int ibuf)
 \brief calculates exact timing for event to be sent to DAQ
 \param ibuf index of event for which to calculate the time
 \retval 1 ok
 \retval -1 error
 */
int scope_calc_t3nsec(EV_DATA *t3_buffer)
{
  return(1);
}

/*!
 \func  void scope_calibrate()
 \brief runs the scope offset and gain calibration ( a minirun by itself)
 Note: the data is only printed and not (yet) saved!
 */
void scope_calibrate()
{
  scope_initialize_calibration();           // set up the scope parameters
  while(cal_type != CAL_END){               // continue until we are done
    if(scope_cal_read() == SCOPE_EVENT){    // only look at events
      scope_calibrate_evt();
      // make changes in offset (first) and gain (second)
    }
  }
  scope_stop_run();                         // stop the run
}


/*!
 \func  void scope_initialize_calibration()
 \brief initialize the digitizer to run the calibration
 */
void scope_initialize_calibration()
{
  
}


/*!
 \func  int scope_calibrate_evt()
 \brief Manipulates offset to get the baseline close to 0,
 and gain to get the full scale close to CAL_GAIN_TARG
 \retval CAL_OFFSET - offset calibration is going on
 \retval CAL_GAIN   - moved up to gain calibration
 \retval CAL_END    - we are done!
 */
int scope_calibrate_evt()
{
  
  return (cal_type);
}

//
