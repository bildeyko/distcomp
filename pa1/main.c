#if __STDC_VERSION__ >= 199901L
#define _XOPEN_SOURCE 600
#else
#define _XOPEN_SOURCE 500
#endif /* __STDC_VERSION__ */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <string.h>

#include "ipc.h"
#include "common.h"
#include "pa1.h"

struct pipes_t
{
   int rdwr[2];  // 0 = read, 1 = write
};

struct dataIO_t
{
	int processes;
	int8_t lid;
	struct pipes_t pipes[MAX_PROCESS_ID+1][MAX_PROCESS_ID+1];
};

int main(int argc, char *argv[])
{
	int pid, processes;
	struct dataIO_t data;
	int start_msgs, done_msgs;

	processes = atoi(argv[1])+1;
	data.processes = processes;

	for(int i = 0; i < processes; i++) {		
		for(int j = 0; j < processes; j++) {
			if(j==i) {
				data.pipes[i][j].rdwr[0] = -1;
				data.pipes[i][j].rdwr[1] = -1;
			}
			else {
				if(pipe(data.pipes[i][j].rdwr) < 0)
					printf("Error with pipe");
				int mode = fcntl(data.pipes[i][j].rdwr[0], F_GETFL);
				fcntl(data.pipes[i][j].rdwr[0], F_SETFL, mode | O_NONBLOCK);
			}

			//printf("Pipe (%d): %d\n", i, data.pipes[i][j].rdwr[0]);
		}
	}

	//send(&data, 123, NULL);

	Message msg, resMsg;
	msg.s_header.s_magic = MESSAGE_MAGIC;
	msg.s_header.s_local_time = 0;

	for(int i = 1; i < processes; i++) {
		pid = fork();

		if(pid < 0) {
			printf("Error");
			exit(1);
		} else if (pid == 0) {
			FILE *fd_events;
			start_msgs = data.processes - 2;
			done_msgs = data.processes - 2;

			data.lid = i;

			if ((fd_events = fopen(events_log, "a")) == NULL) {
				printf("Error with file");
       			exit(1);
			}			
			fprintf(fd_events, log_started_fmt, data.lid, getpid(), getppid());
			printf(log_started_fmt, data.lid, getpid(), getppid());
// close unused pipes!
			
			msg.s_header.s_type = STARTED;
			sprintf(msg.s_payload, log_started_fmt, data.lid, getpid(), getppid());
			msg.s_header.s_payload_len = strlen(msg.s_payload);
			send_multicast(&data, &msg);

			while(start_msgs) {
				
				if(receive_any(&data, &resMsg) == 0) {
					if(resMsg.s_header.s_type == STARTED)
					{
						start_msgs--;
						//printf("%d start_msgs %d\n", data.lid, start_msgs);
					}
					if(resMsg.s_header.s_type == DONE)
						done_msgs--;
				}
			}

			fprintf(fd_events, log_received_all_started_fmt, data.lid);
			printf(log_received_all_started_fmt, data.lid);

			exit(0); 
		}
	}

	//write(pipes[1][1], "Hello world\n", 12);
	//write(pipes[3][1], "Hello world\n", 12);
	/*nbytes = read(pipes[0][0], buf, BSIZE);
	printf("Msg (%d): %s\n", 0, buf);*/

	//receive(&data, 2, NULL);

	start_msgs = data.processes - 1;
	done_msgs = data.processes - 1;

	while(start_msgs) {				
		if(receive_any(&data, &resMsg) == 0) {
			if(resMsg.s_header.s_type == STARTED)
			{
				start_msgs--;
				//printf("%d start_msgs %d\n", data.lid, start_msgs);
			}
			if(resMsg.s_header.s_type == DONE)
				done_msgs--;
		}
	}	

	for(int i = 0; i < data.processes; i++) {
		wait(i);
	}	
}

int send(void * self, local_id dst, const Message * msg) {
	struct dataIO_t* data = self;
	//printf("Msg: %s\n", &msg->s_payload);
	if(write(data->pipes[dst][data->lid].rdwr[1], msg, sizeof(msg->s_header) + msg->s_header.s_payload_len) != sizeof(msg->s_header) + msg->s_header.s_payload_len)
		return 1;
	return 0;
}

int send_multicast(void * self, const Message * msg) {
	struct dataIO_t* data = self;
	for(int i = 0; i < data->processes; i++) {
		if(i == data->lid)
			continue;
		if(send(self, i, msg) != 0)
			return 1;
	}
	return 0;
}

int receive(void * self, local_id from, Message * msg) {
	struct dataIO_t* data = self;

	if(read(data->pipes[data->lid][from].rdwr[0], &msg->s_header, sizeof(msg->s_header)) != sizeof(msg->s_header)) {
		return -1;
	}
	if(read(data->pipes[data->lid][from].rdwr[0], &msg->s_payload, msg->s_header.s_payload_len) != msg->s_header.s_payload_len) {
		return -1;
	}
	printf("%d received %s from %d\n", data->lid, msg->s_header.s_type == STARTED ? "STARTED" : "DONE", from);
	return 0;
}

int receive_any(void * self, Message * msg) {
	struct dataIO_t* data = self;

	/*for(int i = 0; i < data->processes; i++)
		if(i != data->lid)
			if (dup2(data->pipes[data->lid][i].rdwr[0], fd[0]) == -1)
				return 1;*/


	for(int i = 0; i < data->processes; i++) {
		if(i == data->lid)
			continue;
		if(receive(self, i, msg) == 0)
			return 0;
	}

	struct timespec tmr;
	tmr.tv_sec = 0;
	tmr.tv_nsec = 10000000;

	if(nanosleep(&tmr, NULL) < 0 )   
	{
		printf("Nano sleep system call failed \n");
		return -1;
	}

	return 1;
}
