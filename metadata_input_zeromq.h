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
#include "metadata_exporter.h"

#define ZMQ_NL_INTERFACE_TOPIC "CELERWAY.NL.INTERFACE"

#define ZMQ_NL_RADIOEVENT_TOPIC "CELERWAY.NL.RADIOEVENT"

#define ZMQ_NL_GPS_TOPIC "CELERWAY.NL.GPS"

#define ZMQ_NL_SYSTEMEVENT_TOPIC "CELERWAY.NL.SYSTEMEVENT"

#define ZMQ_DLB_METADATA_TOPIC "CELERWAY.DLB.METADATA"

#define ZMQ_DLB_DATAUSAGE_TOPIC "CELERWAY.DLB.DATAUSAGE"

struct backend_epoll_handle;

struct md_input_zeromq {
    MD_INPUT;
    struct backend_epoll_handle *event_handle;
    uint32_t md_zmq_mask;
    void* zmq_ctx;
    void* zmq_socket;
    int zmq_fd;
    struct md_conn_event *mce;
    struct md_iface_event *mie;
    struct md_radio_event *mre;
    md_system_event_t *mse;
};

void md_zeromq_input_usage();
void md_zeromq_input_setup(struct md_exporter *mde, struct md_input_zeromq *miz);
