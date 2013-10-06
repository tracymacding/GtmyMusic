#include "bufferManger.h"


struct buffer_header
{
	int total_size;
	int content_size;
	int free_space;
};


struct my_buffer
{
	struct buffer_header bh;
	void *buf;
};


void * my_malloc(int size)
{
	if(size <= 0)
		return NULL;

	if()

}
