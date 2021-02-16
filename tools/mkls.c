#include <stdio.h>

#define MAXDIGI 130

main()
{
  int digi,ic,nlines,i;
  int gain[MAXDIGI][4],offset[MAXDIGI][4],hv[MAXDIGI][4],aera[MAXDIGI];
  char line[300];
  char settings[300][300];
  FILE *fpin,*fpout;
  //  step 1: read the digitizer gains
  fpin = fopen("gain.tbl","r");
  while(fgets(line,299,fpin)  == line){
    if(line[0] == '#') continue;
    sscanf(line,"%d",&digi);
    sscanf(line,
	   "%d %04x %04x %04x %04x %04x %04x %04x %04x %04x %04x %04x %04x",
	   &ic,&gain[digi][0],&offset[digi][0],&hv[digi][0],
	   &gain[digi][1],&offset[digi][1],&hv[digi][1],
	   &gain[digi][2],&offset[digi][2],&hv[digi][2],
	   &gain[digi][3],&offset[digi][3],&hv[digi][3]);
  }
  fclose(fpin);
  // step 2: translation table digi->LS number
  for(digi=0;digi<MAXDIGI;digi++) aera[digi] = -1;
  fpin = fopen("lsdigi.tbl","r");
  while(fgets(line,299,fpin)  == line){
    if(line[0] == '#') continue;
    ic = -1;
    if(sscanf(line,"%d %d",&digi,&ic) == 2) aera[digi] = ic;
  }
  fclose(fpin);
  // step 3: Read master file
  fpin = fopen("ls.settings","r");
  nlines = 0;
  while(fgets(settings[nlines],299,fpin) == settings[nlines]) nlines++;
  fclose(fpin);
  for(digi=80;digi<MAXDIGI;digi++){
    if(aera[digi]>0){
      sprintf(line,"LS/aera_%03d",aera[digi]);
      fpout = fopen(line,"w");
      fprintf(fpout,"# Hardware box %d\n",digi);
      for(ic=0;ic<nlines;ic++){
	sscanf(settings[ic],"0x%02x",&i);
	printf("%x\n",i);
	i = i-8;
	if(i>=0 && i<=3) {
	  sprintf(&settings[ic][14],"0x%04x",gain[digi][i]);
	  settings[ic][20]=' ';
	  sprintf(&settings[ic][25],"%02x",offset[digi][i]);
	  settings[ic][27]=' ';
	  sprintf(&settings[ic][42],"0x%04x",hv[digi][i]);
	  settings[ic][48]=' ';
	}
	fputs(settings[ic],fpout);
      }
      fclose(fpout);
    }
  }
  // step 4: Read V1 master file
  fpin = fopen("lsv1.settings","r");
  nlines = 0;
  while(fgets(settings[nlines],299,fpin) == settings[nlines]) nlines++;
  fclose(fpin);
  for(digi=0;digi<80;digi++){
    if(aera[digi]>0){
      sprintf(line,"LS/aera_%03d",aera[digi]);
      fpout = fopen(line,"w");
      fprintf(fpout,"# Hardware box %d\n",digi);
      for(ic=0;ic<nlines;ic++){
	sscanf(settings[ic],"0x%02x",&i);
	printf("%x\n",i);
	i = i-8;
	if(i>=0 && i<=3) {
	  sprintf(&settings[ic][14],"0x%04x",gain[digi][i]);
	  settings[ic][20]=' ';
	  sprintf(&settings[ic][25],"%02x",offset[digi][i]);
	  settings[ic][27]=' ';
	  sprintf(&settings[ic][42],"0x%04x",hv[digi][i]);
	  settings[ic][48]=' ';
	}
	fputs(settings[ic],fpout);
      }
      fclose(fpout);
    }
  }
}
