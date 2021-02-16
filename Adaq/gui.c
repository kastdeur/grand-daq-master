/***
User interface (gui) related tasks
Version:1.0
Date: 17/2/2020
Author: Charles Timmermans, Nikhef/Radboud University

Altering the code without explicit consent of the author is forbidden
 ***/
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/tcp.h>
#include <netinet/in.h>
#include <sys/errno.h>
#include "amsg.h"
#include "Adaq.h"

#define MAX_INP_MSG 200

extern int errno;
void cmd_run(unsigned short mode);
void send_cmd(unsigned short mode,unsigned short istat);

/**
 void interpret_command(unsigned short *cmdlist)

    interpret command received from gui. The following commands are implemented:
        Initialize
        Start
        Stop
 */
void interpret_command(unsigned short *cmdlist)
{
    int i=1;
    while(i<cmdlist[0]-1){
        if(cmdlist[i+1]==GUI_INITIALIZE) send_cmd(DU_INITIALIZE,0);
        if(cmdlist[i+1]==GUI_START_RUN)cmd_run(DU_START);
        if(cmdlist[i+1]==GUI_STOP_RUN)cmd_run(DU_STOP);
        i+=cmdlist[i];
    }
}

/**
 void gui_main()
 
 Create a TCP/IP socket
 wait for connections
    read command
    close connection
 */
void gui_main()
{
    int sock;
    unsigned int A_length,RD_alength;
    struct sockaddr_in A_address;
    fd_set sockset;
    int option, gui,i,bytesRead;
    struct timeval timeout;
    unsigned short gui_input[MAX_INP_MSG];
    unsigned char *bf = (unsigned char *)gui_input;
    timeout.tv_sec = 0;
    timeout.tv_usec=100;
    
    gui=-1;
    sock =  socket ( PF_INET, SOCK_STREAM, 0 );
    if(sock < 0 ) {
      printf("make gui connection: Cannot create a socket\n");
      return;
    }
    option = 1;
    if(setsockopt (sock,SOL_SOCKET, SO_REUSEADDR,&option,sizeof (int) )<0) return;
    option = 0;
    if(setsockopt (sock,SOL_SOCKET, SO_KEEPALIVE,&option,sizeof (int) ) < 0) return;
    option = 1;
    if(setsockopt (sock,IPPROTO_TCP, TCP_NODELAY, &option,sizeof (int) )<0) return;
    A_address.sin_family       = AF_INET;
    A_address.sin_port         = htons (5010);
    A_address.sin_addr.s_addr  = htonl ( INADDR_ANY );
    A_length = sizeof(A_address);
    if (bind ( sock, (struct sockaddr*)&A_address,A_length) < 0)  {
      shutdown(sock,SHUT_RDWR);
      close(sock);
      sock = -1;
      printf("make gui connection: bind does not work\n");
      return;
    }
    if (listen (sock, 1 ) <0) {
      shutdown(sock,SHUT_RDWR);
      close(sock);
      sock = -1;
      printf("make gui connection: listen does not work\n");
      return;
    }
    while(gui < 0 ){
        gui = accept(sock, (struct sockaddr*)&A_address,&A_length);
        if(gui <0){
            if(errno != EWOULDBLOCK && errno != EAGAIN) return; // an error if socket is not non-blocking
            continue;
        }
        //from here on accept succeeded
        FD_ZERO(&sockset);
        FD_SET(gui, &sockset);
        //printf("Set timeout\n");
        i= select(gui, &sockset, NULL, NULL, &timeout);//was gui+1
        if((i = recvfrom(gui, gui_input,2,0,(struct sockaddr*)&A_address,&A_length))==2){ //read the buffer size
            if ((gui_input[0] < 0) || (gui_input[0]>MAX_INP_MSG)) {
                printf("Check Server Data: bad buffer size when receiving socket data (%d shorts)\n", gui_input[0]);
                shutdown(gui,SHUT_RDWR);
                close(gui);
                gui = -1;
                continue;
            }
            bytesRead = 2;
            while (bytesRead < (2*gui_input[0]+2) &&gui>0) {
                A_length = sizeof(A_address);
                i = recvfrom(gui,&bf[bytesRead],
                             2*gui_input[0]+2-bytesRead,0,(struct sockaddr*)&A_address,&A_length);
                if(i>0) bytesRead +=i;
                if(errno == EAGAIN && i<0) continue;
                if (i <= 0) {
                    //printf("Check Server Data: connection died when receiving socket data %d\n",errno);
                    shutdown(gui,SHUT_RDWR);
                    close(gui);
                    gui= -1;
                }
            }
            //printf("Read %d %d\n",gui_input[0],bytesRead);
            if(bytesRead==(2*gui_input[0]+2)){// all is well, interpret
                interpret_command(gui_input);
            }
            shutdown(gui,SHUT_RDWR);
            close(gui);
            gui= -1;
        }
    }
    printf("Gui done\n");
}
