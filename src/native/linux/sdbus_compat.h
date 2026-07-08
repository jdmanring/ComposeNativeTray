/*
 * sdbus_compat.h — make a single prebuilt libLinuxTray.so work on both systemd
 * and non-systemd systems.
 *
 * The tray talks StatusNotifierItem over sd-bus. Linking directly against
 * libsystemd (DT_NEEDED libsystemd.so.0) makes the shipped .so fail to load on
 * systemd-free distros (Artix, Void, Gentoo/OpenRC, Alpine, Devuan, ...), where
 * that soname does not exist — the tray then silently never registers.
 *
 * Instead we resolve the sd_bus_* entry points at runtime via dlopen/dlsym,
 * trying the ABI-compatible providers in order:
 *     libsystemd.so.0  →  libelogind.so.0  →  libbasu.so.0
 * elogind and basu both expose the same sd-bus symbols, so one binary now runs
 * everywhere. Call sdbus_compat_init() once before any sd_bus_* use.
 *
 * sni.c is otherwise unchanged: including this header (after <systemd/sd-bus.h>,
 * which still supplies the types and SD_BUS_* vtable macros) rewrites the
 * existing sd_bus_* call sites to the loaded function pointers via the macros at
 * the bottom. The implementation TU defines SDBUS_COMPAT_IMPL to opt out of the
 * rewrite so it can assign the pointers.
 *
 * Logging (to stderr, matching sni.c's "sni:" prefix):
 *   - always: a warning naming the missing symbol when a provider opens but is
 *     incomplete, and a final line if no provider could be loaded at all;
 *   - when the environment variable COMPOSETRAY_DEBUG is set: one line naming
 *     the provider that was bound (e.g. "sni: sd-bus provider loaded:
 *     libelogind.so.0"), for diagnosing which backend a given host resolved.
 */
#ifndef SDBUS_COMPAT_H
#define SDBUS_COMPAT_H

#include <systemd/sd-bus.h>
#include <stddef.h>
#include <stdint.h>

/* Loads the first available sd-bus provider. Returns 0 on success, -1 if none
 * of libsystemd/libelogind/basu could be found or a symbol was missing. */
int sdbus_compat_init(void);

/* Human-readable soname of the provider that was loaded (or NULL). */
const char *sdbus_compat_provider(void);

/* Function pointers, filled in by sdbus_compat_init(). Variadic entries keep
 * their variadic signature so existing call sites pass args unchanged. */
extern int  (*p_sd_bus_open_user)(sd_bus **ret);
extern int  (*p_sd_bus_call_method)(sd_bus *bus, const char *destination, const char *path,
                                    const char *interface, const char *member,
                                    sd_bus_error *ret_error, sd_bus_message **reply,
                                    const char *types, ...);
extern int  (*p_sd_bus_add_object_vtable)(sd_bus *bus, sd_bus_slot **slot, const char *path,
                                          const char *interface, const sd_bus_vtable *vtable,
                                          void *userdata);
extern int  (*p_sd_bus_request_name)(sd_bus *bus, const char *name, uint64_t flags);
extern int  (*p_sd_bus_release_name)(sd_bus *bus, const char *name);
extern int  (*p_sd_bus_process)(sd_bus *bus, sd_bus_message **r);
extern int  (*p_sd_bus_get_fd)(sd_bus *bus);
extern int  (*p_sd_bus_send)(sd_bus *bus, sd_bus_message *m, uint64_t *cookie);
extern int  (*p_sd_bus_emit_signal)(sd_bus *bus, const char *path, const char *interface,
                                    const char *member, const char *types, ...);
extern int  (*p_sd_bus_emit_properties_changed)(sd_bus *bus, const char *path,
                                                const char *interface, const char *name, ...);
extern int  (*p_sd_bus_message_new_method_return)(sd_bus_message *call, sd_bus_message **m);
extern int  (*p_sd_bus_message_append)(sd_bus_message *m, const char *types, ...);
extern int  (*p_sd_bus_message_append_array)(sd_bus_message *m, char type,
                                             const void *ptr, size_t size);
extern int  (*p_sd_bus_message_open_container)(sd_bus_message *m, char type, const char *contents);
extern int  (*p_sd_bus_message_close_container)(sd_bus_message *m);
extern int  (*p_sd_bus_message_enter_container)(sd_bus_message *m, char type, const char *contents);
extern int  (*p_sd_bus_message_exit_container)(sd_bus_message *m);
extern int  (*p_sd_bus_message_read)(sd_bus_message *m, const char *types, ...);
extern int  (*p_sd_bus_message_skip)(sd_bus_message *m, const char *types);
extern sd_bus_message *(*p_sd_bus_message_unref)(sd_bus_message *m);
extern int  (*p_sd_bus_reply_method_return)(sd_bus_message *call, const char *types, ...);
extern sd_bus_slot *(*p_sd_bus_slot_unref)(sd_bus_slot *slot);
extern sd_bus *(*p_sd_bus_flush_close_unref)(sd_bus *bus);
extern void (*p_sd_bus_error_free)(sd_bus_error *e);

/* Reroute existing sd_bus_* call sites through the pointers, except inside the
 * compat implementation TU (which needs the real names for dlsym assignment). */
#ifndef SDBUS_COMPAT_IMPL
#define sd_bus_open_user                p_sd_bus_open_user
#define sd_bus_call_method              p_sd_bus_call_method
#define sd_bus_add_object_vtable        p_sd_bus_add_object_vtable
#define sd_bus_request_name             p_sd_bus_request_name
#define sd_bus_release_name             p_sd_bus_release_name
#define sd_bus_process                  p_sd_bus_process
#define sd_bus_get_fd                   p_sd_bus_get_fd
#define sd_bus_send                     p_sd_bus_send
#define sd_bus_emit_signal              p_sd_bus_emit_signal
#define sd_bus_emit_properties_changed  p_sd_bus_emit_properties_changed
#define sd_bus_message_new_method_return p_sd_bus_message_new_method_return
#define sd_bus_message_append           p_sd_bus_message_append
#define sd_bus_message_append_array     p_sd_bus_message_append_array
#define sd_bus_message_open_container   p_sd_bus_message_open_container
#define sd_bus_message_close_container  p_sd_bus_message_close_container
#define sd_bus_message_enter_container  p_sd_bus_message_enter_container
#define sd_bus_message_exit_container   p_sd_bus_message_exit_container
#define sd_bus_message_read             p_sd_bus_message_read
#define sd_bus_message_skip             p_sd_bus_message_skip
#define sd_bus_message_unref            p_sd_bus_message_unref
#define sd_bus_reply_method_return      p_sd_bus_reply_method_return
#define sd_bus_slot_unref               p_sd_bus_slot_unref
#define sd_bus_flush_close_unref        p_sd_bus_flush_close_unref
#define sd_bus_error_free               p_sd_bus_error_free
#endif /* SDBUS_COMPAT_IMPL */

#endif /* SDBUS_COMPAT_H */
