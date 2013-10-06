#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>

#define FALSE 0
#define TRUE  1
#define BOOL  int

#define DEBUG TRUE

BOOL g_shutdown = FALSE;


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

	res->status = error_code;
	remain = sizeof(res->header) + res->data_size;

	if(send_data(fd, res->header, sizeof(*(res->header)) != TRUE))
		return FALSE;

	return send_data(fd, res->data, res->data_size);
}



static void *thread_loop(void *arg)
{
	int fd = *(int *)arg;
	struct command cmd;
	struct response res;
	int error_code = 200;

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
}




/* create new thread for new connect @fd
 */
static void create_work_thread(int fd)
{
	pthread_t thread;

	pthread_create(&thread, NULL, thread_loop, (void *) &fd);

#if DEBUG
	printf("create work thread success!, pid is %d\n", )
#endif

}		



int main(int argc, char *args[])
{
	int lfd, newfd;

	lfd = socket(AF_INET, SOCK_STREAM, 0);
	if(lfd < 0) {
		printf("create listen socket failed: %s\n", strerror(errno));
		exit(1);
	}



	if(bind(lfd, server_addr, sizeof()) < 0) {
		printf("bind error: %s\n", strerror(errno));
		close(lfd);
		exit(1);
	}

	if(listen(lfd, 10) < 0) {
		printf("listen error: %s\n", strerror(errno));
		close(fd);
		exit(1);
	}


	while(true) {

		if(newfd = accept(lfd, , sizeof()) < 0) {
			printf("accept error: %s, we should exit\n", strerror(errno));
			g_shutdown = TRUE;
			break;
		}

#if DEBUG
		printf("Reveive connect from %s, port %d\n");
#endif

		create_work_thread(newfd);
	}

	sleep(5);
	
	wait_all_thread_exit();

	return 0;
}
