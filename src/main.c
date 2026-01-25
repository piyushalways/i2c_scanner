// I2C Scanner with BLE
// Scans all valid I2C addresses and reports which devices are found via BLE

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/logging/log.h>
#include <string.h>

#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/hci.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/bluetooth/gatt.h>

LOG_MODULE_REGISTER(i2c_scanner, LOG_LEVEL_INF);

// Get I2C device from devicetree
const struct device *i2c_dev = DEVICE_DT_GET(DT_NODELABEL(i2c21));

// GPIO configuration for pins 1.08, 1.15 and 2.10
const struct device *gpio_2dev = DEVICE_DT_GET(DT_NODELABEL(gpio2));
const struct device *gpio_1dev = DEVICE_DT_GET(DT_NODELABEL(gpio1));
#define GPIO_PIN_8  8
#define GPIO_PIN_15  15
#define GPIO_PIN_10 10

// I2C address range to scan
// Addresses 0x00-0x07 and 0x78-0x7F are reserved
#define I2C_SCAN_START  0x08
#define I2C_SCAN_END    0x77

#define MAX_FOUND_DEVICES 10

// BLE UUIDs - Custom service for I2C Scanner
#define BT_UUID_I2C_SCANNER_SERVICE_VAL \
	BT_UUID_128_ENCODE(0x12345678, 0x1234, 0x5678, 0x1234, 0x56789abcdef0)
#define BT_UUID_I2C_SCAN_RESULT_VAL \
	BT_UUID_128_ENCODE(0x12345678, 0x1234, 0x5678, 0x1234, 0x56789abcdef1)

#define BT_UUID_I2C_SCANNER_SERVICE BT_UUID_DECLARE_128(BT_UUID_I2C_SCANNER_SERVICE_VAL)
#define BT_UUID_I2C_SCAN_RESULT     BT_UUID_DECLARE_128(BT_UUID_I2C_SCAN_RESULT_VAL)

// Structure to hold I2C scan results for BLE
struct i2c_scan_result {
	uint8_t device_count;
	uint8_t addresses[MAX_FOUND_DEVICES];
} __packed;

static struct i2c_scan_result scan_result;
static bool ble_connected = false;

// BLE advertising data
static const struct bt_data ad[] = {
	BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),
	BT_DATA_BYTES(BT_DATA_UUID128_ALL, BT_UUID_I2C_SCANNER_SERVICE_VAL),
};

// BLE scan response data
static const struct bt_data sd[] = {
	BT_DATA(BT_DATA_NAME_COMPLETE, CONFIG_BT_DEVICE_NAME, sizeof(CONFIG_BT_DEVICE_NAME) - 1),
};

// GATT read callback for scan results
static ssize_t read_scan_result(struct bt_conn *conn,
				const struct bt_gatt_attr *attr,
				void *buf, uint16_t len, uint16_t offset)
{
	return bt_gatt_attr_read(conn, attr, buf, len, offset,
				 &scan_result, sizeof(scan_result));
}

// GATT Service Definition
BT_GATT_SERVICE_DEFINE(i2c_scanner_svc,
	BT_GATT_PRIMARY_SERVICE(BT_UUID_I2C_SCANNER_SERVICE),
	BT_GATT_CHARACTERISTIC(BT_UUID_I2C_SCAN_RESULT,
			       BT_GATT_CHRC_READ | BT_GATT_CHRC_NOTIFY,
			       BT_GATT_PERM_READ,
			       read_scan_result, NULL, &scan_result),
	BT_GATT_CCC(NULL, BT_GATT_PERM_READ | BT_GATT_PERM_WRITE),
);

// BLE connection callbacks
static void connected(struct bt_conn *conn, uint8_t err)
{
	if (err) {
		LOG_ERR("BLE connection failed (err 0x%02x)", err);
		return;
	}
	LOG_INF("BLE Connected");
	ble_connected = true;
}

static void disconnected(struct bt_conn *conn, uint8_t reason)
{
	LOG_INF("BLE Disconnected (reason 0x%02x)", reason);
	ble_connected = false;
}

BT_CONN_CB_DEFINE(conn_callbacks) = {
	.connected = connected,
	.disconnected = disconnected,
};

// Notify BLE clients of scan results
static void notify_scan_results(void)
{
	int err = bt_gatt_notify(NULL, &i2c_scanner_svc.attrs[1],
				 &scan_result, sizeof(scan_result));
	if (err && err != -ENOTCONN) {
		LOG_ERR("BLE notify failed (err %d)", err);
	}
}

// Initialize BLE
static int ble_init(void)
{
	int err;

	err = bt_enable(NULL);
	if (err) {
		LOG_ERR("Bluetooth init failed (err %d)", err);
		return err;
	}

	LOG_INF("Bluetooth initialized");

	err = bt_le_adv_start(BT_LE_ADV_CONN, ad, ARRAY_SIZE(ad), sd, ARRAY_SIZE(sd));
	if (err) {
		LOG_ERR("Advertising failed to start (err %d)", err);
		return err;
	}

	LOG_INF("Advertising started as '%s'", CONFIG_BT_DEVICE_NAME);
	return 0;
}

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
	int devices_found = 0;

	// Clear previous scan results
	memset(&scan_result, 0, sizeof(scan_result));

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
					scan_result.addresses[devices_found] = addr;
				}
				devices_found++;
			} else {
				printk("-- ");
			}
		}
		printk("\n");
	}

	// Update scan result count (cap at MAX_FOUND_DEVICES for BLE)
	scan_result.device_count = (devices_found > MAX_FOUND_DEVICES) ?
				    MAX_FOUND_DEVICES : devices_found;

	LOG_INF("Scan complete. Found %d device(s).", devices_found);
	for (int i = 0; i < scan_result.device_count; i++) {
		LOG_INF("Device[%d] -> 0x%02X", i, scan_result.addresses[i]);
	}

	// Notify BLE clients with updated scan results
	notify_scan_results();
	LOG_INF("BLE notification sent: %d devices", scan_result.device_count);
}

int main(void) {
	int ret;

	// Check if GPIO device is ready
	if (!device_is_ready(gpio_2dev)) {
		LOG_ERR("GPIO device not ready!");
		return -1;
	}
	if (!device_is_ready(gpio_1dev)) {
		LOG_ERR("GPIO device not ready!");
		return -1;
	}


	// Configure GPIO pin 1.08 as output and set it high
	ret = gpio_pin_configure(gpio_1dev, GPIO_PIN_8, GPIO_OUTPUT_INACTIVE);
	if (ret < 0) {
		LOG_ERR("Failed to configure GPIO pin %d: %d", GPIO_PIN_8, ret);
		return ret;
	}

	ret = gpio_pin_set(gpio_1dev, GPIO_PIN_8, 0);
	if (ret < 0) {
		LOG_ERR("Failed to set GPIO pin %d high: %d", GPIO_PIN_8, ret);
		return ret;
	}

	LOG_INF("GPIO 1.08 set HIGH");

	// Configure GPIO pin 1.15 as output and set it low
	ret = gpio_pin_configure(gpio_1dev, GPIO_PIN_15, GPIO_OUTPUT_ACTIVE);
	if (ret < 0) {
		LOG_ERR("Failed to configure GPIO pin %d: %d", GPIO_PIN_15, ret);
		return ret;
	}

	ret = gpio_pin_set(gpio_1dev, GPIO_PIN_15, 1);
	if (ret < 0) {
		LOG_ERR("Failed to set GPIO pin %d low: %d", GPIO_PIN_15, ret);
		return ret;
	}

	LOG_INF("GPIO 1.15 set LOW");

	// Configure GPIO pin 2.10 as output and set it low
	ret = gpio_pin_configure(gpio_2dev, GPIO_PIN_10, GPIO_OUTPUT_INACTIVE);
	if (ret < 0) {
		LOG_ERR("Failed to configure GPIO pin %d: %d", GPIO_PIN_10, ret);
		return ret;
	}

	ret = gpio_pin_set(gpio_2dev, GPIO_PIN_10, 0);
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

	// Initialize BLE
	ret = ble_init();
	if (ret) {
		LOG_ERR("BLE initialization failed!");
		return ret;
	}

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