/* Copyright (c) 2015, Celerway, Tomasz Rozensztrauch <t.rozensztrauch@radytek.com>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/socket.h>
#include <sys/un.h>
#include <sys/time.h>
#include <string.h>
#include <getopt.h>
#include <stdlib.h>
#include <unistd.h>

#include "metadata_writer_neat.h"
#include "backend_event_loop.h"
#include "metadata_exporter_log.h"

static int md_neat_add_json_key_value_int(struct json_object *obj,
        const char *key, int64_t value)
{
    struct json_object *value_obj = NULL;

    value_obj = json_object_new_int64(value);
    if (!value_obj)
        return RETVAL_FAILURE;

    json_object_object_add(obj, key, value_obj);
    return RETVAL_SUCCESS;
}

static int md_neat_add_json_key_value_string(struct json_object *obj,
        const char *key, const char *value)
{
    struct json_object *value_obj = NULL;

    value_obj = json_object_new_string(value);
    if (!value_obj)
        return RETVAL_FAILURE;

    json_object_object_add(obj, key, value_obj);
    return RETVAL_SUCCESS;
}

static int md_neat_add_json_key_value_bool(struct json_object *obj,
        const char *key, bool value)
{
    struct json_object *value_obj = NULL;

    value_obj = json_object_new_boolean(value);
    if (!value_obj)
        return RETVAL_FAILURE;

    json_object_object_add(obj, key, value_obj);
    return RETVAL_SUCCESS;
}

static int md_neat_add_property_string(struct json_object* obj,
                                       const char *key,
                                       const char *value,
                                       uint8_t precedence)
{
    struct json_object *value_obj = NULL;

    value_obj = json_object_new_object();
    if (!value_obj)
        return RETVAL_FAILURE;

    md_neat_add_json_key_value_string(value_obj, "value", value);
    md_neat_add_json_key_value_int(value_obj, "precedence", precedence);

    json_object_object_add(obj, key, value_obj);
    return RETVAL_SUCCESS;
}

static int md_neat_add_property_bool(struct json_object *obj,
                                     const char *key,
                                     bool value,
                                     uint8_t precedence)
{
    struct json_object *value_obj = NULL;

    value_obj = json_object_new_object();
    if (!value_obj)
        return RETVAL_FAILURE;

    md_neat_add_json_key_value_bool(value_obj, "value", value);
    md_neat_add_json_key_value_int(value_obj, "precedence", precedence);

    json_object_object_add(obj, key, value_obj);
    return RETVAL_SUCCESS;
}

static int md_neat_add_property_int(struct json_object *obj,
                                    const char *key,
                                    int64_t value,
                                    uint8_t precedence)
{
    struct json_object *value_obj = NULL;

    value_obj = json_object_new_object();
    if (!value_obj)
        return RETVAL_FAILURE;

    md_neat_add_json_key_value_int(value_obj, "value", value);
    md_neat_add_json_key_value_int(value_obj, "precedence", precedence);

    json_object_object_add(obj, key, value_obj);
    return RETVAL_SUCCESS;
}

static void md_neat_notify_pm(struct md_writer_neat *mwn, struct json_object *obj)
{
    const char *str;
    struct sockaddr_un addr;
    ssize_t res;
    int fd;

    if (strlen(mwn->cib_socket) == 0)
        return;

    if ((fd = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
        META_PRINT_SYSLOG(mwn->parent, LOG_ERR, "NEAT: md_neat_notify_pm: Failed to create unix socket");
        return;
    }

    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, mwn->cib_socket, sizeof(addr.sun_path) - 1);

    if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) == -1) {
        META_PRINT_SYSLOG(mwn->parent, LOG_ERR, "NEAT: md_motify_pm connect error");
        return;
    }

    str = json_object_to_json_string_ext(obj, JSON_C_TO_STRING_PLAIN);

    res = write(fd, str, strlen(str));
    META_PRINT_SYSLOG(mwn->parent, LOG_INFO, "NEAT: md_motift_pm writen %li bytes out of %li", res, strlen(str));

    close(fd);
}

static void md_neat_dump_cib_file(struct md_writer_neat *mwn, const char *uid, struct json_object *obj)
{
    const char *str;
    FILE *file;
    char fname[512];
    int retval;

    if (strlen(mwn->cib_prefix) == 0)
        return;

    retval = snprintf(fname, sizeof(fname), "%s%s%s", mwn->cib_prefix, uid, mwn->cib_extension);
    if (retval >= sizeof(fname)) {
        META_PRINT_SYSLOG(mwn->parent, LOG_ERR, "NEAT: Failed to format CIB file name\n");
        return;
    }

    if ((file = fopen(fname, "w")) == NULL) {
        META_PRINT_SYSLOG(mwn->parent, LOG_ERR, "NEAT: Failed to open file %s\n", fname);
        return;
    }

    str = json_object_to_json_string_ext(obj, JSON_C_TO_STRING_PLAIN);

    if (fprintf(file, "%s", str) < 0) {
        META_PRINT_SYSLOG(mwn->parent, LOG_ERR, "NEAT: Failed to write to file %s\n", fname);
    }

    fclose(file);
}

static void md_neat_handle_iface_event(struct md_writer_neat *mwn,
                                      struct md_iface_event *mie)
{
    struct json_object *obj, *properties;

    const char* const iface_event_type[] = {
        "",
        "IFACE_EVENT_DEV_STATE",
        "IFACE_EVENT_MODE_CHANGE",
        "IFACE_EVENT_SIGNAL_CHANGE",
        "IFACE_EVENT_LTE_BAND_CHANGE",
        "IFACE_EVENT_ISP_NAME_CHANGE",
        "IFACE_EVENT_UPDATE",
        "IFACE_EVENT_IP_ADDR_CHANGE",
        "IFACE_EVENT_LOC_CHANGE",
        "IFACE_EVENT_NW_MCCMNC_CHANGE",
    };

    META_PRINT_SYSLOG(mwn->parent, LOG_ERR, "NEAT: %s: ip_addr=%s, ifname=%s, "
                      "iccid=%s, imsi_mccmnc=%d, nw_mccmnc=%d, cid=%d, "
                      "rscp=%d, lte_rsrp=%d, lac=%d, rssi=%d, ecio=%d, "
                      "lte_rssi=%d, lte_rsrq=%d, device_mode=%d, "
                      "device_submode=%d\n",
                      iface_event_type[mie->event_param],
                      mie->ip_addr, mie->ifname, mie->iccid, mie->imsi_mccmnc,
                      mie->nw_mccmnc, mie->cid, mie->rscp, mie->lte_rsrp,
                      mie->lac, mie->rssi, mie->ecio, mie->lte_rssi,
                      mie->lte_rsrq, mie->device_mode, mie->device_submode);

    obj = json_object_new_object();

    md_neat_add_json_key_value_string(obj, "uid", mie->ifname);
    md_neat_add_json_key_value_int(obj, "ts", mie->tstamp);
    md_neat_add_json_key_value_bool(obj, "root", true);
    md_neat_add_json_key_value_int(obj, "priority", 4);

    properties = json_object_new_object();
    md_neat_add_property_string(properties, "interface", mie->ifname, 2);
    if (mie->ip_addr) md_neat_add_property_string(properties, "local_ip", mie->ip_addr, 2);
    md_neat_add_property_bool(properties, "is_wired", false, 2);
    md_neat_add_property_int(properties, "device_mode", mie->device_mode, 2);
    md_neat_add_property_int(properties, "device_submode", mie->device_submode, 2);

    if (mie->device_mode != 5) {
        md_neat_add_property_int(properties, "rssi", mie->rssi, 2);
        md_neat_add_property_int(properties, "rscp", mie->rscp, 2);
        md_neat_add_property_int(properties, "ecio", mie->ecio, 2);
    } else { // LTE
        md_neat_add_property_int(properties, "lte_rssi", mie->lte_rssi, 2);
        md_neat_add_property_int(properties, "lte_rsrp", mie->lte_rsrp, 2);
        md_neat_add_property_int(properties, "lte_rsrq", mie->lte_rsrq, 2);
    }

    md_neat_add_property_int(properties, "lac", mie->lac, 2);
    md_neat_add_property_int(properties, "cid", mie->cid, 2);
    md_neat_add_property_int(properties, "oper", mie->nw_mccmnc, 2);
    md_neat_add_property_int(properties, "device_state", mie->device_state, 2);

    json_object_object_add(obj, "properties", properties);

    md_neat_notify_pm(mwn, obj);
    md_neat_dump_cib_file(mwn, mie->ifname, obj);

    json_object_put(obj);
}

static int32_t md_neat_init(void *ptr, json_object* config)
{
    struct md_writer_neat *mwn = ptr;
    strcpy(mwn->cib_socket, NEAT_DEFAULT_CIB_SOCKET_PATH);
    strcpy(mwn->cib_prefix, NEAT_DEFAULT_CIB_PREFIX);
    strcpy(mwn->cib_extension, NEAT_DEFAULT_CIB_EXTENSION);

    json_object* subconfig;
    if (json_object_object_get_ex(config, "neat", &subconfig)) {
        json_object_object_foreach(subconfig, key, val) {
            if (!strcmp(key, "cib_socket")) {
                const char* cib_socket = json_object_get_string(val);
                if (strlen(cib_socket) > sizeof(mwn->cib_socket) - 1) {
                    META_PRINT_SYSLOG(mwn->parent, LOG_ERR,
                            "NEAT: CIB socket path too long (>%ld)\n",
                            sizeof(mwn->cib_socket) - 1);
                    return RETVAL_FAILURE;
                } else
                    strcpy(mwn->cib_socket, cib_socket);
            }
            else if (!strcmp(key, "cib_prefix")) {
                const char* cib_prefix = json_object_get_string(val);
                if (strlen(cib_prefix) > sizeof(mwn->cib_prefix) - 1) {
                    META_PRINT_SYSLOG(mwn->parent, LOG_ERR,
                            "NEAT: CIB prefix too long (>%ld)\n",
                            sizeof(mwn->cib_prefix) - 1);
                    return RETVAL_FAILURE;
                } else
                    strcpy(mwn->cib_prefix, cib_prefix);
            }
        }
    }
    
    return RETVAL_SUCCESS;
}

static void md_neat_handle(struct md_writer *writer, struct md_event *event)
{
    struct md_writer_neat *mwn = (struct md_writer_neat*) writer;
    META_PRINT_SYSLOG(mwn->parent, LOG_INFO, "NEAT: md_neat_handle: md_type %u\n", event->md_type);

    switch (event->md_type) {
    case META_TYPE_INTERFACE:
        md_neat_handle_iface_event(mwn, (struct md_iface_event*) event);
        break;
    default:
        return;
    }
}

void md_neat_usage()
{
    fprintf(stderr, "\"neat\": {\t\tNEAT writer.\n");
    fprintf(stderr, "  \"cib_prefix\":\tpath and prefix for CIB files (e.g. /var/lib/neat/cib/md-\n");
    fprintf(stderr, "  \"cib_socket\":\tpath to NEAT policy manager unix socket that accepts CIB data (e.g. /var/lib/neat/neat_cib_socket)\n");
    fprintf(stderr, "}\n");
}

void md_neat_setup(struct md_exporter *mde, struct md_writer_neat *mwn)
{
    mwn->parent = mde;
    mwn->init = md_neat_init;
    mwn->handle = md_neat_handle;
}
