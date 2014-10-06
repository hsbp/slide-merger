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
#define OUT_WIDTH 960 
#define OUT_HEIGHT 540 
#define SLIDE_WIDTH 720
#define SLIDE_HEIGHT OUT_HEIGHT
#define VIDEO_WIDTH 240
#define VIDEO_HEIGHT 480
#define RECORDING_WIDTH 720
#define RECORDING_HEIGHT VIDEO_HEIGHT
#define RECORDING_OFFSET -32
#define BYTES_PER_PIXEL 3

// must match input video FPS
#define FPS 29.97

// uncomment and set if partial processing is needed
// #define FRAMES_OFFSET (76 * 60 * FPS)

// uncomment and set if overlay is needed
// #define OVERLAY "../../overlay960x540.png"

void discardStdInBytes(int bytes) {
	char buf[bytes];
	fread(buf, bytes, 1, stdin);
}

void forwardStdInBytes(QProcess &output, int bytes) {
	char buf[bytes];
	fread(buf, bytes, 1, stdin);
	output.write((const char*)buf, bytes);
	output.waitForBytesWritten();
}

QList<int> loadLog(const char *logfile) {
	QFile log(logfile);
	QList<int> retval;
	if (log.open(QIODevice::ReadOnly | QIODevice::Text)) {
		while (!log.atEnd()) {
			QByteArray line = log.readLine();
			line.remove(line.length() - 1, 1); // trailing newline
			retval.append(line.toShort() * FPS);
		}
	}
	return retval;
}

void writeScanLine(QProcess &output, const QImage &img, const int line) {
	output.write((const char*)img.constScanLine(line), img.bytesPerLine());
	output.waitForBytesWritten();
}

int main(int argc, char **argv) {
	if (argc < 3) {
		fprintf(stderr, ("Usage: %s timestamps.log path/to/slide-%%1.png output.mkv"
				" (note: slides are numbered from 0)\n\n"
				"Standard input expects a series of 24-bit RGB triplets, an ffmpeg example:\n"
				"\tffmpeg -i foo.mkv -vcodec rawvideo -pix_fmt rgb24 -f rawvideo -\n"), argv[0]);
		return 1;
	}

	QString slideFormat(argv[2]);
	QImage southEast(VIDEO_WIDTH, OUT_HEIGHT - VIDEO_HEIGHT, QImage::Format_RGB888);
	QImage slide;
	QList<int> slideChanges(loadLog(argv[1]));
	QProcess ffmpegOut;
	QStringList ffmpegOutArgs;
	QSize slideSize(SLIDE_WIDTH, SLIDE_HEIGHT);

	int frames = (80 * 60 + 54) * FPS; // TODO read from argv and/or input
	int slidePos = 0;

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

	ffmpegOutArgs << "-an" << "-vcodec" << "libx264" << "-y" << argv[3];

	ffmpegOut.start("ffmpeg", ffmpegOutArgs);
	ffmpegOut.waitForStarted();

	for (int frame = FRAMES_OFFSET; frame < frames; frame++) {
		if (frame == nextChange) {
			fprintf(stderr, "Loaded frame %d at %d\n", slidePos, frame);
			slide = QImage(slideFormat.arg(slidePos++)).scaled(slideSize, Qt::IgnoreAspectRatio,
					Qt::SmoothTransformation).convertToFormat(QImage::Format_RGB888);
			fprintf(stderr, "Size: %dx%d\n", slide.width(), slide.height());
			nextChange = slideChanges.isEmpty() ? 0 : slideChanges.takeFirst();
		}
		for (int line = 0; line < OUT_HEIGHT; line++) {
			writeScanLine(ffmpegOut, slide, line);
			if (line < VIDEO_HEIGHT) {
				discardStdInBytes(((RECORDING_WIDTH - VIDEO_WIDTH) / 2
							+ RECORDING_OFFSET) * BYTES_PER_PIXEL);
				forwardStdInBytes(ffmpegOut, VIDEO_WIDTH * BYTES_PER_PIXEL);
				discardStdInBytes(((RECORDING_WIDTH - VIDEO_WIDTH) / 2
							- RECORDING_OFFSET) * BYTES_PER_PIXEL);
			} else {
				writeScanLine(ffmpegOut, southEast, line - VIDEO_HEIGHT);
			}
		}
	}

	ffmpegOut.closeWriteChannel();
	ffmpegOut.waitForFinished();
	return 0;
}