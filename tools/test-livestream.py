"""Play the Live panel's stream from a local CasparCG server and report what a
client gets: first frame, frame rate, size and the player's own CPU.

    python tools/test-livestream.py [path to casparcg.exe] [clip]

Starts the server (and stops it after), loops the clip on channel 1, asks for
the stream with the client's own default parameters for the panel and for the
window, and plays each with tools/probe-livestream (Qt Multimedia). A third
round starts the player five seconds after the stream, as a client that joins
late would. Build the probe first with tools/probe-livestream.bat.

Not part of check-all: it needs a server, a clip and a free port 5250.
"""

import os
import socket
import subprocess
import sys
import time

SERVER = sys.argv[1] if len(sys.argv) > 1 else r"C:\CasparCG\CasparCG Server 2.5.x-50aaf87\casparcg.exe"
CLIP = sys.argv[2] if len(sys.argv) > 2 else "011 INST TIKTOK"
PROBE = os.path.join(os.environ["TEMP"], "casparcg-probe-livestream", "probe-livestream.exe")
QT = r"C:\Qt\6.5.3\msvc2019_64"
PORT = 9250

X264 = "-format mpegts -codec:v libx264 -crf:v 25 -tune:v zerolatency -preset:v ultrafast -x264-params:v repeat-headers=1 -g:v 25"
AUDIO = '-filter:a "pan=stereo|c0=FL|c1=FR"'
ROUNDS = [
    ("panel 288x162", X264 + " -filter:v fps=25,scale=288:162:flags=fast_bilinear,format=yuv420p " + AUDIO, 0),
    ("window 1280 wide", X264 + " -filter:v fps=25,scale=1280:-2:flags=fast_bilinear,format=yuv420p " + AUDIO, 0),
    ("panel, player started 5 s late", X264 + " -filter:v fps=25,scale=288:162:flags=fast_bilinear,format=yuv420p " + AUDIO, 5),
]


def amcp(sock, command):
    sock.sendall((command + "\r\n").encode("utf-8"))
    data = b""
    while b"\r\n" not in data:
        data += sock.recv(65536)
    return data.split(b"\r\n", 1)[0].decode("utf-8", "replace")


def main():
    env = dict(os.environ)
    env["PATH"] = QT + r"\bin;" + env["PATH"]
    env["QT_PLUGIN_PATH"] = QT + r"\plugins"

    server = subprocess.Popen([SERVER], cwd=os.path.dirname(SERVER), stdout=subprocess.DEVNULL,
                              stderr=subprocess.DEVNULL, creationflags=subprocess.CREATE_NEW_CONSOLE)
    try:
        for _ in range(120):
            try:
                socket.create_connection(("127.0.0.1", 5250), timeout=1).close()
                break
            except OSError:
                time.sleep(1)
        time.sleep(5)
        s = socket.create_connection(("127.0.0.1", 5250), timeout=10)
        print("play:", amcp(s, 'PLAY 1-10 "%s" LOOP' % CLIP))
        time.sleep(3)
        url = "udp://<client_ip_address>:%d?pkt_size=1316" % PORT

        for name, params, late in ROUNDS:
            print("\n== " + name)
            if late:
                print("stream:", amcp(s, "ADD 1 STREAM %s %s" % (url, params)))
                time.sleep(late)
                probe = subprocess.Popen([PROBE, str(PORT), "15"], env=env, stdout=subprocess.PIPE, text=True)
            else:
                probe = subprocess.Popen([PROBE, str(PORT), "15"], env=env, stdout=subprocess.PIPE, text=True)
                time.sleep(1)
                print("stream:", amcp(s, "ADD 1 STREAM %s %s" % (url, params)))
            output, _ = probe.communicate(timeout=60)
            print(output.strip())
            print("remove:", amcp(s, "REMOVE 1 STREAM %s" % url))
            time.sleep(2)

        amcp(s, "CLEAR 1")
        try:
            amcp(s, "KILL")
        except OSError:
            pass
    finally:
        try:
            server.wait(timeout=20)
        except subprocess.TimeoutExpired:
            server.kill()
        print("\nserver stopped")


main()
