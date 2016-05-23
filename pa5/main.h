#ifndef DISTRIBUTED_CLASS_LAB_1_H
#define DISTRIBUTED_CLASS_LAB_1_H

typedef struct 
{
	int rdwr[2]; // 0 = read, 1 = write
} pipes_t;

typedef struct
{
	int processes;
	int8_t lid;
	pipes_t pipes[MAX_PROCESS_ID][MAX_PROCESS_ID];
} dataIO_t;

typedef struct 
{
	dataIO_t * data;
} cs_t;

#endif
