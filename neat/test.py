#!/usr/bin/python

import zmq
import sys

port = "5556"
if len(sys.argv) > 1:
    port = sys.argv[1]
context = zmq.Context().instance()
socket = context.socket(zmq.SUB)
socket.connect ("tcp://172.17.0.1:" + port)

socket.setsockopt(zmq.SUBSCRIBE, '')

while True:
    string = socket.recv()
    print string
    sys.stdout.flush()
