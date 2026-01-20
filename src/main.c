// // I2C Scanner
// // Scans all valid I2C addresses and reports which devices are found

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(i2c_scanner, LOG_LEVEL_INF);

// Get I2C device from devicetree
const struct device *i2c_dev = DEVICE_DT_GET(DT_NODELABEL(i2c21));

// GPIO configuration for pins 2.07, 2.09 and 2.10
const struct device *gpio_dev = DEVICE_DT_GET(DT_NODELABEL(gpio2));
#define GPIO_PIN_7  7
#define GPIO_PIN_9  9
#define GPIO_PIN_10 10

// I2C address range to scan
// Addresses 0x00-0x07 and 0x78-0x7F are reserved
#define I2C_SCAN_START  0x08
#define I2C_SCAN_END    0x77

#define MAX_FOUND_DEVICES 5

/**
 * @brief Test if a device exists at the given I2C address
 * @param addr I2C address to test
 * @return 0 if device found, negative error code otherwise
 */
static int test_i2c_address(uint8_t addr) {
	uint8_t dummy_data;
	// Try to read one byte from the device
	// Most I2C devices will ACK their address even with a simple read
	return i2c_read(i2c_dev, &dummy_data, 1, addr);
}

/**
 * @brief Scan all I2C addresses and report devices found
 */
static void scan_i2c_bus(void) {
	uint8_t found_addrs[MAX_FOUND_DEVICES] = {0};
	int devices_found = 0;

	LOG_INF("Scanning I2C bus...");
	LOG_INF("     0  1  2  3  4  5  6  7  8  9  A  B  C  D  E  F");

	for (uint8_t row = 0; row < 8; row++) {
		printk("%02X: ", row * 16);

		for (uint8_t col = 0; col < 16; col++) {
			uint8_t addr = (row * 16) + col;

			// Skip reserved addresses
			if (addr < I2C_SCAN_START || addr > I2C_SCAN_END) {
				printk("   ");
				continue;
			}

			// Test if device responds at this address
			if (test_i2c_address(addr) == 0) {
				printk("%02X ", addr);
				if (devices_found < MAX_FOUND_DEVICES) {
                    found_addrs[devices_found] = addr;
                }
				devices_found++;
			} else {
				printk("-- ");
			}
		}
		printk("\n");
	}

	LOG_INF("Scan complete. Found %d device(s).", devices_found);
	    for (int i = 0; i < MAX_FOUND_DEVICES; i++) {
        if (found_addrs[i] != 0) {
            LOG_INF("Device[%d] -> 0x%02X", i, found_addrs[i]);
        }
    }

}

int main(void) {
	int ret;

	// Check if GPIO device is ready
	if (!device_is_ready(gpio_dev)) {
		LOG_ERR("GPIO device not ready!");
		return -1;
	}

	// Configure GPIO pin 2.07 as output and set it high
	ret = gpio_pin_configure(gpio_dev, GPIO_PIN_7, GPIO_OUTPUT_ACTIVE);
	if (ret < 0) {
		LOG_ERR("Failed to configure GPIO pin %d: %d", GPIO_PIN_7, ret);
		return ret;
	}

	ret = gpio_pin_set(gpio_dev, GPIO_PIN_7, 1);
	if (ret < 0) {
		LOG_ERR("Failed to set GPIO pin %d high: %d", GPIO_PIN_7, ret);
		return ret;
	}

	LOG_INF("GPIO 2.07 set HIGH");

	// Configure GPIO pin 2.09 as output and set it low
	ret = gpio_pin_configure(gpio_dev, GPIO_PIN_9, GPIO_OUTPUT_INACTIVE);
	if (ret < 0) {
		LOG_ERR("Failed to configure GPIO pin %d: %d", GPIO_PIN_9, ret);
		return ret;
	}

	ret = gpio_pin_set(gpio_dev, GPIO_PIN_9, 0);
	if (ret < 0) {
		LOG_ERR("Failed to set GPIO pin %d low: %d", GPIO_PIN_9, ret);
		return ret;
	}

	LOG_INF("GPIO 2.09 set LOW");

	// Configure GPIO pin 2.10 as output and set it low
	ret = gpio_pin_configure(gpio_dev, GPIO_PIN_10, GPIO_OUTPUT_INACTIVE);
	if (ret < 0) {
		LOG_ERR("Failed to configure GPIO pin %d: %d", GPIO_PIN_10, ret);
		return ret;
	}

	ret = gpio_pin_set(gpio_dev, GPIO_PIN_10, 0);
	if (ret < 0) {
		LOG_ERR("Failed to set GPIO pin %d low: %d", GPIO_PIN_10, ret);
		return ret;
	}

	LOG_INF("GPIO 2.10 set low");

	// Check if I2C device is ready
	if (!device_is_ready(i2c_dev)) {
		LOG_ERR("I2C device not ready!");
		return -1;
	}

	LOG_INF("I2C device is ready");
	LOG_INF("Starting I2C bus scan...");
	LOG_INF("-----------------------------------");

	// Wait a moment for devices to stabilize
	k_msleep(100);

	// Perform continuous scanning (scan every 5 seconds)
	while (1) {
		scan_i2c_bus();
		LOG_INF("-----------------------------------");
		LOG_INF("Waiting 5 seconds before next scan...");
		k_msleep(5000);
	}

	return 0;
}


// I2C Scanner prints the addr. 
// Scans all valid I2C addresses and reports which devices are found

// #include <zephyr/kernel.h>
// #include <zephyr/device.h>
// #include <zephyr/drivers/i2c.h>
// #include <zephyr/drivers/gpio.h>
// #include <zephyr/logging/log.h>

// #define MAX_FOUND_DEVICES 5

// LOG_MODULE_REGISTER(i2c_scanner, LOG_LEVEL_INF);

// // Get I2C device from devicetree
// const struct device *i2c_dev = DEVICE_DT_GET(DT_NODELABEL(i2c21));

// // GPIO configuration for pins 2.07, 2.09 and 2.10
// const struct device *gpio_dev = DEVICE_DT_GET(DT_NODELABEL(gpio2));
// #define GPIO_PIN_7  7
// #define GPIO_PIN_9  9
// #define GPIO_PIN_10 10

// // I2C address range to scan
// // Addresses 0x00-0x07 and 0x78-0x7F are reserved
// #define I2C_SCAN_START  0x08
// #define I2C_SCAN_END    0x77

// /**
//  * @brief Test if a device exists at the given I2C address
//  * @param addr I2C address to test
//  * @return 0 if device found, negative error code otherwise
//  */
// static int test_i2c_address(uint8_t addr) {
// 	uint8_t dummy_data;
// 	// Try to read one byte from the device
// 	// Most I2C devices will ACK their address even with a simple read
// 	return i2c_read(i2c_dev, &dummy_data, 1, addr);
// }


// static void scan_i2c_bus(void) {
//     uint8_t found_addrs[MAX_FOUND_DEVICES] = {0};
//     int devices_found = 0;

//     LOG_INF("Scanning I2C bus...");
//     LOG_INF("     0  1  2  3  4  5  6  7  8  9  A  B  C  D  E  F");

//     for (uint8_t row = 0; row < 8; row++) {
//         printk("%02X: ", row * 16);

//         for (uint8_t col = 0; col < 16; col++) {
//             uint8_t addr = (row * 16) + col;

//             if (addr < I2C_SCAN_START || addr > I2C_SCAN_END) {
//                 printk("   ");
//                 continue;
//             }

//             if (test_i2c_address(addr) == 0) {
//                 printk("%02X ", addr);

//                 if (devices_found < MAX_FOUND_DEVICES) {
//                     found_addrs[devices_found] = addr;
//                 }
//                 devices_found++;
//             } else {
//                 printk("-- ");
//             }
//         }
//         printk("\n");
//     }

//     LOG_INF("Scan complete. Found %d device(s).", devices_found);

//     for (int i = 0; i < MAX_FOUND_DEVICES; i++) {
//         if (found_addrs[i] != 0) {
//             LOG_INF("Device[%d] -> 0x%02X", i, found_addrs[i]);
//         }
//     }
// }

// int main(void) {
// 	int ret;

// 	// Check if GPIO device is ready
// 	if (!device_is_ready(gpio_dev)) {
// 		LOG_ERR("GPIO device not ready!");
// 		return -1;
// 	}

// 	// Configure GPIO pin 2.07 as output and set it high
// 	ret = gpio_pin_configure(gpio_dev, GPIO_PIN_7, GPIO_OUTPUT_ACTIVE);
// 	if (ret < 0) {
// 		LOG_ERR("Failed to configure GPIO pin %d: %d", GPIO_PIN_7, ret);
// 		return ret;
// 	}

// 	ret = gpio_pin_set(gpio_dev, GPIO_PIN_7, 1);
// 	if (ret < 0) {
// 		LOG_ERR("Failed to set GPIO pin %d high: %d", GPIO_PIN_7, ret);
// 		return ret;
// 	}

// 	LOG_INF("GPIO 2.07 set HIGH");

// 	// Configure GPIO pin 2.09 as output and set it low
// 	ret = gpio_pin_configure(gpio_dev, GPIO_PIN_9, GPIO_OUTPUT_INACTIVE);
// 	if (ret < 0) {
// 		LOG_ERR("Failed to configure GPIO pin %d: %d", GPIO_PIN_9, ret);
// 		return ret;
// 	}

// 	ret = gpio_pin_set(gpio_dev, GPIO_PIN_9, 0);
// 	if (ret < 0) {
// 		LOG_ERR("Failed to set GPIO pin %d low: %d", GPIO_PIN_9, ret);
// 		return ret;
// 	}

// 	LOG_INF("GPIO 2.09 set LOW");

// 	// Configure GPIO pin 2.10 as output and set it low
// 	ret = gpio_pin_configure(gpio_dev, GPIO_PIN_10, GPIO_OUTPUT_INACTIVE);
// 	if (ret < 0) {
// 		LOG_ERR("Failed to configure GPIO pin %d: %d", GPIO_PIN_10, ret);
// 		return ret;
// 	}

// 	ret = gpio_pin_set(gpio_dev, GPIO_PIN_10, 0);
// 	if (ret < 0) {
// 		LOG_ERR("Failed to set GPIO pin %d low: %d", GPIO_PIN_10, ret);
// 		return ret;
// 	}

// 	LOG_INF("GPIO 2.10 set LOW");

// 	// Check if I2C device is ready
// 	if (!device_is_ready(i2c_dev)) {
// 		LOG_ERR("I2C device not ready!");
// 		return -1;
// 	}

// 	LOG_INF("I2C device is ready");
// 	LOG_INF("Starting I2C bus scan...");
// 	LOG_INF("-----------------------------------");

// 	// Wait a moment for devices to stabilize
// 	k_msleep(100);

// 	// Perform continuous scanning (scan every 5 seconds)
// 	while (1) {
// 		scan_i2c_bus();
// 		LOG_INF("-----------------------------------");
// 		LOG_INF("Waiting 5 seconds before next scan...");
// 		k_msleep(5000);
// 	}

// 	return 0;
// }

