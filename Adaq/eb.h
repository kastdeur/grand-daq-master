/***
Event Builder definitions
Version:1.0
Date: 18/2/2020
Author: Charles Timmermans, Nikhef/Radboud University

Altering the code without explicit consent of the author is forbidden
 ***/
#include <unistd.h>

#define EVENTVERSION 3

typedef struct{
  uint32_t length;
  uint32_t run_id;
  uint32_t run_mode;
  uint32_t file_id;
  uint32_t first_event_id;
  uint32_t first_event_time;
  uint32_t last_event_id;
  uint32_t last_event_time;
  uint32_t add1;
  uint32_t add2;
}FILEHDR;

typedef struct{
  uint32_t length;
  uint32_t run_id;
  uint32_t event_id;
  uint32_t t3_id;
  uint32_t first_DU;
  uint32_t seconds;
  uint32_t nanosec;
  uint16_t type;
  uint16_t version;
  uint32_t free;
  uint32_t event_free;
  uint32_t DU_count;
  uint16_t DUdata[];
}EVHDR;

