Slide + video recording merger
==============================

Dependencies
------------

### merger ###

 - QT build environment (tested on 4.8.6)
 - FFmpeg

### preview ###

 - Python 2.x (tested on 2.7.8)
 - PIL
 - Mplayer
 - Blessings
 - Drawille

### editor ###

 - Python 2.x (tested on 2.7.8)
 - PIL
 - FFmpeg
 - Flask
 - NumPy

Building merger
---------------

Please read the comments in `merger/merger.cpp` carefully and update the
parameters appropriately.

	cd merger
	qmake
	make
