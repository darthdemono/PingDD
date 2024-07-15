# PingDD

**PingDD** is a versatile ping tool for TCP port checking. This utility is designed to help network administrators and enthusiasts test the availability and responsiveness of specific TCP ports on remote servers.

## Features

- **Port Checking**: Test the availability of specific TCP ports on remote servers.
- **Timeout Handling**: Set a timeout for connection attempts and receive detailed error messages.
- **Cross-Platform Support**: Works on both Windows and Unix-like systems.
- **Detailed Error Messages**: Provides specific error messages such as "Request Timed Out" and "Closed Ports".
- **Metadata Embedding**: Includes detailed metadata in the executable for versioning and author information.

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
./pingdd <hostname> -p <port> -t [timeout]
```

- `hostname`: The address of the server you want to test.
- `port`: The TCP port you want to check.
- `timeout` (optional): Timeout for the connection attempt in milliseconds.

### Example

```bash
./pingdd example.com -p 80 -t 5000
```

## Error Handling

PingDD provides detailed error messages for different connection issues:

- **Request Timed Out**: When the connection attempt exceeds the specified timeout.
- **Closed Ports**: When the service is online but the port is closed.
- **Cannot Resolve Host**: When the hostname cannot be resolved to an IP address.
- **General Failure**: For any other unspecified errors.

## Contributing

Contributions are welcome! Please open an issue or submit a pull request for any improvements or bug fixes.

## License

This project is licensed under the MIT License. See the [LICENSE](LICENSE) file for details.