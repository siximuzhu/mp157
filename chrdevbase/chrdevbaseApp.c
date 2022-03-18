#include "stdio.h"
#include "unistd.h"
#include "sys/types.h"
#include "sys/stat.h"
#include "fcntl.h"
#include "stdlib.h"
#include "string.h"

static char usrdata[] = {"usr data!"};

int main(int argc,char *argv[])
{
	int fd,retvalue;
	char *filename;
	char readbuf[100],writebuf[100];

	if(argc != 3)
	{
		printf("Error Usage!\r\n");
		return -1;
	}
	
	filename = argv[1];

	fd = open(filename,O_RDWR);

	if(fd < 0)
	{
		printf("cant open file %s\n",filename);
		return -1;
	}

	if(atoi(argv[2]) == 1)
	{
		retvalue = read(fd,readbuf,50);
		if(retvalue < 0)
		{
			printf("read file %s failed!\n",filename);
		}
		else
		{
			printf("read data:%s\n",readbuf);
		}
	}

	if(atoi(argv[2]) == 2)
	{
		memcpy(writebuf,usrdata,sizeof(usrdata));
		retvalue = write(fd,writebuf,50);
		if(retvalue < 0)
		{
			printf("write file %s failed!\n",filename);
		}
		else
		{
			printf("write file:%s success!\n",writebuf);	
		}
	}

	retvalue = close(fd);

	if(retvalue < 0)
	{
		printf("cant close file %s\n",filename);
		return -1;
	}
	else
	{
		printf("close file %s success!\n",filename);	
	}

	return 0;

}
