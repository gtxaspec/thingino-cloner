#include "cloner/thingino.h"
#include "cloner/platform_profile.h"

/* Defined in per-family files */
extern const platform_profile_t platform_t10;
extern const platform_profile_t platform_t20;
extern const platform_profile_t platform_t21;
extern const platform_profile_t platform_t30;
extern const platform_profile_t platform_t31;
extern const platform_profile_t platform_t31a;
extern const platform_profile_t platform_t31zx;
extern const platform_profile_t platform_t41;
extern const platform_profile_t platform_t40;
extern const platform_profile_t platform_a1;

const platform_profile_t *platform_get_profile(processor_variant_t variant) {
    switch (variant) {
    case VARIANT_T10:
        return &platform_t10;

    case VARIANT_T20:
        return &platform_t20;

    case VARIANT_T21:
        return &platform_t21;

    case VARIANT_T31ZX:
        return &platform_t31zx;

    case VARIANT_T41:
        return &platform_t41;

    case VARIANT_A1:
        return &platform_a1;

    case VARIANT_T30:
        return &platform_t30;

    case VARIANT_T31A:
        return &platform_t31a;

    case VARIANT_T40:
        return &platform_t40;

    /* T31 family + anything else defaults to T31 profile */
    case VARIANT_T23:
    case VARIANT_T32:
    case VARIANT_T31:
    case VARIANT_T31X:
    default:
        return &platform_t31;
    }
}
