/* test_sdbus_compat.c — functional check for the runtime sd-bus loader.
 *
 * Verifies that sdbus_compat_init() finds a provider on this host, that the
 * resolved function pointers actually work (open + round-trip on the session
 * bus), and cleans up. Run under a private bus, e.g.:
 *     dbus-run-session -- ./test_sdbus_compat
 * Exit 0 on success. See run_sdbus_compat_test.sh (which also asserts the built
 * library carries no libsystemd DT_NEEDED). */
#include "sdbus_compat.h"

#include <stdio.h>
#include <string.h>

int main(void) {
    if (sdbus_compat_init() != 0) {
        fprintf(stderr, "FAIL: sdbus_compat_init() found no usable provider\n");
        return 1;
    }

    const char *provider = sdbus_compat_provider();
    if (!provider) {
        fprintf(stderr, "FAIL: provider name is NULL after successful init\n");
        return 1;
    }
    printf("provider: %s\n", provider);

    /* Exercise a resolved pointer end to end: open the user bus, make one call
     * that requires a working sd-bus, then tear down. */
    sd_bus *bus = NULL;
    int r = sd_bus_open_user(&bus);
    if (r < 0) {
        fprintf(stderr, "FAIL: sd_bus_open_user via %s: %s\n", provider, strerror(-r));
        return 1;
    }

    sd_bus_error err = SD_BUS_ERROR_NULL;
    sd_bus_message *reply = NULL;
    r = sd_bus_call_method(bus,
                           "org.freedesktop.DBus", "/org/freedesktop/DBus",
                           "org.freedesktop.DBus", "GetId",
                           &err, &reply, NULL);
    if (r < 0) {
        fprintf(stderr, "FAIL: sd_bus_call_method(GetId): %s\n",
                err.message ? err.message : strerror(-r));
        sd_bus_error_free(&err);
        sd_bus_flush_close_unref(bus);
        return 1;
    }

    sd_bus_error_free(&err);
    sd_bus_message_unref(reply);
    sd_bus_flush_close_unref(bus);

    printf("PASS: loader resolved sd-bus via %s and completed a bus round-trip\n",
           provider);
    return 0;
}
