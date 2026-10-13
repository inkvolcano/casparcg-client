// Reference images on rundown items: what is accepted, how it is stored, and that
// it comes back as a picture.

#include "../src/Common/ReferenceImage.h"

#include <QtCore/QCoreApplication>
#include <QtCore/QTemporaryDir>
#include <QtCore/QTextStream>
#include <QtGui/QPainter>

static int checks = 0;
static int failures = 0;

static void expectTrue(bool actual, const QString& what)
{
    checks++;
    if (actual)
        return;

    failures++;
    QTextStream(stdout) << "  FAIL  " << what << "\n";
}

int main(int argc, char* argv[])
{
    QCoreApplication app(argc, argv);
    // The JPEG writer is a plugin; the test runs outside the client's own folder.
    QCoreApplication::addLibraryPath(qEnvironmentVariable("QT_PLUGIN_PATH", "C:/Qt/6.5.3/msvc2019_64/plugins"));

    QTextStream out(stdout);
    out << "Reference images\n";

    using namespace ReferenceImage;

    expectTrue(isImageFile("C:/grabs/lower third.PNG") && isImageFile("a.jpg") && isImageFile("a.jpeg"), "pictures are accepted");
    expectTrue(!isImageFile("rundown.xml") && !isImageFile("clip.mov") && !isImageFile("noext"), "other files are not");

    // A full HD frame, opaque: scaled to fit 480 and kept as JPEG.
    QImage frame(1920, 1080, QImage::Format_RGB32);
    frame.fill(QColor(20, 60, 120));
    QPainter(&frame).fillRect(100, 800, 1200, 160, Qt::white);
    const QString stored = encode(frame);
    expectTrue(stored.startsWith("data:image/jpeg;base64,"), "an opaque picture is stored as JPEG");
    const QImage back = decode(stored);
    expectTrue(back.width() == 480 && back.height() == 270, QString("and scaled to fit 480: %1x%2").arg(back.width()).arg(back.height()));
    expectTrue(stored.size() < 60 * 1024, QString("and small: %1 KB").arg(stored.size() / 1024));

    // A graphic with transparency: PNG, so the transparent part stays transparent.
    QImage graphic(800, 200, QImage::Format_ARGB32);
    graphic.fill(Qt::transparent);
    QPainter(&graphic).fillRect(0, 100, 600, 100, QColor(200, 30, 30));
    const QString png = encode(graphic);
    expectTrue(png.startsWith("data:image/png;base64,"), "a picture with transparency is stored as PNG");
    const QImage pngBack = decode(png);
    expectTrue(pngBack.width() == 480 && pngBack.pixelColor(10, 10).alpha() == 0, "and keeps its transparency");

    // A small picture is not enlarged.
    QImage small(200, 100, QImage::Format_RGB32);
    small.fill(Qt::gray);
    expectTrue(decode(encode(small)).size() == QSize(200, 100), "a small picture keeps its size");

    // From a file, the way the Inspector and a drop read it.
    QTemporaryDir dir;
    const QString path = dir.filePath("grab.png");
    frame.save(path);
    expectTrue(fromFile(path).startsWith("data:image/jpeg;base64,"), "a file is read and stored");
    expectTrue(fromFile(dir.filePath("missing.png")).isEmpty(), "a missing file stores nothing");

    expectTrue(encode(QImage()).isEmpty() && decode("not a picture").isNull(), "nothing in, nothing out");
    expectTrue(tooltipHtml(stored).startsWith("<img src=\"data:image/jpeg;base64,"), "the hover shows the picture");
    expectTrue(tooltipHtml("javascript:alert(1)").isEmpty(), "and only a picture");

    // One size for every hover.
    expectTrue(tooltipHtml(stored).endsWith("width=\"384\" height=\"216\"/>"), "a full-frame grab fills the hover frame exactly");
    expectTrue(tooltipHtml(encode(small)).contains("width=\"384\" height=\"192\""), "a small picture is enlarged to the frame's width");
    QImage tall(300, 600, QImage::Format_RGB32);
    tall.fill(Qt::darkGray);
    expectTrue(tooltipHtml(encode(tall)).contains("width=\"108\" height=\"216\""), "a tall picture fills the frame's height, undistorted");
    expectTrue(hoverSize(QSize()) == QSize(HOVER_WIDTH, HOVER_HEIGHT), "an unreadable size still gets the frame");

    out << "\n" << (checks - failures) << " passed, " << failures << " failed\n";
    return failures == 0 ? 0 : 1;
}
