/// @file
/// @brief routines for reading RS232 port
/// @author C. Timmermans, Nikhef/RU

#include <ctype.h>
#ifndef Fake
#include "rs232.c"
#endif
extern float *ch_volt;
extern float *ch_cur;
static int comport_nr = 2;

/*!
 \fn char readcomport(void)
 * \brief returns next character on RS232 port
 * \author C. Timmermans
 */

char readcomport (void)
{
  char ch;
#ifndef Fake
  static unsigned char buf[4096];
  static int bp = 0;
  static int nc = 0;

  if (bp < nc) {
    ch = buf[bp];
    ++bp;
  }
  else {
    nc = PollComport(comport_nr, buf, 4095);
    if (nc > 0) {
      ch = buf[0];
      bp = 1;
    }
    else {
      ch = '\0';
      bp = 0;
    }
  }
#endif
  return (ch);
}

/*!
 \fn void writecomport(char ci)
 * \brief writes single character to RS232 port
 * \author C. Timmermans
 */

void
writecomport(char ci)
{
#ifndef Fake
  if(SendByte(comport_nr, ci)== 1) printf("ERROR ON RS232\n");
#endif
}


/*!
 \fn void chc_read()
 * \brief reads charge controller
 * - open the COM port
 * - flush the port
 * - write an 'F'
 * - read response
 * - get voltage and current from response
 * - close the COM port
 * \author C. Timmermans
 */
void chc_read()
{
#ifndef Fake
  int ich;
  int ntry,again;
  char ch,chbuf[1000];
  int bdrate = 1200;
  if (OpenComport(comport_nr, bdrate)) {
    printf ("Can not open comport number %d\n", comport_nr);
    return;
  }
  while((ch = readcomport()) != '\0') {
    usleep(1);
  }

  writecomport('F');
  writecomport('\n');
  usleep(50000);
  ntry = 0;
  ich = 0;
  while(ntry <1000) {
    chbuf[ich] = '\0';
    chbuf[ich] = readcomport();
    if(chbuf[ich] != '\0') {
      if(chbuf[ich] == 'R') ich=0; 
      else if(chbuf[ich] == ',') chbuf[ich]='.'; //proper fraction
      else if(chbuf[ich] == '\n'|| chbuf[ich]=='\r') break;
      if(isprint(chbuf[ich]))ich++;
    }
    else ntry++;
    usleep(1000);
  }
  chbuf[ich]='\0';
  printf("!!%s!!\n",chbuf);
  if(chbuf[0] == 'R' && chbuf[1] == ':'){
    chbuf[7]='\0';
    *ch_volt = 0;
    *ch_volt=10*(chbuf[2]-'0')+chbuf[3]-'0'+0.1*(chbuf[5]-'0')+
      0.01*(chbuf[6]-'0');
    sscanf(&chbuf[9],"%g",ch_cur);
    if(chbuf[8] == '-') *ch_cur = -1*(*ch_cur);
    printf("CHC %g %g\n",*ch_volt,*ch_cur);
  }
  CloseComport(comport_nr);
#endif
}

