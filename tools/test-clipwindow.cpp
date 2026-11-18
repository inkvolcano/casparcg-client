// Where a playing clip is inside the part of the file it plays (upstream #272).
// The numbers are the OSC a CasparCG 2.5 server sent in build 321.

#include "../src/Common/ClipWindow.h"

#include <QtCore/QTextStream>
#include <QtCore/QtGlobal>

static int checks = 0;
static int failures = 0;

static void expectNear(double actual, double expected, const char* what)
{
    checks++;
    if (qAbs(actual - expected) < 0.0001)
        return;

    failures++;
    QTextStream(stdout) << "  FAIL  " << what << ": got " << actual << ", expected " << expected << "\n";
}

int main(int argc, char* argv[])
{
    Q_UNUSED(argc);
    Q_UNUSED(argv);

    QTextStream out(stdout);
    out << "Clip window\n";

    // PLAY ... SEEK 100 LENGTH 250 at 25 fps: file/time [8.2, 32.12], file/clip [4.0, 10.0].
    ClipWindow::Window seek = ClipWindow::from(8.2, 32.12, 4.0, 10.0);
    expectNear(seek.in, 4.0, "seek: in point");
    expectNear(seek.out, 14.0, "seek: out point is in + length, not the length");
    expectNear(seek.length, 10.0, "seek: plays LENGTH 250 = 10 s");
    expectNear(seek.elapsed, 4.2, "seek: played since the in point");
    expectNear(seek.remaining, 5.8, "seek: time left counts to the out point, not the end of the file");

    // Plain PLAY: file/time [3.68, 32.12], file/clip [0, 32.12].
    ClipWindow::Window plain = ClipWindow::from(3.68, 32.12, 0.0, 32.12);
    expectNear(plain.length, 32.12, "plain: the whole file");
    expectNear(plain.elapsed, 3.68, "plain: elapsed is the file time");
    expectNear(plain.remaining, 28.44, "plain: time left");

    ClipWindow::Window noClip = ClipWindow::from(5.0, 30.0, 10.0, 0.0);
    expectNear(noClip.length, 20.0, "no clip length yet: the rest of the file");
    expectNear(noClip.elapsed, 0.0, "a time before the in point is not negative");
    expectNear(ClipWindow::from(15.0, 32.0, 4.0, 10.0).remaining, 0.0, "past the out point: nothing left, not negative");

    out << "\n" << (checks - failures) << " passed, " << failures << " failed\n";
    return failures == 0 ? 0 : 1;
}
