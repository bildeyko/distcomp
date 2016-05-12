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
#include <time.h>

#include "ipc.h"
#include "common.h"
#include "pa2345.h"
#include "main.h"
#include "banking.h"


void usage();
int closeUnusedPipes(void *);
void doChild(void *, FILE *, int, int);

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
	argc -= optind;
	argv += optind;	

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
			doChild(&data, fd_events, i, atoi(argv[i-1]));
			exit(0); 
		}
	}

	closeUnusedPipes(&data);

	timestamp_t tm = get_physical_time();
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

	tm = get_physical_time();

	fprintf(fd_events, log_received_all_started_fmt, tm, data.lid);
	fflush(fd_events);
	printf(log_received_all_started_fmt, tm, data.lid);

	bank_robbery(&data,data.processes-1);

	msg.s_header.s_type = STOP;
	msg.s_header.s_payload_len = 0;
	send_multicast(&data, &msg);

	while(done_msgs) {				
		if(receive_any(&data, &resMsg) == 0) {
			if(resMsg.s_header.s_type == DONE)
				done_msgs--;
		}
	}

	tm = get_physical_time();

	fprintf(fd_events, log_received_all_done_fmt, tm, data.lid);
	fflush(fd_events);
	printf(log_received_all_done_fmt, tm, data.lid);

	int history_msgs = data.processes - 1;
	AllHistory allHistory;
	allHistory.s_history_len = history_msgs;

	do {				
		if(receive_any(&data, &resMsg) == 0) {
			BalanceHistory hist;
			memcpy(&hist, resMsg.s_payload, resMsg.s_header.s_payload_len);

			if(resMsg.s_header.s_type == BALANCE_HISTORY)
			{	
				allHistory.s_history[hist.s_id-1] = hist;
				history_msgs--;
			}
		}
	} while(history_msgs);

	print_history(&allHistory);

	fclose(fd_events);

	for(int i = 0; i < data.processes; i++) {
		wait(NULL);
	}	

	return 0;
}

void doChild(void *parentData, FILE *fd_events, int lid, int initBalance)
{
	struct dataIO_t* data = parentData;

	data->lid = lid;
	int done_msgs = data->processes - 2;
	Message msg, resMsg;

	BalanceHistory history;
	history.s_id = lid;
	history.s_history_len = 0;		

	timestamp_t tm = get_physical_time();

	balance_t childBalance = initBalance;

	history.s_history[history.s_history_len].s_balance = childBalance;
	history.s_history[history.s_history_len].s_time = tm;
	history.s_history[history.s_history_len].s_balance_pending_in = 0;			

	closeUnusedPipes(data);

	fprintf(fd_events, log_started_fmt, tm, history.s_id, getpid(), getppid(), 
			history.s_history[history.s_history_len].s_balance);
	fflush(fd_events);
	printf(log_started_fmt, tm, history.s_id, getpid(), getppid(), 
		   history.s_history[history.s_history_len].s_balance);
	
	msg.s_header.s_type = STARTED;
	msg.s_header.s_magic = MESSAGE_MAGIC;
	sprintf(msg.s_payload, log_started_fmt, tm, history.s_id, getpid(), 
			getppid(), history.s_history[history.s_history_len].s_balance);
	msg.s_header.s_payload_len = strlen(msg.s_payload);
	send(data, 0, &msg);

	history.s_history_len ++;

	while(1 && done_msgs) {
		tm = get_physical_time();

		/*
			Set balance for empty timestamps
		*/	
		BalanceState balance;
		balance.s_balance_pending_in = 0;
		balance.s_balance = childBalance;
		balance.s_time = get_physical_time();
		history.s_history[history.s_history_len] = balance;
		history.s_history_len ++;

		if(receive_any(data, &resMsg) == 0) {
			if(resMsg.s_header.s_type == DONE) {
				done_msgs--;	
			}
			if(resMsg.s_header.s_type == STOP) {
				msg.s_header.s_type = DONE;
				sprintf(msg.s_payload, log_done_fmt, tm, data->lid, childBalance);
				msg.s_header.s_payload_len = strlen(msg.s_payload);
				send_multicast(data, &msg);

				fprintf(fd_events, log_done_fmt, tm, data->lid, childBalance);
				fflush(fd_events);
				printf(log_done_fmt, tm, data->lid, childBalance);
			}
			if(resMsg.s_header.s_type == TRANSFER) {
				TransferOrder order;
				memcpy(&order, resMsg.s_payload, resMsg.s_header.s_payload_len);

				if(order.s_src == data->lid) {
					fprintf(fd_events, log_transfer_out_fmt, tm, data->lid, 
							order.s_amount, order.s_dst);
					fflush(fd_events);
					printf(log_transfer_out_fmt, tm, data->lid, order.s_amount, 
						   order.s_dst);
					send(data, order.s_dst, &resMsg);

					childBalance -= order.s_amount;

					BalanceState balance;
					balance.s_balance_pending_in = 0;
					balance.s_balance = childBalance;
					balance.s_time = tm;

					history.s_history[history.s_history_len] = balance;
				}
				if(order.s_dst == data->lid) {
					fprintf(fd_events, log_transfer_in_fmt, tm, data->lid, 
							order.s_amount, order.s_src);
					fflush(fd_events);
					printf(log_transfer_in_fmt, tm, data->lid, order.s_amount, 
						   order.s_src);

					childBalance += order.s_amount;

					BalanceState balance;
					balance.s_balance_pending_in = 0;
					balance.s_balance = childBalance;
					balance.s_time = tm;

					history.s_history[history.s_history_len] = balance;

					msg.s_header.s_type = ACK;
					msg.s_header.s_magic = MESSAGE_MAGIC;
					msg.s_header.s_local_time = tm;
					msg.s_header.s_payload_len = 0;
					send(data, 0, &msg);
				}
			}
		}
	}

	struct timespec tmr;
	tmr.tv_sec = 0;
	tmr.tv_nsec = 50000000;
	nanosleep(&tmr, NULL);

	msg.s_header.s_magic = MESSAGE_MAGIC;
	msg.s_header.s_type = BALANCE_HISTORY;
	msg.s_header.s_local_time = get_physical_time();
	msg.s_header.s_payload_len = sizeof(history);
	memcpy(msg.s_payload, &history, sizeof(history));	

	send(data, 0, &msg);

	fclose(fd_events);
}

void transfer(void * parent_data, local_id src, local_id dst,
              balance_t amount)
{
    TransferOrder order;
    order.s_src = src;
    order.s_dst = dst;
    order.s_amount = amount;

    Message msg, resMsg;
    msg.s_header.s_magic = MESSAGE_MAGIC;
	msg.s_header.s_type = TRANSFER;
	msg.s_header.s_local_time = get_physical_time();
	msg.s_header.s_payload_len = sizeof(order);
	memcpy(msg.s_payload, &order, sizeof(order));	

	send(parent_data, src, &msg);

	while(1) {			
		if(receive(parent_data, dst, &resMsg) == 0) {
			if(resMsg.s_header.s_type == ACK)
				break;			
		}
	}
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
