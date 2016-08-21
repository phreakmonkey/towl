#!/usr/bin/python2.7

import datetime
import sys
import time
import threading
import traceback
import SocketServer
from dnslib import *

import base64
import struct

# Port to listen on.  I use a high port to run as a non-root user and then
# map to it in iptables.  Alternatively change to port 53 and run as root.
PORT = 5300
SUBDOMAIN = '.foobar.example.com.'
LOGDIR = '/var/tmp'

def parseCovert(devid, data):
  dstr = data.upper().split('.')[0]
  print "dstr: %s" % dstr
  if dstr.startswith('L-'):
    dstr = dstr[2:]
    t = 'long'
  elif dstr.startswith('S-'):
    dstr = dstr[2:]
    t = 'telem'
  else:
    t = 'str'

  plen = len(dstr) % 8
  if plen:
    dstr = dstr + '=' * (8 - plen)
  res = base64.b32decode(dstr)

  with open(LOGDIR + '/%s.log' % devid, 'a') as lf:
    if t == 'long':
      res = struct.unpack('>i', res)[0]
      print 'Decoded long: %d' % res
      lf.write('%s : %d\n' % (datetime.datetime.now(), res))
      return 99

    if t == 'telem':
      print 'len: %d' % len(res)
      tm,lat,lon,spd,sats,id,mode = struct.unpack('IiiBBBB', res)
      print 'Decoded telem: %d %d %d %d' % (tm, lat, lon, spd)
      lf.write('%s,%d,%f,%f,%d,%d,%d\n' % (datetime.datetime.now(), tm,
                                        lat/1000000.0, lon/1000000.0,
                                        spd, sats, mode))
      return id

    print 'Decoded: %s' % res
    lf.write('%s : %s\n' % (datetime.datetime.now(), res))
    return len(res)


def dns_response(data):
    request = DNSRecord.parse(data)

    print request

    reply = DNSRecord(DNSHeader(id=request.header.id, qr=1, aa=1, ra=1), q=request.q)

    qname = request.q.qname
    qn = str(qname)
    qtype = request.q.qtype
    qt = QTYPE[qtype]
    print "qt is %s, qn is %s" % (qt, qn)

    if qt == 'A' and qn.lower().endswith(SUBDOMAIN):
      devid = qn.lower().split('.')[1]
      # Sanity check: devid must be xnn where x = letter and n = number
      if len(devid) != 3:
        return
      if (devid[0] not in string.letters or
          devid[1] not in string.digits or
          devid[2] not in string.digits):
          return
      rIP = '10.0.11.%d' % parseCovert(devid, qn)
      reply.add_answer(
          RR(rname=qname, rtype=QTYPE.A, rclass=1, ttl=300, rdata=A(rIP)))
    return reply.pack()


class BaseRequestHandler(SocketServer.BaseRequestHandler):

    def get_data(self):
        raise NotImplementedError

    def send_data(self, data):
        raise NotImplementedError

    def handle(self):
        now = datetime.datetime.utcnow().strftime('%Y-%m-%d %H:%M:%S.%f')
        print "\n\n%s request %s (%s %s):" % (self.__class__.__name__[:3], now, self.client_address[0],
                                               self.client_address[1])
        try:
            data = self.get_data()
            #print len(data), data.encode('hex')  # repr(data).replace('\\x', '')[1:-1]
            self.send_data(dns_response(data))
        except Exception:
            traceback.print_exc(file=sys.stderr)


class TCPRequestHandler(BaseRequestHandler):

    def get_data(self):
        data = self.request.recv(8192).strip()
        sz = int(data[:2].encode('hex'), 16)
        if sz < len(data) - 2:
            raise Exception("Wrong size of TCP packet")
        elif sz > len(data) - 2:
            raise Exception("Too big TCP packet")
        return data[2:]

    def send_data(self, data):
        sz = hex(len(data))[2:].zfill(4).decode('hex')
        return self.request.sendall(sz + data)


class UDPRequestHandler(BaseRequestHandler):

    def get_data(self):
        return self.request[0].strip()

    def send_data(self, data):
        return self.request[1].sendto(data, self.client_address)


if __name__ == '__main__':
    print "Starting nameserver..."

    servers = [
        SocketServer.ThreadingUDPServer(('', PORT), UDPRequestHandler),
        SocketServer.ThreadingTCPServer(('', PORT), TCPRequestHandler),
    ]
    for s in servers:
        thread = threading.Thread(target=s.serve_forever)  # that thread will start one more thread for each request
        thread.daemon = True  # exit the server thread when the main thread terminates
        thread.start()
        print "%s server loop running in thread: %s" % (s.RequestHandlerClass.__name__[:3], thread.name)

    try:
        while 1:
            time.sleep(1)
            sys.stderr.flush()
            sys.stdout.flush()

    except KeyboardInterrupt:
        pass
    finally:
        for s in servers:
            s.shutdown()
