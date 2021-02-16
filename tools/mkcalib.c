#include<stdio.h>


main(int argc,char **argv)
{
  int istat,ich;
  char fname[200],line[500];
  FILE *fp;
  short gain[4],offset[4];
  //0x60 = 96 = 300+1200*(96/256) = 750V
  short hv[4]={0,0,0x60,0x60};
  unsigned int  sh[8];

  for(istat=81;istat<130;istat++){
    for(ich=0;ich<4;ich++){
      gain[ich] = 0;
      offset[ich] = 0;
    }
    sprintf(fname,"%s/Auger_settings%03d.txt",argv[1],istat);
    fp = fopen(fname,"r");
    if(fp != NULL) {
      while(fgets(line,499,fp) == line){
	sh[1] = 0;
	sscanf(line,"%x %x %x %x %x %x %x",&sh[0],&sh[1],&sh[2],&sh[3],
	       &sh[4],&sh[5],&sh[6]);
	ich = sh[1]-8;
	if(ich >=0 && ich <4){
	  gain[ich] = sh[4] + (sh[5]<<8);
	  offset[ich] = sh[6];
	}
      }
      fclose(fp);
    }
    printf("%3d ",istat);
    for(ich=0;ich<4;ich++) printf("%04x %04x %04x ",gain[ich],offset[ich],
				  hv[ich]);
    printf("\n");
  }
}
