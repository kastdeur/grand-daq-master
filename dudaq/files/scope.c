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
#include "du_monitor.h"
#include "scope.h"


#ifdef Fake
#define MAXRAND 0x7FFFFFFF
#endif
#define DEVFILE "/dev/mem" //!< Device for talking to the FPGA
#define DEV int32_t //!< the type of the device id is really just a 32 bit integer

DEV dev = 0;                    //!< Device id
void *axi_ptr;
uint32_t page_offset;

extern int station_id;
extern shm_struct shm_ev;
uint16_t *t3buf;
extern shm_struct shm_gps,shm_ts,shm_mon;
extern GPS_DATA *gpsbuf;
extern TS_DATA *timestampbuf;
uint16_t *evtbuf=NULL;
int ptr_evt=0;
uint16_t ppsbuf[WCNT_PPS*GPSSIZE];
int n_evt = 0;

int32_t evgps=0;                //!< pointer to next GPS info
int32_t prevgps = 0;

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

#define scope_raw_read(reg_addr) *((unsigned int *)(axi_ptr+page_offset+reg_addr))
/*int32_t scope_raw_read(uint32_t reg_addr, uint32_t *value) //new, reading from AXI
{
  *value = *((unsigned int *)(axi_ptr+page_offset+reg_addr));
  return(1);
  }*/
/*!
 \func void scope_flush)
 \brief empty routine
 */
void scope_flush()
{
  if(axi_ptr == NULL) return;
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

  if(dev != 0) close(dev); 
  axi_ptr = NULL;
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
  dev = 0;
  axi_ptr = NULL;
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
  if(axi_ptr == NULL) return;
  scope_raw_write(Reg_Dig_Control,0x00004000); // reset firmware, but this
  scope_raw_write(Reg_Dig_Control,0x00000000); // does not clear registers
}

/*!
 \func void scope_start_run()
 \brief starts the run
 */
void scope_start_run()
{
  if(axi_ptr == NULL) return;
  scope_flush();
  scope_set_parameters(Reg_Dig_Control,shadowlist[Reg_Dig_Control>>2] |(CTRL_PPS_EN | CTRL_SEND_EN ),1);
}

/*!
 \func  void scope_stop_run()
 \brief disables output
 */
void scope_stop_run()
{
  printf("Scope stop run\n");
  if(axi_ptr == NULL) return; 
  scope_set_parameters(Reg_Dig_Control,shadowlist[Reg_Dig_Control>>2] & (~CTRL_PPS_EN & ~CTRL_SEND_EN ),1);
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
  if(axi_ptr == NULL) return;
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
    //printf("Set Param %x %x\n",i,shadowlist[i>>2]);
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
  scope_reset();    // reset the scope
  scope_init_shadow();
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
  ptr_evt = 0;
  t3buf = (uint16_t *)shm_ev.Ubuf; //T3 requests will be in here 
  *shm_ts.next_read = 0;
  *shm_ts.next_write = 0;
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
  //printf("PPS %d %d %d %d\n",evgps,ppsbuf[offset+PPS_TRIG_RATE] ,ppsbuf[offset+PPS_MINHOUR],ppsbuf[offset+PPS_STATSEC]);
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
  nanoseconds = 1000*tvFake.tv_usec;
  if(Send_10 == 0){
    nanoseconds = 100*(((double)(random())/(double)MAXRAND));
  }else
    nanoseconds += 1000*(((double)(random())/(double)MAXRAND)); //add random ns
  evtbuf[offset+EVT_LENGTH] = evtlen;
  evtbuf[offset+EVT_ID] = MAGIC_EVT;
  evtbuf[offset+EVT_HARDWARE] = station_id;
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
  static uint16_t evtnr=0;
  int offset = ptr_evt*evtlen;
  int32_t rread,nread,ntry;
  uint32_t Is_Data,tbuf,*ebuf;
  uint16_t *sbuf;
  struct tm tt;
  int length,i;
  double fracsec;
  uint32_t *sec,*nanosec;
  int next_write = *(shm_ts.next_write);

  if(axi_ptr == NULL) return(-1);
  if(evtbuf == NULL) return(-2);
  if(timestampbuf == NULL) return(-3);
  scope_raw_write(Reg_GenControl,GENCTRL_EVTREAD);
  Is_Data = scope_raw_read(Reg_GenStatus);
  if((Is_Data&(GENSTAT_EVTFIFO)) == 0){
    tbuf = scope_raw_read(Reg_Data);
  } else return(0);
  if((tbuf>>16) == 0xADC0 || evtbuf == NULL) length = (tbuf&0xffff);
  else length = -1;
  if(length>0 &&evtbuf != NULL){
    //printf("Offset = %d (%d %d)\n",offset,evtlen,length);
    ebuf = (uint32_t *)&evtbuf[offset];
    *ebuf++ = tbuf;
    for(i=2;i<HEADER_EVT;i+=2){
      *ebuf++ = scope_raw_read(Reg_Data);
    }
    //printf("%d %d\n",i,length);
    sbuf = (uint16_t *)ebuf;
    length -=HEADER_EVT;
    //printf("%d %d %d\n",i,length,(int)(sbuf-&evtbuf[offset]));
    while(length>=0){
      *ebuf++ = scope_raw_read(Reg_Data);
      //scope_raw_read(Reg_Data,&tbuf);
      //*sbuf++ = (uint16_t)((tbuf)&0xffff);
      //*sbuf++ = (uint16_t)((tbuf>>16)&0xffff);
      //usleep(1);
      length-=2;
      /*scope_raw_read(Reg_GenStatus,&Is_Data);
      if(length>0  && (Is_Data&(GENSTAT_EVTFIFO)) != 0){
	printf("Not enough data %d!\n",length);
	break;
	}*/
    }
    Is_Data = scope_raw_read(Reg_GenStatus);
    /*if((Is_Data&(GENSTAT_EVTFIFO)) == 0){
      printf("Still data left!\n");
      length = 100;
      while(length>0 && (Is_Data&(GENSTAT_EVTFIFO)) == 0){
	scope_raw_read(Reg_Data,&tbuf);
	//printf("%x\t",tbuf);
	scope_raw_read(Reg_GenStatus,&Is_Data);
	length--;
      }
      //printf("\n");
      }*/
    evtbuf[offset+EVT_HDRLEN] = HEADER_EVT;
    sec = (uint32_t *)&evtbuf[offset+EVT_SECOND];
    tt.tm_sec = (evtbuf[offset+EVT_STATSEC]&0xff)-evtbuf[offset+EVT_LEAP];    // Convert GPS in a number of seconds
    tt.tm_min = (evtbuf[offset+EVT_MINHOUR]>>8)&0xff;
    tt.tm_hour = (evtbuf[offset+EVT_MINHOUR])&0xff;
    tt.tm_mday = (evtbuf[offset+EVT_DAYMONTH]>>8)&0xff;
    tt.tm_mon = (evtbuf[offset+EVT_DAYMONTH]&0xff)-1;
    tt.tm_year = evtbuf[offset+EVT_YEAR] - 1900;
    //printf("Event timestamp %02d/%02d/%04d %2d:%2d:%2d\n",tt.tm_mday,tt.tm_mon+1,tt.tm_year+1900,tt.tm_hour,tt.tm_min,tt.tm_sec);
    *sec = (unsigned int)timegm(&tt);    
    fracsec = (double)(*(uint32_t *)&evtbuf[offset+EVT_CTD])/(double)(*(uint32_t *)&evtbuf[offset+EVT_CTP]);
    nanosec = (uint32_t *)&evtbuf[offset+EVT_NANOSEC];
    *nanosec = 1.E9*fracsec;
    evtbuf[offset+EVT_TRIGGERPOS] = shadowlist[Reg_Time1_Pre>>1]+shadowlist[Reg_Time_Common>>1];
    evtbuf[offset+EVT_ID] = evtnr++;
    evtbuf[offset+EVT_HARDWARE] = station_id;
    evtbuf[offset+EVT_ATM_TEMP] = shm_mon.Ubuf[MON_TEMP];
    evtbuf[offset+EVT_ATM_PRES] = shm_mon.Ubuf[MON_PRESSURE];
    evtbuf[offset+EVT_ATM_HUM] = shm_mon.Ubuf[MON_HUMIDITY];
    evtbuf[offset+EVT_ACCEL_X] = shm_mon.Ubuf[MON_AccX];
    evtbuf[offset+EVT_ACCEL_Y] = shm_mon.Ubuf[MON_AccY];
    evtbuf[offset+EVT_ACCEL_Z] = shm_mon.Ubuf[MON_AccZ];
    evtbuf[offset+EVT_BATTERY] = shm_mon.Ubuf[MON_BATTERY];
    //printf("Reading event %08x %d %u.%09d %g\n",tbuf,evtbuf[EVT_STATSEC]&0xff,*sec,*nanosec,fracsec);
    timestampbuf[next_write].ts_seconds = *sec;
    timestampbuf[next_write].ts_nanoseconds = *nanosec;
    timestampbuf[next_write].event_nr = evtbuf[offset+EVT_ID];
    timestampbuf[next_write].trigmask = evtbuf[offset+EVT_TRIG_PAT];
    next_write+=ioff;
    if(next_write >=BUFSIZE) next_write = 0;
    *shm_ts.next_write = next_write;
    ptr_evt +=ioff;
    if(ptr_evt>=BUFSIZE) ptr_evt = 0; // remember: circular buffer
    //printf("Next offset = %d\n",ptr_evt*evtlen);
    return(SCOPE_EVENT);                  // success!
  } else{ //flushing, but why???
    if(length<0) length = 10000;
    //printf("Flushing\n");
    while(length>0 && ((Is_Data&GENSTAT_EVTFIFO) == 0)){
      tbuf = scope_raw_read(Reg_Data);
      length--;
      Is_Data = scope_raw_read(Reg_GenStatus);
      }
    return(-1);
  }
}



/*!
 \func int32_t scope_read_pps()
 \brief read pps, convert timestamp to GPS time, update circular GPS buffer
 \retval -7 error in reading the PPS
 \retval SCOPE_GPS OK
 */
int32_t scope_read_pps()  //27/7/2012 ok
{
  int offset = evgps*WCNT_PPS;
  uint32_t Is_Data,tbuf,*pbuf,ctp;
  int length;
  
  if(axi_ptr == NULL) return(-1);
  Is_Data = scope_raw_read(Reg_GenStatus);
  scope_raw_write(Reg_GenControl,0);
  if((Is_Data&(GENSTAT_PPSFIFO)) == 0){
    tbuf = scope_raw_read(Reg_Data);
    if((tbuf>>16) != 0xFACE){
      length = 1000;
      while(length>0 && ((Is_Data&GENSTAT_PPSFIFO) == 0)){
	tbuf = scope_raw_read(Reg_Data);
	length--;
	Is_Data = scope_raw_read(Reg_GenStatus);
      }
      return(-1);
    }
  }
  pbuf = (uint32_t *)&ppsbuf[offset];
  *pbuf++ = tbuf;
  for(length = 1;length<WCNT_PPS;length++){
    *pbuf++ = scope_raw_read(Reg_Data);
  }
  ctp = *(uint32_t *)&ppsbuf[offset+PPS_CTP];
  ctp = ctp&0x7fffffff;
  //printf("PPS %d %u %02d:%02d:%02d-%d %g\n",evgps,ctp ,(ppsbuf[offset+PPS_MINHOUR])&0xff,(ppsbuf[offset+PPS_MINHOUR]>>8)&0xff,ppsbuf[offset+PPS_STATSEC]&0xff,
  // ppsbuf[offset+PPS_LEAP],*(float *)&ppsbuf[offset+PPS_OFFSET]);
  prevgps = evgps;
  evgps++;
  if(evgps>=GPSSIZE)evgps = 0;
  
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
  uint32_t Is_Data;
  
#ifdef Fake
  return(scope_fake_event(ioff));
#else
  Is_Data = scope_raw_read(Reg_GenStatus);
  if((Is_Data&(GENSTAT_PPSFIFO)) == 0){
    scope_read_pps();
  }
  scope_raw_write(Reg_GenControl,GENCTRL_EVTREAD);
  Is_Data = scope_raw_read(Reg_GenStatus);
  if((Is_Data&(GENSTAT_EVTFIFO)) == 0){
    scope_read_event(ioff);
  }
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
  
  iret = scope_read(1); //should be 1
  return iret;
}

void scope_event_to_shm(uint16_t evnr,uint16_t trflag, uint16_t sec,uint32_t ssec)
{
  int i;
  int offset = 0;
  uint32_t nanosec;
  int next_write = *(shm_ev.next_write);
  

  for(i=0;i<BUFSIZE;i++){
    nanosec = *(uint32_t *)&evtbuf[offset+EVT_NANOSEC];
    if(sec == (evtbuf[offset+EVT_SECOND]&0xff)){
      if(nanosec>>6 == ssec) {
	evtbuf[offset+EVT_ID]=evnr;
	evtbuf[offset+EVT_T3FLAG] = trflag;
	memcpy(&t3buf[next_write*evtlen],&evtbuf[offset],evtbuf[offset+EVT_LENGTH]*sizeof(uint16_t));
	printf("Found event %d Buffer-id %d Flag = %d\n",evnr,next_write,trflag);
	next_write++;
	if(next_write >= MAXT3) next_write = 0;
	*(shm_ev.next_write) = next_write;
	break;
      } 
    }
    offset += evtlen;
  }
}
