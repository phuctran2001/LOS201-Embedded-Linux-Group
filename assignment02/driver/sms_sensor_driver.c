/*
 * Simulated temperature and humidity character device.
 * Author: Tran Thien Phuc - SE203798
 * Date: 2026-10-18
 */
#include <linux/module.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/err.h>
#include <linux/uaccess.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/mutex.h>
#include <linux/ktime.h>
#include <linux/random.h>

#include "sms_sensor.h"

#define SMS_CLASS_NAME "sms_sensor_class"

static int sensor_major;
static struct class *sensor_class;
static struct device *sensor_device;
static struct proc_dir_entry *stats_entry;
static DEFINE_MUTEX(sensor_lock);

static u64 started_sec;
static u64 last_read_ms;
static u64 read_count;
static u32 interval_ms = SMS_DEFAULT_INTERVAL_MS;
static u32 last_temp_centi = 2500;
static u32 last_hum_centi = 5000;

static ssize_t sensor_read(struct file *file, char __user *user_buf,
                           size_t count, loff_t *pos)
{
    char line[80];
    u64 now;
    u32 temp, hum;
    int length;

    if (*pos != 0)
        return 0;
    if (count == 0)
        return 0;
    if (mutex_lock_interruptible(&sensor_lock))
        return -ERESTARTSYS;

    now = ktime_to_ms(ktime_get());
    if (last_read_ms && now - last_read_ms < interval_ms) {
        mutex_unlock(&sensor_lock);
        return -EAGAIN;
    }

    /* Fixed point: 2534 means 25.34 degrees Celsius. */
    temp = 1500 + get_random_u32() % 3001;
    hum = 3000 + get_random_u32() % 6001;
    length = scnprintf(line, sizeof(line), "%llu,%u.%02u,%u.%02u\n",
                       (unsigned long long)now,
                       temp / 100, temp % 100,
                       hum / 100, hum % 100);

    if (count < length) {
        mutex_unlock(&sensor_lock);
        return -EINVAL;
    }
    if (copy_to_user(user_buf, line, length)) {
        mutex_unlock(&sensor_lock);
        return -EFAULT;
    }

    last_read_ms = now;
    last_temp_centi = temp;
    last_hum_centi = hum;
    read_count++;
    *pos += length;
    mutex_unlock(&sensor_lock);
    return length;
}

static loff_t sensor_llseek(struct file *file, loff_t offset, int whence)
{
    if (whence != SEEK_SET || offset != 0)
        return -EINVAL;
    file->f_pos = 0;
    return 0;
}

static long sensor_ioctl(struct file *file, unsigned int cmd,
                         unsigned long arg)
{
    u32 requested_ms;

    if (cmd != SMS_SET_INTERVAL)
        return -ENOTTY;
    if (copy_from_user(&requested_ms, (void __user *)arg,
                       sizeof(requested_ms)))
        return -EFAULT;
    if (requested_ms == 0 || requested_ms > 60000)
        return -EINVAL;

    mutex_lock(&sensor_lock);
    interval_ms = requested_ms;
    mutex_unlock(&sensor_lock);
    return 0;
}

static const struct file_operations sensor_fops = {
    .owner = THIS_MODULE,
    .read = sensor_read,
    .llseek = sensor_llseek,
    .unlocked_ioctl = sensor_ioctl,
};

static int stats_show(struct seq_file *out, void *unused)
{
    u64 uptime = ktime_get_seconds() - started_sec;

    mutex_lock(&sensor_lock);
    seq_printf(out, "read_count: %llu\n",
               (unsigned long long)read_count);
    seq_printf(out, "last_temperature: %u.%02u\n",
               last_temp_centi / 100, last_temp_centi % 100);
    seq_printf(out, "last_humidity: %u.%02u\n",
               last_hum_centi / 100, last_hum_centi % 100);
    seq_printf(out, "driver_uptime_sec: %llu\n",
               (unsigned long long)uptime);
    seq_printf(out, "min_interval_ms: %u\n", interval_ms);
    mutex_unlock(&sensor_lock);
    return 0;
}

static int stats_open(struct inode *inode, struct file *file)
{
    return single_open(file, stats_show, NULL);
}

static const struct proc_ops stats_ops = {
    .proc_open = stats_open,
    .proc_read = seq_read,
    .proc_lseek = seq_lseek,
    .proc_release = single_release,
};

static int __init sensor_init(void)
{
    int ret;

    started_sec = ktime_get_seconds();
    ret = register_chrdev(0, SMS_DEVICE_NAME, &sensor_fops);
    if (ret < 0)
        return ret;
    sensor_major = ret;

    sensor_class = class_create(THIS_MODULE, SMS_CLASS_NAME);
    if (IS_ERR(sensor_class)) {
        ret = PTR_ERR(sensor_class);
        goto undo_major;
    }

    sensor_device = device_create(sensor_class, NULL,
                                  MKDEV(sensor_major, 0), NULL,
                                  SMS_DEVICE_NAME);
    if (IS_ERR(sensor_device)) {
        ret = PTR_ERR(sensor_device);
        goto undo_class;
    }

    stats_entry = proc_create(SMS_PROC_NAME, 0444, NULL, &stats_ops);
    if (!stats_entry) {
        ret = -ENOMEM;
        goto undo_device;
    }

    pr_info("sms_sensor: loaded, major=%d\n", sensor_major);
    return 0;

undo_device:
    device_destroy(sensor_class, MKDEV(sensor_major, 0));
undo_class:
    class_destroy(sensor_class);
undo_major:
    unregister_chrdev(sensor_major, SMS_DEVICE_NAME);
    return ret;
}

static void __exit sensor_exit(void)
{
    proc_remove(stats_entry);
    device_destroy(sensor_class, MKDEV(sensor_major, 0));
    class_destroy(sensor_class);
    unregister_chrdev(sensor_major, SMS_DEVICE_NAME);
    pr_info("sms_sensor: unloaded\n");
}

module_init(sensor_init);
module_exit(sensor_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Simulated SMS temperature and humidity sensor");
