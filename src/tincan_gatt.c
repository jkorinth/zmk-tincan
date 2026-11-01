#include <stddef.h>
#include <stdint.h>
#include <zephyr/bluetooth/att.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/hci.h>
#include <zephyr/logging/log.h>
#include <tincan/tincan.h>
#include "tincan_frame.h"
#include "tincan_gatt.h"

// #define ZMK_TINCAN_PERIPHERAL_HEARTBEAT

LOG_MODULE_REGISTER(tincan_gatt, CONFIG_ZMK_LOG_LEVEL);

static struct {
    tincan_cb_t cb;
    struct bt_conn *conn;
} _tincan;

void tincan_listen(tincan_cb_t cb) {
    _tincan.cb = cb;
    LOG_INF("callback registered");
}

static bool notify_enabled;

static uint8_t tincan_rx_asm_buf[TINCAN_FRAME_MAX_MSG_LEN];
static size_t tincan_rx_asm_len;

static ssize_t on_write(struct bt_conn *conn, const struct bt_gatt_attr *attr, const void *buf,
                        uint16_t len, uint16_t offset, uint8_t flags) {
    // shortcut: a peer that aborts mid-message leaves this stuck assembling
    // until the next message or a disconnect resets it (see tincan_frame.h).
    int ret = tincan_frame_reassemble(buf, len, tincan_rx_asm_buf, sizeof(tincan_rx_asm_buf),
                                      &tincan_rx_asm_len);
    if (ret == -2) {
        LOG_ERR("frame reassembly error, dropping message");
    } else if (ret >= 0 && _tincan.cb) {
        _tincan.cb(tincan_rx_asm_buf, ret);
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

int tincan_speak(const void *payload, size_t length) {
    if (!_tincan.conn || !notify_enabled) {
        return -EACCES;
    }
    if (length > TINCAN_FRAME_MAX_MSG_LEN) {
        LOG_ERR("payload too large (%u > %u)", length, TINCAN_FRAME_MAX_MSG_LEN);
        return -EMSGSIZE;
    }

    uint16_t mtu = bt_gatt_get_mtu(_tincan.conn);
    if (!mtu) {
        mtu = BT_ATT_DEFAULT_LE_MTU;
    }
    size_t max_chunk_payload = mtu - 3 - TINCAN_FRAME_HDR_LEN;

    uint8_t frame[TINCAN_FRAME_HDR_LEN + TINCAN_FRAME_MAX_MSG_LEN];
    size_t offset = 0;
    bool more;
    do {
        int n = tincan_frame_write_chunk(payload, length, &offset, max_chunk_payload, frame,
                                         sizeof(frame), &more);
        if (n < 0) {
            LOG_ERR("frame too large for current MTU");
            return -EMSGSIZE;
        }
        int err = bt_gatt_notify(_tincan.conn, &tincan_svc.attrs[1], frame, n);
        if (err) {
            LOG_ERR("notify failed mid-message (%d)", err);
            return err;
        }
    } while (more);
    return 0;
}

size_t tincan_mtu_size(void) { return _tincan.conn ? bt_gatt_get_mtu(_tincan.conn) : 0; }

static void connected(struct bt_conn *conn, uint8_t err) {
    if (err) {
        LOG_ERR("%s: err = %d", __func__, err);
        return;
    }
    struct bt_conn_info info;
    bt_conn_get_info(conn, &info);
    if (info.role != BT_CONN_ROLE_PERIPHERAL) {
        LOG_WRN("connected, but role != peripheral, skipping");
        return;
    }
    _tincan.conn = conn;
}

static void disconnected(struct bt_conn *conn, uint8_t reason) {
    struct bt_conn_info info;
    bt_conn_get_info(conn, &info);
    if (info.role != BT_CONN_ROLE_PERIPHERAL) {
        return;
    }
    LOG_INF("disconnected, reason %s (%d)", bt_hci_err_to_str(reason), reason);
    _tincan.conn = NULL;
    tincan_rx_asm_len = 0;
}

BT_CONN_CB_DEFINE(tincan_gatt_conn_callbacks) = {
    .connected = connected,
    .disconnected = disconnected,
};

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
