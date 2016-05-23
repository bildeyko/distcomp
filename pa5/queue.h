#ifndef DISTRIBUTED_CLASS_QUEUE_H
#define DISTRIBUTED_CLASS_QUEUE_H

#include "ipc.h"

typedef struct item
{
	local_id pid;
	timestamp_t times;
	struct item *next;	
} item_t;

typedef struct
{
	int length;
	item_t *head;
} queue_t;

void add_item(local_id, timestamp_t);

int delete_item(local_id);

item_t *get_head();

void print_queue();

#endif
