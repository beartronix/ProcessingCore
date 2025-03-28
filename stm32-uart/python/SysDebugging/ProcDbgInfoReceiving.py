#!/usr/bin/env python

import serial
import sys
import socket
import select
from threading import Thread, Lock
from typing import List
from time import sleep

from signal import signal, SIGINT

args = sys.argv

pathComPort = '/dev/ttyUSB0'
if len(args) > 1:
	pathComPort = args[1]

comPort = serial.Serial(pathComPort, 38400, timeout=0.4)
comPort.reset_input_buffer()


ID_TREE = 'T'
ID_LOG  = 'L'

class Srv:
	def __init__(self, port):
		self.port = port

		self.shutdown = False

		self.actConns : List[socket.socket] = []

		# self.main()
		self.thr = Thread(target=self.main, name=str(self.port))
		self.thr.start()

	def main(self):
		self.sock = socket.socket(socket.AF_INET, socket.SocketKind.SOCK_STREAM)
		self.sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
		self.sock.setblocking(0)
		self.sock.bind(('0.0.0.0', self.port))
		self.sock.listen(2)

		print(f'Listening on port {self.port}...')
		while not self.shutdown:
			sockRdOk, sockWrOk, sockErr = select.select([self.sock], [], [], 0.005)
			if not self.sock in sockRdOk:
				continue

			conn, addr = self.sock.accept()
			print(f'Accepted Connection from {addr[0]}:{addr[1]}')
			self.actConns.append(conn)

		for conn in self.actConns:
			self.disconnect(conn)

	def disconnect(self, conn : socket.socket):
		self.actConns.remove(conn)
		conn.shutdown(socket.SHUT_RDWR)
		conn.close()

	def send(self, data):
		sockRdOk, sockWrOk, sockErr = select.select([], self.actConns, [], 0.005)
		for conn in sockWrOk:
			conn : socket.socket = conn
			try:
				conn.sendall(data)
			except Exception as e:
				print(f"Conn shutdown: {e}")
				self.disconnect(conn)

servers = {
	ID_TREE : Srv(3030),
	ID_LOG  : Srv(3031),
}

while True:
	try:
		msgId = comPort.read(1).decode()
		data = b''
		if msgId == ID_LOG:
			data = b'\033[37m' + comPort.read_until(b'\r\n')
		elif msgId == ID_TREE:
			data = b'\033\143' + comPort.read_until(b'\r\n\r\n')
		else:
			# print("Got CRAP")
			comPort.reset_input_buffer()

		if data:
			servers[msgId].send(data)
		# print(data)
	except KeyboardInterrupt as k:
		for srv in servers.values():
			print(f"Shutdown server on port {srv.thr.name}")
			srv.shutdown = True
			srv.thr.join()
		sys.exit(1)
	except Exception as exc:
		print(f"EXCEPTION (id {msgId}): {exc}")
		# sleep(0.001)

	continue
	if data[0] == 'T': # Tree
		pass
	elif data[0] == 'L': # Log
		pass
	else:
		print(f"############### ERROR DATA: {data}")

