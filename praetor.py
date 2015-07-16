#!/usr/bin/env python
# -*- coding: utf-8 -*-

from __future__ import division
from PyQt4 import QtGui, QtCore
from threading import Thread
from socket import socket, AF_INET, SOCK_DGRAM, SHUT_RD
import sys

UDP_HOST_PORT = '127.0.0.1', 23867
FPS = 29.97

WAIT_TEXT = 'Waiting\nfor MPlayer'
TCR_FMT = '{h}:{m:02}:{s:02}.{ms:03}\nFrame: {frame}'

SOCKETS = []

def main():
    app = QtGui.QApplication(sys.argv)
    w = MainWindow()
    w.show()
    rv = app.exec_()
    for s in SOCKETS:
        try:
            s.shutdown(SHUT_RD)
        except Exception:
            pass
    sys.exit(rv)

class MplayerThread(Thread):
    def __init__(self, tcr, port=23867):
        Thread.__init__(self)
        self.tcr = tcr

    def run(self):
        self.tcr.setText(WAIT_TEXT)
        s = socket(AF_INET, SOCK_DGRAM)
        s.bind(UDP_HOST_PORT)
        SOCKETS.append(s)
        while True:
            data = s.recv(16)
            if not data:
                break
            elif data == 'bye':
                self.tcr.setText(WAIT_TEXT)
            else:
                ts = float(data)
                its = int(ts)
                self.tcr.setText(TCR_FMT.format(
                    h=its // 3600, m=its // 60, s=its % 60,
                    ms=int((ts * 1000) % 1000), frame=int(ts * FPS)))

class MainWindow(QtGui.QMainWindow):
    def __init__(self, parent = None):
        QtGui.QWidget.__init__(self, parent)
        self.setWindowTitle('Praetor')
        tcr = QtGui.QLabel()
        vbox = QtGui.QVBoxLayout()
        vbox.addWidget(tcr)
        w = QtGui.QWidget(self)
        w.setLayout(vbox)
        self.setCentralWidget(w)
        MplayerThread(tcr).start()

if __name__ == '__main__':
    main()
