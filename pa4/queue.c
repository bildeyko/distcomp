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
		while (current->next != NULL && current->times<times) {
			current = current->next;
		}

		if(current->next != NULL)
			newItem->next = current->next;

		current->next = newItem;
	}

	queue.length ++;
}

int delete_item(local_id pid)
{
	item_t *prevItem, *current;
	current = queue.head;
	
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
	return 0;
}

item_t *get_head()
{
	return queue.head;
}

void print_list() {
	item_t * current = queue.head;

	while (current != NULL) {
		printf("(%d;%d)\n", current->times, current->pid);
		current = current->next;
	}
}
