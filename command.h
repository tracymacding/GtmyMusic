#ifndef COMMAND_H
#define COMMAND_H



#define LIST  0
#define DIFF  1
#define PULL  2
#define LEAVE 3

typedef struct command_header
{
	char type; // command type list above
	int  data_size; // data size of command
} command_header_t;



typedef struct command
{
	struct command_header header;
	void   *data;
} command_t;



typedef struct response_header
{
	int status;
	int data_size;
} response_header_t;


typedef struct response
{
	struct response_header header;
	void *data;
} response_t;


#endif
