#!/usr/bin/env python

import array
import binascii
import zmq
import struct

port = 28332

zmqContext = zmq.Context()
zmqSubSocket = zmqContext.socket(zmq.SUB)
zmqSubSocket.setsockopt(zmq.SUBSCRIBE, b"hashblock")
zmqSubSocket.setsockopt(zmq.SUBSCRIBE, b"hashtx")
zmqSubSocket.setsockopt(zmq.SUBSCRIBE, b"hashtxlock")
zmqSubSocket.setsockopt(zmq.SUBSCRIBE, b"rawblock")
zmqSubSocket.setsockopt(zmq.SUBSCRIBE, b"rawtx")
zmqSubSocket.setsockopt(zmq.SUBSCRIBE, b"rawtxlock")
zmqSubSocket.connect("tcp://127.0.0.1:%i" % port)

        self.zmqSubSocket = self.zmqContext.socket(zmq.SUB)
        self.zmqSubSocket.setsockopt_string(zmq.SUBSCRIBE, "hashblock")
        self.zmqSubSocket.setsockopt_string(zmq.SUBSCRIBE, "hashchainlock")
        self.zmqSubSocket.setsockopt_string(zmq.SUBSCRIBE, "hashtx")
        self.zmqSubSocket.setsockopt_string(zmq.SUBSCRIBE, "hashtxlock")
        self.zmqSubSocket.setsockopt_string(zmq.SUBSCRIBE, "rawblock")
        self.zmqSubSocket.setsockopt_string(zmq.SUBSCRIBE, "rawchainlock")
        self.zmqSubSocket.setsockopt_string(zmq.SUBSCRIBE, "rawchainlocksig")
        self.zmqSubSocket.setsockopt_string(zmq.SUBSCRIBE, "rawtx")
        self.zmqSubSocket.setsockopt_string(zmq.SUBSCRIBE, "rawtxlock")
        self.zmqSubSocket.setsockopt_string(zmq.SUBSCRIBE, "rawtxlocksig")
        self.zmqSubSocket.connect("tcp://127.0.0.1:%i" % port)

    async def handle(self) :
        msg = await self.zmqSubSocket.recv_multipart()
        topic = msg[0]
        body = msg[1]
        sequence = "Unknown";

        if len(msg[-1]) == 4:
          msgSequence = struct.unpack('<I', msg[-1])[-1]
          sequence = str(msgSequence)

        if topic == "hashblock":
            print('- HASH BLOCK ('+sequence+') -')
            print(body.hex())
        elif topic == "hashtx":
            print ('- HASH TX ('+sequence+') -')
            print(body.hex())
        elif topic == "hashtxlock":
            print('- HASH TX LOCK ('+sequence+') -')
            print(body.hex())
        elif topic == "rawblock":
            print('- RAW BLOCK HEADER ('+sequence+') -')
            print(binascii.hexlify(body[:80]).decode("utf-8"))
        elif topic == b"rawchainlock":
            print('- RAW CHAINLOCK ('+sequence+') -')
            print(binascii.hexlify(body[:80]).decode("utf-8"))
        elif topic == b"rawchainlocksig":
            print('- RAW CHAINLOCK SIG ('+sequence+') -')
            print(binascii.hexlify(body[:80]).decode("utf-8"))
        elif topic == b"rawtx":
            print('- RAW TX ('+sequence+') -')
            print(body.hex())
        elif topic == "rawtxlock":
            print('- RAW TX LOCK ('+sequence+') -')
            print(binascii.hexlify(body).decode("utf-8"))
        elif topic == b"rawtxlocksig":
            print('- RAW TX LOCK SIG ('+sequence+') -')
            print(binascii.hexlify(body).decode("utf-8"))
        # schedule ourselves to receive the next message
        asyncio.ensure_future(self.handle())

except KeyboardInterrupt:
    zmqContext.destroy()
