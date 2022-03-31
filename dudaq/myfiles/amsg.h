/// @file
/// @brief General definitions used in the AERA DAQ
/// @author C. Timmermans, Nikhef/RU
#ifndef _AMSG_H_
#define _AMSG_H_


#include <stdint.h>

#define DU_BOOT     0      //!< Message tag as defined by DAQ group
#define DU_UPLOAD   1      //!< Message tag as defined by DAQ group
#define DU_RESET    2      //!< Message tag as defined by DAQ group
#define DU_INITIALIZE 3    //!< Message tag as defined by DAQ group
#define DU_SET      4      //!< Message tag as defined by DAQ group
#define DU_GET      5      //!< Message tag as defined by DAQ group
#define DU_MONITOR  6      //!< Message tag as defined by DAQ group
#define DU_START    7      //!< Message tag as defined by DAQ group
#define DU_STOP     8      //!< Message tag as defined by DAQ group
#define DU_T2       9      //!< Message tag as defined by DAQ group
#define DU_GETEVENT 10     //!< Message tag as defined by DAQ group
#define DU_NO_EVENT 11     //!< Message tag as defined by DAQ group
#define DU_EVENT    12     //!< Message tag as defined by DAQ group
#define DU_GETMEM   13     //!< Message tag as defined by DAQ group
#define DU_SETMEM   14     //!< Message tag as defined by DAQ group
#define DU_SETBIT   15     //!< Message tag as defined by DAQ group
#define DU_CLRBIT   16     //!< Message tag as defined by DAQ group
#define DU_GETFILE  17     //!< Message tag as defined by DAQ group
#define DU_SENDFILE 18     //!< Message tag as defined by DAQ group
#define DU_EXEC     19     //!< Message tag as defined by DAQ group
#define DU_CONNECT  20     //!< Message tag as defined by DAQ group
#define DU_LISTEN   21     //!< Message tag as defined by DAQ group
#define DU_SCRIPT   22     //!< Message tag as defined by DAQ group
#define DU_GET_MINBIAS_EVENT           29 //!< Message tag as defined by DAQ group
#define DU_GET_RANDOM_EVENT            32 //!< Message tag as defined by DAQ group
#define T3_EVENT_REQUEST_LIST 201  //!< Message tag as defined by DAQ group
#define GUI_UPDATE_DB 401          //!< Message tag as defined by DAQ group
#define GUI_INITIALIZE 402         //!< Message tag as defined by DAQ group
#define GUI_START_RUN 403          //!< Message tag as defined by DAQ group
#define GUI_STOP_RUN 404           //!< Message tag as defined by DAQ group
#define GUI_DELETE_RUN 406         //!< Message tag as defined by DAQ group
#define GUI_BC_SETATTENUATION  450  //!< Message tag as defined by DAQ group
#define GUI_BC_SWITCHIO  451        //!< Message tag as defined by DAQ group
#define GUI_BC_SETMODE  452         //!< Message tag as defined by DAQ group

#define GRND1 0x5247    //!< End word1 for a message (GR)
#define GRND2 0x444E    //!< End word 2 for a message (ND)
#define ALIVE 9998      //!< Alive command sent by central DAQ
#define ALIVE_ACK 9999  //!< Acknowledge alive, sent by DU

#define DU_HWNL 1       //!< Dutch hardware

typedef struct{
  uint16_t length;      //!< Message length, not including the length word itself
  uint16_t tag;         //!< Message type
  uint16_t body[];      //!< the body of a message
}AMSG;

#define AMSG_OFFSET_LENGTH  0 //!< offset for message length wrt start of msg
#define AMSG_OFFSET_TAG     1 //!< offset for message tag wrt start of msg
#define AMSG_OFFSET_BODY    2 //!< offset for message body wrt start of msg

#define DUPOS(a)       (a&0xff)       //!< detector unit position from DU_id
#define DUHWTYPE(a)    ((a>>8)&0x7)   //!< hardware type from DU_id
#define DUHWVERSION(a) ((a>>11)&0x1f) //!< hardware version from DU_id

#define T2FILL(a,b,c) {a->NS1=(b&0xff0000)>>16;a->NS2=(b&0xff00)>>8;a->NS3=(b&0xff);a->ADC=c&0xff;}
//!< macro to fill T2 block pointed to by a with ns b and ADC value c
#define T2NSEC(a) ((((unsigned int)(a->NS3))<<6)+(((unsigned int)(a->NS2))<<14)+(((unsigned int)(a->NS1))<<22))
//!< macro to obtain the ns from the T2 block
#define T2ADC(a)  (a->ADC)
//!< macro to obtain the ADC value from the T2 block

typedef struct{
  unsigned char NS1; //!< Bits 31-24 of T2 subsecond
  unsigned char NS2; //!< Bits 23-16 of T2 subsecond
  unsigned char NS3; //!< Bits 15-8 of T2 subsecond
  unsigned char ADC; //!< additional T2 info
}T2SSEC;

#define T0(a) ((a[1]<<16)+a[0]) //!< get a time from 2  shorts

typedef struct{
  uint16_t DU_id;   //!< identifier of Detector Unit
  uint16_t t0[2];   //!< Second marker (gps time)
  T2SSEC t2ssec[];  //!< subsecond markers for each trigger
}T2BODY;

#define T3STATIONSIZE 3 //!< size of T3 request message
#define T3STATFILL(a,b,c,d) {a->DU_id=b;a->sec=c&0xff;a->NS1=(d&0xff0000)>>16;a->NS2=(d&0xff00)>>8;a->NS3=(d&0xff);}
//!< fill T3 request message (a) with ID (b), second (c) and subsecond(d)

typedef struct{
  uint16_t DU_id;     //!< identifier of Detector Unit
  unsigned char sec;  //!< gps second information
  unsigned char NS1;  //!< Bits 31-24 of nanosecond
  unsigned char NS2;  //!< Bits 23-16 of nanosecond
  unsigned char NS3;  //!< Bits 15-8 of nanosecond
}T3STATION;

typedef struct{
  uint16_t event_nr;       //!< T3 event number
  T3STATION  t3station[];  //!< Information required for LS
}T3BODY;


#define USE_EVENTBODY_VERSION //!< An eventbody version was added to the event body
#define EVENTBODY_VERSION		3 //!< The version number is 3

#define MIN_EVHEADER_LENGTH 12 //!< The minimal length of the event header


typedef struct{
  uint16_t length;
  uint16_t event_nr;
  uint16_t DU_id;
  uint16_t header_length;
  uint32_t GPSseconds;
  uint32_t GPSnanoseconds;
  uint16_t trigger_flag;
  uint16_t trigger_pos;
  uint16_t sampling_freq;
  uint16_t channel_mask;
  uint16_t ADC_resolution;
  uint16_t tracelength;
  uint16_t version;				// use a version information, in use since ??.02.2011
  uint16_t info_ADCbuffer[];
}EVENTBODY;

/* Halfword offset of EVENTBODY fields assuming struct is packed */
/* Necessary to avoid alignment problems on ARM */
/* These need to be updated if struct is changed */
#define EB_OFFSET_LENGTH       0
#define EB_OFFSET_EVENT_NR     1
#define EB_OFFSET_DU_ID        2
#define EB_OFFSET_HDR_LENGTH   3
#define EB_OFFSET_GPSSEC       4
#define EB_OFFSET_GPSNSEC      6
#define EB_OFFSET_TRIG_FLAG    8
#define EB_OFFSET_TRIG_POS     9
#define EB_OFFSET_SAMP_FREQ   10
#define EB_OFFSET_CHAN_MASK   11
#define EB_OFFSET_ADC_RES     12
#define EB_OFFSET_TRACELENGTH 13
#ifdef USE_EVENTBODY_VERSION
#define EB_OFFSET_VERSION     14
#define EB_OFFSET_ADCBUFFER   15
#else
#define EB_OFFSET_ADCBUFFER   14
#endif

typedef struct{
  uint16_t DU_id;
  uint16_t event_nr;
  unsigned char sec;
  unsigned char NS1;
  unsigned char NS2;
  unsigned char NS3;
}du_geteventbody;

typedef struct{
  uint16_t event_nr;
  uint16_t DU_id;
}ls_no_eventbody;
#else
#endif
