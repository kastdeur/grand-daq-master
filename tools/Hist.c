// Traces.c
#include <stdio.h>
#include <time.h>
#include <stdlib.h>
//#include "amsg.h"
#include "scope.h"
#include "Traces.h"
#define INTSIZE 4
#define SHORTSIZE 2
#include "fftw3.h"
#include "TROOT.h"
#include "TFile.h"
#include "TTree.h"
#include "TClass.h"
#include "TSystem.h"
#include "TH1.h"
#include "TH2.h"
#include "TCanvas.h"
#include "TStyle.h"
#include "TProfile.h"
#include "TProfile2D.h"
#include "grand_util.h"

int *filehdr=NULL;
unsigned short *event=NULL;
float *ttrace,*fmag,*fphase;
int fftlen = 0;
TProfile *HFsum[100][4]; // One for each arm
TProfile2D *HFTime[100][4];
int n_DU=0,DU_id[100];


unsigned long valid_trace_counter = 0;
unsigned long invalid_trace_counter = 0;

/**
 * Validate a trace
 * @param[in] ttrace The trace to validate
 * @param[in] ttrace_size Size of ttrace
 * @return exit-status: 0 means OK
 */
int validate_trace(
        const float *ttrace,
        const uint16_t ttrace_size,
        const int16_t max_trace_value = 8193,
        const uint16_t max_values_wo_zero_crossing = 100
){
  uint16_t zero_crossing_counter = 0;

  for( int i=0; i < ttrace_size; i++ )
  {
        // Too high ADC values
        if ( ttrace[i] > max_trace_value  || ttrace[i] < -max_trace_value)
        {
            return 1;
        }

        // Zero Crossings
        if ( i > 1 ) {
            if ( signbit(ttrace[i-1]) != signbit(ttrace[i]) ){
                zero_crossing_counter = 0;
            } else {
                zero_crossing_counter++;
            }

            if ( zero_crossing_counter >= max_values_wo_zero_crossing )
            {
                return 2;
            }
        }
    }

  return 0;
}

int grand_read_file_header(FILE *fp)
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

void print_file_header()
{
  int i,additional_int;
  struct tm *mytime;
  
  additional_int = 1+(filehdr[FILE_HDR_LENGTH]/INTSIZE) - FILE_HDR_ADDITIONAL; //number of additional words in the header
  if(additional_int<0){
    printf("The header is too short!\n");
    return;
  }
  printf("Header Length is %d bytes\n",filehdr[FILE_HDR_LENGTH]);
  printf("Header Run Number is %d\n",filehdr[FILE_HDR_RUNNR]);
  printf("Header Run Mode is %d\n",filehdr[FILE_HDR_RUN_MODE]);
  printf("Header File Serial Number is %d\n",filehdr[FILE_HDR_SERIAL]);
  printf("Header First Event is %d\n",filehdr[FILE_HDR_FIRST_EVENT]);
  mytime = gmtime((const time_t *)(&filehdr[FILE_HDR_FIRST_EVENT_SEC]));
  printf("Header First Event Time is %s",asctime(mytime));
  printf("Header Last Event is %d\n",filehdr[FILE_HDR_LAST_EVENT]);
  mytime = gmtime((const time_t *)(&filehdr[FILE_HDR_LAST_EVENT_SEC]));
  printf("Header Last Event Time is %s",asctime(mytime));
  for(i=0;i<additional_int;i++){
    printf("HEADER Additional Word %d = %d\n",i,filehdr[i+FILE_HDR_ADDITIONAL]);
  }
}

int grand_read_event(FILE *fp)
{
  int isize,return_code;
  
  if( !fread(&isize,INTSIZE,1,fp)) {
    printf("Cannot read the Event length\n");
    return(0);                                                       //cannot read the header length
  }
  printf("The event length is %d bytes \n",isize);
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

void print_du(uint16_t *du)
{
  int i,ic,idu;
  int ioff;
  short value;
  short bit14;
  int mask;
  const char invalid_trace_suffix[] = "-Invalid";
  bool invalid_trace = 0;
  char hname[100],fname[100];
  
  printf("\t T3 ID = %u\n",du[EVT_ID]);
  printf("\t DU ID = %u\n",du[EVT_HARDWARE]);
  printf("\t DU time = %u.%09u\n",*(uint32_t *)&du[EVT_SECOND],
         *(uint32_t *)&du[EVT_NANOSEC]);
  printf("\t Trigger Position = %d T3 FLAG = 0x%x\n",
         du[EVT_TRIGGERPOS],du[EVT_T3FLAG]);
  printf("\t Atmosphere (ADC) Temp = %d Pres = %d Humidity = %d\n",
         du[EVT_ATM_TEMP],du[EVT_ATM_PRES],du[EVT_ATM_HUM]);
  printf("\t Acceleration (ADC) X = %d Y = %d Z = %d\n",
         du[EVT_ACCEL_X],du[EVT_ACCEL_Y],du[EVT_ACCEL_Z]);
  printf("\t Battery (ADC) = %d\n",du[EVT_BATTERY]);
  printf("\t Format Firmware version = %d\n",du[EVT_VERSION]);
  printf("\t ADC: sampling frequency = %d MHz, resolution=%d bits\n",
         du[EVT_MSPS],du[EVT_ADC_RES]);
  printf("\t ADC Input channels =0x%x, Enabled Channels=0x%x\n",
         du[EVT_INP_SELECT],du[EVT_CH_ENABLE]);
  printf("\t Number of ADC samples Total = %d",16*du[EVT_TOT_SAMPLES]);
  for(i=1;i<=4;i++) printf(" Ch%d = %d",i,du[EVT_TOT_SAMPLES+i]);
  printf("\n");
  printf("\t Trigger pattern=0x%x Rate=%d\n",du[EVT_TRIG_PAT],du[EVT_TRIG_RATE]);
  printf("\t Clock tick %u Nticks/sec %u\n",
         *(uint32_t *)&du[EVT_CTD],*(uint32_t *)&du[EVT_CTP]);
  printf("\t GPS: Offset=%g LeapSec=%d Status 0x%x Alarms 0x%x Warnings 0x%x\n",
         *(float *)&du[EVT_PPS_OFFSET],du[EVT_LEAP],du[EVT_GPS_STATFLAG],
         du[EVT_GPS_CRITICAL],du[EVT_GPS_WARNING]);
  float fh = (du[EVT_MINHOUR]&0xff)+((du[EVT_MINHOUR]>>8)&0xff)/60.;
  printf("\t GPS: %02d/%02d/%04d %02d:%02d:%02d (%g)\n",
         (du[EVT_DAYMONTH]>>8)&0xff,(du[EVT_DAYMONTH])&0xff,du[EVT_YEAR],
         du[EVT_MINHOUR]&0xff,(du[EVT_MINHOUR]>>8)&0xff,du[EVT_STATSEC]&0xff,fh);
  printf("\t GPS: Long = %g Lat = %g Alt = %g Chip Temp=%g\n",
         57.3*(*(double *)&du[EVT_LONGITUDE]),57.3*(*(double *)&du[EVT_LATITUDE]),
         *(double *)&du[EVT_ALTITUDE],*(float *)&du[EVT_GPS_TEMP]);
  printf("\t Digi CTRL");
  for(i=0;i<8;i++) printf(" 0x%x",du[EVT_CTRL+i]);
  printf("\n");
  printf("\t Digi Pre-Post trigger windows");
  for(i=0;i<8;i++) printf(" 0x%x",du[EVT_WINDOWS+i]);
  printf("\n");
  for(ic=1;ic<=4;ic++){
    printf("\t Ch%d properties:",ic);
    for(i=0;i<6;i++)printf(" 0x%x",du[EVT_CHANNEL+6*(ic-1)+i]);
    printf("\n");
  }
  for(ic=1;ic<=4;ic++){
    printf("\t Ch%d trigger settings:",ic);
    for(i=0;i<6;i++)printf(" 0x%x",du[EVT_TRIGGER+6*(ic-1)+i]);
    printf("\n");
  }
  ioff = du[EVT_HDRLEN];
  if(n_DU == 0){
    for(ic=0;ic<4;ic++){
      sprintf(fname,"HSF%d",ic);
      sprintf(hname,"HSF%d",ic);
      HFsum[0][ic] = new TProfile(fname,hname,du[EVT_TOT_SAMPLES+1]/2,0.,250);
    }
    n_DU = 1;
  }
  for(idu=1;idu<n_DU;idu++){
    if(du[EVT_HARDWARE] == DU_id[idu]) break;
  }
  if(idu ==n_DU) {
    DU_id[n_DU++] = du[EVT_HARDWARE];
    for(ic=0;ic<4;ic++){
      sprintf(fname,"HSF%d_%d",DU_id[idu],ic);
      sprintf(hname,"HSF%d_%d",DU_id[idu],ic);
      HFsum[idu][ic] = new TProfile(fname,hname,du[EVT_TOT_SAMPLES+1]/2,0.,250);
      sprintf(fname,"HSFTime%d_%d",DU_id[idu],ic);
      sprintf(hname,"HSFTime%d_%d",DU_id[idu],ic);
      HFTime[idu][ic] = new TProfile2D(fname,hname,du[EVT_TOT_SAMPLES+1]/2,0.,250,240,0.,24.);
    }
  }
  for(ic=1;ic<=4;ic++){
    if(du[EVT_TOT_SAMPLES+ic]>0){
      if(fftlen !=du[EVT_TOT_SAMPLES+ic]){
        fftlen = du[EVT_TOT_SAMPLES+ic];
        fft_init(fftlen);
        ttrace = (float *)malloc(fftlen*sizeof(float));
        fmag = (float *)malloc(fftlen*sizeof(float));
        fphase = (float *)malloc(fftlen*sizeof(float));
      }
      sprintf(fname,"H%dT%dD%d",du[EVT_ID],ic,du[EVT_HARDWARE]);
      sprintf(hname,"H%dT%dD%d",du[EVT_ID],ic,du[EVT_HARDWARE]);
      TH1F *Hist = new TH1F(fname,hname,du[EVT_TOT_SAMPLES+ic],0.,2*du[EVT_TOT_SAMPLES+ic]);

      // Trace
      for(i=0;i<du[EVT_TOT_SAMPLES+ic];i++){
        value =(int16_t)du[ioff++];
        bit14 = (value & ( 1 << 13 )) >> 13;
        mask = 1 << 14; // --- bit 15
        value = (value & (~mask)) | (bit14 << 14);
        mask = 1 << 15; // --- bit 16
        value = (value & (~mask)) | (bit14 << 15);
        Hist->SetBinContent(i+1,value);
        ttrace[i] = value;
      }

      invalid_trace = validate_trace(ttrace, du[EVT_TOT_SAMPLES+ic]);

	  // Increase appropriate trace counter
	  valid_trace_counter += !invalid_trace;
	  invalid_trace_counter += (bool) invalid_trace;

      if ( invalid_trace != 0 ){
            printf("!! Invalid Trace (%d): %s \n", invalid_trace, hname);

            if ( 1 ){ // Append a suffix to filename
                char tmp_fname[100];
                strcpy(tmp_fname, fname);

                sprintf(fname,"%s%s", tmp_fname, invalid_trace_suffix);
                Hist->SetName(fname);
            }
            else{ // Throw away this trace and continue with next channel
                Hist->Delete();
                continue;
            }
      }
      Hist->Write();
      Hist->Delete();

      // Fourier
      mag_and_phase(ttrace,fmag,fphase);

      // Magnitude
      sprintf(fname,"H%d%s%dD%d%s",du[EVT_ID],"FM",ic,du[EVT_HARDWARE],(invalid_trace == 0 ? "" : invalid_trace_suffix));
      sprintf(hname,"H%d%s%dD%d",  du[EVT_ID],"FM",ic,du[EVT_HARDWARE]);
      Hist = new TH1F(fname,hname,fftlen/2,0.,250);

      for(i=0;i<fftlen/2;i++){
        Hist->SetBinContent(i+1,fmag[i]);
        HFsum[0][ic-1]->Fill(500*(i+0.5)/fftlen,fmag[i]);
        HFsum[idu][ic-1]->Fill(500*(i+0.5)/fftlen,fmag[i]);
        HFTime[idu][ic-1]->Fill(500*(i+0.5)/fftlen,fh,fmag[i]);
      }
      Hist->Write();
      Hist->Delete();

      // Phase
      sprintf(fname,"H%d%s%dD%d%s",du[EVT_ID],"FP",ic,du[EVT_HARDWARE],(invalid_trace == 0 ? "" : invalid_trace_suffix));
      sprintf(hname,"H%d%s%dD%d",  du[EVT_ID],"FP",ic,du[EVT_HARDWARE]);
      Hist = new TH1F(fname,hname,fftlen/2,0.,250);
      for(i=0;i<fftlen/2;i++){
        Hist->SetBinContent(i+1,fphase[i]);
      }
      Hist->Write();
      Hist->Delete();

      // Magnitude * Phase
      sprintf(fname,"H%d%s%dD%d%s",du[EVT_ID],"FMP",ic,du[EVT_HARDWARE],(invalid_trace == 0 ? "" : invalid_trace_suffix));
      sprintf(hname,"H%d%s%dD%d",  du[EVT_ID],"FMP",ic,du[EVT_HARDWARE]);
      Hist = new TH1F(fname,hname,fftlen/2,0.,250);
      for(i=0;i<fftlen/2;i++){
        Hist->SetBinContent(i+1,fphase[i]*fmag[i]);
      }
      Hist->Write();
      Hist->Delete();
    }
  }
}

void print_grand_event(){
  uint16_t *evdu;
  unsigned int *evptr = (unsigned int *)event;
  int idu = EVENT_DU;                                                      //parameter indicating start of LS
  int ev_end = ((int)(event[EVENT_HDR_LENGTH+1]<<16)+(int)(event[EVENT_HDR_LENGTH]))/SHORTSIZE;
  printf("Event Size = %d\n",*evptr++);
  printf("      Run Number = %d\n",*evptr++);
  printf("      Event Number = %d\n",*evptr++);
  printf("      T3 Number = %d\n",*evptr++);
  printf("      First DU = %d\n",*evptr++);
  printf("      Time Seconds = %u\n",*evptr++);
  printf("      Time Nano Seconds = %d\n",*evptr++);
  evdu = (uint16_t *)evptr;
  printf("      Event Type = ");
  if((evdu[0] &TRIGGER_T3_MINBIAS)) printf("10 second trigger\n");
  else if((evdu[0] &TRIGGER_T3_RANDOM)) printf("random trigger\n");
  else printf("Shower event\n");
  ++evdu;
  printf("      Event Version = %d\n",*evdu);
  evptr +=3;
  printf("      Number of DU's = %d\n",*evptr);
  while(idu<ev_end){
    evdu = (uint16_t *)(&event[idu]);
    print_du(evdu);
    idu +=(evdu[EVT_LENGTH]);
  }
}


int main(int argc, char **argv)
{
  FILE *fp;
  int i,ich,ib;
  std::string fout {"Hist.root"};

  if ( argc > 2 )
  {
	  fout = argv[2];
  }

  TFile g(fout.c_str(), "RECREATE");

  fp = fopen(argv[1],"r");
  if(fp == NULL) printf("Error opening  !!%s!!\n",argv[1]);
  
  if(grand_read_file_header(fp) ){ //lets read events
    print_file_header();
    while (grand_read_event(fp) >0 ) {
      print_grand_event();
    }
  }
  if (fp != NULL) fclose(fp); // close the file

  for(ib=0;ib<n_DU;ib++){
    for(ich = 0;ich<4;ich++){
      if(HFsum[ib][ich]!= NULL) HFsum[ib][ich]->Write();
      if(HFTime[ib][ich]!= NULL) HFTime[ib][ich]->Write();
    }
  }

  // report stats
  printf("\n");
  printf("----- Traces Stats -----\n");
  printf("Total: %d\n", valid_trace_counter + invalid_trace_counter);
  printf("Valid: %d\n", valid_trace_counter);
  printf("Invalid: %d\n", invalid_trace_counter);

  g.Close();
}
