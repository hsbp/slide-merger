#!/usr/bin/env python
# -*- encoding: utf-8 -*-

from __future__ import print_function
from blessings import Terminal
from drawille import Canvas
from PIL import Image
from sys import argv, stdin, stderr
from os import path
import mplayer_sync

T = Terminal()
C = Canvas()
W = 240
H = 180

def draw_slide(fn, player):
    img = Image.open(fn)
    px = img.resize((W, H)).convert('L').load()
    for x in xrange(W):
        for y in xrange(H):
            setter = C.set if px[x, y] > 127 else C.unset
            setter(x, y)
    with T.location(0, 0):
        frame = C.frame(min_x=0, min_y=0, max_x=W, max_y=H)
        print(T.white(frame.decode('utf-8')))
        print('')
        print('Slide {0} (player={1})'.format(fn, player))

def main(log):
    log = [row.rstrip().split(',') for row in file(log)]
    log.append(None) # guard

    last_slide = None
    print(log)

    for player in mplayer_sync.monitor(stdin):
        slide = next(n for n, t in enumerate(log) if t is None or (player * 29.97) < float(t[0]))
        slide = log[slide - 1]
        slide = slide[1] if slide is not None else None
        if last_slide != slide:
            draw_slide(slide, player)
            last_slide = slide

if __name__ == '__main__':
    try:
        log = argv[1]
    except IndexError:
        print(('Usage: {0} timestamps.log'
                '\n\nStandard input expects mplayer standard output.').format(argv[0]), file=stderr)
    else:
        main(log)
