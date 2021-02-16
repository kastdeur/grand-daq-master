#include <stdio.h>
#include<string.h>
#include<stdlib.h>
#include<math.h>
#include "readerv4.h"

int *filehdr=NULL;
unsigned short *event=NULL;


int aera_read_file_header(FILE *fp)
{
  int i,return_code;
  int isize;
  
  if( !fread(&isize,INTSIZE,1,fp)) {
    printf("Cannot read the header length\n");
    return(0);                                                       //cannot read the header length
  }
  if(isize < FILE_HDR_ADDITIONAL){
    printf("The file header is too short, only %d integers\n",isize);
    return(0);                                                       //file header too short
  }
  if(filehdr != NULL) free((void *)filehdr);                         //in case we run several files
  filehdr = (int *)malloc(isize+INTSIZE);                            //allocate memory for the file header
  if(filehdr == NULL){
    printf("Cannot allocate enough memory to save the file header!\n");
    return(0);                                                       //cannot allocate memory for file header
  }
  filehdr[0] = isize;                                                //put the size into the header
  if((return_code = fread(&(filehdr[1]),1,isize,fp)) !=(isize)) {
    printf("Cannot read the full header (%d)\n",return_code);
    return(0);                                                       //cannot read the full header
  }
  return(1);
}

int aera_read_event(FILE *fp)
{
  int isize,return_code;
  
  if( !fread(&isize,INTSIZE,1,fp)) {
    printf("Cannot read the Event length\n");
    return(0);                                                       //cannot read the header length
  }
  if(event != NULL) {
    if(event[0] != isize) {
      free((void *)event);                                           //free and alloc only if needed
      event = (unsigned short *)malloc(isize+INTSIZE);                          //allocate memory for the event
    }
  }
  else{
      event = (unsigned short *)malloc(isize+INTSIZE);                          //allocate memory for the event
  }
  if(event == NULL){
    printf("Cannot allocate enough memory to save the event!\n");
    return(0);                                                       //cannot allocate memory for event
  }
  event[0] = isize&0xffff;                                                  //put the size into the event
  event[1] = isize>>16;
  if((return_code = fread(&(event[2]),1,isize,fp)) !=(isize)) {
    printf("Cannot read the full event (%d requested %d bytes) Error %d EOF %d\n",return_code,isize,ferror(fp),feof(fp));
    return(0);                                                       //cannot read the full event
  }
  return(1);
}

analyze_ls(char *fname)
{
  FILE *fp;
  int i,istat,ibox;
  EVENTBODY *evls;
  int ils ;                                                      //parameter indicating start of LS 
  int ev_end ;
  int firm;
  unsigned char *raw;
  unsigned short Sthres[4],Nthres[4],PMV[4];

  fp = fopen(fname,"r");
  if(fp == NULL) return;

  if(aera_read_file_header(fp) ){ //lets read events
    while(1){
      if(aera_read_event(fp)== 0){
	printf("Failed to read event %d from %d\n",i,filehdr[FILE_HDR_LAST_EVENT]);
	break;
      }
      ils = EVENT_LS;
      ev_end = ((int)(event[EVENT_HDR_LENGTH+1]<<16)+(int)(event[EVENT_HDR_LENGTH]))/SHORTSIZE;
      while(ils<ev_end){
	evls = (EVENTBODY *)(&event[ils]);
	raw = (unsigned char *)evls->info_ADCbuffer;
	istat = evls->LS_id&0xff;
	firm = *((int32_t *)&raw[PPS_GPS]);
	ibox = SERIAL_NUMBER(firm);
	for(i=0;i<4;i++){
	  Sthres[i] = *(unsigned short *)&raw[PPS_TRIG1+12*i];
	  Nthres[i] = *(unsigned short *)&raw[PPS_TRIG1+12*i+2];
	  PMV[i] = raw[PPS_CH1+12*i+8];
	  printf("Station %03d HW %03d CH%d Thres: %04x (%4d) %04x (%4d) HV %02x (%3d)\n",
		 istat,ibox,i+1,Sthres[i],Sthres[i],Nthres[i],Nthres[i],PMV[i],PMV[i]);
	}
	ils +=(evls->length);
      }
    }
  }
  if (fp != NULL) fclose(fp); // close the file

}


main(int argc, char **argv)
{
  analyze_ls(argv[1]);
}
