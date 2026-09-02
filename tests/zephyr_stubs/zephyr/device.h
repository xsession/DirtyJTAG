#ifndef ZSTUB_DEVICE_H
#define ZSTUB_DEVICE_H
struct device { int dummy; };
extern struct device zstub_device;
#define DT_NODELABEL(x) 0
#define DEVICE_DT_GET(x) (&zstub_device)
static inline int device_is_ready(const struct device *d){(void)d;return 1;}
#endif
