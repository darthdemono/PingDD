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
./pingdd <hostname> <port> [timeout]
```

- `hostname`: The address of the server you want to test.
- `port`: The TCP port you want to check.
- `timeout` (optional): Timeout for the connection attempt in milliseconds.

### Example

```bash
./pingdd example.com 80 5000
```

## Error Handling

PingDD provides detailed error messages for different connection issues:

- **Request Timed Out**: When the connection attempt exceeds the specified timeout.
- **Closed Ports**: When the service is online but the port is closed.
- **Cannot Resolve Host**: When the hostname cannot be resolved to an IP address.
- **General Failure**: For any other unspecified errors.

## Metadata

PingDD includes the following metadata in the executable:

- **Name**: PingDD
- **Version**: 1.0.0_beta0.1
- **Author**: Jubair Hasan (Joy)
- **Description**: A ping tool for TCP port checking
- **Product Name**: PingDD

## Directory Structure

```
pingdd/
├── bin/
│   └── x64/
├── obj/
│   └── x64/
├── src/
│   ├── main.cpp
│   ├── socket.cpp
│   ├── timer.cpp
│   └── i18n.cpp
├── version.rc
├── Makefile
├── CMakeLists.txt
└── README.md
```

## Contributing

Contributions are welcome! Please open an issue or submit a pull request for any improvements or bug fixes.

## License

This project is licensed under the MIT License. See the [LICENSE](LICENSE) file for details.

## Contact

For any inquiries or support, please contact Jubair Hasan (Joy) at [your-email@example.com].

---

**PingDD** - Your reliable tool for TCP port checking.
```

### Explanation

- **Project Overview**: Brief introduction to what PingDD does.
- **Features**: Highlight key features of the tool.
- **Installation**: Detailed steps on how to clone and build the project.
- **Usage**: Instructions on how to use the tool with example commands.
- **Error Handling**: Description of the error messages provided by the tool.
- **Metadata**: Information about the embedded metadata.
- **Directory Structure**: Overview of the project's directory structure.
- **Contributing**: Information for potential contributors.
- **License**: Licensing information.
- **Contact**: Contact details for the author.

Make sure to replace `https://github.com/yourusername/pingdd.git` with the actual URL of your GitHub repository and `[your-email@example.com]` with your actual email address. This README should provide a comprehensive overview of your project and guide users on how to use and contribute to it.