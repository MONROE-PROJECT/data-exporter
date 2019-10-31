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
#include <unistd.h>
#include <getopt.h>
#include <libmnl/libmnl.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>
#include <stdlib.h>
#include <errno.h>
#include <sqlite3.h>
#include <linux/sysinfo.h>
#include <sys/syscall.h>

#include "metadata_writer_sqlite.h"
#include "metadata_writer_inventory_conn.h"
#include "metadata_writer_sqlite_helpers.h"
#include "metadata_writer_inventory_gps.h"
#include "metadata_writer_sqlite_monitor.h"
#include "metadata_writer_inventory_system.h"
#include "system_helpers.h"
#include "backend_event_loop.h"
#include "metadata_exporter_log.h"

static void md_sqlite_copy_db(struct md_writer_sqlite *mws, uint8_t from_timeout);
static void md_sqlite_handle_timeout(void *ptr);
static void md_sqlite_handle(struct md_writer *writer, struct md_event *event);

static void md_sqlite_itr_cb(void *ptr)
{
    struct md_writer_sqlite *mws = ptr;

    if (mws->file_failed && !mws->timeout_added) {
        mde_start_timer(mws->parent->event_loop,
                        mws->timeout_handle,
                        TIMEOUT_FILE);
        mws->timeout_added = 1; 
    }
}

static void md_sqlite_copy_db(struct md_writer_sqlite *mws, uint8_t from_timeout)
{
    uint8_t retval = RETVAL_FAILURE;
    uint8_t num_failed = 0;

    if (!mws->node_id || !mws->valid_timestamp ||
            (mws->session_id_file && !mws->session_id))
    {
        META_PRINT_SYSLOG(mws->parent, LOG_ERR, "Can't export DB. # node_id %d "
                            "# valid_timestamp %u # session_id_file %s # session_id %lu\n",
                            mws->node_id,
                            mws->valid_timestamp,
                            mws->session_id_file ? mws->session_id_file : "EMPTY",
                            mws->session_id);
        return;
    }

    if (mws->timeout_added && !from_timeout) {
        backend_remove_timeout(mws->timeout_handle);
        mws->timeout_added = 0;
    }

    META_PRINT_SYSLOG(mws->parent, LOG_INFO, "Will export DB. # meta %u "
                      "# gps %u # monitor %u usage %u system %u\n",
            mws->num_conn_events,
            mws->num_gps_events,
            mws->num_munin_events,
            mws->num_usage_events,
            mws->num_system_events);

    if (mws->num_conn_events) {
        retval = md_inventory_conn_copy_db(mws);

        if (retval == RETVAL_FAILURE)
            num_failed++;
    }

    if (mws->num_gps_events) {
        retval = md_inventory_gps_copy_db(mws);

        if (retval == RETVAL_FAILURE)
            num_failed++;
    }

    if (mws->num_munin_events) {
        retval = md_sqlite_monitor_copy_db(mws);

        if (retval == RETVAL_FAILURE)
            num_failed++;
    }

    if (mws->num_usage_events) {
        retval = md_inventory_conn_usage_copy_db(mws);

        if (retval == RETVAL_FAILURE)
            num_failed++;
    }

    if (mws->num_system_events) {
        retval = md_inventory_system_copy_db(mws);

        if (retval == RETVAL_FAILURE)
            num_failed++;
    }

    if (num_failed != 0) {
        META_PRINT_SYSLOG(mws->parent, LOG_ERR, "%u DB dump(s) failed\n", num_failed);
        mws->file_failed = 1;
    } else {
        mws->file_failed = 0;
    }
}

static uint8_t md_sqlite_update_nodeid_db(struct md_writer_sqlite *mws, const char *sql_str)
{
    int32_t retval;
    sqlite3_stmt *update_tables;

    if ((retval = sqlite3_prepare_v2(mws->db_handle, sql_str, -1,
                    &update_tables, NULL))) {
        META_PRINT_SYSLOG(mws->parent, LOG_ERR, "Prepare failed %s\n", sqlite3_errstr(retval));
        return RETVAL_FAILURE; 
    }

    if ((retval = sqlite3_bind_int(update_tables, 1, mws->node_id))) {
        META_PRINT_SYSLOG(mws->parent, LOG_ERR, "Bind failed %s\n", sqlite3_errstr(retval));
        return RETVAL_FAILURE; 
    }
   
    retval = sqlite3_step(update_tables);

    if (retval != SQLITE_DONE) {
        META_PRINT_SYSLOG(mws->parent, LOG_ERR, "Step faild %s\n", sqlite3_errstr(retval));
        return RETVAL_FAILURE;
    }

    sqlite3_finalize(update_tables);
    return RETVAL_SUCCESS;
}

static uint8_t md_sqlite_update_timestamp_db(struct md_writer_sqlite *mws,
        const char *sql_str, uint64_t orig_boot_time, uint64_t real_boot_time)
{
    int32_t retval;
    sqlite3_stmt *update_timestamp;

    if ((retval = sqlite3_prepare_v2(mws->db_handle, sql_str, -1,
                    &update_timestamp, NULL))) {
        META_PRINT_SYSLOG(mws->parent, LOG_ERR, "Prepare failed %s\n", sqlite3_errstr(retval));
        return RETVAL_FAILURE;
    }

    if ((retval = sqlite3_bind_int64(update_timestamp, 1, orig_boot_time))) {
        META_PRINT_SYSLOG(mws->parent, LOG_ERR, "Bind failed %s\n", sqlite3_errstr(retval));
        return RETVAL_FAILURE;
    }

    if ((retval = sqlite3_bind_int64(update_timestamp, 2, real_boot_time))) {
        META_PRINT_SYSLOG(mws->parent, LOG_ERR, "Bind failed %s\n", sqlite3_errstr(retval));
        return RETVAL_FAILURE;
    }

    if ((retval = sqlite3_bind_int64(update_timestamp, 3, real_boot_time))) {
        META_PRINT_SYSLOG(mws->parent, LOG_ERR, "Bind failed %s\n", sqlite3_errstr(retval));
        return RETVAL_FAILURE;
    }

    retval = sqlite3_step(update_timestamp);

    if (retval != SQLITE_DONE) {
        META_PRINT_SYSLOG(mws->parent, LOG_ERR, "Step faild %s\n", sqlite3_errstr(retval));
        return RETVAL_FAILURE;
    }

    sqlite3_finalize(update_timestamp);

    return RETVAL_SUCCESS;
}

static uint8_t md_sqlite_update_session_id_db(struct md_writer_sqlite *mws,
        const char *sql_str)
{
    int32_t retval;
    sqlite3_stmt *update_session_id;

    if ((retval = sqlite3_prepare_v2(mws->db_handle, sql_str, -1,
                    &update_session_id, NULL))) {
        META_PRINT_SYSLOG(mws->parent, LOG_ERR, "Prepare failed %s\n", sqlite3_errstr(retval));
        return RETVAL_FAILURE;
    }

    if ((retval = sqlite3_bind_int64(update_session_id, 1, mws->session_id))) {
        META_PRINT_SYSLOG(mws->parent, LOG_ERR, "Bind failed %s\n", sqlite3_errstr(retval));
        return RETVAL_FAILURE;
    }

    if ((retval = sqlite3_bind_int64(update_session_id, 2,
                    mws->session_id_multip))) {
        META_PRINT_SYSLOG(mws->parent, LOG_ERR, "Bind failed #2 %s\n", sqlite3_errstr(retval));
        return RETVAL_FAILURE;
    }

    retval = sqlite3_step(update_session_id);

    if (retval != SQLITE_DONE) {
        META_PRINT_SYSLOG(mws->parent, LOG_ERR, "Step faild %s\n", sqlite3_errstr(retval));
        return RETVAL_FAILURE;
    }

    sqlite3_finalize(update_session_id);

    return RETVAL_SUCCESS;
}

static sqlite3* md_sqlite_configure_db(struct md_writer_sqlite *mws, const char *db_filename)
{
    sqlite3 *db_handle = NULL;
    int retval = 0;
    char *db_errmsg = NULL;

    retval = sqlite3_open_v2(db_filename, &db_handle, SQLITE_OPEN_READWRITE |
                             SQLITE_OPEN_CREATE | SQLITE_OPEN_FULLMUTEX, NULL);

    if (retval != SQLITE_OK) {
        if (db_handle != NULL)
            META_PRINT_SYSLOG(mws->parent, LOG_ERR, "open failed with message: %s\n", sqlite3_errmsg(db_handle));
        else
            META_PRINT_SYSLOG(mws->parent, LOG_ERR, "not enough memory to create db_handle object\n");

        return NULL;
    }

    //make sure database is ready to be used. this avoids having checks in
    //metadata_produce, since it will first export any message stored in
    //database
    if (sqlite3_exec(db_handle, CREATE_SQL, NULL, NULL, &db_errmsg)) {
        META_PRINT_SYSLOG(mws->parent, LOG_ERR, "db create failed with message: %s\n", db_errmsg);
        sqlite3_close_v2(db_handle);
        return NULL;
    }

    if (sqlite3_exec(db_handle, CREATE_UPDATE_SQL, NULL, NULL, &db_errmsg)) {
        META_PRINT_SYSLOG(mws->parent, LOG_ERR, "db create (update) failed with message: %s\n", db_errmsg);
        sqlite3_close_v2(db_handle);
        return NULL;
    }

    if (sqlite3_exec(db_handle, CREATE_GPS_SQL, NULL, NULL, &db_errmsg)) {
        META_PRINT_SYSLOG(mws->parent, LOG_ERR, "db create (gps) failed with message: %s\n", db_errmsg);
        sqlite3_close_v2(db_handle);
        return NULL;
    }

    if (sqlite3_exec(db_handle, CREATE_MONITOR_SQL, NULL, NULL, &db_errmsg)) {
        META_PRINT_SYSLOG(mws->parent, LOG_ERR, "db create (monitor) failed with message: %s\n", db_errmsg);
        sqlite3_close_v2(db_handle);
        return NULL;
    }

    if (sqlite3_exec(db_handle, CREATE_USAGE_SQL, NULL, NULL, &db_errmsg)) {
        META_PRINT_SYSLOG(mws->parent, LOG_ERR, "db create (usage) failed with message: %s\n", db_errmsg);
        sqlite3_close_v2(db_handle);
        return NULL;
    }

    if (sqlite3_exec(db_handle, CREATE_REBOOT_SQL, NULL, NULL, &db_errmsg)) {
        META_PRINT_SYSLOG(mws->parent, LOG_ERR, "db create (reboot) failed with message: %s\n", db_errmsg);
        sqlite3_close_v2(db_handle);
        return NULL;
    }

    return db_handle;
}

static int md_sqlite_read_orig_boot_time(struct md_writer_sqlite *mws)
{
    struct timespec tp_raw, tp_real;
    struct sysinfo info = {0};

    syscall(SYS_sysinfo, &info);

    clock_gettime(CLOCK_MONOTONIC_RAW, &tp_raw);
    clock_gettime(CLOCK_REALTIME, &tp_real);

    //This can only happen on boot on devices where time is reset to epoch.
    //uptime and clock_gettime() come from different clock sources, uptime might
    //for example have been updated before tp_real
    if (info.uptime > tp_real.tv_sec) {
        mws->orig_boot_time = 0;
    } else {
        mws->orig_boot_time = tp_real.tv_sec - info.uptime;
    }

    mws->orig_uptime = info.uptime;
    mws->orig_raw_time = tp_raw.tv_sec;

    return RETVAL_SUCCESS;
}

static void md_sqlite_get_real_boot_time(struct md_writer_sqlite *mws, uint64_t *real_boot_time)
{
    struct timespec tp_raw, tp_real;

    clock_gettime(CLOCK_MONOTONIC_RAW, &tp_raw);
    clock_gettime(CLOCK_REALTIME, &tp_real);

    //The calculation here is as follows:
    //* The goal is to find the correct boot time, now that we have a valid NTP fix
    //* We get the current (walltime).
    //* From this value we want to subtract the total uptime.
    //* To reduce the number of clock sources, we use use MONOTONIG_RAW to calculate
    //the amount of time that has passed since data_exporter was started.
    //* We add the diff between (cur_raw - orig_raw) to the uptime we read when
    //data_exporter was started. The sum is time elapsed since boot.
    //* In order to get the "correct" boot time, we subtract the calculation above
    //from the current walltime.
    *real_boot_time = tp_real.tv_sec - (mws->orig_uptime + (tp_raw.tv_sec - mws->orig_raw_time));
}

static int md_sqlite_configure(struct md_writer_sqlite *mws,
        const char *db_filename, uint32_t node_id, uint32_t db_interval,
        uint32_t db_events, const char *meta_prefix, const char *gps_prefix,
        const char *monitor_prefix, const char *usage_prefix,
        const char *system_prefix, const char *ntp_fix_file)
{
    sqlite3 *db_handle = md_sqlite_configure_db(mws, db_filename);

    if (db_handle == NULL)
        return RETVAL_FAILURE;

    if (node_id > 0) {
        mws->node_id = node_id;
    } else {
#ifdef MONROE
        mws->node_id = system_helpers_get_nodeid(mws->node_id_file);
#endif
    }

    //Only set variables that are not 0
    mws->db_handle = db_handle;
    mws->db_interval = db_interval;
    mws->db_events = db_events;
    mws->do_fake_updates = 1;
    mws->delete_conn_update = 1;

    //We will not use timer right away
    if(!(mws->timeout_handle = backend_event_loop_create_timeout(0,
            md_sqlite_handle_timeout, mws, 0))) {
        sqlite3_close_v2(db_handle);
        return RETVAL_FAILURE;
    }

    if(sqlite3_prepare_v2(mws->db_handle, INSERT_EVENT, -1,
            &(mws->insert_event), NULL) ||
       sqlite3_prepare_v2(mws->db_handle, DELETE_TABLE, -1,
            &(mws->delete_table), NULL) ||
       sqlite3_prepare_v2(mws->db_handle, INSERT_UPDATE, -1,
            &(mws->insert_update), NULL) ||
       sqlite3_prepare_v2(mws->db_handle, UPDATE_UPDATE, -1,
            &(mws->update_update), NULL) ||
       sqlite3_prepare_v2(mws->db_handle, SELECT_LAST_UPDATE, -1,
            &(mws->last_update), NULL) ||
       sqlite3_prepare_v2(mws->db_handle, INSERT_GPS_EVENT, -1,
            &(mws->insert_gps), NULL) ||
       sqlite3_prepare_v2(mws->db_handle, DELETE_GPS_TABLE, -1,
            &(mws->delete_gps), NULL) ||
       sqlite3_prepare_v2(mws->db_handle, DUMP_GPS_JSON, -1,
            &(mws->dump_gps), NULL) ||
       sqlite3_prepare_v2(mws->db_handle, INSERT_MONITOR_EVENT, -1,
            &(mws->insert_monitor), NULL) ||
       sqlite3_prepare_v2(mws->db_handle, DELETE_MONITOR_TABLE, -1,
            &(mws->delete_monitor), NULL) ||
       sqlite3_prepare_v2(mws->db_handle, DUMP_MONITOR_JSON, -1,
            &(mws->dump_monitor), NULL) ||
       sqlite3_prepare_v2(mws->db_handle, INSERT_USAGE, -1,
            &(mws->insert_usage), NULL) ||
       sqlite3_prepare_v2(mws->db_handle, UPDATE_USAGE, -1,
            &(mws->update_usage), NULL) ||
       sqlite3_prepare_v2(mws->db_handle, DUMP_USAGE_JSON, -1,
            &(mws->dump_usage), NULL) ||
       sqlite3_prepare_v2(mws->db_handle, DELETE_USAGE_TABLE, -1,
            &(mws->delete_usage), NULL) ||
       sqlite3_prepare_v2(mws->db_handle, INSERT_REBOOT_EVENT, -1,
            &(mws->insert_system), NULL) ||
       sqlite3_prepare_v2(mws->db_handle, DUMP_SYSTEM_JSON, -1,
            &(mws->dump_system), NULL) ||
       sqlite3_prepare_v2(mws->db_handle, DELETE_SYSTEM_TABLE, -1,
            &(mws->delete_system), NULL)) {
        META_PRINT_SYSLOG(mws->parent, LOG_ERR, "Statement failed: %s\n",
                sqlite3_errmsg(mws->db_handle));
        sqlite3_close_v2(db_handle);
        return RETVAL_FAILURE;
    }

    if (sqlite3_prepare_v2(mws->db_handle, DUMP_EVENTS_JSON, -1,
                &(mws->dump_table), NULL) ||
        sqlite3_prepare_v2(mws->db_handle, DUMP_UPDATES_JSON, -1,
            &(mws->dump_update), NULL)) {
        META_PRINT_SYSLOG(mws->parent, LOG_ERR, "Dump prepare failed: %s\n",
                sqlite3_errmsg(mws->db_handle));
        sqlite3_close_v2(db_handle);
        return RETVAL_FAILURE;
    }

    if (meta_prefix) {
        memset(mws->meta_prefix, 0, sizeof(mws->meta_prefix));
        memcpy(mws->meta_prefix, meta_prefix, strlen(meta_prefix));

        //We need to reset the last six characthers to X, so keep track of the
        //length of the original prefix
        mws->meta_prefix_len = strlen(meta_prefix);
    }

    if (gps_prefix) {
        memset(mws->gps_prefix, 0, sizeof(mws->gps_prefix));
        memcpy(mws->gps_prefix, gps_prefix, strlen(gps_prefix));

        //We need to reset the last six characthers to X, so keep track of the
        //length of the original prefix
        mws->gps_prefix_len = strlen(gps_prefix);
    }

    if (monitor_prefix) {
        memset(mws->monitor_prefix, 0, sizeof(mws->monitor_prefix));
        memcpy(mws->monitor_prefix, monitor_prefix, strlen(monitor_prefix));

        //We need to reset the last six characthers to X, so keep track of the
        //length of the original prefix
        mws->monitor_prefix_len = strlen(monitor_prefix);
    }

    if (usage_prefix) {
        memset(mws->usage_prefix, 0, sizeof(mws->usage_prefix));
        memcpy(mws->usage_prefix, usage_prefix, strlen(usage_prefix));

        //We need to reset the last six characthers to X, so keep track of the
        //length of the original prefix
        mws->usage_prefix_len = strlen(usage_prefix);
    }

    if (system_prefix) {
        memset(mws->system_prefix, 0, sizeof(mws->system_prefix));
        memcpy(mws->system_prefix, system_prefix, strlen(system_prefix));

        //We need to reset the last six characthers to X, so keep track of the
        //length of the original prefix
        mws->system_prefix_len = strlen(system_prefix);
    }

    if (ntp_fix_file) {
        memset(mws->ntp_fix_file, 0, sizeof(mws->ntp_fix_file));
        memcpy(mws->ntp_fix_file, ntp_fix_file, strlen(ntp_fix_file));
    }

    if (mws->node_id && (md_sqlite_update_nodeid_db(mws, UPDATE_EVENT_ID) ||
        md_sqlite_update_nodeid_db(mws, UPDATE_UPDATES_ID) ||
        md_sqlite_update_nodeid_db(mws, UPDATE_GPS_ID) ||
        md_sqlite_update_nodeid_db(mws, UPDATE_USAGE_ID) ||
        md_sqlite_update_nodeid_db(mws, UPDATE_SYSTEM_ID))) {
        META_PRINT_SYSLOG(mws->parent, LOG_ERR, "Could not update old ements with id 0\n");
        return RETVAL_FAILURE;
    }

    if (mws->session_id_file)
        system_helpers_read_session_id(mws->session_id_file, &(mws->session_id),
                &(mws->session_id_multip));

    if (mws->last_conn_tstamp_path)
        system_helpers_read_uint64_from_file(mws->last_conn_tstamp_path,
                &(mws->dump_tstamp));

    return md_sqlite_read_orig_boot_time(mws);
}

void md_sqlite_usage()
{
    fprintf(stderr, "\"sqlite\": {\t\tSQLite writer. At least one prefix is required.\n");
    fprintf(stderr, "  \"database\":\t\tpath to database (local files only)\n");
    fprintf(stderr, "  \"nodeid\":\t\tnode id.\n");
    fprintf(stderr, "  \"nodeid_file\":\tpath to node id file.\n");
    fprintf(stderr, "  \"meta_prefix\":\tlocation + filename prefix for connection metadata (max 116 characters)\n");
    fprintf(stderr, "  \"gps_prefix\":\t\tlocation + filename prefix for GPS data (max 116 characters)\n");
    fprintf(stderr, "  \"monitor_prefix\":\tlocation + filename prefix for monitor data (max 116 characters)\n");
    fprintf(stderr, "  \"usage_prefix\":\tlocation + filename prefix for usage data (max 116 characters)\n");
    fprintf(stderr, "  \"system_prefix\":\tlocation + filename prefix for system events (max 116 characters)\n");
    fprintf(stderr, "  \"interval\":\t\ttime (in ms) from event and until database is copied (default: 5 sec)\n");
    fprintf(stderr, "  \"events\":\t\tnumber of events before copying database (default: 10)\n");
    fprintf(stderr, "  \"session_id\":\t\tpath to session id file\n");
    fprintf(stderr, "  \"api_version\":\tbackend API version (default: 1)\n");
    fprintf(stderr, "  \"last_conn_tstamp_path\":\toptional path to file where we read/store timestamp of last conn dump\n");
    fprintf(stderr, "  \"ntp_fix_file\":\tFile to check for NTP fix\n");
    fprintf(stderr, "}\n");
}

int32_t md_sqlite_init(void *ptr, json_object* config)
{
    struct md_writer_sqlite *mws = ptr;
    uint32_t node_id = 0, interval = DEFAULT_TIMEOUT, num_events = EVENT_LIMIT;
    const char *db_filename = NULL, *meta_prefix = NULL, *gps_prefix = NULL,
               *monitor_prefix = NULL, *usage_prefix = NULL,
               *system_prefix = NULL, *ntp_fix_file = NULL;

    json_object* subconfig;
    if (json_object_object_get_ex(config, "sqlite", &subconfig)) {
        json_object_object_foreach(subconfig, key, val) {
            if (!strcmp(key, "database"))
                db_filename = json_object_get_string(val);
            else if (!strcmp(key, "nodeid"))
                node_id = (uint32_t) json_object_get_int(val);
            else if (!strcmp(key, "nodeid_file"))
                mws->node_id_file = strdup(json_object_get_string(val));
            else if (!strcmp(key, "meta_prefix"))
                meta_prefix = json_object_get_string(val);
            else if (!strcmp(key, "gps_prefix"))
                gps_prefix = json_object_get_string(val);
            else if (!strcmp(key, "monitor_prefix"))
                monitor_prefix = json_object_get_string(val);
            else if (!strcmp(key, "usage_prefix"))
                usage_prefix = json_object_get_string(val);
            else if (!strcmp(key, "system_prefix"))
                system_prefix = json_object_get_string(val);
            else if (!strcmp(key, "interval"))
                interval = ((uint32_t) json_object_get_int(val)) * 1000;
            else if (!strcmp(key, "events"))
                num_events = (uint32_t) json_object_get_int(val);
            else if (!strcmp(key, "session_id"))
                mws->session_id_file = strdup(json_object_get_string(val));
            else if (!strcmp(key, "api_version"))
                mws->api_version = (uint32_t) json_object_get_int(val);
            else if (!strcmp(key, "last_conn_tstamp_path"))
                mws->last_conn_tstamp_path = strdup(json_object_get_string(val));
            else if (!strcmp(key, "ntp_fix_file"))
                ntp_fix_file = json_object_get_string(val);
        }
    }

    if (!db_filename || (!gps_prefix && !meta_prefix && !monitor_prefix)) {
        META_PRINT_SYSLOG(mws->parent, LOG_ERR, "Required SQLite argument missing\n");
        return RETVAL_FAILURE;
    }

    if ((meta_prefix    && strlen(meta_prefix)    > 117) ||
        (gps_prefix     && strlen(gps_prefix)     > 117) ||
        (monitor_prefix && strlen(monitor_prefix) > 117) ||
        (usage_prefix   && strlen(usage_prefix) > 117)   ||
        (system_prefix  && strlen(system_prefix) > 117) ||
        (ntp_fix_file   && strlen(ntp_fix_file) > 127)) {
        META_PRINT_SYSLOG(mws->parent, LOG_ERR, "SQLite temp file prefix too long\n");
        return RETVAL_FAILURE;
    }

    if (!db_filename || (!gps_prefix && !meta_prefix && !monitor_prefix)) {
        META_PRINT_SYSLOG(mws->parent, LOG_ERR, "Required SQLite argument missing\n");
        return RETVAL_FAILURE;
    }

    if (!interval || !num_events) {
        META_PRINT_SYSLOG(mws->parent, LOG_ERR, "Invalid SQLite interval/number of events\n");
        return RETVAL_FAILURE;
    }
 
    if (!mws->api_version || mws->api_version > 2) {
        META_PRINT_SYSLOG(mws->parent, LOG_ERR, "Unknown backend API version\n");
        return RETVAL_FAILURE;
    }

    META_PRINT_SYSLOG(mws->parent, LOG_ERR, "Done configuring SQLite handle\n");

    return md_sqlite_configure(mws, db_filename, node_id, interval,
            num_events, meta_prefix, gps_prefix, monitor_prefix, usage_prefix,
            system_prefix, ntp_fix_file);
}

static uint8_t md_sqlite_check_valid_tstamp(struct md_writer_sqlite *mws)
{
    struct timeval tv;
    uint64_t real_boot_time;

    gettimeofday(&tv, NULL);

    if (mws->ntp_fix_file[0] && access(mws->ntp_fix_file, F_OK)) {
        return RETVAL_FAILURE;
    }

    md_sqlite_get_real_boot_time(mws, &real_boot_time);

    META_PRINT_SYSLOG(mws->parent, LOG_INFO, "Real boot %" PRIu64 " orig boot %" PRIu64 "\n", real_boot_time, mws->orig_boot_time);

    if (md_sqlite_update_timestamp_db(mws, UPDATE_EVENT_TSTAMP,
                mws->orig_boot_time, real_boot_time) ||
        md_sqlite_update_timestamp_db(mws, UPDATE_UPDATES_TSTAMP,
            mws->orig_boot_time, real_boot_time) ||
        md_sqlite_update_timestamp_db(mws, UPDATE_SYSTEM_TSTAMP,
            mws->orig_boot_time, real_boot_time) ||
        md_sqlite_update_timestamp_db(mws, UPDATE_GPS_TSTAMP,
            mws->orig_boot_time, real_boot_time)) {
        META_PRINT_SYSLOG(mws->parent, LOG_ERR, "Could not update tstamp in database\n");
        return RETVAL_FAILURE;
    }

    //Update data usage too

    mws->valid_timestamp = 1;
    return RETVAL_SUCCESS;
}

static uint8_t md_sqlite_check_session_id(struct md_writer_sqlite *mws)
{
    if (system_helpers_read_session_id(mws->session_id_file,
        &(mws->session_id), &(mws->session_id_multip))) {
        mws->timeout_handle->intvl = DEFAULT_TIMEOUT;
        return RETVAL_FAILURE;
    }

    if (md_sqlite_update_session_id_db(mws, UPDATE_EVENT_SESSION_ID) ||
        md_sqlite_update_session_id_db(mws, UPDATE_UPDATES_SESSION_ID) ||
        md_sqlite_update_session_id_db(mws, UPDATE_GPS_SESSION_ID) ||
        md_sqlite_update_session_id_db(mws, UPDATE_SYSTEM_SESSION_ID)) {
        META_PRINT_SYSLOG(mws->parent, LOG_ERR, "Could not update session id in database\n");
        mws->timeout_handle->intvl = DEFAULT_TIMEOUT;
        mws->session_id = 0;
        mws->session_id_multip = 0;
        return RETVAL_FAILURE;
    }

    free(mws->session_id_file);

    META_PRINT_SYSLOG(mws->parent, LOG_INFO, "Session ID values: %"PRIu64" %"PRIu64"\n",
            mws->session_id, mws->session_id_multip);

    return RETVAL_SUCCESS;
}

static void md_sqlite_handle(struct md_writer *writer, struct md_event *event)
{
    uint8_t retval = RETVAL_SUCCESS;
    struct md_writer_sqlite *mws = (struct md_writer_sqlite*) writer;

    switch (event->md_type) {
    case META_TYPE_CONNECTION:
        if (!mws->meta_prefix[0])
            return;

        retval = md_inventory_handle_conn_event(mws, (struct md_conn_event*) event);
        break;
    case META_TYPE_POS:
        if (!mws->gps_prefix[0])
            return;

        retval = md_inventory_handle_gps_event(mws, (struct md_gps_event*) event);
        if (!retval)
            mws->num_gps_events++;
        break;
    case META_TYPE_MUNIN:
        if (!mws->monitor_prefix[0])
            return;

        retval = md_sqlite_handle_munin_event(mws, (struct md_munin_event*) event);
        if (!retval)
            mws->num_munin_events++;
        break;
    case META_TYPE_SYSTEM:
        if (!mws->system_prefix[0])
            return;

        retval = md_inventory_handle_system_event(mws, (md_system_event_t*) event);

        if (!retval)
            mws->num_system_events++;
        break;
    default:
        META_PRINT_SYSLOG(mws->parent, LOG_INFO, "SQLite writer does not support event %u\n",
                event->md_type);
        return;
    }

    if (retval == RETVAL_FAILURE) {
        META_PRINT_SYSLOG(mws->parent, LOG_ERR, "Failed/ignored to insert JSON in DB\n");
        return;
    }

    //Something failed when dumping database, we have already rearmed timer for
    //checking again. So wait with trying new export etc. This also means that
    //we have a good timestamp
    if (mws->file_failed)
        return;

    //We have received an indication that a valid timestamp is present, so
    //check and update
    if (!mws->valid_timestamp) {
        if (md_sqlite_check_valid_tstamp(mws)) {
            printf("Invalid timestamp\n");
            return;
        }

        META_PRINT_SYSLOG(mws->parent, LOG_INFO, "Tstamp update from event\n");
    }

    if (mws->session_id_file && !mws->session_id) {
        if (md_sqlite_check_session_id(mws)) {
            printf("No session ID\n");
            return;
        }
    }

    //These two are exclusive. There is no point adding timeout if event_limit
    //is hit. This can happen if event_limit is 1. The reason we do not use
    //lte is that if a copy fails, we deal with that in a timeout
    if ((mws->num_conn_events + mws->num_gps_events + mws->num_munin_events +
                mws->num_usage_events) == mws->db_events) {
        md_sqlite_copy_db(mws, 0);
    } else if (!mws->timeout_added) {
        mde_start_timer(mws->parent->event_loop, mws->timeout_handle,
                        mws->db_interval);
        mws->timeout_added = 1;
    }
}

static void md_sqlite_handle_timeout(void *ptr)
{
    struct md_writer_sqlite *mws = ptr;

    if (mws->file_failed)
        META_PRINT_SYSLOG(mws->parent, LOG_INFO, "DB export retry\n");
    else
        META_PRINT_SYSLOG(mws->parent, LOG_INFO, "Will export DB after timeout\n");

    if(!mws->node_id) {
#ifdef OPENWRT
        mws->node_id = system_helpers_get_nodeid();
#else
        mws->node_id = system_helpers_get_nodeid(mws->node_id_file);
#endif
        if(mws->node_id) {
            META_PRINT_SYSLOG(mws->parent, LOG_INFO, "Got nodeid %d\n", mws->node_id);
        }
       
        if (!mws->node_id) {
            META_PRINT_SYSLOG(mws->parent, LOG_INFO, "No node id found\n");
            mws->timeout_handle->intvl = DEFAULT_TIMEOUT;
            return;
        }

        if (md_sqlite_update_nodeid_db(mws, UPDATE_EVENT_ID) ||
            md_sqlite_update_nodeid_db(mws, UPDATE_UPDATES_ID) ||
            md_sqlite_update_nodeid_db(mws, UPDATE_GPS_ID) ||
            md_sqlite_update_nodeid_db(mws, UPDATE_USAGE_ID) ||
            md_sqlite_update_nodeid_db(mws, UPDATE_SYSTEM_ID)) {
            META_PRINT_SYSLOG(mws->parent, LOG_ERR, "Could not update node id in database\n");

            mws->timeout_handle->intvl = DEFAULT_TIMEOUT;
            //TODO: Work-around for making sure we check the node id on next
            //timeout, we should do something nicer
            mws->node_id = 0;
            return;
        }
    }

    if (!mws->valid_timestamp) {
        if (md_sqlite_check_valid_tstamp(mws)) {
            mws->timeout_handle->intvl = DEFAULT_TIMEOUT;
            return;
        }
    }

    //Try to read session id
    if (mws->session_id_file && !mws->session_id) {
        if (md_sqlite_check_session_id(mws)) {
            mws->timeout_handle->intvl = DEFAULT_TIMEOUT;
            return;
        }
    }

    //TODO: Add support for the online backup API provided by SQLite
    //TODO: READDDDDD
    md_sqlite_copy_db(mws, 1);

    //If we get here, then timeout has been processed. If copy_db has failed,
    //timeout will be started by the iteration callback. Intvl is set to 0 here
    //to make sure that we reset interval
    mws->timeout_added = 0;
    mws->timeout_handle->intvl = 0;
}

void md_sqlite_setup(struct md_exporter *mde, struct md_writer_sqlite* mws) {
    mws->parent = mde;
    mws->init = md_sqlite_init;
    mws->handle = md_sqlite_handle;
    mws->itr_cb = md_sqlite_itr_cb;
    mws->usage = md_sqlite_usage;
    mws->api_version = 1;
}

