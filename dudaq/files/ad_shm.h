/*** \file ad_shm.h
DAQ shared memory definitions
Version:1.0
Date: 17/2/2020
Author: Charles Timmermans, Nikhef/Radboud University

Altering the code without explicit consent of the author is forbidden
 ***/
#include <stdint.h>
#include <unistd.h>
#include<sys/types.h>
#include<sys/ipc.h>
#include<sys/shm.h>

typedef struct{
  int shmid; /**< shared memory identifier */
  int *next_read;  /**< pointer to the next block to read out in Ubuf */
  int *next_readb; /**< pointer to the next block to read out in Ubuf by a second process */
  int *next_write; /**< Pointer to the next position where to write a block of data */
  int *nbuf; /**< number of data blocks in the circular buffer */
  int *size; /**< size (in shorts) of a data block */
  char *buf; /**< pointer to the buffer */
  uint16_t *Ubuf; /**< our data is in uint16, so a uint16 pointer */
}shm_struct;

int ad_shm_create(shm_struct *ptr,int nbuf,int size);
void ad_shm_delete(shm_struct *ptr);
