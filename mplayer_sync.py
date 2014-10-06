#!/usr/bin/env python

import re

STATUS_RE = re.compile(r'A: *(\d+(?:.\d*)?) ')

def monitor(fp):
    buf = ''
    last = None
    while True:
        b = fp.read(16)
        if not b:
            break
        buf += b
        m = STATUS_RE.search(buf)
        if m:
            ts = m.group(1)
            if ts != last:
                last = ts
                yield float(ts)
            buf = ''

if __name__ == '__main__':
    from sys import stdin
    for ts in monitor(stdin):
        print ts
