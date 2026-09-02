#ifndef ZSTUB_DEVICETREE_H
#define ZSTUB_DEVICETREE_H

#define DT_ALIAS(name) name
#define DT_NODE_HAS_STATUS(node, status) 1
#define DT_NODE_HAS_PROP(node, prop) 1
#define BUILD_ASSERT(cond, msg) _Static_assert(cond, msg)

#define DT_PROP(node, prop) DT_PROP_RESOLVE(prop)
#define DT_PROP_RESOLVE(prop) DT_PROP_##prop

#define DT_PROP_vtarget_adc_channel 0
#define DT_PROP_vpp_adc_channel 1
#define DT_PROP_itarget_adc_channel 2

#endif
