#ifndef ZSTUB_ADC_H
#define ZSTUB_ADC_H
#include <stddef.h>
#include <stdint.h>
#include <zephyr/device.h>
#define ADC_GAIN_1 1
#define ADC_REF_INTERNAL 1
#define ADC_ACQ_TIME_DEFAULT 0
#ifndef BIT
#define BIT(n) (1u << (n))
#endif
struct adc_channel_cfg {
	int gain;
	int reference;
	int acquisition_time;
	uint8_t channel_id;
};
struct adc_sequence {
	uint32_t channels;
	void *buffer;
	size_t buffer_size;
	uint8_t resolution;
};
static inline int adc_channel_setup(const struct device *d, const struct adc_channel_cfg *c) {
	(void)d;
	(void)c;
	return 0;
}
static inline int adc_read(const struct device *d, const struct adc_sequence *s) {
	(void)d;
	(void)s;
	return 0;
}
static inline uint16_t adc_ref_internal(const struct device *d) {
	(void)d;
	return 3300;
}
#endif
