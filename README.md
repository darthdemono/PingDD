<p align="center">
  <img src="https://raw.githubusercontent.com/darthdemono/PingDD/refs/heads/main/pic/icon/PingDD%20Icon%20Small.png" alt="PingDD Icon">
</p>

# PingDD

PingDD is a cross-platform TCP, UDP, and ICMP reachability tool written in C. You give it a host and a port; it tells you whether that port is open, how fast the handshake completes, and, if you ask, keeps a CSV record of every attempt so you can compare runs later.

It began as a TCP "ping," which is where the name comes from. It now does considerably more than ping.

<p align="center">
  <img src="https://raw.githubusercontent.com/darthdemono/PingDD/refs/heads/main/pic/github/Screenshot.png" alt="PingDD Screenshot">
</p>

---

## Why PingDD exists

Classic `ping` answers exactly one question: can I reach this host over ICMP? In real troubleshooting, that is almost never the question you actually have. The real questions are:

- Is the service port open at all: 80, 443, 22, 3389, whatever you care about?
- Is the path slow because of latency, filtering, or a sluggish handshake?
- Was the service down ten minutes ago, and can I prove it afterward?

ICMP answers none of those. A host can happily reply to pings while the service behind it is dead, and plenty of hosts drop ICMP entirely while serving traffic just fine. So PingDD checks the thing you actually care about: the port. It measures how long the connection takes, and keeps the evidence.

---

## Features

- TCP and UDP port checks (connect-based for TCP, datagram-based for UDP).
- ICMP echo mode (classic ping) over IPv4 and IPv6.
- Connect time (RTT) in milliseconds, measured with microsecond precision.
- IPv4 and IPv6, resolved automatically from A and AAAA records. If the first address is unreachable, it falls through to the next.
- Multi-target probing: many hosts × ports (lists and ranges) × protocols, run sequentially or concurrently.
- Source-interface selection: bind probes to a specific NIC or IP, so you can compare wifi against ethernet against a VPN on the same target.
- Summary statistics: min/max/average, standard deviation, and p50/p95/p99 percentiles.
- Network-quality diagnostics: downtime detection, packet loss, jitter, latency spikes, a bufferbloat estimate, DNS timing, and a one-word verdict of `HEALTHY`, `DEGRADED`, or `DOWN`.
- Three operator modes: availability monitoring, authorized load testing, and a resilience sweep.
- Machine-readable JSON output, or CSV logging for long runs.
- Colored output that turns itself off when piped, and honors `NO_COLOR`.

---

## Installation

### Windows

#### Winget

Install with [Winget](https://learn.microsoft.com/en-us/windows/package-manager/). It drops PingDD on your `PATH` automatically.

```bash
winget install -e --id DarthDemono.PingDD
```

### Linux

#### From Releases

Grab the latest binary from the Releases page:

https://github.com/darthdemono/PingDD/releases

#### Compile from Source

If your distro is not in the release list, build it yourself. The only hard dependency is a C compiler and `make`.

```bash
git clone https://github.com/darthdemono/PingDD.git
cd PingDD
make clean
make
```

Then put the binary on your `PATH`:

- https://www.sysadmit.com/2016/06/linux-anadir-ruta-al-path.html

---

## Usage

```bash
pingdd <host> [host ...] -p <port[,list,a-b]> [options]
```

| Option              | Description                                       | Required | Default       |
| :------------------ | :------------------------------------------------ | :------- | :------------ |
| `<host> ...`        | One or more target hostnames/IPs (IPv4 or IPv6)   | Yes\*    | -             |
| `-p, --port P`      | Port, comma list, or range (e.g. `80,443,8000-8010`) | TCP/UDP | -          |
| `-P, --protocol P`  | Protocol list: `TCP`, `UDP`, `ICMP` (comma-separated) | No   | `TCP`         |
| `-I, --interface X` | Bind probes to a source IP or interface name      | No       | default route |
| `--target SPEC`     | Add a `host:port/proto` target (repeatable)       | No       | -             |
| `--targets FILE`    | Read targets from a file, one per line            | No       | -             |
| `--concurrent`      | Probe every target in parallel each cycle         | No       | sequential    |
| `-t, --timeout N`   | Per-probe timeout in milliseconds                 | No       | `1000`        |
| `-c, --count N`     | Number of checks                                  | No       | infinite      |
| `-r, --rate N`      | Delay between checks, in milliseconds             | No       | `50`          |
| `-w, --deadline N`  | Stop after N milliseconds total                   | No       | none          |
| `-q, --quiet`       | Suppress per-probe lines, print the summary only  | No       | off           |
| `-a, --audible`     | Ring the terminal bell on each success            | No       | off           |
| `--json`            | Emit machine-readable JSON (implies no color)     | No       | off           |
| `--csv`             | Log every attempt to a CSV file                   | No       | off           |
| `--color`           | Force colored output                              | No       | auto (TTY)    |
| `--no-color`        | Disable colored output                            | No       | auto (TTY)    |
| `-V, --version`     | Print the version                                 | No       | -             |
| `-?, --help`        | Print help                                        | No       | -             |

\* You need a destination, but it does not have to be a positional host. `--target` or `--targets` works too. Give it several hosts, a port list or range, and a protocol list, and PingDD expands the combination into a full host × port × protocol matrix. ICMP targets ignore the port. Every target keeps its own statistics and diagnostics, and you get a combined roll-up at the end.

Color is automatic: on when output is a terminal, off when it is piped or when the [`NO_COLOR`](https://no-color.org/) environment variable is set.

ICMP mode uses no port, so `-p` is optional there. ICMP may need elevated privileges (`CAP_NET_RAW`) on systems that do not allow unprivileged ICMP datagram sockets.

---

## Diagnostics

PingDD does not just count drops; it tries to tell you whether a drop means anything.

A single failed probe surrounded by successes is noise. A network has bad seconds. So PingDD only calls a target `DOWN` after several consecutive failures, and it tracks how long the outage lasted and how many times it happened. One lost packet is not an outage. Three in a row is.

The summary grades the link from loss, jitter, and latency, estimates bufferbloat from how far the slow probes drift above the baseline, and times the DNS lookup separately so a slow resolver does not get blamed on the network. The result is a one-word verdict, `HEALTHY`, `DEGRADED`, or `DOWN`, plus the numbers behind it. With `--monitor` it also prints an availability percentage.

---

## Monitoring & authorized testing

PingDD has three modes aimed at operators rather than one-off checks.

| Mode         | Flag           | What it does                                                          |
| :----------- | :------------- | :------------------------------------------------------------------- |
| Availability | `--monitor`    | Probes continuously, raises outage and recovery alerts, reports uptime. |
| Load test    | `--load-test`  | Opens sustained concurrent connections to measure behavior under load. |
| Resilience   | `--resilience` | Ramps concurrency in steps and reports where the service starts to degrade. |

Load-test and resilience take `--concurrency N` (1–256) and `--duration N` (seconds, 1–3600).

A word on what PingDD is not. It is not a DoS tool, and it will not become one. There is no packet flooding, no amplification, no reflection, no spoofing, and no filter bypass. None of it, by design. Load testing exists to measure your own service under pressure, nothing else. Because it does generate real load, `--load-test` and `--resilience` refuse to run without `--authorize`, and they refuse a public target unless you also pass `--allow-public`. Concurrency is capped at 256 and duration at one hour. Use it on systems you own or have written permission to test. The rest is on you.

```bash
# Watch a service and report availability:
pingdd 192.168.1.10 -p 443 --monitor

# Load test your own service (loopback or private), 50 connections for 30s:
pingdd 127.0.0.1 -p 8080 --load-test --authorize --concurrency 50 --duration 30

# Find the concurrency at which your service starts to degrade:
pingdd 10.0.0.5 -p 80 --resilience --authorize --concurrency 128 --duration 60
```

---

## Examples

Check port 80 a hundred times:

```bash
pingdd example.com -p 80 -c 100
```

Slow it down to one check every 500 milliseconds:

```bash
pingdd example.com -p 443 -r 500
```

Log every attempt to CSV:

```bash
pingdd example.com -p 443 --csv
```

Probe two hosts across a port range and two protocols at once:

```bash
pingdd 1.1.1.1 8.8.8.8 -p 53,443,8000-8005 -P TCP,UDP
```

Compare the same target over wifi and ethernet by binding the source interface:

```bash
pingdd 1.1.1.1 -p 443 -I wlan0
pingdd 1.1.1.1 -p 443 -I eth0
```

Probe explicit targets in parallel, or read them from a file:

```bash
pingdd --target 1.1.1.1:443/tcp --target 8.8.8.8:53/udp --concurrent
pingdd --targets hosts.txt --monitor
```

---

## CSV output

With `--csv`, PingDD writes a timestamped file and records one row per successful probe:

- DateTime
- Host
- IPAddress
- Protocol
- Port
- Time_ms

`IPAddress` is the address actually probed, not just the first one resolved, so when fail-over picks a different address the log shows the truth. Feed the file to whatever you graph with, compare it against another network, or keep it as a record for the next time someone insists the service "was never down."

---

## Output formats

By default the output is colored and human-readable. Two alternatives exist for everything else:

- `--csv` keeps a row per success, as above.
- `--json` prints one JSON object per probe plus a summary and a diagnostics object, with color disabled. It is line-delimited, so you can pipe it straight into `jq` or anything that reads NDJSON.

---

## Building for other platforms

The makefile targets Windows (x86 and ARM64) and Linux (x86_64, ARM, ARM64).

```bash
make                # auto-detect the host
make win32          # Windows x86      -> bin/win-x86/pingdd.exe
make winarm64       # Windows ARM64    -> bin/win-arm64/pingdd.exe
make linux          # Linux native     -> bin/linux/pingdd
make linuxarm       # Linux ARM 32-bit -> bin/linux-arm/pingdd
make linuxarm64     # Linux ARM64      -> bin/linux-arm64/pingdd
```

Cross-compiling needs the matching toolchain (`i686-w64-mingw32-gcc` for Windows, `aarch64-linux-gnu-gcc` for Linux ARM64, and so on). The Windows targets also need a resource compiler; the makefile finds `windres` on its own, whether it is the plain `windres` from MSYS2 or the cross-prefixed one from a Linux mingw package.

---

## Project notes

PingDD keeps the code straightforward and readable. It prefers predictable behavior, clean output, and portability over clever tricks. Changes that improve reliability and cross-platform correctness win over changes that just look smart.

> "An idiot admires complexity, a genius admires simplicity" — [Terry A. Davis](https://en.wikipedia.org/wiki/Terry_A._Davis)

---

## Contributing

Contributions are welcome.

- Open an issue for bugs, feature requests, or suggestions.
- Send a pull request if you want to improve the code, the portability, the documentation, or the CI packaging.

---

## License

MIT License. See [LICENSE](https://github.com/darthdemono/PingDD/blob/main/LICENSE).
