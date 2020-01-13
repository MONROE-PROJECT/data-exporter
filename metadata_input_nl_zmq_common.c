#include <stdio.h>
#include <string.h>

#include "metadata_exporter_log.h"
#include "metadata_input_nl_zmq_common.h"

uint8_t parse_conn_info(struct json_object *meta_obj, struct md_conn_event *mce, struct md_exporter *parent)
{
    json_object_object_foreach(meta_obj, key, val) {
        if (!strcmp(key, "md_seq"))
            mce->sequence = (uint16_t) json_object_get_int(val);
        else if (!strcmp(key, "timestamp"))
            mce->tstamp = json_object_get_int64(val);
        else if (!strcmp(key, "event_type"))
            mce->event_type = (uint8_t) json_object_get_int(val);
        else if (!strcmp(key, "event_param"))
            mce->event_param = (uint8_t) json_object_get_int(val);
        else if (!strcmp(key, "event_value"))
            mce->event_value = (uint8_t) json_object_get_int(val);
        else if (!strcmp(key, "interface_id_type"))
            mce->interface_id_type = (uint8_t) json_object_get_int(val);
        else if (!strcmp(key, "interface_id"))
            mce->interface_id = json_object_get_string(val);
        else if (!strcmp(key, "imei"))
            mce->imei = json_object_get_string(val);
        else if (!strcmp(key, "imsi"))
            mce->imsi = json_object_get_string(val);
        else if (!strcmp(key, "interface_name"))
            mce->interface_name = json_object_get_string(val);
        else if (!strcmp(key, "interface_type"))
            mce->interface_type = (uint8_t) json_object_get_int(val);
        else if (!strcmp(key, "network_address_family"))
            mce->network_address_family = (uint8_t) json_object_get_int(val);
        else if (!strcmp(key, "network_address"))
            mce->network_address = json_object_get_string(val);
        else if (!strcmp(key, "network_provider_type"))
            mce->network_provider_type = (uint8_t) json_object_get_int(val);
        else if (!strcmp(key, "network_provider"))
            mce->network_provider = json_object_get_int(val);
        else if (!strcmp(key, "l3_session_id"))
            mce->l3_session_id = (uint64_t) json_object_get_int64(val);
        else if (!strcmp(key, "l4_session_id"))
            mce->l4_session_id = (uint64_t) json_object_get_int64(val);
        else if (!strcmp(key, "signal_strength"))
            mce->signal_strength = (int8_t) json_object_get_int(val);
        else if (!strcmp(key, "rx_bytes"))
            mce->rx_bytes = (uint64_t) json_object_get_int64(val);
        else if (!strcmp(key, "tx_bytes"))
            mce->tx_bytes = (uint64_t) json_object_get_int64(val);
        else if (!strcmp(key, "has_ip"))
            mce->has_ip = (uint8_t) json_object_get_int(val);
        else if (!strcmp(key, "connectivity"))
            mce->connectivity = (uint8_t) json_object_get_int(val);
        else if (!strcmp(key, "connection_mode"))
            mce->connection_mode = (uint8_t) json_object_get_int(val);
        else if (!strcmp(key, "quality"))
            mce->quality = (uint8_t) json_object_get_int(val);
    }

    if (mce->event_param == CONN_EVENT_DATA_USAGE_UPDATE) {
        if (!mce->tstamp || !mce->event_param || !mce->interface_id || (mce->imei && !mce->imsi) ||
            (mce->imsi && !mce->imei)) {
            META_PRINT_SYSLOG(parent, LOG_ERR, "Missing required argument in usage JSON\n");
            return RETVAL_FAILURE;
        } else {
            return RETVAL_SUCCESS;
        }
    }

    if (!mce->tstamp || !mce->sequence ||
        !mce->l3_session_id || !mce->event_param ||
        !mce->interface_type || !mce->network_address_family ||
        !mce->network_address || !mce->interface_id ||
        !mce->interface_id_type) {
        META_PRINT_SYSLOG(parent, LOG_ERR, "Missing required argument in JSON\n");
        return RETVAL_FAILURE;
    }

    return RETVAL_SUCCESS;
}

uint8_t parse_iface_event(struct json_object *meta_obj, struct md_iface_event *mie, struct md_exporter *parent)
{
    json_object_object_foreach(meta_obj, key, val) {
        if (!strcmp(key, "md_seq")) {
            mie->sequence = (uint16_t) json_object_get_int(val);
        } else if (!strcmp(key, "timestamp")) {
            mie->tstamp = json_object_get_int64(val);
        } else if (!strcmp(key, "event_param")) {
            mie->event_param = (uint8_t) json_object_get_int(val);
        } else if (!strcmp(key, "event_type")) {
            mie->event_type = (uint8_t) json_object_get_int(val);
        } else if (!strcmp(key, "iccid")) {
            mie->iccid = json_object_get_string(val);
        } else if (!strcmp(key, "imsi")) {
            mie->imsi = json_object_get_string(val);
        } else if (!strcmp(key, "imei")) {
            mie->imei = json_object_get_string(val);
        } else if (!strcmp(key, "ip_addr")) {
            mie->ip_addr = json_object_get_string(val);
        } else if (!strcmp(key, "internal_ip_addr")) {
            mie->internal_ip_addr = json_object_get_string(val);
        } else if (!strcmp(key, "isp_name")) {
            mie->isp_name = json_object_get_string(val);
        } else if (!strcmp(key, "ifname")) {
            mie->ifname = json_object_get_string(val);
        } else if (!strcmp(key, "imsi_mccmnc")) {
            mie->imsi_mccmnc = (uint32_t) json_object_get_int(val);
        } else if (!strcmp(key, "network_mccmnc")) {
            mie->nw_mccmnc = (uint32_t) json_object_get_int(val);
        } else if (!strcmp(key, "cid")) {
            mie->cid = json_object_get_int(val);
        } else if (!strcmp(key, "device_mode")) {
            mie->device_mode = (uint8_t) json_object_get_int(val);
        } else if (!strcmp(key, "device_sub_mode")) {
            mie->device_submode = (uint8_t) json_object_get_int(val);
        } else if (!strcmp(key, "rssi")) {
            mie->rssi = (int8_t) json_object_get_int(val);
        } else if (!strcmp(key, "ecio")) {
            mie->ecio = (int8_t) json_object_get_int(val);
        } else if (!strcmp(key, "rscp")) {
            mie->rscp = (int16_t) json_object_get_int(val);
        } else if (!strcmp(key, "lte_rssi")) {
            mie->lte_rssi = (int8_t) json_object_get_int(val);
        } else if (!strcmp(key, "lte_rsrp")) {
            mie->lte_rsrp = (int16_t) json_object_get_int(val);
        } else if (!strcmp(key, "lte_rsrq")) {
            mie->lte_rsrq = (int8_t) json_object_get_int(val);
        } else if (!strcmp(key, "lac")) {
            mie->lac = (uint16_t) json_object_get_int(val);
        } else if (!strcmp(key, "lte_band")) {
            mie->lte_band = (uint8_t) json_object_get_int(val);
        } else if (!strcmp(key, "lte_freq")) {
            mie->lte_freq = (uint16_t) json_object_get_int(val);
        } else if (!strcmp(key, "lte_pci")) {
            mie->lte_pci = (uint16_t) json_object_get_int(val);
        } else if (!strcmp(key, "device_state")) {
            mie->device_state = (uint8_t) json_object_get_int(val);
        } else if (!strcmp(key, "enodeb_id")) {
            mie->enodeb_id = json_object_get_int(val);
        } else if (!strcmp(key, "ca_info")) {
            mie->ca_info = json_object_to_json_string_ext(val, JSON_C_TO_STRING_PLAIN);

        } else if (!strcmp(key, "tx_power")) {
            mie->tx_power = (int32_t) json_object_get_int(val);
        } else if (!strcmp(key, "earfcn")) {
            mie->iot_earfcn = (uint32_t) json_object_get_int(val);
        } else if (!strcmp(key, "celevel")) {
            mie->iot_celevel = (uint8_t) json_object_get_int(val);
        }
    }

    return RETVAL_SUCCESS;
}

struct md_radio_cell_loc_geran_event * radio_cell_loc_geran(json_object *obj)
{
    struct md_radio_cell_loc_geran_event *event = calloc(sizeof(struct md_radio_cell_loc_geran_event), 1);

    if (!event)
        return NULL;

    json_object_object_foreach(obj, key, val) {
        if (!strcmp(key, "md_seq"))
            event->sequence = (uint16_t) json_object_get_int(val);
        else if (!strcmp(key, "timestamp"))
            event->tstamp = json_object_get_int64(val);
        else if (!strcmp(key, "event_param"))
            event->event_param = (uint8_t) json_object_get_int(val);
        else if (!strcmp(key, "event_type"))
            event->md_type = (uint8_t) json_object_get_int(val);
        else if (!strcmp(key, "iccid"))
            event->iccid = json_object_get_string(val);
        else if (!strcmp(key, "imsi"))
            event->imsi = json_object_get_string(val);
        else if (!strcmp(key, "imei"))
            event->imei = json_object_get_string(val);
        else if (!strcmp(key, "cell_id"))
            event->cell_id = (uint32_t) json_object_get_int(val);
        else if (!strcmp(key, "plmn"))
            event->plmn = json_object_get_string(val);
        else if (!strcmp(key, "lac"))
            event->lac = (uint16_t) json_object_get_int(val);
        else if (!strcmp(key, "arfcn"))
            event->arfcn = (uint16_t) json_object_get_int(val);
        else if (!strcmp(key, "bsic"))
            event->bsic = (uint8_t) json_object_get_int(val);
        else if (!strcmp(key, "timing_advance"))
            event->timing_advance = (uint32_t) json_object_get_int(val);
        else if (!strcmp(key, "rx_lev"))
            event->rx_lev = (uint16_t) json_object_get_int(val);
        else if (!strcmp(key, "cell_geran_info_nmr"))
            event->cell_geran_info_nmr = json_object_to_json_string_ext(val, JSON_C_TO_STRING_PLAIN);
    }

    return event;
}

struct md_radio_grr_cell_resel_event* radio_grr_cell_resel(json_object* obj)
{
    struct md_radio_grr_cell_resel_event *event = calloc(sizeof(struct md_radio_grr_cell_resel_event), 1);
    struct json_object *obj_tmp;
    uint8_t neigh_count = 0;

    if (!event)
        return NULL;

    json_object_object_foreach(obj, key, val) {
        if (!strcmp(key, "md_seq"))
            event->sequence = (uint16_t) json_object_get_int(val);
        else if (!strcmp(key, "timestamp"))
            event->tstamp = json_object_get_int64(val);
        else if (!strcmp(key, "event_param"))
            event->event_param = (uint8_t) json_object_get_int(val);
        else if (!strcmp(key, "event_type"))
            event->md_type = (uint8_t) json_object_get_int(val);
        else if (!strcmp(key, "iccid"))
            event->iccid = json_object_get_string(val);
        else if (!strcmp(key, "imsi"))
            event->imsi = json_object_get_string(val);
        else if (!strcmp(key, "imei"))
            event->imei = json_object_get_string(val);
        else if (!strcmp(key, "serving_bcch_arfcn"))
            event->serving_bcch_arfcn = (uint16_t) json_object_get_int(val);
        else if (!strcmp(key, "serving_pbcch_arfcn"))
            event->serving_pbcch_arfcn = (uint16_t) json_object_get_int(val);
        else if (!strcmp(key, "serving_priority_class"))
            event->serving_priority_class = (uint8_t) json_object_get_int(val);
        else if (!strcmp(key, "serving_rxlev_avg"))
            event->serving_rxlev_avg = (uint8_t) json_object_get_int(val);
        else if (!strcmp(key, "serving_c1"))
            event->serving_c1 = (uint32_t) json_object_get_int(val);
        else if (!strcmp(key, "serving_c2"))
            event->serving_c2 = (uint32_t) json_object_get_int(val);
        else if (!strcmp(key, "serving_c31"))
            event->serving_c31 = (uint32_t) json_object_get_int(val);
        else if (!strcmp(key, "serving_c32"))
            event->serving_c32 = (uint32_t) json_object_get_int(val);
        else if (!strcmp(key, "serving_five_second_timer"))
            event->serving_five_second_timer = (uint8_t) json_object_get_int(val);
        else if (!strcmp(key, "cell_reselect_status"))
            event->cell_reselect_status = (uint8_t) json_object_get_int(val);
        else if (!strcmp(key, "recent_cell_selection"))
            event->recent_cell_selection = (uint8_t) json_object_get_int(val);
    }

    json_object_object_get_ex(obj, "neighbor_cell_count", &obj_tmp);

    if (obj_tmp)
        neigh_count = json_object_get_int(obj_tmp);

    if (neigh_count) {
        json_object_object_get_ex(obj, "grr_cell_neighbor", &obj_tmp);

        if (obj_tmp)
            event->neighbors = json_object_to_json_string_ext(obj_tmp, JSON_C_TO_STRING_PLAIN);
    }

    return event;
}

struct md_radio_gsm_rr_cell_sel_reset_param_event* radio_gsm_rr_cell_sel_reset_param(json_object *obj)
{
    struct md_radio_gsm_rr_cell_sel_reset_param_event *event = calloc(sizeof(struct md_radio_gsm_rr_cell_sel_reset_param_event), 1);

    if (!event)
        return NULL;

    json_object_object_foreach(obj, key, val) {
        if (!strcmp(key, "md_seq"))
            event->sequence = (uint16_t) json_object_get_int(val);
        else if (!strcmp(key, "timestamp"))
            event->tstamp = json_object_get_int64(val);
        else if (!strcmp(key, "event_param"))
            event->event_param = (uint8_t) json_object_get_int(val);
        else if (!strcmp(key, "event_type"))
            event->md_type = (uint8_t) json_object_get_int(val);
        else if (!strcmp(key, "iccid"))
            event->iccid = json_object_get_string(val);
        else if (!strcmp(key, "imsi"))
            event->imsi = json_object_get_string(val);
        else if (!strcmp(key, "imei"))
            event->imei = json_object_get_string(val);
        else if (!strcmp(key, "cell_reselect_hysteresis"))
            event->cell_reselect_hysteresis = (uint8_t) json_object_get_int(val);
        else if (!strcmp(key, "ms_txpwr_max_cch"))
            event->ms_txpwr_max_cch = (uint8_t) json_object_get_int(val);
        else if (!strcmp(key, "rxlev_access_min"))
            event->rxlev_access_min = (uint8_t) json_object_get_int(val);
        else if (!strcmp(key, "power_offset_valid"))
            event->power_offset_valid = (uint8_t) json_object_get_int(val);
        else if (!strcmp(key, "power_offset"))
            event->power_offset = (uint8_t) json_object_get_int(val);
        else if (!strcmp(key, "neci"))
            event->neci = (uint8_t) json_object_get_int(val);
        else if (!strcmp(key, "acs"))
            event->acs = (uint8_t) json_object_get_int(val);
        else if (!strcmp(key, "opt_reselect_param_ind"))
            event->opt_reselect_param_ind = (uint8_t) json_object_get_int(val);
        else if (!strcmp(key, "cell_bar_qualify"))
            event->cell_bar_qualify = (uint8_t) json_object_get_int(val);
        else if (!strcmp(key, "cell_reselect_offset"))
            event->cell_reselect_offset = (uint8_t) json_object_get_int(val);
        else if (!strcmp(key, "temporary_offset"))
            event->temporary_offset = (uint8_t) json_object_get_int(val);
        else if (!strcmp(key, "penalty_time"))
            event->penalty_time = (uint8_t) json_object_get_int(val);
    }

    return event;
}

struct md_radio_gsm_rr_cipher_mode_event* radio_gsm_rr_cipher_mode(json_object *obj)
{
    struct md_radio_gsm_rr_cipher_mode_event *event = calloc(sizeof(struct md_radio_gsm_rr_cipher_mode_event), 1);

    if (!event)
        return NULL;

    json_object_object_foreach(obj, key, val) {
        if (!strcmp(key, "md_seq"))
            event->sequence = (uint16_t) json_object_get_int(val);
        else if (!strcmp(key, "timestamp"))
            event->tstamp = json_object_get_int64(val);
        else if (!strcmp(key, "event_param"))
            event->event_param = (uint8_t) json_object_get_int(val);
        else if (!strcmp(key, "event_type"))
            event->md_type = (uint8_t) json_object_get_int(val);
        else if (!strcmp(key, "iccid"))
            event->iccid = json_object_get_string(val);
        else if (!strcmp(key, "imsi"))
            event->imsi = json_object_get_string(val);
        else if (!strcmp(key, "imei"))
            event->imei = json_object_get_string(val);
        else if (!strcmp(key, "ciphering_state"))
            event->ciphering_state = (uint8_t) json_object_get_int(val);
        else if (!strcmp(key, "ciphering_algorithm"))
            event->ciphering_algorithm = (uint8_t) json_object_get_int(val);
    }

    return event;
}

struct md_radio_gsm_rr_channel_conf_event* radio_gsm_rr_channel_conf(json_object *obj)
{
    struct md_radio_gsm_rr_channel_conf_event *event = calloc(sizeof(struct md_radio_gsm_rr_channel_conf_event), 1);

    if (!event)
        return NULL;

    json_object_object_foreach(obj, key, val) {
        if (!strcmp(key, "md_seq"))
            event->sequence = (uint16_t) json_object_get_int(val);
        else if (!strcmp(key, "timestamp"))
            event->tstamp = json_object_get_int64(val);
        else if (!strcmp(key, "event_param"))
            event->event_param = (uint8_t) json_object_get_int(val);
        else if (!strcmp(key, "event_type"))
            event->md_type = (uint8_t) json_object_get_int(val);
        else if (!strcmp(key, "iccid"))
            event->iccid = json_object_get_string(val);
        else if (!strcmp(key, "imsi"))
            event->imsi = json_object_get_string(val);
        else if (!strcmp(key, "imei"))
            event->imei = json_object_get_string(val);
        else if (!strcmp(key, "num_ded_chans;"))
            event->num_ded_chans = (uint8_t) json_object_get_int(val);
        else if (!strcmp(key, "dtx_indicator"))
            event->dtx_indicator = (uint8_t) json_object_get_int(val);
        else if (!strcmp(key, "power_level"))
            event->power_level = (uint8_t) json_object_get_int(val);
        else if (!strcmp(key, "starting_time_valid"))
            event->starting_time_valid = (uint8_t) json_object_get_int(val);
        else if (!strcmp(key, "starting_time"))
            event->starting_time = (uint16_t) json_object_get_int(val);
        else if (!strcmp(key, "cipher_flag"))
            event->cipher_flag = (uint8_t) json_object_get_int(val);
        else if (!strcmp(key, "cipher_algorithm"))
            event->cipher_algorithm = (uint8_t) json_object_get_int(val);
        else if (!strcmp(key, "channel_mode_1"))
            event->channel_mode_1 = (uint8_t) json_object_get_int(val);
        else if (!strcmp(key, "channel_mode_2"))
            event->channel_mode_2 = (uint8_t) json_object_get_int(val);
        else if (!strcmp(key, "after_channel_config"))
            event->after_channel_config = json_object_get_string(val);
        else if (!strcmp(key, "before_channel_config"))
            event->before_channel_config = json_object_get_string(val);
    }

    return event;
}

struct md_radio_wcdma_rrc_state_event* radio_wcdma_rrc_state(json_object *obj)
{
    struct md_radio_wcdma_rrc_state_event *event = calloc(sizeof(struct md_radio_wcdma_rrc_state_event), 1);

    if (!event)
        return NULL;

    json_object_object_foreach(obj, key, val) {
        if (!strcmp(key, "md_seq"))
            event->sequence = (uint16_t) json_object_get_int(val);
        else if (!strcmp(key, "timestamp"))
            event->tstamp = json_object_get_int64(val);
        else if (!strcmp(key, "event_param"))
            event->event_param = (uint8_t) json_object_get_int(val);
        else if (!strcmp(key, "event_type"))
            event->md_type = (uint8_t) json_object_get_int(val);
        else if (!strcmp(key, "iccid"))
            event->iccid = json_object_get_string(val);
        else if (!strcmp(key, "imsi"))
            event->imsi = json_object_get_string(val);
        else if (!strcmp(key, "imei"))
            event->imei = json_object_get_string(val);
        else if (!strcmp(key, "rrc_state"))
            event->rrc_state = (uint8_t) json_object_get_int(val);
    }

    return event;
}

struct md_radio_wcdma_cell_id_event* radio_wcdma_cell_id(json_object *obj)
{
    struct md_radio_wcdma_cell_id_event *event = calloc(sizeof(struct md_radio_wcdma_cell_id_event), 1);

    if (!event)
        return NULL;

    json_object_object_foreach(obj, key, val) {
        if (!strcmp(key, "md_seq"))
            event->sequence = (uint16_t) json_object_get_int(val);
        else if (!strcmp(key, "timestamp"))
            event->tstamp = json_object_get_int64(val);
        else if (!strcmp(key, "event_param"))
            event->event_param = (uint8_t) json_object_get_int(val);
        else if (!strcmp(key, "event_type"))
            event->md_type = (uint8_t) json_object_get_int(val);
        else if (!strcmp(key, "iccid"))
            event->iccid = json_object_get_string(val);
        else if (!strcmp(key, "imsi"))
            event->imsi = json_object_get_string(val);
        else if (!strcmp(key, "imei"))
            event->imei = json_object_get_string(val);
        else if (!strcmp(key, "ul_uarfcn"))
            event->ul_uarfcn = (uint32_t) json_object_get_int(val);
        else if (!strcmp(key, "dl_uarfcn"))
            event->dl_uarfcn = (uint32_t) json_object_get_int(val);
        else if (!strcmp(key, "cell_id"))
            event->cell_id = (uint32_t) json_object_get_int(val);
        else if (!strcmp(key, "ura_id"))
            event->ura_id = (uint16_t) json_object_get_int(val);
        else if (!strcmp(key, "cell_access_rest"))
            event->cell_access_rest = (uint8_t) json_object_get_int(val);
        else if (!strcmp(key, "call_accs"))
            event->dl_uarfcn = (uint8_t) json_object_get_int(val);
    }

    return event;
}

struct md_gps_event* handle_gps_event(struct json_object *json_obj)
{
    int8_t sentence_id = 0;

    struct md_gps_event *gps_event = calloc(sizeof(struct md_gps_event), 1);

    if (!gps_event)
        return NULL;

    union {
        struct minmea_sentence_gga gga;
        struct minmea_sentence_rmc rmc;
    } gps;

    gps_event->md_type = META_TYPE_POS;

    json_object_object_foreach(json_obj, key, val) {
        if (!strcmp(key, "md_seq"))
            gps_event->sequence = (uint16_t) json_object_get_int(val);

        if (!strcmp(key, "timestamp"))
            gps_event->tstamp_tv.tv_sec = json_object_get_int64(val);

        if (!strcmp(key, "nmea_string"))
            gps_event->nmea_raw = json_object_get_string(val);
    }

    if (!gps_event->sequence || !gps_event->nmea_raw)
    {
        free(gps_event);
        return NULL;
    }

    sentence_id = minmea_sentence_id(gps_event->nmea_raw, 0);

    if (sentence_id <= 0)
    {
        free(gps_event);
        return NULL;
    }

    gps_event->minmea_id = sentence_id;

    //We can ignore NMEA checksum
    switch (sentence_id) {
    case MINMEA_SENTENCE_GGA:
        if (minmea_parse_gga(&gps.gga, gps_event->nmea_raw) && !gps.gga.fix_quality) {
            free(gps_event);
            return NULL;
        } else {
            gps_event->time = gps.gga.time;
            gps_event->latitude = minmea_tocoord(&gps.gga.latitude);
            gps_event->longitude = minmea_tocoord(&gps.gga.longitude);
            gps_event->altitude = minmea_tofloat(&gps.gga.altitude);
            gps_event->satellites_tracked = gps.gga.satellites_tracked;
        }
        break;
    case MINMEA_SENTENCE_RMC:
        if (minmea_parse_rmc(&gps.rmc, gps_event->nmea_raw) && !gps.rmc.valid) {
            free(gps_event);
            return NULL;
        } else {
            gps_event->time = gps.rmc.time;
            gps_event->latitude = minmea_tocoord(&gps.rmc.latitude);
            gps_event->longitude = minmea_tocoord(&gps.rmc.longitude);
            gps_event->speed = minmea_tofloat(&gps.rmc.speed);
        }
        break;
    default:
        free(gps_event);
        return NULL;
    }

    return gps_event;
}

uint8_t add_json_key_value(const char *key,
                           int32_t value, struct json_object *obj)
{
    struct json_object *obj_add = NULL;
    obj_add = json_object_new_int(value);

    if (!obj_add)
        return RETVAL_FAILURE;

    json_object_object_add(obj, key, obj_add);

    return RETVAL_SUCCESS;
}

void init_iface_event(struct md_iface_event *mie)
{
    memset(mie, 0, sizeof(struct md_iface_event));
    mie->md_type = META_TYPE_INTERFACE;
    mie->lac = -1;
    mie->cid = -1;
    mie->rscp = DEFAULT_RSCP;
    mie->lte_rsrp = DEFAULT_RSRP;
    mie->rssi = DEFAULT_RSSI;
    mie->ecio = DEFAULT_ECIO;
    mie->lte_rssi = DEFAULT_RSSI;
    mie->lte_rsrq = DEFAULT_RSRQ;
    mie->lte_pci = DEFAULT_LTE_PCI;
    mie->enodeb_id = DEFAULT_ENODEBID;
    mie->device_mode = DEFAULT_MODE;
    mie->device_submode = DEFAULT_SUBMODE;

    mie->tx_power = DEFAULT_TX_POWER;
    mie->iot_earfcn = DEFAULT_IOT_EARFCN;
    mie->iot_celevel = DEFAULT_IOT_CELEVEL;
}

void init_conn_event(struct md_conn_event* mce)
{
    memset(mce, 0, sizeof(struct md_conn_event));
    mce->md_type = META_TYPE_CONNECTION;
    //255 is reserved value used to indicate that there is no value to export
    //(look at writers)
    mce->event_value = UINT8_MAX;
}
