#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/cdev.h>
#include <linux/string.h>

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("BSU CS 452 HW4");
MODULE_AUTHOR("<andrewmartin418@u.boisestate.edu>");

typedef struct {
    char *s;
    int curr;
    char *separators;
    int custom_separators;  //per open()
    int endReached;
} Scanner;

typedef struct {
    dev_t devno;
    struct cdev cdev;
    char *default_separators; //per init()
} ScannerDevice;

static ScannerDevice device;

static int release(struct inode *inode, struct file *filp) {
    Scanner *scanner = filp->private_data;
    kfree(scanner->s);
    kfree(scanner->separators);
    kfree(scanner);
    return 0;
}

static int open(struct inode *inode, struct file *filp) {
    Scanner *scanner = (Scanner *)kmalloc(sizeof(*scanner), GFP_KERNEL);
    if (!scanner) {
        printk(KERN_ERR "%s: kmalloc() failed\n",DEVNAME);
        return -ENOMEM;
    }
    scanner->s = (char *)kmalloc(256 * sizeof(char), GFP_KERNEL);
    scanner->separators = (char *)kmalloc(256 * sizeof(char), GFP_KERNEL);
    
    if (!scanner->s || !scanner->separators) {
        printk(KERN_ERR "%s: kmalloc() failed\n",DEVNAME);
        kfree(scanner->s);
        kfree(scanner->separators);
        kfree(scanner);
        return -ENOMEM;
    }

    strcpy(scanner->separators, device.default_separators);
    scanner->curr = 0;
    scanner->endReached = 0;
    scanner->custom_separators = 0;
    filp->private_data = scanner;
    return 0;
}

static ssize_t read(struct file *filp, char *buf, size_t count, loff_t *f_pos) {
    size_t tok_len;
    Scanner *scanner = filp->private_data;
    char *kbuf = kmalloc(count + 1, GFP_KERNEL);
    if (!kbuf) {
        printk(KERN_ERR "%s: kmalloc() failed\n",DEVNAME);
        return -ENOMEM;
    }

    if (scanner->s[scanner->curr] == '\0') {
        if (scanner->endReached == 1) {
            return -1;
        }
        scanner->endReached = 1;
        return 0;
    }
    tok_len = 0;
    printk(KERN_INFO "Input line: %s\n", scanner->s);

    while (scanner->s[scanner->curr] && strchr(scanner->separators, scanner->s[scanner->curr])) {
        scanner->curr++;
    }

    while (scanner->s[scanner->curr] && !strchr(scanner->separators, scanner->s[scanner->curr])) {
        if (tok_len < count - 1) {
            kbuf[tok_len++] = scanner->s[scanner->curr];
        }
        scanner->curr++;
    }
    scanner->curr++;

    kbuf[tok_len] = '\0';
    if (copy_to_user(buf, kbuf, tok_len+1)) {
        printk(KERN_ERR "%s: copy_to_user() failed\n", DEVNAME);
        return -EFAULT;
    }
    kfree(kbuf);
    return tok_len;
}

static ssize_t write(struct file *filp, const char *buf, size_t count, loff_t *f_pos) {
    Scanner *scanner;
    char *kbuf = kmalloc(count + 1, GFP_KERNEL);
    if (!kbuf) {
        printk(KERN_ERR "%s: kmalloc() failed\n",DEVNAME);
        return -ENOMEM;
    }
    if (copy_from_user(kbuf, buf, count)) {
        printk(KERN_ERR "%s: copy_from_user() failed\n", DEVNAME);
        return -EFAULT;
    }
    kbuf[count] = '\0';

    if (kbuf[count - 1] == '\n') {
        kbuf[count - 1] = '\0';
        count--;
    } //Maybe
    
    scanner = (Scanner *)filp->private_data;
    if (scanner->custom_separators) {
        strcpy(scanner->separators, kbuf);
        scanner->custom_separators = 0;
    }
    else {
        strcpy(scanner->s, kbuf);
    }
    kfree(kbuf);
    return count;
}

static long ioctl(struct file *filp, unsigned int cmd, unsigned long arg) {
    if (cmd == 0) {
        Scanner *scanner = filp->private_data;
        scanner->custom_separators = 1;
    }
    return 0;
}

static void __exit my_exit(void) {
  cdev_del(&device.cdev);
  unregister_chrdev_region(device.devno,1);
  kfree(device.default_separators);
  printk(KERN_INFO "%s: exit\n",DEVNAME);
}

static struct file_operations ops = {
    .owner = THIS_MODULE,
    .open = open,
    .release = release,
    .read = read,
    .write = write,
    .unlocked_ioctl = ioctl
};

static int __init my_init(void) {
    int err;
    const char *DEFAULT_SEPARATORS = " ./;:!?";
    device.default_separators = (char *)kmalloc(strlen(DEFAULT_SEPARATORS) + 1, GFP_KERNEL);
    if (!device.default_separators) {
        printk(KERN_ERR "%s: kmalloc() failed\n",DEVNAME);
        return -ENOMEM;
    }
    strcpy(device.default_separators, DEFAULT_SEPARATORS);
    err=alloc_chrdev_region(&device.devno,0,1,DEVNAME);
    if (err<0) {
        printk(KERN_ERR "%s: alloc_chrdev_region() failed\n",DEVNAME);
        return err;
    }
    cdev_init(&device.cdev,&ops);
    device.cdev.owner=THIS_MODULE;
    err=cdev_add(&device.cdev,device.devno,1);
    if (err) {
        printk(KERN_ERR "%s: cdev_add() failed\n",DEVNAME);
        return err;
    }
    printk(KERN_INFO "%s: init\n",DEVNAME);
    return 0;
}

module_init(my_init);
module_exit(my_exit);
