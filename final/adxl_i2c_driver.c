#include <linux/module.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/kernel.h>
#include <linux/cdev.h>
#include <linux/fs.h>
#include <linux/i2c-dev.h>


#define I2C_BUS_AVAILABLE   (2)
#define SLAVE_DEVICE_NAME   ("ADXL345")
#define adxl_SLAVE_ADDR     (0x53)

#define FIFO_WATERMARK_LEVEL 0x1F // Define the FIFO watermark level

static struct i2c_adapter *adxl_i2c_adapter = NULL;
static struct i2c_client *adxl_i2c_client = NULL;
static int major;  // Declare major and minor here
static int minor;
static dev_t dev_number; // Declare dev_number here

static const unsigned short i2c_addresses[] = {
    adxl_SLAVE_ADDR,
    I2C_CLIENT_END
};


static int I2C_Write(struct i2c_client *client, unsigned char reg_addr, unsigned char *buf, unsigned int len)
{
    int ret;
    struct i2c_msg msg;

    msg.addr = client->addr;
    msg.flags = 0; // Write
    msg.len = len + 1;
    msg.buf = kmalloc(msg.len, GFP_KERNEL);

    if (!msg.buf)
        return -ENOMEM;

    msg.buf[0] = reg_addr;
    memcpy(&msg.buf[1], buf, len);

    ret = i2c_transfer(client->adapter, &msg, 1);

    kfree(msg.buf);

    if (ret < 0)
        pr_err("I2C Write error: %d\n", ret);

    return ret;
}

static int I2C_Read(struct i2c_client *client, unsigned char reg_addr, unsigned char *out_buf, unsigned int len)
{
    int ret;
    struct i2c_msg msg[2];

    msg[0].addr = client->addr;
    msg[0].flags = 0; // Write
    msg[0].len = 1;
    msg[0].buf = &reg_addr;

    msg[1].addr = client->addr;
    msg[1].flags = I2C_M_RD; // Read
    msg[1].len = len;
    msg[1].buf = out_buf;

    ret = i2c_transfer(client->adapter, msg, 2);

    if (ret < 0)
        pr_err("I2C Read error: %d\n", ret);

    return ret;
}


static int adxl_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
    unsigned char data[2];
    unsigned char fifo_data[6 * FIFO_WATERMARK_LEVEL];			//Since each entry in the FIFO buffer consists of three axes of acceleration data (X, Y, and Z), 
																//and each axis is represented by 2 bytes (16 bits),you need 6 bytes to represent one entry. 
																//Therefore, multiplying the watermark level by 6 gives you the total number of bytes needed 
																//to store the data samples up to the watermark level.
    int i;

    unsigned char fifo_status;

    pr_info("ADXL345 Probed!!!\n");

    // Configure Data Format (Full Resolution, +/- 16g range)
    data[0] = 0x31; // Data Format register
    data[1] = 0x0B; // Set full resolution and range
    I2C_Write(client, data[0], &data[1], 1);

    // Set Power Mode (Measurement mode)
    data[0] = 0x2D; // Power Control register
    data[1] = 0x08; // Set measurement mode
    I2C_Write(client, data[0], &data[1], 1);

    msleep(100); // Delay for sensor stabilization

    // Configure FIFO Mode and Watermark Level
    data[0] = 0x38; // FIFO Control register
    data[1] = 0x80 | FIFO_WATERMARK_LEVEL; // Set FIFO mode and watermark level
    I2C_Write(client, data[0], &data[1], 1);

    msleep(100); // Allow time for FIFO to fill

    // Read FIFO Data
    I2C_Read(client, 0x39, &fifo_status, 1); // Read FIFO status register

    int fifo_entries = fifo_status & 0x3F; // Extract number of entries in FIFO

    if (fifo_entries > 0) {
        I2C_Read(client, 0x32, fifo_data, 6 * fifo_entries);

        for (i = 0; i < fifo_entries; ++i) {
            int16_t x_accel = (int16_t)(fifo_data[i * 6] | (fifo_data[i * 6 + 1] << 8));
            int16_t y_accel = (int16_t)(fifo_data[i * 6 + 2] | (fifo_data[i * 6 + 3] << 8));
            int16_t z_accel = (int16_t)(fifo_data[i * 6 + 4] | (fifo_data[i * 6 + 5] << 8));

            pr_info("FIFO Entry %d - X: %hd, Y: %hd, Z: %hd\n", i, x_accel, y_accel, z_accel);
        }
    }

    // Return success
    return 0;
}



static int adxl_remove(struct i2c_client *client)
{
    pr_info("ADXL345 Removed!!!\n");
    return 0;
}

static const struct i2c_device_id adxl_id[] = {
    { SLAVE_DEVICE_NAME, 0 },
    { }
};
MODULE_DEVICE_TABLE(i2c, adxl_id);

static struct i2c_driver adxl_driver = {
    .driver = {
        .name   = SLAVE_DEVICE_NAME,
        .owner  = THIS_MODULE,
    },
    .probe          = adxl_probe,
    .remove         = adxl_remove,
    .id_table       = adxl_id,
};

static struct i2c_board_info adxl_i2c_board_info = {
    I2C_BOARD_INFO(SLAVE_DEVICE_NAME, adxl_SLAVE_ADDR)
};


static int adxl_driver_init(void)
{
    int ret = -1;
    adxl_i2c_adapter = i2c_get_adapter(I2C_BUS_AVAILABLE);

    if (adxl_i2c_adapter != NULL) {
        adxl_i2c_client = i2c_new_probed_device(adxl_i2c_adapter, &adxl_i2c_board_info, i2c_addresses, NULL);

        if (adxl_i2c_client != NULL) {
            i2c_add_driver(&adxl_driver);
            pr_info("ADXL345 Driver Added!!!\n");

            // Allocate a character device
            ret = alloc_chrdev_region(&dev_number, 0, 1, SLAVE_DEVICE_NAME);
            if (ret < 0) {
                pr_err("Failed to allocate major and minor numbers\n");
                return ret;
            }
            major = MAJOR(dev_number);
            minor = MINOR(dev_number);
            pr_info("Allocated character device: major=%d, minor=%d\n", major, minor);

            ret = 0;
        } else {
            pr_info("ADXL345 client not found!!!\n");
        }

        i2c_put_adapter(adxl_i2c_adapter);
    } else {
        pr_info("I2C Bus Adapter Not Available!!!\n");
    }

    return ret;
}

static void adxl_driver_exit(void)
{
    if (adxl_i2c_client != NULL) {
        i2c_unregister_device(adxl_i2c_client);
        i2c_del_driver(&adxl_driver);

        // Release the character device
        unregister_chrdev_region(dev_number, 1);
        pr_info("Released character device: major=%d, minor=%d\n", major, minor);
    }
    pr_info("ADXL345 Driver Removed!!!\n");
}

module_init(adxl_driver_init);
module_exit(adxl_driver_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Kiran <sasikiran021@gmail.com>");
MODULE_DESCRIPTION("ADXL345 Accelerometer I2C Driver");


