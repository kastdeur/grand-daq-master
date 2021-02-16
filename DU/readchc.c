/**************************************************************************/
/* Thei Wijnen, 25-Jun-2012. */
/* readchc2.c   12-Nov-2012  */

// readout charge controller 25-Jun-2012
// # 15:35:57  F
// R:12,04; 0,1;23; 9,86;12,24; 0,0; 0,1;


// get rid of "deprecated conversion from string constant to char*" warning.
// gcc note: expected 'char *' but argument is of type 'const char *'
// #pragma GCC diagnostic ignored "-Wwrite-strings" 

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <Windows.h>
#define  msSleep(x) Sleep(x)        // sleep x miliseconds
#else
#include <unistd.h>
#define  msSleep(x) usleep(x*1000)  // sleep x miliseconds
#endif

#include "rs232.c"

// use COM3 on Voipac PC (Auger digitizer)
static int comport_nr = 2;        /* 0 means /dev/ttyS0 (COM1 on windows) */
                                  /* fixed format:  CS8 | CLOCAL | PARENB | PARODD */
void initcomport (void)
{
  int bdrate = 1200;
  if (OpenComport(comport_nr, bdrate)) {
    printf ("Can not open comport number %d\n", comport_nr);
    exit (0);
  }
}

char readcomport (void)
{
  char ch;
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
  //# ifdef _WIN32
  //  Sleep(100); /* It's ugly to use a sleeptimer. In a real program,
  //              /* change the while-loop into a (interrupt) timerroutine */
  //# else
  //  usleep(100000);  /* sleep for 100 milliSeconds */
  //# endif
  return (ch);
}

void
writecomport(char ci)
{
  SendByte(comport_nr, ci);
}

/* --- */

#include <time.h>

// calculate lap time.
// start with calclaptime(0); and get the result with res = calclaptime(1);

int
calclaptime (int flag)
{
  static time_t timeStart, timeLap;
  double diftime;
  int    difseconds;
  //
  if (flag == 0) {
    timeStart = time(NULL);     /* Gets system time */
    difseconds = 0;
  }
  else {
    timeLap = time(NULL);       /* gets system time again */
    diftime = difftime(timeLap,timeStart);
    if (diftime < 1.0) {
      diftime = 1.0;
    }
    difseconds = (int) diftime;
  }

  return (difseconds);
}

/* --- */

#include <stdio.h>
#include <termios.h>
#include <unistd.h>
#include <fcntl.h>
 
int kbhit(void)
{
  struct termios oldt, newt;
  int ch;
  int oldf;
 
  tcgetattr(STDIN_FILENO, &oldt);
  newt = oldt;
  newt.c_lflag &= ~(ICANON | ECHO);
  tcsetattr(STDIN_FILENO, TCSANOW, &newt);
  oldf = fcntl(STDIN_FILENO, F_GETFL, 0);
  fcntl(STDIN_FILENO, F_SETFL, oldf | O_NONBLOCK);
  ch = getchar();
  tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
  fcntl(STDIN_FILENO, F_SETFL, oldf);
  if(ch != EOF) {
    ungetc(ch, stdin);
    return 1;
  }
  return 0;
}
 
/* --- */

/* main program */

FILE *stdlog;

int
main (int argc, char **argv)
{
  int   busy = 1;
  char  ch, ci;                 /* character received from COM port */
  int   secprev = 0;
  int   secnow = 0;
  int   waittime = 10;          /* every 10 seconds */
  time_t rawtime;
  struct tm* thetime;
  char  datumtijd[60];
  int   printflag = 0;
  int   relaxtime = 50;         /* 50 miliseconds */
  //
  fprintf (stderr, "Usage: readchc [logfile] [seconds] [printflag]\n");
  initcomport ();               /* open predefined COM port (static comport_nr) */
  stdlog = NULL;
  if (argc >= 2) {
    /* anything (non-null) as command line argument, will open a log file */
    if ((stdlog = fopen(argv[1], "w")) == NULL) {
      fprintf (stderr, " Can not open logfile %s\n", argv[1]);
      busy = 0;
    }
  }
  else {
    printflag = 1;                      /* turn on printing when no log file */
  }
  if (argc >= 3) {
    waittime = atoi(argv[2]);
  }
  if (waittime <= 1) {
    waittime = 0;                       /* minimum wait time 1+0 seconds */
  }
  else {
    waittime -= 1;                      /* number of seconds to wait */
  }
  if (argc >= 4) {
    printflag = atoi(argv[3]);          /* >0 to turn on the printflag */
  }
  //
  calclaptime(0);                       /* start the laptimer */
  secprev = 0;
  while (busy) {
#   ifdef KEYS
    if (kbhit()) {
      if ((ci = getc(stdin)) == 'Q') {
        busy = 0;
      }
    }
#   else
    ci = 0;
#   endif
    /* check wether I received something from the COM port  */
    ch = readcomport();
    if (ch != '\0') {
      if (printflag != 0) {
        printf ("%c", ch);              /* print character to screen */
      }
      if (ch != '\r') {                 /* before \n at end of answer line */
        if (stdlog != NULL) { fprintf (stdlog, "%c", ch); }
      }
      if (ch == '\n') {                 /* at the end of the answer */
        relaxtime = 50;                 /* set sleeptime to 50 ms */
      }
    }
    secnow = calclaptime(1);            /* check the laptimer */
    if (secnow > (secprev + waittime)) {
      secprev = secnow;
      time (&rawtime);
      thetime = gmtime (&rawtime);
      sprintf (datumtijd, "# %02d:%02d:%02d  F\n"
        , (thetime->tm_hour+2)%24, thetime->tm_min, thetime->tm_sec);
      if (printflag != 0) {
        printf ("%s", datumtijd);
        if (printflag == -1) printf ("# ...\n");
        if (printflag < 0) ++printflag;
      }
      if (stdlog != NULL) {
        fprintf (stdlog, "%s", datumtijd);
      }
      writecomport('F');
      writecomport('\n');
      relaxtime = 1;                    /* set sleeptime to 1 ms */
      /* I expect an answer coming in at 1200 baud ... */
    }
    else {
      msSleep(relaxtime);
      /* it's ugly to use a sleeptimer. In a real program, */
      /* change the while-loop into a (interrupt) timerroutine */
    }
  }
  //
  if (stdlog != NULL) {
    fclose (stdlog);
  }
  return(0);
}

/* --- */

