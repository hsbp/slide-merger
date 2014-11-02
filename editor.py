#!/usr/bin/env python

from __future__ import division, print_function
from subprocess import check_output, Popen, PIPE
from flask import Flask, render_template, request
from base64 import urlsafe_b64encode
from hashlib import sha256
from numpy import array
from os import devnull, path
from sys import stderr
from PIL import Image
import struct, json

DEV_NULL = file(devnull, 'w')
FILE_NAME = 'output.bin'
X, Y = range(2)
TOP_LEFT, BOTTOM_RIGHT = range(2)
TRESHOLD = 5
APP = Flask(__name__)
SIZE_FMT = '>HH'
FRAME_FMT = '>I'
MIN_FRAME_DIFF = 1

@APP.route('/export_frames', methods=['POST'])
def export_frames():
    exval = '\n'.join('{0},{1}'.format(k[5:], v[7:]) for k, v in
            sorted(request.form.iteritems(), key=lambda x: int(x[0][5:])) if v)
    return exval, 200, [('Content-Type', 'text/plain')]

@APP.route('/')
def home():
    return render_template('home.html', slide_changes=gen_slide_changes())

def gen_slide_changes():
    with file(FILE_NAME, 'rb') as f:
        fhash = sha256(f.read()).digest()
        f.seek(0)
        size = read_struct(f, SIZE_FMT)
        byte_length = size[X] * size[Y]
        prev_no = -2 * MIN_FRAME_DIFF
        to_yield = None
        while True:
            try:
                (frame_no,) = read_struct(f, FRAME_FMT)
            except struct.error:
                return
            else:
                if frame_no - prev_no > MIN_FRAME_DIFF:
                    if to_yield is not None:
                        yield to_yield
                        to_yield = None
                frame = f.read(byte_length)
                fname = 'static/stills/{0}.jpg'.format(urlsafe_b64encode(
                    fhash + struct.pack('<I', frame_no)).rstrip('='))
                if not path.exists(fname):
                    Image.frombytes('L', size, frame).save(fname)
                to_yield = (frame_no, fname)
                prev_no = frame_no

def read_struct(file_obj, fmt):
    return struct.unpack(fmt, file_obj.read(struct.calcsize(fmt)))

def preprocess(fn, rect):
    info = json.loads(check_output(['ffprobe', fn, '-of', 'json', '-show_streams'], stderr=DEV_NULL))
    video = next(stream for stream in info['streams'] if stream['codec_type'] == 'video')
    width = video['width']
    height = video['height']
    frames = video['duration_ts']

    skip_head = rect[TOP_LEFT][Y] * width + rect[TOP_LEFT][X]
    line_len = rect[BOTTOM_RIGHT][X] - rect[TOP_LEFT][X]
    line_count = rect[BOTTOM_RIGHT][Y] - rect[TOP_LEFT][Y]
    skip_line = width - line_len
    skip_trail = (height - rect[BOTTOM_RIGHT][Y]) * width - rect[TOP_LEFT][X]
    assert skip_head + (line_len + skip_line) * line_count + skip_trail == width * height

    diff_treshold = TRESHOLD * line_len * line_count

    ffmpeg = Popen(['ffmpeg', '-i', fn, '-vcodec', 'rawvideo',
        '-pix_fmt', 'gray', '-f', 'rawvideo', '-'], stderr=DEV_NULL, stdout=PIPE)
    raw_video = ffmpeg.stdout

    prev = None

    with file(FILE_NAME, 'wb') as f:
        f.write(struct.pack(SIZE_FMT, line_len, line_count))
        for frame_no in xrange(frames):
            raw_video.read(skip_head)
            sums = 0
            rows = []
            for _ in xrange(line_count):
                rows.append(raw_video.read(line_len))
                raw_video.read(skip_line)
            raw_video.read(skip_trail)

            frame = ''.join(rows)
            values = array(map(ord, frame))

            if prev is not None:
                diff = values / prev
                if frame_no % 64 == 0:
                    print(frame_no, file=stderr)
                if sum(abs(i) for i in (values - prev)) > diff_treshold:
                    f.write(struct.pack(FRAME_FMT, frame_no))
                    f.write(frame)
                    print('->', frame_no, file=stderr)
            prev = values

    ffmpeg.terminate()

if __name__ == '__main__':
    from sys import argv
    if len(argv) > 2:
        preprocess(argv[1], [map(int, point.split(',')) for point in argv[2].split('-')])
    elif len(argv) > 1 and argv[1] == 'web':
        APP.run(debug=True, port=5142)
    else:
        print('Usage: {0} filename.mkv 8,8-160,160\n       {0} web'.format(argv[0]), file=stderr)
