#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <theft.h>

#include "../src/tincan_dispatch.h"
#include "../src/tincan_frame.h"
#include "../src/tincan_hello.h"

struct gen_msg {
    uint8_t data[TINCAN_FRAME_MAX_MSG_LEN];
    size_t len;
};

struct gen_chunk {
    size_t max_chunk_payload;
    uint8_t channel;
};

static enum theft_alloc_res alloc_msg(struct theft *t, void *env, void **output) {
    (void)env;
    struct gen_msg *m = malloc(sizeof(*m));
    if (!m) {
        return THEFT_ALLOC_ERROR;
    }
    m->len = (size_t)(theft_random_bits(t, 8) % (TINCAN_FRAME_MAX_MSG_LEN + 1));
    for (size_t i = 0; i < m->len; i++) {
        m->data[i] = (uint8_t)theft_random_bits(t, 8);
    }
    *output = m;
    return THEFT_ALLOC_OK;
}

static enum theft_alloc_res alloc_chunk(struct theft *t, void *env, void **output) {
    (void)env;
    struct gen_chunk *c = malloc(sizeof(*c));
    if (!c) {
        return THEFT_ALLOC_ERROR;
    }
    c->max_chunk_payload = 1 + (size_t)(theft_random_bits(t, 8) % TINCAN_FRAME_MAX_MSG_LEN);
    c->channel = (uint8_t)(theft_random_bits(t, 8) % (TINCAN_CHANNEL_MAX + 1));
    *output = c;
    return THEFT_ALLOC_OK;
}

static void free_cb(void *instance, void *env) {
    (void)env;
    free(instance);
}

// Property: splitting a message into frames and reassembling them always
// reconstructs the exact original bytes on the exact original channel,
// with only the final frame reporting completion.
static enum theft_trial_res prop_roundtrip(struct theft *t, void *arg1, void *arg2) {
    (void)t;
    const struct gen_msg *m = arg1;
    const struct gen_chunk *c = arg2;

    uint8_t frame[TINCAN_FRAME_HDR_LEN + TINCAN_FRAME_MAX_MSG_LEN];
    uint8_t asm_buf[TINCAN_FRAME_MAX_MSG_LEN];
    size_t asm_len = 0;
    size_t offset = 0;
    uint8_t channel = 0xff;
    bool more;
    int done;

    do {
        int n = tincan_frame_write_chunk(c->channel, m->data, m->len, &offset, c->max_chunk_payload,
                                         frame, sizeof(frame), &more);
        if (n < 0) {
            return THEFT_TRIAL_FAIL;
        }
        done = tincan_frame_reassemble(frame, (uint16_t)n, asm_buf, sizeof(asm_buf), &asm_len,
                                       &channel);
        if (more && done != TINCAN_FRAME_INCOMPLETE) {
            return THEFT_TRIAL_FAIL; // completed too early
        }
        if (!more && done < 0) {
            return THEFT_TRIAL_FAIL; // final frame should always complete
        }
    } while (more);

    if ((size_t)done != m->len) {
        return THEFT_TRIAL_FAIL;
    }
    if (channel != c->channel) {
        return THEFT_TRIAL_FAIL;
    }
    if (done > 0 && memcmp(asm_buf, m->data, (size_t)done) != 0) {
        return THEFT_TRIAL_FAIL;
    }
    return THEFT_TRIAL_PASS;
}

static void test_empty_message_roundtrip(void) {
    uint8_t frame[TINCAN_FRAME_HDR_LEN + 4];
    size_t offset = 0;
    bool more;
    int n = tincan_frame_write_chunk(0, NULL, 0, &offset, 4, frame, sizeof(frame), &more);
    assert(n == TINCAN_FRAME_HDR_LEN);
    assert(!more);

    uint8_t asm_buf[8];
    size_t asm_len = 0;
    uint8_t channel = 0xff;
    int done =
        tincan_frame_reassemble(frame, (uint16_t)n, asm_buf, sizeof(asm_buf), &asm_len, &channel);
    assert(done == 0);
    assert(channel == 0);
}

static void test_reassembly_overflow(void) {
    uint8_t asm_buf[4];
    size_t asm_len = 0;
    uint8_t channel = 0xff;
    uint8_t frame1[1 + 4] = {TINCAN_FRAME_MORE_FLAG, 1, 2, 3, 4};
    int r1 = tincan_frame_reassemble(frame1, sizeof(frame1), asm_buf, sizeof(asm_buf), &asm_len,
                                     &channel);
    assert(r1 == TINCAN_FRAME_INCOMPLETE);

    uint8_t frame2[1 + 1] = {0, 5}; // would push asm_len to 5 > cap 4
    int r2 = tincan_frame_reassemble(frame2, sizeof(frame2), asm_buf, sizeof(asm_buf), &asm_len,
                                     &channel);
    assert(r2 == TINCAN_FRAME_ERROR);
    assert(asm_len == 0);
}

static void test_channel_desync(void) {
    uint8_t asm_buf[8];
    size_t asm_len = 0;
    uint8_t channel = 0xff;

    // first fragment on channel 2, more to come
    uint8_t frame1[1 + 2] = {(uint8_t)(TINCAN_FRAME_MORE_FLAG | (2 << TINCAN_FRAME_CHANNEL_SHIFT)),
                             1, 2};
    int r1 = tincan_frame_reassemble(frame1, sizeof(frame1), asm_buf, sizeof(asm_buf), &asm_len,
                                     &channel);
    assert(r1 == TINCAN_FRAME_INCOMPLETE);
    assert(channel == 2);

    // a fragment claiming channel 3 arrives mid-message -> desync
    uint8_t frame2[1 + 2] = {(uint8_t)(3 << TINCAN_FRAME_CHANNEL_SHIFT), 3, 4};
    int r2 = tincan_frame_reassemble(frame2, sizeof(frame2), asm_buf, sizeof(asm_buf), &asm_len,
                                     &channel);
    assert(r2 == TINCAN_FRAME_ERROR);
    assert(asm_len == 0);
}

static void test_writer_out_cap_too_small(void) {
    uint8_t msg[4] = {1, 2, 3, 4};
    size_t offset = 0;
    bool more;
    int n = tincan_frame_write_chunk(0, msg, sizeof(msg), &offset, 4, NULL, 0, &more);
    assert(n == TINCAN_FRAME_WRITE_ERROR);
}

static void test_hello_roundtrip(void) {
    uint8_t buf[TINCAN_HELLO_LEN];
    int n = tincan_hello_encode(100, buf, sizeof(buf));
    assert(n == TINCAN_HELLO_LEN);

    struct tincan_hello hello;
    assert(tincan_hello_decode(buf, n, &hello) == 0);
    assert(hello.version == TINCAN_PROTO_VERSION);
    assert(hello.flags == 0);
    assert(hello.max_msg == 100);

    assert(tincan_hello_decode(buf, n - 1, &hello) == -1);
}

static int listener_a_calls;
static int listener_b_calls;

static void listener_a(const void *payload, size_t length) {
    (void)payload;
    (void)length;
    listener_a_calls++;
}

static void listener_b(const void *payload, size_t length) {
    (void)payload;
    (void)length;
    listener_b_calls++;
}

static void test_dispatch_reaches_only_its_channel(void) {
    assert(tincan_listen(3, listener_a) == 0);
    assert(tincan_listen(5, listener_b) == 0);
    assert(tincan_listen(TINCAN_CHANNEL_MAX + 1, listener_a) == -EINVAL);

    tincan_dispatch(3, "x", 1);
    assert(listener_a_calls == 1);
    assert(listener_b_calls == 0);

    tincan_dispatch(5, "x", 1);
    assert(listener_a_calls == 1);
    assert(listener_b_calls == 1);

    tincan_dispatch(4, "x", 1); // nobody registered, no-op
    assert(listener_a_calls == 1);
    assert(listener_b_calls == 1);
}

int main(void) {
    test_empty_message_roundtrip();
    test_reassembly_overflow();
    test_channel_desync();
    test_writer_out_cap_too_small();
    test_hello_roundtrip();
    test_dispatch_reaches_only_its_channel();

    struct theft_type_info msg_info = {.alloc = alloc_msg, .free = free_cb};
    struct theft_type_info chunk_info = {.alloc = alloc_chunk, .free = free_cb};
    struct theft_run_config cfg = {
        .name = "frame_roundtrip",
        .prop2 = prop_roundtrip,
        .type_info = {&msg_info, &chunk_info},
        .trials = 1000,
    };
    enum theft_run_res res = theft_run(&cfg);
    assert(res == THEFT_RUN_PASS);

    printf("all tincan_frame tests passed\n");
    return 0;
}
