#!/usr/bin/env python

from sys import argv
from collections import defaultdict
from operator import itemgetter

from PIL import Image, ImageDraw

for filename in argv[1:]:
    img = Image.open(filename)
    parts = [{"from": 0}, {"from": img.height - 1}]
    pixels = img.load()
    w = img.width
    for part in parts:
        dd = defaultdict(int)
        y = part["from"]
        for x in xrange(w):
            dd[pixels[x, y]] += 1
        part["color"], _ = max(dd.iteritems(), key=itemgetter(1))
    resized = Image.new(img.mode, (int(w / 4) * 4, w * 3 / 4))
    draw = ImageDraw.Draw(resized)
    unit = resized.height / len(parts)
    for n, part in enumerate(parts):
        draw.rectangle((0, n * unit, img.width, (len(parts) + 1 - n) * unit),
                fill=part["color"])
    resized.paste(img, (0, (resized.height - img.height) / 2))
    resized.save(filename + "-r.png")
