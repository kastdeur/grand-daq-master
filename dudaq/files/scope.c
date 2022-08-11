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
// #include "du_monitor.h"
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

void scope_raw_write(uint32_t reg_addr, uint32_t value)
{
  *((unsigned int *)(axi_ptr+page_offset+reg_addr)) = value;
}

uint32_t scope_raw_read(uint32_t reg_addr) 
{
  return *((unsigned int *)(axi_ptr+page_offset+reg_addr));
}

void scope_flush()
{
  if(axi_ptr == NULL) return;
  scope_raw_write(Reg_GenControl,0x08000000); // clear DAQ Fifo's
  scope_raw_write(Reg_GenControl,0x00000000);
}

int scope_open()  
{
  unsigned int addr, page_addr;
  unsigned int page_size=sysconf(_SC_PAGESIZE);

#ifdef Fake
  return(1);
#endif
  if(dev != 0) close(dev); 
  axi_ptr = NULL;
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
  return(1);
}

void scope_close() 
{
#ifndef Fake
  close(dev);
  dev = 0;
  axi_ptr = NULL;
#endif
}

void scope_reset()
{
  if(axi_ptr == NULL) return;
  scope_raw_write(Reg_Dig_Control,0x00004000); // reset firmware, but this
  scope_raw_write(Reg_Dig_Control,0x00000000); // does not clear registers
}

void scope_start_run()
{
  if(axi_ptr == NULL) return;
  scope_flush();
  scope_set_parameters(Reg_Dig_Control,shadowlist[Reg_Dig_Control>>2] |(CTRL_PPS_EN | CTRL_SEND_EN ),1);
}

void scope_stop_run()
{
  printf("Scope stop run\n");
  if(axi_ptr == NULL) return; 
  scope_set_parameters(Reg_Dig_Control,shadowlist[Reg_Dig_Control>>2] & (~CTRL_PPS_EN & ~CTRL_SEND_EN ),1);
  scope_flush();
}

void scope_set_parameters(uint32_t reg_addr, uint32_t value,uint32_t to_shadow)
{
  if(to_shadow == 1) shadowlist[reg_addr>>2] = value;
  if(axi_ptr == NULL) return;
  scope_raw_write(reg_addr,value);
  usleep(1000);
}

void scope_reboot()
{
  scope_reset();                // for now the same as a reset
}

void scope_copy_shadow()
{
  for(int i=0;i<Reg_End;i+=4){
    scope_set_parameters(i,shadowlist[i>>2],0);
  }
}

void scope_init_shadow()
{
  int32_t list;
  
  shadow_filled = 0;
  memset(shadowlist,0,sizeof(shadowlist));
}

void scope_initialize() 
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

int scope_t2()
{
  return(1);
}

int scope_read_event(int32_t ioff)
{
  static uint16_t evtnr=0;
  int offset = ptr_evt*evtlen;
  int32_t rread,nread,ntry;
  uint32_t Is_Data,tbuf,*ebuf;
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
    ebuf = (uint32_t *)&evtbuf[offset];
    *ebuf++ = tbuf;
    for(i=2;i<HEADER_EVT;i+=2){
      *ebuf++ = scope_raw_read(Reg_Data);
    }
    length -=HEADER_EVT;
    while(length>=0){
      *ebuf++ = scope_raw_read(Reg_Data);
      length-=2;
    }
    Is_Data = scope_raw_read(Reg_GenStatus);
    evtbuf[offset+EVT_HDRLEN] = HEADER_EVT;
    sec = (uint32_t *)&evtbuf[offset+EVT_SECOND];
    tt.tm_sec = (evtbuf[offset+EVT_STATSEC]&0xff)-evtbuf[offset+EVT_LEAP];    // Convert GPS in a number of seconds
    tt.tm_min = (evtbuf[offset+EVT_MINHOUR]>>8)&0xff;
    tt.tm_hour = (evtbuf[offset+EVT_MINHOUR])&0xff;
    tt.tm_mday = (evtbuf[offset+EVT_DAYMONTH]>>8)&0xff;
    tt.tm_mon = (evtbuf[offset+EVT_DAYMONTH]&0xff)-1;
    tt.tm_year = evtbuf[offset+EVT_YEAR] - 1900;
    *sec = (unsigned int)timegm(&tt);    
    fracsec = (double)(*(uint32_t *)&evtbuf[offset+EVT_CTD])/(double)(*(uint32_t *)&evtbuf[offset+EVT_CTP]);
    nanosec = (uint32_t *)&evtbuf[offset+EVT_NANOSEC];
    *nanosec = 1.E9*fracsec;
    evtbuf[offset+EVT_TRIGGERPOS] = shadowlist[Reg_Time1_Pre>>1]+shadowlist[Reg_Time_Common>>1];
    evtbuf[offset+EVT_ID] = evtnr++;
    evtbuf[offset+EVT_HARDWARE] = station_id;
    // evtbuf[offset+EVT_ATM_TEMP] = shm_mon.Ubuf[MON_TEMP];
    // evtbuf[offset+EVT_ATM_PRES] = shm_mon.Ubuf[MON_PRESSURE];
    // evtbuf[offset+EVT_ATM_HUM] = shm_mon.Ubuf[MON_HUMIDITY];
    // evtbuf[offset+EVT_ACCEL_X] = shm_mon.Ubuf[MON_AccX];
    // evtbuf[offset+EVT_ACCEL_Y] = shm_mon.Ubuf[MON_AccY];
    // evtbuf[offset+EVT_ACCEL_Z] = shm_mon.Ubuf[MON_AccZ];
    // evtbuf[offset+EVT_BATTERY] = shm_mon.Ubuf[MON_BATTERY];
    timestampbuf[next_write].ts_seconds = *sec;
    timestampbuf[next_write].ts_nanoseconds = *nanosec;
    timestampbuf[next_write].event_nr = evtbuf[offset+EVT_ID];
    timestampbuf[next_write].trigmask = evtbuf[offset+EVT_TRIG_PAT];
    if(scope_t2() == 1) next_write+=ioff;
    if(next_write >=BUFSIZE) next_write = 0;
    *shm_ts.next_write = next_write;
    ptr_evt +=ioff;
    if(ptr_evt>=BUFSIZE) ptr_evt = 0; // remember: circular buffer
    return(SCOPE_EVENT);                  // success!
  } else{ //flushing, but why???
    if(length<0) length = 10000;
    while(length>0 && ((Is_Data&GENSTAT_EVTFIFO) == 0)){
      tbuf = scope_raw_read(Reg_Data);
      length--;
      Is_Data = scope_raw_read(Reg_GenStatus);
      }
    return(-1);
  }
}



int32_t scope_read_pps()  
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
  printf("PPS %d %d %d %d\n",evgps,ppsbuf[offset+PPS_TRIG_RATE] ,ppsbuf[offset+PPS_MINHOUR],ppsbuf[offset+PPS_STATSEC]);
  prevgps = evgps;
  evgps++;
  if(evgps>=GPSSIZE)evgps = 0;
  
  return(SCOPE_GPS);
}


int scope_read(int ioff)
{
  unsigned short int totlen;
  int rread,nread,ntry;
  int ir;
  uint32_t Is_Data;
  
#ifdef Fake
  return(scope_fake_event(ioff));
#endif
  Is_Data = scope_raw_read(Reg_GenStatus);
  if((Is_Data&(GENSTAT_PPSFIFO)) == 0){
    scope_read_pps();
  }
  scope_raw_write(Reg_GenControl,GENCTRL_EVTREAD);
  Is_Data = scope_raw_read(Reg_GenStatus);
  if((Is_Data&(GENSTAT_EVTFIFO)) == 0){
    scope_read_event(ioff);
  }
  return(0);
}

int scope_no_run_read()
{
  return scope_read(0);
}

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
