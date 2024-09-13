# PingDD

**PingDD** is a ping tool for TCP port checking. This utility is designed to help network administrators and enthusiasts test the availability and responsiveness of specific TCP ports on remote servers.

## Installation

### Prerequisites

- A C++ compiler (e.g., g++)
- CMake or GNU Make
- Git

### Cloning the Repository

```bash
git clone https://github.com/yourusername/pingdd.git
cd pingdd
```

### Building the Project

#### Using GNU Make

```bash
make
```

#### Using CMake

```bash
mkdir build
cd build
cmake ..
make
```

## Usage

```bash
./pingdd <hostname> -p <port> -c [time] -t [timeout]
```

- `hostname`: The address of the server you want to test.
- `port`: The TCP port you want to check.
- `time`: The amount of times port has to be pinged (Default infinite)
- `timeout` (optional): Timeout for the connection attempt in milliseconds.
- 
### Example

```bash
./pingdd example.com -p 80 -c 100
```

## Contributing

Contributions are welcome! Please open an issue or submit a pull request for any improvements or bug fixes.

## License

This project is licensed under the MIT License. See the [LICENSE](LICENSE) file for details.
