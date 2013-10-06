#include "command.h"


static BOOL read_command_header(int fd, struct command_header *header)
{
	char *buf = NULL;
	int size = sizeof(*header);
	int ret = 0;
	assert(fd > 0);
	assert(header != NULL);
	
	buf = (char *)malloc(size);
	if(buf == NULL)
		return FALSE;
	memset(buf, 0, size);

	for(;;) {
		ret = read(fd, header, size);
		if(ret < 0) {
			if(ret == EINTR)
				continue;
			free(buf);
			return FALSE;
		}
		size -= ret;
		if(size <= 0) {
			header->type = *(char *)buf;
			header->size = *(int *)(char *)buf + 1);
			break;
		}
	}
	
	free(buf);
	return TRUE;
}






/* read command data from socket @fd */
static BOOL read_command_body(int fd, void **buf, int size)
{
	int ret = 0;
	assert(fd > 0);
	assert(size > 0);

	if(*buf == NULL) {
		if((*buf = malloc(size)) == NULL)
			return FALSE;
	}

	memset(*buf, 0, size);

	for(;;) {
		ret = read(fd, *buf, size);
		if(ret < 0) {
			if(ret == EINTR)
				continue;
			free(*buf);
			buf = NULL;
			return FALSE;
		}
		size -= ret;
		if(size <= 0) 
			break;
	}

	return TRUE;
}



/* read command from socket @fd
 * Return: command if success
 *         NULL    if failed
 */
BOOL read_command(int fd, struct command *comm)
{
	char command;
	BOOL res = TRUE;

	assert(comm != NULL);
	assert(fd > 0);

	memset(&header, 0, sizeof(header));

	/* read command header */
	res = read_command_header(fd, &(comm->header));
	if(res == FALSE) {
		printf("Read command header failed of %d\n");
		return FALSE;
	}

	command = comm->header->type;

	/* read command data if neccessary */
	switch(command) {
		case LIST:
		case LEAVE:
			break;
		/* have no idea how to deal with DIFF command*/
		case DIFF:
			;
			break;
		case PULL:
			res = read_command_body(fd, comm->data, comm->header.size);
			break;
		default:
			printf("Invalid command\n");
			res = FALSE;
	}

	return res;
}






/* process command @comm
 * Return  0  if success
 *         -1 if failed
 * */
BOOL process_command(struct command *comm, struct response *res)
{
	char command = comm->header.type;
	switch(command) {
		case LIST:
			return list_current_files(res);
			break;
		case DIFF:
		case PULL:
			return read_file_data((char *)comm->data, res);
			break;
		case LEAVE:
			break;
		default:
			return FALSE;
			break;
	}
}



/* read file data and store in response */
static BOOL read_file_data(char *file_name, struct response *res)
{
	BOOL success = TRUE;
	struct stat file_stat;
	int fd;
	int read = 0;
	int remain = 0;

	if(stat(file_name, &file_stat) < 0) {
		printf("get file %s stat failed: %s\n", file_name, strerror(errno));
		return FALSE;
	}
	
	if((fd = open(file_name, O_RDONLY)) < 0) {
		printf("open file %s failed: %s\n", file_name, strerror(errno));
		return FALSE;
	}

	if(res->data == NULL) {
		if((res->data = malloc(file_stat.st_size)) == NULL) {
			close(fd);
			return FALSE;
		}
		memset(res->data, 0, file_stat.st_size);
	}
	
	remain = file_stat.st_size;

	for(; ;) {
		ret = read(fd, (char *)res->data + read, remain);
		if(ret < 0) {
			printf("read file %s failed: %s\n", file_name, strerror(errno));
			success = FALSE;
			goto out;
		}
		remain -= ret;
		read += ret;
		if(remain <= 0)
			break;
	}

out:
	close(fd);
	if(success != TRUE && res->data != NULL)
		free(res->data);
	return success;
}



/* list current files */
static BOOL list_current_files(struct response *res)
{
	BOOL ret = TRUE;
	DIR *dir = NULL;
	struct dirent *d_entry = NULL;
	struct file_entry fe;
	struct stat fstat;
	int curr_pos = 0;
	int f_entry_size = 0;

	dir = opendir(g_dir);
	if(dir == NULL) {
		printf("opendir %s failed: %s\n", g_dir, strerror(errno));
		return FALSE;
	}

	if(res->data == NULL) {
		/* tmp solution, fixed bufer size = 8MB */
		if((res->data = malloc(FILE_ENTRIES_SIZE)) == NULL) {
			ret = FALSE;
			goto out;
		}
		memset(res->data, 0, FILE_ENTRIES_SIZE);
	}

	/* read all file entry and get it's stat */
	while((d_entry = readdir(dir)) != NULL) {
		if(d_entry->d_name[0] == '.')
			continue;
		
		if(stat(d_entry->dname, &fstat) < 0) {
			printf("stat file %s failed: %s\n", d_entry->d_name, strerror(errno));
			ret = FALSE;
			goto out;
		}
		f_entry_size = strlen(d_entry->d_name) + sizeof(fstat.st_size) + sizeof(fstat.st_mtime);

		assert(curr_pos < FILE_ENTRIES_SIZE);
		assert(curr_pos + f_entry_size < FILE_ENTRIES_SIZE);
		
		memcpy((char *)res->data + curr_pos, &f_entry_size,  sizeof(f_entry_size));
		curr_pos += sizeof(f_entry_size);
		memcpy((char *)res->data + curr_pos, d_entry->d_name, strlen(d_entry->d_name));
		curr_pos += strlen(d_entry->d_name);
		memcpy((char *)res->data + curr_pos, &(fstat.st_size), sizeof(fstat.st_size));
		curr_pos += sizeof(fstat.st_size);
		memcpy(char *)res->data + curr_pos, &(fstat.st_mtime), sizeof(fstat.m_time));
		curr_pos += sizeof(fstat.st_mtime);
		
		res->data_size += f_entry_size + sizeof(f_entry_size);
	}

out:
	if(ret != TRUE && res->data != NULL)
			free(res->data);

	closedir(dir);
	return ret;
}
