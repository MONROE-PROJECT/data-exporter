#Standard stuff
cmake_minimum_required(VERSION 2.6)
project(meta_exporter)

set(CMAKE_C_FLAGS "-O1 -Wall -std=gnu99 -s")

set(LIBS pthread mnl)
set(SOURCE
    metadata_exporter.c
    backend_event_loop.c
    metadata_utils.c
    system_helpers.c
    metadata_input_nl_zmq_common.c
    lib/minmea.c)


if (TARGET_OWRT)
    MESSAGE(STATUS "OpenWRT build, will include UCI")
    add_definitions("-DOPENWRT")
    LIST(APPEND LIBS uci json-c)
    add_definitions("-DJSON_LOC=<json-c/json.h>")
elseif(TARGET_NNE)
    MESSAGE(STATUS "NNE build")
    LIST(APPEND LIBS json)
    #CMake does magic to convert this value
    add_definitions("-DJSON_LOC=<json/json.h>")
else()
    MESSAGE(STATUS "Standard build")
    add_definitions("-DMONROE")
    LIST(APPEND LIBS json-c)
    add_definitions("-DJSON_LOC=<json-c/json.h>")
endif()

if (SQLITE3)
    set(LIBS ${LIBS} sqlite3)
    set(SOURCE ${SOURCE} 
        metadata_writer_sqlite.c
        metadata_writer_sqlite_helpers.c
        metadata_writer_json_helpers.c
        metadata_writer_inventory_conn.c
        metadata_writer_inventory_gps.c
        metadata_writer_inventory_system.c
        metadata_writer_sqlite_monitor.c)
    add_definitions("-DSQLITE_SUPPORT")
endif()

if (NNE)
    set(SOURCE ${SOURCE}
        metadata_writer_nne.c)
    add_definitions("-DNNE_SUPPORT")
endif()

if (NEAT)
    set(SOURCE ${SOURCE}
        metadata_writer_neat.c)
    add_definitions("-DNEAT_SUPPORT")
endif()

if (ZEROMQ_WRITER)
    set(LIBS ${LIBS} zmq)
    set(SOURCE ${SOURCE}
        metadata_writer_zeromq.c
        metadata_writer_zeromq_monroe.c
        metadata_writer_zeromq_nne.c)
    add_definitions("-DZEROMQ_SUPPORT_WRITER")
endif()

if (FILE)
    set(SOURCE ${SOURCE}
        metadata_writer_file.c)
    add_definitions("-DFILE_SUPPORT")
endif()

if (ZEROMQ_INPUT)
    set(LIBS ${LIBS} zmq)
    set(SOURCE ${SOURCE}
        metadata_input_zeromq.c)
    add_definitions("-DZEROMQ_SUPPORT_INPUT")
endif()

if (ZEROMQ_RELAY)
    set(LIBS ${LIBS} zmq)
    set(SOURCE ${SOURCE}
        metadata_input_zeromq_relay.c)
    add_definitions("-DZEROMQ_SUPPORT_INPUT_RELAY")
endif()

if (GPSD)
    set(LIBS ${LIBS} gps)
    set(SOURCE ${SOURCE}
        metadata_input_gpsd.c)
    add_definitions("-DGPSD_SUPPORT")
endif()

if (MUNIN)
    set(SOURCE ${SOURCE}
        metadata_input_munin.c)
    add_definitions("-DMUNIN_SUPPORT")
endif()

if (SYSEVENT)
    set(SOURCE ${SOURCE}
        metadata_input_sysevent.c)
    add_definitions("-DSYSEVENT_SUPPORT")
endif()

if (GPS_NSB)
    set(SOURCE ${SOURCE}
        metadata_input_gps_nsb.c)
    add_definitions("-DGPS_NSB_SUPPORT")
endif()

set(CPACK_GENERATOR "DEB")
set(CPACK_PACKAGE_VERSION_MAJOR "0")
set(CPACK_PACKAGE_VERSION_MINOR "1")
set(CPACK_PACKAGE_VERSION_PATCH "0")
set(CPACK_DEBIAN_PACKAGE_MAINTAINER "Kristian R. Evensen <kristrev@celerway.com>")
set(CPACK_DEBIAN_PACKAGE_CONTROL_EXTRA "${PROJECT_SOURCE_DIR}/../files/linux/postinst")
set (CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
include(CPack)

#set(CMAKE_C_LINK_EXECUTABLE "${CMAKE_C_LINK_EXECUTABLE} -s")
add_executable(meta_exporter ${SOURCE})
target_link_libraries(meta_exporter ${LIBS})


if (TARGET_OWRT)
    install(TARGETS meta_exporter RUNTIME DESTINATION bin)
else()
    install(TARGETS meta_exporter RUNTIME DESTINATION /usr/sbin)
    #    install(FILES ${PROJECT_SOURCE_DIR}/../files/metadata_exporter_timeloop.sh DESTINATION
    #   /usr/local/sbin PERMISSIONS OWNER_EXECUTE OWNER_READ OWNER_WRITE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE)
    #install(FILES ${PROJECT_SOURCE_DIR}/../files/metadata-ubuntu.init DESTINATION
    #   /etc/init/ PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ WORLD_READ
    #   RENAME metadata.conf)
    #install(FILES ${PROJECT_SOURCE_DIR}/../files/metadata-exporter-ubuntu.init DESTINATION
    #    /etc/init/ PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ WORLD_READ
    #    RENAME metadata_exporter.conf)
endif()
