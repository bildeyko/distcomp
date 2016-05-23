#include <stdio.h>
#include "queue.h"
#include "stdlib.h" 

queue_t queue = {0, NULL};

void add_item(local_id pid, timestamp_t times)
{
	item_t *newItem;
	newItem = malloc(sizeof(item_t));
    newItem->pid = pid;
    newItem->times = times;
    newItem->next = NULL;

	if(queue.length == 0)
		queue.head = newItem;
	else
	{
		item_t *current = queue.head;
		item_t *prevItem;

		if(current->times > times)
		{
			newItem->next = current;
			queue.head = newItem;
			queue.length ++;
			return;
		}

		while (current->next != NULL && current->times < times) {
			prevItem = current;
			current = current->next;
		}

		if(current->next != NULL){
			newItem->next = current;
			prevItem->next = newItem;
		}
		else
		{
			if(current->times < times)
				current->next = newItem;
			else {
				newItem->next = current;
				prevItem->next = newItem;
			}
		}
	}

	queue.length ++;
}

int delete_item(local_id pid)
{
	item_t *prevItem, *current;
	current = queue.head;

	if(current->pid == pid)
	{
		queue.head = current->next;
		free(current);
		queue.length --;
		return 0;
	}
	
	while (current->next != NULL && current->pid != pid) {
		prevItem = current;
		current = current->next;
	}

	if(current->next == NULL && current->pid != pid)
		return 1;

	if(queue.length == 1)
		queue.head = NULL;
	else
		prevItem->next = current->next;

	free(current);
	queue.length --;
	return 0;
}

item_t *get_head()
{
	return queue.head;
}

void print_queue() {
	item_t * current = queue.head;

	while (current != NULL) {
		printf("(%d;%d)\n", current->times, current->pid);
		current = current->next;
	}
}
