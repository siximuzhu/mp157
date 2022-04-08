/***************************************************************
Copyright © ALIENTEK Co., Ltd. 1998-2029. All rights reserved.
文件名		: ili9341.c
作者	  	: 正点原子Linux团队
版本	   	: V1.0
描述	   	: ili9341 SPI驱动程序
其他	   	: 无
论坛 	   	: www.openedv.com
日志	   	: 初版V1.0 2021/03/22 正点原子Linux团队创建

SPI CS 		 	GPIOZ5
SPI CLK			GPIOZ0
SPI MOSI		GPIOZ2
SPI MISO		GPIOZ1
RESET			GPIOF6
DC 				GPIOF7



***************************************************************/
#include <linux/spi/spi.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/ide.h>
#include <linux/errno.h>
#include <linux/platform_device.h>
#include "icm20608reg.h"
#include <linux/gpio.h>
#include <linux/device.h>
#include <asm/uaccess.h>
#include <linux/cdev.h>

#define ILI9341_CNT	1
#define ILI9341_NAME	"ili9341"

#define USE_HORIZONTAL 2  //设置横屏或者竖屏显示 0或1为竖屏 2或3为横屏

#if USE_HORIZONTAL==0||USE_HORIZONTAL==1
#define LCD_W 240
#define LCD_H 320

#else
#define LCD_W 320
#define LCD_H 240
#endif

//画笔颜色
#define WHITE         	 0xFFFF
#define BLACK         	 0x0000	  
#define BLUE           	 0x001F  
#define BRED             0XF81F
#define GRED 			       0XFFE0
#define GBLUE			       0X07FF
#define RED           	 0xF800
#define MAGENTA       	 0xF81F
#define GREEN         	 0x07E0
#define CYAN          	 0x7FFF
#define YELLOW        	 0xFFE0
#define BROWN 			     0XBC40 //棕色
#define BRRED 			     0XFC07 //棕红色
#define GRAY  			     0X8430 //灰色
#define DARKBLUE      	 0X01CF	//深蓝色
#define LIGHTBLUE      	 0X7D7C	//浅蓝色  
#define GRAYBLUE       	 0X5458 //灰蓝色
#define LIGHTGREEN     	 0X841F //浅绿色
#define LGRAY 			     0XC618 //浅灰色(PANNEL),窗体背景色
#define LGRAYBLUE        0XA651 //浅灰蓝色(中间层颜色)
#define LBBLUE           0X2B12 //浅棕蓝色(选择条目的反色)

/* 寄存器物理地址 */
#define PERIPH_BASE     		     	(0x40000000)
#define MPU_AHB4_PERIPH_BASE			(PERIPH_BASE + 0x10000000)
#define RCC_BASE        		    	(MPU_AHB4_PERIPH_BASE + 0x0000)	
#define RCC_MP_AHB4ENSETR				(RCC_BASE + 0XA28)
#define GPIOF_BASE						(MPU_AHB4_PERIPH_BASE + 0x7000)	
#define GPIOF_MODER      			    (GPIOF_BASE + 0x0000)	
#define GPIOF_OTYPER      			    (GPIOF_BASE + 0x0004)	
#define GPIOF_OSPEEDR      			    (GPIOF_BASE + 0x0008)	
#define GPIOF_PUPDR      			    (GPIOF_BASE + 0x000C)	
#define GPIOF_ODR      			    (GPIOF_BASE + 0x0014)
#define GPIOF_BSRR      			    (GPIOF_BASE + 0x0018)

/* 映射后的寄存器虚拟地址指针 */
static void __iomem *MPU_AHB4_PERIPH_RCC_PI;
static void __iomem *GPIOF_MODER_PI;
static void __iomem *GPIOF_OTYPER_PI;
static void __iomem *GPIOF_OSPEEDR_PI;
static void __iomem *GPIOF_PUPDR_PI;
static void __iomem *GPIOF_ODR_PI;
static void __iomem *GPIOF_BSRR_PI;


struct ili9341_dev {
	struct spi_device *spi;		/* spi设备 */
	dev_t devid;				/* 设备号 	 */
	struct cdev cdev;			/* cdev 	*/
	struct class *class;		/* 类 		*/
	struct device *device;		/* 设备 	 */
	struct device_node	*nd; 	/* 设备节点 */
};

struct ili9341_dev *ili9341dev;

static void ili9341_res_clr(void)
{
	u32 val;
	val = readl(GPIOF_ODR_PI);
	val &= ~(0x1 << 6);
	writel(val,GPIOF_ODR_PI);
}

static void ili9341_res_set(void)
{
	u32 val;
	val = readl(GPIOF_ODR_PI);
	val |= (0x1 << 6);
	writel(val,GPIOF_ODR_PI);
}

static void ili9341_blk_clr(void)
{

}

static void ili9341_blk_set(void)
{

}

static void ili9341_dc_clr(void)
{
	u32 val;
	val = readl(GPIOF_ODR_PI);
	val &= ~(0x1 << 7);
	writel(val,GPIOF_ODR_PI);
}

static void ili9341_dc_set(void)
{
	u32 val;
	val = readl(GPIOF_ODR_PI);
	val |= (0x1 << 7);
	writel(val,GPIOF_ODR_PI);
}

static int ili9341_write_spi(unsigned char *data,unsigned int len)
{
	int ret = -1;
	unsigned char *txdata;
	struct spi_message m;
	struct spi_transfer *t;
	struct spi_device *spi = (struct spi_device *)ili9341dev->spi;
	
	t = kzalloc(sizeof(struct spi_transfer), GFP_KERNEL);	/* 申请内存 */
	if(!t) {
		printk("mem error1\n");
		return -ENOMEM;
	}

	txdata = kzalloc(len,GFP_KERNEL);
	if(!txdata)
	{
		printk("mem error2\n");
		goto out1;
	}
	
	memcpy(txdata,data,len);
	t->tx_buf = txdata;			/* 要发送的数据 */
	t->len = len;				/* t->len=发送的长度+读取的长度 */
//	t->speed_hz = 30000000;
//	t->effective_speed_hz = 30000000;
	spi_message_init(&m);		/* 初始化spi_message */
	spi_message_add_tail(t, &m);/* 将spi_transfer添加到spi_message队列 */
	ret = spi_sync(spi, &m);	/* 同步发送 */
	if(ret)
	{
		printk("spi err\n");
		goto out2;
	}
	//printk("%02x %d\n",txdata[0],len);

out2:
	kfree(txdata);

out1:	
	kfree(t);					/* 释放内存 */
	return ret;
}

static void ili9341_write_reg(unsigned char data)
{
	ili9341_dc_clr();  
	ili9341_write_spi(&data,1);
	ili9341_dc_set();
}

static void ili9341_write_data8(unsigned char data)
{
	ili9341_write_spi(&data,1);
}

static void ili9341_write_data16(unsigned short data)
{
	u8 v1 = data >> 8;
	u8 v2 = data;
	ili9341_write_spi(&v1,1);
	ili9341_write_spi(&v2,1);
}

void ili9341_address_set(u16 x1,u16 y1,u16 x2,u16 y2)
{
		ili9341_write_reg(0x2a);//列地址设置
		ili9341_write_data16(x1);
		ili9341_write_data16(x2);
		ili9341_write_reg(0x2b);//行地址设置
		ili9341_write_data16(y1);
		ili9341_write_data16(y2);
		ili9341_write_reg(0x2c);//储存器写
}

void ili9341_fill(u16 xsta,u16 ysta,u16 xend,u16 yend,u16 color)
{          
	u16 i,j; 
	int k;
	int frame_size = 320 * 240 *2;
	char *val;

	val = kzalloc(frame_size,GFP_KERNEL);
	if(!val)
	{
		printk("mem error\n");
		return;
	}

	ili9341_address_set(xsta,ysta,xend-1,yend-1);//设置显示范围
/*	for(i=ysta;i<yend;i++)
	{													   	 	
		for(j=xsta;j<xend;j++)
		{
			ili9341_write_data16(0x001F);
		}
	} 	*/
	memset(val,0x0,320 * 240 *2);
	for(k = 0;k < 320 * 240 *2;k++)
	{	if((k % 2))
			val[k] = 0x1f;
	}	
	

	for(k = 0;k < 240;k++)
	{
		ili9341_write_spi(val + 320 * 2 * k,320 * 2);
	}

	kfree(val);
			  	    
}

void ili9341_draw_picture(u16 xsta,u16 ysta,u16 xend,u16 yend,u8 *rgb_565)
{
	u32 i;

	ili9341_address_set(xsta,ysta,xend-1,yend-1);//设置显示范围
	for(i = 0;i < 240;i++)
	{
		ili9341_write_spi(rgb_565 + 320 * 2 * i,320 * 2);
	}

}



/*
 * @description		: 打开设备
 * @param - inode 	: 传递给驱动的inode
 * @param - filp 	: 设备文件，file结构体有个叫做pr似有ate_data的成员变量
 * 					  一般在open的时候将private_data似有向设备结构体。
 * @return 			: 0 成功;其他 失败
 */
static int ili9341_open(struct inode *inode, struct file *filp)
{
	return 0;
}

static ssize_t ili9341_write(struct file *filp,const char __user *buf, size_t cnt, loff_t *off)
{
	u8* rgb_565;

	rgb_565 = kzalloc(320 * 240 * 2,GFP_KERNEL);
	if(!rgb_565)
	{
		printk("write mem error!\n");
		return -ENOMEM;
	}
	copy_from_user(rgb_565,buf,cnt);
	ili9341_draw_picture(0,0,LCD_W,LCD_H,rgb_565);

	return 0;
}

static ssize_t ili9341_read(struct file *filp, char __user *buf, size_t cnt, loff_t *off)
{


	return 0;
}



/*
 * @description		: 关闭/释放设备
 * @param - filp 	: 要关闭的设备文件(文件描述符)
 * @return 			: 0 成功;其他 失败
 */
static int ili9341_release(struct inode *inode, struct file *filp)
{
	return 0;
}

/* ili9341操作函数 */
static const struct file_operations ili9341_ops = {
	.owner = THIS_MODULE,
	.open = ili9341_open,
	.write = ili9341_write,
	.read = ili9341_read,
	.release = ili9341_release,
};

static void ili9341_gpio_init(void)
{
	u32 i;
	u32 val;
	/* 1、寄存器地址映射 */
    MPU_AHB4_PERIPH_RCC_PI = ioremap(RCC_MP_AHB4ENSETR, 4);
    GPIOF_MODER_PI = ioremap(GPIOF_MODER, 4);
    GPIOF_OTYPER_PI = ioremap(GPIOF_OTYPER, 4);
    GPIOF_OSPEEDR_PI = ioremap(GPIOF_OTYPER, 4);
    GPIOF_PUPDR_PI = ioremap(GPIOF_PUPDR, 4);
	GPIOF_ODR_PI = ioremap(GPIOF_ODR, 4);
    GPIOF_BSRR_PI = ioremap(GPIOF_BSRR, 4);

    /* 2、使能GPIOF时钟 */
    val = readl(MPU_AHB4_PERIPH_RCC_PI);
    val &= ~(0X1 << 5); /* 清除以前的设置 */
    val |= (0X1 << 5);  /* 设置新值 */
    writel(val, MPU_AHB4_PERIPH_RCC_PI);


    /* 3、设置GPIOF6通用的输出模式。*/
    val = readl(GPIOF_MODER_PI);
    val &= ~(0X3 << 12); /* bit0:1清零 */
    val |= (0X1 << 12);  /* bit0:1设置01 */
    writel(val, GPIOF_MODER_PI);

    /* 3、设置GPIOF6 7为推挽模式。*/
    val = readl(GPIOF_OTYPER_PI);
    val &= ~(0X3 << 6); /* bit0清零，设置为上拉*/
    writel(val, GPIOF_OTYPER_PI);

    /* 4、设置PI0为高速。*/
    val = readl(GPIOF_OSPEEDR_PI);
    val &= ~(0X3 << 12); /* bit0:1 清零 */
    val |= (0x2 << 12); /* bit0:1 设置为10*/
    writel(val, GPIOF_OSPEEDR_PI);

    /* 5、设置PI0为上拉。*/
    val = readl(GPIOF_PUPDR_PI);
    val &= ~(0X3 << 12); /* bit0:1 清零*/
    val |= (0x1 << 12); /*bit0:1 设置为01*/
    writel(val,GPIOF_PUPDR_PI);



	    /* 3、设置GPIOF6通用的输出模式。*/
    val = readl(GPIOF_MODER_PI);
    val &= ~(0X3 << 14); /* bit0:1清零 */
    val |= (0X1 << 14);  /* bit0:1设置01 */
    writel(val, GPIOF_MODER_PI);

    /* 4、设置PI0为高速。*/
    val = readl(GPIOF_OSPEEDR_PI);
    val &= ~(0X3 << 14); /* bit0:1 清零 */
    val |= (0x2 << 14); /* bit0:1 设置为10*/
    writel(val, GPIOF_OSPEEDR_PI);

    /* 5、设置PI0为上拉。*/
    val = readl(GPIOF_PUPDR_PI);
    val &= ~(0X3 << 14); /* bit0:1 清零*/
    val |= (0x1 << 14); /*bit0:1 设置为01*/
    writel(val,GPIOF_PUPDR_PI);

	val = readl(GPIOF_ODR_PI);
	val |= (0x3 << 6);
	writel(val,GPIOF_ODR_PI);

/*
	val = readl(GPIOF_ODR_PI);
	val &= ~(0x3 << 6);
	writel(val,GPIOF_ODR_PI); */

}

/*
 * ILI9341内部寄存器初始化函数 
 * @param - spi : 要操作的设备
 * @return 	: 无
 */
void ili9341_lcd_init(struct ili9341_dev *dev)
{
	ili9341_gpio_init();

	ili9341_res_clr();
	mdelay(100);
	ili9341_res_set();
	mdelay(100);
	ili9341_blk_set();
	mdelay(100);

	ili9341_write_reg(0x11);   //sleep out
	mdelay(120);

	ili9341_write_reg(0xcf);
	ili9341_write_data8(0x00);
	ili9341_write_data8(0xc1);
	ili9341_write_data8(0x30);

	ili9341_write_reg(0xED);
	ili9341_write_data8(0x64);
	ili9341_write_data8(0x03);
	ili9341_write_data8(0X12);
	ili9341_write_data8(0X81);

	ili9341_write_reg(0xE8);
	ili9341_write_data8(0x85);
	ili9341_write_data8(0x00);
	ili9341_write_data8(0x79);

	ili9341_write_reg(0xCB);
	ili9341_write_data8(0x39);
	ili9341_write_data8(0x2C);
	ili9341_write_data8(0x00);
	ili9341_write_data8(0x34);
	ili9341_write_data8(0x02);

	ili9341_write_reg(0xF7);
	ili9341_write_data8(0x20);

	ili9341_write_reg(0xEA);
	ili9341_write_data8(0x00);
	ili9341_write_data8(0x00);

	ili9341_write_reg(0xC0); //Power control
	ili9341_write_data8(0x1D); //VRH[5:0]

	ili9341_write_reg(0xC1); //Power control
	ili9341_write_data8(0x12); //SAP[2:0];BT[3:0]

	ili9341_write_reg(0xC5); //VCM control
	ili9341_write_data8(0x33);
	ili9341_write_data8(0x3F);

	ili9341_write_reg(0xC7); //VCM control
	ili9341_write_data8(0x92);

	ili9341_write_reg(0x3A); // Memory Access Control
	ili9341_write_data8(0x55);

	ili9341_write_reg(0x36); // Memory Access Control
	if(USE_HORIZONTAL==0)ili9341_write_data8(0x08);
	else if(USE_HORIZONTAL==1)ili9341_write_data8(0xC8);
	else if(USE_HORIZONTAL==2)ili9341_write_data8(0x78);
	else ili9341_write_data8(0xA8);

	ili9341_write_reg(0xB1);
	ili9341_write_data8(0x00);
	ili9341_write_data8(0x12);
	
	ili9341_write_reg(0xB6); // Display Function Control
	ili9341_write_data8(0x0A);
	ili9341_write_data8(0xA2);

	ili9341_write_reg(0x44);
	ili9341_write_data8(0x02);

	ili9341_write_reg(0xF2); // 3Gamma Function Disable
	ili9341_write_data8(0x00);
	ili9341_write_reg(0x26); //Gamma curve selected
	ili9341_write_data8(0x01);
	ili9341_write_reg(0xE0); //Set Gamma
	ili9341_write_data8(0x0F);
	ili9341_write_data8(0x22);
	ili9341_write_data8(0x1C);
	ili9341_write_data8(0x1B);
	ili9341_write_data8(0x08);
	ili9341_write_data8(0x0F);
	ili9341_write_data8(0x48);
	ili9341_write_data8(0xB8);
	ili9341_write_data8(0x34);
	ili9341_write_data8(0x05);
	ili9341_write_data8(0x0C);
	ili9341_write_data8(0x09);
	ili9341_write_data8(0x0F);
	ili9341_write_data8(0x07);
	ili9341_write_data8(0x00);
	ili9341_write_reg(0XE1); //Set Gamma
	ili9341_write_data8(0x00);
	ili9341_write_data8(0x23);
	ili9341_write_data8(0x24);
	ili9341_write_data8(0x07);
	ili9341_write_data8(0x10);
	ili9341_write_data8(0x07);
	ili9341_write_data8(0x38);
	ili9341_write_data8(0x47);
	ili9341_write_data8(0x4B);
	ili9341_write_data8(0x0A);
	ili9341_write_data8(0x13);
	ili9341_write_data8(0x06);
	ili9341_write_data8(0x30);
	ili9341_write_data8(0x38);
	ili9341_write_data8(0x0F);
	ili9341_write_reg(0x29); //Display on


	
	ili9341_fill(0,0,LCD_W,LCD_H,BRED);
	

}

/*
  * @description     : spi驱动的probe函数，当驱动与
  *                    设备匹配以后此函数就会执行
  * @param - spi  	: spi设备
  * 
  */	
static int ili9341_probe(struct spi_device *spi)
{
	int ret;
//	struct ili9341_dev *ili9341dev;

	printk("ili9341_probe1:max = speed = %d\n",spi->max_speed_hz);
	
	/* 分配ili9341dev对象的空间 */
	ili9341dev = devm_kzalloc(&spi->dev, sizeof(*ili9341dev), GFP_KERNEL);
	if(!ili9341dev)
		return -ENOMEM;
		
	/* 注册字符设备驱动 */
	/* 1、创建设备号 */
	ret = alloc_chrdev_region(&ili9341dev->devid, 0, ILI9341_CNT, ILI9341_NAME);
	if(ret < 0) {
		pr_err("%s Couldn't alloc_chrdev_region, ret=%d\r\n", ILI9341_NAME, ret);
        return 0;
	}

	/* 2、初始化cdev */
	ili9341dev->cdev.owner = THIS_MODULE;
	cdev_init(&ili9341dev->cdev, &ili9341_ops);
	
	/* 3、添加一个cdev */
	ret = cdev_add(&ili9341dev->cdev, ili9341dev->devid, ILI9341_CNT);
	if(ret < 0) {
		goto del_unregister;
	}
	
	/* 4、创建类 */
	ili9341dev->class = class_create(THIS_MODULE, ILI9341_NAME);
	if (IS_ERR(ili9341dev->class)) {
		goto del_cdev;
	}

	/* 5、创建设备 */
	ili9341dev->device = device_create(ili9341dev->class, NULL, ili9341dev->devid, NULL, ILI9341_NAME);
	if (IS_ERR(ili9341dev->device)) {
		goto destroy_class;
	}
	ili9341dev->spi = spi;
	
	/*初始化spi_device */
	spi->mode = SPI_MODE_0;	/*MODE0，CPOL=0，CPHA=0*/
	spi_setup(spi);
	
	/* 初始化ILI9341 */
	ili9341_lcd_init(ili9341dev);	
	/* 保存ili9341dev结构体 */
	spi_set_drvdata(spi, ili9341dev);

	printk("ili9341_probe2:max = speed = %d\n",spi->max_speed_hz);

	return 0;
destroy_class:
	device_destroy(ili9341dev->class, ili9341dev->devid);
del_cdev:
	cdev_del(&ili9341dev->cdev);
del_unregister:
	unregister_chrdev_region(ili9341dev->devid, ILI9341_CNT);
	return -EIO;
}

/*
 * @description     : spi驱动的remove函数，移除spi驱动的时候此函数会执行
 * @param - spi 	: spi设备
 * @return          : 0，成功;其他负值,失败
 */
static int ili9341_remove(struct spi_device *spi)
{
	struct ili9341_dev *ili9341dev = spi_get_drvdata(spi);
	/* 注销字符设备驱动 */
	/* 1、删除cdev */
	cdev_del(&ili9341dev->cdev);
	/* 2、注销设备号 */
	unregister_chrdev_region(ili9341dev->devid, ILI9341_CNT); 
	/* 3、注销设备 */
	device_destroy(ili9341dev->class, ili9341dev->devid);
	/* 4、注销类 */
	class_destroy(ili9341dev->class); 
	return 0;
}

/* 传统匹配方式ID列表 */
static const struct spi_device_id ili9341_id[] = {
	{"alientek,ili9341", 0},
	{}
};

/* 设备树匹配列表 */
static const struct of_device_id ili9341_of_match[] = {
	{ .compatible = "alientek,ili9341" },
	{ /* Sentinel */ }
};

/* SPI驱动结构体 */
static struct spi_driver ili9341_driver = {
	.probe = ili9341_probe,
	.remove = ili9341_remove,
	.driver = {
			.owner = THIS_MODULE,
		   	.name = "ili9341",
		   	.of_match_table = ili9341_of_match,
		   },
	.id_table = ili9341_id,
};

/*
 * @description	: 驱动入口函数
 * @param 		: 无
 * @return 		: 无
 */
static int __init ili9341_init(void)
{
	int i;
	printk("ili9341_init\n");
	return spi_register_driver(&ili9341_driver);
}

/*
 * @description	: 驱动出口函数
 * @param 		: 无
 * @return 		: 无
 */
static void __exit ili9341_exit(void)
{
	printk("ili9341_exit\n");
	spi_unregister_driver(&ili9341_driver);
}

module_init(ili9341_init);
module_exit(ili9341_exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("ALIENTEK");
MODULE_INFO(intree, "Y");
