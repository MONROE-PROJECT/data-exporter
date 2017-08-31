#!/usr/bin/env python

import zmq

if __name__ == "__main__":
    print "Starting metadata message broker"
    print "If no traffic is received, check firewall settings!!"
    print "Connect clients to ZeroMQ port tcp://172.17.0.1:5577 to receive messages."    

    context = zmq.Context()

    # This is where generic Metadata is collected from
    frontend = context.socket(zmq.XSUB)
    frontend.bind("tcp://172.17.0.1:5576")
    frontend.connect("tcp://172.17.0.1:5556")

    # Not needed for XSUB / XPUB
    #frontend.setsockopt(zmq.SUBSCRIBE, '')

    # This is the internal forward for MONROE Metadata
    backend = context.socket(zmq.XPUB)
    backend.bind("tcp://172.17.0.1:5577")

    try:
        zmq.proxy(frontend, backend)
    finally:
        frontend.close()
        backend.close()
        context.term()
