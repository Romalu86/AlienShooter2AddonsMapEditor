#ifndef AS2_OGG_CONFIG_TYPES_H
#define AS2_OGG_CONFIG_TYPES_H
/* Fixed-width configuration for the portable decoder validation build.
   Win32 selects its original public ABI in os_types.h. */
#include <stdint.h>
typedef int16_t ogg_int16_t;
typedef int32_t ogg_int32_t;
typedef uint32_t ogg_uint32_t;
typedef int64_t ogg_int64_t;
#endif
