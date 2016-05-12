#ifndef DISTRIBUTED_CLASS_LAB_1_H
#define DISTRIBUTED_CLASS_LAB_1_H

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

#endif
