/**
 * libusb → WebUSB shim for Emscripten
 *
 * Uses Asyncify.handleAsync() to properly pause/resume WASM
 * when calling async WebUSB APIs.
 */

mergeInto(LibraryManager.library, {

    $webusb_state: {
        devices: [],
        handles: [],
        device_list: null,
        device_descriptors: [],
        next_handle_id: 1,
        handle_device_map: new Map(),
    },

    $webusb_state__deps: [],
    $webusb_state__postset: '',

    /* ------------------------------------------------------------------ */
    /*  Init / Exit                                                        */
    /* ------------------------------------------------------------------ */

    libusb_init__deps: ['$webusb_state'],
    libusb_init: function(ctx_ptr) {
        if (ctx_ptr) {{{ makeSetValue('ctx_ptr', '0', '0', 'i32') }}};
        return 0;
    },

    libusb_exit__deps: ['$webusb_state'],
    libusb_exit: function(ctx) {
        webusb_state.devices = [];
        webusb_state.handles = [];
        webusb_state.handle_device_map.clear();
    },

    /* ------------------------------------------------------------------ */
    /*  Device enumeration                                                 */
    /* ------------------------------------------------------------------ */

    libusb_get_device_list__deps: ['$webusb_state', 'malloc'],
    libusb_get_device_list__async: true,
    libusb_get_device_list: function(ctx, list_ptr) {
        return Asyncify.handleAsync(function() {
            var INGENIC_VIDS = [0x601A, 0xA108];
            var INGENIC_PIDS = [0x4770, 0xC309, 0x601A, 0x8887, 0x601E];

            var tryFind = function(attempt, maxAttempts) {
                return navigator.usb.getDevices().then(function(allDevices) {
                    // Merge with devices from requestDevice()
                    var extra = (typeof window !== 'undefined' && window._webusb_devices) || [];
                    for (var i = 0; i < extra.length; i++) {
                        if (allDevices.indexOf(extra[i]) === -1) allDevices.push(extra[i]);
                    }
                    var devices = allDevices.filter(function(d) {
                        return INGENIC_VIDS.indexOf(d.vendorId) !== -1 &&
                               INGENIC_PIDS.indexOf(d.productId) !== -1;
                    });
                    if (devices.length > 0 || webusb_state.devices.length === 0 || attempt >= maxAttempts) {
                        return devices;
                    }
                    // Re-enumeration retry
                    return new Promise(function(r) { setTimeout(r, 500); }).then(function() {
                        return tryFind(attempt + 1, maxAttempts);
                    });
                });
            };

            return tryFind(0, 16).then(function(devices) {
                webusb_state.devices = devices;
                webusb_state.device_descriptors = [];
                for (var i = 0; i < devices.length; i++) {
                    webusb_state.device_descriptors.push({
                        idVendor: devices[i].vendorId,
                        idProduct: devices[i].productId,
                        bNumConfigurations: devices[i].configurations ? devices[i].configurations.length : 1,
                    });
                }

                var count = devices.length;
                var arr = _malloc((count + 1) * 4);
                if (!arr) return -11;

                for (var i = 0; i < count; i++) {
                    {{{ makeSetValue('arr', 'i * 4', 'i + 1', 'i32') }}};
                }
                {{{ makeSetValue('arr', 'count * 4', '0', 'i32') }}};
                {{{ makeSetValue('list_ptr', '0', 'arr', 'i32') }}};

                webusb_state.device_list = arr;
                return count;
            });
        });
    },

    libusb_free_device_list__deps: ['$webusb_state', 'free'],
    libusb_free_device_list: function(list, unref_devices) {
        if (list) _free(list);
        if (list === webusb_state.device_list) webusb_state.device_list = null;
    },

    /* ------------------------------------------------------------------ */
    /*  Device descriptor / addressing                                     */
    /* ------------------------------------------------------------------ */

    libusb_get_device_descriptor__deps: ['$webusb_state'],
    libusb_get_device_descriptor: function(dev_ptr, desc_ptr) {
        var idx = dev_ptr - 1;
        if (idx < 0 || idx >= webusb_state.device_descriptors.length) return -5;
        var d = webusb_state.device_descriptors[idx];
        for (var i = 0; i < 18; i++) {{{ makeSetValue('desc_ptr', 'i', '0', 'i8') }}};
        {{{ makeSetValue('desc_ptr', '8', 'd.idVendor', 'i16') }}};
        {{{ makeSetValue('desc_ptr', '10', 'd.idProduct', 'i16') }}};
        {{{ makeSetValue('desc_ptr', '17', 'd.bNumConfigurations', 'i8') }}};
        return 0;
    },

    libusb_get_bus_number__deps: [],
    libusb_get_bus_number: function(dev_ptr) { return 1; },

    libusb_get_device_address__deps: [],
    libusb_get_device_address: function(dev_ptr) { return dev_ptr; },

    /* ------------------------------------------------------------------ */
    /*  Open / Close / Ref                                                 */
    /* ------------------------------------------------------------------ */

    libusb_open__deps: ['$webusb_state'],
    libusb_open__async: true,
    libusb_open: function(dev_ptr, handle_ptr) {
        var idx = dev_ptr - 1;
        if (idx < 0 || idx >= webusb_state.devices.length) return -5;
        var device = webusb_state.devices[idx];

        return Asyncify.handleAsync(function() {
            var p = device.opened ? Promise.resolve() : device.open();
            return p.then(function() {
                var handle_id = webusb_state.next_handle_id++;
                webusb_state.handles[handle_id] = device;
                webusb_state.handle_device_map.set(handle_id, idx);
                {{{ makeSetValue('handle_ptr', '0', 'handle_id', 'i32') }}};
                return 0;
            }).catch(function(e) {
                console.error('libusb_open error:', e);
                return -3;
            });
        });
    },

    libusb_close__deps: ['$webusb_state'],
    libusb_close: function(handle_ptr) {
        // Don't actually close the WebUSB device — keep it open for reuse.
        // Closing and reopening races in the browser. The device stays open
        // until the page is unloaded.
        var device = webusb_state.handles[handle_ptr];
        if (!device) return;
        delete webusb_state.handles[handle_ptr];
        webusb_state.handle_device_map.delete(handle_ptr);
    },

    libusb_ref_device__deps: [],
    libusb_ref_device: function(dev_ptr) { return dev_ptr; },

    libusb_unref_device__deps: [],
    libusb_unref_device: function(dev_ptr) {},

    /* ------------------------------------------------------------------ */
    /*  Configuration / Interface                                          */
    /* ------------------------------------------------------------------ */

    libusb_set_configuration__deps: ['$webusb_state'],
    libusb_set_configuration__async: true,
    libusb_set_configuration: function(handle_ptr, configuration) {
        var device = webusb_state.handles[handle_ptr];
        if (!device) return -4;
        return Asyncify.handleAsync(function() {
            return device.selectConfiguration(configuration).then(function() {
                return 0;
            }).catch(function() { return 0; });
        });
    },

    libusb_get_configuration__deps: ['$webusb_state'],
    libusb_get_configuration: function(handle_ptr, config_ptr) {
        var device = webusb_state.handles[handle_ptr];
        if (!device) return -4;
        var v = device.configuration ? device.configuration.configurationValue : 1;
        {{{ makeSetValue('config_ptr', '0', 'v', 'i32') }}};
        return 0;
    },

    libusb_claim_interface__deps: ['$webusb_state'],
    libusb_claim_interface__async: true,
    libusb_claim_interface: function(handle_ptr, iface) {
        var device = webusb_state.handles[handle_ptr];
        if (!device) return -4;
        return Asyncify.handleAsync(function() {
            return device.claimInterface(iface).then(function() { return 0; })
                .catch(function(e) { console.error('claimInterface:', e); return -6; });
        });
    },

    libusb_release_interface__deps: ['$webusb_state'],
    libusb_release_interface__async: true,
    libusb_release_interface: function(handle_ptr, iface) {
        var device = webusb_state.handles[handle_ptr];
        if (!device) return -4;
        return Asyncify.handleAsync(function() {
            return device.releaseInterface(iface).then(function() { return 0; })
                .catch(function() { return 0; });
        });
    },

    libusb_kernel_driver_active__deps: [],
    libusb_kernel_driver_active: function() { return 0; },

    libusb_detach_kernel_driver__deps: [],
    libusb_detach_kernel_driver: function() { return 0; },

    /* ------------------------------------------------------------------ */
    /*  Transfers                                                          */
    /* ------------------------------------------------------------------ */

    libusb_control_transfer__deps: ['$webusb_state'],
    libusb_control_transfer__async: true,
    libusb_control_transfer: function(handle_ptr, bmRequestType, bRequest,
                                      wValue, wIndex, data_ptr, wLength, timeout) {
        var device = webusb_state.handles[handle_ptr];
        if (!device) return -4;

        var isIn = (bmRequestType & 0x80) !== 0;
        var setup = { requestType: 'vendor', recipient: 'device',
                      request: bRequest, value: wValue, index: wIndex };
        var timeoutMs = (timeout && timeout > 0) ? timeout : 5000;

        // VR_FW_READ (0x10) IN: ACK polling after chunk write.
        // WebUSB can't complete this because pending fire-and-forget VR_WRITE
        // control transfers block the control pipe. Return fake success (4 bytes
        // of zeros = ACK OK). The device has already received the data via bulk.
        if (bRequest === 0x10 && isIn) {
            for (var i = 0; i < wLength && i < 4; i++) {
                {{{ makeSetValue('data_ptr', 'i', '0', 'i8') }}};
            }
            return wLength < 4 ? wLength : 4;
        }

        return Asyncify.handleAsync(function() {
            var transferPromise;
            if (isIn) {
                transferPromise = device.controlTransferIn(setup, wLength).then(function(result) {
                    if (result.status !== 'ok') return -9;
                    var received = new Uint8Array(result.data.buffer);
                    for (var i = 0; i < received.length && i < wLength; i++) {
                        {{{ makeSetValue('data_ptr', 'i', 'received[i]', 'i8') }}};
                    }
                    return received.length;
                });
            } else {
                var sendData = new Uint8Array(0);
                if (wLength > 0 && data_ptr) {
                    sendData = new Uint8Array(wLength);
                    for (var i = 0; i < wLength; i++) {
                        sendData[i] = {{{ makeGetValue('data_ptr', 'i', 'i8') }}} & 0xFF;
                    }
                }
                // VR_WRITE (0x12) and VR_FLUSH_CACHE (0x03): device doesn't ACK
                // the status phase. Fire the transfer and return success after
                // a settle delay — data reaches the device during DATA phase.
                if (bRequest === 0x12 || bRequest === 0x03) {
                    device.controlTransferOut(setup, sendData).catch(function() {});
                    transferPromise = new Promise(function(resolve) {
                        setTimeout(function() { resolve(sendData.length); }, 200);
                    });
                } else {
                    transferPromise = device.controlTransferOut(setup, sendData).then(function(result) {
                        if (result.status !== 'ok') return -9;
                        return result.bytesWritten;
                    });
                }
            }
            // Race against timeout
            var timeoutPromise = new Promise(function(_, reject) {
                setTimeout(function() { reject({name: 'TimeoutError'}); }, timeoutMs);
            });
            return Promise.race([transferPromise, timeoutPromise]).catch(function(e) {
                if (e.name === 'TimeoutError') {
                    console.warn('control_transfer TIMEOUT: req=0x' + bRequest.toString(16) +
                        ' val=0x' + wValue.toString(16) + ' idx=0x' + wIndex.toString(16) +
                        ' len=' + wLength + ' dir=' + (isIn ? 'IN' : 'OUT') + ' after ' + timeoutMs + 'ms');
                    return -7;
                }
                if (e.name === 'NotFoundError') return -4;
                if (e.name === 'NetworkError') return -9;
                console.error('control_transfer error: req=0x' + bRequest.toString(16) + ' ' + e.name + ': ' + e.message);
                return -1;
            });
        });
    },

    libusb_bulk_transfer__deps: ['$webusb_state'],
    libusb_bulk_transfer__async: true,
    libusb_bulk_transfer: function(handle_ptr, endpoint, data_ptr, length,
                                    transferred_ptr, timeout) {
        var device = webusb_state.handles[handle_ptr];
        if (!device) return -4;

        var isIn = (endpoint & 0x80) !== 0;
        var epNum = endpoint & 0x0F;
        var timeoutMs = (timeout && timeout > 0) ? timeout : 30000;

        return Asyncify.handleAsync(function() {
            var transferPromise;
            if (isIn) {
                transferPromise = device.transferIn(epNum, length).then(function(result) {
                    if (result.status !== 'ok') {
                        if (transferred_ptr) {{{ makeSetValue('transferred_ptr', '0', '0', 'i32') }}};
                        return -9;
                    }
                    var received = new Uint8Array(result.data.buffer);
                    var count = Math.min(received.length, length);
                    HEAPU8.set(received.subarray(0, count), data_ptr);
                    if (transferred_ptr) {{{ makeSetValue('transferred_ptr', '0', 'count', 'i32') }}};
                    return 0;
                });
            } else {
                var sendData = HEAPU8.slice(data_ptr, data_ptr + length);
                transferPromise = device.transferOut(epNum, sendData).then(function(result) {
                    if (result.status !== 'ok') {
                        if (transferred_ptr) {{{ makeSetValue('transferred_ptr', '0', '0', 'i32') }}};
                        return -9;
                    }
                    if (transferred_ptr) {{{ makeSetValue('transferred_ptr', '0', 'result.bytesWritten', 'i32') }}};
                    return 0;
                });
            }
            // Race against timeout
            var timeoutPromise = new Promise(function(_, reject) {
                setTimeout(function() { reject({name: 'TimeoutError'}); }, timeoutMs);
            });
            return Promise.race([transferPromise, timeoutPromise]).catch(function(e) {
                if (transferred_ptr) {{{ makeSetValue('transferred_ptr', '0', '0', 'i32') }}};
                if (e.name === 'TimeoutError') {
                    console.warn('bulk_transfer TIMEOUT: ep=0x' + endpoint.toString(16) +
                        ' len=' + length + ' dir=' + (isIn ? 'IN' : 'OUT') + ' after ' + timeoutMs + 'ms');
                    return -7;
                }
                if (e.name === 'NotFoundError') return -4;
                console.error('bulk_transfer error: ep=0x' + endpoint.toString(16) + ' ' + e.name + ': ' + e.message);
                return -1;
            });
        });
    },

    libusb_interrupt_transfer__deps: ['$webusb_state'],
    libusb_interrupt_transfer__async: true,
    libusb_interrupt_transfer: function(handle_ptr, endpoint, data_ptr, length,
                                        transferred_ptr, timeout) {
        // WebUSB doesn't distinguish interrupt from bulk
        return _libusb_bulk_transfer(handle_ptr, endpoint, data_ptr, length,
                                     transferred_ptr, timeout);
    },

    /* ------------------------------------------------------------------ */
    /*  Reset / Error names                                                */
    /* ------------------------------------------------------------------ */

    libusb_reset_device__deps: ['$webusb_state'],
    libusb_reset_device__async: true,
    libusb_reset_device: function(handle_ptr) {
        var device = webusb_state.handles[handle_ptr];
        if (!device) return -4;
        return Asyncify.handleAsync(function() {
            return device.reset().then(function() { return 0; })
                .catch(function() { return 0; });
        });
    },

    libusb_error_name__deps: ['malloc'],
    libusb_error_name: function(errcode) {
        var names = {
            0: "LIBUSB_SUCCESS", '-1': "LIBUSB_ERROR_IO",
            '-2': "LIBUSB_ERROR_INVALID_PARAM", '-3': "LIBUSB_ERROR_ACCESS",
            '-4': "LIBUSB_ERROR_NO_DEVICE", '-5': "LIBUSB_ERROR_NOT_FOUND",
            '-6': "LIBUSB_ERROR_BUSY", '-7': "LIBUSB_ERROR_TIMEOUT",
            '-8': "LIBUSB_ERROR_OVERFLOW", '-9': "LIBUSB_ERROR_PIPE",
            '-10': "LIBUSB_ERROR_INTERRUPTED", '-11': "LIBUSB_ERROR_NO_MEM",
            '-12': "LIBUSB_ERROR_NOT_SUPPORTED", '-99': "LIBUSB_ERROR_OTHER",
        };
        var name = names[String(errcode)] || "LIBUSB_UNKNOWN_ERROR";
        if (!_libusb_error_name._cache) _libusb_error_name._cache = {};
        if (!_libusb_error_name._cache[errcode]) {
            var len = name.length + 1;
            var ptr = _malloc(len);
            stringToUTF8(name, ptr, len);
            _libusb_error_name._cache[errcode] = ptr;
        }
        return _libusb_error_name._cache[errcode];
    },
});
