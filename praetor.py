#!/usr/bin/env python
# -*- coding: utf-8 -*-

from __future__ import division, print_function
from PyQt4 import QtGui, QtCore
from threading import Thread
from socket import socket, AF_INET, SOCK_DGRAM, SHUT_RD
import sys

UDP_HOST_PORT = '127.0.0.1', 23867
FPS = 29.97

WAIT_TEXT = 'Waiting for MPlayer'
TCR_FMT = '{h}:{m:02}:{s:02}.{ms:03} // Frame: {frame}'

SOCKETS = []

def main():
    app = QtGui.QApplication(sys.argv)
    if len(sys.argv) < 2:
        print('Usage: {0} <log.txt>'.format(sys.argv[0]), file=sys.stderr)
        sys.exit(1)
    w = MainWindow(sys.argv[1])
    w.show()
    rv = app.exec_()
    for s in SOCKETS:
        try:
            s.shutdown(SHUT_RD)
        except Exception:
            pass
    sys.exit(rv)

class MplayerThread(Thread):
    def __init__(self, mw):
        Thread.__init__(self)
        self.mw = mw
        self.cur_img = None

    def run(self):
        self.mw.tcr.setText(WAIT_TEXT)
        s = socket(AF_INET, SOCK_DGRAM)
        s.bind(UDP_HOST_PORT)
        SOCKETS.append(s)
        while True:
            data = s.recv(16)
            if not data:
                break
            elif data == 'bye':
                self.mw.tcr.setText(WAIT_TEXT)
            else:
                ts = float(data)
                its = int(ts)
                cur_frame = int(ts * FPS)
                prev_filename = None
                for log_frame, filename in self.mw.log:
                    if log_frame > cur_frame:
                        break
                    prev_filename = filename
                if prev_filename != self.cur_img:
                    self.cur_img = prev_filename
                    if prev_filename is not None:
                        self.mw.load_image.emit(prev_filename)
                self.mw.tcr.setText(TCR_FMT.format(
                    h=its // 3600, m=its // 60, s=its % 60,
                    ms=int((ts * 1000) % 1000), frame=cur_frame))


class MainWindow(QtGui.QMainWindow):
    load_image = QtCore.pyqtSignal(str)
    log = None

    def __init__(self, logfile, parent=None):
        QtGui.QWidget.__init__(self, parent)
        self.setWindowTitle('Praetor')
        self.tcr = QtGui.QLabel()
        self.slide = QtGui.QLabel()
        btn = QtGui.QPushButton()
        btn.setText('Reload log')
        btn.clicked.connect(self._load_log)
        hbox = QtGui.QHBoxLayout()
        hbox.addWidget(btn)
        hbox.addWidget(self.tcr)
        hbox.addStretch()
        vbox = QtGui.QVBoxLayout()
        vbox.addLayout(hbox)
        vbox.addWidget(self.slide)
        vbox.addStretch()
        w = QtGui.QWidget(self)
        w.setLayout(vbox)
        self.setCentralWidget(w)
        self.logfile = logfile
        self._load_log()
        self.load_image.connect(self._load_image)
        self.resize(680, 540)
        MplayerThread(self).start()

    def _load_image(self, filename):
        img = QtGui.QImage()
        img.load(filename)
        self.slide.setPixmap(QtGui.QPixmap.fromImage(
            img.scaledToWidth(640, QtCore.Qt.SmoothTransformation)))

    def _load_log(self):
        log = []
        with file(self.logfile) as f:
            for line in f:
                frame, filename = line.rstrip().split(',', 1)
                log.append((int(frame), filename))
        self.log = log

if __name__ == '__main__':
    main()
