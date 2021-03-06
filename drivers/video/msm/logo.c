/* drivers/video/msm/logo.c
 *
 * Show Logo in RLE 565 format
 *
 * Copyright (C) 2008 Google Incorporated
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#include <linux/module.h>
#include <linux/types.h>
#include <linux/fb.h>
#include <linux/vt_kern.h>
#include <linux/unistd.h>
#include <linux/syscalls.h>

#include <linux/irq.h>
#include <asm/system.h>
#include <asm/cacheflush.h>

#define fb_width(fb)	((fb)->var.xres)
#define fb_height(fb)	((fb)->var.yres)
#define fb_size(fb)	((fb)->var.xres * (fb)->var.yres * 2)

/* convert RGB565 to RBG8888 */
static void memset16_rgb8888(void *_ptr, unsigned short val, unsigned count)
{
	unsigned short *ptr = _ptr;
	unsigned short red; 
	unsigned short green;
	unsigned short blue;
	
	red = ( val & 0xF800) >> 8;
	green = (val & 0x7E0) >> 3;
	blue = (val & 0x1F) << 3;

	count >>= 1;
	while (count--)
	{
		*ptr++ = (red<<8) | green;
		*ptr++ = blue;
	}
}

int load_565rle_image(char *filename, bool bf_supported)
{
	int fd, err = 0;
	unsigned count, max;
	unsigned short *data, *bits, *ptr;
	struct fb_info *info;
#ifndef CONFIG_FRAMEBUFFER_CONSOLE 
	struct module *owner; 
#endif 	
	info = registered_fb[0];
	
	if (!info) {
		printk(KERN_WARNING "%s: Can not access framebuffer\n",
			__func__);
		return -ENODEV;
	}
#ifndef CONFIG_FRAMEBUFFER_CONSOLE 
	owner = info->fbops->owner; 
	if (!try_module_get(owner)) 
		return 0; 
	if (info->fbops->fb_open && info->fbops->fb_open(info, 0)) { 
		module_put(owner); 
		return 0; 
	}
#endif
	fd = sys_open(filename, O_RDONLY, 0);
	if (fd < 0) {
		printk(KERN_WARNING "%s: Can not open %s\n",
			__func__, filename);
		return -ENOENT;
	}

	count = (unsigned)sys_lseek(fd, (off_t)0, 2);
	if (count == 0) {
		sys_close(fd);
		err = -EIO;
		goto err_logo_close_file;
	}

	sys_lseek(fd, (off_t)0, 0);
	data = kmalloc(count, GFP_KERNEL);
	if (!data) {
		printk(KERN_WARNING "%s: Can not alloc data\n", __func__);
		err = -ENOMEM;
		goto err_logo_close_file;
	}
	if ((unsigned)sys_read(fd, (char *)data, count) != count) {
		err = -EIO;
		goto err_logo_free_data;
	}

	max = fb_width(info) * fb_height(info);

	ptr = data;
	bits = (unsigned short *)(info->screen_base);

	while (count > 3) {
		unsigned n = ptr[0];
		if (n > max)
			break;

		memset16_rgb8888(bits, ptr[1], n << 1);
		bits += n*2; // for rgb8888
		max -= n;
		ptr += 2;
		count -= 4;
	}
err_logo_free_data:
	kfree(data);
err_logo_close_file:
	sys_close(fd);
	return err;
}
EXPORT_SYMBOL(load_565rle_image);
