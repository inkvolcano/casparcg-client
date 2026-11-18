#pragma once

// Where a playing clip is inside the part of the file it plays (upstream #272).
//
// What CasparCG Server sends for a clip on a layer, measured on 2.5 in build 321
// with PLAY 1-10 "clip" SEEK 100 LENGTH 250 on a 25 fps channel:
//   file/time  [8.2, 32.12]  seconds since the start of the FILE, and the file's length
//   file/clip  [4.0, 10.0]   the in point, and the LENGTH that plays - not an out point
// Without SEEK or LENGTH, file/clip is [0, the file's length].
//
// Until 321 the row counted down from the file's length and treated the length
// as an out point, so a clip with Seek and Length showed the wrong time left and
// the end-of-clip autoplay fired early.

namespace ClipWindow
{
    struct Window
    {
        double in = 0;        // seconds into the file where playback started
        double out = 0;       // seconds into the file where it stops
        double length = 0;    // out - in
        double elapsed = 0;   // played so far
        double remaining = 0; // left to play, never below zero
    };

    // time and fileLength from file/time; clipIn and clipLength from file/clip.
    // A missing or zero clip length falls back to the rest of the file.
    inline Window from(double time, double fileLength, double clipIn, double clipLength)
    {
        Window w;
        w.in = clipIn > 0 ? clipIn : 0;
        w.length = clipLength > 0 ? clipLength : (fileLength > w.in ? fileLength - w.in : 0);
        w.out = w.in + w.length;
        w.elapsed = time > w.in ? time - w.in : 0;
        w.remaining = w.out > time ? w.out - time : 0;
        return w;
    }
}
