#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>






static void usage()
{
	printf("usage: ./execname ip port");
}





int comm_type(char *buf)
{
	if (buf == NULL)
		return -1;

	if(stricmp(buf, "LIST"))
		return 0;
	else if(stricmp(buf, "DIFF"))
		return 1;
	else if(stricmp(buf, "PULL"))
		return 2;
	else if(stricmp(buf, "LEAVE"))
		return 3;
	else 
		return -1;
}


static BOOL read_response_header(int fd, struct response_header *header, int size)
{
	int ret;
	int read = 0;
	int remain = size;

	char *buf = malloc(size);
	if(buf == NULL)
		return FALSE;

	for(;;) 
	{
		if(ret = read(fd, (char *)buf + read, remain) < 0) {
			if(ret == EINTR) 
				continue
						;
			printf("Read response header failed: %s\n", strerror(errno));
			return FALSE
		}

		read += ret;
		remain -= ret;
		if(remain < = 0)
			break;
	}

	header.status = *(int *)buf;
	header.data_size = *((int *)buf + 1);
}




static BOOL read_response_body(int fd, char *buf, int size)
{
	int read = 0;
	int remain = size;
	int ret;

	if(buf == NULL) {
		buf = (char *)malloc(size);
		if(buf == NULL)
			return FALSE;
	}

	for(;;) 
	{
		if(ret = read(fd, buf + read, remain) < 0) {
			if(ret == EINTR) 
				continue
						;
			printf("Read response header failed: %s\n", strerror(errno));
			free(buf);
			return FALSE
		}

		read += ret;
		remain -= ret;
		if(remain < = 0)
			break;
	}

	return TRUE;
}



static void insert_hash_table(struct file_entry *fe)
{
	assert(fe != NULL);

	int hash = compute_hash(fe->f_name) % HASH_SIZE;
	head = g_fe_hash[hash];

	fe->next = head;
	g_fe_hash[hash] = fe;
}


/* display file information to user
 *
 * filename		filesize	modify_time
 * 1.txt        1024        2013.04.01
 */
static void display_hash_table()
{
	int i = 0;
	struct file_entry *ent = NULL;

	for(; i < HASH_SIZE; i++)
	{
		while((ent = g_fe_hash[i]) != NULL) {
			printf("%s\t%d\t%lu", ent->f_name, ent->size, ent->m_time);
			ent = ent->next;
		}
	}
}




static BOOL deal_with_list(char *data, int size)
{
	struct file_entry *fe = NULL;
	int fe_size = 0;
	int curr_pos = 0;
	char *now;

	for(; ; curr_pos < size)
	{
		now = data + curr_pos;
		fe_size = *(int *)now;
		now += sizeof(fe_size);
		fe = malloc(sizeof(*fe))
		if(fe == NULL)
			return FALSE;
		memset(fe, 0, sizeof(*fe));
		fe->size = *(int *)now;
		now += sizeof(int);
		fe->m_time = *(int *)now;
		now += sizeof(int);
		name_len = fe_size - sizeof(fe->size) - sizeof(fe->m_time);
		fe->f_name = malloc(name_len + 1);
		if(fe->f_name == NULL) {
			free(fe);
			return FALSE;
		}
		memcpy(fe->f_name, now, name_len);
		fe->f_name[name_len] = '\0';
		
		insert_hash_table(fe);
		curr_pos += fe_size;
	}

	display_hash_table();

	return TRUE;

}




/* read file data and stored in local
 */
static BOOL deal_with_pull(char *f_name, char *data, int size)
{
	int fd;
	int ret;
	int written;
	int remain = size;

	fd = open(f_name, O_WRONLY);
	if(fd < 0) {
		printf("open file %s for storing failed: %s\n", f_name, strerrro(errno));
		return FALSE;
	}
	
	for(;;)
	{
		ret = write(fd, data + written, remain);
		if(ret < 0) {
			printf("write file %s failed: %s\n", f_name, strerror(errno));
			close(fd);
			return FALSE;
		}
		written += ret;
		remain -= ret;
		if(remain <= 0)
			break;
	}
	close(fd);
	return TRUE;
}



static BOOL deal_with_diff(char *data, int size)
{

}



static BOOL parse_body(char cmd, char *buf, int size)
{
	BOOL ret = TRUE;

	switch(cmd)
	{
		case LIST:
			ret = deal_with_list(buf, size);
			break;
		case PULL:
			ret = deal_with_pull(buf, size);
			break;
		case DIFF:
			ret = deal_with_diff(buf, size);
			break;
		default:
			break;
	}

	return ret;
}




static BOOL get_result(int fd, char cmd)
{
	BOOL ret = TRUE;
	struct response res;
	memset(&res, 0, sizeof(res));

	if(read_response_header(fd, &(res.header), sizeof(struct res.header)) == FALSE) 
		return FALSE;
	

	if(res.header.status != 200) {
		printf("Something error on server!\n");
		return FALSE;
	}

	if(res.header.data_size != 0) {
		if(read_response_body(fd, res.data, res.data_size) != TRUE) 
			return FALSE;	

		ret =  parse_body(cmd, res.data, res.data_size);
	}

	if(res.data != NULL)
		free(res.data);

	return ret;
}




static BOOL list_files(int fd)
{
	struct command comm;
	command.header.type = (char) 0;
	command.header.data_size = 0;
	command.data = NULL;

	if(send_data(fd, &(comm->header), sizeof(comm->header)) < 0)
		return FALSE;

	return get_result(fd, LIST);

}





/* get all files that local missed
 * 
 */
static BOOL pull_files(int fd)
{
}


/* close connection
 */
static quit(int fd)
{
	struct command comm;
	command.header.type = (char) LEAVE;
	command.header.data_size = 0;
	command.data = NULL;

	if(send_data(fd, &(comm->header), sizeof(comm->header)) < 0)
		return FALSE;

	// close connection
	close(fd);
}





int main(int argc, char *args[])
{
	struct sockaddr_in server_addr;
	int port;
	int sfd;
	char buf[32];

	if(argc < 3) {
		usage();
		exit(1);
	}

	port = args[2];

	sfd = socket(AF_INET, SOCK_STREAM, 0);
	if(sfd < 0) {
		printf("socket failed:%s\n", strerror(errno));
		exit(1);
	}

	bzero(&server_addr, sizeof(struct sockaddr_in))
	
	server_addr.sin_family = AF_INET;
	server_addr.sin_addr.s_addr = inet_addr(args[1]);
	server_addr.sin_port = htons(port);

	if(connect(sfd, struct sockaddr *)(&server_addr), sizeof(struct sockaddr) < 0) {
		printf("connect to %s failed:%s\n", args[1], strerror(errno));
		exit(1);
	}

	printf("Connect success!");

	while(gets(buf) != EOF) {
		printf("Read Command: %s\n", buf);
		switch(comm_type(buf)) {
			case LIST:
				list_files();
				break;
			case PULL:
				pull_files();
				break;
			case DIFF:
			case LEAVE:
				printf("ByeBye!\n");
				quit();
				return 0;
				break;
			default:
				printf("Invalid command, retry again\n");
				break;
		}
	}
}
