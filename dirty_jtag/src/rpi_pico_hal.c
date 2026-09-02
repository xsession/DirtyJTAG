/* SPDX-License-Identifier: MIT */
#include "djprog/hw.h"
#include "djprog/swim_phy.h"
#include <zephyr/device.h>
#include <zephyr/drivers/adc.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>
#include <errno.h>

/*
 * Raspberry Pi Pico universal-programmer front end.
 *
 * Target-side translators are SN74LVC1T45 devices. They have DIR but no OE,
 * so safe isolation is achieved by forcing B->A and making the Pico A pin an
 * input. RESET/MCLR is deliberately separate: GP6 drives an open-drain sink.
 */
static const struct device *gpio = DEVICE_DT_GET(DT_NODELABEL(gpio0));
static const struct device *adc  = DEVICE_DT_GET(DT_NODELABEL(adc));
static struct dj_pinmap map = {{2, 3, 4, 5, 6, 7}};
static const uint8_t dir_gpio[DJ_PIN_COUNT] = {8, 9, 10, 11, 0, 12};

#define PIN_TGT_REG_EN       13 /* TPS630702 EN */
#define PIN_TGT_VSEL         14 /* low=3.3 V, high=~5.0 V */
#define PIN_TGT_SW_EN        15 /* TPS2553 EN */
#define PIN_VPP_BOOST        16 /* TPS61040 EN */
#define PIN_VPP_APPLY        17 /* NPN -> PMOS high-side gate */
#define PIN_TGT_FAULT_N      18 /* TPS2553 FAULT#, active low */
#define PIN_DATA0_ISO_EN     19 /* TMUX4827: normal DATA0 path */
#define PIN_HV_DATA0_APPLY   20 /* Q4/Q5: 11.8V activation pulse */

#define ADC_CH_VTARGET       0  /* GP26, 100k/100k => x2 */
#define ADC_CH_VPP           1  /* GP27, 330k/100k => x4.3 */
#define ADC_CH_ITARGET       2  /* GP28, INA180A2, 0.1R shunt */

static bool reset_asserted;
static bool boost_on;
static bool vpp_on;
static bool hv_data0_on;
static bool signal_latch[DJ_PIN_COUNT];
static enum dj_power_mode power_mode;

static int adc_setup_channel(uint8_t ch)
{
    struct adc_channel_cfg c = {
        .gain = ADC_GAIN_1,
        .reference = ADC_REF_INTERNAL,
        .acquisition_time = ADC_ACQ_TIME_DEFAULT,
        .channel_id = ch,
    };
    return adc_channel_setup(adc, &c);
}

static int init(void)
{
    if (!device_is_ready(gpio) || !device_is_ready(adc)) return -ENODEV;

    /* Target signal A-side pins start high-Z. */
    for (int i = 0; i < DJ_PIN_COUNT; ++i) {
        if (i == DJ_PIN_RESET) continue;
        int r = gpio_pin_configure(gpio, map.gpio[i], GPIO_INPUT);
        if (r) return r;
    }

    /* RESET sink gate low means released. */
    int r = gpio_pin_configure(gpio, map.gpio[DJ_PIN_RESET], GPIO_OUTPUT_INACTIVE);
    if (r) return r;

    /* All translators face target->Pico in idle state. */
    for (int i = 0; i < DJ_PIN_COUNT; ++i) {
        if (i == DJ_PIN_RESET) continue;
        r = gpio_pin_configure(gpio, dir_gpio[i], GPIO_OUTPUT_INACTIVE);
        if (r) return r;
    }

    const uint8_t outputs[] = {
        PIN_TGT_REG_EN, PIN_TGT_VSEL, PIN_TGT_SW_EN,
        PIN_VPP_BOOST, PIN_VPP_APPLY, PIN_DATA0_ISO_EN, PIN_HV_DATA0_APPLY,
    };
    for (size_t i = 0; i < sizeof(outputs) / sizeof(outputs[0]); ++i) {
        r = gpio_pin_configure(gpio, outputs[i], GPIO_OUTPUT_INACTIVE);
        if (r) return r;
    }
    r = gpio_pin_configure(gpio, PIN_TGT_FAULT_N, GPIO_INPUT | GPIO_PULL_UP);
    if (r) return r;

    for (uint8_t ch = ADC_CH_VTARGET; ch <= ADC_CH_ITARGET; ++ch) {
        r = adc_setup_channel(ch);
        if (r) return r;
    }

    reset_asserted = false;
    boost_on = false;
    vpp_on = false;
    hv_data0_on = false;
    gpio_pin_set(gpio, PIN_DATA0_ISO_EN, 0);
    power_mode = DJ_PWR_OFF;
    return 0;
}

static int configure(const struct dj_pinmap *m)
{
    if (!m) return -EINVAL;
    for (int i = 0; i < DJ_PIN_COUNT; ++i) if (m->gpio[i] > 28) return -ERANGE;
    map = *m;
    return 0;
}

static int dir(enum dj_pin_role role, enum dj_dir d)
{
    if (role >= DJ_PIN_COUNT) return -EINVAL;
    if (role == DJ_PIN_RESET) return 0;
    if (role == DJ_PIN_DATA0 && !hv_data0_on) gpio_pin_set(gpio, PIN_DATA0_ISO_EN, 1);

    uint8_t p = map.gpio[role];
    if (d == DJ_DIR_INPUT || d == DJ_DIR_RELEASE) {
        /* Remove target drive first, then make Pico input. */
        int r = gpio_pin_set(gpio, dir_gpio[role], 0);
        if (r) return r;
        return gpio_pin_configure(gpio, p, GPIO_INPUT);
    }

    if (d == DJ_DIR_OD_LOW) {
        /* Open-drain low emulation through the unidirectional translator: drive
         * a low only for the active pulse, then callers release with INPUT.
         * External/front-end pull-up returns the SWIM/UPDI line high. */
        signal_latch[role] = false;
        int r = gpio_pin_configure(gpio, p, GPIO_OUTPUT_INACTIVE);
        if (r) return r;
        return gpio_pin_set(gpio, dir_gpio[role], 1);
    }

    /* Preload A-side value before enabling A->B to avoid a direction glitch. */
    int r = gpio_pin_configure(gpio, p,
        signal_latch[role] ? GPIO_OUTPUT_ACTIVE : GPIO_OUTPUT_INACTIVE);
    if (r) return r;
    return gpio_pin_set(gpio, dir_gpio[role], 1);
}

static int wr(enum dj_pin_role role, bool value)
{
    if (role >= DJ_PIN_COUNT) return -EINVAL;
    if (role == DJ_PIN_RESET) {
        /* API true=released, false=asserted; QRESET gate is active high. */
        reset_asserted = !value;
        return gpio_pin_set(gpio, map.gpio[role], reset_asserted ? 1 : 0);
    }
    signal_latch[role] = value;
    return gpio_pin_set(gpio, map.gpio[role], value ? 1 : 0);
}

static int rd(enum dj_pin_role role, bool *value)
{
    if (role >= DJ_PIN_COUNT || !value) return -EINVAL;
    if (role == DJ_PIN_RESET) {
        *value = !reset_asserted;
        return 0;
    }
    int x = gpio_pin_get(gpio, map.gpio[role]);
    if (x < 0) return x;
    *value = x != 0;
    return 0;
}

static int clkbits(enum dj_pin_role c, enum dj_pin_role o, enum dj_pin_role in,
                   const uint8_t *tx, uint8_t *rx, size_t bits, bool lsb,
                   uint32_t hz)
{
    if (!hz) return -EINVAL;
    uint32_t half = 500000u / hz;
    if (!half) half = 1;
    if (rx) for (size_t i = 0; i < (bits + 7u) / 8u; ++i) rx[i] = 0;

    int r = dir(c, DJ_DIR_OUTPUT); if (r) return r;
    if (tx) { r = dir(o, DJ_DIR_OUTPUT); if (r) return r; }
    if (rx) { r = dir(in, DJ_DIR_INPUT); if (r) return r; }

    for (size_t i = 0; i < bits; ++i) {
        size_t bi = i / 8u;
        unsigned bj = (unsigned)(i & 7u), idx = lsb ? bj : 7u - bj;
        if ((r = wr(c, false))) return r;
        if (tx && (r = wr(o, ((tx[bi] >> idx) & 1u) != 0))) return r;
        k_busy_wait(half);
        if ((r = wr(c, true))) return r;
        if (rx) {
            bool b = false;
            if ((r = rd(in, &b))) return r;
            if (b) rx[bi] |= (uint8_t)(1u << idx);
        }
        k_busy_wait(half);
    }
    return wr(c, false);
}

static uint32_t adc_reference_mv(void)
{
    uint16_t mv = adc_ref_internal(adc);
    return mv ? mv : 3300u;
}

static int adc_pin_mv(uint8_t ch, uint32_t *out)
{
    if (!out) return -EINVAL;
    int16_t raw = 0;
    struct adc_sequence s = {
        .channels = BIT(ch),
        .buffer = &raw,
        .buffer_size = sizeof(raw),
        .resolution = 12,
    };
    int r = adc_read(adc, &s);
    if (r) return r;
    if (raw < 0) raw = 0;
    uint64_t mv = (uint64_t)(uint16_t)raw * adc_reference_mv();
    mv /= 4095u;
    *out = (uint32_t)mv;
    return 0;
}

static int measure(struct dj_measurement *m)
{
    if (!m) return -EINVAL;
    uint32_t mv = 0;
    int r = adc_pin_mv(ADC_CH_VTARGET, &mv);
    if (r) return r;
    m->vtarget_mv = mv * 2u;

    r = adc_pin_mv(ADC_CH_VPP, &mv);
    if (r) return r;
    m->vpp_mv = (mv * 43u + 5u) / 10u;

    r = adc_pin_mv(ADC_CH_ITARGET, &mv);
    if (r) return r;
    /* 0.1 ohm * INA180A2 gain 50 => 5 mV/mA. */
    m->itarget_ma = (mv + 2u) / 5u;

    int fault_n = gpio_pin_get(gpio, PIN_TGT_FAULT_N);
    if (fault_n < 0) return fault_n;
    m->power_fault = fault_n == 0;
    return 0;
}

static void local_power_off(void)
{
    gpio_pin_set(gpio, PIN_TGT_SW_EN, 0);
    k_sleep(K_MSEC(1));
    gpio_pin_set(gpio, PIN_TGT_REG_EN, 0);
}

static int power(enum dj_power_mode m)
{
    if (m < DJ_PWR_OFF || m > DJ_PWR_5V) return -EINVAL;
    if (vpp_on) return -EBUSY;

    local_power_off();
    power_mode = DJ_PWR_OFF;
    k_sleep(K_MSEC(2));

    if (m == DJ_PWR_OFF || m == DJ_PWR_EXTERNAL) {
        power_mode = m;
        return 0;
    }

    gpio_pin_set(gpio, PIN_TGT_VSEL, m == DJ_PWR_5V ? 1 : 0);
    gpio_pin_set(gpio, PIN_TGT_REG_EN, 1);
    k_sleep(K_MSEC(3));
    gpio_pin_set(gpio, PIN_TGT_SW_EN, 1);
    k_sleep(K_MSEC(3));

    struct dj_measurement x = {0};
    int r = measure(&x);
    uint32_t lo = m == DJ_PWR_3V3 ? 3000u : 4500u;
    uint32_t hi = m == DJ_PWR_3V3 ? 3650u : 5300u;
    if (r || x.power_fault || x.vtarget_mv < lo || x.vtarget_mv > hi) {
        local_power_off();
        return r ? r : (x.power_fault ? -EIO : -ERANGE);
    }
    power_mode = m;
    return 0;
}

static int vboost(bool on)
{
    if (!on && hv_data0_on) {
        gpio_pin_set(gpio, PIN_HV_DATA0_APPLY, 0);
        hv_data0_on = false;
    }
    if (!on && vpp_on) {
        gpio_pin_set(gpio, PIN_VPP_APPLY, 0);
        vpp_on = false;
    }
    boost_on = on;
    gpio_pin_set(gpio, PIN_VPP_BOOST, on ? 1 : 0);
    if (on) k_sleep(K_MSEC(5));
    return 0;
}

static int vapply(bool on)
{
    if (on) {
        if (!boost_on) return -EPERM;
        if (reset_asserted || hv_data0_on) return -EBUSY;
        struct dj_measurement m = {0};
        int r = measure(&m);
        if (r) return r;
        if (m.power_fault) return -EIO;
        if (m.vpp_mv < 10500u || m.vpp_mv > 13500u) return -ERANGE;
    }
    vpp_on = on;
    return gpio_pin_set(gpio, PIN_VPP_APPLY, on ? 1 : 0);
}

static int hvdata0(bool on)
{
    if (on) {
        if (!boost_on) return -EPERM;
        if (vpp_on) return -EBUSY;
        struct dj_measurement m = {0};
        int r = measure(&m);
        if (r) return r;
        if (m.power_fault) return -EIO;
        /* Hardware is intentionally designed for an ~11.8 V shared VPP rail. */
        if (m.vpp_mv < 10500u || m.vpp_mv > 12000u) return -ERANGE;
        /* Disconnect 5.5 V translator before applying HV to target DATA0. */
        gpio_pin_set(gpio, PIN_DATA0_ISO_EN, 0);
        gpio_pin_set(gpio, dir_gpio[DJ_PIN_DATA0], 0);
        gpio_pin_configure(gpio, map.gpio[DJ_PIN_DATA0], GPIO_INPUT);
        k_busy_wait(10);
    }
    hv_data0_on = on;
    int r = gpio_pin_set(gpio, PIN_HV_DATA0_APPLY, on ? 1 : 0);
    if (!on) k_busy_wait(10);
    return r;
}

static void delay(uint32_t us) { k_busy_wait(us); }

static const struct dj_hw_ops ops = {
    init, configure, dir, wr, rd, clkbits, power, vboost, vapply, hvdata0, measure, delay
};

int dj_rpi_pico_hal_init(void)
{
    int r = ops.init();
    if (r) return r;
    dj_hw_bind(&ops);
    int r2 = dj_hw_set_pinmap(&map);
#if defined(CONFIG_DJPROG_SWIM_RP2040_PIO)
    if (!r2) {
        (void)dj_swim_rp2040_pio_try_bind(map.gpio[DJ_PIN_DATA0], CONFIG_DJPROG_SWIM_PIO_DEFAULT_HZ);
    }
#endif
    return r2;
}
