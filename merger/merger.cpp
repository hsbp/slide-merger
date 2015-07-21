#include <QTextStream>
#include <QProcess>
#include <QImage>
#include <QFile>

#include <unistd.h>
#include <string.h>
#include <stdio.h>

/*
 *  Input video (stdin):
 *
 *       <--|--> VIDEO_WIDTH
 *          <> RECORDING_OFFSET
 *  +---50%--||--50%---+  ^
 *  |abcdefghijklmnopqr|  |
 *  |stuvwxyz0123456789|  |
 *  |@ABCDEFGHIJKLMNOPQ| RECORDING_HEIGHT = VIDEO_HEIGHT
 *  |RSTUVWXYZ.,-/+*_%!|  |
 *  |#:{}&=~[]?;$\§'"()|  |
 *  +------------------+  V
 *
 *  <-RECORDING_WIDTH-->
 *
 *
 *  Output video (ffmpeg):    <-VIDEO_WIDTH->
 *
 *  +-------------------------+-------------+
 *  |                   ^     |efghijkl  ^  |
 *  |<----SLIDE_WIDTH---+---->|wxyz0     |  |
 *  |                   |     |DEFGHI VID_H |
 *  |                   |     |VWXYZ.,   |  |
 *  | OUT_HEIGHT=SLIDE_HEIGHT |&=~[]?;$  V  |
 *  |                   |     +-------------+
 *  |                   V     |   (blank)   |
 *  +-------------------------+-------------+
 *
 *  <-----------OUT_WIDTH------------------->
 *
 */
#define OUT_WIDTH 854
#define OUT_HEIGHT 480
#define SLIDE_WIDTH 640
#define SLIDE_HEIGHT OUT_HEIGHT
#define VIDEO_WIDTH (OUT_WIDTH - SLIDE_WIDTH)
#define VIDEO_HEIGHT 480
#define RECORDING_WIDTH 720
#define RECORDING_HEIGHT VIDEO_HEIGHT
#define RECORDING_OFFSET -32
#define BYTES_PER_PIXEL 3

#define SLIDE_FRAME_OFFSET 0
// uncomment the line below to put recording on the left
// #define RECORDING_ON_LEFT

#if VIDEO_HEIGHT < OUT_HEIGHT
#define HAS_SOUTHEAST
#endif

// must match input video FPS
#define FPS 29.97

// uncomment and set if partial processing is needed
// #define FRAMES_OFFSET (76 * 60 * FPS)

// uncomment and set if overlay is needed
// #define OVERLAY "../../overlay960x540.png"

inline void discardStdInBytes(int bytes) {
	char buf[bytes];
	fread(buf, bytes, 1, stdin);
}

inline void forwardStdInBytes(QProcess &output, int bytes) {
	char buf[bytes];
	fread(buf, bytes, 1, stdin);
	output.write((const char*)buf, bytes);
	output.waitForBytesWritten();
}

QStringList loadLog(const char *logfile) {
	QFile log(logfile);
	QStringList retval;
	if (log.open(QIODevice::ReadOnly | QIODevice::Text)) {
		QTextStream textStream(&log);
		while (true) {
			const QString line(textStream.readLine());
			if (line.isNull()) break; else retval.append(line);
		}
	}
	return retval;
}

inline void writeScanLine(QProcess &output, const QImage &img, const int line) {
	output.write((const char*)img.constScanLine(line), SLIDE_WIDTH * BYTES_PER_PIXEL);
	output.waitForBytesWritten();
}

int main(int argc, char **argv) {
	if (argc < 3) {
		fprintf(stderr, ("Usage: %s timestamps.log path/to/slide-0.png output.mkv\n\n"
				"Standard input expects a series of 24-bit RGB triplets, an ffmpeg example:\n"
				"\tffmpeg -i foo.mkv -vcodec rawvideo -pix_fmt rgb24 -f rawvideo -\n"), argv[0]);
		return 1;
	}

#ifdef HAS_SOUTHEAST
	char southEast[VIDEO_WIDTH * BYTES_PER_PIXEL];
	memset(southEast, 0xFF, sizeof(southEast));
#endif
#define ZOOM_SIDE_BYTES ((OUT_WIDTH - RECORDING_WIDTH) / 2 * BYTES_PER_PIXEL)
	char zoomSide[ZOOM_SIDE_BYTES];
	memset(zoomSide, 0x00, ZOOM_SIDE_BYTES);
	QImage slide;
	QStringList slideChanges(loadLog(argv[1]));
	QProcess ffmpegOut;
	QStringList ffmpegOutArgs;
	QSize slideSize(SLIDE_WIDTH, SLIDE_HEIGHT);

	int frames = (80 * 60 + 54) * FPS; // TODO read from argv and/or input
	QString slidePath(argv[2]);

#ifdef FRAMES_OFFSET
	foreach (const int timestamp, slideChanges) {
		if (timestamp > FRAMES_OFFSET) break;
		slidePos++;
	}
	for (int i = 0; i < slidePos; i++) slideChanges.removeFirst();
#else
#define FRAMES_OFFSET 0
#endif

	int nextChange = FRAMES_OFFSET;

	ffmpegOutArgs << "-r" << QString::number(FPS) << "-loglevel" << "quiet";
	ffmpegOutArgs << "-s" << QString::number(OUT_WIDTH) + "x" + QString::number(OUT_HEIGHT);
	ffmpegOutArgs << "-f" << "rawvideo" << "-pixel_format" << "rgb24" << "-i" << "-";

#ifdef OVERLAY
	ffmpegOutArgs << "-i" << OVERLAY << "-filter_complex" << "overlay";
#endif

	ffmpegOutArgs << "-an" << "-vcodec" << "libx264" << "-y" << "-pix_fmt" << "yuv420p" << argv[3];

	ffmpegOut.start("ffmpeg", ffmpegOutArgs);
	ffmpegOut.waitForStarted();

	bool zoomMode = false;

	for (int frame = FRAMES_OFFSET; frame < frames; frame++) {
		if (frame == nextChange) {
			fprintf(stderr, "Loaded frame %s at %d\n", slidePath.toUtf8().constData(), frame);
			zoomMode = (slidePath == "zoom");
			if (!zoomMode) {
				slide = QImage(slidePath).convertToFormat(QImage::Format_RGB888)
						.scaled(slideSize, Qt::IgnoreAspectRatio, Qt::SmoothTransformation)
						.convertToFormat(QImage::Format_RGB888);
				fprintf(stderr, "Size: %dx%d\n", slide.width(), slide.height());
			}

			if (!slideChanges.isEmpty()) {
				const QStringList parts(slideChanges.takeFirst().split(','));
				if (parts.size() == 2) {
					nextChange = parts[0].toInt() - SLIDE_FRAME_OFFSET;
					slidePath = parts[1];
				}
			}
		}
		for (int line = 0; line < OUT_HEIGHT; line++) {
			if (zoomMode) {
				ffmpegOut.write(zoomSide, ZOOM_SIDE_BYTES);
				ffmpegOut.waitForBytesWritten();
				forwardStdInBytes(ffmpegOut, RECORDING_WIDTH * BYTES_PER_PIXEL);
				ffmpegOut.write(zoomSide, ZOOM_SIDE_BYTES);
				ffmpegOut.waitForBytesWritten();
			} else {
#ifndef RECORDING_ON_LEFT
				writeScanLine(ffmpegOut, slide, line);
#endif
#ifdef HAS_SOUTHEAST
				if (line < VIDEO_HEIGHT) {
#endif
					discardStdInBytes(((RECORDING_WIDTH - VIDEO_WIDTH) / 2
								+ RECORDING_OFFSET) * BYTES_PER_PIXEL);
					forwardStdInBytes(ffmpegOut, VIDEO_WIDTH * BYTES_PER_PIXEL);
					discardStdInBytes(((RECORDING_WIDTH - VIDEO_WIDTH) / 2
								- RECORDING_OFFSET) * BYTES_PER_PIXEL);
#ifdef HAS_SOUTHEAST
				} else {
					ffmpegOut.write(southEast, sizeof(southEast));
					ffmpegOut.waitForBytesWritten();
				}
#endif

#ifdef RECORDING_ON_LEFT
				writeScanLine(ffmpegOut, slide, line);
#endif
			}
		}
	}

	ffmpegOut.closeWriteChannel();
	ffmpegOut.waitForFinished();
	return 0;
}
