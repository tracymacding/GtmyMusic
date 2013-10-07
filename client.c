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
#include <dirent.h>
#include <assert.h>
#include <openssl/md5.h>



struct file_entry
{
	int  size;
	int  m_time;
	char f_md5[33];
	char *f_name;
	struct file_entry *next;
};


#define BOOL int
#define FALSE 0
#define TRUE  1

#define HASH_SIZE 128
#define MISS_FILE_ARRAY_SIZE 100000

struct file_entry* g_fe_hash[HASH_SIZE];
char* g_miss_file_array[MISS_FILE_ARRAY_SIZE];
int g_miss_file_num;
char g_local_dir[256];


static void usage()
{
	printf("usage: ./execname ip port local_file_dir\n");
}


int comm_type(char *buf)
{
	if (buf == NULL)
		return -1;

	if(strcasecmp(buf, "LIST"))
		return 0;
	else if(strcasecmp(buf, "DIFF"))
		return 1;
	else if(strcasecmp(buf, "PULL"))
		return 2;
	else if(strcasecmp(buf, "LEAVE"))
		return 3;
	else 
		return -1;
}


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


static BOOL  compute_file_md5(char *f_name, char *md5, int f_size)
{
	int fd;
	char *data;
	int finished = 0;
	int remain = f_size;
	int ret;
	BOOL res = TRUE;
	unsigned char md[16];
	int i;
	char tmp[3]={'\0'};


	assert(md5 != NULL);

	fd = open(f_name, O_RDONLY);
	if(fd < 0) {
		printf("open file %s failed while compute file md5: %s\n", f_name, strerror(errno));
		return FALSE;
	}

	data = (char *)malloc(f_size);
	if(data == NULL) {
		printf("Malloc failed while compute md5\n");
		close(fd);
		return FALSE;
	}

	for(;;) {
		ret = read(fd, data + finished, remain);
		if(ret < 0) {
			res = FALSE;
			printf("Read file %s failed while compute file md5: %s\n", f_name, strerror(errno));
			goto out;
		}
		finished += ret;
		remain -= ret;
		if(remain <= 0)
			break;
	}

	/* compute md5, using openssl/md5.h */
	MD5(data,f_size,md);
	for (i = 0; i < 16; i++) {
		sprintf(tmp,"%2.2x",md[i]);
		strcat(md5,tmp);
	}

out:
	if(data != NULL)
		free(data);

	if(fd > 0)
		close(fd);

	return res;
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
	int finished = 0;
	int remain = size;

	char *buf = malloc(size);
	if(buf == NULL)
		return FALSE;

	for(;;) 
	{
		if((ret = read(fd, (char *)buf + finished, remain)) < 0) {
			if(ret == EINTR) 
				continue;

			printf("Read response header failed: %s\n", strerror(errno));
			return FALSE;
		}

		finished += ret;
		remain -= ret;
		if(remain <= 0)
			break;
	}

	header->status = *(int *)buf;
	header->data_size = *((int *)buf + 1);

	return TRUE;
}




static BOOL read_response_body(int fd, char *buf, int size)
{
	int finished = 0;
	int remain = size;
	int ret;

	if(buf == NULL) {
		buf = (char *)malloc(size);
		if(buf == NULL)
			return FALSE;
	}

	for(;;) 
	{
		if((ret = read(fd, buf + finished, remain)) < 0) {
			if(ret == EINTR) 
				continue;
			printf("Read response header failed: %s\n", strerror(errno));
			free(buf);
			return FALSE;
		}

		finished += ret;
		remain -= ret;
		if(remain <= 0)
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

	head = g_fe_hash[hash];

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
	int name_len;

	/* delete local file entry hash first */

	delete_hash_entries();

	for(; curr_pos < size;)
	{
		now = data + curr_pos;
		fe_size = *(int *)now;
		now += sizeof(fe_size);
		fe = malloc(sizeof(*fe));
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
		printf("open file %s for storing failed: %s\n", f_name, strerror(errno));
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


static BOOL parse_body(char cmd, char *buf, int size, char *file_name)
{
	BOOL ret = TRUE;

	switch(cmd)
	{
		case LIST:
			ret = deal_with_list(buf, size);
			break;
		case PULL:
			ret = deal_with_pull(file_name, buf, size);
			break;
		default:
			break;
	}

	return ret;
}




static BOOL get_result(int fd, char cmd, char *file_name)
{
	BOOL ret = TRUE;
	struct response res;
	memset(&res, 0, sizeof(res));

	if(read_response_header(fd, &(res.header), sizeof(res.header)) == FALSE) 
		return FALSE;
	

	if(res.header.status != 200) {
		printf("Something error on server!\n");
		return FALSE;
	}

	if(res.header.data_size != 0) {
		if(read_response_body(fd, res.data, res.header.data_size) != TRUE) 
			return FALSE;	

		ret =  parse_body(cmd, res.data, res.header.data_size, file_name);
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

	if(send_data(fd, &(comm.header), sizeof(comm.header)) < 0)
		return FALSE;

	return get_result(fd, LIST, NULL);

}



static BOOL get_miss_files()
{
	int i = 0;
	BOOL ret = TRUE;
	DIR *dir = NULL;
	struct dirent *d_entry = NULL;
	struct file_entry *fe = NULL;
	struct stat fstat;
	int curr_pos = 0;
	int f_entry_size = 0;
	char f_md5[33];
	int name_len;

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

		if(compute_file_md5(d_entry->d_name, f_md5, fstat.st_size) == FALSE)
			goto out;

		fe = malloc(sizeof(*fe));
		if(fe == NULL)
			return FALSE;
		memset(fe, 0, sizeof(*fe));
		fe->size = fstat.st_size;
		fe->m_time = fstat.st_mtime;
		memcpy(fe->f_md5, f_md5, 32);
		fe->f_md5[33] = '\0';
		name_len = strlen(d_entry->d_name);
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
	int i;
	struct command comm;
	memset(&comm, 0, sizeof(comm));

	if(diff(fd) == FALSE)
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
			free(comm.data);
			continue;
		}
		free(comm.data);
		get_result(fd, PULL, g_miss_file_array[i]);
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
	strcpy(g_local_dir, args[3]);

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
