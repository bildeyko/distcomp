#if __STDC_VERSION__ >= 199901L
#define _XOPEN_SOURCE 600
#else
#define _XOPEN_SOURCE 500
#endif /* __STDC_VERSION__ */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <wait.h>
#include <getopt.h>

#include "ipc.h"
#include "common.h"
#include "pa2345.h"
#include "main.h"
#include "banking.h"
#include "queue.h"


void usage();
int closeUnusedPipes(void * self);
void doChild(void *, FILE *, int, int, int);

static timestamp_t lamportStamp = 0;

timestamp_t get_lamport_time()
{
	return ++lamportStamp;
}

timestamp_t max(timestamp_t a, timestamp_t b)
{
	if (a>b)
		return a;
	else
		return b;
}

int main(int argc, char *argv[])
{
	int pid;
	dataIO_t data;
	int start_msgs, done_msgs;
	FILE *fd_pipes, *fd_events;

	if(argc < 2)
		usage();

	int c;
	opterr=0;

	int mutexfl = 0;

    const char* short_options = "p:";

    const struct option long_options[] = {
        {"mutexl", 0, &mutexfl, 1},
        {NULL,0,NULL,0}
    };

    while(1){
		c = getopt_long(argc, argv, short_options, long_options, NULL);
		if (c == -1)
			break;

		switch (c) {
			case 0:
				break;
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

				int mode;
				mode = fcntl(data.pipes[i][j].rdwr[0], F_GETFL);
				fcntl(data.pipes[i][j].rdwr[0], F_SETFL, mode | O_NONBLOCK);

				mode = fcntl(data.pipes[i][j].rdwr[1], F_GETFL);
				fcntl(data.pipes[i][j].rdwr[1], F_SETFL, mode | O_NONBLOCK);

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

			doChild(&data, fd_events, i, 0, mutexfl);
			exit(0);			
		}
	}

	closeUnusedPipes(&data);

	timestamp_t tm = get_lamport_time();
	start_msgs = data.processes - 1;
	done_msgs = data.processes - 1;

	while(start_msgs) {				
		if(receive_any(&data, &resMsg) > -1) {
			if(resMsg.s_header.s_type == STARTED)
			{
				start_msgs--;
				lamportStamp = max(lamportStamp, resMsg.s_header.s_local_time);
				tm = get_lamport_time();
			}
			if(resMsg.s_header.s_type == DONE) {
				done_msgs--;
				lamportStamp = max(lamportStamp, resMsg.s_header.s_local_time);
				tm = get_lamport_time();
			}
		}
	}

	fprintf(fd_events, log_received_all_started_fmt, tm, data.lid);
	fflush(fd_events);
	printf(log_received_all_started_fmt, tm, data.lid);

	while(done_msgs) {				
		if(receive_any(&data, &resMsg) > -1) {
			if(resMsg.s_header.s_type == DONE){
				done_msgs--;
				lamportStamp = max(lamportStamp, resMsg.s_header.s_local_time);
				tm = get_lamport_time();
			}
		}
	}

	fprintf(fd_events, log_received_all_done_fmt, tm, data.lid);
	fflush(fd_events);
	printf(log_received_all_done_fmt, tm, data.lid);		

	fclose(fd_events);

	for(int i = 0; i < data.processes; i++) {
		wait(&i);
	}	

	return 0;
}

void doChild(void *parentData, FILE *fd_events, int lid, int initBalance, int mutexfl)
{
	Message msg, resMsg;

	dataIO_t* data = parentData;

	int start_msgs = data->processes - 2;
	int done_msgs = data->processes - 2;

	timestamp_t tm = lamportStamp;

	data->lid = lid;

	closeUnusedPipes(data);

	balance_t childBalance = initBalance;

	fprintf(fd_events, log_started_fmt, tm, data->lid, getpid(), getppid(), childBalance);
	fflush(fd_events);
	printf(log_started_fmt, tm, data->lid, getpid(), getppid(),childBalance);

	tm = get_lamport_time();
	msg.s_header.s_type = STARTED;
	msg.s_header.s_magic = MESSAGE_MAGIC;
	sprintf(msg.s_payload, log_started_fmt, tm, data->lid, getpid(), getppid(), childBalance);
	msg.s_header.s_payload_len = strlen(msg.s_payload);
	msg.s_header.s_local_time = tm;
	send_multicast(data, &msg);

	while(start_msgs) {				
		if(receive_any(data, &resMsg) > -1) {
			if(resMsg.s_header.s_type == STARTED)
				start_msgs--;
			if(resMsg.s_header.s_type == DONE)
				done_msgs--;
		}
	}

	fprintf(fd_events, log_received_all_started_fmt, tm, data->lid);
	fflush(fd_events);
	printf(log_received_all_started_fmt, tm, data->lid);

	int replyCounter = 0;
	int printIterator = 0;
	int printMax = data->lid * 5;
	while(done_msgs || printIterator < printMax) {		
		int rPid = receive_any(data, &resMsg);	
		if(rPid > -1) {
			if(resMsg.s_header.s_type == DONE)
			{
				lamportStamp = max(lamportStamp, resMsg.s_header.s_local_time);
				tm = get_lamport_time();

				done_msgs--;
			}

			if(resMsg.s_header.s_type == CS_REQUEST)
			{
				lamportStamp = max(lamportStamp, resMsg.s_header.s_local_time);
				tm = get_lamport_time();

				add_item(rPid, resMsg.s_header.s_local_time);

				tm = get_lamport_time();
				msg.s_header.s_type = CS_REPLY;
				msg.s_header.s_payload_len = 0;
				msg.s_header.s_local_time = tm;
				send(data, rPid, &msg);
			}

			if(resMsg.s_header.s_type == CS_RELEASE)
			{
				lamportStamp = max(lamportStamp, resMsg.s_header.s_local_time);
				tm = get_lamport_time();

				delete_item(rPid);
			}

			if(resMsg.s_header.s_type == CS_REPLY)
			{
				lamportStamp = max(lamportStamp, resMsg.s_header.s_local_time);
				tm = get_lamport_time();

				replyCounter ++;
			}
		}

		if(mutexfl == 1){
			cs_t csdata;
			csdata.data = data;

			if(printIterator < printMax)
				request_cs(&csdata);

			item_t *head = get_head();
			if(head != NULL)
			{
				if(replyCounter == data->processes - 2 && head->pid == data->lid)
				{
					char str[MAX_PAYLOAD_LEN];
					sprintf(str, log_loop_operation_fmt, data->lid, printIterator+1, printMax);
					print(str);

					printIterator++;

					release_cs(&csdata);
					replyCounter = 0;

					
				}
			} 			
		} else {
			if(printIterator < printMax)
			{
				char str[MAX_PAYLOAD_LEN];
				sprintf(str, log_loop_operation_fmt, data->lid, printIterator+1, printMax);
				print(str);
				printIterator++;
			}
		}

		if(printIterator == printMax)
		{
			fprintf(fd_events, log_done_fmt, tm, data->lid, childBalance);
			fflush(fd_events);
			printf(log_done_fmt, tm, data->lid, childBalance);			

			msg.s_header.s_type = DONE;
			sprintf(msg.s_payload, log_done_fmt, tm, data->lid, childBalance);
			msg.s_header.s_payload_len = strlen(msg.s_payload);
			send_multicast(data, &msg);

			printIterator ++;
		}
	}

	fprintf(fd_events, log_received_all_done_fmt, tm, data->lid);
	fflush(fd_events);
	printf(log_received_all_done_fmt, tm, data->lid);

	fclose(fd_events);
	exit(0); 
}

int requested = 0;

int request_cs(const void * self)
{
	if(requested == 0) {
		Message msg;
		const cs_t* csdata = self;

		timestamp_t tm = get_lamport_time();
		msg.s_header.s_type = CS_REQUEST;
		msg.s_header.s_payload_len = 0;
		msg.s_header.s_local_time = tm;
		send_multicast(csdata->data, &msg);

		add_item(csdata->data->lid, tm);

		requested = 1;
		return 1;
	} else 
		return 2;
}

int release_cs(const void * self)
{
	if(requested == 1) {
		Message msg;
		const cs_t* csdata = self;

		timestamp_t tm = get_lamport_time();
		msg.s_header.s_type = CS_RELEASE;
		msg.s_header.s_payload_len = 0;
		msg.s_header.s_local_time = tm;
		send_multicast(csdata->data, &msg);

		delete_item(csdata->data->lid);

		requested = 0;
		return 1;
	} else 
		return 2;
}

int closeUnusedPipes(void * self)
{
	dataIO_t* data = self;
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
	printf("usage: pa4 -p num --mutexl\n");
	exit(1);
}
