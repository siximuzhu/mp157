/***************************************************************
 Copyright © ALIENTEK Co., Ltd. 1998-2021. All rights reserved.
 文件名 : v4l2_camera.c
 作者 : 邓涛
 版本 : V1.0
 描述 : V4L2摄像头应用编程实战
 其他 : 无
 论坛 : www.openedv.com
 日志 : 初版 V1.0 2021/7/09 邓涛创建
 ***************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <string.h>
#include <errno.h>
#include <sys/mman.h>
#include <linux/videodev2.h>
#include <linux/fb.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include "base64.h"

#define  DISPLAY_BY_ILI9341     0
#define  DISPLAY_BY_FILE        0
#define  DISPLAY_BY_TCP         0
#define  DISPLAY_BY_HTTP        1
      

#define FB_DEV              "/dev/fb0"      //LCD设备节点
#define FRAMEBUFFER_COUNT   3               //帧缓冲数量
#define HOST_IP				"172.16.10.189"
#define PORT			    80

/*** 摄像头像素格式及其描述信息 ***/
typedef struct camera_format {
    unsigned char description[32];  //字符串描述信息
    unsigned int pixelformat;       //像素格式
} cam_fmt;

/*** 描述一个帧缓冲的信息 ***/
typedef struct cam_buf_info {
    unsigned short *start;      //帧缓冲起始地址
    unsigned long length;       //帧缓冲长度
} cam_buf_info;

static int width = 320;                       //LCD宽度
static int height = 240;                      //LCD高度
static int line_length;
static unsigned short *screen_base = NULL;//LCD显存基地址
static int fb_fd = -1;                  //LCD设备文件描述符
static int v4l2_fd = -1;                //摄像头设备文件描述符
static cam_buf_info buf_infos[FRAMEBUFFER_COUNT];
static cam_fmt cam_fmts[10];
static int frm_width, frm_height;   //视频帧宽度和高度

static int fb_dev_init(void)
{
    struct fb_var_screeninfo fb_var = {0};
    struct fb_fix_screeninfo fb_fix = {0};
    unsigned long screen_size;

    /* 打开framebuffer设备 */
    fb_fd = open(FB_DEV, O_RDWR);
    if (0 > fb_fd) {
        fprintf(stderr, "open error: %s: %s\n", FB_DEV, strerror(errno));
        return -1;
    }

    /* 获取framebuffer设备信息 */
    ioctl(fb_fd, FBIOGET_VSCREENINFO, &fb_var);
    ioctl(fb_fd, FBIOGET_FSCREENINFO, &fb_fix);

    screen_size = fb_fix.line_length * fb_var.yres;
    width = fb_var.xres;
    height = fb_var.yres;
    line_length = fb_fix.line_length / (fb_var.bits_per_pixel / 8);

    /* 内存映射 */
    screen_base = mmap(NULL, screen_size, PROT_READ | PROT_WRITE, MAP_SHARED, fb_fd, 0);
    if (MAP_FAILED == (void *)screen_base) {
        perror("mmap error");
        close(fb_fd);
        return -1;
    }

    /* LCD背景刷白 */
    memset(screen_base, 0xFF, screen_size);
    return 0;
}

static int v4l2_dev_init(const char *device)
{
    struct v4l2_capability cap = {0};

    /* 打开摄像头 */
    v4l2_fd = open(device, O_RDWR);
    if (0 > v4l2_fd) {
        fprintf(stderr, "open error: %s: %s\n", device, strerror(errno));
        return -1;
    }

    /* 查询设备功能 */
    ioctl(v4l2_fd, VIDIOC_QUERYCAP, &cap);

    /* 判断是否是视频采集设备 */
    if (!(V4L2_CAP_VIDEO_CAPTURE & cap.capabilities)) {
        fprintf(stderr, "Error: %s: No capture video device!\n", device);
        close(v4l2_fd);
        return -1;
    }

    return 0;
}

static void v4l2_enum_formats(void)
{
    struct v4l2_fmtdesc fmtdesc = {0};

    /* 枚举摄像头所支持的所有像素格式以及描述信息 */
    fmtdesc.index = 0;
    fmtdesc.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    while (0 == ioctl(v4l2_fd, VIDIOC_ENUM_FMT, &fmtdesc)) {

        // 将枚举出来的格式以及描述信息存放在数组中
        cam_fmts[fmtdesc.index].pixelformat = fmtdesc.pixelformat;
        strcpy(cam_fmts[fmtdesc.index].description, fmtdesc.description);
        fmtdesc.index++;
    }
}

static void v4l2_print_formats(void)
{
    struct v4l2_frmsizeenum frmsize = {0};
    struct v4l2_frmivalenum frmival = {0};
    int i;

    frmsize.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    frmival.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    for (i = 0; cam_fmts[i].pixelformat; i++) {
printf("format<0x%x>, description<%s>\n", cam_fmts[i].pixelformat, cam_fmts[i].description); /* 枚举出摄像头所支持的所有视频采集分辨率 */ frmsize.index = 0;
        frmsize.pixel_format = cam_fmts[i].pixelformat;
        frmival.pixel_format = cam_fmts[i].pixelformat;
        while (0 == ioctl(v4l2_fd, VIDIOC_ENUM_FRAMESIZES, &frmsize)) {

            printf("size<%d*%d> ",
                    frmsize.discrete.width,
                    frmsize.discrete.height);
            frmsize.index++;

            /* 获取摄像头视频采集帧率 */
            frmival.index = 0;
            frmival.width = frmsize.discrete.width;
            frmival.height = frmsize.discrete.height;
            while (0 == ioctl(v4l2_fd, VIDIOC_ENUM_FRAMEINTERVALS, &frmival)) {

                printf("<%dfps>", frmival.discrete.denominator /
                        frmival.discrete.numerator);
                frmival.index++;
            }
            printf("\n");
        }
        printf("\n");
    }
}

static int v4l2_set_format(void)
{
    struct v4l2_format fmt = {0};
    struct v4l2_streamparm streamparm = {0};

    /* 设置帧格式 */
    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;//type类型
    fmt.fmt.pix.width = width;  //视频帧宽度
    fmt.fmt.pix.height = height;//视频帧高度
    fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_JPEG; //V4L2_PIX_FMT_RGB565; V4L2_PIX_FMT_JPEG  //像素格式
	if( 0 > ioctl(v4l2_fd,VIDIOC_S_FMT, &fmt)) {
        fprintf(stderr, "ioctl error: VIDIOC_S_FMT: %s\n", strerror(errno));
        return -1;
    }

    /*** 判断是否已经设置为我们要求的RGB565像素格式
    如果没有设置成功表示该设备不支持RGB565像素格式 */
    if (V4L2_PIX_FMT_JPEG != fmt.fmt.pix.pixelformat) {
    //if (V4L2_PIX_FMT_JPEG != fmt.fmt.pix.pixelformat) {
        fprintf(stderr, "Error: the device does not support RGB565 format!\n");
        return -1;
    }

    frm_width = fmt.fmt.pix.width;  //获取实际的帧宽度
    frm_height = fmt.fmt.pix.height;//获取实际的帧高度
    printf("视频帧大小<%d * %d>\n", frm_width, frm_height);

    /* 获取streamparm */
    streamparm.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    ioctl(v4l2_fd, VIDIOC_G_PARM, &streamparm);

    /** 判断是否支持帧率设置 **/
    if (V4L2_CAP_TIMEPERFRAME & streamparm.parm.capture.capability) {
        streamparm.parm.capture.timeperframe.numerator = 1;
        streamparm.parm.capture.timeperframe.denominator = 30;//30fps
        if (0 > ioctl(v4l2_fd, VIDIOC_S_PARM, &streamparm)) {
            fprintf(stderr, "ioctl error: VIDIOC_S_PARM: %s\n", strerror(errno));
            return -1;
        }
    }

    return 0;
}

static int v4l2_init_buffer(void)
{
    struct v4l2_requestbuffers reqbuf = {0};
    struct v4l2_buffer buf = {0};

    /* 申请帧缓冲 */
    reqbuf.count = FRAMEBUFFER_COUNT;       //帧缓冲的数量
    reqbuf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    reqbuf.memory = V4L2_MEMORY_MMAP;
    if (0 > ioctl(v4l2_fd, VIDIOC_REQBUFS, &reqbuf)) {
        fprintf(stderr, "ioctl error: VIDIOC_REQBUFS: %s\n", strerror(errno));
        return -1;
    }

    /* 建立内存映射 */
    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_MMAP;
    for (buf.index = 0; buf.index < FRAMEBUFFER_COUNT; buf.index++) {

        ioctl(v4l2_fd, VIDIOC_QUERYBUF, &buf);
        buf_infos[buf.index].length = buf.length;
        buf_infos[buf.index].start = mmap(NULL, buf.length,
                PROT_READ | PROT_WRITE, MAP_SHARED,
                v4l2_fd, buf.m.offset);
        if (MAP_FAILED == buf_infos[buf.index].start) {
            perror("mmap error");
            return -1;
        }
    }

#if 1
    /* 入队 */
    for (buf.index = 0; buf.index < FRAMEBUFFER_COUNT; buf.index++) {

        if (0 > ioctl(v4l2_fd, VIDIOC_QBUF, &buf)) {
            fprintf(stderr, "ioctl error: VIDIOC_QBUF: %s\n", strerror(errno));
            return -1;
        }
    }
#endif
    return 0;
}

static int v4l2_stream_on(void)
{
    /* 打开摄像头、摄像头开始采集数据 */
    enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

    if (0 > ioctl(v4l2_fd, VIDIOC_STREAMON, &type)) {
        fprintf(stderr, "ioctl error: VIDIOC_STREAMON: %s\n", strerror(errno));
        return -1;
    }

    return 0;
}

/*
static void v4l2_read_data(void)
{
    struct v4l2_buffer buf = {0};
    unsigned short *base;
    unsigned short *start;
    int min_w, min_h;
    int j;

    if (width > frm_width)
        min_w = frm_width;
    else
        min_w = width;
    if (height > frm_height)
        min_h = frm_height;
    else
        min_h = height;

    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_MMAP;
    for ( ; ; ) {

        for(buf.index = 0; buf.index < FRAMEBUFFER_COUNT; buf.index++) {

            ioctl(v4l2_fd, VIDIOC_DQBUF, &buf);     //出队
            for (j = 0, base=screen_base, start=buf_infos[buf.index].start;
                        j < min_h; j++) {

                memcpy(base, start, min_w * 2); //RGB565 一个像素占2个字节
                base += line_length;  //LCD显示指向下一行
                start += frm_width;//指向下一行数据
            }

            // 数据处理完之后、再入队、往复
            ioctl(v4l2_fd, VIDIOC_QBUF, &buf);
        }
    }
}
*/

int socket_connect_server(char *ip,int port)
{
	int sockfd;
	struct hostent *he;
	struct sockaddr_in server;
	char recv_data[10];

	if((he = gethostbyname(ip)) == NULL){
		printf("gethostbyname() error\n");
		return -1;
	}

	if((sockfd = socket(AF_INET,SOCK_STREAM,0)) == -1){
		printf("socket() error\n");
		return -2;
	}
	bzero(&server,sizeof(server));
	server.sin_family = AF_INET;
	server.sin_port = htons(port);
	server.sin_addr = *((struct in_addr *)he->h_addr);
	if(connect(sockfd,(struct sockaddr*)&server,sizeof(server)) == -1){
		printf("connect() error\n");
		return -3;
	}
	send(sockfd,"mp157",5,0);
	recv(sockfd,recv_data,7,0);

	return sockfd;
	
}

int http_connect_server()
{
    int sockfd;
    struct sockaddr_in servaddr;

    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0 ) {
            printf("创建网络连接失败,本线程即将终止---socket error!\n");
            exit(-1);
    };

    bzero(&servaddr, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(PORT);
    if (inet_pton(AF_INET, HOST_IP, &servaddr.sin_addr) <= 0 ){
            printf("创建网络连接失败,本线程即将终止--inet_pton error!\n");
            exit(0);
    };

    if (connect(sockfd, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0){
            printf("连接到服务器失败,connect error!\n");
            exit(0);
    }
    printf("与远端建立了连接\n");

    return sockfd;
}

int http_post_data(int sockfd,char *data,unsigned int len)
{
    char *post_data;
    char str_len[10],read_buf[1024];
    fd_set   t_set1;
    struct timeval  tv;
    int ret;


    memset(str_len,0x0,sizeof(str_len));
    memset(read_buf,0x0,sizeof(read_buf));

    post_data = (char *)malloc(1 * 1024 * 1024);
    if(!post_data)
    {
        printf("post data malloc error\n");
        return -1;
    }
 

    memset(post_data,0x0, 1 *1024 * 1024);

    strcat(post_data, "POST /stream HTTP/1.1\r\n");
    strcat(post_data, "Host: 172.16.10.189:80\r\n");
    strcat(post_data, "Accept: */*\r\n");
    strcat(post_data, "Content-Type: application/json\r\n");
    strcat(post_data, "Content-Length: ");

    sprintf(str_len,"%d",len);
    strcat(post_data, str_len);
    strcat(post_data, "\r\n\r\n");

    //"{\"username\":\"luozhu\",\"password\":\"12343\"}"
    memcpy(post_data + strlen(post_data),data,len);
    strcat(post_data, "\r\n\r\n");

    //printf("%s\n",post_data);

    printf("start write fd = %d,ptr = %p,len = %d\n",sockfd,post_data,strlen(post_data));

    ret = write(sockfd,post_data,strlen(post_data));
    printf("write over!\n");
    if (ret < 0) 
    {
        printf("发送失败！错误代码是%d，错误信息是'%s'\n",errno, strerror(errno));
        exit(-1);
    }
    else
    {
        printf("消息发送成功，共发送了%d个字节！\n\n", ret);
    }

    FD_ZERO(&t_set1);
    FD_SET(sockfd, &t_set1);

    while(1)
    {
        tv.tv_sec= 20;
        tv.tv_usec= 0;
      
        ret = select(sockfd +1, &t_set1, NULL, NULL, &tv);

        if (ret == 0) 
        {
            printf("timeout\n");
            continue;
        }
        if (ret < 0) {
            close(sockfd);
            printf("在读取数据报文时SELECT检测到异常，该异常导致线程终止！\n");
            free(post_data);
            return -2;
        };

        if (ret > 0){
            //ret = read(sockfd, read_buf, 1024);
            ret = recv(sockfd,read_buf, 1024,0);
            if (ret == 0)
            {
                close(sockfd);
                printf("读取数据报文时发现远端关闭，该线程终止！\n");
                free(post_data);
                return -3;
            }

            printf("ret = %d ,%s\n", ret,read_buf);
            break;
        }       
    }

    free(post_data);
    return 0;
}

char http_data[3 * 1024 *1024];
char frame_buf[3 * 1024 * 1024];
unsigned int frame_len = 0;
int push_images(void)
{
	struct v4l2_buffer buf = {0};
	char buffer[256];
	char recv_data[10];
    char temp;

#if DISPLAY_BY_TCP
	int sockfd = socket_connect_server(HOST_IP,PORT);
	if(sockfd < 0)
	{
		return -1;
	}	
#endif

#if DISPLAY_BY_ILI9341
    int fbfd = open("/dev/ili9341",O_RDWR);
    if(fbfd < 0)
    {
        perror("open /dev/ili9314 error:");
        return -1;
    }else{
        printf("open /dev/ili9341 success\n");
    }
#endif

#if DISPLAY_BY_HTTP
    int sockfd =  http_connect_server();

#endif

    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_MMAP;
	for(int j = 0;j < 100;j++)
	{
		
		for(buf.index = 0; buf.index < FRAMEBUFFER_COUNT; buf.index++)
		{
			
			if(ioctl(v4l2_fd, VIDIOC_DQBUF, &buf) < 0)     //出队
			{
				printf("DQBUF fail\n");
				return -2;
			}
			frame_len = buf.bytesused;
			memcpy(frame_buf,buf_infos[buf.index].start,frame_len);

#if DISPLAY_BY_FILE
			sprintf(buffer,"/workspace/%d.jpg",j * FRAMEBUFFER_COUNT + buf.index);
			int file_fd = open(buffer,O_RDWR | O_CREAT); // 若打开失败则不存储该帧图像
			if(file_fd > 0){
				printf("saving %d images\n",j * FRAMEBUFFER_COUNT + buf.index);
				write(file_fd,frame_buf,frame_len);
				close(file_fd);
			}
#endif

#if DISPLAY_BY_ILI9341
            if(frame_len != (320 * 240 *2))
            {
                printf("len error:%d\n",frame_len);
                continue;
            }
            
            for(int m = 0;m < 320 * 240 * 2;m += 2)
            {
                temp = frame_buf[m];
                frame_buf[m] = frame_buf[m + 1];
                frame_buf[m + 1] = temp;
            }
 
            if(write(fbfd,frame_buf,frame_len))
            {
                perror("write error:");
            }
            else
            {
                printf("write success times = %d\n",j * FRAMEBUFFER_COUNT + buf.index);
            }
#endif 

#if DISPLAY_BY_TCP

			char str_len[10];
			memset(str_len,0x0,10);
			memcpy(str_len,&(frame_len),sizeof(frame_len));
			printf("send len = %d\n",frame_len);
			send(sockfd,str_len,10,0);
			memset(recv_data,0x0,10);
			recv(sockfd,recv_data,7,0);
			printf("recv1:%s\n",recv_data);
			usleep(100 * 1000);

			printf("send data\n");
			send(sockfd,frame_buf,frame_len,0);
			printf("0x%04x 0x%04x   \n",frame_buf[0],frame_buf[frame_len / 2 - 1]);
			memset(recv_data,0x0,10);
			recv(sockfd,recv_data,7,0);
			printf("recv2:%s\n",recv_data);
			usleep(200 * 1000);
#endif

#if DISPLAY_BY_HTTP
            char *image_base64;
            size_t out_len;

            printf("start base64\n");
            image_base64 = base64_encode(frame_buf,frame_len,&out_len);
            sprintf(http_data,"{\"image_len\":%d,\"image_data\":\"%s\"}",out_len,image_base64);
            
            printf("start post\n");
            http_post_data(sockfd,http_data,strlen(http_data));
            printf("end post\n");
            free(image_base64);


#endif


#if 1
			if(ioctl(v4l2_fd, VIDIOC_QBUF, &buf) < 0)
			{
				printf("QBUF fail\n");
				return -3;
			}
#endif
	
		}
	}
#if DISPLAY_BY_ILI9341
    close(fbfd);
#endif

    return 0;
	
}


int main(int argc, char *argv[])
{
    if (2 != argc) {
        fprintf(stderr, "Usage: %s <video_dev>\n", argv[0]);
        exit(EXIT_FAILURE);
    }
#if 1
    /* 初始化LCD */
//    if (fb_dev_init())
//        exit(EXIT_FAILURE);

    /* 初始化摄像头 */
    if (v4l2_dev_init(argv[1]))
        exit(EXIT_FAILURE);

    /* 枚举所有格式并打印摄像头支持的分辨率及帧率 */
    v4l2_enum_formats();
    v4l2_print_formats();

    /* 设置格式 */
    if (v4l2_set_format())
        exit(EXIT_FAILURE);

    /* 初始化帧缓冲：申请、内存映射、入队 */
    if (v4l2_init_buffer())
        exit(EXIT_FAILURE);

    /* 开启视频采集 */
    if (v4l2_stream_on())
        exit(EXIT_FAILURE);

    /* 读取数据：出队 */
    //v4l2_read_data();       //在函数内循环采集数据、将其显示到LCD屏
	push_images();
#else

    char *base64_data;
    size_t out_len;
    char *data = "{\"username\":\"luozhu\",\"password\":\"12343\"}";

    base64_data = base64_encode(data,strlen(data),&out_len);


    int sockfd =  http_connect_server();
    http_post_data(sockfd,base64_data,strlen(base64_data));

    free(base64_data);
#endif

    exit(EXIT_SUCCESS);
}
