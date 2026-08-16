#include <stdint.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/bluetooth/hci.h>
#include <zephyr/logging/log.h>

#include "tincan.h"
#include "tincan_gatt.h"

LOG_MODULE_REGISTER(tincan, CONFIG_ZMK_LOG_LEVEL);

static struct bt_conn *_conn;
static struct bt_gatt_discover_params _dis_params;
static struct bt_gatt_subscribe_params _sub_params;
static struct bt_gatt_exchange_params exchange_params;
static uint16_t _tincan_char_handle, _tincan_ccc_handle, _tincan_mtu;
static tincan_cb_t _tincan_cb;

static void tincan_exchange_func(struct bt_conn *conn, uint8_t err,
                                 struct bt_gatt_exchange_params *params) {
    if (err) {
        LOG_ERR("MTU exchange failed (err %d)\n", err);
        return;
    }

    _tincan_mtu = bt_gatt_get_mtu(conn);
    LOG_WRN("MTU size is: %d\n", _tincan_mtu);
}

void tincan_exchange_params(struct bt_conn *conn, uint8_t conn_err) {
    exchange_params.func = tincan_exchange_func;
    int err = bt_gatt_exchange_mtu(conn, &exchange_params);
    if (err) {
        LOG_ERR("MTU exchange failed to start (err %d)\n", err);
    }
}

static void debug_send_work_handler(struct k_work *work);

K_WORK_DELAYABLE_DEFINE(debug_send_work, debug_send_work_handler);

static void debug_send_work_handler(struct k_work *work) {
    static uint8_t counter = 0;
    uint8_t payload[] = {0xAA, counter++};

    if (_conn) {
        LOG_INF("writing payload to peripheral");
        int ret = tincan_speak(payload, sizeof(payload));
        LOG_INF("wrote payload: %d", ret);
    } else {
        LOG_INF("no connection to peripheral, skipping write");
    }
    k_work_schedule(&debug_send_work, K_SECONDS(5));
}

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
        LOG_WRN("stopping discovery ccc");
        return BT_GATT_ITER_STOP;
    }

    _tincan_ccc_handle = attr->handle;
    LOG_INF("tincan CCC found, handle 0x%04x", attr->handle);

    _sub_params.notify = tincan_on_notify;
    _sub_params.value = BT_GATT_CCC_NOTIFY;
    _sub_params.value_handle = _tincan_char_handle;
    _sub_params.ccc_handle = _tincan_ccc_handle;

    int err = bt_gatt_subscribe(conn, &_sub_params);
    if (err) {
        LOG_ERR("subscribe failed (%d)", err);
    } else {
        LOG_ERR("subscribe success!");
        k_work_submit(&debug_send_work);
    }
    return BT_GATT_ITER_CONTINUE;
}

static uint8_t tincan_discover_chrc_cb(struct bt_conn *conn, const struct bt_gatt_attr *attr,
                                       struct bt_gatt_discover_params *params) {
    if (!attr) {
        LOG_WRN("stopping characteristic discovery");
        return BT_GATT_ITER_STOP;
    }

    struct bt_gatt_chrc *chrc = attr->user_data;
    char seen[BT_UUID_STR_LEN], want[BT_UUID_STR_LEN];
    bt_uuid_to_str(chrc->uuid, seen, sizeof(seen));
    bt_uuid_to_str(BT_UUID_TINCAN_CHAR, want, sizeof(want));

    if (bt_uuid_cmp(chrc->uuid, BT_UUID_TINCAN_CHAR) != 0) {
        LOG_INF("chrc @0x%04x uuid=%s (want %s) - skip", attr->handle, seen, want);
        return BT_GATT_ITER_CONTINUE;
    }
    LOG_INF("chrc @0x%04x uuid=%s - MATCH", attr->handle, seen);
    _tincan_char_handle = chrc->value_handle;
    LOG_INF("tincan char found, value handle %u", attr->handle);

    _dis_params.uuid = NULL; // BT_UUID_GATT_CCC;
    _dis_params.start_handle = attr->handle + 2;
    _dis_params.end_handle = 0xffff;
    _dis_params.type = BT_GATT_DISCOVER_DESCRIPTOR;
    _dis_params.func = tincan_discover_ccc_cb;

    int err = bt_gatt_discover(conn, &_dis_params);
    if (err) {
        LOG_ERR("CCC discover failed (%d)", err);
    }
    return BT_GATT_ITER_STOP;
}

static uint8_t tincan_discover_svc_cb(struct bt_conn *conn, const struct bt_gatt_attr *attr,
                                      struct bt_gatt_discover_params *params) {
    if (!attr) {
        LOG_WRN("ending discovery");
        return BT_GATT_ITER_STOP;
    }

    char seen[BT_UUID_STR_LEN], want[BT_UUID_STR_LEN];
    bt_uuid_to_str(attr->uuid, seen, sizeof(seen));
    bt_uuid_to_str(BT_UUID_TINCAN_CHAR, want, sizeof(want));

    LOG_INF("service found, handle 0x%04x:\nexpected: %s,\nfound: %s", attr->handle, want, seen);
    if (bt_uuid_cmp(attr->uuid, BT_UUID_TINCAN_SERVICE) != 0) {
        LOG_WRN("tincan service found!!");
    }

    _dis_params.uuid = NULL; // BT_UUID_TINCAN_CHAR;
    _dis_params.start_handle = attr->handle + 1;
    _dis_params.end_handle = 0xffff;
    _dis_params.type = BT_GATT_DISCOVER_CHARACTERISTIC;
    _dis_params.func = tincan_discover_chrc_cb;

    int err = bt_gatt_discover(conn, &_dis_params);
    if (err) {
        LOG_ERR("characteristic discover failed (%d)", err);
    }
    return BT_GATT_ITER_CONTINUE;
}

void tincan_start_discovery(struct bt_conn *conn) {
    _dis_params.uuid = NULL; // BT_UUID_TINCAN_SERVICE;
    _dis_params.func = tincan_discover_svc_cb;
    _dis_params.start_handle = BT_ATT_FIRST_ATTRIBUTE_HANDLE;
    _dis_params.end_handle = BT_ATT_LAST_ATTRIBUTE_HANDLE;
    _dis_params.type = BT_GATT_DISCOVER_PRIMARY;

    int err = bt_gatt_discover(conn, &_dis_params);
    if (err) {
        LOG_ERR("service discovery failed (%d)", err);
    }
}

static void discovery_handler(struct k_work *work) {
    tincan_exchange_params(_conn, 0);
    tincan_start_discovery(_conn);
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
        _conn = conn;
        k_work_schedule(&discovery, K_SECONDS(10));
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
    _conn = NULL;
}

BT_CONN_CB_DEFINE(conn_callbacks) = {
    .connected = connected,
    .disconnected = disconnected,
};

size_t tincan_mtu_size(void) { return _tincan_mtu; }

int tincan_speak(void *payload, size_t length) {
    static struct bt_gatt_write_params params;

    if (_conn) {
        LOG_INF("writing %u bytes payload to peripheral", length);
        params.data = payload;
        params.length = length;
        params.handle = _tincan_char_handle;
        params.func = 0;
        params.offset = 0;
        return bt_gatt_write(_conn, &params);
    } else {
        LOG_ERR("no connection to peripheral, skipping write");
    }
    return -EAGAIN;
}

void tincan_listen(tincan_cb_t cb) {
    _tincan_cb = cb;
    LOG_INF("callback registered");
}
