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
#define ADC_RESOLUTION 14

/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
/* Maxima / minima */

/* Maximum ADC size = 4 channels * max_samples/ch * 2 bytes/sample */
/* Maximum event size = header + ADC data + message end       */
#define DATA_MAX_SAMP   8192                       //!< Maximal trace length (samples)

#define MAX_READOUT     (256 + DATA_MAX_SAMP*4) //!< Maximal raw event size

/*----------------------------------------------------------------------*/
#define TDAQ_BASE             0x80000000
/* Register Definitions*/
#define Reg_Dig_Control       0x000
#define Reg_Trig_Enable       0x002
#define Reg_TestPulse_ChRead  0x004
#define Reg_Time_Common       0x006
#define Reg_Inp_Select        0x008
#define Reg_Battery_Off       0x00A
#define Reg_Battery_On        0x00C
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

/*----------------------------------------------------------------------*/
/* Control register bits */
#define CTRL_SEND_EN    (1 << 0)
#define CTRL_PPS_EN     (1 << 1)
#define CTRL_FLTR_EN    (1 << 3)
#define CTRL_FAKE_ADC   (1 << 6)
#define CTRL_FILTER1    (1 <<  8)
#define CTRL_FILTER2    (1 <<  9)
#define CTRL_FILTER3    (1 << 10)
#define CTRL_FILTER4    (1 << 11)
#define CTRL_AUTOBOOT   (1 << 15)

#define GENSTAT_PPSFIFO  (1<<24)
#define GENSTAT_EVTFIFO  (1<<25)
#define GENSTAT_DMAFIFO  (1<<26)
#define GENCTRL_EVTREAD  (1<<25)

// general event types
#define TRIGGER_T3_MINBIAS 0x1000
#define TRIGGER_T3_RANDOM  0x8000

/*----------------------------------------------------------------------*/
/* PPS definition */
#define MAGIC_PPS     0xFACE
#define WCNT_PPS      32
#define PPS_MAGIC       1
#define PPS_TRIG_PAT    2
#define PPS_TRIG_RATE   3
#define PPS_CTD         4
#define PPS_CTP         6
#define PPS_OFFSET      8
#define PPS_LEAP       10
#define PPS_STATFLAG   11
#define PPS_CRITICAL   12
#define PPS_WARNING    13
#define PPS_YEAR       14
#define PPS_DAYMONTH   15
#define PPS_MINHOUR    16
#define PPS_STATSEC    17
#define PPS_LONGITUDE  18
#define PPS_LATITUDE   22
#define PPS_ALTITUDE   26
#define PPS_TEMPERATURE 30

/*----------------------------------------------------------------------*/
/* Event definition */
#define MAGIC_EVT         0xADC0
#define HEADER_EVT        256
#define FORMAT_EVT        1
#define EVT_LENGTH        0 // nr of int16 words
#define EVT_ID            1 // nr of int16 words
#define EVT_HARDWARE      2
#define EVT_HDRLEN        3 //256 (int16 words in the header)
#define EVT_SECOND        4
#define EVT_NANOSEC       6
#define EVT_TRIGGERPOS    8
#define EVT_T3FLAG        9
#define EVT_ATM_TEMP      17
#define EVT_ATM_PRES      18
#define EVT_ATM_HUM       19
#define EVT_ACCEL_X       20
#define EVT_ACCEL_Y       21
#define EVT_ACCEL_Z       22
#define EVT_BATTERY       23
#define EVT_VERSION       24
#define EVT_MSPS          25
#define EVT_ADC_RES       26
#define EVT_INP_SELECT    27
#define EVT_CH_ENABLE     28
#define EVT_TOT_SAMPLES   29
#define EVT_CH1_SAMPLES   30
#define EVT_CH2_SAMPLES   31
#define EVT_CH3_SAMPLES   32
#define EVT_CH4_SAMPLES   33
#define EVT_TRIG_PAT      34
#define EVT_TRIG_RATE     35
#define EVT_CTD           36
#define EVT_CTP           38
#define EVT_PPS_OFFSET    40
#define EVT_LEAP          42
#define EVT_GPS_STATFLAG  43
#define EVT_GPS_CRITICAL  44
#define EVT_GPS_WARNING   45
#define EVT_YEAR          46
#define EVT_DAYMONTH      47
#define EVT_MINHOUR       48
#define EVT_STATSEC       49
#define EVT_LONGITUDE     50
#define EVT_LATITUDE      54
#define EVT_ALTITUDE      58
#define EVT_GPS_TEMP      62
#define EVT_CTRL          64
#define EVT_WINDOWS       72
#define EVT_CHANNEL       80
#define EVT_TRIGGER       104
#define EVT_FILTER1       128
#define EVT_FILTER2       160
#define EVT_FILTER3       192
#define EVT_FILTER4       224

/*----------------------------------------------------------------------*/

/*
  buffer definitions for the scope readout process.
 */

#define BUFSIZE 3000            //!< store up to 3000 events in circular buffer

#define GPSSIZE 35              //!< buffer upto 35 GPS seconds info in circular buffer
#define MAXT3 20               //!< 200 T3 events in circular cuffer

// next: what did we read from the scope?

#define SCOPE_EVENT 2          //!< return code for reading an event
#define SCOPE_GPS   3          //!< return code for reading a PPS message

typedef struct
{
  uint32_t ts_seconds;
  uint32_t ts_nanoseconds;
  uint16_t event_nr;
  uint16_t trigmask;
}TS_DATA; //timestamps

typedef struct
{
  uint32_t ts_seconds;      //!< time marker in GPS sec
  uint16_t data[WCNT_PPS];  //! all data read in PPS
} GPS_DATA;


// the routines

void scope_raw_write(uint32_t reg_addr, uint32_t value);
void scope_flush();
int scope_open();
void scope_close();
void scope_reset();
void scope_start_run();
void scope_stop_run();
void scope_set_parameters(uint32_t reg_addr, uint32_t value,uint32_t to_shadow);
void scope_reboot();
void scope_copy_shadow();
void scope_init_shadow();
void scope_initialize();
void scope_create_memory();
int scope_read_event(int32_t ioff);
int scope_read_pps();
int scope_read(int ioff);
int scope_no_run_read();
int scope_run_read();
void scope_event_to_shm(uint16_t evnr,uint16_t trflag,uint16_t sec,uint32_t ssec);
//
