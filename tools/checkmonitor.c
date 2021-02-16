#include <stdio.h>
#include<string.h>
#include<stdlib.h>
#include<sys/time.h>
#include<time.h>
#include<math.h>
#include <fftw3.h>
#include "readerv4.h"
#include "fftdata.h"

#define MAXSTATION 160

int isdutch[MAXSTATION]={
  0,1,0,1,1,0,0,1,1,0,
  0,1,0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0,0,0,
  0,0,0,1,1,1,0,0,0,0,
  0,0,1,1,0,0,0,0,1,0,
  0,0,0,0,0,0,0,1,1,1,
  0,0,0,0,0,0,0,0,1,1,
  0,0,0,0,0,0,0,0,0,0,
  0,0,1,1,1,0,0,1,0,0,
  0,1,1,1,0,0,0,0,0,1,
  1,1,1,0,0,0,0,0,0,1,
  1,1,1,0,0,0,0,0,0,0,
  0,1,1,1,1,1,0,0,0,0,
  0,0,0,0,1,1,1,0,0,0,
  0,0,0,1,1,1,0,0,0,0,
  1,1,0,0,0,0,1,0,0,0
};

typedef struct{
  int hw;
  int version;
  int second;
  int status;
  float temperature;
  float volt;
  float current;
  int ndata;
  int lowrate;
  int highrate;
  int avrate;
  int rates[4];
  float lowvoltage;
  float highvoltage;
  int nsecleap;
  int nten;
  int nad;
  int no_gps;
  float sigNS,sigEW;
  double fmagNS[2048],fmagEW[2048];
}MONSTAT;

MONSTAT station[MAXSTATION];

int *filehdr=NULL;
unsigned short *event=NULL;
int nten,nad;
int inifft = 0;
int fftlen = 0;

void analyze_monitor(char *fname)
{
  FILE *fp;
  char line[200];
  int ir;
  int stat,hw,vers,rate,crate[4],status;
  unsigned int sec;
  float temp,volt,cur;
  
  fp=fopen(fname,"r");
  if(fp == NULL) return;
  while(line == fgets(line,199,fp)){
    ir = sscanf(line,"%d %d %d %d %d %d %d %d %d %g %g %g %d",&stat,&hw,&vers,&sec,
                &rate,&crate[0],&crate[1],&crate[2],&crate[3],&temp,&volt,&cur,&status);
    stat -=1;
    if(station[stat].hw != 0){
      if(hw != station[stat].hw) printf("Changed hardware to %d station %d at time %d\n",hw,stat,sec);
      if(vers != station[stat].version) printf("Updated firmware to %d station %d at time %d\n",vers,stat,sec);
      if(sec != station[stat].second+1) station[stat].nsecleap++;
    }
    if(rate < station[stat].lowrate || station[stat].lowrate == 0) station[stat].lowrate = rate;
    if(rate > station[stat].highrate ) station[stat].highrate = rate;
    station[stat].avrate +=rate;
    for(ir=0;ir<4;ir++) station[stat].rates[ir] += crate[ir];
    if((station[stat].lowvoltage > volt || station[stat].lowvoltage == 0) && volt > 0.1) station[stat].lowvoltage = volt;
    if(station[stat].highvoltage < volt) station[stat].highvoltage = volt;
    station[stat].hw = hw;
    station[stat].version = vers;
    station[stat].second = sec;
    station[stat].temperature = temp;
    station[stat].volt += volt;
    station[stat].current = cur;
    station[stat].status = status;
    if(!(status&0x1)|| sec<1000000000) station[stat].no_gps++;
    station[stat].ndata++;
  }
  fclose(fp);
}

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

void analyze_aera_ls(EVENTBODY *ls)
{
  unsigned char *raw;
  int i,iof;
  unsigned short len;
  int istat;
  short value;
  float mean,meansq;
  float fin[2048][2],fout[2048][2];
  
  raw = (unsigned char *)ls->info_ADCbuffer;
  istat = ls->LS_id&0xff;
  istat-=1;
  station[istat].nten++;
  iof = EVENT_ADC;
  len = *(unsigned short *)&raw[EVENT_LENCH1];
  
  if(inifft == 0) {
    if(len < 2048) fftlen = len;
    else fftlen = 2048;
    fft_init(fftlen); // assumption all lengths are the same
    inifft = 1;
  }
  mean = 0;
  meansq = 0;
  for(i=0;i<len;i++){ //ch1
    value = (*( short *)&raw[iof+2*i]);
    if(i<fftlen){
      fin[i][0] = value;
      fin[i][1] = 0;
    }
    //if(istat == 82) printf("%d ",value);
    mean+=value;
    meansq+=(value*value);
  }
  fft_forward(fin,fout);
  for(i=0;i<fftlen;i++) station[istat].fmagNS[i] +=sqrt(fout[i][0]*fout[i][0]+fout[i][1]*fout[i][1]);
  //if(istat == 82) printf("\n");
  mean = mean/len;
  meansq = meansq/len;
  //if(istat == 82) printf("Station 83: NS Mean %g, Meansq %g\n",mean,meansq);
  station[istat].sigNS += sqrt(meansq-mean*mean);
  
  iof += 2*(*(unsigned short *)&raw[EVENT_LENCH1]);
  len = *(unsigned short *)&raw[EVENT_LENCH2];
  mean = 0;
  meansq = 0;
  for(i=0;i<len;i++){ //ch2
    value = (*( short *)&raw[iof+2*i]);
    if(i<fftlen){
      fin[i][0] = value;
      fin[i][1] = 0;
    }
    mean+=value;
    meansq+=(value*value);
  }
  fft_forward(fin,fout);
  for(i=0;i<fftlen;i++) station[istat].fmagEW[i] +=sqrt(fout[i][0]*fout[i][0]+fout[i][1]*fout[i][1]);
  mean = mean/len;
  meansq = meansq/len;
  //if(istat == 82) printf("Station 83: EW Mean %g, Meansq %g\n",mean,meansq);
  station[istat].sigEW += sqrt(meansq-mean*mean);
}

void analyze_aera_event()
{
  EVENTBODY *evls;
  int ils = EVENT_LS;                                                      //parameter indicating start of LS
  int ev_end = ((int)(event[EVENT_HDR_LENGTH+1]<<16)+(int)(event[EVENT_HDR_LENGTH]))/SHORTSIZE;
  int evnr=(event[EVENT_HDR_EVENTNR+1]<<16)+event[EVENT_HDR_EVENTNR];
  
  while(ils<ev_end){
    evls = (EVENTBODY *)(&event[ils]);
    analyze_aera_ls(evls);
    ils +=(evls->length);
  }
}

void analyze_10s(char *fname)
{
  FILE *fp;
  int i;
  
  fp = fopen(fname,"r");
  if(fp == NULL) return;
  
  if(aera_read_file_header(fp) ){ //lets read events
    while(1){
      nten++;
      if(aera_read_event(fp)== 0){
        printf("Failed to read event %d from %d\n",i,filehdr[FILE_HDR_LAST_EVENT]);
        break;
      }
      analyze_aera_event();
    }
  }
  if (fp != NULL) fclose(fp); // close the file
  
}

void analyze_ad(char *fname)
{
  FILE *fp;
  int i,istat;
  EVENTBODY *evls;
  int ils ;                                                      //parameter indicating start of LS
  int ev_end ;
  
  
  
  fp = fopen(fname,"r");
  if(fp == NULL) return;
  
  if(aera_read_file_header(fp) ){ //lets read events
    while(1){
      nad++;
      if(aera_read_event(fp)== 0){
        printf("Failed to read event %d from %d\n",i,filehdr[FILE_HDR_LAST_EVENT]);
        break;
      }
      ils = EVENT_LS;
      ev_end = ((int)(event[EVENT_HDR_LENGTH+1]<<16)+(int)(event[EVENT_HDR_LENGTH]))/SHORTSIZE;
      while(ils<ev_end){
        evls = (EVENTBODY *)(&event[ils]);
        istat = evls->LS_id&0xff;
        istat-=1;
        station[istat].nad++;
        ils +=(evls->length);
      }
    }
  }
  if (fp != NULL) fclose(fp); // close the file
  
}

int ncompare(const void *a, const void *b)
{
  float *n1,*n2;
  n1 = (float *)a;
  n2 = (float *)b;
  if(*n1 < *n2) return(-1);
  if(*n1 > *n2) return(1);
  return(0);
}


void print_logmessage(char *logfile,char *actionfile)
{
  int ndatamax;
  int stat;
  int i,j,k;
  double bigsize[4],bigfreq[4];
  double oldfftNS[2048],newfftNS[2048];
  double oldfftEW[2048],newfftEW[2048];
  float NOISEew[MAXSTATION],NOISEns[MAXSTATION];
  float MedNoiseEW[2],MedNoiseNS[2];
  int nold,nnew,nnoise;
  FILE *fpl,*fpa;
  struct timeval tp;
  
  gettimeofday(&tp,NULL);
  fpl = fopen(logfile,"w");
  fpa = fopen(actionfile,"w");
  if(fpl == NULL || fpa == NULL)return;
  fprintf(fpa,"Time of check: %s \n",ctime(&tp.tv_sec));
  ndatamax = 0;
  for(stat=0;stat<MAXSTATION;stat++){
    if(station[stat].ndata>ndatamax) ndatamax = station[stat].ndata;
  }
  // median noise value
  nnoise = 0;
  for(stat=0;stat<25;stat++){
    if(station[stat].nten>0) {
      NOISEew[nnoise] = station[stat].sigEW/station[stat].nten;
      NOISEns[nnoise] = station[stat].sigNS/station[stat].nten;
      nnoise++;
    }
  }
  qsort(NOISEew,nnoise,sizeof(float),ncompare);
  MedNoiseEW[0] = NOISEew[nnoise/2];
  qsort(NOISEns,nnoise,sizeof(float),ncompare);
  MedNoiseNS[0] = NOISEns[nnoise/2];
  nnoise = 0;
  for(stat=25;stat<MAXSTATION;stat++){
    if(station[stat].nten>0) {
      NOISEew[nnoise] = station[stat].sigEW/station[stat].nten;
      NOISEns[nnoise] = station[stat].sigNS/station[stat].nten;
      nnoise++;
    }
  }
  qsort(NOISEew,nnoise,sizeof(float),ncompare);
  MedNoiseEW[1] = NOISEew[nnoise/2];
  qsort(NOISEns,nnoise,sizeof(float),ncompare);
  MedNoiseNS[1] = NOISEns[nnoise/2];
  nold = 0;
  nnew = 0;
  for(i=0;i<2048;i++){
    oldfftNS[i] = 0;
    newfftNS[i] = 0;
    oldfftEW[i] = 0;
    newfftEW[i] = 0;
  }
  for(stat=0;stat<MAXSTATION;stat++){
    if(isdutch[stat] == 0) continue;
    fprintf(fpl,"Station %d\n",stat+1);
    fprintf(fpl,"\t Rates: %d %5.3f %d %5.3f %5.3f %5.3f %5.3f\n",
            station[stat].lowrate,((float)station[stat].avrate)/station[stat].ndata,station[stat].highrate,
            ((float)station[stat].rates[0])/station[stat].ndata,
            ((float)station[stat].rates[1])/station[stat].ndata,
            ((float)station[stat].rates[2])/station[stat].ndata,
            ((float)station[stat].rates[3])/station[stat].ndata);
    if(station[stat].ndata == 0)fprintf(fpa,"Error: Station %d: No monitoring data\n",stat+1);
    fprintf(fpl,"\t Voltage %5.3f %5.3f %5.3f\n",station[stat].lowvoltage,station[stat].volt/station[stat].ndata,station[stat].highvoltage);
    if(station[stat].lowvoltage <11 && station[stat].ndata>0 && stat>24)
      fprintf(fpa,"Error: Station %d: Battery was low %g\n",stat+1,station[stat].lowvoltage);
    if(station[stat].highvoltage >15)
      fprintf(fpa,"Error: Station %d: Battery was high %g\n",stat+1,station[stat].highvoltage);
    fprintf(fpl,"\t Second jumps %4d\n",station[stat].nsecleap);
    fprintf(fpl,"\t Missing GPS %4d\n",station[stat].no_gps);
    if(station[stat].no_gps != 0)
      fprintf(fpa,"Error: Station %d: No GPS in %g %% of the data\n",stat+1,
              100.*station[stat].no_gps/station[stat].ndata);
    fprintf(fpl,"\t Missing monitoring data %4d = %3d %%\n",ndatamax-station[stat].ndata,
            100-(int)(.5+(100.*station[stat].ndata)/ndatamax));
    fprintf(fpl,"\t Ntensec = %3d (%d %%) SigmaNS = %5.3f, SigmaEW = %5.3f \n",station[stat].nten,
            (int)(0.5+(100.*station[stat].nten)/nten),
            station[stat].sigNS/station[stat].nten,
            station[stat].sigEW/station[stat].nten);
    if(station[stat].nten == 0)
      fprintf(fpa,"Error: Station %d: No 10 second data\n",stat+1);
    else{
      if(stat<25 &&(station[stat].sigNS/station[stat].nten >1.3*MedNoiseNS[0] ||
                    station[stat].sigEW/station[stat].nten >1.3*MedNoiseEW[0]))
        fprintf(fpa,"\t Warning: Station %d: Too much noise (NS=%g EW=%g) reference (NS=%g,EW=%g)\n",stat+1,
                station[stat].sigNS/station[stat].nten,station[stat].sigEW/station[stat].nten
                ,MedNoiseNS[0],MedNoiseEW[0]);
      if(stat<25 &&(station[stat].sigNS/station[stat].nten <.5*MedNoiseNS[0] ||
                    station[stat].sigEW/station[stat].nten <.5*MedNoiseEW[0]))
        fprintf(fpa,"\t Warning: Station %d: Very low noise (NS=%g EW=%g) reference (NS=%g,EW=%g)\n",stat+1,
                station[stat].sigNS/station[stat].nten,station[stat].sigEW/station[stat].nten
                ,MedNoiseNS[0],MedNoiseEW[0]);
      if(stat>25 &&(station[stat].sigNS/station[stat].nten >1.3*MedNoiseNS[1] ||
                    station[stat].sigEW/station[stat].nten >1.3*MedNoiseEW[1]))
        fprintf(fpa,"\t Warning: Station %d: Too much noise (NS=%g EW=%g) reference (NS=%g,EW=%g)\n",stat+1,
                station[stat].sigNS/station[stat].nten,station[stat].sigEW/station[stat].nten
                ,MedNoiseNS[1],MedNoiseEW[1]);
      if(stat>25 &&(station[stat].sigNS/station[stat].nten <.5*MedNoiseNS[1] ||
                    station[stat].sigEW/station[stat].nten <.5*MedNoiseEW[1]))
        fprintf(fpa,"\t Warning: Station %d: Very low noise (NS=%g EW=%g) reference (NS=%g,EW=%g)\n",stat+1,
                station[stat].sigNS/station[stat].nten,station[stat].sigEW/station[stat].nten
                ,MedNoiseNS[0],MedNoiseEW[0]);
    }
    for(i=0;i<4;i++) bigsize[i] = 0;
    for(i=1;i<fftlen/2;i++) {
      if(station[stat].fmagNS[i]>station[stat].fmagNS[i-1] &&
         station[stat].fmagNS[i]>station[stat].fmagNS[i+1]){ // search the peaks
        for(j=0;j<4;j++){
          if(station[stat].fmagNS[i]>bigsize[j]){
            for(k=3;k>j;k--){
              bigsize[k] = bigsize[k-1];
              bigfreq[k] = bigfreq[k-1];
            }
            bigsize[j] = station[stat].fmagNS[i];
            bigfreq[j] = (i+0.5)*(200./fftlen);
            break;
          }
        }
      }
    }
    fprintf(fpl,"\t NS noise: ");
    for(i=0;i<4;i++) fprintf(fpl,"F = %g Mag = %g ",bigfreq[i],bigsize[i]/station[stat].nten);
    fprintf(fpl,"\n");
    for(i=0;i<4;i++) bigsize[i] = 0;
    for(i=1;i<fftlen/2;i++) {
      if(station[stat].fmagEW[i]>station[stat].fmagEW[i-1] &&
         station[stat].fmagEW[i]>station[stat].fmagEW[i+1]){ // search the peaks
        for(j=0;j<4;j++){
          if(station[stat].fmagEW[i]>bigsize[j]){
            for(k=3;k>j;k--){
              bigsize[k] = bigsize[k-1];
              bigfreq[k] = bigfreq[k-1];
            }
            bigsize[j] = station[stat].fmagEW[i];
            bigfreq[j] = (i+0.5)*(200./fftlen);
            break;
          }
        }
      }
    }
    fprintf(fpl,"\t EW noise: ");
    for(i=0;i<4;i++) fprintf(fpl,"F = %g Mag = %g ",bigfreq[i],bigsize[i]/station[stat].nten);
    fprintf(fpl,"\n");
    fprintf(fpl,"\t Event fraction (%d %d) = %d %%\n",station[stat].nad,nad,(int)(0.5+(100.*station[stat].nad)/nad));
    if(station[stat].nad == 0)fprintf(fpa,"Error: Station %d: No Event data\n",stat+1);
    if(stat<25) nold+=station[stat].nten;
    else nnew += station[stat].nten;
    for(i=0;i<fftlen/2;i++){
      if(stat < 25){
        oldfftEW[i] += station[stat].fmagEW[i];
        oldfftNS[i] += station[stat].fmagNS[i];
      }else{
        newfftEW[i] += station[stat].fmagEW[i];
        newfftNS[i] += station[stat].fmagNS[i];
      }
    }
  }
  for(i=0;i<4;i++) bigsize[i] = 0;
  for(i=1;i<fftlen/2;i++) {
    if(newfftNS[i]>newfftNS[i-1] &&
       newfftNS[i]>newfftNS[i+1]){ // search the peaks
      for(j=0;j<4;j++){
        if(newfftNS[i]>bigsize[j]){
          for(k=3;k>j;k--){
            bigsize[k] = bigsize[k-1];
            bigfreq[k] = bigfreq[k-1];
          }
          bigsize[j] = newfftNS[i];
          bigfreq[j] = (i+0.5)*(200./fftlen);
          break;
        }
      }
    }
  }
  fprintf(fpl,"Stage 2 NS noise %d points:\n",fftlen/2);
  for(i=0;i<fftlen/2;i++) fprintf(fpl," %6.1f ",newfftNS[i]/nnew);
  fprintf(fpl,"\n");
  fprintf(fpl,"Stage 2 EW noise %d points:\n",fftlen/2);
  for(i=0;i<fftlen/2;i++) fprintf(fpl," %6.1f ",newfftEW[i]/nnew);
  fprintf(fpl,"\n");
  fprintf(fpl,"Stage 1 NS noise %d points:\n",fftlen/2);
  for(i=0;i<fftlen/2;i++) fprintf(fpl," %6.1f ",oldfftNS[i]/nold);
  fprintf(fpl,"\n");
  fprintf(fpl,"Stage 1 EW noise %d points:\n",fftlen/2);
  for(i=0;i<fftlen/2;i++) fprintf(fpl," %6.1f ",oldfftEW[i]/nold);
  fprintf(fpl,"\n");
  
  fprintf(fpl,"\n");
  if (fpl != NULL) fclose(fpl); // close the file
  if (fpa != NULL) fclose(fpa); // close the file
  
}

int main(int argc, char **argv)
{
  char cmd[200];
  memset(station,0,MAXSTATION*sizeof(MONSTAT));
  if(argc>1) analyze_monitor(argv[1]);
  nten = 0;
  if(argc>2) analyze_10s(argv[2]);
  nad = 0;
  if(argc>3) analyze_ad(argv[3]);
  print_logmessage(argv[4],argv[5]);
  sprintf(cmd,"cp %s /home/daq/DutchProblems",argv[5]);
  system(cmd);
}
