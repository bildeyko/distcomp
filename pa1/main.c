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
#include <wait.h>

#include "ipc.h"
#include "common.h"
#include "pa1.h"

struct pipes_t
{
   int rdwr[2]; // 0 = read, 1 = write
};

struct dataIO_t
{
	int processes;
	int8_t lid;
	struct pipes_t pipes[MAX_PROCESS_ID][MAX_PROCESS_ID];
};

void usage();
int closeUnusedPipes(void * self);

int main(int argc, char *argv[])
{
	int pid;
	struct dataIO_t data;
	int start_msgs, done_msgs;
	FILE *fd_pipes, *fd_events;

	if(argc < 2)
		usage();

	int c;
	opterr=0;
	while((c = getopt(argc, argv, "p:")) != -1) {
		switch (c) {
			case 'p':
				data.processes = atoi(optarg)+1;
				break;
			case '?':
			default:
				usage();
		}
	}	
	
	data.lid = 0;

	if ((fd_pipes = fopen(pipes_log, "w")) == NULL) {
		fprintf(stderr, "Error opening the file %s\n", pipes_log);
		exit(1);
	}	

	for(int i = 0; i < data.processes; i++) {		
		for(int j = 0; j < data.processes; j++) {
			if(j==i) {
				data.pipes[i][j].rdwr[0] = -1;
				data.pipes[i][j].rdwr[1] = -1;
			}
			else {
				if(pipe(data.pipes[i][j].rdwr) < 0) {
					fprintf(stderr, "Error creating the pipe\n");
					exit(1);
				}

				int mode = fcntl(data.pipes[i][j].rdwr[0], F_GETFL);
				fcntl(data.pipes[i][j].rdwr[0], F_SETFL, mode | O_NONBLOCK);

				fprintf(fd_pipes, "The pipe %d ===> %d was created\n", j, i);
			}
		}
	}

	fclose(fd_pipes);

	Message msg, resMsg;
	msg.s_header.s_magic = MESSAGE_MAGIC;
	msg.s_header.s_local_time = 0;

	if ((fd_events = fopen(events_log, "a")) == NULL) {
		fprintf(stderr, "Error opening the file %s\n", events_log);
		exit(1);
	}	

	for(int i = 1; i < data.processes; i++) {
		pid = fork();

		if(pid < 0) {
			fprintf(stderr, "Error creating the child\n");
			exit(1);
		} else if (pid == 0) {
			
			start_msgs = data.processes - 2;
			done_msgs = data.processes - 2;

			data.lid = i;

			closeUnusedPipes(&data);
		
			fprintf(fd_events, log_started_fmt, data.lid, getpid(), getppid());
			fflush(fd_events);
			printf(log_started_fmt, data.lid, getpid(), getppid());
			
			msg.s_header.s_type = STARTED;
			sprintf(msg.s_payload, log_started_fmt, data.lid, getpid(), getppid());
			msg.s_header.s_payload_len = strlen(msg.s_payload);
			send_multicast(&data, &msg);

			while(start_msgs) {				
				if(receive_any(&data, &resMsg) == 0) {
					if(resMsg.s_header.s_type == STARTED)
						start_msgs--;
					if(resMsg.s_header.s_type == DONE)
						done_msgs--;
				}
			}

			fprintf(fd_events, log_received_all_started_fmt, data.lid);
			fflush(fd_events);
			printf(log_received_all_started_fmt, data.lid);

			fprintf(fd_events, log_done_fmt, data.lid);
			fflush(fd_events);
			printf(log_done_fmt, data.lid);			

			msg.s_header.s_type = DONE;
			sprintf(msg.s_payload, log_done_fmt, data.lid);
			msg.s_header.s_payload_len = strlen(msg.s_payload);
			send_multicast(&data, &msg);

			while(done_msgs) {				
				if(receive_any(&data, &resMsg) == 0) {
					if(resMsg.s_header.s_type == DONE)
						done_msgs--;
				}
			}

			fprintf(fd_events, log_received_all_done_fmt, data.lid);
			fflush(fd_events);
			printf(log_received_all_done_fmt, data.lid);

			fclose(fd_events);
			exit(0); 
		}
	}

	closeUnusedPipes(&data);

	start_msgs = data.processes - 1;
	done_msgs = data.processes - 1;

	while(start_msgs) {				
		if(receive_any(&data, &resMsg) == 0) {
			if(resMsg.s_header.s_type == STARTED)
				start_msgs--;
			if(resMsg.s_header.s_type == DONE)
				done_msgs--;
		}
	}

	fprintf(fd_events, log_received_all_started_fmt, data.lid);
	fflush(fd_events);
	printf(log_received_all_started_fmt, data.lid);

	while(done_msgs) {				
		if(receive_any(&data, &resMsg) == 0) {
			if(resMsg.s_header.s_type == DONE)
				done_msgs--;
		}
	}

	fprintf(fd_events, log_received_all_done_fmt, data.lid);
	fflush(fd_events);
	printf(log_received_all_done_fmt, data.lid);		

	fclose(fd_events);

	for(int i = 0; i < data.processes; i++) {
		wait(&i);
	}	

	return 0;
}

int send(void * self, local_id dst, const Message * msg) {
	struct dataIO_t* data = self;
	uint16_t size = sizeof(msg->s_header) + msg->s_header.s_payload_len;

	if(write(data->pipes[dst][data->lid].rdwr[1], msg, size) != size)
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
	uint16_t size = sizeof(msg->s_header);

	if(read(data->pipes[data->lid][from].rdwr[0], &msg->s_header, size) != size)
		return -1;

	size = msg->s_header.s_payload_len;

	if(read(data->pipes[data->lid][from].rdwr[0], &msg->s_payload, size) != size)
		return -1;

	return 0;
}

int receive_any(void * self, Message * msg) {
	struct dataIO_t* data = self;

	for(int i = 0; i < data->processes; i++) {
		if(i == data->lid)
			continue;
		if(receive(self, i, msg) == 0)
			return 0;
	}

	struct timespec tmr;
	tmr.tv_sec = 0;
	tmr.tv_nsec = 50000000;

	if(nanosleep(&tmr, NULL) < 0 )   
	{
		printf("Nano sleep system call failed \n");
		return -1;
	}

	return 1;
}

int closeUnusedPipes(void * self)
{
	struct dataIO_t* data = self;
	for(int i = 0; i < data->processes; i++) {

		for(int j = 0; j < data->processes; j++) {	
			if(i == j)
				continue;	
			if(i == data->lid) {
				close(data->pipes[data->lid][j].rdwr[1]);
				continue;	
			}
			if(i != data->lid && j != data->lid) {
				close(data->pipes[i][j].rdwr[1]);
				close(data->pipes[i][j].rdwr[0]);
				continue;
			}
			close(data->pipes[i][j].rdwr[0]);
		}
	}
	return 0;
}

void usage()
{
	printf("usage: pa1 -p num\n");
	exit(1);
}
