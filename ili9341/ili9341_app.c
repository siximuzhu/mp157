/***************************************************************
Copyright © ALIENTEK Co., Ltd. 1998-2029. All rights reserved.
文件名		: icm20608App.c
作者	  	: 正点原子Linux团队
版本	   	: V1.0
描述	   	: icm20608设备测试APP。
其他	   	: 无
使用方法	 ：./icm20608App /dev/icm20608
论坛 	   	: www.openedv.com
日志	   	: 初版V1.0 2021/03/22 正点原子Linux团队创建
***************************************************************/
#include "stdio.h"
#include "unistd.h"
#include "sys/types.h"
#include "sys/stat.h"
#include "sys/ioctl.h"
#include "fcntl.h"
#include "stdlib.h"
#include "string.h"
#include <poll.h>
#include <sys/select.h>
#include <sys/time.h>
#include <signal.h>
#include <fcntl.h>


#define DEVICE_NAME							"/dev/ili9341"
#define FRAME_SIZE							(320 * 240 * 2)

/*
 * @description		: main主程序
 * @param - argc 	: argv数组元素个数
 * @param - argv 	: 具体参数
 * @return 			: 0 成功;其他 失败
 */
int main(int argc, char *argv[])
{
	int fd;
	char rgb_frame[FRAME_SIZE];

	memset(rgb_frame,0x55,FRAME_SIZE);


	fd = open(DEVICE_NAME, O_RDWR);
	if(fd < 0) {
		printf("can't open file %s\r\n", DEVICE_NAME);
		return -1;
	}

	if(write(fd,rgb_frame,FRAME_SIZE))
	{
		perror("write error:");
	}
	else
	{
		printf("write success\n");
	}

	close(fd);	/* 关闭文件 */	
	return 0;
}

