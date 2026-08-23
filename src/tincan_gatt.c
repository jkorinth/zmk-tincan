#include <stddef.h>
#include <stdint.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/logging/log.h>
#include <tincan/tincan.h>
#include "tincan_gatt.h"

// #define ZMK_TINCAN_PERIPHERAL_HEARTBEAT

LOG_MODULE_REGISTER(tincan_gatt, CONFIG_ZMK_LOG_LEVEL);

static struct {
	tincan_cb_t cb;
} _tincan;

void tincan_listen(tincan_cb_t cb) {
    _tincan.cb = cb;
    LOG_INF("callback registered");
}

static bool notify_enabled;

static ssize_t on_write(struct bt_conn *conn, const struct bt_gatt_attr *attr, const void *buf,
                        uint16_t len, uint16_t offset, uint8_t flags) {
    LOG_DBG("received messsage from central: 0x%02x%02x", ((const uint8_t *)buf)[0],
            ((const uint8_t *)buf)[1]);
    if (_tincan.cb) {
	    _tincan.cb(buf, len);
    }
    return len;
}

static void on_ccc_change(const struct bt_gatt_attr *attr, uint16_t value) {
    notify_enabled = (value == BT_GATT_CCC_NOTIFY);
}

BT_GATT_SERVICE_DEFINE(tincan_svc, BT_GATT_PRIMARY_SERVICE(BT_UUID_TINCAN_SERVICE),
                       BT_GATT_CHARACTERISTIC(BT_UUID_TINCAN_CHAR,
                                              BT_GATT_CHRC_WRITE | BT_GATT_CHRC_NOTIFY,
                                              BT_GATT_PERM_WRITE, NULL, on_write, NULL),
                       BT_GATT_CCC(on_ccc_change, BT_GATT_PERM_READ | BT_GATT_PERM_WRITE), );

int tincan_send(struct bt_conn *conn, const uint8_t *data, uint16_t len) {
    if (!notify_enabled) {
        return -EACCES;
    }
    return bt_gatt_notify(conn, &tincan_svc.attrs[1], data, len);
}

#ifdef ZMK_TINCAN_PERIPHERAL_HEARTBEAT

static void heartbeat_handler(struct k_work_delayable *work) {
    LOG_INF("heartbeat!");
    LOG_INF("tincan service registered, %d attrs, char uuid in db", tincan_svc.attr_count);
    char uuid_str[BT_UUID_STR_LEN];
    bt_uuid_to_str(BT_UUID_TINCAN_CHAR, uuid_str, sizeof(uuid_str));
    LOG_INF("expecting char uuid: %s", uuid_str);
    k_work_schedule(work, K_SECONDS(5));
}

K_WORK_DELAYABLE_DEFINE(heartbeat, heartbeat_handler);

#endif

static int tincan_gatt_init(void) {
    LOG_INF("tincan service registered, %d attrs, char uuid in db", tincan_svc.attr_count);
    char uuid_str[BT_UUID_STR_LEN];
    bt_uuid_to_str(BT_UUID_TINCAN_CHAR, uuid_str, sizeof(uuid_str));
    LOG_DBG("expecting char uuid: %s", uuid_str);
#ifdef ZMK_TINCAN_PERIPHERAL_HEARTBEAT
    k_work_schedule(&heartbeat, K_SECONDS(5));
#endif
    return 0;
}
SYS_INIT(tincan_gatt_init, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);
