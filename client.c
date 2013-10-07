#include "command.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <linux/in.h>
#include <unistd.h>
#include <errno.h>




struct file_entry
{
	int  size;
	int  m_time;
	char f_md5[33];
	char *f_name;
	struct file_entry *next;
};


#define FALSE 0
#define TRUE  1

#define HASH_SIZE 128
#define MISS_FILE_ARRAY_SIZE 100000

struct file_entry* g_fe_hash[HASH_SIZE];
char* g_miss_file_array[MISS_FILE_ARRAY_SIZE];
int g_miss_file_num;



static void usage()
{
	printf("usage: ./execname ip port local_file_dir");
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



static unsigned long  compute_hash(char *key, int length)
{
		unsigned long h = 0, g;
		char *arEnd = key + length; 
		
		while (key < arEnd) {
			h = (h << 4) + *key++;
			if ((g = (h & 0xF0000000))) {
				h = h ^ (g >> 24);
				h = h ^ g;
			}
		}
		return h;
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
				continue;

			printf("Read response header failed: %s\n", strerror(errno));
			return FALSE;
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
	struct file_entry *head = NULL;
	assert(fe != NULL);

	unsigned long hash = compute_hash(fe->f_md5, strlen(fe->f_md5)) % HASH_SIZE;
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
			printf("%s\t%d\t%lu\n", ent->f_name, ent->size, ent->m_time);
			ent = ent->next;
		}
	}
}

static void delete_hash_entries()
{
	int i = 0;
	struct file_entry *ent = NULL;

	for(; i < HASH_SIZE; i++)
	{
		while((ent = g_fe_hash[i]) != NULL) {
			free(ent);
			ent = ent->next;
		}
		g_fe_hash[i] = NULL;
	}
}

static BOOL is_miss(struct file_entry *fe)
{
	struct file_entry *head = NULL;
	unsigned long  hash = compute_hash(fe->f_md5, strlen(fe->f_md5));

	head = g_server_fe_hash[hash];

	while(head != NULL) {
		if(strcmp(fe->f_md5, head->f_md5) == 0)
			return TRUE;
	}

	return FALSE;
}





static BOOL deal_with_list(char *data, int size)
{
	struct file_entry *fe = NULL;
	int fe_size = 0;
	int curr_pos = 0;
	char *now;

	/* delete local file entry hash first */

	delete_hash_entries();

	for(; curr_pos < size;)
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
		memcpy(fe->f_md5, (char *)now, 32);
		fe->f_md5[33] = '\0';
		name_len = fe_size - sizeof(fe->size) - sizeof(fe->m_time) - 32;
		fe->f_name = malloc(name_len + 1);
		if(fe->f_name == NULL) {
			free(fe);
			return FALSE;
		}
		memcpy(fe->f_name, now, name_len);
		fe->f_name[name_len] = '\0';
		
		insert_hash_table(fe, SERVER);
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
	comm.header.type = (char) 0;
	comm.header.data_size = 0;
	comm.data = NULL;

	if(send_data(fd, &(comm->header), sizeof(comm->header)) < 0)
		return FALSE;

	return get_result(fd, LIST);

}



static BOOL get_miss_files()
{
	BOOL ret = TRUE;
	DIR *dir = NULL;
	struct dirent *d_entry = NULL;
	struct file_entry *fe = NULL;
	struct stat fstat;
	int curr_pos = 0;
	int f_entry_size = 0;
	char f_md5[33];

	dir = opendir(g_local_dir);
	if(dir == NULL) {
		printf("opendir %s failed: %s\n", g_local_dir, strerror(errno));
		return FALSE;
	}

	g_miss_file_num = 0;

	/* read all file entry and get it's stat */
	while((d_entry = readdir(dir)) != NULL) {
		if(d_entry->d_name[0] == '.')
			continue;
		
		if(stat(d_entry->d_name, &fstat) < 0) {
			printf("stat file %s failed: %s\n", d_entry->d_name, strerror(errno));
			ret = FALSE;
			goto out;
		}

		if(compute_file_md5(d_entry->d_name, f_md5) == FALSE)
			goto out;

		fe = malloc(sizeof(*fe))
		if(fe == NULL)
			return FALSE;
		memset(fe, 0, sizeof(*fe));
		fe->size = fstat.st_size;
		fe->m_time = fstat.st_mtime;
		memcpy(fe->f_md5, f_md5, 32);
		fe->f_md5[33] = '\0';
		name_len = strlen(entry->d_name);
		fe->f_name = malloc(name_len + 1);
		if(fe->f_name == NULL) {
			free(fe);
			return FALSE;
		}
		memcpy(fe->f_name, d_entry->d_name, name_len);
		fe->f_name[name_len] = '\0';
		
		if(is_miss(fe)) {
			printf("%s\n", fe->f_name);
			g_miss_file_array[i++] = fe->f_name;
			g_miss_file_num++;
		}
	}

out:
	if(ret != TRUE && res->data != NULL)
		free(res->data);

	closedir(dir);
	return ret;
	
}

static BOOL diff(int fd)
{
	/* Get all files server own */
	if(list_files(fd) == FALSE)
		return FALSE;
	
	/* Get local file */
	if(get_miss_files() == FALSE)
		return FALSE;

	return TRUE;
}



/* get all files that local missed
 * 
 */
static BOOL pull_files(int fd)
{
	struct command comm;
	memset(&comm, 0, sizeof(comm));

	if(diff() == FALSE)
		return FALSE;

	/* pull all miss file */
	for(i = 0; i < g_miss_file_num; i++) {
		comm.header.type = (char) PULL;
		comm.header.data_size = strlen(g_miss_file_array[i]);
		comm.data = malloc(comm.header.data_size);
		if(comm.data == NULL);
			continue;

		if(send_data(fd, &(comm.header), sizeof(comm.header)) < 0) {
			free(comm.data);
			continue;
		}
		if(send_data(fd, comm.data, comm.header.data_size) < 0) {
			free(comm.data)
			continue;
		}
		free(comm.data);
		get_result(fd, PULL);
	}

	return TRUE;
}


/* close connection
 */
static quit(int fd)
{
	struct command comm;
	comm.header.type = (char) LEAVE;
	comm.header.data_size = 0;
	comm.data = NULL;

	if(send_data(fd, &(comm.header), sizeof(comm.header)) < 0)
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

	if(argc < 4) {
		usage();
		exit(1);
	}

	port = atoi(args[2]);

	sfd = socket(AF_INET, SOCK_STREAM, 0);
	if(sfd < 0) {
		printf("socket failed:%s\n", strerror(errno));
		exit(1);
	}

	bzero(&server_addr, sizeof(struct sockaddr_in));
	
	server_addr.sin_family = AF_INET;
	server_addr.sin_addr.s_addr = inet_addr(args[1]);
	server_addr.sin_port = htons(port);

	if(connect(sfd, (struct sockaddr *)(&server_addr), sizeof(struct sockaddr)) < 0) {
		printf("connect to %s failed:%s\n", args[1], strerror(errno));
		exit(1);
	}

	printf("Connect success!");

	while(read(STDIN_FILENO, buf, 32) > 0) {
		printf("Read Command: %s\n", buf);
		switch(comm_type(buf)) {
			case LIST:
				list_files(sfd);
				break;
			case PULL:
				pull_files(sfd);
				break;
			case DIFF:
				diff(sfd);
				break;
			case LEAVE:
				printf("ByeBye!\n");
				quit(sfd);
				goto out;
				break;
			default:
				printf("Invalid command, retry again\n");
				break;
		}
	
	}

out:
	delete_hash_entries();
}
