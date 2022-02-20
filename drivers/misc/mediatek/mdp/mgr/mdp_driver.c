#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/proc_fs.h>
#include <linux/uaccess.h>
#include <linux/platform_device.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/fs.h>



#include "mdp_def.h"
#include "mdp_param.h"
#include "mdp_driver.h"
#include "mdp_core.h"
#include "mdp_log.h"
#include "mdp_fence.h"
#include "mdp_buffer.h"
#include "mdp_cli.h"

static struct proc_dir_entry   *gMdpProcEntry;
static struct proc_dir_entry *pDir;
static atomic_t is_suspend;

#define MDP_DEVICE_NAME "mdp_device"

static dev_t mdp_devno;
static struct cdev *mdp_cdev;
static struct class *mdp_class;


bool mdp_driver_is_suspend(void)
{
	return atomic_read(&is_suspend);
}

void mdp_driver_set_suspend(bool suspend)
{
	atomic_set(&is_suspend, suspend);
}

int mdp_config_frame_count;	/* debug mdp config buffer */

static long mdp_ioctl(struct file *pFile, unsigned int code, unsigned long param)
{
	struct mdp_command_struct command;
	enum MDP_TASK_STATUS status = MDP_TASK_STATUS_OK;

	if (mdp_driver_is_suspend()) {
		pr_err_ratelimited("HW is suspend, bypass\n");
		return 0;
	}

	switch (code) {
	case MDP_IOCTL_EXEC_COMMAND:
		if (copy_from_user(&command, (void *)param, sizeof(struct mdp_command_struct))) {
			pr_err_ratelimited("copy MDP_IOCTL_EXEC_COMMAND info failed\n");
			return MDP_TASK_STATUS_COPY_FROM_USER_FAIL;
		}
		mdp_config_frame_count++;

		/* force taskString last char = \0 to ensure taskString is a string type. */
		command.taskString[MAX_ARRAY_SIZE - 1] = '\0';

		status = mdp_core_handle_exec_command(&command);
		if (status != MDP_TASK_STATUS_OK)
			pr_err_ratelimited("exec MDP_IOCTL_EXEC_COMMAND error:%d\n", status);


		break;
	default:
		pr_err_ratelimited("undefined ioctl:0x%x kernel:0x%lx\n", code, (unsigned long)MDP_IOCTL_EXEC_COMMAND);
		pr_err_ratelimited("sizeof(mdp_command_struct) = %zu, sizeof(mdp_buffer_struct) = %zu, sizeof(uint32_t) = %zu, sizeof(size_t) = %zu, sizeof(DPCOLOR_ENUM) = %zu, sizeof(DpInterlaceFormat) = %zu\n",
			sizeof(struct mdp_command_struct),
			sizeof(struct mdp_buffer_struct),
			sizeof(uint32_t),
			sizeof(size_t),
			sizeof(enum DP_COLOR_ENUM),
			sizeof(enum DpInterlaceFormat));
		return MDP_TASK_STATUS_UNDEFINED_IOCTL;
	}

	if (copy_to_user((void *)param, (void *)&command, sizeof(struct mdp_command_struct))) {
		pr_err_ratelimited("copy to user fail\n");
		status = MDP_TASK_STATUS_COPY_TO_USER_FAIL;
	}

	return (long)status;
}

#ifdef CONFIG_COMPAT
static long mdp_ioctl_compat(struct file *pFile, unsigned int code, unsigned long param)
{
	switch (code) {
	case MDP_IOCTL_EXEC_COMMAND:
		return mdp_ioctl(pFile, code, param);
	default:
		pr_err_ratelimited("undefined compat ioctl:0x%x, kernel:0x%lx, size:%ld\n",
			code, MDP_IOCTL_EXEC_COMMAND, sizeof(struct mdp_command_struct));
		pr_err_ratelimited("sizeof(mdp_command_struct) = %ld, sizeof(mdp_buffer_struct) = %ld, sizeof(uint32_t) = %ld\n",
					sizeof(struct mdp_command_struct), sizeof(struct mdp_buffer_struct),
					sizeof(uint32_t));
		pr_err_ratelimited("sizeof(size_t) = %ld, sizeof(DPCOLOR_ENUM) = %ld, sizeof(DpInterlaceFormat) = %ld\n",
			sizeof(size_t), sizeof(enum DP_COLOR_ENUM), sizeof(enum DpInterlaceFormat));
		return MDP_TASK_STATUS_UNDEFINED_IOCTL;
	}

	return 0;
}
#endif

void mdp_shutdown(struct platform_device *pdev)
{
	int status = 0;

	if (mdp_driver_is_suspend()) {
		MDP_ERR("HW is already suspend\n");
		return;
	}
	mdp_driver_set_suspend(true);

	status = wait_event_timeout(global_mdp_task.mdp_core_wait_task_done_queue,
		list_empty(&global_mdp_task.trigger_imgresz_task_list.list),
		msecs_to_jiffies(MDP_WAIT_TASK_TIME_MS));

	if (status < 0) {
		/* Note: in this case, HW may access dram while power off disp_m domain.
		** This may cause HW hang.
		*/
		MDP_ERR("mdp_shutdown wait hw idle timeout");
	}
}

static const struct file_operations fop = {
	.owner = THIS_MODULE,
	.unlocked_ioctl = mdp_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = mdp_ioctl_compat,
#endif

};

static const struct file_operations error_fop = {
	.owner = THIS_MODULE,
	.open = mdp_proc_print_open_error,
	.read = seq_read,
};

static const struct file_operations record_fop = {
	.owner = THIS_MODULE,
	.open = mdp_proc_print_open_record,
	.read = seq_read,
};

static int mtk_mdp_device_probe(struct platform_device *pdev)
{
	int ret = 0;

	pr_err("harry2 call mdp probe\n");

	alloc_chrdev_region(&mdp_devno, 0, 1, MDP_DEVICE_NAME);
	mdp_cdev = cdev_alloc();
	mdp_cdev->owner = THIS_MODULE;
	mdp_cdev->ops = &fop;
	cdev_add(mdp_cdev, mdp_devno, 1);

	mdp_class = class_create(THIS_MODULE, MDP_DEVICE_NAME);
	device_create(mdp_class, NULL, mdp_devno, NULL, MDP_DEVICE_NAME);

	return ret;
}

static int mtk_mdp_device_remove(struct platform_device *pdev)
{
	int ret = 0;

	cdev_del(mdp_cdev);
	unregister_chrdev_region(mdp_devno, 1);
	device_destroy(mdp_class, mdp_devno);
	class_destroy(mdp_class);
	return ret;
}

static struct platform_device mtk_mdp_device = {
	.name = MDP_DEVICE_NAME,
};

static struct platform_driver mtk_mdp_driver = {
	.probe = mtk_mdp_device_probe,
	.remove = mtk_mdp_device_remove,
	.shutdown = NULL,
	.driver = {
		   .name = MDP_DEVICE_NAME,
		   .owner = THIS_MODULE,
		   },
	};

enum MDP_TASK_STATUS mdp_driver_create_fs(void)
{
	enum MDP_TASK_STATUS status = MDP_TASK_STATUS_OK;

	do {
		gMdpProcEntry = proc_mkdir("mdp", NULL);
		if (gMdpProcEntry == NULL) {
			pr_err("MDP_ERR: creat /proc/mdp dir fail\n");
			return MDP_TASK_STATUS_CREATE_MDP_DIR_FAIL;
		}

		pDir = proc_create("error", 0644, gMdpProcEntry, &error_fop);
		if (!pDir) {
			pr_err("MDP_ERR: create /proc/mdp/error fail\n");
			return MDP_TASK_STATUS_CREATE_MDP_ERROR_FAIL;
		}
		MDP_LOG("create /proc/mdp/error success\n");

		pDir = proc_create("record", 0644, gMdpProcEntry, &record_fop);
		if (!pDir) {
			pr_err("MDP_ERR: create /proc/mdp/record fail\n");
			return MDP_TASK_STATUS_CREATE_MDP_RECORD_FAIL;
		}
		MDP_LOG("create /proc/mdp/record success\n");

#if 0
		pDir = proc_create("mdp_device", 0644, gMdpProcEntry, &fop);
		if (pDir == NULL) {
			pr_err("MDP_ERR: create /proc/mdp/mdp_device fail\n");
			return MDP_TASK_STATUS_CREATE_MDP_DEVICE_FAIL;
		}
		MDP_LOG("create /proc/mdp/mdp_device success\n");
#endif
	} while (0);

	return status;
}


static int __init mdp_init(void)
{
	enum MDP_TASK_STATUS status = MDP_TASK_STATUS_OK;
	MDP_LOG("MDP module init\n");
	do {
		atomic_set(&is_suspend, false);

		if (platform_device_register(&mtk_mdp_device)) {
			status = MDP_TASK_STATE_ERROR;
			break;
		}

		if (platform_driver_register(&mtk_mdp_driver)) {
			platform_device_unregister(&mtk_mdp_device);
			status = MDP_TASK_STATE_ERROR;
			break;
		}

		status = mdp_proc_print_init();
		if (status != MDP_TASK_STATUS_OK) {
			platform_driver_unregister(&mtk_mdp_driver);
			platform_device_unregister(&mtk_mdp_device);
			break;
		}


		status = mdp_core_init();
		if (status != MDP_TASK_STATUS_OK) {
			platform_driver_unregister(&mtk_mdp_driver);
			platform_device_unregister(&mtk_mdp_device);
			break;

		}

		mdp_fence_init();
		#ifdef CONFIG_MTK_CLI_DEBUG_SUPPORT
		mdp_cli_init();
		#endif

		mdp_temp_buffer_init();

		/* creat /proc/mdp/ debug node. */
		status = mdp_driver_create_fs();
		if (status != MDP_TASK_STATUS_OK) {
			platform_driver_unregister(&mtk_mdp_driver);
			platform_device_unregister(&mtk_mdp_device);
			break;

		}
	} while (0);

	if (status != MDP_TASK_STATUS_OK)
		pr_err("init mdp device fail:%d\n", status);

	return status;
}

static void __exit mdp_exit(void)
{
	/* TODO release resource */
}


module_init(mdp_init);
module_exit(mdp_exit);

MODULE_DESCRIPTION("MTK MDP driver");
MODULE_AUTHOR("Harry<jiaguang.zhang@mediatek.com>");
MODULE_LICENSE("GPL");
