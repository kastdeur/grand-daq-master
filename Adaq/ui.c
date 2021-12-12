/***
User interface (command line) related tasks
Version:1.0
Date: 17/2/2020
Author: Charles Timmermans, Nikhef/Radboud University

Altering the code without explicit consent of the author is forbidden
 ***/
#include<stdio.h>
#include<string.h>
#include<strings.h>
#include <unistd.h>
#include "amsg.h"
#include "Adaq.h"

uint16_t cmdlist[CMDSIZE];

/**
 void cmd_run(uint16_t mode)
 
 continuous loop waiting for a buffer to free up
 put the command into the command shared memory
 run command should be read bu the du-interface and event-builder
 update the pointer to the next empty buffer (circular!)
 */
void cmd_run(uint16_t mode)
{
  cmdlist[0] = 3;
  cmdlist[1] = mode;
  cmdlist[2]=0; // all local stations
  while(shm_cmd.Ubuf[(*shm_cmd.size)*(*shm_cmd.next_write)] != 0) {//possible problem!
    printf("UI: Wait for buffer \n");
    usleep(1000); // wait for buffer to be free
  }
  printf("UI: Writing to SHM\n");
  memcpy((void *)&(shm_cmd.Ubuf[(*shm_cmd.size)*(*shm_cmd.next_write)+1]),(void *)cmdlist,2*cmdlist[0]);
  shm_cmd.Ubuf[(*shm_cmd.size)*(*shm_cmd.next_write)] = 3; // to be read by du(1)+eb(2)!
  *shm_cmd.next_write = *shm_cmd.next_write + 1;
  if(*shm_cmd.next_write >= *shm_cmd.nbuf) *shm_cmd.next_write = 0;
}

/**
void send_cmd(uint16_t mode,uint16_t istat)
 
continuous loop waiting for a buffer to free up
put the command into the command shared memory
run command should be read bu the du-interface
update the pointer to the next empty buffer (circular!)
*/
void send_cmd(uint16_t mode,uint16_t istat)
{
  cmdlist[0] = 3;
  cmdlist[1] = mode;
  cmdlist[2]=istat; 
  while(shm_cmd.Ubuf[(*shm_cmd.size)*(*shm_cmd.next_write)] != 0) {//possible problem
    printf("UI: Wait for buffer \n");
    usleep(1000); // wait for buffer to be free
  }
  printf("UI: Writing to SHM\n");
  memcpy((void *)&(shm_cmd.Ubuf[(*shm_cmd.size)*(*shm_cmd.next_write)+1]),(void *)cmdlist,2*cmdlist[0]);
  shm_cmd.Ubuf[(*shm_cmd.size)*(*shm_cmd.next_write)] = 1; // to be read by du(1)
  *shm_cmd.next_write = *shm_cmd.next_write + 1;
  if(*shm_cmd.next_write >= *shm_cmd.nbuf) *shm_cmd.next_write = 0;
}

/**
 void ui_main()
 
prints prompt
continuous loop waiting for input on the command line. Sends the following commands to the DAQ:
 STOP_RUN
 START_RUN
 INITIALIZE
 */
void ui_main()
{
  char cmd[200];

  printf("Adaq >");
  while(fgets(cmd,199,stdin)){
    if(strncasecmp(cmd,"STOP_RUN",8) == 0) cmd_run(DU_STOP);
    if(strncasecmp(cmd,"START_RUN",9) == 0) cmd_run(DU_START);
    if(strncasecmp(cmd,"INITIALIZE",10) == 0) send_cmd(DU_INITIALIZE,0);
    printf("Adaq >");
  }
}
