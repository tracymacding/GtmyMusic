#include "command.h"
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <assert.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <linux/in.h>

#define FALSE 0
#define TRUE  1
#define BOOL  int

#define DEBUG TRUE

BOOL g_shutdown = FALSE;
char g_dir[256];

typedef struct thread
{
	int fd;      //socket fd
	int running; //1:has exited
	pthread_t pid;
	struct thread *next;
} thread_t;


static BOOL send_data(int fd, void * data, int size)
{
	int written = 0;
	int remain = 0;
	int ret;

	if(data == NULL || size == 0)
		return TRUE;

	for(; ;) {
		ret = write(fd, (char *)data + written, remain);
		if(ret < 0) {
			if(ret == EINTR)
				continue;

			printf("write response to socket %d failed\n", fd);
			return FALSE;
		}
		written += ret;
		remain -= ret;
		if(remain <= 0)
			break;
	}

	return TRUE;
}



static BOOL send_response(int fd, struct response *res, int error_code)
{
	assert(fd > 0);
	assert(res != NULL);

	res->header.status = error_code;

	if(send_data(fd, &(res->header), sizeof(res->header) != TRUE))
		return FALSE;

	return send_data(fd, res->data, res->header.data_size);
}



static void *thread_loop(void *arg)
{
	struct thread *t = (thread_t *)arg;
	struct command cmd;
	struct response res;
	int error_code = 200;
	int fd = t->fd;

	while(!g_shutdown) {
		
		memset(&cmd, 0, sizeof(cmd));
		memset(&res, 0, sizeof(res));

		if(read_command(fd, &cmd) == FALSE) {
			printf("Read command failed on %d\n", fd);
			error_code = 400;
			goto send_response;
		}
		
		/* do nothing, just quit */
		if(cmd.header.type == LEAVE)
				break;

		if(process_command(&cmd, &res) == FALSE) {
			printf("Process command failed\n");
			error_code = 400;
			goto send_response;
		}

send_response:

		if(send_response(fd, &res, error_code) == FALSE) {
			printf("Send response failed on fd %d\n", fd);
		}

		if(cmd.data != NULL)
			free(cmd.data);

		if(res.data != NULL)
			free(res.data);
	}

	close(fd);

	if(cmd.data != NULL)
		free(cmd.data);

	if(res.data != NULL)
		free(res.data);

	t->running = 0;
}



int main(int argc, char *args[])
{
	int lfd, newfd;
	struct sockaddr_in server_addr, c_addr;
	int port = 123456;
	struct thread *thread_info = NULL;
	struct thread *t_queue_head = NULL;
	struct thread *entry = NULL;
	struct thread *next = NULL;
	int sin_len;

	if(argc < 3) {
		printf("Usage: ./server port file_dir\n");
		exit(1);
	}

	port = atoi(args[1]);
	strcpy(g_dir, args[2]);

	lfd = socket(AF_INET, SOCK_STREAM, 0);
	if(lfd < 0) {
		printf("create listen socket failed: %s\n", strerror(errno));
		exit(1);
	}

	bzero(&server_addr, sizeof(server_addr));
	server_addr.sin_family = AF_INET;
	server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	server_addr.sin_port = htons(port);

	if(bind(lfd, (struct sockaddr *)(&server_addr), sizeof(struct sockaddr)) < 0) {
		printf("bind error: %s\n", strerror(errno));
		close(lfd);
		exit(1);
	}

	if(listen(lfd, 10) < 0) {
		printf("listen error: %s\n", strerror(errno));
		close(lfd);
		exit(1);
	}

	
	for(;;) {

		if(newfd = accept(lfd, (struct sockaddr *)(&c_addr), &sin_len) < 0) {
			printf("accept error: %s, we should exit\n", strerror(errno));
			g_shutdown = TRUE;
			break;
		}

#if DEBUG
		//printf("Reveive connect from %s, port %d\n");
#endif

		if((thread_info = (struct thread *)malloc(sizeof(thread_t))) == NULL) {
			printf("malloc failed for new thread failed\n");
			goto out;
		}
		memset(thread_info, 0, sizeof(thread_t));
		
		thread_info->fd = newfd;
		thread_info->running = 1;

		/* insert thread queue */
		thread_info->next = t_queue_head;
		t_queue_head = thread_info;

		pthread_create(&(thread_info->pid), NULL, thread_loop, thread_info);
	}

	sleep(5);

out:
	g_shutdown = TRUE;
	
	entry = t_queue_head;

	while(entry != NULL) {

		if(entry->running)
			pthread_join();
		entry = entry->next;
	}

	entry = t_queue_head;
	next = entry;
	while(next != NULL) {
		next = entry->next;
		free(entry);
	}

	return 0;
}
