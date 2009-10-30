/*
 * lirc_bfin_timer.c - LIRC driver using Blackfin timers
 *
 * Enter bugs at http://blackfin.uclinux.org/
 *
 * Copyright 2007 Analog Devices Inc.
 * Licensed under the GPL-2 or later.
 */

#include <linux/gpio.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/uaccess.h>

#include <linux/version.h>
#include "drivers/lirc.h"
#include "drivers/lirc_dev/lirc_dev.h"

#include <asm/gptimers.h>
#include <asm/portmux.h>

static int sense = -1;	/* -1 = auto, 0 = active high, 1 = active low */
module_param(sense, bool, 0444);
MODULE_PARM_DESC(sense, "Override autodetection of IR receiver circuit (0 = active high, 1 = active low)");

/* BF537-EZKIT Timers Header:
 *   PIN      FUNCTION
 *    1        PF2 (Timer 7)
 *    2        5V
 *    3        PF3 (Timer 6)
 *    4        3.3V
 *    5        PF4 (Timer 5)
 *    6        PF9 (Timer 0)
 *    7        nothing
 *    8        PF8 (Timer 1)
 *    9        Ground
 *   10        PF7 (Timer 2)
 */

#define DRIVER_NAME "lirc_bfin_gptimers"

#define pr_stamp() pr_debug(DRIVER_NAME ":%i:%s: here i am\n", __LINE__, __func__)

struct bfin_gptimer {
	/* hardware pieces */
	const char *name;
	int irq, id, bit, mux, gpio;

	/* lirc pieces */
	uint32_t last_width;
	bool period_overflow, opened;
	struct lirc_driver driver;
	struct lirc_buffer lirc_buf;
};

static irqreturn_t gptimer_irq(int irq, void *dev_id)
{
	struct bfin_gptimer *g = dev_id;
	uint32_t width, period;
	lirc_t codes[2];

	pr_stamp();

	/* see if it was our timer */
	if (!get_gptimer_intr(g->id))
		return IRQ_NONE;

	/* check for overflow and clear it */
	if (get_gptimer_over(g->id)) {
		clear_gptimer_over(g->id);
		g->period_overflow = true;
		goto finish;
	}

 	/* record this sample ! */
	width = get_gptimer_pwidth(g->id);
	period = get_gptimer_period(g->id);
	pr_debug(DRIVER_NAME ":irq: width = 0x%08x period = 0x%08x %s\n",
		width, period, (g->last_width > period ? "!!!" : ""));

	/* queue up the samples.  each irq will give us the current
	 * width value and the period value for the last irq.  so
	 * maintain state across irq's to properly compute spaces.
	 */
	if (g->period_overflow) {
		codes[0] = PULSE_MASK;
		g->period_overflow = false;
	} else
		codes[0] = sclk_to_usecs(period - g->last_width) & PULSE_MASK;
	codes[1] = PULSE_BIT | (sclk_to_usecs(width) & PULSE_MASK);
	lirc_buffer_write_n(&g->lirc_buf, (void *)codes, ARRAY_SIZE(codes));
	g->last_width = width;
	wake_up(&g->lirc_buf.wait_poll);

 finish:
	clear_gptimer_intr(g->id);

	return IRQ_HANDLED;
}

static int gptimer_ioctl(struct inode *node, struct file *filep,
                         unsigned int cmd, unsigned long arg)
{
	switch (cmd) {
		case LIRC_GET_REC_RESOLUTION:
			return put_user(sclk_to_usecs(1), (unsigned long *)arg);
		default:
			return -ENOIOCTLCMD;
	}
}

/* Called when userspace opens up our device.  We do not have
 * to do any locking at the function level here ourselves as
 * the lirc_dev layer takes care of it for us.
 */
static int gptimer_set_use_inc(void *data)
{
	struct bfin_gptimer *g = data;
	int ret;

	pr_stamp();

	if (g->opened == true)
		return -EBUSY;

	/* figure out the idle level because we need to trigger
	 * away from it or the first sample gets ignored.
	 */
	if (sense == -1) {
		size_t i, nlow, nhigh;

		ret = gpio_request(g->gpio, DRIVER_NAME);
		if (ret) {
			printk(KERN_NOTICE DRIVER_NAME ": gpio_request() failed\n");
			return ret;
		}

		/* probe 9 times every 0.04s, collect "votes" for
		 * active high/low state.
		 */
		gpio_direction_input(g->gpio);
		set_current_state(TASK_INTERRUPTIBLE);

		nlow = nhigh = 0;
		for (i = 0; i < 9; ++i) {
			if (gpio_get_value(g->gpio))
				++nhigh;
			else
				++nlow;
			schedule_timeout(HZ / 25);
		}
		sense = (nlow >= nhigh ? 1 : 0);
		printk(KERN_INFO DRIVER_NAME ": auto-detected active "
			"%s receiver\n", (sense ? "low" : "high"));

		gpio_free(g->gpio);
	} else
		printk(KERN_INFO DRIVER_NAME ": manually using active "
			"%s receiver\n", (sense ? "low" : "high"));

	/* request the timer peripheral */
	ret = peripheral_request(g->mux, g->name);
	if (ret) {
		printk(KERN_NOTICE DRIVER_NAME ": peripheral_request() failed\n");
		return ret;
	}

	/* grab the irq for this timer */
	ret = request_irq(g->irq, gptimer_irq, IRQF_SHARED, g->name, g);
	if (ret) {
		printk(KERN_NOTICE DRIVER_NAME ": request_irq() failed\n");
		peripheral_free(g->mux);
		return ret;
	}

	/* init our timer-specific data */
	g->period_overflow = true;

	/* configure the timer and enable it */
	set_gptimer_config(g->id, WDTH_CAP | (sense ? PULSE_HI : 0) | IRQ_ENA);
	enable_gptimers(g->bit);

	pr_debug(DRIVER_NAME ": device opened; sclk is 0x%08lx\n", get_sclk());

	g->opened = true;

	return 0;
}

/* Called when userspace closes our device */
static void gptimer_set_use_dec(void *data)
{
	struct bfin_gptimer *g = data;

	pr_stamp();

	disable_gptimers(g->bit);

	free_irq(g->irq, g);

	peripheral_free(g->mux);

	g->opened = false;
}

/* XXX: move this to platform data in a boards file */
static struct bfin_gptimer timer5 = {
	.name   = DRIVER_NAME "5",
	.irq    = IRQ_TIMER5,
	.id     = TIMER5_id,
	.bit    = TIMER5bit,
	.mux    = P_TMR5,
	.gpio   = GPIO_PF4,
};
static struct file_operations timer_fops = {
	.ioctl        = gptimer_ioctl,
};
static struct lirc_driver __initdata driver_template = {
	.name         = DRIVER_NAME,
	.minor        = -1,
	.code_length  = sizeof(lirc_t) * 8,
	.sample_rate  = 0,
	.features     = LIRC_CAN_REC_MODE2,
	.data         = NULL,
	.add_to_buf   = NULL,
	.get_queue    = NULL,
	.rbuf         = NULL,
	.set_use_inc  = gptimer_set_use_inc,
	.set_use_dec  = gptimer_set_use_dec,
	.fops         = &timer_fops,
	.dev          = NULL,
	.owner        = THIS_MODULE,
};

static int __init lirc_bfin_timer_init(void)
{
	struct bfin_gptimer *g = &timer5;
	int ret;

	pr_stamp();

	/* init the driver data */
	g->opened = false;
	g->driver = driver_template;
	g->driver.data = g;
	g->driver.rbuf = &g->lirc_buf;
	ret = lirc_buffer_init(&g->lirc_buf, sizeof(lirc_t), 64);
	if (ret) {
		printk(KERN_NOTICE DRIVER_NAME ": lirc_buffer_init() failed\n");
		return ret;
	}

	/* register the lirq driver */
	ret = lirc_register_driver(&g->driver);
	if (ret < 0) {
		printk(KERN_NOTICE DRIVER_NAME ": lirc_register_driver() failed\n");
		lirc_buffer_free(&g->lirc_buf);
		return ret;
	}

	return 0;
}
module_init(lirc_bfin_timer_init);

static void __exit lirc_bfin_timer_exit(void)
{
	struct bfin_gptimer *g = &timer5;

	pr_stamp();

	lirc_buffer_free(&g->lirc_buf);
	lirc_unregister_driver(g->driver.minor);
}
module_exit(lirc_bfin_timer_exit);

MODULE_AUTHOR("Mike Frysinger <vapier@gentoo.org>");
MODULE_DESCRIPTION("LIRC driver using Blackfin general purpose timers");
MODULE_LICENSE("GPL");
