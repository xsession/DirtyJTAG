#ifndef ZSTUB_GPIO_H
#define ZSTUB_GPIO_H
#include <stdint.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#define GPIO_INPUT 1
#define GPIO_OUTPUT_INACTIVE 2
#define GPIO_OUTPUT_ACTIVE 4
#define GPIO_PULL_UP 8
struct gpio_dt_spec { const struct device *port; uint8_t pin; int dt_flags; };
#define GPIO_DT_SPEC_GET(node, prop) { DEVICE_DT_GET(gpio0), GPIO_STUB_PIN(prop), 0 }
#define GPIO_STUB_PIN(prop) GPIO_STUB_PIN_RESOLVE(prop)
#define GPIO_STUB_PIN_RESOLVE(prop) GPIO_STUB_PIN_##prop
#define GPIO_STUB_PIN_clk_gpios 2
#define GPIO_STUB_PIN_data0_gpios 3
#define GPIO_STUB_PIN_data1_gpios 4
#define GPIO_STUB_PIN_data2_gpios 5
#define GPIO_STUB_PIN_reset_gpios 6
#define GPIO_STUB_PIN_aux_gpios 7
#define GPIO_STUB_PIN_clk_dir_gpios 8
#define GPIO_STUB_PIN_data0_dir_gpios 9
#define GPIO_STUB_PIN_data1_dir_gpios 10
#define GPIO_STUB_PIN_data2_dir_gpios 11
#define GPIO_STUB_PIN_aux_dir_gpios 12
#define GPIO_STUB_PIN_tgt_reg_en_gpios 13
#define GPIO_STUB_PIN_tgt_vsel_gpios 14
#define GPIO_STUB_PIN_tgt_sw_en_gpios 15
#define GPIO_STUB_PIN_vpp_boost_en_gpios 16
#define GPIO_STUB_PIN_vpp_mclr_apply_gpios 17
#define GPIO_STUB_PIN_tgt_fault_n_gpios 18
#define GPIO_STUB_PIN_data0_iso_en_gpios 19
#define GPIO_STUB_PIN_hv_data0_apply_gpios 20
static inline int gpio_pin_configure(const struct device*d,uint8_t p,int f){(void)d;(void)p;(void)f;return 0;}
static inline int gpio_pin_set(const struct device*d,uint8_t p,int v){(void)d;(void)p;(void)v;return 0;}
static inline int gpio_pin_get(const struct device*d,uint8_t p){(void)d;(void)p;return 1;}
static inline int gpio_is_ready_dt(const struct gpio_dt_spec*s){(void)s;return 1;}
static inline int gpio_pin_configure_dt(const struct gpio_dt_spec*s,int f){return gpio_pin_configure(s->port,s->pin,f);}
static inline int gpio_pin_set_dt(const struct gpio_dt_spec*s,int v){return gpio_pin_set(s->port,s->pin,v);}
static inline int gpio_pin_get_dt(const struct gpio_dt_spec*s){return gpio_pin_get(s->port,s->pin);}
#endif
