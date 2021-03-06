/* Copyright (c) 2010, Code Aurora Forum. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <mach/board.h>

#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/device.h>
#include <linux/uaccess.h>

#include <media/msm_gemini.h>
#include "msm_gemini_sync.h"
#include "msm_gemini_common.h"

#define MSM_GEMINI_NAME "gemini"

static int msm_gemini_open(struct inode *inode, struct file *filp)
{
	int rc;

	struct msm_gemini_device *pgmn_dev = container_of(inode->i_cdev,
		struct msm_gemini_device, cdev);
	filp->private_data = pgmn_dev;

	GMN_DBG("%s:%d]\n", __func__, __LINE__);

	rc = __msm_gemini_open(pgmn_dev);

	GMN_DBG(KERN_INFO "%s:%d] %s open_count = %d\n", __func__, __LINE__,
		filp->f_path.dentry->d_name.name, pgmn_dev->open_count);

	return rc;
}

static int msm_gemini_release(struct inode *inode, struct file *filp)
{
	int rc;

	struct msm_gemini_device *pgmn_dev = filp->private_data;

	GMN_DBG(KERN_INFO "%s:%d]\n", __func__, __LINE__);

	rc = __msm_gemini_release(pgmn_dev);

	GMN_DBG(KERN_INFO "%s:%d] %s open_count = %d\n", __func__, __LINE__,
		filp->f_path.dentry->d_name.name, pgmn_dev->open_count);
	return rc;
}

static long msm_gemini_ioctl(struct file *filp, unsigned int cmd,
	unsigned long arg)
{
	int rc;
	struct msm_gemini_device *pgmn_dev = filp->private_data;

	GMN_DBG(KERN_INFO "%s:%d] cmd = %d\n", __func__, __LINE__,
		_IOC_NR(cmd));

	rc = __msm_gemini_ioctl(pgmn_dev, cmd, arg);

	GMN_DBG("%s:%d]\n", __func__, __LINE__);
	return rc;
}

static const struct file_operations msm_gemini_fops = {
	.owner	  = THIS_MODULE,
	.open	   = msm_gemini_open,
	.release	= msm_gemini_release,
	.unlocked_ioctl = msm_gemini_ioctl,
};

static struct class *msm_gemini_class;
static dev_t msm_gemini_devno;
static struct msm_gemini_device *msm_gemini_device_p;

static int msm_gemini_init(struct platform_device *pdev)
{
	int rc = -1;
	struct device *dev;

	GMN_DBG("%s:%d]\n", __func__, __LINE__);

	msm_gemini_device_p = __msm_gemini_init(pdev);
	if (msm_gemini_device_p == NULL) {
		GMN_PR_ERR("%s: initialization failed\n", __func__);
		goto fail;
	}

	rc = alloc_chrdev_region(&msm_gemini_devno, 0, 1, MSM_GEMINI_NAME);
	if (rc < 0) {
		GMN_PR_ERR("%s: failed to allocate chrdev\n", __func__);
		goto fail_1;
	}

	if (!msm_gemini_class) {
		msm_gemini_class = class_create(THIS_MODULE, MSM_GEMINI_NAME);
		if (IS_ERR(msm_gemini_class)) {
			rc = PTR_ERR(msm_gemini_class);
			GMN_PR_ERR("%s: create device class failed\n",
				__func__);
			goto fail_2;
		}
	}

	dev = device_create(msm_gemini_class, NULL,
		MKDEV(MAJOR(msm_gemini_devno), MINOR(msm_gemini_devno)), NULL,
		"%s%d", MSM_GEMINI_NAME, 0);

	if (IS_ERR(dev)) {
		GMN_PR_ERR("%s: error creating device\n", __func__);
		rc = -ENODEV;
		goto fail_3;
	}

	cdev_init(&msm_gemini_device_p->cdev, &msm_gemini_fops);
	msm_gemini_device_p->cdev.owner = THIS_MODULE;
	msm_gemini_device_p->cdev.ops   =
		(const struct file_operations *) &msm_gemini_fops;
	rc = cdev_add(&msm_gemini_device_p->cdev, msm_gemini_devno, 1);
	if (rc < 0) {
		GMN_PR_ERR("%s: error adding cdev\n", __func__);
		rc = -ENODEV;
		goto fail_4;
	}

	GMN_DBG(KERN_INFO "%s %s: success\n", __func__, MSM_GEMINI_NAME);

	return rc;

fail_4:
	device_destroy(msm_gemini_class, msm_gemini_devno);

fail_3:
	class_destroy(msm_gemini_class);

fail_2:
	unregister_chrdev_region(msm_gemini_devno, 1);

fail_1:
	__msm_gemini_exit(msm_gemini_device_p);

fail:
	return rc;
}

static void msm_gemini_exit(void)
{
	cdev_del(&msm_gemini_device_p->cdev);
	device_destroy(msm_gemini_class, msm_gemini_devno);
	class_destroy(msm_gemini_class);
	unregister_chrdev_region(msm_gemini_devno, 1);

	__msm_gemini_exit(msm_gemini_device_p);
}

static int __msm_gemini_probe(struct platform_device *pdev)
{
	int rc;
	rc = msm_gemini_init(pdev);
	return rc;
}

static int __msm_gemini_remove(struct platform_device *pdev)
{
	msm_gemini_exit();
	return 0;
}

static struct platform_driver msm_gemini_driver = {
	.probe  = __msm_gemini_probe,
	.remove = __msm_gemini_remove,
	.driver = {
		.name = "msm_gemini",
		.owner = THIS_MODULE,
	},
};

static int __init msm_gemini_driver_init(void)
{
	int rc;
	rc = platform_driver_register(&msm_gemini_driver);
	return rc;
}

static void __exit msm_gemini_driver_exit(void)
{
	platform_driver_unregister(&msm_gemini_driver);
}

MODULE_DESCRIPTION("msm gemini jpeg driver");
MODULE_VERSION("msm gemini 0.1");

module_init(msm_gemini_driver_init);
module_exit(msm_gemini_driver_exit);

