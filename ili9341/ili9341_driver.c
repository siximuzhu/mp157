/***************************************************************
Copyright © ALIENTEK Co., Ltd. 1998-2029. All rights reserved.
文件名		: ili9341.c
作者	  	: 正点原子Linux团队
版本	   	: V1.0
描述	   	: ili9341 SPI驱动程序
其他	   	: 无
论坛 	   	: www.openedv.com
日志	   	: 初版V1.0 2021/03/22 正点原子Linux团队创建
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

}

static void ili9341_res_set(void)
{

}

static void ili9341_blk_clr(void)
{

}

static void ili9341_blk_set(void)
{

}

static void ili9341_dc_clr(void)
{

}

static void ili9341_dc_set(void)
{

}

static int ili9341_write_spi(unsigned char data)
{
	int ret = -1;
	unsigned char txdata = data;
	struct spi_message m;
	struct spi_transfer *t;
	struct spi_device *spi = (struct spi_device *)ili9341dev->spi;
	
	t = kzalloc(sizeof(struct spi_transfer), GFP_KERNEL);	/* 申请内存 */
	if(!t) {
		return -ENOMEM;
	}
	
	t->tx_buf = &txdata;			/* 要发送的数据 */
	t->len = 1;				/* t->len=发送的长度+读取的长度 */
	spi_message_init(&m);		/* 初始化spi_message */
	spi_message_add_tail(t, &m);/* 将spi_transfer添加到spi_message队列 */
	ret = spi_sync(spi, &m);	/* 同步发送 */
	
	kfree(t);					/* 释放内存 */
	return ret;
}

static void ili9341_write_reg(unsigned char data)
{
	ili9341_dc_clr();  
	ili9341_write_spi(data);
	ili9341_dc_set();
}

static void ili9341_write_data8(unsigned char data)
{
	ili9341_write_spi(data);
}

static void ili9341_write_data16(unsigned short data)
{
	ili9341_write_spi(data >> 8);
	ili9341_write_spi(data);
}




/*
 * @description	: 从ili9341读取多个寄存器数据
 * @param - dev:  ili9341设备
 * @param - reg:  要读取的寄存器首地址
 * @param - val:  读取到的数据
 * @param - len:  要读取的数据长度
 * @return 		: 操作结果
 */
static int ili9341_read_regs(struct ili9341_dev *dev, u8 reg, void *buf, int len)
{

	int ret = -1;
	unsigned char txdata[1];
	unsigned char * rxdata;
	struct spi_message m;
	struct spi_transfer *t;
	struct spi_device *spi = (struct spi_device *)dev->spi;
    
	t = kzalloc(sizeof(struct spi_transfer), GFP_KERNEL);	/* 申请内存 */
	if(!t) {
		return -ENOMEM;
	}
	rxdata = kzalloc(sizeof(char) * len, GFP_KERNEL);	/* 申请内存 */
	if(!rxdata) {
		goto out1;
	}
	/* 一共发送len+1个字节的数据，第一个字节为
	寄存器首地址，一共要读取len个字节长度的数据，*/
	txdata[0] = reg | 0x80;		/* 写数据的时候首寄存器地址bit8要置1 */			
	t->tx_buf = txdata;			/* 要发送的数据 */
    t->rx_buf = rxdata;			/* 要读取的数据 */
	t->len = len+1;				/* t->len=发送的长度+读取的长度 */
	spi_message_init(&m);		/* 初始化spi_message */
	spi_message_add_tail(t, &m);/* 将spi_transfer添加到spi_message队列 */
	ret = spi_sync(spi, &m);	/* 同步发送 */
	if(ret) {
		goto out2;
	}
	
    memcpy(buf , rxdata+1, len);  /* 只需要读取的数据 */

out2:
	kfree(rxdata);					/* 释放内存 */
out1:	
	kfree(t);						/* 释放内存 */
	
	return ret;
}

/*
 * @description	: 向ili9341多个寄存器写入数据
 * @param - dev:  ili9341设备
 * @param - reg:  要写入的寄存器首地址
 * @param - val:  要写入的数据缓冲区
 * @param - len:  要写入的数据长度
 * @return 	  :   操作结果
 */
static s32 ili9341_write_regs(struct ili9341_dev *dev, u8 reg, u8 *buf, u8 len)
{
	int ret = -1;
	unsigned char *txdata;
	struct spi_message m;
	struct spi_transfer *t;
	struct spi_device *spi = (struct spi_device *)dev->spi;
	
	t = kzalloc(sizeof(struct spi_transfer), GFP_KERNEL);	/* 申请内存 */
	if(!t) {
		return -ENOMEM;
	}
	
	txdata = kzalloc(sizeof(char)+len, GFP_KERNEL);
	if(!txdata) {
		goto out1;
	}
	
	/* 一共发送len+1个字节的数据，第一个字节为
	寄存器首地址，len为要写入的寄存器的集合，*/
	*txdata = reg & ~0x80;	/* 写数据的时候首寄存器地址bit8要清零 */
    memcpy(txdata+1, buf, len);	/* 把len个寄存器拷贝到txdata里，等待发送 */
	t->tx_buf = txdata;			/* 要发送的数据 */
	t->len = len+1;				/* t->len=发送的长度+读取的长度 */
	spi_message_init(&m);		/* 初始化spi_message */
	spi_message_add_tail(t, &m);/* 将spi_transfer添加到spi_message队列 */
	ret = spi_sync(spi, &m);	/* 同步发送 */
    if(ret) {
        goto out2;
    }
	
out2:
	kfree(txdata);				/* 释放内存 */
out1:
	kfree(t);					/* 释放内存 */
	return ret;
}

/*
 * @description	: 读取ili9341指定寄存器值，读取一个寄存器
 * @param - dev:  ili9341设备
 * @param - reg:  要读取的寄存器
 * @return 	  :   读取到的寄存器值
 */
static unsigned char ili9341_read_onereg(struct ili9341_dev *dev, u8 reg)
{
	u8 data = 0;
	ili9341_read_regs(dev, reg, &data, 1);
	return data;
}

/*
 * @description	: 向ili9341指定寄存器写入指定的值，写一个寄存器
 * @param - dev:  ili9341设备
 * @param - reg:  要写的寄存器
 * @param - data: 要写入的值
 * @return   :    无
 */	

static void ili9341_write_onereg(struct ili9341_dev *dev, u8 reg, u8 value)
{
	u8 buf = value;
	ili9341_write_regs(dev, reg, &buf, 1);
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

/*
 * ILI9341内部寄存器初始化函数 
 * @param - spi : 要操作的设备
 * @return 	: 无
 */
void ili9341_lcd_init(struct ili9341_dev *dev)
{
	/*
	ili9341_res_clr();
	mdelay(100);
	ili9341_res_set();
	mdelay(100);
	ili9341_blk_set();
	mdelay(100);

	ili9341_write_reg(0x11)   //sleep out
	mdelay(120);
	*/
	printk("start send spi 0x12\n");
	ili9341_write_spi(0x12);
	ili9341_write_spi(0x34);
	ili9341_write_spi(0x56);
	ili9341_write_spi(0x78);
	printk("spi send over\n");

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

	printk("ili9341_probe\n");
	
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
