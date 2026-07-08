/* sdbus_compat.c — runtime loader for the sd-bus API. See sdbus_compat.h. */
#define SDBUS_COMPAT_IMPL
#include "sdbus_compat.h"

#include <dlfcn.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

int  (*p_sd_bus_open_user)(sd_bus **);
int  (*p_sd_bus_call_method)(sd_bus *, const char *, const char *, const char *, const char *,
                             sd_bus_error *, sd_bus_message **, const char *, ...);
int  (*p_sd_bus_add_object_vtable)(sd_bus *, sd_bus_slot **, const char *, const char *,
                                   const sd_bus_vtable *, void *);
int  (*p_sd_bus_request_name)(sd_bus *, const char *, uint64_t);
int  (*p_sd_bus_release_name)(sd_bus *, const char *);
int  (*p_sd_bus_process)(sd_bus *, sd_bus_message **);
int  (*p_sd_bus_get_fd)(sd_bus *);
int  (*p_sd_bus_send)(sd_bus *, sd_bus_message *, uint64_t *);
int  (*p_sd_bus_emit_signal)(sd_bus *, const char *, const char *, const char *, const char *, ...);
int  (*p_sd_bus_emit_properties_changed)(sd_bus *, const char *, const char *, const char *, ...);
int  (*p_sd_bus_message_new_method_return)(sd_bus_message *, sd_bus_message **);
int  (*p_sd_bus_message_append)(sd_bus_message *, const char *, ...);
int  (*p_sd_bus_message_append_array)(sd_bus_message *, char, const void *, size_t);
int  (*p_sd_bus_message_open_container)(sd_bus_message *, char, const char *);
int  (*p_sd_bus_message_close_container)(sd_bus_message *);
int  (*p_sd_bus_message_enter_container)(sd_bus_message *, char, const char *);
int  (*p_sd_bus_message_exit_container)(sd_bus_message *);
int  (*p_sd_bus_message_read)(sd_bus_message *, const char *, ...);
int  (*p_sd_bus_message_skip)(sd_bus_message *, const char *);
sd_bus_message *(*p_sd_bus_message_unref)(sd_bus_message *);
int  (*p_sd_bus_reply_method_return)(sd_bus_message *, const char *, ...);
sd_bus_slot *(*p_sd_bus_slot_unref)(sd_bus_slot *);
sd_bus *(*p_sd_bus_flush_close_unref)(sd_bus *);
void (*p_sd_bus_error_free)(sd_bus_error *);

/* The SD_BUS_VTABLE_START macro embeds &sd_bus_object_vtable_format into the
 * static vtable as a load-time data relocation — resolved before our runtime
 * dlopen runs, so we must define it ourselves or the library won't load without
 * a provider present at load time. The provider only compares this pointer to
 * its own symbol to enable optional parameter-name introspection metadata; when
 * they differ it falls back to the legacy vtable format, which is fully
 * functional (parameter names are cosmetic D-Bus introspection only). */
const unsigned sd_bus_object_vtable_format = 0;

static const char *g_provider = NULL;

/* Providers of the sd-bus API, in preference order. libsystemd first (real
 * systemd), then elogind and basu (drop-in sd-bus for systemd-free distros). */
static const char *const kProviders[] = {
    "libsystemd.so.0",
    "libelogind.so.0",
    "libbasu.so.0",
    NULL,
};

const char *sdbus_compat_provider(void) { return g_provider; }

/* Resolve every sd_bus_* entry point from an opened provider handle. Returns 0
 * on success; on failure returns -1 and sets *missing to the first symbol not
 * found (so callers can report exactly what an incomplete provider lacked). */
static int resolve_symbols(void *h, const char **missing) {
#define LOAD(sym)                                             \
    do {                                                      \
        *(void **)(&p_##sym) = dlsym(h, #sym);                \
        if (!p_##sym) { *missing = #sym; return -1; }         \
    } while (0)

    LOAD(sd_bus_open_user);
    LOAD(sd_bus_call_method);
    LOAD(sd_bus_add_object_vtable);
    LOAD(sd_bus_request_name);
    LOAD(sd_bus_release_name);
    LOAD(sd_bus_process);
    LOAD(sd_bus_get_fd);
    LOAD(sd_bus_send);
    LOAD(sd_bus_emit_signal);
    LOAD(sd_bus_emit_properties_changed);
    LOAD(sd_bus_message_new_method_return);
    LOAD(sd_bus_message_append);
    LOAD(sd_bus_message_append_array);
    LOAD(sd_bus_message_open_container);
    LOAD(sd_bus_message_close_container);
    LOAD(sd_bus_message_enter_container);
    LOAD(sd_bus_message_exit_container);
    LOAD(sd_bus_message_read);
    LOAD(sd_bus_message_skip);
    LOAD(sd_bus_message_unref);
    LOAD(sd_bus_reply_method_return);
    LOAD(sd_bus_slot_unref);
    LOAD(sd_bus_flush_close_unref);
    LOAD(sd_bus_error_free);
#undef LOAD
    return 0;
}

int sdbus_compat_init(void) {
    if (g_provider) return 0; /* already loaded */

    /* Try each provider in turn. A provider that opens but is missing a symbol
     * (e.g. an old/partial sd-bus) is skipped rather than fatal, so a later,
     * complete provider still wins. */
    for (const char *const *name = kProviders; *name; ++name) {
        void *h = dlopen(*name, RTLD_NOW | RTLD_GLOBAL);
        if (!h) continue;

        const char *missing = NULL;
        if (resolve_symbols(h, &missing) == 0) {
            g_provider = *name;
            if (getenv("COMPOSETRAY_DEBUG"))
                fprintf(stderr, "sni: sd-bus provider loaded: %s\n", *name);
            return 0;
        }

        fprintf(stderr, "sni: sd-bus provider '%s' is missing symbol '%s'; "
                        "trying next\n", *name, missing);
        dlclose(h);
    }

    fprintf(stderr, "sni: no usable sd-bus provider found (tried "
                    "libsystemd.so.0, libelogind.so.0, libbasu.so.0)\n");
    return -1;
}
