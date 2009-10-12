/*
 * Simple - REALLY simple memory mapping demonstration.
 *
 * Copyright (C) 2001 Alessandro Rubini and Jonathan Corbet
 * Copyright (C) 2001 O'Reilly & Associates
 *
 * The source code in this file can be freely used, adapted,
 * and redistributed in source or binary form, so long as an
 * acknowledgment appears in derived source files.  The citation
 * should list that the code comes from the book "Linux Device
 * Drivers" by Alessandro Rubini and Jonathan Corbet, published
 * by O'Reilly & Associates.   No warranty is attached;
 * we cannot take responsibility for errors or fitness for use.
 *
 * $Id: simple.c 3032 2006-07-13 05:43:19Z rgetz $
 */

/*
 * This code was found at:
 * http://cs.wellesley.edu/~systems/Lectures/Device-Drivers/ldd3-examples/simple/
 * and assumed to be orginal
 */

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>

#include <linux/kernel.h>	/* printk() */
#include <linux/slab.h>		/* kmalloc() */
#include <linux/fs.h>		/* everything... */
#include <linux/errno.h>	/* error codes */
#include <linux/types.h>	/* size_t */
#include <linux/mm.h>
#include <linux/kdev_t.h>
#include <asm/page.h>
#include <asm/sections.h>
#include <linux/cdev.h>

#include <linux/device.h>

static int simple_major = 0;
module_param(simple_major, int, 0);
MODULE_AUTHOR("Jonathan Corbet");
MODULE_LICENSE("Dual BSD/GPL");

/*
 * Open the device; in fact, there's nothing to do here.
 */
static int simple_open(struct inode *inode, struct file *filp)
{
	return 0;
}

/*
 * Closing is just as simpler.
 */
static int simple_release(struct inode *inode, struct file *filp)
{
	return 0;
}

/*
 * Common VMA ops.
 */

void simple_vma_open(struct vm_area_struct *vma)
{
	printk(KERN_NOTICE "Simple VMA open, virt %lx, phys %lx\n",
	       vma->vm_start, vma->vm_pgoff << PAGE_SHIFT);
}

void simple_vma_close(struct vm_area_struct *vma)
{
	printk(KERN_NOTICE "Simple VMA close.\n");
}

/*
 * The remap_pfn_range version of mmap.  This one is heavily borrowed
 * from drivers/char/mem.c.
 */

static struct vm_operations_struct simple_remap_vm_ops = {
	.open = simple_vma_open,
	.close = simple_vma_close,
};

/* In this sample we are using _ramend <-> physical_mem_end as
 * the buffer to be mmaped.
 */
static int simple_remap_mmap(struct file *filp, struct vm_area_struct *vma)
{
	/* the kernel passes in the vm_pgoff - the offset, in *pages*
	 * from the start of the buffer that the user space app wants
	 * to mmap. This is the last arg in the user space mmap call.
	 * We check it to make sure it is not out of bounds.
	 */
	if (vma->vm_pgoff > ((physical_mem_end - _ramend) >> PAGE_SHIFT)) {
		return -EINVAL;
	}

	vma->vm_start = (unsigned long)_ramend;
	vma->vm_end = (unsigned long)physical_mem_end;

	/* before remap is called, pgoff must be set to the kernel page
	 * offset from zero. Do to this, we figure out the address,
	 * ( << PAGE_SHIFT ), add it to the start of the buffer, and
	 * turn that back to pages (>> PAGE_SHIFT)
	 */
	vma->vm_pgoff = ((vma->vm_pgoff << PAGE_SHIFT) +
			 (unsigned long)_ramend) >> PAGE_SHIFT;

	/*   VM_MAYSHARE limits for mprotect(), and must be set on nommu.
	 *   Other flags can be set, and are documented in
	 *   include/linux/mm.h
	 */
	vma->vm_flags |= VM_MAYSHARE;

	if (remap_pfn_range(vma, vma->vm_start, vma->vm_pgoff,
			    vma->vm_end - vma->vm_start, vma->vm_page_prot))
		return -EAGAIN;

	vma->vm_ops = &simple_remap_vm_ops;
	simple_vma_open(vma);
	return 0;
}

/*
 * The nopage version.
 */
/*
struct page *simple_vma_nopage(struct vm_area_struct *vma,
			       unsigned long address, int *type)
{
	struct page *pageptr;
	unsigned long offset = vma->vm_pgoff << PAGE_SHIFT;
	unsigned long physaddr = address - vma->vm_start + offset;
	unsigned long pageframe = physaddr >> PAGE_SHIFT;

// Eventually remove these printks
	printk(KERN_NOTICE "---- Nopage, off %lx phys %lx\n", offset, physaddr);
	printk(KERN_NOTICE "VA is %p\n", __va(physaddr));
	printk(KERN_NOTICE "Page at %p\n", virt_to_page(__va(physaddr)));
	if (!pfn_valid(pageframe))
		return NOPAGE_SIGBUS;
	pageptr = pfn_to_page(pageframe);
	printk(KERN_NOTICE "page->index = %ld mapping %p\n", pageptr->index,
	       pageptr->mapping);
	printk(KERN_NOTICE "Page frame %ld\n", pageframe);
	get_page(pageptr);
	if (type)
		*type = VM_FAULT_MINOR;
	return pageptr;
}

static struct vm_operations_struct simple_nopage_vm_ops = {
	.open = simple_vma_open,
	.close = simple_vma_close,
	.nopage = simple_vma_nopage,
};
*/

ssize_t simple_read(struct file * filp, char __user * buf, size_t count, loff_t * f_pos)
{
	return count;
}

/*
static int simple_nopage_mmap(struct file *filp, struct vm_area_struct *vma)
{
	unsigned long offset = vma->vm_pgoff << PAGE_SHIFT;

	if (offset >= __pa(high_memory) || (filp->f_flags & O_SYNC))
		vma->vm_flags |= VM_IO;
	vma->vm_flags |= VM_RESERVED;

	vma->vm_ops = &simple_nopage_vm_ops;
	simple_vma_open(vma);
	return 0;
}
*/

/*
 * Set up the cdev structure for a device.
 */
static void simple_setup_cdev(struct cdev *dev, int minor,
			      struct file_operations *fops)
{
	int err, devno = MKDEV(simple_major, minor);

	cdev_init(dev, fops);
	dev->owner = THIS_MODULE;
	dev->ops = fops;
	err = cdev_add(dev, devno, 1);
	/* Fail gracefully if need be */
	if (err)
		printk(KERN_NOTICE "Error %d adding simple%d", err, minor);
}

/*
 * Our various sub-devices.
 */
/* Device 0 uses remap_pfn_range */
static struct file_operations simple_remap_ops = {
	.owner = THIS_MODULE,
	.open = simple_open,
	.read = simple_read,
	.release = simple_release,
	.mmap = simple_remap_mmap,
};

/* Device 1 uses nopage */
/*
static struct file_operations simple_nopage_ops = {
	.owner = THIS_MODULE,
	.open = simple_open,
	.release = simple_release,
	.mmap = simple_nopage_mmap,
};
*/

#define MAX_SIMPLE_DEV 1

#if 0
static struct file_operations *simple_fops[MAX_SIMPLE_DEV] = {
	&simple_remap_ops,
	&simple_nopage_ops,
};
#endif

/*
 * We export two simple devices.  There's no need for us to maintain any
 * special housekeeping info, so we just deal with raw cdevs.
 */
static struct cdev SimpleDevs[MAX_SIMPLE_DEV];

/*
 * Module housekeeping.
 */
static int simple_init(void)
{
	int result;
	dev_t dev = MKDEV(simple_major, 0);

	/* Figure out our device number. */
	if (simple_major)
		result = register_chrdev_region(dev, 1, "simple");
	else {
		result = alloc_chrdev_region(&dev, 0, 1, "simple");
		simple_major = MAJOR(dev);
	}
	if (result < 0) {
		printk(KERN_WARNING "simple: unable to get major %d\n",
		       simple_major);
		return result;
	}
	if (simple_major == 0)
		simple_major = result;

	/* Now set up two cdevs. */
	simple_setup_cdev(SimpleDevs, 0, &simple_remap_ops);
        //simple_setup_cdev(SimpleDevs + 1, 1, &simple_nopage_ops);
	return 0;
}

static void simple_cleanup(void)
{
	cdev_del(SimpleDevs);
	//cdev_del(SimpleDevs + 1);
	unregister_chrdev_region(MKDEV(simple_major, 0), 1);
}

module_init(simple_init);
module_exit(simple_cleanup);
