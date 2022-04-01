/***
 T3 Maker
 Version:1.0
 Date: 18/2/2020
 Author: Charles Timmermans, Nikhef/Radboud University
 
 Altering the code without explicit consent of the author is forbidden
 ***/
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/time.h>
#include <math.h>
#include "Adaq.h"
#include "amsg.h"

#define MAXRATE 1000
#define MAXSEC 12 // has to be above 10 to avoid missing 10sec triggers!
#define NEVT (MAXSEC*MAXDU*MAXRATE)
#define GIGA    1000000000
#define T3DELAY GIGA
#define TCOINC 34000  // Maximum coincidence time window
#define TNEAR 4900 //maximum time for nearest neighbours
#define NTRIG 4 //total at least 4 stations
#define NNEAR 2 // station needs 2 nearest neghbours to fire (ie 3 DUs  in 1 km2)

typedef struct{
  int stat;
  int unit;
  unsigned int insertsec;
  int insertmusec;
  unsigned int sec;
  unsigned int nsec;
  int trigflag;
  int used;
}T2evts;

T2evts t2evts[NEVT]; //storage of data directly from the shared memory. This is the local sorted storage!
uint16_t t3list[3+3*MAXDU]; //identifiers of the T3 event
uint16_t t3event=0;

int32_t t2write = 0;
uint32_t last_read_sec = 0;

extern int idebug;

//database
int statlist[MAXDU]; //conversion from DU number to regular index to be used
float posx[MAXDU],posy[MAXDU]; // stations X and Y positions, to be read at initialization!
int ctimes[MAXDU][MAXDU];
/**
 int t3_compare(const void *a, const void *b)
 
 To be used in qsort. Returns 1 if a<b and -1 if b<a --> reverse ordering
 */
int t3_compare(const void *a, const void *b)
{ /* sorting in REVERSE order, easy removal of older data */
  T2evts *t1,*t2;
  t1 = (T2evts *)a;
  t2 = (T2evts *)b;
  if(t1->sec < t2->sec) return(1);
  if(t1->sec == t2->sec){
    if(t1->nsec < t2->nsec) return(1);
    else if(t2->nsec < t1->nsec) return(-1);
    else return(0);
  }
  return(-1);
}
/**
 void t3_gett2()
 get data from the t2-shared memory
 interpret the T2 buffer (see documentation)
 write the buffer in the T2evts array
 determine if we need to upgrade trigger automatically to T3
 reverse-sort the T2evts array
 remove old data from the array (at least MAXSEC seconds old)
 */
void t3_gett2()
{
  AMSG *msg;
  T2BODY *t2b;
  T2SSEC *t2ss;
  uint32_t sec,stat;
  uint32_t prev_nsec,nsec;
  int32_t isub,nsub;
  int32_t ind;
  int gotdata=0;
  static int irandom=0;
  struct timeval tp;
  struct timezone tz;
  
  gettimeofday(&tp,&tz);
  if(idebug) printf("Get T2 %d %ld\n",t2write,tp.tv_sec);
  while(shm_t2.Ubuf[(*shm_t2.size)*(*shm_t2.next_read)] == 1){ // loop over the input
    msg = (AMSG *)(&(shm_t2.Ubuf[(*shm_t2.size)*(*shm_t2.next_read)+1]));
    if(msg->tag == DU_T2){    // work on T2 messages only
      t2b = (T2BODY *)msg->body;
      stat = t2b->DU_id; // the indices of my array start at 0, stations at 1
      sec = T0(t2b->t0);  // obtain the seconds
      /**if(sec>0x80000000) {
        shm_t2.Ubuf[(*shm_t2.size)*(*shm_t2.next_read)] = 0;
        *shm_t2.next_read = (*shm_t2.next_read) + 1;
        if( *shm_t2.next_read >= *shm_t2.nbuf) *shm_t2.next_read = 0;
        continue;
      }**/
      if(sec != last_read_sec) last_read_sec = sec;
      if((sec>(t2evts[0].sec+100))&&t2write != 0) {
        printf("T3: Error in timing, large jump; LS=%d\n",stat);
      }
      if(idebug)printf("Received a T2 %d %u\n",stat,sec);
      prev_nsec = 0;      // needed to check if we loop over into next second
      nsub = (msg->length-5)/2;  // number of subseconds in this T2
      for(isub=0;isub<nsub;isub++){
        t2ss = &(t2b->t2ssec[isub]);
        nsec = T2NSEC(t2ss)+(((t2ss->ADC)&0xf)<<2); //lower bits removed (not according to specs, we have 28 bits?)
        if(nsec < prev_nsec) sec++;
        prev_nsec = nsec;
        if(t2write> 0){
          if(sec == t2evts[t2write-1].sec &&
             nsec == t2evts[t2write-1].nsec &&
             stat == t2evts[t2write-1].stat 
             )  continue;
        }
        t2evts[t2write].insertsec = tp.tv_sec;
        t2evts[t2write].insertmusec = tp.tv_usec;
        t2evts[t2write].sec = sec; // save data into array
        t2evts[t2write].nsec = nsec;
        t2evts[t2write].trigflag = ((t2ss->ADC>>4)&0xf);
        irandom++; // for random writing of data
        if(t3_rand>0){
          if(irandom >=t3_rand){
            irandom = 0;
            //printf("T3: A random event %d.%09d ",t2evts[t2write].sec,t2evts[t2write].nsec);
            t2evts[t2write].trigflag = (t2evts[t2write].trigflag|8);
            //printf("%x\n",t2evts[t2write].trigflag);
          }
        }
        t2evts[t2write].stat = stat;
        for(ind=0;ind<MAXDU;ind++) {
          if(stat == statlist[ind]) t2evts[t2write].unit = ind;
        }
        t2evts[t2write].used = 0;
        t2write+= 1;
        gotdata = 1;
        if(t2write >=NEVT) {
          printf("T3: Full buffer %d\n",t2write);
          t2write = NEVT-1;
          break;
        }
      }
    }
    shm_t2.Ubuf[(*shm_t2.size)*(*shm_t2.next_read)] = 0;
    *shm_t2.next_read = (*shm_t2.next_read) + 1;
    if( *shm_t2.next_read >= *shm_t2.nbuf) *shm_t2.next_read = 0;
  }
  if(gotdata == 0) return;
  if(idebug)printf("T3: T2write = %d\n",t2write);
  for(ind=0;ind<t2write;ind++){ //clear old or used data
    if(((tp.tv_sec-t2evts[ind].insertsec)>MAXSEC) ||
       (t2evts[ind].used == 1)){
      t2evts[ind].insertsec = 0;
      t2evts[ind].sec = 0;
    }
    if((t2evts[ind].sec>t2evts[t2write-1].sec) &&(t2evts[ind].sec-t2evts[t2write-1].sec)>MAXSEC){
      if(idebug) printf("T3: Cleanup1 %u %u %d\n",t2evts[ind].sec,t2evts[t2write-1].sec,ind);
      t2evts[ind].insertsec = 0; //get rid of GPS issues
      t2evts[ind].sec = 0; //get rid of GPS issues
    }
  }
  qsort(t2evts,t2write,sizeof(T2evts),t3_compare);
  // remove old data
  ind = t2write-1;
  sec = t2evts[0].insertsec-MAXSEC;//t2evts[0] == newest!
  if(idebug) printf("Timetest %d %d %d\n",t2evts[0].insertsec,t2evts[ind].insertsec,ind);
  while(sec > t2evts[ind].insertsec &&ind>0) {
    ind--; //real cleanup!
  }
  //printf(" %d\n",ind);
  if(sec >t2evts[1].insertsec ) t2write = ind+1;
  else t2write = ind;
}


/**
 int  t3_check_t3list()
 
 check if there are at least 3 stations (T3 condition)
 */
int t3_check_t3list()
{
  int nstat = (t3list[0]-3)/T3STATIONSIZE;
  if(nstat<3) return(0);
  return(1);
}

/**
 void t3_maket3()
 
 Loop over the sorted t2 array
 Make sure the event is at least 1.5 seconds old
 Check if there are several entries for which the time difference is less than TCOINC between then
 Check if the event is a T3 or Minbias or random
 Write data to T3 shared memory, to be submitted to the DU's
 */
void t3_maket3()
{
  int ind,ip,i;
  int tdif,insertdif;
  int isten,israndom,nstat;
  int usedstat[MAXDU];
  int is1,is2;
  int ntry;
  int nscint,nradio,nwait;
  T3STATION *t3stat;
  int evsize,evnear;
  int eventindex[MAXDU];
  struct timeval tp;
  struct timezone tz;
  
  gettimeofday(&tp,&tz);

  if(idebug) printf("Entering make t3 %d\n",t2write);
  for(ind=(t2write-1);ind>=0;ind--){
    // 1st ensure that event is old enough, not likely for new data to appear
    insertdif = tp.tv_sec-t2evts[ind].sec;
    if(insertdif < 2 ){ //2 or more is clearly ok!
      insertdif = GIGA*(insertdif) + (t2evts[0].nsec-t2evts[ind].nsec);
    } else insertdif = T3DELAY+1;
    if(insertdif< T3DELAY) break;
    // if event is used, do not use it again
    if(t2evts[ind].used ==1) continue;
    //check if there are any coincidences
    eventindex[0] = ind;
    evsize = 1;
    evnear = 0;
    if(t2evts[ind].trigflag&0x4) isten = 1;
    else isten = 0;
    if(t2evts[ind].trigflag&0x8) israndom = 1;
    else israndom = 0;
    if(isten == 1) printf("A 10 sec trigger %d\n",t2evts[ind].sec);
    for(i=ind-1;i>=0;i--){
      if(t2evts[i].sec-t2evts[ind].sec > 1) tdif = TCOINC+1;
      else if(t2evts[i].sec-t2evts[ind].sec == 1){
        tdif = GIGA+t2evts[i].nsec-t2evts[ind].nsec;
      }else{
        tdif = t2evts[i].nsec-t2evts[ind].nsec;
      }
      if(tdif <= ctimes[t2evts[i].unit][t2evts[ind].unit]) {
        if ((isten && (t2evts[i].trigflag&0x4)) || (isten==0 &&(t2evts[i].trigflag&0x4))==0){
          eventindex[evsize] = i;
          evsize++;
          if(evsize>=MAXDU) {
            printf("Too many DUs in an event, loosing data %d %d %d (%d %d) %d %d\n",isten,evsize,MAXDU,tdif,ctimes[t2evts[i].unit][t2evts[ind].unit],i,ind);
            evsize = MAXDU-1;
          }
          if((ctimes[t2evts[i].unit][t2evts[ind].unit]<=TNEAR)
             &&(t2evts[i].unit != t2evts[ind].unit)) evnear++;
        }
      } else if(tdif>TCOINC) break;
    }
    // trigger condition is easy
    if((evsize>=NTRIG &&evnear>=NNEAR) || isten == 1 || israndom == 1) {
      if(isten == 1) printf("Found a T10 with %d stations T=%u\n",evsize,t2evts[ind].sec);
      //start creating the list to send
      t3list[0] = 3; // length before adding a station
      if(isten == 1) t3list[1] = DU_GET_MINBIAS_EVENT;
      else if(israndom == 1) t3list[1] = DU_GET_RANDOM_EVENT;
      else t3list[1] = DU_GETEVENT; // requesting an event
      t3list[2] = t3event; // event number
      ip = 3;
      for(i=0;i<evsize;i++){
        t2evts[eventindex[i]].used = 1;
        t3stat = (T3STATION *)(&(t3list[ip]));
        T3STATFILL(t3stat,t2evts[eventindex[i]].stat,t2evts[eventindex[i]].sec,
                   ((t2evts[eventindex[i]].nsec)>>6)); //filling the station info
        ip+=3;
        t3list[0]+=3;
      }
      // move the event to shared memory to be sent
      ntry = 0;
      while(shm_t3.Ubuf[(*shm_t3.size)*(*shm_t3.next_write)] != 0 &&ntry<10) {//danger! infinite loop
        ntry++;
        usleep(1000); // wait for buffer to be free
      }
      if(ntry >= 10 &&shm_t3.Ubuf[(*shm_t3.size)*(*shm_t3.next_write)] != 0 ){
        printf("T3: No buffer, loosing data\n");
      }else{
        memcpy((void *)&(shm_t3.Ubuf[(*shm_t3.size)*(*shm_t3.next_write)+1]),(void *)t3list,2*t3list[0]);
        shm_t3.Ubuf[(*shm_t3.size)*(*shm_t3.next_write)] = 3; // to be read by du and eb, thus will be 3 (=1+2)!!
        *shm_t3.next_write = *shm_t3.next_write + 1;
        if(*shm_t3.next_write >= *shm_t3.nbuf) *shm_t3.next_write = 0;
      }
      t3event++;
    }
  }
}

void t3_initialize()
{
  int i,j;
  int Narray = sqrt(MAXDU);
  
  for(i=0;i<MAXDU;i++){
    statlist[i] = 5100+i; //for now all hardcoded dummies
    posx[i] = 1000*(i%Narray);
    posy[i] = 1000*(i/Narray);
    for(j=0;j<=i;j++){
      ctimes[i][j] = 100+(int)(GIGA*
        sqrt((posx[i]-posx[j])*(posx[i]-posx[j])+(posy[i]-posy[j])*(posy[i]-posy[j]))/3E8);
       ctimes[j][i] = ctimes[i][j];
    }
  }
}

/**
 void t3_main()
 
 infinite loop: gett2 and maket3
 */
void t3_main()
{
  t3_initialize();
  while(1) {
    t3_gett2();
    if(shm_t3.Ubuf[(*shm_t3.size)*(*shm_t3.next_write)] == 0 ) t3_maket3();
    usleep(100000);
  }
}
