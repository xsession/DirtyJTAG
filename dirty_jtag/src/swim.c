/* SPDX-License-Identifier: MIT */
#include "djprog/swim.h"
#include "djprog/swim_phy.h"
#include "djprog/hw.h"
#include "djprog/common.h"
#include <errno.h>
#include <string.h>

/* UM0470 command binary codes.  They are three-bit command fields inside the
 * SWIM command frame, not standalone UART bytes. */
#define SWIM_CMD_SRST 0u
#define SWIM_CMD_ROTF 1u
#define SWIM_CMD_WOTF 2u

/* Conservative default: low-speed mode after activation.  High-speed mode is
 * enabled only after SWIM_CSR.HSIT is expected to be valid and SWIM_CSR.HS has
 * been written by WOTF. */
static uint32_t swim_hz = 363000u;
static bool mock_enabled;
static bool attached;
static bool use_packet_phy;
static const struct dj_swim_phy_ops *packet_phy;
static uint8_t mock_mem[0x10000];


__attribute__((weak)) int dj_swim_rp2040_pio_try_bind(uint8_t data0_gpio, uint32_t initial_hz)
{
    (void)data0_gpio;
    (void)initial_hz;
    return -ENOTSUP;
}

int dj_swim_phy_register(const struct dj_swim_phy_ops *ops)
{
    if (!ops || !ops->send_packet || !ops->recv_packet) return -EINVAL;
    packet_phy = ops;
    return 0;
}

const char *dj_swim_phy_name(void)
{
    if (use_packet_phy && packet_phy && packet_phy->name) return packet_phy->name;
    return mock_enabled ? "mock" : "gpio-soft";
}

bool dj_swim_phy_active(void)
{
    return use_packet_phy;
}

const char *dj_swim_selected_phy_name(void)
{
    return dj_swim_phy_name();
}

static void delay_ticks(unsigned ticks)
{
    const struct dj_hw_ops *h = dj_hw();
    if (!h || !h->delay_us) return;
    uint32_t us = (uint32_t)(((uint64_t)ticks * 1000000ull + swim_hz - 1u) / swim_hz);
    if (!us) us = 1u;
    h->delay_us(us);
}

static int swim_drive_low(unsigned ticks)
{
    const struct dj_hw_ops *h = dj_hw();
    if (!h) return -ENODEV;
    int r = h->dir(DJ_PIN_DATA0, DJ_DIR_OD_LOW);
    if (r) return r;
    r = h->write(DJ_PIN_DATA0, false);
    if (r) return r;
    delay_ticks(ticks);
    r = h->dir(DJ_PIN_DATA0, DJ_DIR_RELEASE);
    if (r) return r;
    delay_ticks(1u);
    return 0;
}

static int swim_send_bit(bool bit)
{
    if (swim_hz >= 700000u) {
        /* High speed: 10 clocks/bit: 2 low + 8 high for logical 1,
         * 8 low + 2 high for logical 0. */
        return swim_drive_low(bit ? 2u : 8u);
    }
    /* Low speed: 22 clocks/bit: 2 low + 20 high for logical 1,
     * 20 low + 2 high for logical 0. */
    return swim_drive_low(bit ? 2u : 20u);
}

static int swim_read_bit(bool *bit)
{
    const struct dj_hw_ops *h = dj_hw();
    if (!h || !bit) return -EINVAL;
    unsigned low = 0u;
    unsigned limit = swim_hz >= 700000u ? 12u : 26u;
    int r = h->dir(DJ_PIN_DATA0, DJ_DIR_INPUT);
    if (r) return r;
    for (unsigned i = 0; i < limit; ++i) {
        bool v = true;
        r = h->read(DJ_PIN_DATA0, &v);
        if (r) return r;
        if (!v) ++low;
        delay_ticks(1u);
    }
    *bit = swim_hz >= 700000u ? (low <= 4u) : (low <= 8u);
    return 0;
}

static int swim_send_packet_gpio(uint32_t value, unsigned bits)
{
    if (bits != 3u && bits != 8u) return -EINVAL;
    int r = swim_send_bit(false); /* header */
    if (r) return r;
    unsigned p = 0u;
    for (unsigned i = 0; i < bits; ++i) {
        bool b = ((value >> i) & 1u) != 0u;
        p ^= b ? 1u : 0u;
        r = swim_send_bit(b);
        if (r) return r;
    }
    r = swim_send_bit(p != 0u);
    if (r) return r;

    /* ACK bit: target pulls low for NACK.  With no attached hardware in unit
     * tests this reads as high through the pull-up and is therefore ACK. */
    bool ack = true;
    r = swim_read_bit(&ack);
    if (r) return r;
    return ack ? 0 : -EAGAIN;
}

static int swim_recv_packet_gpio(uint8_t *value)
{
    if (!value) return -EINVAL;
    bool bit = true;
    int r = swim_read_bit(&bit); /* header, expected zero */
    if (r) return r;
    if (bit) return -EIO;
    uint8_t x = 0u;
    unsigned p = 0u;
    for (unsigned i = 0; i < 8u; ++i) {
        r = swim_read_bit(&bit);
        if (r) return r;
        if (bit) x |= (uint8_t)(1u << i);
        p ^= bit ? 1u : 0u;
    }
    r = swim_read_bit(&bit);
    if (r) return r;
    if ((bit ? 1u : 0u) != p) return -EBADMSG;
    r = swim_send_bit(true); /* ACK */
    if (r) return r;
    *value = x;
    return 0;
}


static int swim_send_packet(uint32_t value, unsigned bits)
{
    if (use_packet_phy && packet_phy && packet_phy->send_packet) {
        return packet_phy->send_packet(value, bits);
    }
    return swim_send_packet_gpio(value, bits);
}

static int swim_recv_packet(uint8_t *value)
{
    if (use_packet_phy && packet_phy && packet_phy->recv_packet) {
        return packet_phy->recv_packet(value);
    }
    return swim_recv_packet_gpio(value);
}

static int write_byte_retry(uint8_t b)
{
    for (unsigned retry = 0; retry < 32u; ++retry) {
        int r = swim_send_packet(b, 8u);
        if (r == 0) return 0;
        if (r != -EAGAIN) return r;
    }
    return -ETIMEDOUT;
}

void dj_swim_mock_reset(void)
{
    memset(mock_mem, 0xff, sizeof(mock_mem));
    mock_mem[STM8_SWIM_CSR] = STM8_SWIM_CSR_SWIM_DM;
    mock_mem[STM8_DM_CSR1] = 0x10u;
    mock_mem[STM8_DM_CSR2] = 0x00u;
    mock_mem[STM8_DM_CR1] = 0x00u;
    mock_mem[STM8_DM_CR2] = 0x00u;
}

int dj_swim_mock_read(uint32_t addr, uint8_t *data, size_t len)
{
    if (!data || addr + len > sizeof(mock_mem)) return -EINVAL;
    memcpy(data, &mock_mem[addr], len);
    return 0;
}

int dj_swim_mock_write(uint32_t addr, const uint8_t *data, size_t len)
{
    if (!data || addr + len > sizeof(mock_mem)) return -EINVAL;
    memcpy(&mock_mem[addr], data, len);
    return 0;
}

int dj_swim_select(uint32_t requested_hz, bool mock_target)
{
    mock_enabled = mock_target;
    attached = false;
    use_packet_phy = false;
    if (mock_enabled) dj_swim_mock_reset();
    swim_hz = requested_hz ? requested_hz : 363000u;
    if (swim_hz > 8000000u) return -ERANGE;
    if (swim_hz < 100000u) swim_hz = 100000u;

    if (!mock_enabled && packet_phy) {
        bool avail = packet_phy->available ? packet_phy->available() : true;
        if (avail) {
            int r = packet_phy->select ? packet_phy->select(swim_hz) : 0;
            if (r == 0) use_packet_phy = true;
        }
    }
    return 0;
}

int dj_swim_enter(void)
{
    const struct dj_hw_ops *h = dj_hw();
    if (!h) return -ENODEV;
    if (mock_enabled) { attached = true; return 0; }

    int r = h->dir(DJ_PIN_DATA0, DJ_DIR_RELEASE);
    if (r) return r;
    if (use_packet_phy && packet_phy && packet_phy->enter) {
        r = packet_phy->enter();
    } else {
        /* Entry/communication reset: hold SWIM low for at least 128 SWIM clocks.
         * The target answers with a synchronization frame. */
        r = swim_drive_low(160u);
    }
    if (r) return r;
    attached = true;
    /* Enable SWIM_DM so full memory/DM access and SRST are legal. */
    uint8_t csr = STM8_SWIM_CSR_SWIM_DM;
    (void)dj_swim_write_mem(STM8_SWIM_CSR, &csr, 1u);
    return 0;
}

int dj_swim_leave(void)
{
    attached = false;
    if (use_packet_phy && packet_phy && packet_phy->leave) {
        int r = packet_phy->leave();
        if (r) return r;
    }
    const struct dj_hw_ops *h = dj_hw();
    return h && h->dir ? h->dir(DJ_PIN_DATA0, DJ_DIR_INPUT) : 0;
}

int dj_swim_system_reset(void)
{
    if (!attached) return -ENODEV;
    if (mock_enabled) {
        mock_mem[STM8_DM_CSR1] |= STM8_DM_CSR1_RST;
        mock_mem[STM8_DM_CSR2] |= STM8_DM_CSR2_STALL;
        return 0;
    }
    return swim_send_packet(SWIM_CMD_SRST, 3u);
}

int dj_swim_read_mem(uint32_t addr, uint8_t *data, size_t len)
{
    if (!data || len == 0u) return -EINVAL;
    if (!attached) return -ENODEV;
    if (mock_enabled) return dj_swim_mock_read(addr, data, len);

    size_t off = 0u;
    while (off < len) {
        size_t chunk = DJ_MIN((size_t)255u, len - off);
        int r = swim_send_packet(SWIM_CMD_ROTF, 3u);
        if (r) return r;
        r = write_byte_retry((uint8_t)chunk);
        if (r) return r;
        uint32_t a = addr + (uint32_t)off;
        r = write_byte_retry((uint8_t)(a >> 16)); if (r) return r;
        r = write_byte_retry((uint8_t)(a >> 8));  if (r) return r;
        r = write_byte_retry((uint8_t)a);         if (r) return r;
        for (size_t i = 0; i < chunk; ++i) {
            r = swim_recv_packet(&data[off + i]);
            if (r) return r;
        }
        off += chunk;
    }
    return 0;
}

int dj_swim_write_mem(uint32_t addr, const uint8_t *data, size_t len)
{
    if (!data || len == 0u) return -EINVAL;
    if (!attached) return -ENODEV;
    if (mock_enabled) return dj_swim_mock_write(addr, data, len);

    size_t off = 0u;
    while (off < len) {
        size_t chunk = DJ_MIN((size_t)255u, len - off);
        int r = swim_send_packet(SWIM_CMD_WOTF, 3u);
        if (r) return r;
        r = write_byte_retry((uint8_t)chunk);
        if (r) return r;
        uint32_t a = addr + (uint32_t)off;
        r = write_byte_retry((uint8_t)(a >> 16)); if (r) return r;
        r = write_byte_retry((uint8_t)(a >> 8));  if (r) return r;
        r = write_byte_retry((uint8_t)a);         if (r) return r;
        for (size_t i = 0; i < chunk; ++i) {
            r = write_byte_retry(data[off + i]);
            if (r) return r;
        }
        off += chunk;
    }
    return 0;
}

int dj_swim_set_speed(bool high_speed)
{
    uint8_t csr = 0u;
    int r = dj_swim_read_mem(STM8_SWIM_CSR, &csr, 1u);
    if (r) return r;
    csr |= STM8_SWIM_CSR_SWIM_DM;
    if (high_speed) csr |= STM8_SWIM_CSR_HS;
    else csr &= (uint8_t)~STM8_SWIM_CSR_HS;
    r = dj_swim_write_mem(STM8_SWIM_CSR, &csr, 1u);
    if (r) return r;
    swim_hz = high_speed ? 8000000u : 363000u;
    if (use_packet_phy && packet_phy && packet_phy->set_speed) {
        r = packet_phy->set_speed(high_speed, swim_hz);
        if (r) return r;
    }
    return 0;
}

static int set_csr2_bits(uint8_t set, uint8_t clear)
{
    uint8_t v = 0u;
    int r = dj_swim_read_mem(STM8_DM_CSR2, &v, 1u);
    if (r) return r;
    v = (uint8_t)((v | set) & (uint8_t)~clear);
    return dj_swim_write_mem(STM8_DM_CSR2, &v, 1u);
}

int dj_stm8_debug_attach(void)
{
    int r = dj_swim_enter();
    if (r) return r;
    uint8_t csr = 0u;
    r = dj_swim_read_mem(STM8_SWIM_CSR, &csr, 1u);
    if (r) return r;
    csr |= STM8_SWIM_CSR_SWIM_DM;
    r = dj_swim_write_mem(STM8_SWIM_CSR, &csr, 1u);
    if (r) return r;
    /* Stop watchdogs while halted where allowed. */
    uint8_t cr1 = STM8_DM_CR1_WDGOFF;
    (void)dj_swim_write_mem(STM8_DM_CR1, &cr1, 1u);
    return 0;
}

int dj_stm8_debug_detach(void)
{
    (void)dj_stm8_debug_run();
    return dj_swim_leave();
}

int dj_stm8_debug_status(uint8_t *csr1, uint8_t *csr2, bool *halted)
{
    uint8_t c1 = 0u, c2 = 0u;
    int r = dj_swim_read_mem(STM8_DM_CSR1, &c1, 1u);
    if (r) return r;
    r = dj_swim_read_mem(STM8_DM_CSR2, &c2, 1u);
    if (r) return r;
    if (csr1) *csr1 = c1;
    if (csr2) *csr2 = c2;
    if (halted) *halted = (c2 & STM8_DM_CSR2_STALL) != 0u;
    return 0;
}

int dj_stm8_debug_halt(void)
{
    return set_csr2_bits(STM8_DM_CSR2_STALL, 0u);
}

int dj_stm8_debug_run(void)
{
    return set_csr2_bits(0u, STM8_DM_CSR2_STALL | STM8_DM_CSR2_FLUSH);
}

int dj_stm8_debug_step(void)
{
    uint8_t csr1 = 0u;
    int r = dj_swim_read_mem(STM8_DM_CSR1, &csr1, 1u);
    if (r) return r;
    csr1 |= STM8_DM_CSR1_STE;
    r = dj_swim_write_mem(STM8_DM_CSR1, &csr1, 1u);
    if (r) return r;
    r = dj_stm8_debug_run();
    if (r) return r;
    /* On hardware, the step flag is set by DM after one instruction.  The mock
     * model makes this deterministic for API/CI tests. */
    if (mock_enabled) {
        mock_mem[STM8_DM_CSR1] |= STM8_DM_CSR1_STF;
        mock_mem[STM8_DM_CSR2] |= STM8_DM_CSR2_STALL;
    }
    for (unsigned i = 0; i < 1000u; ++i) {
        bool halted = false;
        r = dj_stm8_debug_status(NULL, NULL, &halted);
        if (r) return r;
        if (halted) return 0;
        const struct dj_hw_ops *h = dj_hw();
        if (h && h->delay_us) h->delay_us(50u);
    }
    return -ETIMEDOUT;
}

int dj_stm8_debug_reset(bool halt_after_reset)
{
    int r = dj_swim_system_reset();
    if (r) return r;
    if (halt_after_reset) return dj_stm8_debug_halt();
    return 0;
}

int dj_stm8_debug_read_regs(uint8_t *out, size_t *len)
{
    if (!out || !len) return -EINVAL;
    if (*len < STM8_CPU_REG_COUNT) return -ENOSPC;
    int r = dj_swim_read_mem(STM8_CPU_REG_A, out, STM8_CPU_REG_COUNT);
    if (!r) *len = STM8_CPU_REG_COUNT;
    return r;
}

int dj_stm8_debug_write_regs(const uint8_t *in, size_t len)
{
    if (!in || len != STM8_CPU_REG_COUNT) return -EINVAL;
    bool halted = false;
    int r = dj_stm8_debug_status(NULL, NULL, &halted);
    if (r) return r;
    if (!halted) return -EBUSY;
    r = dj_swim_write_mem(STM8_CPU_REG_A, in, STM8_CPU_REG_COUNT);
    if (r) return r;
    return set_csr2_bits(STM8_DM_CSR2_FLUSH, 0u);
}

int dj_stm8_debug_set_breakpoint(uint32_t address, uint8_t type, uint8_t slot)
{
    if (slot > 1u || address > 0xFFFFFFu) return -EINVAL;
    if (type != 0u) return -ENOTSUP; /* Phase-D implements instruction-fetch BP. */
    uint8_t a[3] = {(uint8_t)(address >> 16), (uint8_t)(address >> 8), (uint8_t)address};
    uint32_t base = slot == 0u ? STM8_DM_BK1E : STM8_DM_BK2E;
    int r = dj_swim_write_mem(base, a, sizeof(a));
    if (r) return r;
    uint8_t cr1 = 0u;
    r = dj_swim_read_mem(STM8_DM_CR1, &cr1, 1u);
    if (r) return r;
    if (slot == 0u) cr1 = (uint8_t)((cr1 & 0xc7u) | STM8_DM_CR1_BC_IFETCH_OR);
    else cr1 = (uint8_t)((cr1 & 0xc7u) | STM8_DM_CR1_BC_IFETCH_OR);
    return dj_swim_write_mem(STM8_DM_CR1, &cr1, 1u);
}

int dj_stm8_debug_clear_breakpoint(uint8_t slot)
{
    if (slot > 1u) return -EINVAL;
    uint8_t ff[3] = {0xffu, 0xffu, 0xffu};
    uint32_t base = slot == 0u ? STM8_DM_BK1E : STM8_DM_BK2E;
    int r = dj_swim_write_mem(base, ff, sizeof(ff));
    if (r) return r;
    /* Disable both hardware breakpoints only when both slots are cleared. */
    uint8_t b1[3], b2[3];
    r = dj_swim_read_mem(STM8_DM_BK1E, b1, sizeof(b1)); if (r) return r;
    r = dj_swim_read_mem(STM8_DM_BK2E, b2, sizeof(b2)); if (r) return r;
    if (!memcmp(b1, ff, 3u) && !memcmp(b2, ff, 3u)) {
        uint8_t cr1 = 0u;
        r = dj_swim_read_mem(STM8_DM_CR1, &cr1, 1u);
        if (r) return r;
        cr1 &= 0xc7u;
        r = dj_swim_write_mem(STM8_DM_CR1, &cr1, 1u);
    }
    return r;
}
