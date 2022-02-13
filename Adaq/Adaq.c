/// @file Adaq.c
/// @brief DAQ Main project
/// @author C. Timmermans, Nikhef/RU

/***
DAQ Main project
Version:1.0
Date: 17/2/2020
Author: Charles Timmermans, Nikhef/Radboud University

Altering the code without explicit consent of the author is forbidden
 ***/
#include <sys/types.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#define _MAINDAQ    // this is the main program
#include "Adaq.h"

#define DU_PORT 5001

pid_t pid_du,pid_t3,pid_eb,pid_ui,pid_gui;
uint8_t stop_process=0;
int idebug = 0;

void du_main();
void t3_main();
void eb_main();
void ui_main();
void gui_main();

void remove_shared_memory();

char *configfile;


/**
void r clean_stop (int signum)
 removes all attached shared memories and remove all spawned processes
*/
void clean_stop (int signum)
{
  remove_shared_memory();
  stop_process = 1;
  kill(pid_du,9);
  kill(pid_t3,9);
  kill(pid_eb,9);
  kill(pid_ui,9);
  kill(pid_gui,9);
}

/**
pid_t ad_spawn_du()
creates a new instance of the interface to the detector units
*/
pid_t ad_spawn_du()
{
  pid_t i;
  printf("Spawning new DU\n");
  if((i = fork()) == 0) du_main();
  return(i);
}

/**
pid_t ad_spawn_t3()
creates a new instance of the T3 maker
*/
pid_t ad_spawn_t3()
{
  pid_t i;
  printf("Spawning new T3\n");
  if((i = fork()) == 0) t3_main();
  return(i);
}

/**
pid_t ad_spawn_eb()
creates a new instance of the event builder
*/
pid_t ad_spawn_eb()
{
  pid_t i;
  printf("Spawning new EB\n");
  if((i = fork()) == 0) eb_main();
  return(i);
}

/**
pid_t ad_spawn_ui()
creates a new instance of the command line interface
*/
pid_t ad_spawn_ui()
{
  pid_t i;
  if((i = fork()) == 0) ui_main();
  return(i);
}

/**
 pid_t ad_spawn_gui()
 creates a new instance of the interface to the graphical user interface
 */
pid_t ad_spawn_gui()
{
  pid_t i;
  if((i = fork()) == 0) gui_main();
  return(i);
}

/**
int ad_init_param(char *file)
 interprets the initialization file with keywords:
    DU ipaddress port
    EBRUN runnr
    EBSIZE maxevents --> maximum number of events in a file
    EBDIR datadir --> folder in which the data is stored
    T3RAND randfrac --> one T2 in every randfrac events is raised to a T3
 */
int ad_init_param(char *file)
{
    FILE *fp=NULL;
    char line[200];
    char key[20],ebkey[20];
    int i;
    
    fp = fopen(file,"r");
    if(fp == NULL) return(ERROR);
    tot_du = 0;
    while (line==fgets(line,199,fp) && tot_du < MAXDU) { // loop over all lines
        if(line[0] == '#') continue;
        sscanf(line,"%s",key);
        if(strcmp(key,"DU") == 0){
            if(sscanf(line,"%s %s %d",key,DUinfo[tot_du].DUip,&(DUinfo[tot_du].DUport)) ==3){
                if(DUinfo[tot_du].DUport!=DU_PORT)
                    DUinfo[tot_du].DUid = DUinfo[tot_du].DUport; //for a series of fake stations on same PC
                else sscanf(DUinfo[tot_du].DUip,"%d.%d.%d.%d", &i,&i,&i,&(DUinfo[tot_du].DUid));
                tot_du++;
            }
        }
        if(strcmp(key,"EBRUN") == 0){
            sscanf(line,"%s %d",key,&eb_run) ;
        }
        if(strcmp(key,"EBMODE") == 0){
            sscanf(line,"%s %d",key,&eb_run_mode);
        }
        if(strcmp(key,"EBSIZE") == 0){
            sscanf(line,"%s %d",key,&eb_max_evts);
        }
        if(strcmp(key,"EBDIR") == 0){
            sscanf(line,"%s %s",key,eb_dir);
        }
        if(strcmp(key,"T3RAND") == 0){
            sscanf(line,"%s %d",key,&t3_rand);
        }
    }
    fclose(fp);
    if(tot_du == MAXDU) printf("Warning: Reading out the maximal number of du stations:%d\n",MAXDU);
    return(NORMAL);
}
/**
 void ad_initialize(char *file)
 
 Initializes the main daq:
 - read parameters from the initialization file
 - create the T2 shared memory
 - create the T3 shared memory
 - create event shared memory
 - create a shared memory for commands
 - spawn the T3 maker, event builder, interface to the detector units, graphical and command line  interfaces to the user
 */
void ad_initialize(char *file)
{
    if(ad_init_param(file) == ERROR){
        printf("An error occured reading file %s\n",file);
        printf("Adaq does not know which du to connect to and exits\n");
        exit(-1);
    }
    if(ad_shm_create(&shm_t2,NT2BUF,T2SIZE) == ERROR){
        printf("An error occured creating the T2 buffer space\n");
        exit(-1);
    }
    if(ad_shm_create(&shm_t3,NT3BUF,T3SIZE) == ERROR){
        printf("An error occured creating the T3 buffer space\n");
        exit(-1);
    }
    if(ad_shm_create(&shm_eb,NEVBUF,EVSIZE) == ERROR){
        printf("An error occured creating the Event buffer space\n");
        exit(-1);
    }
    if(ad_shm_create(&shm_cmd,CMDBUF,CMDSIZE) == ERROR){
        printf("An error occured creating the Command buffer space\n");
        exit(-1);
    }
    pid_du = ad_spawn_du();
    pid_t3 = ad_spawn_t3();
    pid_eb = ad_spawn_eb();
    pid_ui = ad_spawn_ui();
    pid_gui = ad_spawn_gui();
}
/**
 void remove_shared_memory()
 
 removes all central DAQ shared memories: t2, t3, event builder and commands
 */
void remove_shared_memory()
{
    ad_shm_delete(&shm_t2);
    ad_shm_delete(&shm_t3);
    ad_shm_delete(&shm_eb);
    ad_shm_delete(&shm_cmd);
}

/**
 void ad_check_processes()
 
 check if the several spawned processes (interface to detector units, T3Maker, Eventbuilder, command line and graphical interface to the user) are still alive; if not it spawns a new instance
 */
void ad_check_processes()
{
    pid_t pid;
    int status;
    
    pid = waitpid (WAIT_ANY, &status, 0);
    if(pid == pid_du && stop_process ==0) pid_du = ad_spawn_du();
    if(pid == pid_t3 && stop_process ==0) pid_t3 = ad_spawn_t3();
    if(pid == pid_eb && stop_process ==0) pid_eb = ad_spawn_eb();
    if(pid == pid_ui && stop_process ==0) pid_ui = ad_spawn_ui();
    if(pid == pid_gui && stop_process ==0) pid_ui = ad_spawn_gui();}

/**
 int main(int argc, char **argv)
 
 Main program; first kills possible remnants of earlier instances. Afterwards it reads the configuration file and
 start the T3 maker, Event builder, Detector Unit communication and User interfaces. While the main DAQ process is alive it checks the state of the child processes.
 
 Possible parameter: name of the configuration file
 */
int main(int argc,char **argv)
{
    
    signal(SIGHUP,clean_stop);
    signal(SIGINT,clean_stop);
    signal(SIGTERM,clean_stop);
    signal(SIGABRT,clean_stop);
    signal(SIGKILL,clean_stop);
    
  if(argc>1) sscanf(argv[1],"%d",&idebug);
    if(argc>2) configfile = argv[2];
    else configfile = DEFAULT_CONFIGFILE;
    ad_initialize(configfile);
    stop_process = 0;
    while(stop_process == 0){
        ad_check_processes();
        sleep(1); // check every second
    }
}
