#!/usr/bin/env python
# -*- encoding: utf-8 -*-

from __future__ import print_function
from blessings import Terminal
from drawille import Canvas
from PIL import Image
from sys import argv, stdin, stderr
from os import path
import mplayer_sync

log = '/home/dnet/_projekt/cryptonite/syncert/merger/humangep.log'
slides = '/home/dnet/_projekt/cryptonite/syncert/mvm'
T = Terminal()
C = Canvas()
W = 240
H = 180

def draw_slide(num, player):
    img = Image.open(path.join(slides, 'slide-{0}.png'.format(num)))
    px = img.resize((W, H)).convert('L').load()
    for x in xrange(W):
        for y in xrange(H):
            setter = C.set if px[x, y] > 127 else C.unset
            setter(x, y)
    with T.location(0, 0):
        frame = C.frame(min_x=0, min_y=0, max_x=W, max_y=H)
        print(T.white(frame.decode('utf-8')))
        print('')
        print('Slide {0} (player={1})'.format(num, player))

def main(log, slides):
    log = [int(row.rstrip()) for row in file(log)]
    log.append(None) # guard

    last_slide = None

    for player in mplayer_sync.monitor(stdin):
        slide = next(n for n, t in enumerate(log) if player < t or t is None)
        if last_slide != slide:
            draw_slide(slide, player)
            last_slide = slide

if __name__ == '__main__':
    try:
        log, slides = argv[1:3]
    except ValueError:
        print(('Usage: {0} timestamps.log path/to/slides (slide-0.png will'
                'be the first slide)\n\nStandard input expects mplayer '
                'standard output.').format(argv[0]), file=stderr)
    else:
        main(log, slides)
