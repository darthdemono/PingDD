<p align="center">
  <img src="https://raw.githubusercontent.com/darthdemono/PingDD/refs/heads/main/pic/icon/PingDD%20Icon%20Small.png" alt="PingDD Icon">
</p>

# PingDD

**PingDD** is a cross-platform TCP “ping” tool written in C.  
Instead of ICMP, it checks a specific TCP port and tells you whether it’s reachable, how fast the connection handshake completes, and (optionally) logs every attempt to CSV for later analysis. 
This tool is designed to help network administrators and enthusiasts test the availability and responsiveness on remote servers.

<p align="center">
  <img src="https://raw.githubusercontent.com/darthdemono/PingDD/refs/heads/main/pic/github/Screenshot.png" alt="PingDD Screenshot">
</p>

---

## Why PingDD exists

Classic `ping` answers one question: “Can I reach this host over *ICMP*?”  
In real-world troubleshooting, the question is usually different:

- Is the service port actually open (80/443/22/3389/etc..)?
- Is the connection slow because of latency, filtering, or handshake delays?
- Can the results be logged and compared later?

PingDD is built to solve these issues: simple feedback with clean output, and an option to keep evidence (CSV + timestamps).

---

## Features

- TCP and UDP port reachability checks (connect/datagram based).
- ICMP echo mode (classic ping) over IPv4 and IPv6.
- Measures connect time (RTT) in milliseconds (microsecond level precision).
- IPv4 and IPv6 support (automatic resolution of A/AAAA records).
- Multi-target monitoring: many hosts x ports (lists/ranges) x protocols, sequential or concurrent.
- Source-interface selection (bind probes to a NIC/IP) to compare wifi vs ethernet vs VPN paths.
- Summary statistics including min/max/average, standard deviation, and p50/p95/p99 percentiles.
- Network-quality diagnostics: outage/downtime detection, packet loss, jitter, latency spikes, bufferbloat estimate, DNS timing, and a noise-vs-issue verdict (HEALTHY / DEGRADED / DOWN).
- Cross-platform behavior (Windows + Linux).
- Colored terminal output (can be disabled).
- Timestamp printed in output (useful for diagnostics).
- Optional CSV logging (best for long runs and later review).

---

## Installation

### Windows

#### Winget

Download using [Winget](https://learn.microsoft.com/en-us/windows/package-manager/).  
Winget installs PingDD and adds it to your `PATH` automatically.

```bash
winget install -e --id DarthDemono.PingDD
```

### Linux

#### From Releases

Download the latest binary from the GitHub Releases page:

https://github.com/darthdemono/PingDD/releases

#### Compile from Source

If your OS is not available in the release list, compile from source:

```bash
git clone https://github.com/darthdemono/PingDD.git
cd PingDD
make clean
make
```

Then add the binary to your `PATH`:

- https://www.sysadmit.com/2016/06/linux-anadir-ruta-al-path.html

---

## Usage

```bash

pingdd <host> [host ...] -p <port[,list,a-b]> [options]

```

| Option            | Description                                   | Required | Default        |
| :---------------- | :-------------------------------------------- | :------- | :------------- |
| `<host> ...`      | One or more target hostnames/IPs (IPv4/IPv6)  | Yes*     | -              |
| `-p, --port P`    | Port, list, or range (e.g. `80,443,8000-8010`)| TCP/UDP  | -              |
| `-P, --protocol P`| Protocol list: `TCP`,`UDP`,`ICMP` (comma-sep) | No       | `TCP`          |
| `-I, --interface X`| Bind probes to a source IP or interface name | No       | default route  |
| `--target SPEC`   | Add a target `host:port/proto` (repeatable)   | No       | -              |
| `--targets FILE`  | Read targets from a file (one per line)       | No       | -              |
| `--concurrent`    | Probe all targets in parallel each cycle      | No       | sequential     |
| `-t, --timeout N` | Per-probe timeout in milliseconds             | No       | `1000`         |
| `-c, --count N`   | Number of checks                              | No       | infinite       |
| `-r, --rate N`    | Rate: 1 check per N ms (delay between checks) | No       | `50ms`         |
| `-w, --deadline N`| Stop after N milliseconds total               | No       | none           |
| `-q, --quiet`     | Suppress per-probe lines, summary only        | No       | Disabled       |
| `-a, --audible`   | Ring the terminal bell on each success        | No       | Disabled       |
| `--json`          | Emit machine-readable JSON (implies no color) | No       | Disabled       |
| `--csv`           | Enable CSV logging                            | No       | Disabled       |
| `--color`         | Force colored output                          | No       | auto (TTY)     |
| `--no-color`      | Disable color output                          | No       | auto (TTY)     |
| `-V, --version`   | Display version                               | No       | -              |
| `-?, --help`      | Display help                                  | No       | -              |

\* A destination is required, but it can come from positional hosts,
`--target`, or `--targets` instead of a single host argument. Multiple hosts,
a port list/range, and a protocol list expand into a target matrix
(host x port x protocol); ICMP targets ignore the port. Each target gets its
own statistics and diagnostics, with a combined roll-up at the end.

Color is auto-detected: enabled on a terminal, disabled when piped or when the
[`NO_COLOR`](https://no-color.org/) environment variable is set.

In `--protocol ICMP` mode no port is used, so `-p` is not required. ICMP may need
elevated privileges (e.g. `CAP_NET_RAW`) on systems where unprivileged ICMP
datagram sockets are not permitted.

---

## Monitoring & authorized testing

PingDD includes three operator-focused modes for defensive use:

| Mode            | Flag           | Purpose                                                        |
| :-------------- | :------------- | :------------------------------------------------------------- |
| Availability    | `--monitor`    | Continuous probing with outage alerts and an availability %.   |
| Load test       | `--load-test`  | Sustained concurrent connections to measure behavior under load.|
| Resilience      | `--resilience` | Ramps concurrency and reports where the service degrades.       |

Load-test / resilience options: `--concurrency N` (1–256), `--duration N`
(seconds, 1–3600).

> **Authorized use only.** `--load-test` and `--resilience` generate real
> connection load and therefore require `--authorize` to confirm you own or
> have written permission to test the target. Public (non-private) targets are
> refused unless you also pass `--allow-public`. Concurrency is capped at 256
> and duration at one hour. PingDD has **no** packet-flooding, amplification,
> reflection, spoofing, or filter-bypass capability — it is a measurement tool,
> not an attack tool. You are responsible for using it lawfully.

```bash
# Watch a service and report availability:
pingdd 192.168.1.10 -p 443 --monitor

# Load test your own service (loopback / private), 50 conns for 30s:
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

Slow the rate to one check every 500ms:

```bash
pingdd example.com -p 443 -r 500
```

Enable CSV logging:

```bash
pingdd example.com -p 443 --csv
```

Probe several hosts across a port range and two protocols at once:

```bash
pingdd 1.1.1.1 8.8.8.8 -p 53,443,8000-8005 -P TCP,UDP
```

Compare the same target over wifi vs ethernet (bind the source interface):

```bash
pingdd 1.1.1.1 -p 443 -I wlan0
pingdd 1.1.1.1 -p 443 -I eth0
```

Probe explicit targets concurrently, or from a file:

```bash
pingdd --target 1.1.1.1:443/tcp --target 8.8.8.8:53/udp --concurrent
pingdd --targets hosts.txt --monitor
```


---

## CSV output

When CSV logging is enabled, PingDD generates a timestamped filename and writes:

- DateTime
- Host
- IPAddress
- Protocol
- Port
- Time_ms

This makes it easy to graph results later, compare different networks, or keep records for debugging.

---

## Project notes

PingDD aims to keep the codebase straightforward and readable.
The project leans toward predictable behavior, clean output, and portability—so changes that improve reliability and cross-platform correctness are preferred over “clever” complexity.

> "An idiot admires complexity, a genius admires simplicity" - [Terry A. Davis](https://en.wikipedia.org/wiki/Terry_A._Davis)

---

## Contributing

Contributions are welcome.

- Open an issue for bugs, feature requests, or suggestions.
- Submit a pull request if you want to improve code quality, portability, documentation, or CI packaging.

---

## License

MIT License. See [LICENSE](https://github.com/darthdemono/PingDD/blob/main/LICENSE)