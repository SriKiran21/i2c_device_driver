#include <linux/module.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/kernel.h>
#include <linux/cdev.h>
#include <linux/fs.h>
#include <linux/uaccess.h>

#define I2C_BUS_AVAILABLE   (2)
#define SLAVE_DEVICE_NAME   "ADXL345"
#define adxl_SLAVE_ADDR     (0x53)

#define ADXL_REG_DATA_FORMAT  0x31
#define ADXL_REG_POWER_CTL    0x2D
#define ADXL_REG_BW_RATE      0x2C
#define ADXL_REG_FIFO_CTL     0x38
#define ADXL_REG_DATAX0       0x32

#define FIFO_WATERMARK_LEVEL  0x1F

static struct i2c_client *adxl_i2c_client = NULL;
static dev_t dev_number;
static struct cdev adxl_cdev;
static struct class *adxl_class;

struct adxl345_config {
    uint8_t data_format;
    uint8_t power_ctl;
    uint8_t bw_rate;
    uint8_t fifo_ctl;
};

// Default configuration
static struct adxl345_config adxl_config = {
    .data_format = 0x0B, // Full resolution, ±16g
    .power_ctl = 0x08,   // Measurement mode
    .bw_rate = 0x0A,     // 100 Hz
    .fifo_ctl = 0x80 | FIFO_WATERMARK_LEVEL // FIFO enabled, watermark level set
};

static int I2C_Write(struct i2c_client *client, unsigned char reg_addr, unsigned char *buf, unsigned int len) {
    struct i2c_msg msg;
    msg.addr = client->addr;
    msg.flags = 0;
    msg.len = len + 1;
    msg.buf = kmalloc(msg.len, GFP_KERNEL);

    if (!msg.buf)
        return -ENOMEM;

    msg.buf[0] = reg_addr;
    memcpy(&msg.buf[1], buf, len);
    int ret = i2c_transfer(client->adapter, &msg, 1);

    kfree(msg.buf);
    return ret;
}

static int I2C_Read(struct i2c_client *client, unsigned char reg_addr, unsigned char *out_buf, unsigned int len) {
    struct i2c_msg msg[2] = {
        { .addr = client->addr, .flags = 0, .len = 1, .buf = &reg_addr },
        { .addr = client->addr, .flags = I2C_M_RD, .len = len, .buf = out_buf }
    };
    return i2c_transfer(client->adapter, msg, 2);
}

// Read sensor data
static ssize_t adxl_read(struct file *file, char __user *user_buf, size_t count, loff_t *offset) {
    uint8_t data[6];
    int16_t x, y, z;

    if (I2C_Read(adxl_i2c_client, ADXL_REG_DATAX0, data, 6) < 0)
        return -EIO;

    x = (int16_t)(data[0] | (data[1] << 8));
    y = (int16_t)(data[2] | (data[3] << 8));
    z = (int16_t)(data[4] | (data[5] << 8));

    char buf[64];
    int len = snprintf(buf, sizeof(buf), "X: %d, Y: %d, Z: %d\n", x, y, z);

    if (copy_to_user(user_buf, buf, len))
        return -EFAULT;

    return len;
}

// Write configuration
static ssize_t adxl_write(struct file *file, const char __user *user_buf, size_t count, loff_t *offset) {
    struct adxl345_config new_config;

    if (copy_from_user(&new_config, user_buf, sizeof(new_config)))
        return -EFAULT;

    I2C_Write(adxl_i2c_client, ADXL_REG_DATA_FORMAT, &new_config.data_format, 1);
    I2C_Write(adxl_i2c_client, ADXL_REG_POWER_CTL, &new_config.power_ctl, 1);
    I2C_Write(adxl_i2c_client, ADXL_REG_BW_RATE, &new_config.bw_rate, 1);
    I2C_Write(adxl_i2c_client, ADXL_REG_FIFO_CTL, &new_config.fifo_ctl, 1);

    memcpy(&adxl_config, &new_config, sizeof(new_config));
    return count;
}

static long adxl_ioctl(struct file *file, unsigned int cmd, unsigned long arg) {
    struct adxl345_config config;

    switch (cmd) {
        case 1: // Get config
            if (copy_to_user((void __user *)arg, &adxl_config, sizeof(adxl_config)))
                return -EFAULT;
            break;
        case 2: // Set config
            if (copy_from_user(&config, (void __user *)arg, sizeof(config)))
                return -EFAULT;
            return adxl_write(file, (char *)&config, sizeof(config), 0);
        default:
            return -EINVAL;
    }

    return 0;
}

static struct file_operations fops = {
    .owner   = THIS_MODULE,
    .read    = adxl_read,
    .write   = adxl_write,
    .unlocked_ioctl = adxl_ioctl,
};

static int adxl_probe(struct i2c_client *client, const struct i2c_device_id *id) {
    adxl_i2c_client = client;

    // Apply default configuration
    adxl_write(NULL, (char *)&adxl_config, sizeof(adxl_config), 0);

    // Register char device
    alloc_chrdev_region(&dev_number, 0, 1, SLAVE_DEVICE_NAME);
    cdev_init(&adxl_cdev, &fops);
    cdev_add(&adxl_cdev, dev_number, 1);
    adxl_class = class_create(THIS_MODULE, SLAVE_DEVICE_NAME);
    device_create(adxl_class, NULL, dev_number, NULL, SLAVE_DEVICE_NAME);

    pr_info("ADXL345 driver loaded\n");
    return 0;
}

static int adxl_remove(struct i2c_client *client) {
    device_destroy(adxl_class, dev_number);
    class_destroy(adxl_class);
    cdev_del(&adxl_cdev);
    unregister_chrdev_region(dev_number, 1);
    pr_info("ADXL345 driver removed\n");
    return 0;
}

static const struct i2c_device_id adxl_id[] = {
    { SLAVE_DEVICE_NAME, 0 },
    { }
};

static struct i2c_driver adxl_driver = {
    .driver = { .name = SLAVE_DEVICE_NAME },
    .probe = adxl_probe,
    .remove = adxl_remove,
    .id_table = adxl_id,
};

module_i2c_driver(adxl_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Kiran <sasikiran021@gmail.com>");
MODULE_DESCRIPTION("ADXL345 Accelerometer I2C Driver");

