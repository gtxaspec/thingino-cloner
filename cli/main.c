#include "thingino.h"
#include "cloner/core.h"
#include "cloner/protocol.h"
#include "remote.h"
#include "ddr_config_database.h"
#include <unistd.h>

// ============================================================================
// GLOBAL DEBUG FLAG
// ============================================================================

bool g_debug_enabled = false;

// ============================================================================
// MAIN CLI INTERFACE
// ============================================================================

typedef struct {
    bool verbose;
    bool debug;
    bool list_devices;
    bool bootstrap;
    bool read_firmware;
    bool write_firmware;
    int device_index;
    char *config_file;
    char *spl_file;
    char *uboot_file;
    char *output_file;
    char *input_file;
    bool force_erase;
    bool reboot_after;
    bool skip_ddr;
    uint32_t chunk_size; // Flash write chunk size (default: 131072)
    char *force_cpu;     // Force specific CPU variant (e.g., "a1", "t31x", "t31zx")
    char *firmware_dir;  // Firmware directory (default: ./firmwares)
    char *remote_host;   // Remote daemon host (NULL = local mode)
    int remote_port;     // Remote daemon port (default: 5050)
    char *auth_token;    // Auth token for remote daemon
    char *flash_chip;    // Override flash chip name (default: auto-detect)
} cli_options_t;

void print_usage(const char *program_name) {
    printf("Thingino Cloner - USB Device Cloner for Ingenic Processors\n");
    printf("Usage: %s [options]\n\n", program_name);
    printf("Options:\n");
    printf("  -h, --help                Show this help message\n");
    printf("  -v, --verbose             Enable verbose logging\n");
    printf("  -d, --debug               Enable debug output\n");
    printf("  -l, --list                List connected devices\n");
    printf("  -i, --index <num>         Device index to operate on (default: 0)\n");
    printf("  -b, --bootstrap           Bootstrap device to firmware stage\n");
    printf("  -r, --read <file>         Read firmware from device to file\n");
    printf("  -w, --write <file>        Write firmware from file to device\n");
    printf("      --erase               Erase flash before writing (when supported)\n");
    printf("      --reboot              Reboot device after flash write completes\n");
    printf("      --chunk-size <bytes>  Write chunk size (default: 128KB, read: 1MB)\n");
    printf("      --cpu <variant>       CPU variant (a1, t31, t40, t41, etc.)\n");
    printf("      --config <file>       Custom DDR configuration file\n");
    printf("      --spl <file>          Custom SPL file\n");
    printf("      --uboot <file>        Custom U-Boot file\n");
    printf("      --firmware-dir <dir>  Firmware directory (default: ./firmwares)\n");
#ifndef _WIN32
    printf("      --host <addr>         Connect to remote cloner-remote daemon\n");
    printf("      --port <port>         Remote daemon port (default: 5050)\n");
    printf("      --token <secret>      Auth token for remote daemon\n");
#endif
    printf("      --skip-ddr            Skip DDR configuration during bootstrap\n");
    printf("      --flash-chip <name>   Override flash chip (auto-detect from JEDEC ID)\n");
    printf("      --list-cpus           List supported CPU targets for --cpu\n");
    printf("\nExamples:\n");
    printf("  thingino-cloner -l                                 # List devices\n");
    printf("  thingino-cloner -i 0 -b --cpu t31                  # Bootstrap device 0 as T31\n");
    printf("  thingino-cloner -i 0 -b -w firmware.bin --cpu a1   # Bootstrap + write to A1\n");
    printf("  thingino-cloner --list-cpus                        # Show all supported CPUs\n");
}

static void print_supported_cpus(void) {
    printf("Supported CPU targets (use with --cpu <name>):\n\n");
    printf("  %-10s %-10s %s\n", "--cpu", "Arch", "Default DDR Chip");
    printf("  %-10s %-10s %s\n", "-----", "----", "----------------");
    size_t count;
    const processor_config_t *procs = processor_config_list(&count);
    for (size_t i = 0; i < count; i++) {
        const char *arch = procs[i].is_xburst2 ? "xburst2" : "xburst1";
        const ddr_chip_config_t *chip = ddr_chip_config_get_default(procs[i].name);
        printf("  %-10s %-10s %s\n", procs[i].name, arch, chip ? chip->name : "(none)");
    }
}

thingino_error_t parse_arguments(int argc, char *argv[], cli_options_t *options) {
    // Initialize options
    memset(options, 0, sizeof(cli_options_t));
    options->device_index = 0;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            print_usage(argv[0]);
            exit(0);
        } else if (strcmp(argv[i], "-v") == 0 || strcmp(argv[i], "--verbose") == 0) {
            options->verbose = true;
        } else if (strcmp(argv[i], "-d") == 0 || strcmp(argv[i], "--debug") == 0) {
            options->debug = true;
        } else if (strcmp(argv[i], "-l") == 0 || strcmp(argv[i], "--list") == 0) {
            options->list_devices = true;
        } else if (strcmp(argv[i], "-b") == 0 || strcmp(argv[i], "--bootstrap") == 0) {
            options->bootstrap = true;
        } else if (strcmp(argv[i], "-r") == 0 || strcmp(argv[i], "--read") == 0) {
            if (i + 1 >= argc) {
                printf("Error: %s requires a filename\n", argv[i]);
                return THINGINO_ERROR_INVALID_PARAMETER;
            }
            options->read_firmware = true;
            options->output_file = argv[++i];
        } else if (strcmp(argv[i], "-w") == 0 || strcmp(argv[i], "--write") == 0) {
            if (i + 1 >= argc) {
                printf("Error: %s requires a filename\n", argv[i]);
                return THINGINO_ERROR_INVALID_PARAMETER;
            }
            options->write_firmware = true;
            options->input_file = argv[++i];
        } else if (strcmp(argv[i], "--config") == 0) {
            if (i + 1 >= argc) {
                printf("Error: %s requires a filename\n", argv[i]);
                return THINGINO_ERROR_INVALID_PARAMETER;
            }
            options->config_file = argv[++i];
        } else if (strcmp(argv[i], "--spl") == 0) {
            if (i + 1 >= argc) {
                printf("Error: %s requires a filename\n", argv[i]);
                return THINGINO_ERROR_INVALID_PARAMETER;
            }
            options->spl_file = argv[++i];
        } else if (strcmp(argv[i], "--uboot") == 0) {
            if (i + 1 >= argc) {
                printf("Error: %s requires a filename\n", argv[i]);
                return THINGINO_ERROR_INVALID_PARAMETER;
            }
            options->uboot_file = argv[++i];
        } else if (strcmp(argv[i], "--firmware-dir") == 0) {
            if (i + 1 >= argc) {
                printf("Error: %s requires a directory path\n", argv[i]);
                return THINGINO_ERROR_INVALID_PARAMETER;
            }
            options->firmware_dir = argv[++i];
        } else if (strcmp(argv[i], "--host") == 0) {
            if (i + 1 >= argc) {
                printf("Error: %s requires a hostname\n", argv[i]);
                return THINGINO_ERROR_INVALID_PARAMETER;
            }
            options->remote_host = argv[++i];
        } else if (strcmp(argv[i], "--port") == 0) {
            if (i + 1 >= argc) {
                printf("Error: %s requires a port number\n", argv[i]);
                return THINGINO_ERROR_INVALID_PARAMETER;
            }
            options->remote_port = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--token") == 0) {
            if (i + 1 >= argc) {
                printf("Error: %s requires a token string\n", argv[i]);
                return THINGINO_ERROR_INVALID_PARAMETER;
            }
            options->auth_token = argv[++i];
        } else if (strcmp(argv[i], "--flash-chip") == 0) {
            if (i + 1 >= argc) {
                printf("Error: %s requires a chip name\n", argv[i]);
                return THINGINO_ERROR_INVALID_PARAMETER;
            }
            options->flash_chip = argv[++i];
        } else if (strcmp(argv[i], "--list-cpus") == 0) {
            print_supported_cpus();
            exit(0);
        } else if (strcmp(argv[i], "--skip-ddr") == 0) {
            options->skip_ddr = true;
        } else if (strcmp(argv[i], "--erase") == 0) {
            options->force_erase = true;
        } else if (strcmp(argv[i], "--reboot") == 0) {
            options->reboot_after = true;
        } else if (strcmp(argv[i], "--chunk-size") == 0) {
            if (i + 1 >= argc) {
                printf("Error: %s requires a size in bytes\n", argv[i]);
                return THINGINO_ERROR_INVALID_PARAMETER;
            }
            {
                char *endptr;
                unsigned long val = strtoul(argv[++i], &endptr, 10);
                if (*endptr != '\0' || val == 0 || val > 0xFFFFFFFFUL) {
                    printf("Error: invalid chunk-size value\n");
                    return THINGINO_ERROR_INVALID_PARAMETER;
                }
                options->chunk_size = (uint32_t)val;
            }
        } else if (strcmp(argv[i], "--cpu") == 0) {
            if (i + 1 >= argc) {
                printf("Error: %s requires a CPU variant (e.g., a1, t31x, t31zx)\n", argv[i]);
                return THINGINO_ERROR_INVALID_PARAMETER;
            }
            options->force_cpu = argv[++i];
        } else if (strcmp(argv[i], "-i") == 0 || strcmp(argv[i], "--index") == 0) {
            if (i + 1 >= argc) {
                printf("Error: %s requires a device index\n", argv[i]);
                return THINGINO_ERROR_INVALID_PARAMETER;
            }
            options->device_index = atoi(argv[++i]);
            if (options->device_index < 0) {
                printf("Error: device index must be >= 0\n");
                return THINGINO_ERROR_INVALID_PARAMETER;
            }
        } else {
            printf("Error: Unknown option %s\n", argv[i]);
            print_usage(argv[0]);
            return THINGINO_ERROR_INVALID_PARAMETER;
        }
    }

    return THINGINO_SUCCESS;
}

thingino_error_t list_devices(usb_manager_t *manager) {
    printf("Scanning for Ingenic devices...\n\n");

    device_info_t *devices;
    int device_count;
    thingino_error_t result = usb_manager_find_devices(manager, &devices, &device_count);
    if (result != THINGINO_SUCCESS) {
        printf("Failed to list devices: %s\n", thingino_error_to_string(result));
        return result;
    }

    if (device_count == 0) {
        printf("No Ingenic devices found\n");
        return THINGINO_SUCCESS;
    }

    printf("Found %d device(s):\n", device_count);
    printf("Index | Bus | Addr | Vendor  | Product | Stage    | Variant\n");
    printf("-----|-----|------|---------|----------|--------\n");

    for (int i = 0; i < device_count; i++) {
        device_info_t *dev = &devices[i];
        printf("%5d | %3d | %4d | 0x%04X  | 0x%04X  | %-8s | %s\n", i, dev->bus, dev->address, dev->vendor,
               dev->product, device_stage_to_string(dev->stage), processor_variant_to_string(dev->variant));
    }

    printf("\n");
    free(devices);
    return THINGINO_SUCCESS;
}

/* bootstrap_device_by_index, read_firmware_from_device, write_firmware_from_file
 * moved to libcloner/src/operations.c as cloner_op_bootstrap(),
 * cloner_op_read_firmware(), cloner_op_write_firmware(). */

int main(int argc, char *argv[]) {
    fprintf(stderr, "thingino-cloner %s (%s)\n", VERSION, GIT_HASH);

    cli_options_t options;
    thingino_error_t result = parse_arguments(argc, argv, &options);
    if (result != THINGINO_SUCCESS) {
        return 1;
    }

    // Set global debug flag based on CLI options
    g_debug_enabled = options.debug;

    // Remote mode: dispatch to cloner-remote daemon
    if (options.remote_host) {
        int port = options.remote_port > 0 ? options.remote_port : CLONER_DEFAULT_PORT;
        if (remote_connect(options.remote_host, port, options.auth_token) < 0)
            return EXIT_PROTOCOL_ERROR;

        int rc = 0;
        const char *cpu = options.force_cpu;
        if (!cpu) {
            cpu = remote_detect_variant(options.device_index);
            if (cpu) {
                printf("Auto-detected remote device: %s\n", cpu);
            } else {
                fprintf(stderr, "Failed to detect remote device variant\n");
                remote_disconnect();
                return EXIT_DEVICE_ERROR;
            }
        }

        if (options.list_devices) {
            rc = remote_list_devices() < 0 ? EXIT_DEVICE_ERROR : 0;
        } else if (options.write_firmware && options.input_file && options.bootstrap) {
            /* Combined bootstrap + write (remote) */
            rc = remote_bootstrap(options.device_index, cpu, options.firmware_dir);
            if (rc == 0) {
                printf("Bootstrap complete, proceeding to write\n\n");
                rc = remote_write_firmware(options.device_index, cpu, options.input_file);
            }
            rc = rc < 0 ? EXIT_TRANSFER_ERROR : 0;
        } else if (options.bootstrap) {
            rc = remote_bootstrap(options.device_index, cpu, options.firmware_dir) < 0 ? EXIT_DEVICE_ERROR : 0;
        } else if (options.write_firmware && options.input_file) {
            rc = remote_write_firmware(options.device_index, cpu, options.input_file) < 0 ? EXIT_TRANSFER_ERROR : 0;
        } else if (options.read_firmware && options.output_file) {
            rc = remote_read_firmware(options.device_index, options.output_file) < 0 ? EXIT_TRANSFER_ERROR : 0;
        } else {
            printf("Remote mode: specify -l, -b, -w, or -r\n");
            rc = 1;
        }

        remote_disconnect();
        return rc < 0 ? 1 : 0;
    }

    // Local mode: use public core API
    cloner_error_t cerr = cloner_init();
    if (cerr != CLONER_OK) {
        printf("Failed to initialize cloner: %d\n", cerr);
        return EXIT_DEVICE_ERROR;
    }

    int exit_code = 0;

    if (options.list_devices) {
        cloner_device_list_t list = {0};
        cerr = cloner_discover_devices(&list);
        if (cerr != CLONER_OK) {
            exit_code = EXIT_DEVICE_ERROR;
        } else {
            printf("Found %d device(s):\n", list.count);
            printf("Index | Bus | Addr | Vendor  | Product | Stage    | Variant\n");
            printf("------|-----|------|---------|---------|----------|--------\n");
            for (int i = 0; i < list.count; i++) {
                const char *stage = list.devices[i].stage == CLONER_STAGE_FIRMWARE ? "firmware" : "bootrom";
                printf("  %3d | %3d | %4d | 0x%04X  | 0x%04X  | %-8s | %s\n", i, list.devices[i].bus,
                       list.devices[i].address, list.devices[i].vendor_id, list.devices[i].product_id, stage,
                       cloner_variant_to_string(list.devices[i].variant));
            }
            cloner_free_device_list(&list);
        }
    } else if (options.bootstrap && !options.write_firmware && !options.read_firmware) {
        /* Bootstrap only — uses cloner_op_bootstrap which auto-detects SoC */
        usb_manager_t manager;
        result = usb_manager_init(&manager);
        if (result != THINGINO_SUCCESS) {
            exit_code = EXIT_DEVICE_ERROR;
            goto cleanup;
        }
        result =
            cloner_op_bootstrap(&manager, options.device_index, options.force_cpu, options.verbose, options.skip_ddr,
                                options.config_file, options.spl_file, options.uboot_file, options.firmware_dir);
        usb_manager_cleanup(&manager);
        if (result != THINGINO_SUCCESS) {
            exit_code = EXIT_DEVICE_ERROR;
        }
    } else if (options.write_firmware) {
        /* Write with optional bootstrap. cloner_op_write_firmware handles
         * bootstrap internally when do_bootstrap=false (device in bootrom). */
        usb_manager_t manager;
        result = usb_manager_init(&manager);
        if (result != THINGINO_SUCCESS) {
            exit_code = EXIT_DEVICE_ERROR;
            goto cleanup;
        }
        if (options.bootstrap)
            fprintf(stderr, "Bootstrap + write mode\n");
        result = cloner_op_write_firmware(&manager, options.device_index, options.input_file, options.force_cpu,
                                          options.flash_chip, options.force_erase, options.reboot_after, false,
                                          options.verbose, options.skip_ddr, options.config_file, options.spl_file,
                                          options.uboot_file, options.firmware_dir, options.chunk_size);
        usb_manager_cleanup(&manager);
        if (result != THINGINO_SUCCESS)
            exit_code = EXIT_TRANSFER_ERROR;
    } else if (options.read_firmware) {
        /* Read with optional bootstrap. cloner_op_read_firmware detects
         * bootrom stage and bootstraps internally if needed. */
        usb_manager_t manager;
        result = usb_manager_init(&manager);
        if (result != THINGINO_SUCCESS) {
            exit_code = EXIT_DEVICE_ERROR;
            goto cleanup;
        }
        if (options.bootstrap)
            fprintf(stderr, "Bootstrap + read mode\n");
        result = cloner_op_read_firmware(&manager, options.device_index, options.output_file, options.force_cpu,
                                         options.flash_chip);
        usb_manager_cleanup(&manager);
        if (result != THINGINO_SUCCESS)
            exit_code = EXIT_TRANSFER_ERROR;
    } else {
        printf("No action specified. Use -h for help.\n");
        exit_code = 1;
    }

cleanup:
    cloner_cleanup();
    return exit_code;
}
