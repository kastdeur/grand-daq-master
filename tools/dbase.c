#include<stdlib.h>
#include<stdio.h>
#include<time.h>
#include<math.h>
#include "readerv4.h"
//#include "readerv4A.h"

#define INTSIZE   4 //size of an integer
#define SHORTSIZE 2 //size of a short

#define MAXAERA 160

float xstat[MAXAERA]={
  2500,2750,2375,2625,2875,2250,2500,2750,3000,2375,
  2625,2875,2500,2750,2375,2625,2875,2500,2750,2625,
  2500,2750,2735,2765,3000,2875,3000,2625,2875,1750,
  2000,2250,2500,2746.62,3002.47,3242.66,1375,1625,1875,2125,
  2375,2625,2876.28,3132.81,1250,1500,1750,2000,3247.31,375,
  625,875,1125,1375,1625,1875,2125,3118.81,3373.53,3621.53,
  500,750,1000,1250,1500,1750,2000,2250,3246.91,3494.91,
  397.375,621.375,877.375,1125,1375,1625,1875,2125,2375,2625,
  2875,3125,3376.34,3620.06,3872.78,4875,5267,4312.88,4688,5063,
  5438,3366.47,3756.47,4125.78,4500,4875,5267,5625,2813,3187.81,
  3567.19,3936.66,4313.75,4688,5063,5438,5813,2250,2625,2995.56,
  3372.62,3748.66,4137.47,4500,4875,5237,5625,1313,1688,2063,
  2438,2818,3194.84,3565.62,3939.88,4316.88,4688,5063,5438,5813,
  375,750,1125,1500,3372.34,3747.91,4125.16,4500,4875,5248,
  5625,563,938,3564.97,3935.12,4314.66,4688,5063,5438,5813,
  3750.88,4128.38,4500,4875,5244,5625,4310.5,4688,5063,5438
};
float ystat[MAXAERA]={
  794,794,722,722,722,650,650,650,650,577,
  577,577,515,516,433,433,433,361,361,289,
  217,217,624,624,361,289,217,145,145,1083,
  1083,1083,1083,1085.5,1090,1083.5,866,866,866,866,
  866,866,876.5,866,650,650,650,650,645.5,433,
  433,433,433,433,433,433,433,431.5,438.5,437,
  217,217,217,217,217,217,217,217,222.5,227,
  74,55,31,0,0,0,0,0,0,0,
  0,0,4,2,5,2923,2923,2608,2598,2598,
  2598,2280,2280,2274,2273,2273,2273,2273,1949,1952,
  1958,1961,1947.5,1949,1949,1949,1949,1624,1624,1624,
  1621,1622.5,1619,1624,1624,1624,1624,1299,1299,1299,
  1299,1297,1293.5,1299.5,1298,1292,1299,1299,1299,1299,
  974,974,974,974,981,976,986,974,974,974,
  974,650,650,651,654,645.5,650,650,650,650,
  328.5,333,325,325,325,325,5,0,0,0
};
float zstat[MAXAERA]={
  0,1500,0,1500,1500,0,0,1500,1500,0,
  0,1500,0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0,0,0,
  0,0,0,1564.69,1565.8,1566.3,0,0,0,0,
  0,0,1558.61,1564.16,0,0,0,0,1563.43,0,
  0,0,0,0,0,0,0,1560.49,1554.17,1548.42,
  0,0,0,0,0,0,0,0,1558.63,1551.78,
  0,0,0,0,0,0,0,0,0,0,
  0,0,1555.7,1549.94,1546.71,0,0,1554.93,0,0,
  0,1558.58,1559.17,1552.09,0,0,0,0,0,1563.62,
  1556.11,1552.32,1553.91,0,0,0,0,0,0,1565.33,
  1565.93,1559.61,1553.52,0,0,0,0,0,0,0,
  0,1561.4,1560.11,1559.01,1553.87,1550.31,0,0,0,0,
  0,0,0,0,1562.66,1554.76,1552.56,0,0,0,
  0,0,0,1556.7,1549.79,1546.19,0,0,0,0,
  1551.8,1546.9,0,0,0,0,1545.77,0,0,0
};




typedef struct{
  int LSid;
  int daq[2];
  int noise[2];
}MONDATA;

MONDATA mondata[MAXAERA];
FILE *fpbase[MAXAERA];
int *filehdr=NULL;
unsigned short *event=NULL;


int tstart=0,tend=0;

int aera_read_file_header(FILE *fp)
{
  int i,return_code;
  int isize;
  
  if( !fread(&isize,INTSIZE,1,fp)) {
    printf("Cannot read the header length\n");
    return(0);                                                       //cannot read the header length
  }
  printf("The header length is %d bytes \n",isize);
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
    //printf("Cannot read the Event length\n");
    return(0);                                                       //cannot read the header length
  }
  //printf("The event length is %d bytes \n",isize);
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
    printf("Cannot read the full event (%d)\n",return_code);
    return(0);                                                       //cannot read the full event
  }
  return(1);
}

void init_mondata(int ih,int sec,int in[2])
{
  int ls;
  int i;

  mondata[ih].LSid = ih;
  for(i=0;i<2;i++){
    mondata[ih].daq[i] = sec;
    mondata[ih].noise[i] = in[i]; //both channels ok
  }
}

void analyze_aera_ls(EVENTBODY *ls)
{
  unsigned char *raw;
  signed short value;
  unsigned int i,iof;
  int station,i_hist;
  int len,imn;
  double val[2],noise;
  int inoise[2]={1,1};
  int next = 0;
  
  station = ls->LS_id&0xff;
  station -=1;
  if(tstart == 0){
    tstart =ls->GPSseconds;
    for(i=0;i<MAXAERA;i++) init_mondata(i,tstart,inoise);
  }
  tend = ls->GPSseconds;
  if((ls->GPSseconds-mondata[station].daq[1])>20)next = 1;
  raw = (unsigned char *)ls->info_ADCbuffer;
  iof = EVENT_ADC;
  len = *(unsigned short *)&raw[EVENT_LENCH1];
  val[0] = 0.;
  val[1] = 0.;
  for(i=0;i<len;i++){ //ch1
    value = (*( short *)&raw[iof+2*i]);
    val[0]+=value;
    val[1]+=value*value;
  }
  val[0] /=len;
  val[1] /= len;
  noise = sqrt(val[1]-val[0]);
  if(noise<10.) inoise[0]=0;
  if(val[0]>100 || val[0]<-100) inoise[0] = 0;
  iof += 2*(*(unsigned short *)&raw[EVENT_LENCH1]);
  len = *(unsigned short *)&raw[EVENT_LENCH2];
  val[0] = 0.;
  val[1] = 0.;
  for(i=0;i<len;i++){ //ch2
    value = (*( short *)&raw[iof+2*i]);
    val[0]+=value;
    val[1]+=value*value;
  }
  val[0] /=len;
  val[1] /= len;
  noise = sqrt(val[1]-val[0]);
  if(noise<10.) inoise[1]=0;
  if(val[0]>100 || val[0]<-100) inoise[1] = 0;
  if(mondata[station].daq[1] == tstart){ //initialisation
    mondata[station].noise[0] = inoise[0];
    mondata[station].noise[1] = inoise[1];
  }
  if(inoise[0] != mondata[station].noise[0] ||
     inoise[1] != mondata[station].noise[1]){
    next = 1;
  }
  iof += 2*(*(unsigned short *)&raw[EVENT_LENCH2]);
  len = *(unsigned short *)&raw[EVENT_LENCH3];
  for(i=0;i<len;i++){ //ch3
    value = (*( short *)&raw[iof+2*i]);
  }
  iof += 2*(*(unsigned short *)&raw[EVENT_LENCH3]);
  len = *(unsigned short *)&raw[EVENT_LENCH4];
  for(i=0;i<len;i++){ //ch4
    value = (*( short *)&raw[iof+2*i]);
  }
  if(next == 1 && fpbase[station] != NULL){
    fprintf(fpbase[station],"%d %d 1 %d %d\n",mondata[station].daq[0],
            mondata[station].daq[1],mondata[station].noise[0],
            mondata[station].noise[1]);
    if((tend-mondata[station].daq[1])>10){
      fprintf(fpbase[station],"%d %d 0 0 0\n",mondata[station].daq[1]+10,
              ls->GPSseconds-10);
    }
    init_mondata(station,tend,inoise);
  }else{
    mondata[station].daq[1]=tend;
  }
}


void analyze_aera_event(){
  EVENTBODY *evls;
  int ils = EVENT_LS;                                                      //parameter indicating start of LS 
  int ev_end = ((int)(event[EVENT_HDR_LENGTH+1]<<16)+(int)(event[EVENT_HDR_LENGTH]))/SHORTSIZE;

  while(ils<ev_end){
    evls = (EVENTBODY *)(&event[ils]);
    analyze_aera_ls(evls);
    ils +=(evls->length);
  }
}  


int main(int argc, char **argv)
{
  FILE *fp;
  int i,i_hist,ich,ib,runnr,station;
  float g1,g2;
  char fname[100],hname[100],cmd[100];
  int ifi,ifimin,ifimax;
  
  sscanf(argv[2],"%d",&runnr);
  sscanf(argv[3],"%d",&ifimin);
  sscanf(argv[4],"%d",&ifimax);
  for(i=0;i<MAXAERA;i++){
    fpbase[i] = NULL;
    sprintf(fname,"%d/%d",runnr,i+1);
    if(zstat[i]>10)fpbase[i] = fopen(fname,"w");
  }
  for(ifi = ifimin;ifi <=ifimax;ifi++){
    fp = NULL;
    sprintf(fname,"%s/td%06d.f%04d",argv[1],runnr,ifi);
    fp = fopen(fname,"r");
    if(fp == NULL) printf("Error opening  !!%s!!\n",fname);
    
    if(aera_read_file_header(fp)){ //lets read events
      while (aera_read_event(fp) >0 ){
        analyze_aera_event();
      }
    }
    if (fp != NULL) fclose(fp); // close the file
  }
  for(station=0;station<MAXAERA;station++){
    if(fpbase[station] != NULL){
      if((tend-mondata[station].daq[1])<10){
        mondata[station].daq[1] = tend;
      }
      if(mondata[station].daq[1] != tstart){
        fprintf(fpbase[station],"%d %d 1 %d %d\n",mondata[station].daq[0],
                mondata[station].daq[1],mondata[station].noise[0],
                mondata[station].noise[1]);
        if((tend-mondata[station].daq[1])>10){
          fprintf(fpbase[station],"%d %d 0 0 0\n",mondata[station].daq[1]+10,
                  tend);
        }
      }else{
        fprintf(fpbase[station],"%d %d 0 0 0\n",tstart,
                tend);
      }
      fclose(fpbase[station]);
    }
  }
}

