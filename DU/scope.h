/// @file scope.h
/// @brief Header file for GP300 scope
/// @author C. Timmermans, Nikhef/RU

/*
 * Header file for GP300 scope
 *
 * C. Timmermans
 * c.timmermans@science.ru.nl
 *
 * NOT backward compatible with older versions!
 *>
 */

#include <stdint.h>

/*----------------------------------------------------------------------*/
#define SAMPLING_FREQ 500 //!< 500 MHz scope

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
/* Register Definitions*/
#define Reg_Dig_Control       0x000
#define Reg_Trig_Enable       0x002
#define Reg_TestPulse_ChRead  0x004
#define Reg_Time_Common       0x006
#define Reg_Inp_Select        0x008
#define Reg_Spare_A           0x00A
#define Reg_Spare_B           0x00C
#define Reg_Spare_C           0x00E
#define Reg_Time1_Pre         0x010
#define Reg_Time1_Post        0x012
#define Reg_Time2_Pre         0x014
#define Reg_Time2_Post        0x016
#define Reg_Time3_Pre         0x018
#define Reg_Time3_Post        0x01A
#define Reg_Time4_Pre         0x01C
#define Reg_Time4_Post        0x01E
#define Reg_ADC1_Gain         0x020
#define Reg_ADC1_IntOff       0x022
#define Reg_ADC1_BaseMa       0x024
#define Reg_ADC1_BaseMi       0x026
#define Reg_ADC1_SpareA       0x028
#define Reg_ADC1_SpareB       0x02A
#define Reg_ADC2_Gain         0x02C
#define Reg_ADC2_IntOff       0x02E
#define Reg_ADC2_BaseMax      0x030
#define Reg_ADC2_BaseMin      0x032
#define Reg_ADC2_SpareA       0x034
#define Reg_ADC2_SpareB       0x036
#define Reg_ADC3_Gain         0x038
#define Reg_ADC3_IntOff       0x03A
#define Reg_ADC3_BaseMax      0x03C
#define Reg_ADC3_BaseMin      0x03E
#define Reg_ADC3_SpareA       0x040
#define Reg_ADC3_SpareB       0x042
#define Reg_ADC4_Gain         0x044
#define Reg_ADC4_IntOff       0x046
#define Reg_ADC4_BaseMax      0x048
#define Reg_ADC4_BaseMin      0x04A
#define Reg_ADC4_SpareA       0x04C
#define Reg_ADC4_SpareB       0x04E
#define Reg_Trig1_ThresA      0x050
#define Reg_Trig1_ThresB      0x052
#define Reg_Trig1_Times       0x054
#define Reg_Trig1_Tmax        0x056
#define Reg_Trig1_Nmin        0x058
#define Reg_Trig1_Qmin        0x05A
#define Reg_Trig2_ThresA      0x05C
#define Reg_Trig2_ThresB      0x05E
#define Reg_Trig2_Times       0x060
#define Reg_Trig2_Tmax        0x062
#define Reg_Trig2_Nmin        0x064
#define Reg_Trig2_Qmin        0x066
#define Reg_Trig3_ThresA      0x068
#define Reg_Trig3_ThresB      0x06A
#define Reg_Trig3_Times       0x06C
#define Reg_Trig3_Tmax        0x06E
#define Reg_Trig3_Nmin        0x070
#define Reg_Trig3_Qmin        0x072
#define Reg_Trig4_ThresA      0x074
#define Reg_Trig4_ThresB      0x076
#define Reg_Trig4_Times       0x078
#define Reg_Trig4_Tmax        0x07A
#define Reg_Trig4_Nmin        0x07C
#define Reg_Trig4_Qmin        0x07E
#define Reg_FltA1_A1          0x080
#define Reg_FltA1_A2          0x082
#define Reg_FltA1_B1          0x084
#define Reg_FltA1_B2          0x086
#define Reg_FltA1_B3          0x088
#define Reg_FltA1_B4          0x08A
#define Reg_FltA1_B5          0x08C
#define Reg_FltA1_B6          0x08E
#define Reg_FWStatus          0x1C0
#define Reg_GenStatus         0x1D0
#define Reg_GenControl        0x1D4
#define Reg_Data              0x1D8
#define Reg_TestTrace         0x1DC
#define Reg_Rate              0x1E0
#define Reg_End               0x1FC
/* Message definitions  Legacy*/

#define ID_PARAM_PPS          0xC4
#define ID_PARAM_EVENT        0xC0

#define ID_GPS_VERSION        4

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
void scope_write(uint16_t *buf);
int scope_raw_read(uint16_t *bf);
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
