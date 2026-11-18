// Plays the Live panel's UDP stream with Qt Multimedia (FFmpeg) and says what
// arrived: how long until the first frame, how many frames a second, the frame
// size, and this process's own CPU. A second opinion beside the Live panel's
// own player (libvlc): a stream this plays and the panel does not is a player
// problem, and the other way round a stream problem.
//
// Build 320 compared the two for the panel: libvlc was kept - 0.55 s behind the
// server at 300 ms cache against 1.4-1.7 s for Qt Multimedia, which has no
// buffer control in Qt 6.5, and about 1 s to pick up a stream already running
// against 7-8 s.
//
//   probe-livestream.exe [port] [seconds] [latency]
//
// The server is asked for the stream by whoever runs this (tools/test-livestream.py
// does it against a local server); this only listens.

#include <QtCore/QDateTime>
#include <QtCore/QElapsedTimer>
#include <QtCore/QTextStream>
#include <QtCore/QTimer>
#include <QtCore/QUrl>
#include <QtGui/QGuiApplication>
#include <QtGui/QImage>
#include <QtMultimedia/QMediaPlayer>
#include <QtMultimedia/QVideoFrame>
#include <QtMultimedia/QVideoSink>

#include <windows.h>

static double cpuSeconds()
{
    FILETIME created, exited, kernel, user;
    GetProcessTimes(GetCurrentProcess(), &created, &exited, &kernel, &user);
    auto seconds = [](const FILETIME& t) {
        ULARGE_INTEGER v;
        v.LowPart = t.dwLowDateTime;
        v.HighPart = t.dwHighDateTime;
        return double(v.QuadPart) / 1e7;
    };
    return seconds(kernel) + seconds(user);
}

int main(int argc, char* argv[])
{
    QGuiApplication app(argc, argv);
    const int port = argc > 1 ? QString(argv[1]).toInt() : 9250;
    const int seconds = argc > 2 ? QString(argv[2]).toInt() : 20;
    // "latency": print the wall-clock time whenever the picture turns red, for
    // measuring how far behind the server's output it is.
    const bool latency = argc > 3 && QString(argv[3]) == "latency";
    bool wasRed = false;

    QTextStream out(stdout);
    QMediaPlayer player;
    QVideoSink sink;
    player.setVideoSink(&sink);

    QElapsedTimer clock;
    clock.start();
    qint64 firstFrameMs = -1;
    int frames = 0;
    QSize size;
    double cpuAtFirst = 0;

    QObject::connect(&sink, &QVideoSink::videoFrameChanged, [&](const QVideoFrame& frame) {
        if (!frame.isValid())
            return;
        if (firstFrameMs < 0)
        {
            firstFrameMs = clock.elapsed();
            cpuAtFirst = cpuSeconds();
        }
        frames++;
        size = frame.size();
        if (latency)
        {
            const QImage image = frame.toImage();
            const QColor c = image.pixelColor(image.width() / 2, image.height() / 2);
            const bool red = c.red() > 180 && c.green() < 80 && c.blue() < 80;
            if (red && !wasRed)
            {
                out << "red at " << QDateTime::currentMSecsSinceEpoch() << "\n";
                out.flush();
            }
            wasRed = red;
        }
    });
    QObject::connect(&player, &QMediaPlayer::errorOccurred, [&](QMediaPlayer::Error, const QString& text) {
        out << "error: " << text << "\n";
        out.flush();
    });

    player.setSource(QUrl(QString("udp://0.0.0.0:%1").arg(port)));
    player.play();

    QTimer::singleShot(seconds * 1000, [&]() {
        const double elapsed = (clock.elapsed() - firstFrameMs) / 1000.0;
        out << "first frame after " << firstFrameMs << " ms\n";
        out << "frames " << frames << ", " << (firstFrameMs >= 0 && elapsed > 0 ? frames / elapsed : 0.0) << " a second\n";
        out << "size " << size.width() << "x" << size.height() << "\n";
        out << "own cpu " << (firstFrameMs >= 0 && elapsed > 0 ? 100.0 * (cpuSeconds() - cpuAtFirst) / elapsed : 0.0)
            << "% of one core while playing\n";
        out.flush();
        app.quit();
    });
    return app.exec();
}
