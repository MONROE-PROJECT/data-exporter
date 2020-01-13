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
#include <sys/time.h>

#include JSON_LOC

#include "lib/minmea.h"

#define CONFIG_MAX_SIZE 8092

#define OUTPUT_FORMAT_BUF_SIZE 20

#define MDE_VERSION 1
#define METADATA_NL_GROUP 0x03

#define MD_INPUT_MAX (__MD_INPUT_MAX - 1)
#define MD_WRITER_MAX (__MD_WRITER_MAX - 1)

#define META_TYPE_INTERFACE  0x01
#define META_TYPE_CONNECTION 0x02
#define META_TYPE_POS        0x04
#define META_TYPE_MUNIN      0x05
#define META_TYPE_SYSEVENT   0x06
#define META_TYPE_RADIO      0x08
#define META_TYPE_SYSTEM     0x10
#define META_TYPE_ZEROMQ     0xA

#define DEFAULT_RSSI       -127
#define DEFAULT_RSRQ          0
#define DEFAULT_RSRP          0
#define DEFAULT_LTE_BAND      0
#define DEFAULT_LTE_FREQ      0
#define DEFAULT_LTE_PCI      -1
#define DEFAULT_ENODEBID     -1
#define DEFAULT_ECIO          0
#define DEFAULT_RSCP          0
#define DEFAULT_MCCMNC        0
#define DEFAULT_LAC          -1
#define DEFAULT_CID          -1
#define DEFAULT_MODE         -1
#define DEFAULT_SUBMODE      -1
#define DEFAULT_DEVICE_STATE -1

#define DEFAULT_TX_POWER      0
#define DEFAULT_IOT_EARFCN    0
#define DEFAULT_IOT_CELEVEL   0

enum iface_event {
    IFACE_EVENT_DEV_STATE=1,
    IFACE_EVENT_MODE_CHANGE,
    IFACE_EVENT_SIGNAL_CHANGE,
    IFACE_EVENT_LTE_BAND_CHANGE,
    IFACE_EVENT_ISP_NAME_CHANGE,
    IFACE_EVENT_UPDATE,
    IFACE_EVENT_IP_ADDR_CHANGE,
    IFACE_EVENT_LOC_CHANGE,
    IFACE_EVENT_NW_MCCMNC_CHANGE,
    IFACE_EVENT_NSM,
    IFACE_EVENT_SYSTEM,
    IFACE_EVENT_LTE_CA,
    IFACE_EVENT_EARFCN_CHANGE,
    IFACE_EVENT_CELEVEL_CHANGE,
    IFACE_EVENT_TX_POWER_CHANGE
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
    CONN_EVENT_META_QUALITY_UPDATE,
    CONN_EVENT_DATA_USAGE_UPDATE
};

enum interface_types {
    INTERFACE_UNKNOWN=0,
    INTERFACE_MODEM,
    INTERFACE_PHONE,
    INTERFACE_USB_LAN,
    INTERFACE_WAN,
    INTERFACE_WIFI
};

enum radio_event {
    RADIO_EVENT_GSM_RR_CIPHER_MODE = 1,
    RADIO_EVENT_GSM_RR_CHANNEL_CONF,
    RADIO_EVENT_CELL_LOCATION_GERAN,
    RADIO_EVENT_GSM_RR_CELL_SEL_RESEL_PARAM,
    RADIO_EVENT_GRR_CELL_RESEL,
    RADIO_EVENT_WCDMA_RRC_STATE,
    RADIO_EVENT_WCDMA_CELL_ID
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
    MD_INPUT_ZEROMQ,
    MD_INPUT_ZEROMQ_RELAY,
    __MD_INPUT_MAX
};

enum md_writers {
    MD_WRITER_SQLITE,
    MD_WRITER_ZEROMQ,
    MD_WRITER_NNE,
    MD_WRITER_NEAT,
    MD_WRITER_FILE,
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

#define MD_RADIO_EVENT \
    MD_EVENT; \
    uint8_t event_param; \
    const char *iccid; \
    const char *imsi; \
    const char *imei;

struct mnl_socket;
struct backend_event_loop;
struct backend_epoll_handle;
struct backend_timeout_handle;
struct md_input;
struct md_writer;
struct md_event;

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
    const char *ca_info;
    uint32_t imsi_mccmnc;
    uint32_t nw_mccmnc;
    int32_t cid;
    int32_t enodeb_id;
    int16_t rscp;
    int16_t lte_rsrp;
    uint16_t lte_freq;
    uint16_t lac;
    uint16_t lte_pci;
    uint16_t tx_power;
    uint16_t iot_earfcn;
    uint16_t iot_celevel;
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

typedef struct md_iface_event md_system_event_t;

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
    uint64_t rx_bytes;
    uint64_t tx_bytes;
    const char *interface_id;
    const char *imei;
    const char *imsi;
    const char *interface_name;
    uint32_t network_provider;
    const char *network_address;
    uint8_t has_ip;
    uint8_t connectivity;
    uint8_t connection_mode;
    uint8_t quality;
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

struct md_zeromq_event {
    MD_EVENT;
    const char *msg;
};

struct md_radio_event {
    MD_RADIO_EVENT;
};

struct md_radio_cell_loc_geran_event {
    MD_RADIO_EVENT;
    uint32_t cell_id;
    const char *plmn;
    uint16_t lac;
    uint16_t arfcn;
    uint8_t bsic;
    uint32_t timing_advance;
    uint16_t rx_lev;
    const char *cell_geran_info_nmr;
};

struct md_radio_grr_cell_resel_event {
    MD_RADIO_EVENT;
    uint16_t serving_bcch_arfcn;
    uint16_t serving_pbcch_arfcn;
    uint32_t serving_c1;
    uint32_t serving_c2;
    uint32_t serving_c31;
    uint32_t serving_c32;
    const char *neighbors;
    uint8_t serving_priority_class;
    uint8_t serving_rxlev_avg;
    uint8_t serving_five_second_timer;
    uint8_t cell_reselect_status;
    uint8_t recent_cell_selection;
};

struct md_radio_gsm_rr_cell_sel_reset_param_event {
    MD_RADIO_EVENT;
    uint8_t cell_reselect_hysteresis;
    uint8_t ms_txpwr_max_cch;
    uint8_t rxlev_access_min;
    uint8_t power_offset_valid;
    uint8_t power_offset;
    uint8_t neci;
    uint8_t acs;
    uint8_t opt_reselect_param_ind;
    uint8_t cell_bar_qualify;
    uint8_t cell_reselect_offset;
    uint8_t temporary_offset;
    uint8_t penalty_time;
};

struct md_radio_gsm_rr_cipher_mode_event {
    MD_RADIO_EVENT;
    uint8_t ciphering_state;
    uint8_t ciphering_algorithm;
};

struct md_radio_gsm_rr_channel_conf_event {
    MD_RADIO_EVENT;
    uint8_t num_ded_chans;
    uint8_t dtx_indicator;
    uint8_t power_level;
    uint8_t starting_time_valid;
    uint16_t starting_time;
    uint8_t cipher_flag;
    uint8_t cipher_algorithm;
    const char *after_channel_config;
    const char *before_channel_config;
    uint8_t channel_mode_1;
    uint8_t channel_mode_2;
};

struct md_radio_wcdma_rrc_state_event {
    MD_RADIO_EVENT;
    uint8_t rrc_state;
};

struct md_radio_wcdma_cell_id_event {
    MD_RADIO_EVENT;
    uint32_t ul_uarfcn;
    uint32_t dl_uarfcn;
    uint32_t cell_id;
    uint16_t ura_id;
    uint8_t cell_access_rest;
    uint8_t call_accs;
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
