#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <string.h>

#define DEVICE_FILE "/dev/ADXL345"

// Structure matching the kernel driver for configuration
struct adxl345_config {
    uint8_t data_format;
    uint8_t power_ctl;
    uint8_t bw_rate;
    uint8_t fifo_ctl;
};

#define IOCTL_GET_CONFIG 1
#define IOCTL_SET_CONFIG 2

void read_sensor_data(int fd) {
    char buf[64];
    if (read(fd, buf, sizeof(buf)) < 0) {
        perror("Failed to read sensor data");
    } else {
        printf("Sensor Data: %s\n", buf);
    }
}

void get_current_config(int fd) {
    struct adxl345_config config;
    if (ioctl(fd, IOCTL_GET_CONFIG, &config) < 0) {
        perror("Failed to get configuration");
        return;
    }

    printf("Current Configuration:\n");
    printf("  Data Format:  0x%X\n", config.data_format);
    printf("  Power Mode:   0x%X\n", config.power_ctl);
    printf("  Bandwidth:    0x%X\n", config.bw_rate);
    printf("  FIFO Control: 0x%X\n", config.fifo_ctl);
}

void set_new_config(int fd) {
    struct adxl345_config new_config;

    printf("\nEnter new configuration values:\n");
    printf("Data Format (0x00-0x0F): ");
    scanf("%hhx", &new_config.data_format);

    printf("Power Control (0x00-0xFF): ");
    scanf("%hhx", &new_config.power_ctl);

    printf("Bandwidth Rate (0x00-0xFF): ");
    scanf("%hhx", &new_config.bw_rate);

    printf("FIFO Control (0x00-0xFF): ");
    scanf("%hhx", &new_config.fifo_ctl);

    if (ioctl(fd, IOCTL_SET_CONFIG, &new_config) < 0) {
        perror("Failed to set new configuration");
    } else {
        printf("New configuration applied successfully.\n");
    }
}

int main() {
    int fd = open(DEVICE_FILE, O_RDWR);
    if (fd < 0) {
        perror("Failed to open device file");
        return EXIT_FAILURE;
    }

    int choice;
    while (1) {
        printf("\nADXL345 Configuration Menu:\n");
        printf("1. Read Sensor Data\n");
        printf("2. Get Current Configuration\n");
        printf("3. Set New Configuration\n");
        printf("4. Exit\n");
        printf("Enter choice: ");
        scanf("%d", &choice);

        switch (choice) {
            case 1:
                read_sensor_data(fd);
                break;
            case 2:
                get_current_config(fd);
                break;
            case 3:
                set_new_config(fd);
                break;
            case 4:
                close(fd);
                return EXIT_SUCCESS;
            default:
                printf("Invalid choice. Please try again.\n");
        }
    }

    close(fd);
    return EXIT_SUCCESS;
}

