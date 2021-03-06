#if __STDC_VERSION__ >= 199901L
#define _XOPEN_SOURCE 600
#else
#define _XOPEN_SOURCE 500
#endif /* __STDC_VERSION__ */

#include <stdlib.h>
#include <unistd.h>
#include <time.h>

#include "ipc.h"
#include "common.h"
#include "pa1.h"
#include "main.h"

int send(void * self, local_id dst, const Message * msg) {
	dataIO_t* data = self;
	uint16_t size = sizeof(msg->s_header) + msg->s_header.s_payload_len;

	if(write(data->pipes[dst][data->lid].rdwr[1], msg, size) != size)
		return 1;
	return 0;
}

int send_multicast(void * self, const Message * msg) {
	dataIO_t* data = self;

	for(int i = 0; i < data->processes; i++) {
		if(i == data->lid)
			continue;
		if(send(self, i, msg) != 0)
			return 1;
	}
	return 0;
}

int receive(void * self, local_id from, Message * msg) {
	dataIO_t* data = self;
	uint16_t size = sizeof(msg->s_header);

	if(read(data->pipes[data->lid][from].rdwr[0], &msg->s_header, size) != size)
		return -1;

	size = msg->s_header.s_payload_len;

	if(read(data->pipes[data->lid][from].rdwr[0], &msg->s_payload, size) != size)
		return -1;

	return 0;
}

int receive_any(void * self, Message * msg) {
	dataIO_t* data = self;

	for(int i = 0; i < data->processes; i++) {
		if(i == data->lid)
			continue;
		if(receive(self, i, msg) == 0)
			return i;
	}

	struct timespec tmr;
	tmr.tv_sec = 0;
	tmr.tv_nsec = 50000000;

	if(nanosleep(&tmr, NULL) < 0 )   
	{
		return -1;
	}

	return -2;
}
