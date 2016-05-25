/* Copyright (c) 2015, Celerway, Kristian Evensen <kristrev@celerway.com>
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

#ifndef METADATA_EXPORTER_H
#define METADATA_EXPORTER_H

#include <pthread.h>
#include <stdint.h>

#define INCLUDE_JSON(M) <JSON_LOC/M>
#include INCLUDE_JSON(json.h)
#include INCLUDE_JSON(printbuf.h)

#include "lib/minmea.h"

#define CONFIG_MAX_SIZE 8092

#define MDE_VERSION 1
#define METADATA_NL_GROUP 0x03

#define MD_INPUT_MAX (__MD_INPUT_MAX - 1)
#define MD_WRITER_MAX (__MD_WRITER_MAX - 1)

#define META_IFACE_INVALID   0x81

#define META_TYPE_INTERFACE  0x01
#define META_TYPE_CONNECTION 0x02
#define META_TYPE_POS        0x04
#define META_TYPE_MUNIN      0x05
#define META_TYPE_SYSEVENT   0x06

enum iface_event {
    IFACE_EVENT_DEV_STATE=1,
    IFACE_EVENT_MODE_CHANGE,
    IFACE_EVENT_SIGNAL_CHANGE,
    IFACE_EVENT_LTE_BAND_CHANGE,
    IFACE_EVENT_ISP_NAME_CHANGE,
    IFACE_EVENT_UPDATE,
    IFACE_EVENT_IP_ADDR_CHANGE,
    IFACE_EVENT_LOC_CHANGE,
    IFACE_EVENT_NW_MCCMNC_CHANGE
};

enum conn_event {
    CONN_EVENT_L3_UP=1,
    CONN_EVENT_L3_DOWN,
    CONN_EVENT_L4_UP,
    CONN_EVENT_L4_DOWN,
    CONN_EVENT_MODE_CHANGE,
    CONN_EVENT_QUALITY_CHANGE,
    CONN_EVENT_META_UPDATE,
    CONN_EVENT_META_MODE_UPDATE,
    CONN_EVENT_META_QUALITY_UPDATE
};

enum interface_types {
    INTERFACE_UNKNOWN=0,
    INTERFACE_MODEM,
    INTERFACE_PHONE,
    INTERFACE_USB_LAN,
    INTERFACE_WAN,
    INTERFACE_WIFI
};

#define EVENT_STR_LEN 255

#define RETVAL_SUCCESS  0
#define RETVAL_FAILURE  1
#define RETVAL_IGNORE   2

enum md_inputs {
    MD_INPUT_NETLINK,
    MD_INPUT_GPSD,
    MD_INPUT_GPS_NSB,
    MD_INPUT_MUNIN,
    MD_INPUT_SYSEVENT,
    __MD_INPUT_MAX
};

enum md_writers {
    MD_WRITER_SQLITE,
    MD_WRITER_ZEROMQ,
    MD_WRITER_NNE,
    __MD_WRITER_MAX
};

#define MD_INPUT \
    struct md_exporter *parent; \
    uint8_t (*init)(void *ptr, json_object* config); \
    void (*destroy)(void *ptr); \

#define MD_WRITER \
    struct md_exporter *parent; \
    int32_t (*init)(void *ptr, json_object* config); \
    void (*handle)(struct md_writer *writer, struct md_event *event); \
    void (*itr_cb)(void *ptr); \
    void (*usage)()

#define MD_EVENT \
    uint64_t tstamp; \
    uint32_t md_type; \
    uint16_t sequence

struct mnl_socket;
struct backend_event_loop;
struct backend_epoll_handle;
struct backend_timeout_handle;
struct md_input;
struct md_writer;
struct md_event;
struct timeval;

//TODO: Maybe moved this to some shared header file?
struct md_iface_event {
    MD_EVENT;
    const char *iccid;
    const char *imsi;
    const char *imei;
    const char *isp_name;
    const char *ip_addr;
    const char *internal_ip_addr;
    const char *ifname;
    uint32_t imsi_mccmnc;
    uint32_t nw_mccmnc;
    int32_t cid;
    int32_t enodeb_id;
    int16_t rscp;
    int16_t lte_rsrp;
    uint16_t lte_freq;
    uint16_t lac;
    uint16_t lte_pci;
    int8_t rssi;
    int8_t ecio;
    int8_t lte_rssi;
    int8_t lte_rsrq;
    uint8_t device_mode;
    uint8_t device_submode;
    uint8_t lte_band;
    uint8_t device_state;
    uint8_t event_param;
    uint8_t event_type;
};

struct md_conn_event {
    MD_EVENT;
    uint8_t event_type;
    uint8_t event_param;
    uint8_t event_value;
    uint8_t interface_type;
    uint8_t network_address_family;
    uint8_t interface_id_type;
    uint8_t network_provider_type;
    int8_t signal_strength;
    uint64_t l3_session_id;
    uint64_t l4_session_id;
    const char *interface_id;
    const char *imei;
    const char *interface_name;
    uint32_t network_provider;
    const char *network_address;
    const char *event_value_str;
};

struct md_gps_event {
    MD_EVENT;
    struct timeval tstamp_tv;
    const char *nmea_raw;
    struct minmea_time time;
    float latitude;
    float longitude;
    float speed;
    float altitude;
    int satellites_tracked;
    uint8_t minmea_id;
};

struct md_munin_event {
    MD_EVENT;
    json_object* json_blob;
};
#define md_sysevent md_munin_event

struct md_exporter {
    struct mnl_socket *metadata_sock;
    struct backend_event_loop *event_loop;
    struct backend_epoll_handle *event_handle;
    FILE *logfile;

    struct md_input *md_inputs[MD_INPUT_MAX + 1];
    struct md_writer *md_writers[MD_WRITER_MAX + 1];
    struct md_conn_event *mce;

    //Keep track of order in which events arrived at metadata exporter. There
    //could also be a per-app sequence number
    uint16_t seq;
    uint8_t use_syslog;
};

struct md_input {
    MD_INPUT;
};

struct md_writer {
    MD_WRITER;
};

struct md_event {
    MD_EVENT;
};

void mde_start_timer(struct backend_event_loop *event_loop,
                     struct backend_timeout_handle *timeout_handle,
                     uint32_t timeout);

void mde_publish_event_obj(struct md_exporter *mde, struct md_event *event);

uint16_t mde_inc_seq(struct md_exporter *mde);
#endif
