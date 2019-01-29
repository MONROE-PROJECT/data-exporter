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
#include <stdlib.h>
#include <string.h>
#include <sqlite3.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

#include "metadata_utils.h"
#include "metadata_exporter.h"
#include "metadata_writer_inventory_conn.h"
#include "metadata_writer_sqlite_helpers.h"
#include "metadata_writer_json_helpers.h"
#include "metadata_exporter_log.h"
#include "system_helpers.h"

static uint8_t md_inventory_conn_create_event_str(struct md_conn_event *mce,
        char *csv_buf)
{
    if (mce->connection_mode) {
        return !(snprintf(csv_buf, UPDATE_STR_LEN, "%u,%u,%u,%u", mce->has_ip,
                mce->connectivity, mce->connection_mode, mce->quality) >=
                UPDATE_STR_LEN);
    } else {
        return !(snprintf(csv_buf, UPDATE_STR_LEN, "%u,%u,,%u", mce->has_ip,
            mce->connectivity, mce->quality) >= UPDATE_STR_LEN);
    }
}

static int32_t md_inventory_execute_insert_update(struct md_writer_sqlite *mws,
                                               struct md_conn_event *mce)
{
    uint8_t csv_valid = 0;
    char csv_buf[UPDATE_STR_LEN] = {0};

    csv_valid = md_inventory_conn_create_event_str(mce, csv_buf);

    sqlite3_stmt *stmt = mws->insert_update;

    sqlite3_clear_bindings(stmt);
    sqlite3_reset(stmt);

    if (sqlite3_bind_int(stmt, 1, mws->node_id) ||
        sqlite3_bind_int64(stmt, 2, mws->session_id) ||
        sqlite3_bind_int64(stmt, 3, mws->session_id_multip) ||
        sqlite3_bind_int64(stmt, 4, mce->tstamp) ||
        sqlite3_bind_int(stmt, 5, mce->sequence) ||
        sqlite3_bind_int(stmt, 6, mce->l3_session_id) ||
        sqlite3_bind_int(stmt, 7, mce->l4_session_id) ||
        sqlite3_bind_int(stmt, 8, mce->event_type) ||
        sqlite3_bind_int(stmt, 9, mce->event_param) ||
        sqlite3_bind_int(stmt, 10, mce->has_ip) ||
        sqlite3_bind_int(stmt, 11, mce->connectivity) ||
        sqlite3_bind_int(stmt, 13, mce->quality) ||
        sqlite3_bind_int(stmt, 14, mce->interface_type) ||
        sqlite3_bind_int(stmt, 16, mce->network_address_family) ||
        sqlite3_bind_text(stmt, 17, mce->network_address, strlen(mce->network_address), SQLITE_STATIC)){
        META_PRINT_SYSLOG(mws->parent, LOG_ERR, "Failed to bind values to INSERT query\n");
        return SQLITE_ERROR;
    }

    if (mws->api_version == 2 && mce->interface_type == INTERFACE_MODEM) {
        if (sqlite3_bind_text(stmt, 15, mce->imei, strlen(mce->imei), SQLITE_STATIC)) {
            META_PRINT_SYSLOG(mws->parent, LOG_ERR, "Failed to bind IMEI\n");
            return SQLITE_ERROR;
        }
    } else {
        if (sqlite3_bind_text(stmt, 15, mce->interface_id, strlen(mce->interface_id), SQLITE_STATIC)) {
            META_PRINT_SYSLOG(mws->parent, LOG_ERR, "Failed to bind interface id\n");
            return SQLITE_ERROR;
        }
    }

    if (mce->connection_mode &&
        sqlite3_bind_int(stmt, 12, mce->connection_mode)) {
        META_PRINT_SYSLOG(mws->parent, LOG_ERR, "Failed to bind connection mode\n");
        return SQLITE_ERROR;
    }

    if (mce->network_provider &&
        sqlite3_bind_int(stmt, 18, mce->network_provider)) {
        META_PRINT_SYSLOG(mws->parent, LOG_ERR, "Failed to bind network provider\n");
        return SQLITE_ERROR;
    }

    if (csv_valid &&
            sqlite3_bind_text(stmt, 19, csv_buf, -1, NULL)) {
        META_PRINT_SYSLOG(mws->parent, LOG_ERR, "Failed to bind network provider\n");
        return SQLITE_ERROR;
    }

    return sqlite3_step(stmt);
}

static int32_t md_inventory_execute_insert(struct md_writer_sqlite *mws,
                                        struct md_conn_event *mce)
{
    int32_t retval;

    sqlite3_stmt *stmt = mws->insert_event;
    sqlite3_clear_bindings(stmt);
    sqlite3_reset(stmt);

    if (sqlite3_bind_int(stmt, 1, mws->node_id) ||
        sqlite3_bind_int64(stmt, 2, mws->session_id) ||
        sqlite3_bind_int64(stmt, 3, mws->session_id_multip) ||
        sqlite3_bind_int64(stmt, 4, mce->tstamp) ||
        sqlite3_bind_int(stmt, 5, mce->sequence) ||
        sqlite3_bind_int(stmt, 6, mce->l3_session_id) ||
        sqlite3_bind_int(stmt, 7, mce->l4_session_id) ||
        sqlite3_bind_int(stmt, 8, mce->event_type) ||
        sqlite3_bind_int(stmt, 9, mce->event_param) ||
        sqlite3_bind_int(stmt, 11, mce->has_ip) ||
        sqlite3_bind_int(stmt, 12, mce->connectivity) ||
        sqlite3_bind_int(stmt, 14, mce->quality) ||
        sqlite3_bind_int(stmt, 15, mce->interface_type) ||
        sqlite3_bind_int(stmt, 16, mce->interface_id_type) ||
        sqlite3_bind_int(stmt, 19, mce->network_address_family) ||
        sqlite3_bind_text(stmt, 20, mce->network_address, strlen(mce->network_address), SQLITE_STATIC)) {
        META_PRINT_SYSLOG(mws->parent, LOG_ERR, "Failed to bind values to INSERT query\n");
        return SQLITE_ERROR;
    }

    if (mws->api_version == 2 && mce->interface_type == INTERFACE_MODEM) {
        if (sqlite3_bind_text(stmt, 17, mce->imei, strlen(mce->imei), SQLITE_STATIC)) {
            META_PRINT_SYSLOG(mws->parent, LOG_ERR, "Failed to bind IMEI\n");
            return SQLITE_ERROR;
        }
    } else {
        if (sqlite3_bind_text(stmt, 17, mce->interface_id, strlen(mce->interface_id), SQLITE_STATIC)) {
            META_PRINT_SYSLOG(mws->parent, LOG_ERR, "Failed to bind interface id\n");
            return SQLITE_ERROR;
        }
    }

    if (mce->connection_mode &&
        sqlite3_bind_int(stmt, 13, mce->connection_mode)) {
        META_PRINT_SYSLOG(mws->parent, LOG_ERR, "Failed bind event value (int)\n");
        return SQLITE_ERROR;
    }

    if (mce->event_value != UINT8_MAX &&
        sqlite3_bind_int(stmt, 10, mce->event_value)) {
        META_PRINT_SYSLOG(mws->parent, LOG_ERR, "Failed bind event value (int)\n");
        return SQLITE_ERROR;
    }

    if (mce->network_provider) {
        retval = sqlite3_bind_int(stmt, 18, mce->network_provider);

        if (retval) {
            META_PRINT_SYSLOG(mws->parent, LOG_ERR, "Failed to bind provider to INSERT query\n");
            return SQLITE_ERROR;
        }
    }

    return sqlite3_step(stmt);
}

static int32_t md_inventory_update_event(struct md_writer_sqlite *mws,
                                      struct md_conn_event *mce)
{
    sqlite3_stmt *stmt = mws->update_update;
    uint8_t csv_valid = 0;
    char csv_buf[UPDATE_STR_LEN] = {0};

    csv_valid = md_inventory_conn_create_event_str(mce, csv_buf);

    sqlite3_clear_bindings(stmt);
    sqlite3_reset(stmt);

    if (sqlite3_bind_int64(stmt, 1, mce->tstamp) ||
        sqlite3_bind_int(stmt, 2, mce->has_ip) ||
        sqlite3_bind_int(stmt, 3, mce->connectivity) ||
        sqlite3_bind_int(stmt, 5, mce->quality) ||
        sqlite3_bind_int(stmt, 7, mce->l3_session_id) ||
        sqlite3_bind_int(stmt, 8, mce->l4_session_id) ||
        sqlite3_bind_int(stmt, 9, mce->network_address_family) ||
        sqlite3_bind_text(stmt, 10, mce->network_address, strlen(mce->network_address), SQLITE_STATIC)) {
        META_PRINT_SYSLOG(mws->parent, LOG_ERR, "Failed to bind values to UPDATE query\n");
        return SQLITE_ERROR;
    }

    if (mws->api_version == 2 && mce->interface_type == INTERFACE_MODEM) {
        if (sqlite3_bind_text(stmt, 11, mce->imei, strlen(mce->imei), SQLITE_STATIC)) {
            META_PRINT_SYSLOG(mws->parent, LOG_ERR, "Failed to bind IMEI\n");
            return SQLITE_ERROR;
        }
    } else {
        if (sqlite3_bind_text(stmt, 11, mce->interface_id, strlen(mce->interface_id), SQLITE_STATIC)) {
            META_PRINT_SYSLOG(mws->parent, LOG_ERR, "Failed to bind interface id\n");
            return SQLITE_ERROR;
        }
    }

    if (mce->connection_mode &&
        sqlite3_bind_int(stmt, 4, mce->connection_mode)) {
            META_PRINT_SYSLOG(mws->parent, LOG_ERR, "Failed to bind connection mode\n");
            return SQLITE_ERROR;
    }

    if (csv_valid &&
            sqlite3_bind_text(stmt, 6, csv_buf, -1, NULL)) {
        META_PRINT_SYSLOG(mws->parent, LOG_ERR, "Failed to bind network provider\n");
        return SQLITE_ERROR;
    }

    return sqlite3_step(stmt);
}

static int32_t md_inventory_execute_insert_usage(struct md_writer_sqlite *mws,
                                              struct md_conn_event *mce,
                                              uint64_t date_start)
{
    uint8_t interface_id_idx = 1;
    sqlite3_stmt *stmt = mws->insert_usage;
    const char *no_iccid_str = "0";

    sqlite3_clear_bindings(stmt);
    sqlite3_reset(stmt);

    //For modems we need both IMEI and ICCID. ICCID is currently stored in the
    //interface_id variable, so some special handling is needed for now
    if (mce->imei) {
        if (sqlite3_bind_text(stmt, 1, mce->imei, strlen(mce->imei), SQLITE_STATIC) ||
            sqlite3_bind_text(stmt, 6, mce->imsi, strlen(mce->imsi), SQLITE_STATIC)) {
            META_PRINT_SYSLOG(mws->parent, LOG_ERR, "Failed to bind IMEI/IMSI\n");
            return SQLITE_ERROR;
        }

        interface_id_idx = 5;
    } else {
        if (sqlite3_bind_text(stmt, 5, no_iccid_str, strlen(no_iccid_str), SQLITE_STATIC) ||
            sqlite3_bind_text(stmt, 6, no_iccid_str, strlen(no_iccid_str), SQLITE_STATIC)) {
            META_PRINT_SYSLOG(mws->parent, LOG_ERR, "Failed to bind empty IMEI/IMSI\n");
            return SQLITE_ERROR;
        }
    }

    if (sqlite3_bind_text(stmt, interface_id_idx, mce->interface_id,
            strlen(mce->interface_id), SQLITE_STATIC) ||
        sqlite3_bind_int(stmt, 2, mce->network_address_family) ||
        sqlite3_bind_int(stmt, 3, mce->event_type) ||
        sqlite3_bind_int(stmt, 4, mce->event_param) ||
        sqlite3_bind_int64(stmt, 7, date_start) ||
        sqlite3_bind_int64(stmt, 8, mce->rx_bytes) ||
        sqlite3_bind_int64(stmt, 9, mce->tx_bytes)) {
        META_PRINT_SYSLOG(mws->parent, LOG_ERR, "Failed to bind values to INSERT usage query\n");
        return SQLITE_ERROR;
    }

    return sqlite3_step(stmt);
}

static int32_t md_inventory_execute_update_usage(struct md_writer_sqlite *mws,
                                              struct md_conn_event *mce,
                                              uint64_t date_start)
{
    const char *no_iccid_str = "0";
    sqlite3_stmt *stmt = mws->update_usage;

    sqlite3_clear_bindings(stmt);
    sqlite3_reset(stmt);

    if (sqlite3_bind_int64(stmt, 1, mce->rx_bytes) ||
        sqlite3_bind_int64(stmt, 2, mce->tx_bytes) ||
        sqlite3_bind_int(stmt, 4, mce->network_address_family) ||
        sqlite3_bind_int64(stmt, 7, date_start)) {
        META_PRINT_SYSLOG(mws->parent, LOG_ERR, "Failed to bind values to UPDATE usage query\n");
        return SQLITE_ERROR;
    }
        
    if (mce->imei) {
        if (sqlite3_bind_text(stmt, 3, mce->imei, strlen(mce->imei),
                SQLITE_STATIC) ||
            sqlite3_bind_text(stmt, 5, mce->interface_id,
                strlen(mce->interface_id), SQLITE_STATIC) ||
            sqlite3_bind_text(stmt, 6, mce->imsi,
                strlen(mce->imsi), SQLITE_STATIC)) {
            META_PRINT_SYSLOG(mws->parent, LOG_ERR, "Failed to bind values to UPDATE usage query #2\n");
            return SQLITE_ERROR;
        }
    } else {
        if (sqlite3_bind_text(stmt, 3, mce->interface_id,
                strlen(mce->interface_id), SQLITE_STATIC) ||
            sqlite3_bind_text(stmt, 5, no_iccid_str,
                strlen(no_iccid_str), SQLITE_STATIC) ||
            sqlite3_bind_text(stmt, 6, no_iccid_str,
                strlen(no_iccid_str), SQLITE_STATIC)) {
            META_PRINT_SYSLOG(mws->parent, LOG_ERR, "Failed to bind values to UPDATE usage query #2\n");
            return SQLITE_ERROR;
        }
    }

    META_PRINT_SYSLOG(mws->parent, LOG_ERR, "Update query: %s\n", sqlite3_expanded_sql(stmt));

    return sqlite3_step(stmt);
}

static uint8_t md_inventory_handle_insert_conn_event(struct md_writer_sqlite *mws,
                                                  struct md_conn_event *mce)
{
    int32_t retval = md_inventory_execute_insert(mws, mce);

    if (retval != SQLITE_DONE) {
        META_PRINT_SYSLOG(mws->parent, LOG_ERR, "INSERT failed: %s\n", sqlite3_errstr(retval));
        return RETVAL_FAILURE;
    }

    mws->num_conn_events++;
    return RETVAL_SUCCESS;
}

static int16_t md_inventory_get_last_update(struct md_writer_sqlite *mws,
                                         struct md_conn_event *mce,
                                         int16_t *mode, int16_t *quality)
{
    int16_t retval = -1;
    int numbytes = 0;
    const unsigned char *event_value_str = NULL;
    char event_str_cpy[EVENT_STR_LEN];
    sqlite3_stmt *stmt = mws->last_update;
    sqlite3_clear_bindings(stmt);
    sqlite3_reset(stmt);

    *mode = -1;
    *quality = -1;

    if (sqlite3_bind_int(stmt, 1, mce->l3_session_id) ||
        sqlite3_bind_int(stmt, 2, mce->l4_session_id) ||
        sqlite3_bind_int(stmt, 4, mce->network_address_family) ||
        sqlite3_bind_text(stmt, 5, mce->network_address, strlen(mce->network_address), SQLITE_STATIC)) {
        META_PRINT_SYSLOG(mws->parent, LOG_ERR, "Failed to bind values to SELECT query\n");
        return retval;
    }

    if (mws->api_version == 2 && mce->interface_type == INTERFACE_MODEM) {
        if (sqlite3_bind_text(stmt, 3, mce->imei, strlen(mce->imei), SQLITE_STATIC)) {
            META_PRINT_SYSLOG(mws->parent, LOG_ERR, "Failed to bind IMEI\n");
            return SQLITE_ERROR;
        }
    } else {
        if (sqlite3_bind_text(stmt, 3, mce->interface_id, strlen(mce->interface_id), SQLITE_STATIC)) {
            META_PRINT_SYSLOG(mws->parent, LOG_ERR, "Failed to bind interface id\n");
            return SQLITE_ERROR;
        }
    }

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        event_value_str = sqlite3_column_text(stmt, 0);
        numbytes = sqlite3_column_bytes(stmt, 0);

        if (numbytes >= EVENT_STR_LEN) {
            META_PRINT_SYSLOG(mws->parent, LOG_ERR, "Event value string will not fit in buffer\n");
            return SQLITE_ERROR;
        }

        memcpy(event_str_cpy, event_value_str, numbytes);
        event_str_cpy[numbytes] = '\0';

        *mode = metadata_utils_get_csv_pos(event_str_cpy, 2);
        *quality = metadata_utils_get_csv_pos(event_str_cpy, 3);
        break;
    }

    return retval;
}

static void md_inventory_insert_fake_mode(struct md_writer_sqlite *mws,
                                       struct md_conn_event *mce,
                                       uint8_t mode)
{
    int32_t retval;

    //TODO: Not a nice way to access parent variable
    mce->sequence = mde_inc_seq(mws->parent);
    mce->event_value = mode;
    mce->event_param = CONN_EVENT_META_MODE_UPDATE;

    retval = md_inventory_execute_insert(mws, mce);

    if (retval == SQLITE_DONE)
        META_PRINT_SYSLOG(mws->parent, LOG_INFO, "Inserted fake mode update\n");
    else
        META_PRINT_SYSLOG(mws->parent, LOG_ERR, "Failed to insert fake mode update\n");

    //Restore/update query after mode insert
    mce->event_param = CONN_EVENT_META_UPDATE;
    mce->event_value = 0;
}

static void md_inventory_insert_fake_quality(struct md_writer_sqlite *mws,
                                          struct md_conn_event *mce,
                                          uint8_t quality)
{
    int32_t retval;

    mce->sequence = mde_inc_seq(mws->parent);
    mce->event_value = quality;
    mce->event_param = CONN_EVENT_META_QUALITY_UPDATE;

    retval = md_inventory_execute_insert(mws, mce);

    if (retval == SQLITE_DONE)
        META_PRINT_SYSLOG(mws->parent, LOG_INFO, "Inserted fake quality update\n");
    else
        META_PRINT_SYSLOG(mws->parent, LOG_ERR, "Failed to insert fake quality update\n");

    //Restore/update query after quality insert
    mce->event_param = CONN_EVENT_META_UPDATE;
    mce->event_value = 0;
}

static void md_inventory_insert_fake_events(struct md_writer_sqlite *mws,
                                         struct md_conn_event *mce,
                                         int32_t update_exists)
{
    //TODO: Find a way to respect const
    int16_t mode_in_update = -1, mode_in_table = -1;
    int16_t quality_in_update = -1, quality_in_table = -1;
    struct timeval t_now;

    if (mws->first_fake_update.tv_sec != 0) {
        gettimeofday(&t_now, NULL);

        if (t_now.tv_sec > mws->first_fake_update.tv_sec &&
            t_now.tv_sec - mws->first_fake_update.tv_sec > FAKE_UPDATE_LIMIT) {
            mws->do_fake_updates = 0;
            return;
        }
    } else {
        gettimeofday(&(mws->first_fake_update), NULL); 
    }

    mode_in_update = mce->connection_mode;
    quality_in_update = mce->quality;

    //If there was no update messge from before, then insert the fake mode/quality messages (just to be sure)
    //and return
    if (update_exists == SQLITE_DONE) {
        //Always insert mode on the first update, for consistency (it should be
        //possible to follow the mode update messages exclusively)
        if (mode_in_update)
            md_inventory_insert_fake_mode(mws, mce, mode_in_update);

        if (quality_in_update)
            md_inventory_insert_fake_quality(mws, mce, quality_in_update);

        return;
    }

    md_inventory_get_last_update(mws, mce, &mode_in_table, &quality_in_table);

    //Get mode from last update message. If we can read modem mode, then this
    //value will be 0 or larger
    if (mode_in_update && mode_in_update != mode_in_table)
        md_inventory_insert_fake_mode(mws, mce, mode_in_update);

    if (quality_in_update && quality_in_update != quality_in_table)
        md_inventory_insert_fake_quality(mws, mce, quality_in_update);
}

static uint8_t md_inventory_handle_update_event(struct md_writer_sqlite *mws,
                                             struct md_conn_event *mce)
{
    //Check if update is present in update table by doing an insert
    int32_t retval = md_inventory_execute_insert_update(mws, mce);

    if (mws->do_fake_updates)
        md_inventory_insert_fake_events(mws, mce, retval);

    //No need to do UPDATE if INSERT was successful
    if (retval == SQLITE_DONE) {
        mws->num_conn_events++;
        return RETVAL_SUCCESS;
    }

    //Update in update table
    retval = md_inventory_update_event(mws, mce);

    if (retval != SQLITE_DONE) {
        META_PRINT_SYSLOG(mws->parent, LOG_ERR, "UPDATE failed: %s\n",
                sqlite3_errstr(retval));
        return RETVAL_FAILURE;
    }

    mws->num_conn_events++;
    return RETVAL_SUCCESS;
}

static uint8_t md_inventory_handle_usage_update(struct md_writer_sqlite *mws,
                                             struct md_conn_event *mce)
{
    uint64_t date_start = 0;
    struct tm tm_tmp = {0};
    time_t tstamp = (time_t) mce->tstamp;
    int32_t retval;

    //Create correct date_start and date_end (always to the hour) and keep at 0
    //if not, so that we can more easily update
    gmtime_r(&tstamp, &tm_tmp);

    //Only keep hour for date_start, date_end
    tm_tmp.tm_sec = 0;
    tm_tmp.tm_min = 0;

    date_start = (uint64_t) timegm(&tm_tmp);

    META_PRINT_SYSLOG(mws->parent, LOG_ERR, "DATE START %lu\n", date_start);

    retval = md_inventory_execute_update_usage(mws, mce, date_start);

    if (retval == SQLITE_DONE && sqlite3_changes(mws->db_handle)) {
        mws->num_usage_events++;
        return RETVAL_SUCCESS;
    }

    retval = md_inventory_execute_insert_usage(mws, mce, date_start);

    if (retval != SQLITE_DONE) {
        META_PRINT_SYSLOG(mws->parent, LOG_ERR, "Failed to update usage\n");
        return RETVAL_FAILURE;
    }

    mws->num_usage_events++;
    return RETVAL_SUCCESS;
}

uint8_t md_inventory_handle_conn_event(struct md_writer_sqlite *mws,
                                           struct md_conn_event *mce)
{
    uint8_t retval = RETVAL_SUCCESS;

    //Not quite sure how to handle timestamps that would go back in time
    if (mce->tstamp > mws->last_msg_tstamp)
        mws->last_msg_tstamp = mce->tstamp;

    if (mce->event_param == CONN_EVENT_META_UPDATE) {
        retval = md_inventory_handle_update_event(mws, mce);
    } else if (mce->event_param == CONN_EVENT_DATA_USAGE_UPDATE) {
        if (mws->usage_prefix[0] && (mce->rx_bytes || mce->tx_bytes))
            retval = md_inventory_handle_usage_update(mws, mce);
    } else {
        retval = md_inventory_handle_insert_conn_event(mws, mce);
    }

    return retval;
}

static uint8_t md_inventory_conn_dump_db_json(struct md_writer_sqlite *mws, FILE *output)
{
    const char *json_str;

    sqlite3_reset(mws->dump_table);
    sqlite3_reset(mws->dump_update);

    sqlite3_bind_int64(mws->dump_table, 1, mws->dump_tstamp);
    sqlite3_bind_int64(mws->dump_update, 1, mws->dump_tstamp);

    json_object *jarray = json_object_new_array();

    if (md_json_helpers_dump_write(mws->dump_table, jarray))
    {
        json_object_put(jarray);
        return RETVAL_FAILURE;
    }

    if (md_json_helpers_dump_write(mws->dump_update, jarray))
    {
        json_object_put(jarray);
        return RETVAL_FAILURE;
    }

    json_str = json_object_to_json_string_ext(jarray, JSON_C_TO_STRING_PLAIN);
    fprintf(output, "%s", json_str);
    json_object_put(jarray);
    return RETVAL_SUCCESS;
}

static uint8_t md_inventory_conn_delete_db(struct md_writer_sqlite *mws)
{
    int32_t retval;
    sqlite3_stmt *delete_update;

    sqlite3_reset(mws->delete_table);
    retval = sqlite3_step(mws->delete_table);

    if (retval != SQLITE_DONE) {
        META_PRINT_SYSLOG(mws->parent, LOG_ERR, "Failed to delete events %s\n",
                sqlite3_errstr(retval));
        return RETVAL_FAILURE;
    }

    if (!mws->delete_conn_update)
        return RETVAL_SUCCESS;

    if ((retval = sqlite3_prepare_v2(mws->db_handle, DELETE_NW_UPDATE, -1,
                    &delete_update, NULL))) {
        META_PRINT_SYSLOG(mws->parent, LOG_ERR, "Prepare failed %s\n",
                sqlite3_errstr(retval));
        return RETVAL_FAILURE; 
    }

    //Delete all updates that have not been updated for the last half hour, to
    //reduce size of database. If metadata exporter has been stopped for a long
    //time, we might create some redundant fake events, but those can be handled
    //by backend. This can happen if we export for example before an update
    //message for all active sessions have been receieved
    if ((retval = sqlite3_bind_int64(delete_update, 1,
                    mws->last_msg_tstamp - 1800))) {
        META_PRINT_SYSLOG(mws->parent, LOG_ERR, "Bind failed %s\n",
                sqlite3_errstr(retval));
        return RETVAL_FAILURE; 
    }

    retval = sqlite3_step(delete_update);

    if (retval != SQLITE_DONE) {
        META_PRINT_SYSLOG(mws->parent, LOG_ERR, "Failed to delete updates: %s\n",
                sqlite3_errstr(retval));
        return RETVAL_FAILURE;
    }

    mws->delete_conn_update = 0;
    return RETVAL_SUCCESS;
}

static uint8_t md_inventory_usage_dump_db_json(struct md_writer_sqlite *mws, FILE *output)
{
    const char *json_str;
    sqlite3_reset(mws->dump_usage);

    json_object *jarray = json_object_new_array();

    if (md_json_helpers_dump_write(mws->dump_usage, jarray))
    {
        json_object_put(jarray);
        return RETVAL_FAILURE;
    }

    json_str = json_object_to_json_string_ext(jarray, JSON_C_TO_STRING_PLAIN);
    fprintf(output, "%s", json_str);

    json_object_put(jarray);
    return RETVAL_SUCCESS;
}

static uint8_t md_inventory_usage_delete_db(struct md_writer_sqlite *mws)
{
    int32_t retval;

    sqlite3_reset(mws->delete_usage);
    retval = sqlite3_step(mws->delete_usage);

    if (retval == SQLITE_DONE) {
        return RETVAL_SUCCESS;
    } else {
        META_PRINT_SYSLOG(mws->parent, LOG_ERR, "Failed to delete usage\n");
        return RETVAL_FAILURE;
    }
}

uint8_t md_inventory_conn_copy_db(struct md_writer_sqlite *mws)
{
    uint8_t retval = 0;

    md_writer_helpers_copy_db(mws->meta_prefix,
            mws->meta_prefix_len, md_inventory_conn_dump_db_json, mws,
            md_inventory_conn_delete_db);

    if (retval == RETVAL_SUCCESS) {
        mws->dump_tstamp = mws->last_msg_tstamp;
        mws->num_conn_events = 0;

        if (mws->last_conn_tstamp_path)
            system_helpers_write_uint64_to_file(mws->last_conn_tstamp_path,
                    mws->dump_tstamp);
    }

    return retval;
}

uint8_t md_inventory_conn_usage_copy_db(struct md_writer_sqlite *mws)
{
    uint8_t retval = 0;

    md_writer_helpers_copy_db(mws->usage_prefix,
            mws->usage_prefix_len, md_inventory_usage_dump_db_json, mws,
            md_inventory_usage_delete_db);

    if (retval == RETVAL_SUCCESS)
        mws->num_usage_events = 0;

    return retval;
}
