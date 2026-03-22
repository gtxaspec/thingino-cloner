/**
 * Cloner Shared Types
 *
 * Error codes, device structs, variant enums used by both
 * the library and its consumers (CLI, daemon).
 */

#ifndef CLONER_TYPES_H
#define CLONER_TYPES_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/* Error codes */
typedef enum {
    CLONER_OK = 0,
    CLONER_ERR_PARAM,     /* invalid parameter */
    CLONER_ERR_MEMORY,    /* allocation failed */
    CLONER_ERR_USB_INIT,  /* USB init failed */
    CLONER_ERR_DEVICE,    /* device not found or open failed */
    CLONER_ERR_TRANSFER,  /* USB transfer failed */
    CLONER_ERR_FILE,      /* file I/O error */
    CLONER_ERR_PROTOCOL,  /* protocol/format error */
    CLONER_ERR_TIMEOUT,   /* operation timed out */
    CLONER_ERR_CANCELLED, /* operation cancelled */
} cloner_error_t;

/* Device stage */
typedef enum {
    CLONER_STAGE_BOOTROM = 0,
    CLONER_STAGE_FIRMWARE = 1,
} cloner_stage_t;

/* Processor variant - must match processor_variant_t ordering in thingino.h */
typedef enum {
    CLONER_VARIANT_T10 = 0,
    CLONER_VARIANT_T20,
    CLONER_VARIANT_T21,
    CLONER_VARIANT_T23,
    CLONER_VARIANT_T30,
    CLONER_VARIANT_T31,
    CLONER_VARIANT_T31X,
    CLONER_VARIANT_T31ZX,
    CLONER_VARIANT_T31A,
    CLONER_VARIANT_A1,
    CLONER_VARIANT_T40,
    CLONER_VARIANT_T41,
    CLONER_VARIANT_T32,
    CLONER_VARIANT_X1000,
    CLONER_VARIANT_X1600,
    CLONER_VARIANT_X1700,
    CLONER_VARIANT_X2000,
    CLONER_VARIANT_X2100,
    CLONER_VARIANT_X2600,
    CLONER_VARIANT_T31AL,
    CLONER_VARIANT_T40XP,
} cloner_variant_t;

/* Device info (returned by discovery) */
typedef struct {
    uint8_t bus;
    uint8_t address;
    uint16_t vendor_id;
    uint16_t product_id;
    cloner_stage_t stage;
    cloner_variant_t variant;
} cloner_device_info_t;

/* Device list */
typedef struct {
    cloner_device_info_t *devices;
    int count;
} cloner_device_list_t;

/* Progress callback */
typedef void (*cloner_progress_cb)(int percent, const char *stage, const char *message, void *user_data);

/* Opaque device handle */
typedef struct cloner_device cloner_device_t;

#endif /* CLONER_TYPES_H */
