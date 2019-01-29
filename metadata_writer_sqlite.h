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

#pragma once

#include <sys/time.h>
#include <sqlite3.h>
#include "metadata_exporter.h"

#define DEFAULT_TIMEOUT 5000
#define TIMEOUT_FILE 1000
#define EVENT_LIMIT 10
#define MAX_PATH_LEN 128
#define FAKE_UPDATE_LIMIT 120

#define CREATE_SQL          "CREATE TABLE IF NOT EXISTS NetworkEvent(" \
                            "NodeId INTEGER NOT NULL," \
                            "SessionId INTEGER NOT NULL," \
                            "SessionIdMultip INTEGER NOT NULL," \
                            "Timestamp INTEGER NOT NULL," \
                            "Sequence INTEGER NOT NULL," \
                            "L3SessionId INTEGER NOT NULL," \
                            "L4SessionId INTEGER NOT NULL DEFAULT 0," \
                            "EventType INTEGER NOT NULL," \
                            "EventParam INTEGER NOT NULL," \
                            "EventValue INTEGER," \
                            "HasIp INTEGER," \
                            "Connectivity INTEGER," \
                            "ConnectionMode INTEGER," \
                            "Quality INTEGER," \
                            "InterfaceType INTEGER NOT NULL," \
                            "InterfaceIdType INTEGER NOT NULL," \
                            "InterfaceId TEXT NOT NULL," \
                            "NetworkProvider INT," \
                            "NetworkAddressFamily INTEGER NOT NULL," \
                            "NetworkAddress TEXT NOT NULL," \
                            "PRIMARY KEY(SessionId,SessionIdMultip,Timestamp,"\
                            "Sequence))"

#define CREATE_UPDATE_SQL   "CREATE TABLE IF NOT EXISTS NetworkUpdates(" \
                            "NodeId INTEGER NOT NULL," \
                            "SessionId INTEGER NOT NULL," \
                            "SessionIdMultip INTEGER NOT NULL," \
                            "Timestamp INTEGER NOT NULL," \
                            "Sequence INTEGER NOT NULL," \
                            "L3SessionId INTEGER NOT NULL," \
                            "L4SessionId INTEGER NOT NULL DEFAULT 0," \
                            "EventType INTEGER NOT NULL," \
                            "EventParam INTEGER NOT NULL," \
                            "HasIp INTEGER," \
                            "Connectivity INTEGER," \
                            "ConnectionMode INTEGER," \
                            "Quality INTEGER," \
                            "InterfaceType INTEGER NOT NULL," \
                            "InterfaceId TEXT NOT NULL," \
                            "NetworkAddressFamily INTEGER NOT NULL," \
                            "NetworkAddress TEXT NOT NULL," \
                            "NetworkProvider INT," \
                            "EventValueStr TEXT, " \
                            "PRIMARY KEY(SessionId,SessionIdMultip,"\
                            "L3SessionId,L4SessionId,InterfaceId,"\
                            "NetworkAddressFamily,NetworkAddress))"

#define CREATE_GPS_SQL      "CREATE TABLE IF NOT EXISTS GpsUpdate(" \
                            "NodeId INTEGER NOT NULL," \
                            "BootCount INTEGER,"\
                            "BootMultiplier INTEGER,"\
                            "Timestamp INTEGER NOT NULL," \
                            "Sequence INTEGER NOT NULL," \
                            "EventType INTEGER NOT NULL," \
                            "EventParam INTEGER NOT NULL," \
                            "Latitude REAL NOT NULL," \
                            "Longitude REAL NOT NULL," \
                            "Altitude REAL," \
                            "Speed REAL," \
                            "SatelliteCount INTEGER," \
                            "PRIMARY KEY(NodeId) ON CONFLICT REPLACE)"

#define CREATE_MONITOR_SQL  "CREATE TABLE IF NOT EXISTS MonitorEvents(" \
                            "NodeId      INTEGER NOT NULL," \
                            "Timestamp   INTEGER NOT NULL," \
                            "Sequence    INTEGER NOT NULL," \
                            "Boottime    INTEGER NOT NULL," \
                            "PRIMARY KEY(NodeId,Timestamp,Sequence))"

#define CREATE_USAGE_SQL    "CREATE TABLE IF NOT EXISTS DataUse(" \
                            "DeviceId TEXT NOT NULL," \
                            "NetworkAddressFamily INTEGER NOT NULL," \
                            "EventType INTEGER NOT NULL," \
                            "EventParam INTEGER NOT NULL," \
                            "SimCardIccid TEXT NOT NULL," \
                            "SimCardImsi TEXT NOT NULL," \
                            "Timestamp INTEGER NOT NULL," \
                            "RxData INTEGER NOT NULL,"\
                            "TxData INTEGER NOT NULL," \
                            "PRIMARY KEY(DeviceId,NetworkAddressFamily,SimCardIccid,SimCardImsi,Timestamp))"

#define CREATE_REBOOT_SQL   "CREATE TABLE IF NOT EXISTS RebootEvent(" \
                            "NodeId INTEGER NOT NULL," \
                            "BootCount INTEGER," \
                            "BootMultiplier INTEGER," \
                            "Timestamp INTEGER NOT NULL," \
                            "Sequence INTEGER NOT NULL," \
                            "EventType INTEGER NOT NULL," \
                            "DeviceId TEXT NOT NULL," \
                            "PRIMARY KEY(BootCount,BootMultiplier,Timestamp,Sequence))"

#define INSERT_EVENT        "INSERT INTO NetworkEvent(NodeId,SessionId,"\
                            "SessionIdMultip,Timestamp,Sequence,L3SessionId,"\
                            "L4SessionId,EventType,EventParam,EventValue,"\
                            "HasIp,Connectivity,ConnectionMode,Quality,InterfaceType,"\
                            "InterfaceIdType,InterfaceId,NetworkProvider,NetworkAddressFamily,"\
                            "NetworkAddress) " \
                            "VALUES (?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?)"

#define INSERT_UPDATE       "INSERT INTO NetworkUpdates(NodeId,SessionId,"\
                            "SessionIdMultip,Timestamp,Sequence,L3SessionId,"\
                            "L4SessionId,EventType,EventParam,HasIp,Connectivity,ConnectionMode,Quality"\
                            ",InterfaceType,InterfaceId,NetworkAddressFamily,NetworkAddress,NetworkProvider,EventValueStr) " \
                            "VALUES (?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?)"

#define INSERT_GPS_EVENT    "INSERT INTO GpsUpdate(NodeId,BootCount" \
                            ",BootMultiplier,Timestamp" \
                            ",Sequence,EventType,EventParam,Latitude,Longitude" \
                            ",Altitude,Speed,SatelliteCount) " \
                            "VALUES (?,?,?,?,?,?,?,?,?,?,?,?)"

#define INSERT_MONITOR_EVENT "INSERT INTO MonitorEvents(NodeId,Timestamp" \
                             ",Sequence,Boottime) " \
                             "VALUES (?,?,?,?)"

#define INSERT_USAGE        "INSERT INTO DataUse(DeviceId,NetworkAddressFamily,EventType,EventParam" \
                            ",SimCardIccid" \
                            ",SimCardImsi,Timestamp,RxData,TxData) " \
                            "VALUES (?,?,?,?,?,?,?,?,?)"

#define INSERT_REBOOT_EVENT "INSERT INTO RebootEvent(NodeId, BootCount," \
                            "BootMultiplier, Timestamp, Sequence, EventType, DeviceId)"\
                            "VALUES (?,?,?,?,?,16,?)"

#define SELECT_LAST_UPDATE  "SELECT HasIp,Connectivity,ConnectionMode,Quality "\
                            " FROM NetworkUpdates WHERE "\
                            "L3SessionId=? AND "\
                            "L4SessionId=? AND InterfaceId=? AND "\
                            "NetworkAddressFamily=? AND "\
                            "NetworkAddress=? ORDER BY Timestamp DESC LIMIT 1;"

//The reason NetworkAddress is used here, is that we first identify by L3/L4
//session. However, it might be that multiple addresses on same interface has
//the same L3/L4 ID (since we use timestamp). (L3 ID,address) is unique on one
//interface.
//
//We can have the same L3/L4 IDs on multiple nodes. On one node, the same
//address can have multiple L3/L4 IDs (two mf823 for example). However, same
//address (+prefix) and same interface is guaranteed to have unique L3/L4
#define UPDATE_UPDATE       "UPDATE NetworkUpdates SET " \
                            "Timestamp=?,HasIp=?,Connectivity=?,ConnectionMode=?," \
                            "Quality=?,EventValueStr=? " \
                            "WHERE "\
                            "L3SessionId=? AND L4SessionId=? " \
                            "AND NetworkAddressFamily=? AND "\
                            "NetworkAddress=? AND InterfaceId=?"

#define UPDATE_USAGE        "UPDATE DataUse SET " \
                            "RxData = RxData + ?, TxData = TxData + ? " \
                            "WHERE " \
                            "DeviceId=? AND NetworkAddressFamily=? AND SimCardIccid=? AND SimCardImsi=? AND Timestamp=?"

#define UPDATE_EVENT_ID     "UPDATE NetworkEvent SET " \
                            "NodeId=? "\
                            "WHERE NodeId=0"

#define UPDATE_UPDATES_ID   "UPDATE NetworkUpdates SET " \
                            "NodeId=? "\
                            "WHERE NodeId=0"

#define UPDATE_SYSTEM_ID   "UPDATE RebootEvent SET " \
                            "NodeId=? "\
                            "WHERE NodeId=0"

#define UPDATE_GPS_ID   "UPDATE GpsUpdate SET " \
                            "NodeId=? "\
                            "WHERE NodeId=0"

#define UPDATE_EVENT_TSTAMP "UPDATE NetworkEvent SET " \
                            "Timestamp = (Timestamp - ?) + ? "\
                            "WHERE Timestamp < ?"

#define UPDATE_UPDATES_TSTAMP     "UPDATE NetworkUpdates SET " \
                                  "Timestamp = (Timestamp - ?) + ? "\
                                  "WHERE Timestamp < ?"

#define UPDATE_SYSTEM_TSTAMP     "UPDATE RebootEvent SET " \
                                  "Timestamp = (Timestamp - ?) + ? "\
                                  "WHERE Timestamp < ?"

#define UPDATE_GPS_TSTAMP     "UPDATE GpsUpdate SET " \
                              "Timestamp = (Timestamp - ?) + ? "\
                              "WHERE Timestamp < ?"

#define UPDATE_EVENT_SESSION_ID "UPDATE NetworkEvent SET "\
                                "SessionId=?,SessionIdMultip=? "\
                                "WHERE SessionId = 0"

#define UPDATE_UPDATES_SESSION_ID "UPDATE NetworkUpdates SET "\
                                  "SessionId=?,SessionIdMultip=? "\
                                  "WHERE SessionId = 0"

#define UPDATE_SYSTEM_SESSION_ID "UPDATE RebootEvent SET "\
                                  "BootCount=?,BootMultiplier=? "\
                                  "WHERE BootCount = 0"

#define UPDATE_GPS_SESSION_ID "UPDATE GpsUpdate SET "\
                              "BootCount=?,BootMultiplier=? "\
                              "WHERE BootCount = 0"

#define DELETE_TABLE         "DELETE FROM NetworkEvent"

#define DELETE_NW_UPDATE     "DELETE FROM NetworkUpdates WHERE Timestamp < ?"

#define DELETE_GPS_TABLE     "DELETE FROM GpsUpdate"

#define DELETE_MONITOR_TABLE "DELETE FROM MonitorEvents"

#define DELETE_USAGE_TABLE "DELETE FROM DataUse"

#define DELETE_SYSTEM_TABLE "DELETE FROM RebootEvent"

//Define statements for JSON export
#define DUMP_EVENTS_JSON    "SELECT * FROM NetworkEvent WHERE Timestamp>=? ORDER BY TimeStamp"

#define DUMP_UPDATES_JSON   "SELECT * FROM NetworkUpdates WHERE Timestamp>=? ORDER BY TimeStamp"

#define DUMP_GPS_JSON       "SELECT * FROM GpsUpdate ORDER BY Timestamp"

#define DUMP_MONITOR_JSON   "SELECT * FROM MonitorEvents ORDER BY Timestamp"

#define DUMP_USAGE_JSON     "SELECT * FROM DataUse"

#define DUMP_SYSTEM_JSON    "SELECT * FROM RebootEvent"


struct md_event;
struct md_writer;
struct backend_timeout_handle;

struct md_writer_sqlite {
    MD_WRITER;

    uint64_t dump_tstamp;
    uint64_t last_msg_tstamp;
    uint64_t last_gps_insert;
    uint64_t orig_boot_time;
    uint64_t orig_uptime;
    uint64_t orig_raw_time;

    //TODO: Consider moving this to the generic writer struct if need be
    //These values keep track of the unique session id (and multiplier), which
    //are normally assumed to be the boot counter (+ multiplier)
    uint64_t session_id;
    uint64_t session_id_multip;

    float gps_speed;

    size_t meta_prefix_len,  gps_prefix_len,  monitor_prefix_len,
           usage_prefix_len, system_prefix_len;

    sqlite3 *db_handle;

    sqlite3_stmt *insert_event, *insert_update;
    sqlite3_stmt *update_update, *dump_update;
    sqlite3_stmt *delete_table, *dump_table;
    sqlite3_stmt *last_update;

    sqlite3_stmt *insert_gps, *delete_gps, *dump_gps;
    sqlite3_stmt *insert_monitor, *delete_monitor, *dump_monitor;

    sqlite3_stmt *insert_usage, *update_usage, *dump_usage, *delete_usage;

    sqlite3_stmt *insert_system, *dump_system, *delete_system;

    char *session_id_file;
    char *node_id_file;
    const char *last_conn_tstamp_path;
    struct backend_timeout_handle *timeout_handle;
    struct timeval first_fake_update;

    uint32_t node_id;
    uint32_t db_interval;
    uint32_t db_events;
    uint32_t num_conn_events;
    uint32_t num_gps_events;
    uint32_t num_munin_events;
    uint32_t num_usage_events;
    uint32_t num_system_events;

    uint8_t timeout_added;
    uint8_t file_failed;
    uint8_t do_fake_updates;
    uint8_t valid_timestamp;

    char meta_prefix[128], gps_prefix[128], monitor_prefix[128],
        usage_prefix[128], system_prefix[128], ntp_fix_file[128];

    uint8_t api_version;
    uint8_t delete_conn_update;
};

void md_sqlite_usage();
void md_sqlite_setup(struct md_exporter *mde, struct md_writer_sqlite* mws);
