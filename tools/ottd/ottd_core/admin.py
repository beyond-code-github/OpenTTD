"""Admin-port client (stdlib only). Insecure (plain password) join."""
import json, socket, struct, time

ADMIN_JOIN, ADMIN_QUIT, ADMIN_UPDATE_FREQ, ADMIN_POLL, ADMIN_CHAT, ADMIN_RCON, ADMIN_GS = 0, 1, 2, 3, 4, 5, 6
S_FULL, S_BANNED, S_ERROR, S_PROTOCOL, S_WELCOME, S_NEWGAME, S_SHUTDOWN = 100, 101, 102, 103, 104, 105, 106
S_RCON, S_GS, S_RCON_END = 120, 124, 125
UPD_GAMESCRIPT, FREQ_AUTOMATIC = 9, 1 << 6

DEFAULT_PORT = 3977
DEFAULT_PASSWORD = "rigpw"


def cstr(s):
    return s.encode() + b"\0"


def read_cstr(b, off=0):
    end = b.index(b"\0", off)
    return b[off:end].decode(errors="replace"), end + 1


class Admin:
    def __init__(self, host="127.0.0.1", port=DEFAULT_PORT, password=DEFAULT_PASSWORD, timeout=10):
        self.sock = socket.create_connection((host, port), timeout=timeout)
        self._next_id = int(time.time() * 1000) % 1000000 * 100  # distinct per connection
        self.send(ADMIN_JOIN, cstr(password) + cstr("ottd") + cstr("0.2"))
        while True:
            t, p = self.recv()
            if t == S_WELCOME:
                break
            if t in (S_FULL, S_BANNED, S_ERROR):
                raise RuntimeError("join refused, packet %d payload %r" % (t, p))
        self._subscribed = False

    def send(self, ptype, payload=b""):
        self.sock.sendall(struct.pack("<HB", len(payload) + 3, ptype) + payload)

    def recv(self):
        size, ptype = struct.unpack("<HB", self._exact(3))
        return ptype, self._exact(size - 3)

    def _exact(self, n):
        buf = b""
        while len(buf) < n:
            c = self.sock.recv(n - len(buf))
            if not c:
                raise ConnectionError("server closed connection")
            buf += c
        return buf

    def rcon(self, cmd, timeout=30):
        """Run a console command; return its output lines."""
        self.sock.settimeout(timeout)
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

    def subscribe_gs(self):
        self.send(ADMIN_UPDATE_FREQ, struct.pack("<HH", UPD_GAMESCRIPT, FREQ_AUTOMATIC))
        self._subscribed = True

    def gs_request(self, obj, timeout=30):
        """Send JSON to the GameScript with a fresh id; return the reassembled reply with that id."""
        if not self._subscribed:
            self.subscribe_gs()
        self._next_id += 1
        obj = dict(obj, id=self._next_id)
        self.send(ADMIN_GS, cstr(json.dumps(obj, separators=(",", ":"))))
        deadline = time.time() + timeout
        parts = {}
        while True:
            left = deadline - time.time()
            if left <= 0:
                raise TimeoutError("no GameScript reply for cmd %r" % obj.get("cmd"))
            self.sock.settimeout(left)
            try:
                t, p = self.recv()
            except socket.timeout:
                raise TimeoutError("no GameScript reply for cmd %r" % obj.get("cmd"))
            if t != S_GS:
                continue
            reply = json.loads(read_cstr(p)[0])
            if reply.get("id") != obj["id"]:
                continue
            if "chunk" in reply:
                parts[reply["chunk"]] = reply["data"]
                if len(parts) == reply["of"]:
                    return json.loads("".join(parts[i] for i in range(reply["of"])))
                continue
            return reply

    def close(self):
        try:
            self.send(ADMIN_QUIT)
        except OSError:
            pass
        self.sock.close()
