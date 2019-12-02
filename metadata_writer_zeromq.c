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

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <zmq.h>
#include JSON_LOC
#include <getopt.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <getopt.h>

#include "lib/minmea.h"
#include "metadata_exporter.h"
#include "metadata_writer_zeromq.h"
#include "system_helpers.h"
#include "metadata_utils.h"
#include "metadata_exporter_log.h"
#include "backend_event_loop.h"

static double override_tstamp() {
    /* return a current export timestamp with us precision, as the 
       event timestamp with sec resolution is not deemed useful.   */
    struct timeval tv;
    gettimeofday(&tv, NULL);

    return (double) tv.tv_sec + (double) (tv.tv_usec / 1000000.0); 
}


static json_object *md_zeromq_writer_create_json_string(json_object *obj,
        const char *key, const char *value)
{
    if (value == NULL) return NULL;

    struct json_object *obj_add = json_object_new_string(value);

    if (!obj_add)
        return NULL;

    json_object_object_add(obj, key, obj_add);
    return obj;
}

static json_object *md_zeromq_writer_create_json_int(json_object *obj, const char *key,
        int value)
{
    struct json_object *obj_add = json_object_new_int(value);

    if (!obj_add)
        return NULL;

    json_object_object_add(obj, key, obj_add);
    return obj;
}

static json_object *md_zeromq_writer_create_json_int64(json_object *obj,
        const char *key, int64_t value)
{
    struct json_object *obj_add = json_object_new_int64(value);

    if (!obj_add)
        return NULL;

    json_object_object_add(obj, key, obj_add);
    return obj;
}

static json_object *md_zeromq_writer_create_json_double(json_object *obj,
        const char *key, double value)
{
    struct json_object *obj_add = NULL;
    if (!isnan(value)) {   
        obj_add = json_object_new_double(value);
        if (!obj_add)
	    return NULL;
    } // but if value is NaN (not valid in JSON), add a NULL object

    json_object_object_add(obj, key, obj_add);
    return obj;
}

static uint8_t md_zeromq_writer_add_default_fields(const struct md_writer_zeromq *mwz,
        struct json_object* obj, int seq, int64_t tstamp, const char* dataid) {
    if (!md_zeromq_writer_create_json_int(obj, mwz->keys[MD_ZMQ_KEY_SEQ],
            seq)) {
      return 0;
    } 
    if ((mwz->metadata_project == MD_PROJECT_NNE) && 
        !md_zeromq_writer_create_json_int64(obj, mwz->keys[MD_ZMQ_KEY_TSTAMP],
            tstamp)) {
      return 0;
    } else  
    if ((mwz->metadata_project == MD_PROJECT_MNR) && 
        !md_zeromq_writer_create_json_double(obj, mwz->keys[MD_ZMQ_KEY_TSTAMP],
            override_tstamp())) {
      return 0;
    }
    if ((mwz->keys[MD_ZMQ_KEY_DATAVERSION] &&
         !md_zeromq_writer_create_json_int(obj, mwz->keys[MD_ZMQ_KEY_DATAVERSION],
            MD_ZMQ_DATA_VERSION)) ||
        (mwz->keys[MD_ZMQ_KEY_DATAID] &&
         !md_zeromq_writer_create_json_string(obj, mwz->keys[MD_ZMQ_KEY_DATAID],
             dataid))){
        return 0;
    }

    return 1;
}

static json_object* md_zeromq_writer_create_json_gps(struct md_writer_zeromq *mwz,
                                              struct md_gps_event *mge)
{
    struct json_object *obj = NULL;

    if (!(obj = json_object_new_object()))
        return NULL;


    if (!md_zeromq_writer_add_default_fields(mwz, obj, mge->sequence,
            mge->tstamp_tv.tv_sec, mwz->topics[MD_ZMQ_TOPIC_GPS])) {
        json_object_put(obj);
        return NULL;
    }

    if (!md_zeromq_writer_create_json_double(obj, mwz->keys[MD_ZMQ_KEY_LATITUDE],
            mge->latitude) ||
        !md_zeromq_writer_create_json_double(obj, mwz->keys[MD_ZMQ_KEY_LONGITUDE],
            mge->longitude) ||
        (mge->speed && !md_zeromq_writer_create_json_double(obj,
            mwz->keys[MD_ZMQ_KEY_SPEED], mge->speed)) ||
        (mge->altitude && !md_zeromq_writer_create_json_double(obj,
            mwz->keys[MD_ZMQ_KEY_ALTITUDE], mge->altitude)) ||
        (mge->satellites_tracked && !md_zeromq_writer_create_json_int(obj,
            mwz->keys[MD_ZMQ_KEY_NUMSAT], mge->satellites_tracked)) ||
        (mge->nmea_raw && !md_zeromq_writer_create_json_string(obj,
            mwz->keys[MD_ZMQ_KEY_NMEA], mge->nmea_raw))) {
        json_object_put(obj);
        return NULL;
    }

    return obj;
}

static void md_zeromq_writer_handle_gps(struct md_writer_zeromq *mwz,
                                 struct md_gps_event *mge)
{
    char topic[8192];
    struct json_object *gps_obj = md_zeromq_writer_create_json_gps(mwz, mge);
    int retval;

    if (gps_obj == NULL) {
        META_PRINT_SYSLOG(mwz->parent, LOG_ERR, "Failed to create GPS ZMQ JSON\n");
        return;
    }

    char* suffix="";
    if (mge->nmea_raw) {
    	if (strncmp(mge->nmea_raw, "$GPGGA", 6)==0) {
    	    suffix = ".GPGGA";
    	} else if (strncmp(mge->nmea_raw, "$GPRMC", 6)==0) {
          suffix = ".GPRMC";
      }
    }

    retval = snprintf(topic, sizeof(topic), "%s%s %s", mwz->topics[MD_ZMQ_TOPIC_GPS],
            suffix,
            json_object_to_json_string_ext(gps_obj, JSON_C_TO_STRING_PLAIN));

    if (retval < sizeof(topic)) {
        zmq_send(mwz->zmq_publisher, topic, strlen(topic), 0);
    }
    json_object_put(gps_obj);
}

static void md_zeromq_writer_handle_munin(struct md_writer_zeromq *mwz,
                                   struct md_munin_event *mge)
{
    char topic[8192];

    int retval;

    json_object_object_foreach(mge->json_blob, key, val) {
        md_zeromq_writer_add_default_fields(mwz, val, mge->sequence, mge->tstamp, mwz->topics[MD_ZMQ_TOPIC_SENSOR]);

        retval = snprintf(topic, sizeof(topic), "%s.%s %s",
                mwz->topics[MD_ZMQ_TOPIC_SENSOR],
                key, json_object_to_json_string_ext(val, JSON_C_TO_STRING_PLAIN));
        if (retval < sizeof(topic)) {
            zmq_send(mwz->zmq_publisher, topic, strlen(topic), 0);
        }
    }
}

static void md_zeromq_writer_handle_zeromq(struct md_writer_zeromq *mwz,
                                   struct md_zeromq_event *mge)
{
    char topic[8192];  //In reality this is the message not the topic
    int retval;

    retval = snprintf(topic, sizeof(topic), "%s", mge->msg);
    if (retval < sizeof(topic)) {
        zmq_send(mwz->zmq_publisher, topic, strlen(topic), 0);
    }
}


static void md_zeromq_writer_handle_sysevent(struct md_writer_zeromq *mwz,
                                   struct md_sysevent *mge)
{
    char topic[8192];
    int retval;

    md_zeromq_writer_add_default_fields(mwz, mge->json_blob, mge->sequence,
        mge->tstamp, mwz->topics[MD_ZMQ_TOPIC_SYSEVENT]);

    retval = snprintf(topic, sizeof(topic), "%s %s",
            mwz->topics[MD_ZMQ_TOPIC_SYSEVENT],
            json_object_to_json_string_ext(mge->json_blob,
                JSON_C_TO_STRING_PLAIN));
    if (retval < sizeof(topic)) {
        zmq_send(mwz->zmq_publisher, topic, strlen(topic), 0);
    }
}


static json_object* md_zeromq_writer_create_json_modem_default(struct md_writer_zeromq *mwz,
                                                        struct md_conn_event *mce)
{
    struct json_object *obj = NULL, *obj_add = NULL;

    if (!(obj = json_object_new_object()))
        return NULL;

    if (!md_zeromq_writer_add_default_fields(mwz, obj, mce->sequence, mce->tstamp,
            mwz->topics[MD_ZMQ_TOPIC_CONNECTIVITY])) {
        json_object_put(obj);
        return NULL;
    }

    if (!(obj_add = json_object_new_string(mce->interface_id))) {
        json_object_put(obj);
        return NULL;
    }
    json_object_object_add(obj, mwz->keys[MD_ZMQ_KEY_INTERFACEID], obj_add);

    if (!(obj_add = json_object_new_string(mce->interface_name))) {
        json_object_put(obj);
        return NULL;
    }
    json_object_object_add(obj, mwz->keys[MD_ZMQ_KEY_INTERFACENAME], obj_add);

    if (!(obj_add = json_object_new_int(mce->network_provider))) {
        json_object_put(obj);
        return NULL;
    }
    json_object_object_add(obj, mwz->keys[MD_ZMQ_KEY_OPERATOR], obj_add);

    return obj;
}

static void md_zeromq_writer_handle_conn(struct md_writer_zeromq *mwz,
                                   struct md_conn_event *mce)
{
    struct json_object *json_obj, *obj_add;
    uint8_t mode;
    char topic[8192];
    int retval;

    if ((mce->event_param != CONN_EVENT_MODE_CHANGE &&
         mce->event_param != CONN_EVENT_META_UPDATE) ||
         mce->interface_type != INTERFACE_MODEM)
        return;

    if (mce->event_param == CONN_EVENT_MODE_CHANGE) {
        mode = mce->event_value;
    } else {
        mode = mce->connection_mode;
    }

    json_obj = md_zeromq_writer_create_json_modem_default(mwz, mce);

    if (!(obj_add = json_object_new_int(mode))) {
        json_object_put(json_obj);
        return;
    }
    json_object_object_add(json_obj, mwz->keys[MD_ZMQ_KEY_MODE], obj_add);

    if (mce->event_param == CONN_EVENT_META_UPDATE &&
        mce->signal_strength != -127) {
        obj_add = json_object_new_int(mce->signal_strength);

        if (!obj_add) {
            json_object_put(json_obj);
            return;
        } else {
            json_object_object_add(json_obj, mwz->keys[MD_ZMQ_KEY_SIGNAL],
                    obj_add);
        }
    }

    if (mce->event_param != CONN_EVENT_META_UPDATE)
        return;

    retval = snprintf(topic, sizeof(topic), "%s.%s %s",
            mwz->topics[MD_ZMQ_TOPIC_CONNECTIVITY],
            mce->interface_id,
            json_object_to_json_string_ext(json_obj, JSON_C_TO_STRING_PLAIN));

    if (retval < sizeof(topic))
        zmq_send(mwz->zmq_publisher, topic, strlen(topic), 0);

    json_object_put(json_obj);
}


char imei_mapping [10][30];
int   maplen=0;
static int map_imei(const char* imei, const struct md_writer_zeromq *mwz) {
    for (int i=0;i<maplen;i++) {
        if (strcmp(imei_mapping[i], imei)==0) {
            return i;
        }
    }
    FILE *fp = fopen("/tmp/interfaces", "r");
    if (fp==NULL) return -1;
    char* line = NULL;
    int nr=0;
    int match=-1;
    size_t len = 0;
    ssize_t read;
    while ((read = getline(&line, &len, fp)) != -1) {
        if(len>29) len=29;
        strncpy(imei_mapping[nr], line, len);
        imei_mapping[nr][strlen(imei_mapping[nr])-1]='\0';

        if (strcmp(imei_mapping[nr], imei)==0) match=nr;
        if (++nr==10) break;
    }
    maplen=nr;
    fclose(fp);
    if (line) free(line);
    return match;
}

static json_object *md_zeromq_writer_create_iface_json(const struct md_writer_zeromq *mwz,
        struct md_iface_event *mie)
{
    struct json_object *obj = NULL;

    if (!(obj = json_object_new_object()))
        return NULL;

    if (!md_zeromq_writer_add_default_fields(mwz, obj, mie->sequence, mie->tstamp,
            mwz->topics[MD_ZMQ_TOPIC_MODEM])) {
        json_object_put(obj);
        return NULL;
    }

    if (mwz->metadata_project == MD_PROJECT_MNR) {
        if (mie->ifname && mie->imei) {
            int iifindex=map_imei(mie->imei, mwz);
            if (iifindex > -1) {
                char iifname[4]="opX";
                iifname[2]=iifindex + '0';
                if (!md_zeromq_writer_create_json_string(obj, 
                    mwz->keys[MD_ZMQ_KEY_MONROE_IIF_NAME], iifname)) {
                    json_object_put(obj);
                    return NULL;
               }
            }
        }
    }

    if (!md_zeromq_writer_create_json_string(obj, mwz->keys[MD_ZMQ_KEY_ICCID],
            mie->iccid) ||
        !md_zeromq_writer_create_json_string(obj, mwz->keys[MD_ZMQ_KEY_IMSI],
            mie->imsi) ||
        !md_zeromq_writer_create_json_string(obj, mwz->keys[MD_ZMQ_KEY_IMEI],
            mie->imei) ||
        (mie->isp_name && !md_zeromq_writer_create_json_string(obj,
                mwz->keys[MD_ZMQ_KEY_ISP_NAME], mie->isp_name)) ||
        (mie->ip_addr && !md_zeromq_writer_create_json_string(obj,
                mwz->keys[MD_ZMQ_KEY_IP_ADDR], mie->ip_addr)) ||
        (mie->internal_ip_addr && !md_zeromq_writer_create_json_string(obj,
                mwz->keys[MD_ZMQ_KEY_INTERNAL_IP_ADDR],
                mie->internal_ip_addr)) ||
        (mie->ifname && !md_zeromq_writer_create_json_string(obj,
                mwz->keys[MD_ZMQ_KEY_IF_NAME], mie->ifname)) ||
        (mie->imsi_mccmnc &&
            !md_zeromq_writer_create_json_int64(obj, mwz->keys[MD_ZMQ_KEY_IMSI_MCCMNC],
                mie->imsi_mccmnc)) ||
        (mie->nw_mccmnc &&
            !md_zeromq_writer_create_json_int64(obj, mwz->keys[MD_ZMQ_KEY_NW_MCCMNC],
                mie->nw_mccmnc)) ||
        ((mie->cid != DEFAULT_CID && mie->lac != DEFAULT_LAC) &&
            (!md_zeromq_writer_create_json_int(obj, mwz->keys[MD_ZMQ_KEY_LAC],
                                        mie->lac) ||
             !md_zeromq_writer_create_json_int(obj, mwz->keys[MD_ZMQ_KEY_CID],
                 mie->cid))) ||
        (mie->rscp != DEFAULT_RSCP &&
            !md_zeromq_writer_create_json_int(obj, mwz->keys[MD_ZMQ_KEY_RSCP],
                mie->rscp)) ||
        (mie->lte_rsrp != DEFAULT_RSRP &&
            !md_zeromq_writer_create_json_int(obj, mwz->keys[MD_ZMQ_KEY_LTE_RSRP],
                mie->lte_rsrp)) ||
        (mie->lte_freq != DEFAULT_LTE_FREQ &&
            !md_zeromq_writer_create_json_int(obj, mwz->keys[MD_ZMQ_KEY_LTE_FREQ],
                mie->lte_freq)) ||
        (mie->rssi != DEFAULT_RSSI &&
            !md_zeromq_writer_create_json_int(obj, mwz->keys[MD_ZMQ_KEY_RSSI],
                mie->rssi)) ||
        (mie->ecio != DEFAULT_ECIO &&
            !md_zeromq_writer_create_json_int(obj, mwz->keys[MD_ZMQ_KEY_ECIO],
                mie->ecio)) ||
        (mie->lte_rssi != DEFAULT_RSSI &&
            !md_zeromq_writer_create_json_int(obj, mwz->keys[MD_ZMQ_KEY_LTE_RSSI],
                mie->lte_rssi)) ||
        (mie->lte_rsrq != DEFAULT_RSRQ &&
            !md_zeromq_writer_create_json_int(obj, mwz->keys[MD_ZMQ_KEY_LTE_RSRQ],
                mie->lte_rsrq)) ||
        (mie->device_mode != DEFAULT_MODE && !md_zeromq_writer_create_json_int(obj,
                mwz->keys[MD_ZMQ_KEY_DEVICE_MODE], mie->device_mode)) ||
        (mie->device_submode != DEFAULT_SUBMODE && !md_zeromq_writer_create_json_int(obj,
                mwz->keys[MD_ZMQ_KEY_DEVICE_SUBMODE], mie->device_submode)) ||
        (mie->lte_band != DEFAULT_LTE_BAND && !md_zeromq_writer_create_json_int(obj,
                mwz->keys[MD_ZMQ_KEY_LTE_BAND], mie->lte_band)) ||
        (mie->device_state != DEFAULT_DEVICE_STATE && !md_zeromq_writer_create_json_int(obj,
                mwz->keys[MD_ZMQ_KEY_DEVICE_STATE], mie->device_state)) ||
        (mie->lte_pci != DEFAULT_LTE_PCI &&
            !md_zeromq_writer_create_json_int(obj,
                mwz->keys[MD_ZMQ_KEY_LTE_PCI], mie->lte_pci)) ||
        (mie->tx_power != DEFAULT_TX_POWER &&
            !md_zeromq_writer_create_json_int(obj,
                mwz->keys[MD_ZMQ_KEY_TX_POWER], mie->tx_power)) ||
        (mie->iot_earfcn != DEFAULT_IOT_EARCFN &&
            !md_zeromq_writer_create_json_int(obj,
                mwz->keys[MD_ZMQ_KEY_IOT_EARCFN], mie->iot_earfcn)) ||
        (mie->iot_ecl != DEFAULT_IOT_ECL &&
            !md_zeromq_writer_create_json_int(obj,
                mwz->keys[MD_ZMQ_KEY_IOT_ECL], mie->iot_ecl)) ||
        (mie->iot_snr != DEFAULT_IOT_SNR &&
            !md_zeromq_writer_create_json_int(obj,
                mwz->keys[MD_ZMQ_KEY_IOT_SNR], mie->iot_snr)) ||
        (mie->enodeb_id != DEFAULT_ENODEBID &&
            !md_zeromq_writer_create_json_int(obj,
                mwz->keys[MD_ZMQ_KEY_ENODEB_ID], mie->enodeb_id))) {
        json_object_put(obj);
        return NULL;
    }

    return obj;
}

static void md_zeromq_writer_handle_iface(struct md_writer_zeromq *mwz,
                                   struct md_iface_event *mie)
{
    struct json_object *json_obj = md_zeromq_writer_create_iface_json(mwz, mie);
    char topic[8192] = {0};
    int retval = 0;

    if (json_obj == NULL)
        return;

    //Switch on topic
    switch (mie->event_param) {
    case IFACE_EVENT_DEV_STATE:
        retval = snprintf(topic, sizeof(topic), "%s.%s.%s %s", 
                mwz->topics[MD_ZMQ_TOPIC_MODEM],
                mie->iccid,
                mwz->topics[MD_ZMQ_TOPIC_MODEM_STATE],
                json_object_to_json_string_ext(json_obj, JSON_C_TO_STRING_PLAIN));
        break;
    case IFACE_EVENT_MODE_CHANGE:
        retval = snprintf(topic, sizeof(topic), "%s.%s.%s %s",
                mwz->topics[MD_ZMQ_TOPIC_MODEM],
                mie->iccid,
                mwz->topics[MD_ZMQ_TOPIC_MODEM_MODE],
                json_object_to_json_string_ext(json_obj, JSON_C_TO_STRING_PLAIN));
        break;
    case IFACE_EVENT_SIGNAL_CHANGE:
        retval = snprintf(topic, sizeof(topic), "%s.%s.%s %s",
                mwz->topics[MD_ZMQ_TOPIC_MODEM],
                mie->iccid,
                mwz->topics[MD_ZMQ_TOPIC_MODEM_SIGNAL],
                json_object_to_json_string_ext(json_obj, JSON_C_TO_STRING_PLAIN));
        break;
    case IFACE_EVENT_LTE_BAND_CHANGE:
        retval = snprintf(topic, sizeof(topic), "%s.%s.%s %s",
                mwz->topics[MD_ZMQ_TOPIC_MODEM],
                mie->iccid,
                mwz->topics[MD_ZMQ_TOPIC_MODEM_LTE_BAND],
                json_object_to_json_string_ext(json_obj, JSON_C_TO_STRING_PLAIN));
        break;
    case IFACE_EVENT_ISP_NAME_CHANGE:
        retval = snprintf(topic, sizeof(topic), "%s.%s.%s %s",
                mwz->topics[MD_ZMQ_TOPIC_MODEM],
                mie->iccid,
                mwz->topics[MD_ZMQ_TOPIC_MODEM_ISP_NAME],
                json_object_to_json_string_ext(json_obj, JSON_C_TO_STRING_PLAIN));
        break;
    case IFACE_EVENT_UPDATE:
        retval = snprintf(topic, sizeof(topic), "%s.%s.%s %s",
                mwz->topics[MD_ZMQ_TOPIC_MODEM],
                mie->iccid,
                mwz->topics[MD_ZMQ_TOPIC_MODEM_UPDATE],
                json_object_to_json_string_ext(json_obj, JSON_C_TO_STRING_PLAIN));
        break;
    case IFACE_EVENT_IP_ADDR_CHANGE:
        retval = snprintf(topic, sizeof(topic), "%s.%s.%s %s",
                mwz->topics[MD_ZMQ_TOPIC_MODEM],
                mie->iccid,
                mwz->topics[MD_ZMQ_TOPIC_MODEM_IP_ADDR],
                json_object_to_json_string_ext(json_obj, JSON_C_TO_STRING_PLAIN));
        break;
    case IFACE_EVENT_LOC_CHANGE:
        retval = snprintf(topic, sizeof(topic), "%s.%s.%s %s",
                mwz->topics[MD_ZMQ_TOPIC_MODEM],
                mie->iccid,
                mwz->topics[MD_ZMQ_TOPIC_MODEM_LOC_CHANGE],
                json_object_to_json_string_ext(json_obj, JSON_C_TO_STRING_PLAIN));
        break;
    case IFACE_EVENT_NW_MCCMNC_CHANGE:
        retval = snprintf(topic, sizeof(topic), "%s.%s.%s %s",
                mwz->topics[MD_ZMQ_TOPIC_MODEM],
                mie->iccid,
                mwz->topics[MD_ZMQ_TOPIC_MODEM_NW_MCCMNC_CHANGE],
                json_object_to_json_string_ext(json_obj, JSON_C_TO_STRING_PLAIN));
        break;

    default:
        json_object_put(json_obj);
        return;
    }

    if (retval >= sizeof(topic)) {
        json_object_put(json_obj);
        return;
    }

    zmq_send(mwz->zmq_publisher, topic, strlen(topic), 0);
    json_object_put(json_obj);
}

static json_object *md_zeromq_writer_handle_radio_cell_loc_gerant(
        struct md_writer_zeromq *mwz,
        struct md_radio_cell_loc_geran_event *event)
{
    struct json_object *obj, *geran_info;

    if (!(obj = json_object_new_object()))
        return NULL;

    if (!md_zeromq_writer_add_default_fields(mwz, obj, event->sequence,
            event->tstamp, mwz->topics[MD_ZMQ_TOPIC_RADIO_CELL_LOCATION_GERAN])) {
        json_object_put(obj);
        return NULL;
    }

    if (event->iccid &&
        !md_zeromq_writer_create_json_string(obj, mwz->keys[MD_ZMQ_KEY_ICCID],
            event->iccid)) {
        json_object_put(obj);
        return NULL;
    }

    if (event->imsi &&
        !md_zeromq_writer_create_json_string(obj, mwz->keys[MD_ZMQ_KEY_IMSI],
            event->imsi)) {
        json_object_put(obj);
        return NULL;
    }

    if (event->imei &&
        !md_zeromq_writer_create_json_string(obj, mwz->keys[MD_ZMQ_KEY_IMEI],
            event->imei)) {
        json_object_put(obj);
        return NULL;
    }

    if (!md_zeromq_writer_create_json_int64(obj, mwz->keys[MD_ZMQ_KEY_RADIO_CELL_ID],
            event->cell_id) ||
        !md_zeromq_writer_create_json_string(obj, mwz->keys[MD_ZMQ_KEY_RADIO_PLMN],
            event->plmn) ||
        !md_zeromq_writer_create_json_int(obj, mwz->keys[MD_ZMQ_KEY_RADIO_LAC],
            event->lac) ||
        !md_zeromq_writer_create_json_int(obj, mwz->keys[MD_ZMQ_KEY_RADIO_ARFCN],
            event->arfcn) ||
        !md_zeromq_writer_create_json_int(obj, mwz->keys[MD_ZMQ_KEY_RADIO_BSIC],
            event->bsic) ||
        !md_zeromq_writer_create_json_int64(obj,
            mwz->keys[MD_ZMQ_KEY_RADIO_TIMING_ADVANCE],
            event->timing_advance) ||
        !md_zeromq_writer_create_json_int(obj, mwz->keys[MD_ZMQ_KEY_RADIO_RX_LEV],
            event->rx_lev)){
        json_object_put(obj);
        return NULL;
    }

    if (event->cell_geran_info_nmr) {
        geran_info = json_tokener_parse(event->cell_geran_info_nmr);

        if (geran_info) {
            json_object_object_add(obj,
                    mwz->keys[MD_ZMQ_KEY_RADIO_CELL_GERAN_INFO_NMR],
                    geran_info);
        } else {
            META_PRINT_SYSLOG(mwz->parent, LOG_ERR, "Failed to parse geran obj\n");
            json_object_put(obj);
            return NULL;
        }
    }

    return obj;
}

static json_object *md_zeromq_writer_handle_radio_cell_resel_event(
        struct md_writer_zeromq *mwz,
        struct md_radio_grr_cell_resel_event *event)
{
    struct json_object *obj, *neighbors;

    if (!(obj = json_object_new_object()))
        return NULL;

    if (event->iccid &&
        !md_zeromq_writer_create_json_string(obj, mwz->keys[MD_ZMQ_KEY_ICCID],
            event->iccid)) {
        json_object_put(obj);
        return NULL;
    }

    if (event->imsi &&
        !md_zeromq_writer_create_json_string(obj, mwz->keys[MD_ZMQ_KEY_IMSI],
            event->imsi)) {
        json_object_put(obj);
        return NULL;
    }

    if (event->imei &&
        !md_zeromq_writer_create_json_string(obj, mwz->keys[MD_ZMQ_KEY_IMEI],
            event->imei)) {
        json_object_put(obj);
        return NULL;
    }

    if (!md_zeromq_writer_add_default_fields(mwz, obj, event->sequence,
        event->tstamp, mwz->topics[MD_ZMQ_TOPIC_RADIO_GRR_CELL_RESEL])) {
        json_object_put(obj);
        return NULL;
    }

    if (!md_zeromq_writer_create_json_int(obj,
            mwz->keys[MD_ZMQ_KEY_RADIO_SERVING_BCCH_ARFCN],
            event->serving_bcch_arfcn) ||
        !md_zeromq_writer_create_json_int(obj,
            mwz->keys[MD_ZMQ_KEY_RADIO_SERVING_PBCCH_ARFCN],
            event->serving_pbcch_arfcn) ||
        !md_zeromq_writer_create_json_int64(obj,
            mwz->keys[MD_ZMQ_KEY_RADIO_SERVING_C1],
            event->serving_c1) ||
        !md_zeromq_writer_create_json_int64(obj,
            mwz->keys[MD_ZMQ_KEY_RADIO_SERVING_C2],
            event->serving_c2) ||
        !md_zeromq_writer_create_json_int64(obj,
            mwz->keys[MD_ZMQ_KEY_RADIO_SERVING_C31],
            event->serving_c31) ||
        !md_zeromq_writer_create_json_int64(obj,
            mwz->keys[MD_ZMQ_KEY_RADIO_SERVING_C32],
            event->serving_c32) ||
        !md_zeromq_writer_create_json_int(obj,
            mwz->keys[MD_ZMQ_KEY_RADIO_SERVING_PRIORITY_CLASS],
            event->serving_priority_class) ||
        !md_zeromq_writer_create_json_int(obj,
            mwz->keys[MD_ZMQ_KEY_RADIO_SERVING_RXLEV_AVG],
            event->serving_rxlev_avg) ||
        !md_zeromq_writer_create_json_int(obj,
            mwz->keys[MD_ZMQ_KEY_RADIO_SERVING_FIVE_SECOND_TIMER],
            event->serving_five_second_timer) ||
        !md_zeromq_writer_create_json_int(obj,
            mwz->keys[MD_ZMQ_KEY_RADIO_CELL_RESELECT_STATUS],
            event->cell_reselect_status) ||
        !md_zeromq_writer_create_json_int(obj,
            mwz->keys[MD_ZMQ_KEY_RADIO_RECENT_CELL_SELECTION],
            event->recent_cell_selection)) {
        json_object_put(obj);
        return NULL;
    }

    if (event->neighbors) {
        neighbors = json_tokener_parse(event->neighbors);

        if (neighbors) {
            json_object_object_add(obj,
                    mwz->keys[MD_ZMQ_KEY_RADIO_GRR_CELL_NEIGHBORS], neighbors);
        } else {
            META_PRINT_SYSLOG(mwz->parent, LOG_ERR, "Failed to parse neighbor obj\n");
            json_object_put(obj);
            return NULL;
        }
    }
    return obj;
}

static json_object *md_zeromq_writer_handle_radio_cipher_mode_event(
        struct md_writer_zeromq *mwz,
        struct md_radio_gsm_rr_cipher_mode_event *event)
{
    struct json_object *obj;

    if (!(obj = json_object_new_object()))
        return NULL;

    if (event->iccid &&
        !md_zeromq_writer_create_json_string(obj, mwz->keys[MD_ZMQ_KEY_ICCID],
            event->iccid)) {
        json_object_put(obj);
        return NULL;
    }

    if (event->imsi &&
        !md_zeromq_writer_create_json_string(obj, mwz->keys[MD_ZMQ_KEY_IMSI],
            event->imsi)) {
        json_object_put(obj);
        return NULL;
    }

    if (event->imei &&
        !md_zeromq_writer_create_json_string(obj, mwz->keys[MD_ZMQ_KEY_IMEI],
            event->imei)) {
        json_object_put(obj);
        return NULL;
    }

    if (!md_zeromq_writer_add_default_fields(mwz, obj, event->sequence,
        event->tstamp, mwz->topics[MD_ZMQ_TOPIC_RADIO_GSM_RR_CIPHER_MODE])) {
        json_object_put(obj);
        return NULL;
    }

    if (!md_zeromq_writer_create_json_int(obj,
            mwz->keys[MD_ZMQ_KEY_RADIO_CIPHERING_STATE],
            event->ciphering_state) ||
        !md_zeromq_writer_create_json_int(obj,
            mwz->keys[MD_ZMQ_KEY_RADIO_CIPHERING_ALGORITHM],
            event->ciphering_algorithm)) {
        json_object_put(obj);
        return NULL;
    }

    return obj;
}

static json_object *md_zeromq_writer_handle_cell_reset_param_event(
        struct md_writer_zeromq *mwz,
        struct md_radio_gsm_rr_cell_sel_reset_param_event *event)
{
    struct json_object *obj;

    if (!(obj = json_object_new_object()))
        return NULL;

    if (event->iccid &&
        !md_zeromq_writer_create_json_string(obj, mwz->keys[MD_ZMQ_KEY_ICCID],
            event->iccid)) {
        json_object_put(obj);
        return NULL;
    }

    if (event->imsi &&
        !md_zeromq_writer_create_json_string(obj, mwz->keys[MD_ZMQ_KEY_IMSI],
            event->imsi)) {
        json_object_put(obj);
        return NULL;
    }

    if (event->imei &&
        !md_zeromq_writer_create_json_string(obj, mwz->keys[MD_ZMQ_KEY_IMEI],
            event->imei)) {
        json_object_put(obj);
        return NULL;
    } 

    if (!md_zeromq_writer_add_default_fields(mwz, obj, event->sequence,
        event->tstamp, mwz->topics[MD_ZMQ_TOPIC_RADIO_GSM_RR_CELL_SEL_RESEL_PARAM])) {
        json_object_put(obj);
        return NULL;
    }

    if (!md_zeromq_writer_create_json_int(obj,
            mwz->keys[MD_ZMQ_KEY_RADIO_CELL_RESELECT_HYSTERESIS],
            event->cell_reselect_hysteresis) ||
        !md_zeromq_writer_create_json_int(obj,
            mwz->keys[MD_ZMQ_KEY_RADIO_MS_TXPWR_MAX_CCH],
            event->ms_txpwr_max_cch) ||
        !md_zeromq_writer_create_json_int(obj,
            mwz->keys[MD_ZMQ_KEY_RADIO_RXLEV_ACCESS_MIN],
            event->rxlev_access_min) ||
        !md_zeromq_writer_create_json_int(obj,
            mwz->keys[MD_ZMQ_KEY_RADIO_POWER_OFFSET_VALID],
            event->power_offset_valid) ||
        !md_zeromq_writer_create_json_int(obj,
            mwz->keys[MD_ZMQ_KEY_RADIO_POWER_OFFSET],
            event->power_offset) ||
        !md_zeromq_writer_create_json_int(obj,
            mwz->keys[MD_ZMQ_KEY_RADIO_NECI],
            event->neci) ||
        !md_zeromq_writer_create_json_int(obj,
            mwz->keys[MD_ZMQ_KEY_RADIO_ACS],
            event->acs) ||
        !md_zeromq_writer_create_json_int(obj,
            mwz->keys[MD_ZMQ_KEY_RADIO_OPT_RESELECT_PARAM_IND],
            event->opt_reselect_param_ind) ||
        !md_zeromq_writer_create_json_int(obj,
            mwz->keys[MD_ZMQ_KEY_RADIO_CELL_BAR_QUALIFY],
            event->cell_bar_qualify) ||
        !md_zeromq_writer_create_json_int(obj,
            mwz->keys[MD_ZMQ_KEY_RADIO_CELL_RESELECT_OFFSET],
            event->cell_reselect_offset) ||
        !md_zeromq_writer_create_json_int(obj,
            mwz->keys[MD_ZMQ_KEY_RADIO_TEMPORARY_OFFSET],
            event->temporary_offset) ||
        !md_zeromq_writer_create_json_int(obj,
            mwz->keys[MD_ZMQ_KEY_RADIO_PENALTY_TIME],
            event->penalty_time)) {
        json_object_put(obj);
        return NULL;
    }

    return obj;
}

static json_object *md_zeromq_writer_handle_rr_channel_conf_event(
        struct md_writer_zeromq *mwz,
        struct md_radio_gsm_rr_channel_conf_event *event)
{
    struct json_object *obj, *obj_tmp;

    if (!(obj = json_object_new_object()))
        return NULL;

    if (event->iccid &&
        !md_zeromq_writer_create_json_string(obj,
            mwz->keys[MD_ZMQ_KEY_ICCID], event->iccid)) {
        json_object_put(obj);
        return NULL;
    }

    if (event->imsi &&
        !md_zeromq_writer_create_json_string(obj,
            mwz->keys[MD_ZMQ_KEY_IMSI], event->imsi)) {
        json_object_put(obj);
        return NULL;
    }

    if (event->imei &&
        !md_zeromq_writer_create_json_string(obj,
            mwz->keys[MD_ZMQ_KEY_IMEI], event->imei)) {
        json_object_put(obj);
        return NULL;
    }

    if (!md_zeromq_writer_add_default_fields(mwz, obj, event->sequence,
        event->tstamp, mwz->topics[MD_ZMQ_TOPIC_RADIO_GSM_RR_CHANNEL_CONF])) {
        json_object_put(obj);
        return NULL;
    }

    if (!md_zeromq_writer_create_json_int(obj,
            mwz->keys[MD_ZMQ_KEY_RADIO_NUM_DED_CHANS],
            event->num_ded_chans) ||
        !md_zeromq_writer_create_json_int(obj,
            mwz->keys[MD_ZMQ_KEY_RADIO_DTX_INDICATOR],
            event->dtx_indicator) ||
        !md_zeromq_writer_create_json_int(obj,
            mwz->keys[MD_ZMQ_KEY_RADIO_POWER_LEVEL],
            event->power_level) ||
        !md_zeromq_writer_create_json_int(obj,
            mwz->keys[MD_ZMQ_KEY_RADIO_STARTING_TIME_VALID],
            event->starting_time_valid) ||
        !md_zeromq_writer_create_json_int(obj,
            mwz->keys[MD_ZMQ_KEY_RADIO_STARTING_TIME],
            event->starting_time) ||
        !md_zeromq_writer_create_json_int(obj,
            mwz->keys[MD_ZMQ_KEY_RADIO_CIPHER_FLAG],
            event->cipher_flag) ||
        !md_zeromq_writer_create_json_int(obj,
            mwz->keys[MD_ZMQ_KEY_RADIO_CIPHER_ALGORITHM],
            event->cipher_algorithm) ||
        !md_zeromq_writer_create_json_int(obj,
            mwz->keys[MD_ZMQ_KEY_RADIO_CHANNEL_MODE_1],
            event->channel_mode_1) ||
        !md_zeromq_writer_create_json_int(obj,
            mwz->keys[MD_ZMQ_KEY_RADIO_CHANNEL_MODE_2],
            event->channel_mode_2)) {
        json_object_put(obj);
        return NULL;
    }

    if (event->after_channel_config) {
        obj_tmp = json_tokener_parse(event->after_channel_config);

        if (obj_tmp) {
            json_object_object_add(obj,
                    mwz->keys[MD_ZMQ_KEY_RADIO_AFTER_CHANNEL_CONFIG],
                    obj_tmp);
        } else {
            META_PRINT_SYSLOG(mwz->parent, LOG_ERR, "Failed to parse after conf\n");
            json_object_put(obj);
            return NULL;
        }
    }

    if (event->before_channel_config) {
        obj_tmp = json_tokener_parse(event->before_channel_config);

        if (obj_tmp) {
            json_object_object_add(obj,
                    mwz->keys[MD_ZMQ_KEY_RADIO_BEFORE_CHANNEL_CONFIG],
                    obj_tmp);
        } else {
            META_PRINT_SYSLOG(mwz->parent, LOG_ERR, "Failed to parse before conf\n");
            json_object_put(obj);
            return NULL;
        }
    }

    return obj;
}

static void md_zeromq_writer_handle_radio(struct md_writer_zeromq *mwz, 
                                   struct md_radio_event *mre)
{
    char msg[8192] = {0};
    struct json_object *obj = NULL;
    const char *topic = NULL;
    int32_t retval;

    switch (mre->event_param) {
    case RADIO_EVENT_GSM_RR_CIPHER_MODE:
        META_PRINT_SYSLOG(mwz->parent, LOG_ERR, "GSM_RR_CIPHER_MODE\n");
        topic = mwz->topics[MD_ZMQ_TOPIC_RADIO_GSM_RR_CIPHER_MODE];
        obj = md_zeromq_writer_handle_radio_cipher_mode_event(mwz,
                (struct md_radio_gsm_rr_cipher_mode_event*) mre);
        break;
    case RADIO_EVENT_GSM_RR_CHANNEL_CONF:
        META_PRINT_SYSLOG(mwz->parent, LOG_ERR, "GSM_RR_CHANNEL_CONF\n");
        topic = mwz->topics[MD_ZMQ_TOPIC_RADIO_GSM_RR_CHANNEL_CONF];
        obj = md_zeromq_writer_handle_rr_channel_conf_event(mwz,
                (struct md_radio_gsm_rr_channel_conf_event*) mre);
        break;
    case RADIO_EVENT_CELL_LOCATION_GERAN:
        META_PRINT_SYSLOG(mwz->parent, LOG_ERR, "ZMQ CELL_LOCATION_GERAN\n");
        topic = mwz->topics[MD_ZMQ_TOPIC_RADIO_CELL_LOCATION_GERAN];
        obj = md_zeromq_writer_handle_radio_cell_loc_gerant(mwz,
                (struct md_radio_cell_loc_geran_event*) mre);
        break;
    case RADIO_EVENT_GSM_RR_CELL_SEL_RESEL_PARAM:
        META_PRINT_SYSLOG(mwz->parent, LOG_ERR, "GSM_RR_CELL_SEL_RESEL_PARAM\n");
        topic = mwz->topics[MD_ZMQ_TOPIC_RADIO_GSM_RR_CELL_SEL_RESEL_PARAM];
        obj = md_zeromq_writer_handle_cell_reset_param_event(mwz, 
                (struct md_radio_gsm_rr_cell_sel_reset_param_event*) mre);
        break;
    case RADIO_EVENT_GRR_CELL_RESEL:
        META_PRINT_SYSLOG(mwz->parent, LOG_ERR, "GRR_CELL_RESEL\n");
        topic = mwz->topics[MD_ZMQ_TOPIC_RADIO_GRR_CELL_RESEL];
        obj = md_zeromq_writer_handle_radio_cell_resel_event(mwz,
                (struct md_radio_grr_cell_resel_event*) mre);
        break;
    default:
        META_PRINT_SYSLOG(mwz->parent, LOG_INFO, "ZMQ writer does not support event %u\n",
                                    mre->md_type);  
        break;

    }

    if (!obj) {
        META_PRINT_SYSLOG(mwz->parent, LOG_ERR, "Failed to create JSON\n");
        return;
    }

    META_PRINT_SYSLOG(mwz->parent, LOG_ERR, "Will send %s %s\n", topic, msg);
    retval = snprintf(msg, sizeof(msg), "%s %s", topic,
            json_object_to_json_string_ext(obj, JSON_C_TO_STRING_PLAIN));

    if (retval >= sizeof(msg))
        return;

    retval = zmq_send(mwz->zmq_publisher, msg, strlen(msg), 0);
    META_PRINT_SYSLOG(mwz->parent, LOG_ERR, "Sent %d %s\n", retval, msg);
}

static void md_zeromq_writer_handle(struct md_writer *writer, struct md_event *event)
{
    struct md_writer_zeromq *mwz = (struct md_writer_zeromq*) writer;

    if (!mwz->socket_bound) {
        return;
    } else if (mwz->bind_timeout_handle != NULL) {
        //todo: stop doing this check on every iteration, add proper callback
        free(mwz->bind_timeout_handle);
        mwz->bind_timeout_handle = NULL;
    }

    switch (event->md_type) {
    case META_TYPE_POS:
        md_zeromq_writer_handle_gps(mwz, (struct md_gps_event*) event);
        break;
    case META_TYPE_CONNECTION:
        md_zeromq_writer_handle_conn(mwz, (struct md_conn_event*) event);
        break;
    case META_TYPE_MUNIN:
        md_zeromq_writer_handle_munin(mwz, (struct md_munin_event*) event);
        break;
    case META_TYPE_SYSEVENT:
        md_zeromq_writer_handle_sysevent(mwz, (struct md_sysevent*) event);
        break;
    case META_TYPE_INTERFACE:
        md_zeromq_writer_handle_iface(mwz, (struct md_iface_event*) event);
        break;
    case META_TYPE_RADIO:
        md_zeromq_writer_handle_radio(mwz, (struct md_radio_event*) event);
        break;
    case META_TYPE_ZEROMQ:
        md_zeromq_writer_handle_zeromq(mwz, (struct md_zeromq_event*) event);
        break;
    default:
        META_PRINT_SYSLOG(mwz->parent, LOG_INFO, "ZMQ writer does not support event %u\n",
                                    event->md_type);  
        break;
    }
}

static void md_zeromq_writer_bind_timeout(void *ptr)
{
    struct md_writer_zeromq *mwz = ptr;
    int32_t retval = 0;

    if ((retval = zmq_bind(mwz->zmq_publisher, mwz->zmq_addr)) != 0) {
        META_PRINT_SYSLOG(mwz->parent, LOG_ERR, "zmq_bind failed (%d): %s, "
                "stating timer\n", errno, zmq_strerror(errno));
        mwz->bind_timeout_handle->intvl = MD_ZMQ_BIND_INTVL;
    } else {
        META_PRINT_SYSLOG(mwz->parent, LOG_INFO, "zmq_bind succeeded after "
                "timeout\n");
        mwz->bind_timeout_handle->intvl = 0;
        mwz->socket_bound = 1;
    }

}

static uint8_t md_zeromq_writer_config(struct md_writer_zeromq *mwz,
                                const char *address,
                                uint16_t port)
{
    int32_t retval;

    snprintf(mwz->zmq_addr, sizeof(mwz->zmq_addr), "tcp://%s:%d", address,
            port);

    if ((mwz->zmq_context = zmq_ctx_new()) == NULL)
        return RETVAL_FAILURE;

    if ((mwz->zmq_publisher = zmq_socket(mwz->zmq_context, ZMQ_PUB)) == NULL)
        return RETVAL_FAILURE;

    if ((retval = zmq_bind(mwz->zmq_publisher, mwz->zmq_addr)) != 0) {
        META_PRINT_SYSLOG(mwz->parent, LOG_ERR, "zmq_bind failed (%d): %s, "
                "stating timer\n", errno, zmq_strerror(errno));
        if(!(mwz->bind_timeout_handle = backend_event_loop_create_timeout(0,
                        md_zeromq_writer_bind_timeout, mwz, 0))) {
            META_PRINT_SYSLOG(mwz->parent, LOG_ERR, "Failed to create ZMQ bind "
                    "timer\n");
            return RETVAL_FAILURE;
        }

        mde_start_timer(mwz->parent->event_loop, mwz->bind_timeout_handle,
                MD_ZMQ_BIND_INTVL);
    } else {
        mwz->socket_bound = 1;
    }

    if (mwz->metadata_project == MD_PROJECT_NNE) {
        mwz->topics = nne_topics;
        mwz->topics_limit = sizeof(nne_topics) / sizeof(char *);
        mwz->keys = nne_keys;
        mwz->keys_limit = sizeof(nne_keys) / sizeof(char *);
    } else if (mwz->metadata_project == MD_PROJECT_MNR) {
        mwz->topics = monroe_topics;
        mwz->topics_limit = sizeof(monroe_topics) / sizeof(char *);
        mwz->keys = monroe_keys;
        mwz->keys_limit = sizeof(monroe_keys) / sizeof(char *);
    } else {
        META_PRINT_SYSLOG(mwz->parent, LOG_ERR, "Unknown project (%u)\n",
                mwz->metadata_project);
        return RETVAL_FAILURE;
    }

    META_PRINT_SYSLOG(mwz->parent, LOG_INFO, "ZeroMQ init done. Topics limit %u\n", mwz->topics_limit);

    return RETVAL_SUCCESS;
}

static int32_t md_zeromq_writer_init(void *ptr, json_object* config)
{
    struct md_writer_zeromq *mwz = ptr;
    const char *address = NULL;
    uint16_t port = 0;

    json_object* subconfig;
    if (json_object_object_get_ex(config, "zmq", &subconfig)) {
        json_object_object_foreach(subconfig, key, val) {
            if (!strcmp(key, "address")) {
                address = json_object_get_string(val);
            } else if (!strcmp(key, "port")) {
                port = (uint16_t) json_object_get_int(val);
            } else if (!strcmp(key, "project")) {
                mwz->metadata_project = (uint8_t) json_object_get_int(val);
            }
        }
    }

    if (address == NULL || port == 0) {
        META_PRINT_SYSLOG(mwz->parent, LOG_ERR, "Missing required ZeroMQ argument\n");
        return RETVAL_FAILURE;
    }

    if (system_helpers_check_address(address)) {
        META_PRINT_SYSLOG(mwz->parent, LOG_ERR, "Error in ZeroMQ address\n");
        return RETVAL_FAILURE;
    }

    return md_zeromq_writer_config(mwz, address, port);
}

void md_zeromq_writer_usage()
{
    fprintf(stderr, "\"zmq\": {\t\tZeroMQ writer\n");
    fprintf(stderr, "  \"address\":\t\taddress used by publisher\n");
    fprintf(stderr, "  \"port\":\t\tport used by publisher\n");
    fprintf(stderr, "  \"project\":\t\tproject to use (0 for NNE, 1 for MNR)\n");
    fprintf(stderr, "},\n");
}

void md_zeromq_writer_setup(struct md_exporter *mde, struct md_writer_zeromq* mwz) {
    mwz->parent = mde;
    mwz->init = md_zeromq_writer_init;
    mwz->handle = md_zeromq_writer_handle;
    mwz->metadata_project = MD_PROJECT_NNE;
}

