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

- TCP port reachability check (connect-based).
- Measures connect time (RTT) in milliseconds (microsecond level precision).
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

pingdd <destination> -p <port> [options]

```

| Option            | Description                                   | Required | Default        |
| :---------------- | :-------------------------------------------- | :------- | :------------- |
| `<destination>`   | Target hostname or IP address                 | Yes      | -              |
| `-p, --port N`    | Set TCP port N                                | Yes      | -              |
| `-t, --timeout N` | Timeout in milliseconds                       | No       | `1000`         |
| `-c, --count N`   | Number of checks                              | No       | infinite       |
| `-r, --rate N`    | Rate: 1 check per N ms (delay between checks) | No       | `50ms`         |
| `--no-color`      | Disable color output                          | No       | Colors enabled |
| `--csv`           | Enable CSV logging                            | No       | Disabled       |
| `-?, --help`      | Display help                                  | No       | -              |


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

MIT License. See [LICENSE](LICENSE).