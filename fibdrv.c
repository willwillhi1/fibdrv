#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/kdev_t.h>
#include <linux/kernel.h>
#include <linux/kobject.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/sysfs.h>

#include "bignumber.h"

MODULE_LICENSE("Dual MIT/GPL");
MODULE_AUTHOR("National Cheng Kung University, Taiwan");
MODULE_DESCRIPTION("Fibonacci engine driver");
MODULE_VERSION("0.1");

#define DEV_FIBONACCI_NAME "fibonacci"

/* MAX_LENGTH is set to 92 because
 * ssize_t can't fit the number > 92
 */
#define MAX_LENGTH 1000

static dev_t fib_dev = 0;
static struct cdev *fib_cdev;
static struct class *fib_class;
static DEFINE_MUTEX(fib_mutex);

struct kobject *fib_ktime_kobj;
static ktime_t kt;
static long long kt_ns;

static unsigned long long double_fib(long long n)
{
    /* find first 1 */
    uint8_t h = 63 - __builtin_clzll(n);

    long long a = 0;
    long long b = 1;
    long long mask;
    for (int i = h; i >= 0; --i) {
        long long c = a * (2 * b - a);
        long long d = a * a + b * b;

        mask = -!!(n & (1UL << i));
        a = (c & ~mask) + (d & mask);
        b = (c & mask) + d;
    }
    return a;
}

static unsigned long long easy_fib(long long k)
{
    unsigned long long f[k + 2];
    f[0] = 0;
    f[1] = 1;
    for (int i = 2; i <= k; i++) {
        f[i] = f[i - 1] + f[i - 2];
    }
    return f[k];
}

/* calc n-th Fibonacci number and save into dest */
void bn_fib(bn *dest, unsigned int n)
{
    bn_resize(dest, 1);
    if (n < 2) {  // Fib(0) = 0, Fib(1) = 1
        dest->number[0] = n;
        return;
    }

    bn *a = bn_alloc(1);
    bn *b = bn_alloc(1);
    dest->number[0] = 1;

    for (unsigned int i = 1; i < n; i++) {
        bn_swap(b, dest);
        bn_add(a, b, dest);
        bn_swap(a, b);
    }
    bn_free(a);
    bn_free(b);
}

void bn_fib_fdoubling(bn *dest, unsigned int n)
{
    bn_resize(dest, 1);
    if (n < 2) {  // Fib(0) = 0, Fib(1) = 1
        dest->number[0] = n;
        return;
    }

    bn *f1 = dest;        /* F(k) */
    bn *f2 = bn_alloc(1); /* F(k+1) */
    f1->number[0] = 0;
    f2->number[0] = 1;
    bn *k1 = bn_alloc(1);
    bn *k2 = bn_alloc(1);

    for (unsigned int i = 1U << (31 - __builtin_clz(n)); i; i >>= 1) {
        // F(2k) = F(k) * [ 2 * F(k+1) – F(k) ]
        bn_lshift(f2, 1, k1);  // k1 = 2 * F(k+1)
        bn_mult(k1, f1, k2);   // k2 = 2 * F(k+1) * F(k)
        bn_mult(f1, f1, k1);   // k1 = F(k) * F(k)
        bn_sub(k2, k1, f1);    // f1 = F(2k) = 2 * F(k+1) * F(k) - F(k) * F(k)
        // F(2k+1) = F(k)^2 + F(k+1)^2
        bn_mult(f2, f2, k2);  // k2 = F(k+1) * F(k+1)
        bn_add(k1, k2, f2);   // f2 = F(2k+1) = F(k)^2 + F(k+1)^2

        if (n & i) {
            bn_swap(f1, f2);     // f1 = F(2k+1)
            bn_add(f1, f2, f2);  // f2 = F(2k+2)
        }
    }
    bn_free(f2);
    bn_free(k1);
    bn_free(k2);
}

static int fib_open(struct inode *inode, struct file *file)
{
    if (!mutex_trylock(&fib_mutex)) {
        printk(KERN_ALERT "fibdrv is in use");
        return -EBUSY;
    }
    return 0;
}

static int fib_release(struct inode *inode, struct file *file)
{
    mutex_unlock(&fib_mutex);
    return 0;
}

/* calculate the fibonacci number at given offset */
static ssize_t fib_read(struct file *file,
                        char *buf,
                        size_t size,
                        loff_t *offset)
{
    bn *fib = bn_alloc(1);
    bn_fib_fdoubling(fib, *offset);
    int len = fib->size;
    printk("__builtin_clz(fib->number[len-1]): %d\n",
           __builtin_clz(fib->number[len - 1]));
    int count = 32 - __builtin_clz(fib->number[len - 1]);
    count = (count >> 3) + !!(count & 0x7);
    copy_to_user(buf, fib->number, sizeof(unsigned int) * (len - 1) + count);
    bn_free(fib);
    return len;
}

/* write operation is skipped */
static ssize_t fib_write(struct file *file,
                         const char *buf,
                         size_t size,
                         loff_t *offset)
{
    bn *fib = bn_alloc(1);
    // ktime_t k1 = ktime_get();
    bn_fib_fdoubling(fib, *offset);
    // ktime_t k2 = ktime_sub(ktime_get(), k1);
    // char *p = bn_to_string(*fib);
    // size_t len = strlen(p) + 1;
    int len = fib->size;
    ktime_t k1 = ktime_get();
    // copy_to_user(buf, p, len);
    copy_to_user(buf, fib->number, sizeof(unsigned int) * len);
    ktime_t k2 = ktime_sub(ktime_get(), k1);
    bn_free(fib);
    // kfree(p);
    return ktime_to_ns(k2);
}

static loff_t fib_device_lseek(struct file *file, loff_t offset, int orig)
{
    loff_t new_pos = 0;
    switch (orig) {
    case 0: /* SEEK_SET: */
        new_pos = offset;
        break;
    case 1: /* SEEK_CUR: */
        new_pos = file->f_pos + offset;
        break;
    case 2: /* SEEK_END: */
        new_pos = MAX_LENGTH - offset;
        break;
    }

    if (new_pos > MAX_LENGTH)
        new_pos = MAX_LENGTH;  // max case
    if (new_pos < 0)
        new_pos = 0;        // min case
    file->f_pos = new_pos;  // This is what we'll use now
    return new_pos;
}

const struct file_operations fib_fops = {
    .owner = THIS_MODULE,
    .read = fib_read,
    .write = fib_write,
    .open = fib_open,
    .release = fib_release,
    .llseek = fib_device_lseek,
};

static ssize_t show(struct kobject *kobj,
                    struct kobj_attribute *attr,
                    char *buf)
{
    kt_ns = ktime_to_ns(kt);
    return snprintf(buf, 16, "%lld\n", kt_ns);
}

static ssize_t store(struct kobject *kobj,
                     struct kobj_attribute *attr,
                     const char *buf,
                     size_t count)
{
    int ret, n_th;
    if (ret = kstrtoint(buf, 10, &n_th)) {
        return ret;
    }
    bn *fib = bn_alloc(1);
    kt = ktime_get();
    bn_fib_fdoubling(fib, n_th);
    kt = ktime_sub(ktime_get(), kt);
    bn_free(fib);
    return count;
}

static struct kobj_attribute ktime_attr =
    __ATTR(kt_ns, 0664, show, store);  // rw-rw-r--

static struct attribute *ktime_attrs[] = {
    &ktime_attr.attr,
    NULL,
};

static struct attribute_group ktime_attr_group = {
    .attrs = ktime_attrs,
};

static int __init init_fib_dev(void)
{
    int rc = 0;

    mutex_init(&fib_mutex);

    // Let's register the device
    // This will dynamically allocate the major number
    rc = alloc_chrdev_region(&fib_dev, 0, 1, DEV_FIBONACCI_NAME);

    if (rc < 0) {
        printk(KERN_ALERT
               "Failed to register the fibonacci char device. rc = %i",
               rc);
        return rc;
    }

    fib_cdev = cdev_alloc();
    if (fib_cdev == NULL) {
        printk(KERN_ALERT "Failed to alloc cdev");
        rc = -1;
        goto failed_cdev;
    }
    fib_cdev->ops = &fib_fops;
    rc = cdev_add(fib_cdev, fib_dev, 1);

    if (rc < 0) {
        printk(KERN_ALERT "Failed to add cdev");
        rc = -2;
        goto failed_cdev;
    }

    fib_class = class_create(THIS_MODULE, DEV_FIBONACCI_NAME);

    if (!fib_class) {
        printk(KERN_ALERT "Failed to create device class");
        rc = -3;
        goto failed_class_create;
    }

    if (!device_create(fib_class, NULL, fib_dev, NULL, DEV_FIBONACCI_NAME)) {
        printk(KERN_ALERT "Failed to create device");
        rc = -4;
        goto failed_device_create;
    }

    fib_ktime_kobj = kobject_create_and_add("fib_ktime", kernel_kobj);
    if (!fib_ktime_kobj)
        return -ENOMEM;

    int retval = sysfs_create_group(fib_ktime_kobj, &ktime_attr_group);
    if (retval)
        goto failed_sysfs_create;

    return rc;
failed_sysfs_create:
    kobject_put(fib_ktime_kobj);
failed_device_create:
    class_destroy(fib_class);
failed_class_create:
    cdev_del(fib_cdev);
failed_cdev:
    unregister_chrdev_region(fib_dev, 1);
    return rc;
}

static void __exit exit_fib_dev(void)
{
    mutex_destroy(&fib_mutex);
    device_destroy(fib_class, fib_dev);
    class_destroy(fib_class);
    cdev_del(fib_cdev);
    unregister_chrdev_region(fib_dev, 1);
    kobject_put(fib_ktime_kobj);
}

module_init(init_fib_dev);
module_exit(exit_fib_dev);
