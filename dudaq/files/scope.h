/// @file scope.h
/// @brief Header file for NIKHEF scope v4.0
/// @author C. Timmermans, Nikhef/RU

/*
 * Header file for NIKHEF scope v4.0
 *
 * C. Timmermans
 * c.timmermans@hef.ru.nl
 *
 * NOT backward compatible with older versions!
 *>
 */

#include <stdint.h>

/*----------------------------------------------------------------------*/
#define SAMPLING_FREQ 200 //!< 200 MHz scope

/*----------------------------------------------------------------------*/
#define GPS_EPOCH_UNIX  315964800 //!< GPS to unix offst, not counting leap sec

/* Message start / end flags */
#define MSG_START    0x99 //!< start of fpga message
#define MSG_END      0x66 //!< end of fpga message


/* Time stamp offsets (within 7B time field) */

#define TIME_YEAR       0
#define TIME_MON        2
#define TIME_DAY        3
#define TIME_HOUR       4
#define TIME_MIN        5
#define TIME_SEC        6


/*----------------------------------------------------------------------*/
/* Maxima / minima */

/* Maximum ADC size = 4 channels * max_samples/ch * 2 bytes/sample */
/* Maximum event size = header + ADC data + message end       */
#ifdef Fake
#define DATA_MAX_SAMP   10                       //!< Maximal trace length (samples)
#else
#define DATA_MAX_SAMP   4096                       //!< Maximal trace length (samples)
#endif
#define MAX_READOUT     (70 + DATA_MAX_SAMP*8 + 2) //!< Maximal raw event size

#define MIN_MSG_LEN     6                          //!< Minimal length of scope message

/*----------------------------------------------------------------------*/
/* Message definitions */
#define ID_PARAM_GPS          0x00
#define ID_PARAM_CTRL         0x01
#define ID_PARAM_WINDOWS      0x02
#define ID_PARAM_COMRES       0x03
#define ID_PARAM_SPI          0x04
#define ID_PARAM_TRIG5        0x05
#define ID_PARAM_CH1          0x08
#define ID_PARAM_CH2          0x09
#define ID_PARAM_CH3          0x0A
#define ID_PARAM_CH4          0x0B
#define ID_PARAM_TRIG1        0x0C
#define ID_PARAM_TRIG2        0x0D
#define ID_PARAM_TRIG3        0x0E
#define ID_PARAM_TRIG4        0x0F
#define ID_PARAM_FILT11       0x10
#define ID_PARAM_FILT12       0x11
#define ID_PARAM_FILT21       0x12
#define ID_PARAM_FILT22       0x13
#define ID_PARAM_FILT31       0x14
#define ID_PARAM_FILT32       0x15
#define ID_PARAM_FILT41       0x16
#define ID_PARAM_FILT42       0x17
#define ID_PARAM_PPS          0xC4
#define ID_PARAM_EVENT        0xC0
#define ID_PARAM_ERROR        0xCE
#define ID_PARAM_NOP          0x66

#define ID_GPS_BCNT           2 //wil be 46
#define ID_GPS_VERSION        4
#define ID_GPS_TIME           8
#define ID_GPS_STATUS         15
#define ID_GPS_LONG           16
#define ID_GPS_LAT            24
#define ID_GPS_ALT            32
#define ID_GPS_TEMP           40  
#define ID_GPS_END            44

#define ID_CTRL_BCNT          2 //bytecount will be 18 
#define ID_CTRL_CTRLREG       4
#define ID_CTRL_TRMASK        6
#define ID_CTRL_CHENABLE      8
#define ID_CTRL_TRIGDIV       9
#define ID_CTRL_COINCTIME    10
#define ID_CTRL_SPARE1       12
#define ID_CTRL_SPARE2       14
#define ID_CTRL_END          16

#define ID_WINDOWS_BCNT       2 //bytecount will be 22 
#define ID_WINDOWS_PRECH1     4
#define ID_WINDOWS_POSTCH1    6
#define ID_WINDOWS_PRECH2     8
#define ID_WINDOWS_POSTCH2   10
#define ID_WINDOWS_PRECH3    12
#define ID_WINDOWS_POSTCH3   14
#define ID_WINDOWS_PRECH4    16
#define ID_WINDOWS_POSTCH4   18
#define ID_WINDOWS_END       20

#define ID_COMRES_BCNT        2 //bytecount will be 8
#define ID_COMRES_CMD         4
#define ID_COMRES_RES         5
#define ID_COMRES_END         6

#define ID_SPI_BCNT           2 //bytecount will be 8
#define ID_SPI_ADDRESS        4
#define ID_SPI_DATA           5
#define ID_SPI_END            6

#define ID_TRIG_BCNT          2 //bytecount will be 18
#define ID_TRIG_THR1          4
#define ID_TRIG_THR2          6
#define ID_TRIG_TPREV         8
#define ID_TRIG_TPER          9
#define ID_TRIG_TCMAX        10
#define ID_TRIG_NCMAX        11
#define ID_TRIG_NCMIN        12
#define ID_TRIG_QMAX         13
#define ID_TRIG_QMIN         14
#define ID_TRIG_SPARE        15
#define ID_TRIG_END          16

#define ID_CH_BCNT            2 //bytecount will be 18
#define ID_CH_GAIN            4
#define ID_CH_OFFSET          6
#define ID_CH_INTTIME         7
#define ID_CH_BASEMAX         8
#define ID_CH_BASEMIN        10
#define ID_CH_PMV            12
#define ID_CH_FILTER         13  
#define ID_CH_SPARE2         14
#define ID_CH_END            16

#define ID_FILT_BCNT          2 //bytecount will be 22
#define ID_FILT_A1            4
#define ID_FILT_A2            6
#define ID_FILT_B1            8
#define ID_FILT_B2           10
#define ID_FILT_B3           12
#define ID_FILT_B4           14
#define ID_FILT_B5           16
#define ID_FILT_B6           18
#define ID_FILT_END          20

/*----------------------------------------------------------------------*/
/* Control register bits */
#define CTRL_SEND_EN    (1 << 0)
#define CTRL_PPS_EN     (1 << 1)
#define CTRL_FULLSCALE  (1 << 2)
#define CTRL_FILTERREAD (1 << 3)
#define CTRL_THRESHMODE (1 << 4)
#define CTRL_GPS_PROG   (1 << 5)    
#define CTRL_FAKE_ADC   (1 << 6)
#define CTRL_DCO_EDGE   (1 << 7)
#define CTRL_FILTER1    (1 <<  8)
#define CTRL_FILTER2    (1 <<  9)
#define CTRL_FILTER3    (1 << 10)
#define CTRL_FILTER4    (1 << 11)

#define TRIG_POW      (1 << 3)
#define TRIG_EXT      (1 << 4)
#define TRIG_10SEC    (1 << 5)
#define TRIG_CAL      (1 << 6)
#define TRIG_CH1CH2   (1 << 7)


// general aera event types
#define SELF_TRIGGERED  0x0001
#define EXT_EL_TRIGGER  0x0002
#define CALIB_TRIGGER   0x0004
#define EXT_T3_TRIGGER  0x0008  // by SD, Gui ...
#define RANDOM_TRIGGER  0x0010
#define TRIGGER_T3_EXT_SD          0x0100
#define TRIGGER_T3_EXT_GUI         0x0200
#define TRIGGER_T3_EXT_FD          0x0400
#define TRIGGER_T3_EXT_HEAT        0x0800
#define TRIGGER_T3_MINBIAS         0x1000
#define TRIGGER_T3_EXT_AERALET     0x2000
#define TRIGGER_T3_EXT_AIRPLANE    0x4000
#define TRIGGER_T3_RANDOM          0x8000

/*----------------------------------------------------------------------*/
/* PPS definition */
#define PPS_BCNT        2 //332 bytes
#define PPS_TIME        4
#define PPS_STATUS     11 
#define PPS_CTP        12
#define PPS_QUANT      16
#define PPS_FLAGS      20
#define PPS_RATE       24
#define PPS_GPS        26
#define PPS_CTRL       66
#define PPS_WINDOWS    78
#define PPS_CH1        94
#define PPS_CH2       106
#define PPS_CH3       118
#define PPS_CH4       130
#define PPS_TRIG1     142
#define PPS_TRIG2     154
#define PPS_TRIG3     166
#define PPS_TRIG4     178
#define PPS_FILT11    190
#define PPS_FILT12    206
#define PPS_FILT21    222
#define PPS_FILT22    238
#define PPS_FILT31    254
#define PPS_FILT32    270
#define PPS_FILT41    286
#define PPS_FILT42    302
#define PPS_END       318
#define PPS_LENGTH    (320) //!< Total size of the PPS message

/*----------------------------------------------------------------------*/
/* Event definition */
#define EVENT_BCNT        2 //bytecount
#define EVENT_TRIGMASK    4
#define EVENT_GPS         6
#define EVENT_STATUS     13 
#define EVENT_CTD        14
#define EVENT_LENCH1     18
#define EVENT_LENCH2     20
#define EVENT_LENCH3     22
#define EVENT_LENCH4     24
#define EVENT_THRES1CH1  26
#define EVENT_THRES2CH1  28
#define EVENT_THRES1CH2  30
#define EVENT_THRES2CH2  32
#define EVENT_THRES1CH3  34
#define EVENT_THRES2CH3  36
#define EVENT_THRES1CH4  38
#define EVENT_THRES2CH4  40
#define EVENT_CTRL       42
#define EVENT_WINDOWS    54
#define EVENT_ADC        70

/*----------------------------------------------------------------------*/
/* Error Definition */
#define ERROR_BCNT 2
#define ERROR_ID   4
#define ERROR_END  6

/*----------------------------------------------------------------------*/
/* Calibration states */
#define CAL_END         0
#define CAL_OFFSET      1
#define CAL_GAIN        2

/* Calibration targets */
#define CAL_OFFSET_TARG      0
#define CAL_OFFSET_WIDTH     2

#define CAL_OFFSET_OTARG      0
#define CAL_OFFSET_OWIDTH     2

#define CAL_GAIN_TARG      -7250
#define CAL_GAIN_WIDTH      2

#define CAL_GAIN_OTARG      -7000
#define CAL_GAIN_OWIDTH      12

/*----------------------------------------------------------------------*/
/* Trigger rate divider base frequency */
#define TRIG_RATE_BASE_HZ 4800  //!< maximal fpga generated trigger frequency

/*----------------------------------------------------------------------*/
/* Macros */
#define FIRMWARE_VERSION(x) (10*((x>>20)&0xf)+((x>>16)&0xf)) //!< Calculation of Firmware version number
#define FIRMWARE_SUBVERSION(x)   (10*((x>>12)&0xf)+((x>>9)&0x7)) //!< Calculation of subversion number
#define SERIAL_NUMBER(x)    (100*((x>>8)&0x1)+10*((x>>4)&0xf)+((x>>0)&0xf)) //!< serial number of digital board
#define ADC_RESOLUTION(x) (x>79 ? 14 : 12) //!< ADC resolution depends on board number
/*
  buffer definitions for the scope readout process.
 */
#define DEV_READ_BLOCK 100      //!< fpga Device read blocksize, in Bytes

#define MAX_RATE 1000            //!< maximum event rate, in Hz
#ifdef Fake
#define BUFSIZE 3000            //!< store up to 10 events in circular buffer
#else
#define BUFSIZE 3000            //!< store up to 3000 events in circular buffer
#endif
#define GPSSIZE 35              //!< buffer upto 35 GPS seconds info in circular buffer
#define MAXT3 200               //!< 200 T3 events in circular cuffer

// next: what did we read from the scope?

#define SCOPE_PARAM 1          //!< return code for reading a parameter list
#define SCOPE_EVENT 2          //!< return code for reading an event
#define SCOPE_GPS   3          //!< return code for reading a PPS message

#define PARAM_NUM_LIST 0x18     //!< Number of parameter lists for the fpga
#define PARAM_LIST_MAXSIZE 46   //!< maximal listsize 46 bytes


typedef struct
{
  uint16_t event_nr;        //!< an event number
  uint32_t ts_seconds;      //!< second marker
  uint32_t t3calc;          //!< was the T3 time calculated (1/0)
  uint32_t t3_nanoseconds;  //!< proper timing
  uint32_t t2_nanoseconds;  //!< rough timing for t2 purposes only
  uint32_t CTD;             //!< clock tick of the trigger
  uint32_t trig_flag;       //!< trigger flag 
  uint32_t evsize;          //!< size of the event
  float quant1;             //!< quant error previous PPS
  float quant2;             //!< quant error next PPS
  uint32_t CTP;             //!< Number of clock ticks between PPS pulses
  int16_t sync;             //!< Positive or Negative clock edge
  uint8_t buf[MAX_READOUT]; //!< raw data buffer
} EV_DATA;

typedef struct
{
  uint32_t ts_seconds;      //!< time marker in GPS sec
  uint32_t CTP;             //!< clock ticks since previous time marker
  uint32_t SCTP;            //!< clock ticks per second
  int8_t sync;              //!< clock-edge of timestamp
  float quant;              //!< deviation from true second
  double clock_tick;        //!< time between clock ticks
  uint16_t rate[4];         //!< event rate in one second for all channels
  uint8_t buf[PPS_LENGTH];  //!< raw data buffer
} GPS_DATA;


// the routines

void scope_set_parameters(uint16_t *data,int to_shadow);
void scope_write(uint8_t *buf, int32_t len);
int scope_raw_read(uint8_t *bf, int32_t size);
int scope_open();
void scope_get_parameterlist(uint8_t list);
void scope_reset();
void scope_start_run();
void scope_stop_run();
void scope_reboot();
void scope_print_parameters(int32_t list);
void scope_print_pps(uint8_t *buf);
void scope_print_event(uint8_t *buf);
void scope_initialize();
void scope_init_shadow();
void scope_fill_shadow(int8_t *ppsbuf);
int8_t *scope_get_shadow(int32_t list);
int scope_read(int ioff);
int scope_read_pps();
int scope_read_event(int32_t ioff);
int scope_read_error();
int scope_no_run_read();
int scope_run_read();
int scope_cal_read();
int scope_calc_t3nsec(EV_DATA *buf);
int scope_calc_evnsec();
void scope_calibrate();
void scope_initialize_calibration();
int scope_calibrate_evt();
void scope_close();

//
