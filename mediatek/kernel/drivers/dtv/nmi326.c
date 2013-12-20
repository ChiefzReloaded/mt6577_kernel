/*****************************************************************************
 Copyright(c) 2012 NMI Inc. All Rights Reserved

 File name : nmi326.c

 Description : NM326 host interface

 History :
 ----------------------------------------------------------------------
 2012/11/27 	ssw		initial
*******************************************************************************/

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/errno.h>
#include <linux/types.h>
#include <linux/fcntl.h>
//#include <linux/miscdevice.h>
#include <linux/interrupt.h>
#include <asm/uaccess.h>
#include <linux/irq.h>
//#include <asm/irq.h>
#include <asm/mach/irq.h>

#include <linux/wait.h>
#include <linux/stat.h>
#include <linux/ioctl.h>
#include <linux/delay.h>
#include <linux/slab.h>
//#include <plat/gpio.h>
//#include <plat/mux.h>

#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/vmalloc.h>

#include <linux/io.h>
#include <mach/board.h>
//#include <mach/gpio.h>
#include <linux/gpio.h>
#include "mach/mt6575_gpio.h"

#include "nmi326.h"
#include "nmi326_spi_drv.h"


static struct class *isdbt_class;

static wait_queue_head_t isdbt_irq_waitqueue;
static char g_bCatchIrq = 0;
static int g_irq_status = DTV_IRQ_DEINIT;

#define SPI_RW_BUF		(188*50*2)

//----------------------------------------------------------------------
// [[ GPIO


void isdbt_gpio_init(void)
{
	//PWR Enable
	mt_set_gpio_mode(NMI_POWER_PIN, GPIO_MODE_00);
	mt_set_gpio_dir(NMI_POWER_PIN, GPIO_DIR_OUT);
	mt_set_gpio_pull_enable(NMI_POWER_PIN, true);
	mt_set_gpio_out(NMI_POWER_PIN, 0);

	//n_Reset
	mt_set_gpio_mode(NMI_RESET_PIN, GPIO_MODE_00);
	mt_set_gpio_dir(NMI_RESET_PIN, GPIO_DIR_OUT);
	mt_set_gpio_pull_enable(NMI_RESET_PIN, true);
	mt_set_gpio_out(NMI_RESET_PIN, 1);

	// interrupt
	mt_set_gpio_mode(NMI_IRQN_PIN, GPIO_MODE_00);
	mt_set_gpio_dir(NMI_IRQN_PIN, GPIO_DIR_IN);

}

void isdbt_gpio_power_on(void)
{
	mt_set_gpio_out(NMI_POWER_PIN, 1);
}

void isdbt_gpio_power_off(void)
{
	mt_set_gpio_out(NMI_POWER_PIN, 0);
}

void isdbt_gpio_reset_up(void)
{
	mt_set_gpio_out(NMI_RESET_PIN, 1);
}

void isdbt_gpio_reset_down(void)
{
	mt_set_gpio_out(NMI_RESET_PIN, 0);
}



extern void mt65xx_eint_mask(unsigned int eint_num);
extern void mt65xx_eint_unmask(unsigned int eint_num);
extern unsigned int mt65xx_eint_set_sens(unsigned int eint_num, unsigned int sens);
extern void mt65xx_eint_set_hw_debounce ( unsigned int eint_num, unsigned int ms );
extern void mt65xx_eint_registration(unsigned int eint_num, unsigned int is_deb_en, unsigned int pol, void (EINT_FUNC_PTR)(void), unsigned int is_auto_umask);

void isdbt_irq_handler(void)
{
	mt65xx_eint_mask(CUST_EINT_DTV_NUM);
	g_irq_status = DTV_IRQ_INIT;
	g_bCatchIrq = 1;
	wake_up(&isdbt_irq_waitqueue);
}

void isdbt_gpio_interrupt_register(void)
{
	// configuration for detect
	NM_KMSG("<isdbt> isdbt_gpio_interrupt_register, IN [+]\n");
	mt65xx_eint_set_sens(CUST_EINT_DTV_NUM, CUST_EINT_DTV_SENSITIVE);
	mt65xx_eint_set_hw_debounce(CUST_EINT_DTV_NUM, CUST_EINT_DTV_DEBOUTCE_CN);

	// gpio
	mt_set_gpio_mode(GPIO16, GPIO_MODE_02);
	mt_set_gpio_dir(GPIO16, GPIO_DIR_IN);
	mt_set_gpio_pull_enable(GPIO16, false);

	// irq register
	mt65xx_eint_registration(CUST_EINT_DTV_NUM, CUST_EINT_DTV_DEBOUNCE_EN, CUST_EINT_DTV_POLARITY, isdbt_irq_handler, 0);

	// disable irq
	mt65xx_eint_mask(CUST_EINT_DTV_NUM);
	NM_KMSG("<isdbt> isdbt_gpio_interrupt_register, OUT [-]\n");
}

void isdbt_gpio_interrupt_unregister(void)
{
	NM_KMSG("<isdbt> isdbt_gpio_interrupt_unregister, IN [+]\n");
	// gpio
	mt_set_gpio_mode(GPIO16, GPIO_MODE_00);
	mt_set_gpio_dir(GPIO16, GPIO_DIR_IN);

	// mask
	//mt65xx_eint_mask(CUST_EINT_DTV_NUM);

	NM_KMSG("<isdbt> isdbt_gpio_interrupt_unregister, OUT [-]\n");
}

void isdbt_gpio_interrupt_enable(void)
{
	mt65xx_eint_unmask(CUST_EINT_DTV_NUM);
}

void isdbt_gpio_interrupt_disable(void)
{
	mt65xx_eint_mask(CUST_EINT_DTV_NUM);
}

// ]]] GPIO
//----------------------------------------------------------------------

static unsigned int	isdbt_poll(struct file *filp, poll_table *wait)
{
	unsigned int mask = 0;

	poll_wait(filp, &isdbt_irq_waitqueue, wait);

	if( g_irq_status == DTV_IRQ_DEINIT)
	{
		NM_KMSG("<isdbt> DTV_IRQ_DEINIT - do nothing\n");
		return mask;
	}

	if ( g_bCatchIrq == 1)
	{
		mask |= (POLLIN | POLLRDNORM);
		NM_KMSG("<isdbt> poll: release (%d)\n", g_bCatchIrq);
		g_bCatchIrq = 0;
	} else {
		NM_KMSG("<isdbt> poll: release (%d)\n", g_bCatchIrq);
	}

	return mask;
}

static int isdbt_open(struct inode *inode, struct file *filp)
{
	ISDBT_OPEN_INFO_T *pdev = NULL;

	NM_KMSG("<isdbt> isdbt open\n");

	pdev = (ISDBT_OPEN_INFO_T *)kmalloc(sizeof(ISDBT_OPEN_INFO_T), GFP_KERNEL);
	if(pdev == NULL)
	{
		NM_KMSG("<isdbt> pdev kmalloc FAIL\n");
		return -1;
	}

	NM_KMSG("<isdbt> pdev kmalloc SUCCESS\n");

	memset(pdev, 0x00, sizeof(ISDBT_OPEN_INFO_T));
	g_irq_status = DTV_IRQ_DEINIT;
	g_bCatchIrq = 0;

	filp->private_data = pdev;

	pdev->rwBuf = kmalloc(SPI_RW_BUF, GFP_KERNEL);

	if(pdev->rwBuf == NULL)
	{
		NM_KMSG("<isdbt> pdev->rwBuf kmalloc FAIL\n");
		return -1;
	}

	spin_lock_init(&pdev->isr_lock);
	init_waitqueue_head(&isdbt_irq_waitqueue);

	NM_KMSG("<isdbt> isdbt open, success\n");

	return 0;
}

static int isdbt_release(struct inode *inode, struct file *filp)
{
	ISDBT_OPEN_INFO_T *pdev = (ISDBT_OPEN_INFO_T*)(filp->private_data);

	NM_KMSG("<isdbt> isdbt release \n");
	g_irq_status = DTV_IRQ_DEINIT;
	g_bCatchIrq = 0;

	kfree(pdev->rwBuf);
	kfree((void*)pdev);

	return 0;
}

static int isdbt_read(struct file *filp, char __user *buf, size_t count, loff_t *f_pos)
{
	int rv1 = 0;
	int rv2 = 0;
	int blk_cnt = count / 1024;
	int remain = count % 1024;
	ISDBT_OPEN_INFO_T *pdev = (ISDBT_OPEN_INFO_T *)(filp->private_data);

	if(blk_cnt) {
		rv1 = nmi326_spi_read(pdev->rwBuf, blk_cnt*1024);
		if (rv1 < 0) {
			NM_KMSG("isdbt_read() : nmi326_spi_read failed(rv1:%d)\n", rv1);
			return rv1;
		}
	}

	if(remain) {
		rv2 = nmi326_spi_read(&pdev->rwBuf[rv1], remain);
		if(rv2 < 0) {
			NM_KMSG("isdbt_read() : nmi326_spi_read failed(rv2:%d)\n", rv2);
			return rv1 + rv2;
		}
	}

	if(copy_to_user(buf, pdev->rwBuf, count) < 0) {
		return -1;
	}

	return rv1 + rv2;
}

static int isdbt_write(struct file *filp, const char __user *buf, size_t count, loff_t *f_pos)
{
	int rv = 0;
	ISDBT_OPEN_INFO_T* pdev = (ISDBT_OPEN_INFO_T*)(filp->private_data);

	/* move data from user area to kernel  area */
	if(copy_from_user(pdev->rwBuf, buf, count) < 0) {
		return -1;
	}

	/* write data to SPI Controller */
	rv = nmi326_spi_write(pdev->rwBuf, count);
	if (rv < 0) {
		NM_KMSG("isdbt_write() : nmi326_spi_write failed(rv:%d)\n", rv);
		return rv;
	}

	return rv;
}

static int isdbt_ioctl(/*struct inode *inode, */struct file *filp, unsigned int cmd, unsigned long arg)
{
	long res = 1;
	ISDBT_OPEN_INFO_T* pdev = (ISDBT_OPEN_INFO_T*)(filp->private_data);

	int	err, size;

	if(_IOC_TYPE(cmd) != IOCTL_MAGIC) return -EINVAL;
	if(_IOC_NR(cmd) >= IOCTL_MAXNR) return -EINVAL;

	size = _IOC_SIZE(cmd);

	if(size) {
		if(_IOC_DIR(cmd) & _IOC_READ)
			err = access_ok(VERIFY_WRITE, (void *) arg, size);
		else if(_IOC_DIR(cmd) & _IOC_WRITE)
			err = access_ok(VERIFY_READ, (void *) arg, size);
		if(!err) {
			NM_KMSG("%s : Wrong argument! cmd(0x%08x) _IOC_NR(%d) _IOC_TYPE(0x%x) _IOC_SIZE(%d) _IOC_DIR(0x%x)\n",
			__FUNCTION__, cmd, _IOC_NR(cmd), _IOC_TYPE(cmd), _IOC_SIZE(cmd), _IOC_DIR(cmd));
			return err;
		}
	}

	switch(cmd)
	{
		case IOCTL_ISDBT_POWER_ON:
			NM_KMSG("ISDBT_POWER_ON enter..\n");
			isdbt_gpio_power_on();
			break;

		case IOCTL_ISDBT_POWER_OFF:
			NM_KMSG("ISDBT_POWER_OFF enter..\n");
			isdbt_gpio_power_off();
			break;

		case IOCTL_ISDBT_RST_DN:
			NM_KMSG("IOCTL_ISDBT_RST_DN enter..\n");
			isdbt_gpio_reset_down();
			break;

		case IOCTL_ISDBT_RST_UP:
			NM_KMSG("IOCTL_ISDBT_RST_UP enter..\n");
			isdbt_gpio_reset_up();
			break;

		case IOCTL_ISDBT_INTERRUPT_REGISTER:
		{
			unsigned long flags;
			spin_lock_irqsave(&pdev->isr_lock, flags);

			NM_KMSG("<isdbt> ioctl: interrupt register, (stat : %d)\n", g_irq_status);
			if( g_irq_status == DTV_IRQ_DEINIT)
			{
				g_bCatchIrq = 0;
				isdbt_gpio_interrupt_register();
				g_irq_status = DTV_IRQ_INIT;
			}
			spin_unlock_irqrestore(&pdev->isr_lock, flags);

			break;
		}

		case IOCTL_ISDBT_INTERRUPT_UNREGISTER:
		{
			unsigned long flags;
			spin_lock_irqsave(&pdev->isr_lock, flags);

			NM_KMSG("<isdbt> ioctl: interrupt unregister, (stat : %d)\n", g_irq_status);
			if( g_irq_status == DTV_IRQ_INIT)
			{
				g_bCatchIrq = 0;
				isdbt_gpio_interrupt_unregister();
				g_irq_status = DTV_IRQ_DEINIT;
			}
			spin_unlock_irqrestore(&pdev->isr_lock, flags);

			break;
		}

		case IOCTL_ISDBT_INTERRUPT_ENABLE:
		{
			unsigned long flags;
			spin_lock_irqsave(&pdev->isr_lock, flags);

			if(g_irq_status == DTV_IRQ_INIT)
			{
				isdbt_gpio_interrupt_enable();
				g_irq_status = DTV_IRQ_SET;
			}
			spin_unlock_irqrestore(&pdev->isr_lock, flags);
			break;
		}

		case IOCTL_ISDBT_INTERRUPT_DISABLE:
		{
			unsigned long flags;
			spin_lock_irqsave(&pdev->isr_lock, flags);

			if(g_irq_status == DTV_IRQ_SET)
			{
				g_bCatchIrq = 0;
				isdbt_gpio_interrupt_disable();
				//pdev->irq_status = DTV_IRQ_INIT;
				g_irq_status = DTV_IRQ_INIT;
			}
			spin_unlock_irqrestore(&pdev->isr_lock, flags);
			break;
		}

		case IOCTL_ISDBT_INTERRUPT_DONE:
		{
			unsigned long flags;

			spin_lock_irqsave(&pdev->isr_lock,flags);

			if(g_irq_status == DTV_IRQ_INIT)
			{
				isdbt_gpio_interrupt_enable();
				g_irq_status = DTV_IRQ_SET;
			}
			spin_unlock_irqrestore(&pdev->isr_lock,flags);
			break;
		}

		default:
			res = 1;
			break;

	}

	return res;
}


static const struct file_operations isdbt_fops = {
	.owner			= THIS_MODULE,
	.open			= isdbt_open,
	.release		= isdbt_release,
	.read			= isdbt_read,
	.write			= isdbt_write,
	.unlocked_ioctl	= isdbt_ioctl,
	.poll			= isdbt_poll,
};

static int isdbt_probe(struct platform_device *pdev)
{
	int ret;
	struct device *isdbt_dev;

	NM_KMSG("<isdbt> isdbt_probe, MAJOR = %d\n", ISDBT_DEV_MAJOR);

	// 1. register character device
	ret = register_chrdev(ISDBT_DEV_MAJOR, ISDBT_DEV_NAME, &isdbt_fops);
	if(ret < 0)
		NM_KMSG("<isdbt> register_chrdev(ISDBT_DEV) failed\n");

	// 2. class create
	isdbt_class = class_create(THIS_MODULE, ISDBT_DEV_NAME);
	if(IS_ERR(isdbt_class)) {
		unregister_chrdev(ISDBT_DEV_MAJOR, ISDBT_DEV_NAME);
		class_destroy(isdbt_class);
		NM_KMSG("<isdbt> class create failed\n");

		return -EFAULT;
	}

	// 3. device create
	isdbt_dev = device_create(isdbt_class, NULL, MKDEV(ISDBT_DEV_MAJOR, ISDBT_DEV_MINOR), NULL, ISDBT_DEV_NAME);
	if(IS_ERR(isdbt_dev)) {
		unregister_chrdev(ISDBT_DEV_MAJOR, ISDBT_DEV_NAME);
		class_destroy(isdbt_class);
		NM_KMSG("<isdbt> device create failed\n");

		return -EFAULT;
	}

	isdbt_gpio_init();
	nmi326_spi_init();

	NM_KMSG("<isdbt> nmi326_spi_init, is called\n");

	return 0;
}

static int isdbt_remove(struct platform_device *pdev)
{
	return 0;
}

static int isdbt_suspend(struct platform_device *pdev, pm_message_t mesg)
{
	return 0;
}

static int isdbt_resume(struct platform_device *pdev)
{
	return 0;
}

static struct platform_driver isdbt_driver = {
	.probe	= isdbt_probe,
	.remove	= isdbt_remove,
	.suspend	= isdbt_suspend,
	.resume	= isdbt_resume,
	.driver = {
		.owner	= THIS_MODULE,
		.name	= "isdbt"
	},
};

static int __init isdbt_init(void)
{
	int result;

	NM_KMSG("<isdbt> isdbt_init \n");

	result = platform_driver_register(&isdbt_driver);
	if(result)
		return result;

	NM_KMSG("<isdbt> isdbt_init, success \n");

	return 0;
}

static void __exit isdbt_exit(void)
{
	NM_KMSG("<isdbt> isdbt_exit \n");

	unregister_chrdev(ISDBT_DEV_MAJOR, "isdbt");
	device_destroy(isdbt_class, MKDEV(ISDBT_DEV_MAJOR, ISDBT_DEV_MINOR));
	class_destroy(isdbt_class);

	platform_driver_unregister(&isdbt_driver);

	nmi326_spi_exit();
}

module_init(isdbt_init);
module_exit(isdbt_exit);

MODULE_LICENSE("Dual BSD/GPL");

