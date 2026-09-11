// Which Qt Multimedia opener faults on a given file, and with what.
//
// Build 218 died inside ffmpegmediaplugin.dll two seconds after the Library
// selection of a clip - an access violation, so nothing on our side could log
// it. The Preview panel opens a selected movie twice: QMediaPlayer::setSource
// for the picture and QAudioDecoder for the meters. This runs ONE of those, on
// ONE file, in a process of its own, so the faulting one is named by its exit
// code rather than by taking the client down.
//
//   test-mediaprobe player  <file>
//   test-mediaprobe decoder <file>
//
// Exit 0: opened and reached a terminal state. Exit 2: the API reported an
// error (which is fine - an error is not a crash). Exit 3: timed out. Anything
// else, and in particular 0xC0000005, is the plugin faulting. The runner
// compares the two.

#include <QtCore/QCoreApplication>
#include <QtCore/QFileInfo>
#include <QtCore/QTextStream>
#include <QtCore/QTimer>
#include <QtCore/QUrl>
#include <QtMultimedia/QAudioDecoder>
#include <QtMultimedia/QAudioOutput>
#include <QtMultimedia/QMediaPlayer>

#include <cstdio>

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    QTextStream out(stdout);

    if (argc < 3)
    {
        out << "usage: test-mediaprobe player|decoder <file>\n";
        return 64;
    }

    const QString mode = argv[1];
    const QString file = QFileInfo(argv[2]).absoluteFilePath();

    if (!QFileInfo::exists(file))
    {
        out << "no such file: " << file << "\n";
        return 64;
    }

    int result = 3;   // until something says otherwise, it hung

    // Nothing here should take long; a file that neither errors nor finishes
    // in this time is a different finding, and worth a code of its own.
    QTimer::singleShot(15000, &app, [&]() { out << "  timed out\n"; result = 3; app.quit(); });

    if (mode == "player")
    {
        // The same calls PreviewWidget::loadVideo makes when autoplay is off.
        QMediaPlayer* player = new QMediaPlayer(&app);
        player->setAudioOutput(new QAudioOutput(&app));

        QObject::connect(player, &QMediaPlayer::errorOccurred, &app,
                         [&](QMediaPlayer::Error, const QString& text) {
            out << "  player error: " << text << "\n";
            result = 2;
            app.quit();
        });
        QObject::connect(player, &QMediaPlayer::mediaStatusChanged, &app,
                         [&](QMediaPlayer::MediaStatus status) {
            out << "  player status " << int(status) << "\n";
            if (status == QMediaPlayer::LoadedMedia || status == QMediaPlayer::BufferedMedia
                || status == QMediaPlayer::EndOfMedia || status == QMediaPlayer::InvalidMedia)
            {
                result = status == QMediaPlayer::InvalidMedia ? 2 : 0;
                app.quit();
            }
        });

        // From inside the loop: the FFmpeg backend reports the load synchronously
        // from setSource, and a quit() before exec() is ignored.
        QTimer::singleShot(0, &app, [=]() {
            player->setSource(QUrl::fromLocalFile(file));
            player->pause();
        });
    }
    else if (mode == "decoder")
    {
        // The same calls PreviewAudioAnalyser::analyse makes.
        QAudioDecoder* decoder = new QAudioDecoder(&app);

        QObject::connect(decoder, qOverload<QAudioDecoder::Error>(&QAudioDecoder::error), &app,
                         [&](QAudioDecoder::Error) {
            out << "  decoder error: " << decoder->errorString() << "\n";
            result = 2;
            app.quit();
        });
        QObject::connect(decoder, &QAudioDecoder::finished, &app, [&]() {
            out << "  decoder finished\n";
            result = 0;
            app.quit();
        });
        QObject::connect(decoder, &QAudioDecoder::bufferReady, &app, [&]() {
            // Drain, as the analyser does; never reading would stall it.
            decoder->read();
        });

        decoder->setSource(QUrl::fromLocalFile(file));
        decoder->start();
    }
    else
    {
        out << "unknown mode " << mode << "\n";
        return 64;
    }

    app.exec();
    out.flush();
    return result;
}
