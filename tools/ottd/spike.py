#!/usr/bin/env python3
"""Spike: drive a running OpenTTD dedicated server via the admin port + GameScript.

Usage: spike.py [--host 127.0.0.1] [--port 3977] [--password P]
Standard library only. Insecure (plain password) join.
"""
import argparse, glob, json, os, socket, struct, sys, time

# PacketAdminType values
ADMIN_JOIN, ADMIN_QUIT, ADMIN_UPDATE_FREQ, ADMIN_POLL, ADMIN_CHAT, ADMIN_RCON, ADMIN_GS = 0, 1, 2, 3, 4, 5, 6
S_FULL, S_BANNED, S_ERROR, S_PROTOCOL, S_WELCOME, S_NEWGAME, S_SHUTDOWN = 100, 101, 102, 103, 104, 105, 106
S_RCON, S_GS, S_RCON_END = 120, 124, 125
# AdminUpdateType.Gamescript = 9, AdminUpdateFrequency.Automatic = 6 (bit 1<<6)
UPD_GAMESCRIPT, FREQ_AUTOMATIC = 9, 1 << 6


def cstr(s):
    return s.encode() + b"\0"


def read_cstr(b, off=0):
    end = b.index(b"\0", off)
    return b[off:end].decode(errors="replace"), end + 1


class Admin:
    def __init__(self, host, port, password):
        self.sock = socket.create_connection((host, port), timeout=10)
        self.send(ADMIN_JOIN, cstr(password) + cstr("spike.py") + cstr("0.1"))
        while True:
            t, p = self.recv()
            if t == S_WELCOME:
                self.welcome = p
                break
            if t in (S_FULL, S_BANNED, S_ERROR):
                raise RuntimeError("join refused, packet %d payload %r" % (t, p))

    def send(self, ptype, payload=b""):
        self.sock.sendall(struct.pack("<HB", len(payload) + 3, ptype) + payload)

    def recv(self):
        hdr = self._exact(3)
        size, ptype = struct.unpack("<HB", hdr)
        return ptype, self._exact(size - 3)

    def _exact(self, n):
        buf = b""
        while len(buf) < n:
            c = self.sock.recv(n - len(buf))
            if not c:
                raise ConnectionError("server closed connection")
            buf += c
        return buf

    def rcon(self, cmd):
        """Run a console command, return its output lines."""
        self.send(ADMIN_RCON, cstr(cmd))
        lines = []
        while True:
            t, p = self.recv()
            if t == S_RCON:
                lines.append(read_cstr(p, 2)[0])
            elif t == S_RCON_END:
                return lines
            elif t == S_ERROR:
                raise RuntimeError("server error %r" % p)

    def gs_request(self, obj, timeout=10):
        """Send a JSON message to the GameScript, return the reply with a matching id."""
        self.send(ADMIN_UPDATE_FREQ, struct.pack("<HH", UPD_GAMESCRIPT, FREQ_AUTOMATIC))
        self.send(ADMIN_GS, cstr(json.dumps(obj, separators=(",", ":"))))
        deadline = time.time() + timeout
        self.sock.settimeout(timeout)
        while time.time() < deadline:
            t, p = self.recv()
            if t == S_GS:
                reply = json.loads(read_cstr(p)[0])
                if reply.get("id") == obj.get("id"):
                    return reply
        raise TimeoutError("no GameScript reply")

    def close(self):
        try:
            self.send(ADMIN_QUIT)
        except OSError:
            pass
        self.sock.close()


def shot(adm, cmd, shotdir):
    """Run a screenshot console command and report which PNG (if any) appeared."""
    def snap():
        return {f: os.stat(f).st_mtime_ns for f in glob.glob(os.path.join(shotdir, "*.png"))}
    before = snap()
    out = adm.rcon(cmd)  # the server prints "Screenshot successfully saved"/"failed" on its own console, not in the reply
    for _ in range(20):
        new = [f for f, m in snap().items() if before.get(f) != m]
        if new:
            return "%s -> %s (rcon reply: %r)" % (cmd, ", ".join("%s %d bytes" % (f, os.path.getsize(f)) for f in new), out)
        time.sleep(0.1)
    return "%s -> no PNG appeared in %s (rcon reply: %r)" % (cmd, shotdir, out)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--host", default="127.0.0.1")
    ap.add_argument("--port", type=int, default=3977)
    ap.add_argument("--password", default="spikepw")
    ap.add_argument("--screenshot-dir", default=os.path.join(os.environ.get("OTTD_SPIKE_DIR", "."), "screenshot"),
                    help="the server's screenshot/ folder (default: $OTTD_SPIKE_DIR/screenshot)")
    a = ap.parse_args()

    t0 = time.time()
    adm = Admin(a.host, a.port, a.password)
    print("connect+join: %.1f ms" % ((time.time() - t0) * 1000))
    try:
        print("getdate:", adm.rcon("getdate"))
        print(shot(adm, "screenshot minimap", a.screenshot_dir))
        print(shot(adm, "screenshot", a.screenshot_dir))
        print("GS reply:", adm.gs_request({"id": 1, "cmd": "ping"}))
    finally:
        adm.close()


if __name__ == "__main__":
    try:
        main()
    except Exception as e:
        print("FAILED: %s: %s" % (type(e).__name__, e), file=sys.stderr)
        sys.exit(1)
