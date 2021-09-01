/// @file scope.c
/// @brief routines interfacing to the fpga
/// @author C. Timmermans, Nikhef/RU

/************************************
File: scope.c
Author: C. Timmermans
        J. Kelley
        T. Wijnen
Version 3 adapted for AERA DAQ

This file handles the interface to the Nikhef digitizer,
moves the readout asap into ring-buffers.
It only works for scope V4, and is NOT backward compatible!

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
#endif
#include <string.h>
#include<errno.h>

#include "ad_shm.h"
#include "scope.h"


#ifdef Fake
#define MAXRAND 0x7FFFFFFF
#endif
#define DEVFILE "/dev/scope" //!< Device for talking to the FPGA
#define DEV int32_t //!< the type of the device id is really just a 32 bit integer

DEV dev = 0;                    //!< Device id

extern shm_struct shm_ev,shm_gps;
extern EV_DATA *eventbuf;      // buffer to hold the events
extern GPS_DATA *gpsbuf;

int32_t evgps=0;                //!< pointer to next GPS info
int32_t seczero=0;                //!< seczero keeps track of the number of seconds no data is read out

int16_t cal_type=CAL_END;       //!< what to calibrate (END = nothing)
int32_t firmware_version;       //!< version of the firmware of the scope

int leap_sec = 0;               //!< Number of leap seconds in UTC; read from GPS unit

#define MAXTRY 50               //!< maximal number of loops to complete reading from the FPGA
#define UPDATESEC 100           //!< time interval between succesive rate checks. Only used in dynamic monitoring of rate.
#ifdef STAGE1
#define THRESMAX 3000             // radio threshold
#define THRESMIN 400              // radio threshold
#else
#define THRESMAX 1000             //!< maximal radio threshold. Only used in dynamic monitoring of rate.
#define THRESMIN 20               //!< minimal radio threshold. Only used in dynamic monitoring of rate.
#endif
#define MINRRATE (100*UPDATESEC)       //!< minimal radio rate
#define MEANRRATE (200*UPDATESEC)      //!< mean radio rate
#define MAXRRATE (600*UPDATESEC)       //!< maximal radio rate
#define MINSRATE (40*UPDATESEC)        //!< minimal scintillator rate
#define MEANSRATE (50*UPDATESEC)       //!< mean scintillator rate
#define MAXSRATE (80*UPDATESEC)        //!< max scintillator rate
#define HVMAX 0xc0                     //!< maximum HV for PMT's. Only used in dynamic monitoring of rate

uint8_t shadowlist[PARAM_NUM_LIST][PARAM_LIST_MAXSIZE];  //!< all parameters to set in FPGA
uint8_t shadowlistR[PARAM_NUM_LIST][PARAM_LIST_MAXSIZE]; //!< all parameters read from FPGA
int32_t shadow_filled = 0;                               //!< the shadow list is not filled

int32_t tenrate[4]={0,0,0,0};  //!< rate of all channels, to be checked every "UPDATESEC" seconds
int32_t pheight[4]={0,0,0,0};  //!< summed pulseheight of all channels
int32_t n_events[4]={0,0,0,0}; //!< number of events contributing to summed pulse height

int16_t setsystime=0;          //!< check if system time is set
/*!
  \func void scope_write(unsigned char *buf, int len)
  \brief writes a buffer to the digitizer
  \param buf pointer to the data to send
  \param           len number of bytes to send
 */
void scope_write(uint8_t *buf, int32_t len) // ok 24/7/2012
{
  int32_t itot=0,i;
  
  i=0;
#ifndef Fake
  while (itot < len){
    if((i = write(dev,&buf[i],len-itot))< 1) {
#ifdef DEBUG_SCOPE
      perror("scope_write: Error writing to scope");
#endif
      break;
    }
    itot+=i;
#ifdef DEBUG_SCOPE
    if(itot != len)
      printf("scope_write: Cannot write all data %d at once. %d bytes written\n",len,i);
#endif
  }
#endif
}

/*!
  \func int scope_raw_read(unsigned char *bf, int size)
  \brief reads data from digitizer and stores it in a buffer
  \param bf pointer to location where data can be stored
  \param size number of bytes requested
  \retval number of bytes read
 */
int scope_raw_read(uint8_t *bf, int32_t size) //ok 24/7/2012
{
  int ir;
#ifndef Fake
  ir =read(dev, (void *)bf, size);
#ifdef DEBUG_SCOPE
  if(ir <0 && errno != EAGAIN)
    perror("scope_raw_read");
#endif
#else
    ir = 0;
    usleep(100);
#endif
  return(ir);
}
/*!
 \func void scope_flush()
 \brief empty routine
*/
void scope_flush() //ok 24/7/2012
{
  //fpurge(dev, (void *)bf, size));
}

/*!
  \func int scope_open()
  \brief opens connection to digitizer
  \retval -1 failure
  \retval         1 succes
 */
int scope_open()        // ok 24/7/2012
{
#ifndef Fake
  printf("Trying to open !%s!\n",DEVFILE);
  if ((dev = open(DEVFILE, O_RDWR)) == -1) {
    fprintf(stderr, "Error opening scope device file %s for read/write\n", DEVFILE);
    return(-1);
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
  if(close(dev)<0){
#ifdef DEBUG_SCOPE
    perror("scope_close:");
#endif
  }
#endif
}

/*!
  \func void scope_get_parameterlist(char list)
  \brief request scope parameters (not reading them!)
  \param list
 */
void scope_get_parameterlist(uint8_t list) 
{
  uint8_t buf[6]={MSG_START,list,0x06,0,MSG_END,MSG_END};
  scope_set_parameters((unsigned short *)buf,0);
}

/*!
  \func void scope_reset()
  \brief performs a soft reset on the scope
 */
void scope_reset()      //tested 24/7/2012
{
  uint8_t *buf = shadowlist[ID_PARAM_COMRES];

  buf[ID_COMRES_CMD] = 0x0;
  buf[ID_COMRES_RES] = 0x1;
  scope_set_parameters((unsigned short *)buf,0);
  scope_close();
  sleep(10);                     // requires long wait for FPGA_MSL
  scope_open();
  seczero = 0;
  printf("Reset is done\n");
}

/*!
 \func void scope_start_run()
 \brief starts the run
 */
void scope_start_run()  //tested 24/7/2012
{
  uint8_t *buf = shadowlist[ID_PARAM_CTRL];
  uint16_t ctrl;

  ctrl = ((buf[ID_CTRL_CTRLREG+1]&0xff)<<8)+(buf[ID_CTRL_CTRLREG]&0xff);
  ctrl |= (CTRL_PPS_EN | CTRL_SEND_EN );//| 0x8000);

  buf[ID_CTRL_CTRLREG] = (ctrl&0xff);
  buf[ID_CTRL_CTRLREG+1] = ((ctrl>>8)&0xff);
  scope_set_parameters((unsigned short *)buf,1);
  seczero = 0;
}

/*!
  \func  void scope_stop_run()
  \brief disables output
 */
void scope_stop_run()   //tested 24/7/2012
{
  uint8_t *buf = shadowlist[ID_PARAM_CTRL];
  uint16_t ctrl;
  buf[0] = 0x99; 
  buf[1] = 0x01; 
  buf[2] = 0x12;
  buf[3] = 0; 
  buf[4] = CTRL_PPS_EN ;
  buf[5] = 0;//x80;
  buf[6] = 0;
  buf[7] = 0;
  buf[8] = 0xf;
  buf[9] = 0;
  buf[10] = 0x64; 
  buf[11] = 0; 
  buf[12] = 0; 
  buf[13] = 0; 
  buf[14] = 0; 
  buf[15] = 0; 
  buf[16] = 0x66; 
  buf[17] = 0x66; 
  scope_set_parameters((unsigned short *)buf,1);
  seczero = 0;
}

/*!
  \func  scope_set_parameters(unsigned short int *data,int to_shadow)
  \brief writes a parameter list to the scope (and the shadowlist)
  \param data contains the parameter and its value
  \param to_shadow  If 1 writes to the shadowlist, otherwise do not.
 */
void scope_set_parameters(uint16_t *data, int to_shadow)
{
    int i;
#ifdef Fake
    data[0] &=0x0FFF; //remove the top byte (F) for fake data
#endif
    printf("Writing parameters:");
    for(i=0;i<data[1]/2;++i) printf(" %04x",data[i]);
    printf("\n");
    i=(data[0]>>8)&0xff;
    if(to_shadow == 1) printf("To Shadow: %d %d\n",i,data[1]);
    if(i<=PARAM_NUM_LIST && i> 0 && data[1]<=PARAM_LIST_MAXSIZE && data[1]>0){
        if(to_shadow == 1) memcpy(shadowlist[i],data,data[1]); //new CT 20140928
        scope_write((uint8_t *)data,data[1]);
        usleep(1000);
    } else{
        printf("ERROR IN SETTING PARAMETERS\n");
        for(i=0;i<data[1]/2;++i) printf(" %04x",data[i]);
        printf("\n");
    }
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
  uint16_t *lst;
  int i;

  //shadowlist[ID_PARAM_CTRL][5] |= 0x80;
  lst = (uint16_t *)shadowlist[ID_PARAM_CTRL];
  scope_set_parameters(lst,0);
  lst = (uint16_t *)shadowlist[ID_PARAM_WINDOWS];
  scope_set_parameters(lst,0);
  for(i=0;i<4;i++){
    lst = (uint16_t *)shadowlist[ID_PARAM_CH1+i];
    scope_set_parameters(lst,0);
  }
  for(i=0;i<4;i++){
    lst = (uint16_t *)shadowlist[ID_PARAM_TRIG1+i];
    scope_set_parameters(lst,0);
  }
  for(i=0;i<8;i++){
    lst = (uint16_t *)shadowlist[ID_PARAM_FILT11+i];
    scope_set_parameters(lst,0);
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
  for(list=0;list<PARAM_NUM_LIST;list++){
    shadowlist[list][0] = MSG_START;
    shadowlist[list][1] = list;
    if(list==ID_PARAM_GPS) {
      shadowlist[list][2] = ID_GPS_END+2;
      shadowlist[list][ID_GPS_END] = MSG_END;
      shadowlist[list][ID_GPS_END+1] = MSG_END;
    }
    if(list==ID_PARAM_CTRL) {
      shadowlist[list][2] = ID_CTRL_END+2;
      //shadowlist[list][5] = 0x80;
      shadowlist[list][ID_CTRL_CHENABLE] = 0xf;
      shadowlist[list][ID_CTRL_END] = MSG_END;
      shadowlist[list][ID_CTRL_END+1] = MSG_END;
    }
    if(list==ID_PARAM_WINDOWS) {
      shadowlist[list][2] = ID_WINDOWS_END+2;
      shadowlist[list][ID_WINDOWS_END] = MSG_END;
      shadowlist[list][ID_WINDOWS_END+1] = MSG_END;
    }
    if(list==ID_PARAM_COMRES) {
      shadowlist[list][2] = ID_COMRES_END+2;
      shadowlist[list][ID_COMRES_END] = MSG_END;
      shadowlist[list][ID_COMRES_END+1] = MSG_END;
    }
    if(list==ID_PARAM_SPI) {
      shadowlist[list][2] = ID_SPI_END+2;
      shadowlist[list][ID_SPI_END] = MSG_END;
      shadowlist[list][ID_SPI_END+1] = MSG_END;
    }
    if(list>=ID_PARAM_CH1 && list<=ID_PARAM_CH4) {
      shadowlist[list][2] = ID_CH_END+2;
      shadowlist[list][ID_CH_END] = MSG_END;
      shadowlist[list][ID_CH_END+1] = MSG_END;
    }
    if(list>=ID_PARAM_TRIG1 && list<=ID_PARAM_TRIG4) {
      shadowlist[list][2] = ID_TRIG_END+2;
      shadowlist[list][ID_TRIG_END] = MSG_END;
      shadowlist[list][ID_TRIG_END+1] = MSG_END;
    }
    if(list>=ID_PARAM_FILT11 && list<=ID_PARAM_FILT22) {
      shadowlist[list][2] = ID_FILT_END+2;
      shadowlist[list][ID_FILT_END] = MSG_END;
      shadowlist[list][ID_FILT_END+1] = MSG_END;
    }
  }
}

/*!
  \func void scope_initialize()
  \brief initializes shadow memory, resets the digitizer and stops the run
 */
void scope_initialize() //tested 24/7/2012
{
  scope_init_shadow();
  scope_reset();                                // reset the scope
  /*  printf("Starting run\n");
  scope_start_run();                            // enable the output
  while(1){
    scope_no_run_read();
    memcpy(&firmware_version,
           (const void *)&(gpsbuf[0].buf[PPS_GPS+ID_GPS_VERSION-4]),4);
    if(firmware_version!=0) break;
  }
  printf("Firmware = (%x) %d.%d Serial=%d\n",firmware_version,
         FIRMWARE_VERSION(firmware_version),
         FIRMWARE_SUBVERSION(firmware_version),
         SERIAL_NUMBER(firmware_version));
	 seczero = 0;**/
  scope_stop_run(); //disable the output
}

/*!
 \func void scope_print_pps(uint8_t *buf)
 \brief print all parameters available in a PPS message
 */
void scope_print_pps(uint8_t *buf) //ok 25/7/2012
{
  int32_t i;
  uint16_t *sb=(unsigned short *)buf;
  uint16_t *sc=(unsigned short *)buf;
  uint8_t f1, f2;
  uint32_t *ip;
  float *fp;

  printf("PPS record: 0x%x 0x%x\n",sb[0],sb[1]);
  sb = (unsigned short *)&buf[PPS_TIME];
  printf("  GPS: %02d-%02d-%d %02d:%02d:%02d ",
         buf[PPS_TIME+3],buf[PPS_TIME+2],*sb,
         buf[PPS_TIME+4],buf[PPS_TIME+5],buf[PPS_TIME+6]);
  printf("Status: 0x%02x ",buf[PPS_STATUS]);
  ip = (unsigned int *)&buf[PPS_CTP];           // ctp
  fp = (float *)&buf[PPS_QUANT];                // quantisation error
  sc = (unsigned short *)&buf[PPS_FLAGS];       // utc offset
  f1 = (unsigned char) buf[PPS_FLAGS+2];        // gps timing flag
  f2 = (unsigned char) buf[PPS_FLAGS+3];        // gps decoding flag
  sb = (unsigned short *)&buf[PPS_RATE];        // trigger rate
  printf("CTP %d QUANT %g\n",(*ip)&0x7fffffff,*fp);
  printf("  UTCoffset %7d GPStimingflag 0x%02x GPSdecodingflag 0x%02x Rate %d\n",
         *sc,f1,f2,*sb);
  printf("List00: ");
  for(i=PPS_GPS;i<PPS_CTRL;i++) printf(" 0x%02x",buf[i]);
  printf("\n");
  printf("List01: ");
  for(i=PPS_CTRL;i<PPS_WINDOWS;i++) printf(" 0x%02x",buf[i]);
  printf("\n");
  printf("List02: ");
  for(i=PPS_WINDOWS;i<PPS_CH1;i++) printf(" 0x%02x",buf[i]);
  printf("\n");
  printf("List08: ");
  for(i=PPS_CH1;i<PPS_CH2;i++) printf(" 0x%02x",buf[i]);
  printf("\n");
  printf("List09: ");
  for(i=PPS_CH2;i<PPS_CH3;i++) printf(" 0x%02x",buf[i]);
  printf("\n");
  printf("List0a: ");
  for(i=PPS_CH3;i<PPS_CH4;i++) printf(" 0x%02x",buf[i]);
  printf("\n");
  printf("List0b: ");
  for(i=PPS_CH4;i<PPS_TRIG1;i++) printf(" 0x%02x",buf[i]);
  printf("\n");
  printf("List0c: ");
  for(i=PPS_TRIG1;i<PPS_TRIG2;i++) printf(" 0x%02x",buf[i]);
  printf("\n");
  printf("List0d: ");
  for(i=PPS_TRIG2;i<PPS_TRIG3;i++) printf(" 0x%02x",buf[i]);
  printf("\n");
  printf("List0e: ");
  for(i=PPS_TRIG3;i<PPS_TRIG4;i++) printf(" 0x%02x",buf[i]);
  printf("\n");
  printf("List0f: ");
  for(i=PPS_TRIG4;i<PPS_FILT11;i++) printf(" 0x%02x",buf[i]);
  printf("\n");
  printf("List10: ");
  for(i=PPS_FILT11;i<PPS_FILT12;i++) printf(" 0x%02x",buf[i]);
  printf("\n");
  printf("List11: ");
  for(i=PPS_FILT12;i<PPS_FILT21;i++) printf(" 0x%02x",buf[i]);
  printf("\n");
  printf("List12: ");
  for(i=PPS_FILT21;i<PPS_FILT22;i++) printf(" 0x%02x",buf[i]);
  printf("\n");
  printf("List13: ");
  for(i=PPS_FILT22;i<PPS_FILT31;i++) printf(" 0x%02x",buf[i]);
  printf("\n");
  printf("List14: ");
  for(i=PPS_FILT31;i<PPS_FILT32;i++) printf(" 0x%02x",buf[i]);
  printf("\n");
  printf("List15: ");
  for(i=PPS_FILT32;i<PPS_FILT41;i++) printf(" 0x%02x",buf[i]);
  printf("\n");
  printf("List16: ");
  for(i=PPS_FILT41;i<PPS_FILT42;i++) printf(" 0x%02x",buf[i]);
  printf("\n");
  printf("List17: ");
  for(i=PPS_FILT42;i<PPS_END;i++) printf(" 0x%02x",buf[i]);
  printf("\n");
  sb = (unsigned short *)&buf[PPS_END];  
  printf("PPS End mark %x\n",*sb);
}

/*!
 \func void scope_print_event(uint8_t *buf)
 \brief print all information from an event read from the fpga
 */
void scope_print_event(uint8_t *buf)  //ok 26/7/2012
{
  int32_t i,istart,iend,iadc,len[4];
  uint16_t *sb=(uint16_t *)buf;

  printf("Event record: 0x%x 0x%x Trigger Mask 0x%04x\n",sb[0],sb[1],sb[2]);
  sb = (unsigned short *)&buf[EVENT_GPS];
  printf("  GPS: %02d-%02d-%d %02d:%02d:%02d ",buf[EVENT_GPS+3],buf[EVENT_GPS+2],*sb,
         buf[EVENT_GPS+4],buf[EVENT_GPS+5],buf[EVENT_GPS+6]);
  printf("Status 0x%02x CTD %d\n",buf[EVENT_STATUS],*(int *)&buf[EVENT_CTD]);
  printf("Readout length: ");
  for(i=0;i<4;i++) {
    printf("%d ",*(short *)&buf[EVENT_LENCH1+2*i]);
    len[i] = *(short *)&buf[EVENT_LENCH1+2*i];
  }
  printf("\n");
  printf("Trigger Levels: ");
  for(i=0;i<4;i++) printf("(%d,%d) ",*(short *)&buf[EVENT_THRES1CH1+2*i],
                          *(short *)&buf[EVENT_THRES2CH1+2*i]);
  printf("\n");
  printf("List01: ");
  for(i=EVENT_CTRL;i<EVENT_WINDOWS;i++) printf(" 0x%02x",buf[i]);
  printf("\n");
  printf("List02: ");
  for(i=EVENT_WINDOWS;i<EVENT_ADC;i++) printf(" 0x%02x",buf[i]);
  printf("\n");
  istart = EVENT_ADC;
  for(i=0;i<4;i++){
    iend = istart+2*len[i];
    printf("Channel %d:",i+1);
    for(iadc=istart;iadc<iend;iadc+=2) {
      if((iadc-istart)==(16*((iadc-istart)/16))) printf("\n");
      printf("%6d ",*(short *)&buf[iadc]);
    }
    printf("\n");
    istart = iend;
  }
  printf("End Marker 0x%4x (last adc 0x%04x)\n",
         *(unsigned short *)&buf[iend], *(unsigned short *)&buf[iend-2]);
}

/*!
 \func void scope_fill_ph(uint8_t *buf)
 \brief for each channel add to the summed pulse height, also add to the number of events
 */
void scope_fill_ph(uint8_t *buf)
{
  int32_t i,istart,iend,iadc,len[4];
  int16_t sb;
  int16_t vmax=-8100;
  int16_t vmin = 8100;

  for(i=0;i<4;i++) {
    len[i] = *(short *)&buf[EVENT_LENCH1+2*i];
  }
  istart = EVENT_ADC;
  for(i=0;i<4;i++){
    vmax = -8100;
    vmin = 8100;
    iend = istart+2*len[i];
    for(iadc=istart;iadc<iend;iadc+=2) {
      sb = *(short *)&buf[iadc];
      if(sb>vmax) vmax = sb;
      if(sb<vmin) vmin = sb;
    }
    // printf("Channel %d, Start %d End %d (%d %d)\n",i+1,istart,iend,vmax,vmin);
    if(vmax>vmin){
      pheight[i]+=(vmax-vmin);
      n_events[i] +=1;
    }
    istart = iend;
  }
}

#ifdef Fake
int scope_fake_event(int32_t ioff)
{
    struct timeval tv;
    static struct timeval tvFake;
    short rate;
    int t_next;
    static int Send_10 = 0;
    int next_write = *(shm_ev.next_write);

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
    eventbuf[next_write].buf[0] = MSG_START;
    eventbuf[next_write].buf[1] = ID_PARAM_EVENT;
    eventbuf[next_write].buf[EVENT_TRIGMASK] =0;
    eventbuf[next_write].ts_seconds = tvFake.tv_sec;
    eventbuf[next_write].t2_nanoseconds = 1000*tvFake.tv_usec;
    if(Send_10 == 0){
        eventbuf[next_write].buf[EVENT_TRIGMASK] |=0x20;
        eventbuf[next_write].t2_nanoseconds = 100*(((double)(random())/(double)MAXRAND));
    }else
        eventbuf[next_write].t2_nanoseconds += 1000*(((double)(random())/(double)MAXRAND)); //add random ns
    eventbuf[next_write].t3_nanoseconds =     eventbuf[next_write].t2_nanoseconds;
  eventbuf[next_write].t3calc =     1;
    eventbuf[next_write].CTD = eventbuf[next_write].t2_nanoseconds/5; //clock tick
    eventbuf[next_write].CTP = 200000000; //clock freq.
    eventbuf[next_write].quant1 = 0.;
    eventbuf[next_write].quant2 = 0.;
    eventbuf[next_write].sync = 0;
    eventbuf[next_write].evsize = 70; //only header for now (in bytes)
    next_write +=ioff;
    if(next_write>=BUFSIZE) next_write = 0; // remember: circular buffer
    *(shm_ev.next_write) = next_write;
    if(Send_10 ==0){
        Send_10 = 1;
    }else{
        rate = *(short *)(&shadowlist[2][4]); // in principle the rate can change
        // take a 3 musec deadtime into account!
        t_next = 3+(int)(-1000000*log((double)(random())/(double)MAXRAND)/rate);
        if(t_next<5) printf("Rate = %d t_next = %d (%g)\n",rate,t_next,t_next/1000000.);
        //t_next = 2000000;
        tvFake.tv_usec+=t_next;
        while(tvFake.tv_usec >=1000000){
            tvFake.tv_usec -=1000000;
            tvFake.tv_sec+=1;
            if((tvFake.tv_sec%10) == 0) {
                Send_10 = 0;
            }
        }
    }
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
  int32_t prevgps;
  uint16_t length;
  int next_write = *(shm_ev.next_write);
  eventbuf[next_write].buf[0] = MSG_START;
  eventbuf[next_write].buf[1] = ID_PARAM_EVENT;
  scope_raw_read(&(eventbuf[next_write].buf[2]),2);
  nread = 4; // length andA4 words
  length = *(unsigned short *)&(eventbuf[next_write].buf[2]);
  if(length>MAX_READOUT) return(-10); // too long
  ntry = 0;
  do{            //while absolutely needed as blocks are read out        
    rread = scope_raw_read(&(eventbuf[next_write].buf[nread]),length-nread);
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
  prevgps = evgps-1;    // from the previous GPS get a first guess of the event time
  if(prevgps<0) prevgps = GPSSIZE-1;
  prevgps = prevgps-1;  // from the previous GPS get a first guess of the event time
  if(prevgps<0) prevgps = GPSSIZE-1;    //SHOULD IT BE TWICE (I think so)
  eventbuf[next_write].t2_nanoseconds = gpsbuf[prevgps].clock_tick*eventbuf[next_write].CTD;
  eventbuf[next_write].t3_nanoseconds = 0;  // the real time is not (yet) known
  eventbuf[next_write].t3calc = 0;          // and has not yet been calculated
  // and the trigger setting in gpsdata
  if((eventbuf[next_write].buf[EVENT_TRIGMASK+1]&0x1)!=0) gpsbuf[evgps].rate[0] ++;
  if((eventbuf[next_write].buf[EVENT_TRIGMASK+1]&0x2)!=0) gpsbuf[evgps].rate[1] ++;
  if((eventbuf[next_write].buf[EVENT_TRIGMASK+1]&0x4)!=0) gpsbuf[evgps].rate[2] ++;
  if((eventbuf[next_write].buf[EVENT_TRIGMASK+1]&0x8)!=0) gpsbuf[evgps].rate[3] ++;
  //
  if(gpsbuf[prevgps].clock_tick != 0 &&
     eventbuf[next_write].t2_nanoseconds != -1) {
    next_write +=ioff;   // update the buffer counter if needed
  }
  if(next_write>=BUFSIZE) next_write = 0;       // remember: eventbuf is a circular buffer
  *(shm_ev.next_write) = next_write;
  return(SCOPE_EVENT);                  // success!
}


/*!
 \func void scope_fill_shadow(int8_t *ppsbuf)
 \brief fills shadow list from PPS
 this routine is NOT used
 */
void scope_fill_shadow(int8_t *ppsbuf)
{
  shadow_filled = 1;
  if(ppsbuf[PPS_CH1] == 0) {
    printf("Error reading the PPS\n");
    //return;
    if(shadowlist[ID_PARAM_CH1][4] != 0) {
      //exit(-1);
      //scope_copy_shadow();
    }
    return;
  }
  memcpy(&(shadowlist[ID_PARAM_CTRL][4]),&ppsbuf[PPS_CTRL],ID_CTRL_END-4);
  //shadowlist[ID_PARAM_CTRL][5] |= 0x80;
  memcpy(&(shadowlist[ID_PARAM_WINDOWS][4]),&ppsbuf[PPS_WINDOWS],ID_WINDOWS_END-4);
  memcpy(&(shadowlist[ID_PARAM_CH1][4]),&ppsbuf[PPS_CH1],ID_CH_END-4);
  memcpy(&(shadowlist[ID_PARAM_CH2][4]),&ppsbuf[PPS_CH2],ID_CH_END-4);
  memcpy(&(shadowlist[ID_PARAM_CH3][4]),&ppsbuf[PPS_CH3],ID_CH_END-4);
  memcpy(&(shadowlist[ID_PARAM_CH4][4]),&ppsbuf[PPS_CH4],ID_CH_END-4);
  memcpy(&(shadowlist[ID_PARAM_TRIG1][4]),&ppsbuf[PPS_TRIG1],ID_TRIG_END-4);
  memcpy(&(shadowlist[ID_PARAM_TRIG2][4]),&ppsbuf[PPS_TRIG2],ID_TRIG_END-4);
  memcpy(&(shadowlist[ID_PARAM_TRIG3][4]),&ppsbuf[PPS_TRIG3],ID_TRIG_END-4);
  memcpy(&(shadowlist[ID_PARAM_TRIG4][4]),&ppsbuf[PPS_TRIG4],ID_TRIG_END-4);
  memcpy(&(shadowlist[ID_PARAM_FILT11][4]),&ppsbuf[PPS_FILT11],ID_FILT_END-4);
  memcpy(&(shadowlist[ID_PARAM_FILT12][4]),&ppsbuf[PPS_FILT12],ID_FILT_END-4);
  memcpy(&(shadowlist[ID_PARAM_FILT21][4]),&ppsbuf[PPS_FILT21],ID_FILT_END-4);
  memcpy(&(shadowlist[ID_PARAM_FILT22][4]),&ppsbuf[PPS_FILT22],ID_FILT_END-4);
  memcpy(&(shadowlist[ID_PARAM_FILT31][4]),&ppsbuf[PPS_FILT31],ID_FILT_END-4);
  memcpy(&(shadowlist[ID_PARAM_FILT32][4]),&ppsbuf[PPS_FILT32],ID_FILT_END-4);
  memcpy(&(shadowlist[ID_PARAM_FILT41][4]),&ppsbuf[PPS_FILT41],ID_FILT_END-4);
  memcpy(&(shadowlist[ID_PARAM_FILT42][4]),&ppsbuf[PPS_FILT42],ID_FILT_END-4);
}
/*!
 
 \func int8_t *scope_get_shadow(int32_t list)
 \param list parameter list number
 \brief obtain the address of the start of a parameter list
 \retval NULL on failure
 \retval address of the start of the parameter list
*/
 int8_t *scope_get_shadow(int32_t list)
{
  if(list>=0 && list < PARAM_NUM_LIST) return((int8_t *)shadowlist[list]);
  return(NULL);
}

/*!
 \func int scope_modify_threshold(short iad)
 \brief routine to modify the scope thresholds. This routine is NOT used
 \param iad positive to raise the threshold
 \param iad negative or 0 to lower the threshold
*/
int scope_modify_threshold(short iad)
{
  int32_t ich;
  uint16_t thr,i;
  static uint16_t thres[4];
  uint8_t *lst;
  for(ich=0;ich<4;ich++){
    lst = shadowlist[ID_PARAM_TRIG1+ich];
    thr = *(uint16_t *)&lst[ID_TRIG_THR1];
    if(thr != thres[ich]) thres[ich]=thr;
    if(iad> 0) thres[ich] += 5;
    else thres[ich] -=5;
    if(thres[ich]<THRESMIN) thres[ich] = THRESMIN;
    if(thres[ich]>THRESMAX) thres[ich] = THRESMAX;
    *(uint16_t *)&lst[ID_TRIG_THR1] = thres[ich];
    *(uint16_t *)&lst[ID_TRIG_THR2] = (int)((thres[ich]));
    scope_set_parameters((uint16_t *)lst,1);
  }
  return(1);
}

/*!
 \func int32_t scope_check_rates()
 \brief routine to autocheck the event rates, now dummy
 \retval 1
*/
int32_t scope_check_rates()
{
  static int32_t firstten = 1;
  int32_t ich; 
  uint8_t *lst;
  uint16_t thr;
  uint8_t hv;

  return(1);
  printf("Rates %d %d %d %d\n",tenrate[0],tenrate[1],tenrate[2],tenrate[3]);
  if(firstten == 1){
    firstten = 0;
    for(ich=0;ich<4;ich++) tenrate[ich] = 0;
    return(0);
  }
  for(ich=0;ich<2;ich++){
    lst = shadowlist[ID_PARAM_TRIG1+ich];
    thr = *(uint16_t *)&lst[ID_TRIG_THR1];
    printf("Check thres: %d 0x%x\n",tenrate[ich],shadowlist[ID_PARAM_CTRL][ID_CTRL_TRMASK+1]);
    if(tenrate[ich]<MINRRATE && thr>THRESMIN  && (shadowlist[ID_PARAM_CTRL][ID_CTRL_TRMASK+1]&(1<<ich)) != 0) {
      thr -=2;
      printf("Reduce threshold ch %d %d\n",ich,thr);
      *(uint16_t *)&lst[ID_TRIG_THR1] = thr;
      *(uint16_t *)&lst[ID_TRIG_THR2] = thr;
      scope_set_parameters((uint16_t *)lst,1);
    }
    if(tenrate[ich]>MAXRRATE && thr<THRESMAX && (shadowlist[ID_PARAM_CTRL][ID_CTRL_TRMASK+1]&(1<<ich)) != 0) {
      thr +=2;
      printf("Increase threshold ch %d %d\n",ich,thr);
      *(uint16_t *)&lst[ID_TRIG_THR1] = thr;
      *(uint16_t *)&lst[ID_TRIG_THR2] = thr;
      scope_set_parameters((uint16_t *)lst,1);
    }
  }/*
  for(ich=2;ich<4;ich++){
    lst = shadowlist[ID_PARAM_CH1+ich];
    hv = lst[ID_CH_PMV];
    if(tenrate[ich]<MINSRATE && hv<HVMAX && (shadowlist[ID_PARAM_CTRL][ID_CTRL_TRMASK+1]&(1<<ich)) != 0) {
      printf("Rate LOW Prev HV = %d ",hv);
      if((HVMAX-hv) < ((MEANSRATE-tenrate[ich])/20)) hv = HVMAX;
      else hv += ((MEANSRATE-tenrate[ich])/20);
      printf("New hv = %d\n",hv);
      lst[ID_CH_PMV] = hv;
      scope_set_parameters((uint16_t *)lst,1);
    }
    if(tenrate[ich]>MAXSRATE && hv>0 && (shadowlist[ID_PARAM_CTRL][ID_CTRL_TRMASK+1]&(1<<ich)) != 0) {
      printf("Rate HIGH Prev HV = %d ",hv);
      if(((tenrate[ich]-MEANSRATE)/20) > hv) hv = 0;
      else hv -= ((tenrate[ich]-MEANSRATE)/20);
      printf("New hv = %d\n",hv);
      lst[ID_CH_PMV] = hv;
      scope_set_parameters((uint16_t *)lst,1);
    }
    }*/
  for(ich=0;ich<4;ich++) tenrate[ich] = 0;
}

/*!
\func int32_t scope_read_pps()
 \brief read pps, convert timestamp to GPS time, update circular GPS buffer
 \retval -7 error in reading the PPS
 \retval SCOPE_GPS OK
*/
int32_t scope_read_pps()  //27/7/2012 ok
{
  int32_t rread,nread,ntry,i;
  struct tm tt;
  struct timeval tp;
  float *fp;
  unsigned short ppsrate;
  int32_t prevgps;

  nread = 2;                                    // again, already 2 bytes read!
  ntry = 0;
  gpsbuf[evgps].buf[0] = MSG_START;
  gpsbuf[evgps].buf[1] = ID_PARAM_PPS;
  gettimeofday(&tp,NULL);
  do{                                           // now read the remainder
    rread = scope_raw_read(&(gpsbuf[evgps].buf[nread]),PPS_LENGTH-nread);
    if(!rread) { usleep(10); ntry++; }
    else {ntry = 0;nread+=rread;}
  }while(nread <(PPS_LENGTH) &&ntry<MAXTRY);    // until the end or a timeout
  leap_sec = (int)(*(unsigned short *)&gpsbuf[evgps].buf[PPS_FLAGS]);
  printf("SCOPE_READ_PPS %ld; leap = %d; Status 0x%x; rate = %d Pheight: ",tp.tv_sec,leap_sec,gpsbuf[evgps].buf[PPS_STATUS],*(short *)&(gpsbuf[evgps].buf[PPS_RATE]));
  for(i=0;i<4;i++) {
    if(n_events[i]>0) printf("%d ",pheight[i]/n_events[i]);
    pheight[i] = 0;
    n_events[i] = 0;
  }
  printf("\n");
  if((*(short *)&(gpsbuf[evgps].buf[PPS_RATE])) == 0) {
    seczero ++; 
  } else{
    seczero = 0;
  }
  if(nread != PPS_LENGTH || gpsbuf[evgps].buf[nread-1] != MSG_END) {
    printf("Error in PPS %d %d %x\n",PPS_LENGTH,nread,gpsbuf[evgps].buf[nread-1]);
    for(i=0;i<nread;i++){
      if((i%8) == 0) printf("\n");
      printf("gpsbuf[%03d]=%02x\t",i,gpsbuf[evgps].buf[i] );
    }
    printf("\n");
    return(-7);                                 // GPS reading did not go smoothly
  }
  //# if defined(CALFIRST)
  //if(*(short *)&(gpsbuf[evgps].buf[PPS_RATE]) == 0)   scope_print_pps(gpsbuf[evgps].buf);
  //#endif
  //ct 20140928 scope_fill_shadow(gpsbuf[evgps].buf);         // fill all the shadow config. lists
  tt.tm_sec = gpsbuf[evgps].buf[PPS_TIME+6];    // convert GPS into a number of seconds
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
  gpsbuf[evgps].ts_seconds -= (unsigned int)GPS_EPOCH_UNIX;
  //gpsbuf[evgps].ts_seconds -= leap_sec;
  //printf("PPS Time stamp = %d (%d)\n",gpsbuf[evgps].ts_seconds,GPS_EPOCH_UNIX);
  // time in GPS epoch CT 20110630 FIXED Number
  gpsbuf[evgps].CTP = (*(int *)&gpsbuf[evgps].buf[PPS_CTP])&0x7fffffff; //ok 25/7/2012
  gpsbuf[evgps].sync =(gpsbuf[evgps].buf[PPS_CTP]>>7)&0x1;
  // for 2.5 ns accuracy, get the clock-edge
  gpsbuf[evgps].quant = *(float *)(&gpsbuf[evgps].buf[PPS_QUANT]); 
  prevgps = evgps-1;
  if(prevgps<0) prevgps = GPSSIZE-1;
  if((gpsbuf[evgps].ts_seconds -gpsbuf[prevgps].ts_seconds ) != 1){
    // can we calculate things accurately
    printf("ERROR I missed an C4 !!!!\n");
  }
  // length of clock-tick is (total time (ns))/(N clock ticks)
  gpsbuf[prevgps].clock_tick =  ((1000000000 -
    (gpsbuf[prevgps].quant-gpsbuf[evgps].quant)) / gpsbuf[prevgps].CTP);
  gpsbuf[prevgps].SCTP = gpsbuf[prevgps].CTP + 
    (gpsbuf[prevgps].quant-gpsbuf[evgps].quant)/gpsbuf[prevgps].clock_tick;
    // corrected number of clock ticks/second
  *(shm_gps.next_read) = evgps;
  if ((tt.tm_sec%UPDATESEC) == 0) scope_check_rates(); // check rates every 10 seconds
  ppsrate = *(unsigned short *)&(gpsbuf[evgps].buf[PPS_RATE]);
  for(i=0;i<2;i++) {    
    if(ppsrate< 400 || gpsbuf[evgps].rate[i]>(MEANRRATE/UPDATESEC))
      tenrate[i] += gpsbuf[evgps].rate[i];
    else
      tenrate[i] += (MEANRRATE/UPDATESEC); // do not know the real rate
  }
  for(i=2;i<4;i++) {    
    if(ppsrate< 400 || gpsbuf[evgps].rate[i]>(MEANSRATE/UPDATESEC))
      tenrate[i] += gpsbuf[evgps].rate[i];
    else
      tenrate[i] += (MEANSRATE/UPDATESEC); // do not know what to do      
  }
  evgps++;      // update the gpsbuf index, keeping in mind it is a circular buffer
  if(evgps>=GPSSIZE) evgps = 0; 
  *(shm_gps.next_write) = evgps;
  for(i=0;i<4;i++)
    gpsbuf[evgps].rate[i] = 0;
  return(SCOPE_GPS);
}

/*!
\func int scope_read_error()
 \brief read the FPGA error, and print this on screen
\retval 1
*/
int scope_read_error()
{
  // Code 0x99: did not get a 0x99
  // Code 0x89: got the 99 twice
  int16_t len;
  uint8_t buffer[ERROR_END+2];

  buffer[0]=MSG_START;
  buffer[1] =ID_PARAM_ERROR;
  scope_raw_read(&(buffer[2]),2);
  len = *(int16_t *)&(buffer[2]);
  if(len != (ERROR_END+2))
    printf("Scope_read Error length is incorrect 0x%x!\n",len);
  scope_raw_read(&(buffer[4]),ERROR_END-2);
  printf("Reading error: ");
  for(len=0;len<ERROR_END+2;len++)
    printf("0x%x ",buffer[len]);
  printf("\n");
  return(1);
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
    
#ifdef Fake
    return(scope_fake_event(ioff));
#else
  do{                           // flush scope until start-of-message
    nread = scope_raw_read(rawbuf,1);
  } while(rawbuf[0] != MSG_START && nread>0);
  
  if(nread == 0 || rawbuf[0] != MSG_START) {
    return(0);                              // no data (should never happen)
  }
  if(rawbuf[0] != MSG_START && nread>0){    // not a start of message
    printf("Not a message start %x\n",rawbuf[0]);
    return(-1);
  }
  ntry = 0;
  // do{                           // read the identifier of the message (data-type)
    nread = scope_raw_read(&(rawbuf[1]),1);
    if(!nread) {
      usleep(10);
      ntry++;
    }
    //}while(nread==0 &&ntry<MAXTRY);   // second word should come in within a short time
  if(nread ==0) {
    printf("Failed to read a second word\n");
    return(-2);                     // No identifier after start-of-message
  }
  if(rawbuf[1]<PARAM_NUM_LIST){
    // move the parameters in the correct shadow list.
    // TODO: they should first be compared!
    scope_raw_read((unsigned char *)(&rawbuf[3]),2);
    if(rawbuf[3] == MSG_END || rawbuf[4] == MSG_END) return(-4);
    totlen = (rawbuf[4]<<8)+rawbuf[3];
    printf("Length of param = %x\n",totlen);
    if(totlen>PARAM_LIST_MAXSIZE) return(-4);
    nread = 4;
    shadowlistR[rawbuf[1]][0] = MSG_START;
    shadowlistR[rawbuf[1]][1] = rawbuf[1];
    shadowlistR[rawbuf[1]][2] = totlen&0xff;
    shadowlistR[rawbuf[1]][3] = (totlen>>8)&0xff;
    //do{                             // read out the remaining parameters 
      rread = scope_raw_read(&(shadowlistR[rawbuf[1]][nread]),totlen-nread);
      if(!rread) { usleep(10); ntry++; }
      else {ntry = 0; nread+=rread;}
      //}while(nread <totlen &&ntry<MAXTRY);    // until the end or timeout
      //shadowlistR[ID_PARAM_CTRL][5] |= 0x80;
  } 
  else if(rawbuf[1] == ID_PARAM_PPS) {
    ir = scope_read_pps();
    //scope_calc_evnsec();
    return(ir);
  }
  else if(rawbuf[1] == ID_PARAM_EVENT) return(scope_read_event(ioff));
  else if(rawbuf[1] == ID_PARAM_ERROR) return(scope_read_error());
  printf("ERROR Identifier = %x\n",rawbuf[1]);
  return(-3);                               // bad identifier read
#endif
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
int scope_calc_evnsec()
{
  int i;
  int igpsbuf;
  int igps[3]={-1,-1,-1}; // 3 GPS stamps are needed due to delay in readout

  igpsbuf = evgps-1;
  if(igpsbuf<0) igpsbuf = GPSSIZE-1;
  igps[2] = igpsbuf;
  igps[1] = igpsbuf-1;
  if(igps[1] < 0) igps[1] = GPSSIZE-1;
  igps[0] = igps[1]-1;
  if(igps[0] < 0) igps[0] = GPSSIZE-1;
  
  for(i=0;i<BUFSIZE;i++){
      if(eventbuf[i].ts_seconds == gpsbuf[igps[0]].ts_seconds
         && eventbuf[i].t3calc == 0){
          // follow the manual for a calculation of the time
          eventbuf[i].t3_nanoseconds = 
              (int) (2.5*gpsbuf[igps[0]].sync
                     + gpsbuf[igps[1]].quant 
                     + eventbuf[i].CTD*gpsbuf[igps[1]].clock_tick);
          // igps[2] must exist in order to have the clock_tick!
          eventbuf[i].t3calc = 1;
          eventbuf[i].sync = gpsbuf[igps[0]].sync;
          eventbuf[i].quant1 = gpsbuf[igps[1]].quant;
          eventbuf[i].quant2 = gpsbuf[igps[2]].quant;
          eventbuf[i].CTP = gpsbuf[igps[1]].CTP;
      }else{
	if(eventbuf[i].ts_seconds < gpsbuf[igps[0]].ts_seconds
	   && eventbuf[i].t3calc == 0){
	  eventbuf[i].sync = 2;      // really an error, but we need to get out of the loop
	  eventbuf[i].t3calc = 1;    // really an error, but we need to get out of the loop
	}
      }
  }
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
  int i,iset;
  int n_sec;
  int igps[3]={-1,-1,-1}; // 3 GPS stamps are needed due to delay in readout

  iset = 0;
  for(i=0;i<GPSSIZE;i++){
    if(gpsbuf[i].ts_seconds == (t3_buffer->ts_seconds))igps[0] = i;
      // same seconds (contains previous GPS info)
    else if(gpsbuf[i].ts_seconds == (t3_buffer->ts_seconds+1))igps[1] = i;
      // sec+1        (contains current GPS info)
    else if(gpsbuf[i].ts_seconds == (t3_buffer->ts_seconds+2))igps[2] = i;
      // sec+2        (contains next GPS info)
    if(gpsbuf[i].ts_seconds >(t3_buffer->ts_seconds+2) ) iset = 1;
  }
  if(igps[0]<0 || igps[1]<0 || igps[2] <0 ) {
    if(iset == 1){
      t3_buffer->sync = 2;      // really an error, but we need to get out of the loop
      t3_buffer->t3calc = 1;    // really an error, but we need to get out of the loop
      printf("Cannot find all GPS info %d %d %d\n",igps[0],igps[1],igps[2]);
      return(-1);
    }else{
      return(0);
    }
  }
  t3_buffer->t3_nanoseconds = (int) (2.5*gpsbuf[igps[0]].sync   // follow the manual for a calculation of the time
          + gpsbuf[igps[1]].quant
          + (t3_buffer->CTD)*gpsbuf[igps[1]].clock_tick);       // igps[2] must exist in order to have the clock_tick!
  t3_buffer->t3calc = 1;
  t3_buffer->sync = gpsbuf[igps[0]].sync;
  t3_buffer->quant1 = gpsbuf[igps[1]].quant;
  t3_buffer->quant2 = gpsbuf[igps[2]].quant;
  t3_buffer->CTP = gpsbuf[igps[1]].CTP;
  if(evgps>igps[0])
    n_sec = evgps - igps[0];
  else
    n_sec = GPSSIZE-igps[0]+evgps;
  //printf("Nsec %09d %09d --> %d (%d %d %d)\n",t3_buffer[ibuf].t2_nanoseconds,                  // for the moment, print comparison to t2 timing
  //       t3_buffer[ibuf].t3_nanoseconds,
  //       (int)(t3_buffer[ibuf].t3_nanoseconds-t3_buffer[ibuf].t2_nanoseconds)
  //       ,n_sec,evgps,igps[0]);
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
  unsigned short ctrllist[9]=
    {0x0199,0x0012,CTRL_SEND_EN|CTRL_PPS_EN,0, 0x000f,0x50,0,0,0x6666};
  unsigned short winlist[11]=
    {0x0299,0x0016,0x0380,0x30,0x0380,0x30,0x0380,0x30,0x0380,0x30,0x6666};
  unsigned short chlist[9]=
    {0x0899,0x0012,0x4000,0x0080,0x2200,0x1E00,0x0,0x0,0x6666};
  int ic;

  scope_reset();
  printf("Init Shadow\n");
  scope_init_shadow();
  printf("Set channel list\n");
  for(ic=0;ic<4;ic++) {
    chlist[0]=0x0899+ic*0x100;
    scope_set_parameters(chlist,1);
  }
  printf("Set windows\n");
  scope_set_parameters(winlist,1);
  printf("Set control list\n");
  scope_set_parameters(ctrllist,1);
  shadow_filled = 0;
  while(shadow_filled == 0) {
    scope_cal_read();  // make sure that the shadow lists are up to date
  }
# ifdef CALFAST
  ctrllist[3] = TRIG_EXT;
  ctrllist[4] = 0xff0f;
# else
  ctrllist[3] = TRIG_CAL;
  ctrllist[4] = 0x000f;
# endif
  printf("Set control list\n");
  scope_set_parameters(ctrllist,0);
  printf("Set channel list\n");
  for(ic=0;ic<4;ic++) {
    chlist[0]=0x0899+ic*0x100;
    scope_set_parameters(chlist,0);
  }
  shadow_filled = 0;
  memcpy(&firmware_version,
	 (const void *)&(gpsbuf[0].buf[PPS_GPS+ID_GPS_VERSION-4]),4);
  printf("Scope = %d\n",SERIAL_NUMBER(firmware_version));
  cal_type = CAL_OFFSET;                // first calibrate the offset
}

/*!
\func void scope_showchannelproperties ()
 \brief print the data from the shadowlist
*/
void scope_showchannelproperties ()
{
  int i;
  printf(" Channel properties:\n");
  for(i=0; i<shadowlist[0+ID_PARAM_CH1][2]; ++i)
    printf(" %02x",shadowlist[0+ID_PARAM_CH1][i]);
  printf("\n");
  for(i=0; i<shadowlist[1+ID_PARAM_CH1][2]; ++i)
    printf(" %02x",shadowlist[1+ID_PARAM_CH1][i]);
  printf("\n");
  for(i=0; i<shadowlist[2+ID_PARAM_CH1][2]; ++i)
    printf(" %02x",shadowlist[2+ID_PARAM_CH1][i]);
  printf("\n");
  for(i=0; i<shadowlist[3+ID_PARAM_CH1][2]; ++i)
    printf(" %02x",shadowlist[3+ID_PARAM_CH1][i]);
  printf("\n");
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
  int ch;
  int iadc,nadc;
  int istart,iend;
  short length;
  int sumadc;
  int scope_changes;
  unsigned short gainfactor;
  int targ_off,targ_gain,targ_width;
  unsigned char *bf;
  unsigned short ctrllist[9]=
    {0x0199,0x12,CTRL_SEND_EN|CTRL_PPS_EN,TRIG_CAL,0x000f,0x0050,0,0,0x6666};
  int next_write = *(shm_ev.next_write);

# ifdef CALFAST
# else
  if(shadow_filled == 0) return(-1);
# endif
  printf("In calib. event\n");
  scope_changes = 0;
  istart = EVENT_ADC;
  for(ch=0;ch<4;ch++){
    length = *(short *)&eventbuf[next_write].buf[EVENT_LENCH1+2*ch];
    iend = istart+2*length;
    sumadc = 0;
    nadc = (iend-istart)/2;
    for(iadc = istart;iadc<iend;iadc+=2){
      sumadc+=(*(short *)&eventbuf[next_write].buf[iadc]);
    }
    sumadc/=nadc;
    if(cal_type == CAL_OFFSET){
      if(SERIAL_NUMBER(firmware_version)<80){
	targ_off = CAL_OFFSET_OTARG;
	targ_width = CAL_OFFSET_OWIDTH;
      } else{
	targ_off = CAL_OFFSET_TARG;
	targ_width = CAL_OFFSET_WIDTH;
      }
      if(sumadc>(targ_off+targ_width)
        || sumadc<(targ_off-targ_width)){
        bf =(unsigned char *)shadowlist[ch+ID_PARAM_CH1]; 
        if(sumadc>targ_off) bf[ID_CH_OFFSET] -=1; //this for old, same new?
        else bf[ID_CH_OFFSET] +=1;
	if(bf[ID_CH_OFFSET]<10) bf[ID_CH_OFFSET] = 0x80;
	if(bf[ID_CH_OFFSET]>245) bf[ID_CH_OFFSET] = 0x80;
	bf[ID_CH_GAIN]   = 0x00;
	bf[ID_CH_GAIN+1] = 0x40;
        scope_set_parameters((unsigned short *)bf,1);
        printf("Offset Channel %d = %d %d (%d)\n", ch+1,
               shadowlist[ch+ID_PARAM_CH1][ID_CH_OFFSET],
               bf[ID_CH_OFFSET],sumadc);
        scope_changes = 1;
        //shadow_filled = 0;
      }
    }
    if(cal_type == CAL_GAIN){
      if(SERIAL_NUMBER(firmware_version)<80){
	targ_gain = CAL_GAIN_OTARG;
	targ_width = CAL_GAIN_OWIDTH;
      } else{
	targ_gain = CAL_GAIN_TARG;
	targ_width = CAL_GAIN_WIDTH;
      }
      if(sumadc>(targ_gain+targ_width)
         || sumadc<(targ_gain-targ_width)){
        bf = (unsigned char *) shadowlist[ch+ID_PARAM_CH1]; 
        gainfactor = bf[ID_CH_GAIN] + (bf[ID_CH_GAIN+1]<<8);
	if(gainfactor< 0x2000) gainfactor = 0x2000; //reset
        if (sumadc<targ_gain ) {
          gainfactor -= 1;
        }
        else if (gainfactor < 0x6000) {
          gainfactor += 1;
        }
        else {
          continue; // do no more
        }
	bf[ID_CH_GAIN]   = gainfactor & 0xff;
	bf[ID_CH_GAIN+1] = (gainfactor>>8) & 0xff;
        scope_set_parameters((unsigned short *)bf,1);
        printf("Gain Channel %d = %d %d %d (%d)\n", ch+1,gainfactor,
               ((shadowlist[ch+ID_PARAM_CH1][ID_CH_GAIN+1]<<8)+
		shadowlist[ch+ID_PARAM_CH1][ID_CH_GAIN]),
               ((bf[ID_CH_GAIN+1]<<8)+bf[ID_CH_GAIN]),sumadc);
        scope_changes = 1;
	// shadow_filled = 0;
      }
    }
    istart=iend;
  }
  if(!scope_changes) {
    if(cal_type == CAL_OFFSET){
      cal_type = CAL_GAIN;
      ctrllist[2] = CTRL_SEND_EN | CTRL_PPS_EN | CTRL_FULLSCALE;
#     ifdef CALFAST
      ctrllist[3] = TRIG_EXT;
      ctrllist[4] = 0xff0f;
#     else
      ctrllist[3] = TRIG_CAL;
      ctrllist[4] = 0x000f;
#     endif
      scope_set_parameters(ctrllist,1);
      shadow_filled = 0;
    } else {
      cal_type = CAL_END;
      ctrllist[2] = CTRL_SEND_EN | CTRL_PPS_EN;
      ctrllist[3] = TRIG_10SEC;
      ctrllist[4] = 0x000f;
      scope_set_parameters(ctrllist,1);
      scope_showchannelproperties();
    }
  }
  return (cal_type);
}

//
