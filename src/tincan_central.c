#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/bluetooth/hci.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include <tincan/tincan.h>
#include "tincan_gatt.h"

// #define ZMK_TINCAN_DEBUG_SEND

LOG_MODULE_REGISTER(tincan, CONFIG_ZMK_LOG_LEVEL);

static struct tincan_t {
	struct bt_conn *conn;
	struct bt_gatt_discover_params dis_params;
	struct bt_gatt_subscribe_params sub_params;
	struct bt_gatt_exchange_params exc_params;
	uint16_t handle_char, handle_ccc;
	uint16_t mtu;
} _tincan;

#define TINCAN_MSG_MAX_LEN 128
#define TINCAN_TX_QUEUE_DEPTH 8

struct tincan_msg {
    uint8_t data[TINCAN_MSG_MAX_LEN];
    size_t len;
};

K_MSGQ_DEFINE(tincan_tx_q, sizeof(struct tincan_msg), TINCAN_TX_QUEUE_DEPTH, 4);

static struct bt_gatt_write_params tincan_write_params;
static struct tincan_msg tincan_tx_current;
static bool tincan_tx_busy;

static void tincan_tx_work_handler(struct k_work *work);
K_WORK_DEFINE(tincan_tx_work, tincan_tx_work_handler);

static void tincan_on_response(struct bt_conn *conn, uint8_t err, struct bt_gatt_write_params *params)
{
	if (err) {
		LOG_ERR("%s: write failed: %s (%d)", __func__, bt_gatt_err_to_str(err), err);
	}
	LOG_DBG("%s: write successful", __func__);
	tincan_tx_busy = false;
	k_work_submit(&tincan_tx_work); // send the next queued message, if any
}

static void tincan_tx_work_handler(struct k_work *work) {
    if (tincan_tx_busy || !_tincan.conn || !_tincan.handle_char) {
        return; // a write is in flight, or the link isn't ready yet — retried
                // from tincan_on_response() / once discovery completes
    }
    if (k_msgq_get(&tincan_tx_q, &tincan_tx_current, K_NO_WAIT) != 0) {
        return; // queue empty
    }

    tincan_write_params.data = tincan_tx_current.data;
    tincan_write_params.length = tincan_tx_current.len;
    tincan_write_params.handle = _tincan.handle_char;
    tincan_write_params.func = tincan_on_response;
    tincan_write_params.offset = 0;

    tincan_tx_busy = true;
    int ret = bt_gatt_write(_tincan.conn, &tincan_write_params);
    if (ret) {
        LOG_ERR("bt_gatt_write failed (%d), dropping queued message", ret);
        tincan_tx_busy = false;
        k_work_submit(&tincan_tx_work); // try the next one
    }
}

int tincan_speak(const void *payload, size_t length) {
    if (length > TINCAN_MSG_MAX_LEN) {
        LOG_ERR("payload too large (%u > %u)", length, TINCAN_MSG_MAX_LEN);
        return -EMSGSIZE;
    }

    struct tincan_msg msg = {.len = length};
    memcpy(msg.data, payload, length);

    int ret = k_msgq_put(&tincan_tx_q, &msg, K_NO_WAIT);
    if (ret) {
        LOG_ERR("tincan tx queue full, dropping message (%d)", ret);
        return ret;
    }

    k_work_submit(&tincan_tx_work);
    return 0;
}

static void tincan_exchange_func(struct bt_conn *conn, uint8_t err,
                                 struct bt_gatt_exchange_params *params) {
    if (err) {
        LOG_ERR("MTU exchange failed (err %d)\n", err);
        return;
    }

    _tincan.mtu = bt_gatt_get_mtu(conn);
    LOG_DBG("MTU size is: %d\n", _tincan.mtu);
}

void tincan_exchange_params(struct bt_conn *conn, uint8_t conn_err) {
    _tincan.exc_params.func = tincan_exchange_func;
    int err = bt_gatt_exchange_mtu(conn, &_tincan.exc_params);
    if (err) {
        LOG_ERR("MTU exchange failed to start (err %d)\n", err);
    }
}

#ifdef ZMK_TINCAN_DEBUG_SEND
static void debug_send_work_handler(struct k_work *work);

K_WORK_DELAYABLE_DEFINE(debug_send_work, debug_send_work_handler);

static void debug_send_work_handler(struct k_work *work) {
    static uint8_t counter = 0;
    uint8_t payload[] = {0xAA, counter++};

    if (_tincan.conn) {
        LOG_INF("writing payload to peripheral");
        int ret = tincan_speak(payload, sizeof(payload));
        LOG_INF("wrote payload: %d", ret);
    } else {
        LOG_INF("no connection to peripheral, skipping write");
    }
    k_work_schedule(&debug_send_work, K_SECONDS(5));
}
#endif

static uint8_t tincan_on_notify(struct bt_conn *conn, struct bt_gatt_subscribe_params *params,
                                const void *data, uint16_t length) {
    if (!data) {
        LOG_INF("%s: unsubscribed", __func__);
        return BT_GATT_ITER_STOP;
    }
    LOG_INF("%s: on_notify called: %u bytes data", __func__, length);
    return BT_GATT_ITER_CONTINUE;
}

static uint8_t tincan_discover_ccc_cb(struct bt_conn *conn, const struct bt_gatt_attr *attr,
                                      struct bt_gatt_discover_params *params) {
    if (!attr) {
        LOG_DBG("stopping discovery ccc");
        return BT_GATT_ITER_STOP;
    }

    _tincan.handle_ccc = attr->handle;
    LOG_INF("tincan CCC found, handle 0x%04x", attr->handle);

    _tincan.sub_params.notify = tincan_on_notify;
    _tincan.sub_params.value = BT_GATT_CCC_NOTIFY;
    _tincan.sub_params.value_handle = _tincan.handle_char;
    _tincan.sub_params.ccc_handle = _tincan.handle_ccc;

    int err = bt_gatt_subscribe(conn, &_tincan.sub_params);
    if (err) {
        LOG_ERR("subscribe failed (%d)", err);
    } else {
        LOG_INF("subscribe success!");
        k_work_submit(&tincan_tx_work);
#ifdef ZMK_TINCAN_DEBUG_SEND
        k_work_schedule(&debug_send_work, K_SECONDS(1));
#endif
    }
    return BT_GATT_ITER_CONTINUE;
}

static uint8_t tincan_discover_chrc_cb(struct bt_conn *conn, const struct bt_gatt_attr *attr,
                                       struct bt_gatt_discover_params *params) {
    if (!attr) {
        LOG_DBG("stopping characteristic discovery");
        return BT_GATT_ITER_STOP;
    }

    struct bt_gatt_chrc *chrc = attr->user_data;
    char seen[BT_UUID_STR_LEN], want[BT_UUID_STR_LEN];
    bt_uuid_to_str(chrc->uuid, seen, sizeof(seen));
    bt_uuid_to_str(BT_UUID_TINCAN_CHAR, want, sizeof(want));

    if (bt_uuid_cmp(chrc->uuid, BT_UUID_TINCAN_CHAR) != 0) {
        LOG_DBG("chrc @0x%04x uuid=%s (want %s) - skip", attr->handle, seen, want);
        return BT_GATT_ITER_CONTINUE;
    }
    LOG_DBG("chrc @0x%04x uuid=%s - MATCH", attr->handle, seen);
    _tincan.handle_char = chrc->value_handle;
    LOG_INF("tincan char found, value handle %u", attr->handle);

    _tincan.dis_params.uuid = BT_UUID_GATT_CCC;
    _tincan.dis_params.start_handle = attr->handle + 2;
    _tincan.dis_params.end_handle = 0xffff;
    _tincan.dis_params.type = BT_GATT_DISCOVER_DESCRIPTOR;
    _tincan.dis_params.func = tincan_discover_ccc_cb;

    int err = bt_gatt_discover(conn, &_tincan.dis_params);
    if (err) {
        LOG_ERR("CCC discover failed (%d)", err);
    }
    return BT_GATT_ITER_STOP;
}

static uint8_t tincan_discover_svc_cb(struct bt_conn *conn, const struct bt_gatt_attr *attr,
                                      struct bt_gatt_discover_params *params) {
    if (!attr) {
        LOG_DBG("ending discovery");
        return BT_GATT_ITER_STOP;
    }

    char seen[BT_UUID_STR_LEN], want[BT_UUID_STR_LEN];
    bt_uuid_to_str(attr->uuid, seen, sizeof(seen));
    bt_uuid_to_str(BT_UUID_TINCAN_CHAR, want, sizeof(want));

    LOG_DBG("service found, handle 0x%04x: expected: %s, found: %s", attr->handle, want, seen);
    if (bt_uuid_cmp(attr->uuid, BT_UUID_TINCAN_SERVICE) != 0) {
        LOG_INF("tincan service found!!");
    }

    // TODO weird - this should work like above? find out why not
    _tincan.dis_params.uuid = NULL; // BT_UUID_TINCAN_CHAR;
    _tincan.dis_params.start_handle = attr->handle + 1;
    _tincan.dis_params.end_handle = 0xffff;
    _tincan.dis_params.type = BT_GATT_DISCOVER_CHARACTERISTIC;
    _tincan.dis_params.func = tincan_discover_chrc_cb;

    int err = bt_gatt_discover(conn, &_tincan.dis_params);
    if (err) {
        LOG_ERR("characteristic discover failed (%d)", err);
    }
    return BT_GATT_ITER_CONTINUE;
}

void tincan_start_discovery(struct bt_conn *conn) {
    _tincan.dis_params.uuid = BT_UUID_TINCAN_SERVICE;
    _tincan.dis_params.func = tincan_discover_svc_cb;
    _tincan.dis_params.start_handle = BT_ATT_FIRST_ATTRIBUTE_HANDLE;
    _tincan.dis_params.end_handle = BT_ATT_LAST_ATTRIBUTE_HANDLE;
    _tincan.dis_params.type = BT_GATT_DISCOVER_PRIMARY;

    int err = bt_gatt_discover(conn, &_tincan.dis_params);
    if (err) {
        LOG_ERR("service discovery failed (%d)", err);
    }
}

static void discovery_handler(struct k_work *work) {
    tincan_exchange_params(_tincan.conn, 0);
    tincan_start_discovery(_tincan.conn);
}

K_WORK_DELAYABLE_DEFINE(discovery, discovery_handler);

static void connected(struct bt_conn *conn, uint8_t err) {
    if (!err) {
        struct bt_conn_info info;
        bt_conn_get_info(conn, &info);
        if (info.role != BT_CONN_ROLE_CENTRAL) {
            LOG_WRN("connected, but role != central, skipping");
            return;
        }
        _tincan.conn = conn;
        k_work_schedule(&discovery, K_SECONDS(4));
    } else {
        LOG_ERR("%s: err = %d", __func__, err);
    }
}

static void disconnected(struct bt_conn *conn, uint8_t reason) {
    struct bt_conn_info info;
    bt_conn_get_info(conn, &info);
    if (info.role != BT_CONN_ROLE_CENTRAL) {
        LOG_WRN("disconnected, but role != central, skipping");
        return;
    }
    LOG_INF("disconnected, reason %s (%d): , resetting conn", bt_hci_err_to_str(reason), reason);
    _tincan.conn = NULL;
    _tincan.handle_char = 0;
    tincan_tx_busy = false;
}

BT_CONN_CB_DEFINE(conn_callbacks) = {
    .connected = connected,
    .disconnected = disconnected,
};

size_t tincan_mtu_size(void) { return _tincan.mtu; }
