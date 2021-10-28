// test.c

#include <stdio.h>
#include <stdlib.h>
#include "scope.h"

EV_DATA t3_buffer[MAXT3];

int t3_wait=0;          // the pointer to the last event waiting to be sent
int t3_send=0;          // the pointer to the last event that was sent


int main()
{
  char name[100];
  unsigned short ctrllist[9]={0x0199,0x12,CTRL_SEND_EN|CTRL_PPS_EN
    ,0xff00|TRIG_10SEC|TRIG_EXT, 0x000f,0x0010,0,0,0x6666};
  unsigned short winlist[11]={0x299,0x16,0x50,0x54,0x52,0x56,0x54,0x58,0x56,0x5a,0x6666};
  unsigned short chlist[9]={0x0899,0x12,0x4000,0x0080,0x2100,0x1900,0x0,0x0,0x6666};
  unsigned short trlist[9]={0x0C99,0x12,0xa0,0xa0,0x50c8,0xff64,0xff00,0x0,0x6666};
  unsigned short fltlist[11]={0x1099,0x16,0x1,0x2,0x3,0x4,0x5,0x6,0x7,0x8,0x6666};
  unsigned short *shadowlist;
  unsigned char *offset;
  int id,ic;

  printf("Test\n");
  sprintf(name,"TEST");
  id=78;
  if(scope_open() !=1) exit(-1);
  scope_initialize(name,&id);
#if defined(CALFIRST)
  scope_calibrate();
  exit(-1);
#endif
  scope_set_parameters(ctrllist);
  scope_set_parameters(winlist);
  for(ic=0;ic<4;ic++) {
    shadowlist = (unsigned short *)scope_get_shadow(ID_PARAM_CH1+ic);
    chlist[2] = shadowlist[2];  // set offset and gain from shadowlist
    offset = (char *)&shadowlist[3];
    chlist[3] = ((*offset)&0xff) + (chlist[3]&0xff00); 
    chlist[0]=0x0899+ic*0x100;
    scope_set_parameters(chlist);
    trlist[0]=0x0c99+ic*0x100;
    trlist[2]=0x190+ic;
    trlist[3]=0xa0+ic;
    scope_set_parameters(trlist);
    fltlist[0]=0x1099+ic*0x200;
    scope_set_parameters(fltlist);
    fltlist[0]=0x1199+ic*0x200;
    scope_set_parameters(fltlist);
  }
  while(1){
    if(scope_no_run_read() == SCOPE_GPS) scope_calc_evnsec();
  }
  scope_close();
  return(1);
}

//
