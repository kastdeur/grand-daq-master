/***
 Event Builder
 Version:1.0
 Date: 18/2/2020
 Author: Charles Timmermans, Nikhef/Radboud University
 
 Altering the code without explicit consent of the author is forbidden
 ***/

#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "Adaq.h"
#include "amsg.h"
#include "eb.h"
#include "scope.h"

extern char *configfile;
int ad_init_param(char *file);

/* next lines copied from scope.h */
#define FIRMWARE_VERSION(x) (10*((x>>20)&0xf)+((x>>16)&0xf))
#define FIRMWARE_SUBVERSION(x)   (10*((x>>12)&0xf)+((x>>9)&0x7))
#define SERIAL_NUMBER(x)    (100*((x>>8)&0x1)+10*((x>>4)&0xf)+((x>>0)&0xf))

#define NDU (5*MAXDU) // maximally 5 events for each DU in memory
#define EBTIMEOUT 5 // need to have at least 5 seconds of data before writing

int running = 0;

uint16_t DUbuffer[NDU][EVSIZE];
int i_DUbuffer = 0;

int eb_sub = 1; //file subnumber
int eb_event = 0;

FILEHDR eb_fhdr;
FILE *fpout = NULL,*fpten=NULL,*fpmon=NULL,*fpmb=NULL;

/**
 void eb_open(EVHDR *evhdr)
 
 opens different event streams
 writes the file headers
 */
void eb_open(EVHDR *evhdr)
{
  char fname[100];
  
  printf("Trying to open eventfiles\n");
  sprintf(fname,"%s/AD/ad%06d.f%04d",eb_dir,eb_run,eb_sub);
  fpout = fopen(fname,"r");
  while(fpout != NULL){
    eb_sub +=1;
    if(eb_sub>9999){
      eb_sub = 1;
      eb_run++;
    }
    sprintf(fname,"%s/AD/ad%06d.f%04d",eb_dir,eb_run,eb_sub);
    fpout = fopen(fname,"r");
  }
  fpout = fopen(fname,"w");
  sprintf(fname,"%s/TD/td%06d.f%04d",eb_dir,eb_run,eb_sub);
  fpten = fopen(fname,"w");
  sprintf(fname,"%s/MD/md%06d.f%04d",eb_dir,eb_run,eb_sub);
  fpmb = fopen(fname,"w");
  sprintf(fname,"%s/MON/MO%06d.f%04d",eb_dir,eb_run,eb_sub);
  fpmon = fopen(fname,"w");
  // next write file header
  eb_fhdr.length = sizeof(FILEHDR)-sizeof(int32_t);
  eb_fhdr.run_id = eb_run;
  eb_fhdr.run_mode = eb_run_mode;
  eb_fhdr.file_id = eb_sub;
  eb_fhdr.first_event_id = evhdr->event_id;
  eb_fhdr.first_event_time = evhdr->seconds;
  eb_fhdr.last_event_id = evhdr->event_id;
  eb_fhdr.last_event_time = evhdr->seconds;
  eb_fhdr.add1 = 0;
  eb_fhdr.add2 = 0;
  fwrite(&eb_fhdr,1,sizeof(FILEHDR),fpout);
  fwrite(&eb_fhdr,1,sizeof(FILEHDR),fpten);
  fwrite(&eb_fhdr,1,sizeof(FILEHDR),fpmb);
}

/**
 void eb_close()
 
 Closes the different file streams and run a monitoring program on the files
 */
void eb_close()
{
  char cmd[400];
  // first update file header
  rewind(fpout);
  fwrite(&eb_fhdr,1,sizeof(FILEHDR),fpout);
  fclose(fpout);
  rewind(fpten);
  fwrite(&eb_fhdr,1,sizeof(FILEHDR),fpten);
  fclose(fpten);
  rewind(fpmb);
  fwrite(&eb_fhdr,1,sizeof(FILEHDR),fpmb);
  fclose(fpmb);
  fclose(fpmon);
  fpout = NULL;
  fpten = NULL;
  fpmb = NULL;
  fpmon = NULL;
}

/**
 int eb_DUcompare(const void *a, const void *b)
 
 routine used for sorting
 a<b return 1
 a>b return -1
 --> reverse ordering
 */
int eb_DUcompare(const void *a, const void *b)
{ /* sorting in REVERSE order, easy removal of older data */
  uint16_t  *t1,*t2;
  uint32_t t1sec,t2sec;
  uint32_t t1nsec,t2nsec;
  t1 = (uint16_t *)(a);
  t2 = (uint16_t *)(b);
  if(t1[EVT_ID] < t2[EVT_ID]) {
    if((t1[EVT_ID]+1000) < t2[EVT_ID]) return(-1);
    else return(1);
  }
  if(t1[EVT_ID] > t2[EVT_ID]) {
    if(t1[EVT_ID] > (t2[EVT_ID]+1000)) return(1);
    else return(-1);
  }
  t1sec = *(uint32_t *)&t1[EVT_SECOND];
  t1nsec = *(uint32_t *)&t1[EVT_NANOSEC];
  t2sec = *(uint32_t *)&t2[EVT_SECOND];
  t2nsec = *(uint32_t *)&t2[EVT_NANOSEC];
  if(t1sec < t2sec) return(1);
  if(t1sec == t2sec){
    if(t1nsec < t2nsec) return(1);
    else if(t2nsec < t1nsec) return(-1);
    else return(0);
  }
  return(-1);
}

/**
 void eb_getui()
 
 get data from the user interface
 commands read are DU_STOP and DU_START; these will stop and start the EB as well
 */
void eb_getui()
{
  AMSG *msg;
  
  
  while((shm_cmd.Ubuf[(*shm_cmd.size)*(*shm_cmd.next_readb)]) !=  0){ // loop over the UI input
    if(((shm_cmd.Ubuf[(*shm_cmd.size)*(*shm_cmd.next_readb)]) &2) ==  2){ // loop over the UI input
      msg = (AMSG *)(&(shm_cmd.Ubuf[(*shm_cmd.size)*(*shm_cmd.next_readb)+1]));
      if(msg->tag == DU_STOP){
        running = 0;
        if(fpout != NULL) eb_close();
      }
      else if(msg->tag == DU_START){
        ad_init_param(configfile);
        printf("EB: Starting the run\n");
        running = 1;
        i_DUbuffer = 0; // get rid of old data
        eb_sub = 1;
      }
      shm_cmd.Ubuf[(*shm_cmd.size)*(*shm_cmd.next_readb)] &= ~2;
    }
    *shm_cmd.next_readb = (*shm_cmd.next_readb) + 1;
    if( *shm_cmd.next_readb >= *shm_cmd.nbuf) *shm_cmd.next_readb = 0;
  }
}

/**
 void eb_gett3i()
 get data from the T3 Maker
 Information is not used. We could remove this aspect!
 */
void eb_gett3(){
  AMSG *msg;
  T3BODY *T3info;
  int n_t3_du;
  
  while((shm_t3.Ubuf[(*shm_t3.size)*(*shm_t3.next_readb)])  !=  0){ // loop over the input
    if(((shm_t3.Ubuf[(*shm_t3.size)*(*shm_t3.next_readb)]) &2) ==  2){ // loop over the input
      msg = (AMSG *)(&(shm_t3.Ubuf[(*shm_t3.size)*(*shm_t3.next_readb)+1]));
      T3info = (T3BODY *)(&(msg->body[0])); //set the T3 info pointer
      n_t3_du = (msg->length-3)/T3STATIONSIZE; // msg == length+tag+eventnr+T3stations
      //printf("EB: T3 event = %d; NDU = %d \n",T3info->event_nr,n_t3_du);
      shm_t3.Ubuf[(*shm_t3.size)*(*shm_t3.next_readb)] &= ~2;
    }
    *shm_t3.next_readb = (*shm_t3.next_readb) + 1;
    if( *shm_t3.next_readb >= *shm_t3.nbuf) *shm_t3.next_readb = 0;
  }
}

/**
 void eb_getdata()
 
 interpret the data from the detector units
 copy events into a local buffer (DUbuffer)
 write monitoring informatie into an ASCII file (fpmon); also when there is no run!
 sort the event fragments in memory (latest first)
 */
void eb_getdata(){
  AMSG *msg;
  uint16_t *DUinfo;
  int inew=0;
  int firmware;
  
  while(((shm_eb.Ubuf[(*shm_eb.size)*(*shm_eb.next_read)]) &1) ==  1){ // loop over the input
    //printf("EB: Get Data %d %d\n",*shm_eb.next_read,shm_eb.Ubuf[(*shm_eb.size)*(*shm_eb.next_read)]);
    if(i_DUbuffer >= NDU) {
      printf("EB: Cannot accept more data\n");
      break;
    }
    inew = 1;
    msg = (AMSG *)(&(shm_eb.Ubuf[(*shm_eb.size)*(*shm_eb.next_read)+1]));
    //printf("EB getdata: loop over input. TAG = %d\n",msg->tag);
    if(msg->tag == DU_EVENT){
      DUinfo = (uint16_t *)msg->body;
      if(i_DUbuffer < NDU) memcpy((void *)&DUbuffer[i_DUbuffer],(void *)DUinfo,2*DUinfo[EVT_LENGTH]);
      if(running ==1) i_DUbuffer +=1;
    } else if(msg->tag == DU_MONITOR){
      if(fpmon != NULL){
        memcpy(&firmware,&msg->body[1],4);
        fprintf(fpmon,"%03d %03d %5d %d %4d %4d %4d %4d %4d %5.2f %5.2f %5.2f %d\n",DUPOS(msg->body[0]),
                SERIAL_NUMBER(firmware),
                1000*FIRMWARE_VERSION(firmware)+FIRMWARE_SUBVERSION(firmware),
                *(int *)&msg->body[3],msg->body[5],
                msg->body[6],msg->body[7],msg->body[8],msg->body[9],
                *(float *)&msg->body[10],
                *(float *)&msg->body[12],*(float *)&msg->body[14],msg->body[16]);
      }
    }
    shm_eb.Ubuf[(*shm_eb.size)*(*shm_eb.next_read)] = 0;
    *shm_eb.next_read = (*shm_eb.next_read) + 1;
    if( *shm_eb.next_read >= *shm_eb.nbuf) *shm_eb.next_read = 0;
  }
  if(i_DUbuffer > NDU) i_DUbuffer = NDU;
  if(i_DUbuffer>0 && inew == 1) {
    qsort(DUbuffer[0],i_DUbuffer,2*EVSIZE,eb_DUcompare);
  }
}

/**
 void eb_write_events()
 
 write events to disk
 
 check if there is any buffer in memory
 create an event header
 check how many segments should be added to the event
 open the data files when needed
 write the event in the proper event stream
 make a new event header
 */
void eb_write_events(){
  FILE *fp;
  EVHDR evhdr;
  uint16_t *DUinfo,*DUn;
  int i,ils,il_start;
  static int n_written=0;
  
  if(i_DUbuffer == 0) return; //no buffers in memory
  //printf("EB: A buffer in memory\n");
  DUinfo = (uint16_t *)DUbuffer[i_DUbuffer-1];
  il_start = i_DUbuffer-1;
  DUn = (uint16_t *)DUbuffer[0];
  if((*(uint32_t *)&DUn[EVT_SECOND]<=*(uint32_t *)&DUinfo[EVT_SECOND])  &&(i_DUbuffer < (0.8*NDU)))  return; //in case of a huge amount of data in 1 sec
  if(((*(uint32_t *)&DUn[EVT_SECOND]-*(uint32_t *)&DUinfo[EVT_SECOND])<EBTIMEOUT) &&(i_DUbuffer < (0.8*NDU)) ) return;
  evhdr.t3_id = DUinfo[EVT_ID];
  evhdr.DU_count = 1;
  evhdr.length = 40+2*DUinfo[EVT_LENGTH];
  evhdr.run_id = eb_run;
  evhdr.event_id = eb_event;
  evhdr.first_DU = DUinfo[EVT_HARDWARE];
  evhdr.seconds = *(uint32_t *)&DUinfo[EVT_SECOND];
  evhdr.nanosec = *(uint32_t *)&DUinfo[EVT_NANOSEC];
  evhdr.type = DUinfo[EVT_T3FLAG];
  evhdr.version=EVENTVERSION;
  //printf("Event Type %d Version %d\n",evhdr.type,evhdr.version);
  for(i=(i_DUbuffer-2);i>=0;i--){
    DUn = (uint16_t *)DUbuffer[i];
    if(DUn[EVT_ID] == evhdr.t3_id){
      evhdr.DU_count ++;
      evhdr.length += 2*DUn[EVT_LENGTH];
      evhdr.type |= DUn[EVT_T3FLAG];
    }else{
      //printf("EB: Found event %d with %d DU Length = %d (%d, %d %d)\n",evhdr.t3_id,evhdr.DU_count,evhdr.length,i_DUbuffer,DUn->GPSseconds,DUinfo->GPSseconds);
      eb_fhdr.last_event_id = evhdr.event_id;
      eb_fhdr.last_event_time = evhdr.seconds;
      if(fpout == NULL) {
        eb_open(&evhdr);
        n_written = 0;
      }
      if((evhdr.type &TRIGGER_T3_MINBIAS) != 0) // untriggered
        fp = fpten;
      else{
        if((evhdr.type& TRIGGER_T3_RANDOM) != 0) fp=fpmb; //single station triggered
        else fp = fpout;
      }
      fwrite(&evhdr,1,44,fp);
      for(ils=il_start;ils>(il_start-evhdr.DU_count);ils--){
        DUn = (uint16_t *)DUbuffer[ils];
        fwrite(DUn,1,2*DUn[EVT_LENGTH],fp);
        *(uint32_t *)&DUn[EVT_SECOND] = 0;
      }// that is it, start a new event
      n_written++;
      if(n_written >=eb_max_evts) eb_close();
      eb_event++;
      DUinfo = (uint16_t *)DUbuffer[i];
      DUn = (uint16_t *)DUbuffer[0];
      il_start = i;
      i_DUbuffer = i+1;
      if(((*(uint32_t *)&DUn[EVT_SECOND]-*(uint32_t *)&DUinfo[EVT_SECOND])<EBTIMEOUT) &&(i_DUbuffer < (0.8*NDU))) break;
      evhdr.t3_id = DUinfo[EVT_ID];
      evhdr.DU_count = 1;
      evhdr.length = 40+2*DUinfo[EVT_LENGTH];
      evhdr.run_id = eb_run;
      evhdr.event_id = eb_event;
      evhdr.first_DU = DUinfo[EVT_HARDWARE];
      evhdr.seconds = *(uint32_t *)&DUinfo[EVT_SECOND];
      evhdr.nanosec = *(uint32_t *)&DUinfo[EVT_NANOSEC];
      evhdr.type = DUinfo[EVT_T3FLAG];
      evhdr.version=EVENTVERSION;
    }
  }
}
/**
 void eb_main()
 
 main eventbuilder loop.
 Get data from the (graphical) user interface
 Get data from the T3Maker
 Get data from the DUs
 Write events to file
 */
void eb_main()
{
  printf("Starting EB\n");
  while(1) {
    eb_getui();
    eb_gett3();
    eb_getdata();
    if(running == 1) eb_write_events();
    usleep(1000);
  }
}
