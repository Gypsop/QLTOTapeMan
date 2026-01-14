# QLTOTapeMan

**Qt-based LTO Tape Manager with LTFS Support**

[![License: GPL v3](https://img.shields.io/badge/License-GPLv3-blue.svg)](https://www.gnu.org/licenses/gpl-3.0)
[![Qt Version](https://img.shields.io/badge/Qt-6.0+-green.svg)](https://www.qt.io)

## Overview

QLTOTapeMan is a cross-platform Qt6/C++ application for managing LTO (Linear Tape-Open) tapes using the LTFS (Linear Tape File System) standard. It provides an intuitive graphical interface for tape operations, file management, and LTFS index handling.

## Features

- **Cross-Platform**: Supports Windows and Linux
- **Device Management**: Automatic detection and management of LTO tape drives and changers
- **LTFS Support**: Full LTFS format, mount, and unmount operations
- **File Browser**: Built-in file manager for direct read/write operations on tapes
- **Progress Tracking**: Real-time progress display with speed and ETA calculations
- **Multi-Language**: Supports English, Simplified Chinese, and Traditional Chinese

## Screenshots

*(Coming soon)*

## Requirements

### Build Requirements

- CMake 3.16 or higher
- Qt 6.0 or higher (Qt 6.2+ recommended)
- C++17 compatible compiler:
  - Windows: MSVC 2019+ or MinGW-w64 8.0+
  - Linux: GCC 9+ or Clang 10+

### Runtime Requirements

- **Windows**: SPTI (SCSI Pass-Through Interface) support
- **Linux**: sg (SCSI Generic) driver support

## Building

### Quick Start

```bash
# Clone the repository
git clone https://github.com/Gypsop/QLTOTapeMan.git
cd QLTOTapeMan

# Create build directory
mkdir build && cd build

# Configure with CMake
cmake ..

# Build
cmake --build . --config Release
```

### Using Qt Creator

1. Open `CMakeLists.txt` in Qt Creator
2. Select your Qt kit (Qt 6.0+)
3. Configure and build

### Build Options

| Option | Default | Description |
|--------|---------|-------------|
| `CMAKE_BUILD_TYPE` | `Release` | Build type (Debug/Release/MinSizeRel/RelWithDebInfo) |

## Project Structure

```
QLTOTapeMan/
├── CMakeLists.txt          # Root CMake configuration
├── LICENSE                  # GPL v3 License
├── README.md               # This file
├── src/
│   ├── libqltfs/           # Core LTFS library
│   │   ├── core/           # Core data types and structures
│   │   ├── device/         # Device and SCSI management
│   │   ├── io/             # I/O operations
│   │   ├── xml/            # XML index parsing/writing
│   │   └── util/           # Utility functions
│   └── app/                # GUI Application
│       ├── gui/            # Qt widgets and dialogs
│       ├── resources/      # Icons and resources
│       └── translations/   # Language files
└── ref_*/                  # Reference implementations
```

## Usage

### Basic Operations

1. **Launch** the application
2. **Select** a tape device from the device list
3. **Connect** to the selected device
4. **Format** a blank tape (if needed) or mount an existing LTFS tape
5. **Browse** files using the built-in file browser
6. **Copy** files to/from the tape

### File Browser

The File Browser dialog provides a complete file management interface:
- Navigate directories on tape
- Copy files and folders
- Delete files
- View file properties
- Calculate hash values

## Development

### Architecture

The project is organized into two main components:

1. **libqltfs**: A standalone C++ library providing:
   - SCSI command interface
   - Device enumeration
   - LTFS index handling (XML parsing/writing)
   - Tape I/O operations

2. **app**: Qt6 GUI application using libqltfs

### Adding New Features

1. Core functionality goes in `src/libqltfs/`
2. GUI components go in `src/app/gui/`
3. Follow the existing code style and patterns

## License

This project is licensed under the GNU General Public License v3.0 - see the [LICENSE](LICENSE) file for details.

## Author

**Jeffrey ZHU** - [zhxsh1225@gmail.com](mailto:zhxsh1225@gmail.com)

## Acknowledgments

- IBM for the [LTFS Reference Implementation](https://github.com/LinearTapeFileSystem/ltfs)
- The Qt Project for the excellent framework
- LTFSCopyGUI for inspiration

## Contributing

Contributions are welcome! Please feel free to submit a Pull Request.

1. Fork the project
2. Create your feature branch (`git checkout -b feature/AmazingFeature`)
3. Commit your changes (`git commit -m 'Add some AmazingFeature'`)
4. Push to the branch (`git push origin feature/AmazingFeature`)
5. Open a Pull Request

## Support

If you encounter any issues or have questions:

1. Check the [Issues](https://github.com/Gypsop/QLTOTapeMan/issues) page
2. Create a new issue with detailed information

---

Made with ❤️ for the tape archiving community
