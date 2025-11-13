import socket
import time
import errno
import matplotlib
import matplotlib.pyplot as plt
matplotlib.use("Agg")

HOST = "z34_udp_server"
PORT = 8888
TIMEOUT_S = 2.0
PNG_PATH = "results.png"


def attempt(size: int):
    """Try sending 'size' bytes; return (ok, rtt_ms, reason|None)."""
    s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    s.settimeout(TIMEOUT_S)

    try:
        s.setsockopt(socket.IPPROTO_IP, 10, 2)
    except OSError:
        pass

    data = b"A" * size
    t0 = time.perf_counter()

    try:
        s.sendto(data, (HOST, PORT))
        s.recvfrom(1)

        rtt_ms = (time.perf_counter() - t0) * 1000.0
        s.close()

        return True, rtt_ms, None
    except socket.timeout:
        s.close()
        return False, None, "timeout"
    except OSError as e:
        s.close()
        if getattr(e, "errno", None) == errno.EMSGSIZE:
            return False, None, "EMSGSIZE (MTU exceeded)"
        return False, None, f"oserror {e.errno}"


print(f"Test host={HOST} port={PORT} timeout={TIMEOUT_S}s")

rtt_ok = {}
size = 2
last_ok, first_fail = 0, None

while True:
    ok, rtt, why = attempt(size)
    if ok:
        print(f"{size:6d} B -> OK   RTT={rtt:.2f} ms")
        rtt_ok[size] = rtt
        last_ok = size
        size *= 2
        if size > 65507:
            first_fail = size
            break
    else:
        print(f"{size:6d} B -> FAIL ({why})")
        first_fail = size
        break

lo, hi = last_ok, first_fail
while hi - lo > 1:
    mid = (lo + hi) // 2
    ok, rtt, why = attempt(mid)
    if ok:
        print(f"{mid:6d} B -> OK   RTT={rtt:.2f} ms")
        rtt_ok[mid] = rtt
        lo = mid
    else:
        print(f"{mid:6d} B -> FAIL ({why})")
        hi = mid

max_ok = lo
print(f"MAX_OK = {max_ok} bytes (MTU Limit)")


sizes = sorted(rtt_ok.keys())
rtts = [rtt_ok[s] for s in sizes]

plt.figure()
if sizes:
    plt.scatter(sizes, rtts, label="RTT [ms]")
plt.axvline(max_ok, linestyle="--", label=f"MAX_OK = {max_ok} B")
plt.xlabel("UDP payload size [bytes]")
plt.ylabel("RTT [ms]")
plt.title("UDP RTT vs payload size")
plt.legend()
plt.tight_layout()
plt.savefig(PNG_PATH, dpi=130)
print(f"Saved plot: {PNG_PATH}")
