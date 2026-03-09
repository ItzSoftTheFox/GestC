# HandMouse Pro

![C++](https://img.shields.io/badge/C%2B%2B-17-blue?logo=cplusplus) ![Qt](https://img.shields.io/badge/Qt-6-green?logo=qt) ![License](https://img.shields.io/badge/License-MIT-orange) ![Platform](https://img.shields.io/badge/Platform-Linux%20%7C%20Windows-lightgrey)

**Control your mouse pointer with hand gestures.** A lightweight, cross-platform desktop application that leverages AI-powered hand tracking via webcam to enable intuitive gesture-based cursor control.

## Features

✨ **Real-time Hand Tracking**
- Continuous webcam-based hand detection and landmark tracking using TensorFlow Lite and MediaPipe models
- Optimized for low-latency performance on standard hardware

🖱️ **Gesture-Based Mouse Control**
- **Hand position** → Cursor movement (with configurable sensitivity and dead zones)
- **Pinch gesture** (thumb + index finger) → Left mouse click and drag-and-drop
- **Raised middle finger** → Proportional joystick-like scrolling

🎨 **Modern Glassmorphism UI**
- Real-time calibration panel with visual feedback
- Adjustable sensitivity, dead zones, and coordinate offsets
- Responsive Qt 6 QML interface with smooth animations

🔧 **Multi-Platform Architecture**
- Native platform API integration (Linux `/dev/uinput`, Windows `SendInput`)
- Preprocessor-based platform abstraction for clean, maintainable code

⚡ **High Performance**
- Efficient C++17 implementation
- Optimized OpenCV processing pipeline
- Minimal CPU/GPU footprint

## Prerequisites

### System Requirements

#### Arch Linux
```bash
# Essential build tools
sudo pacman -S base-devel cmake ninja

# Graphics and UI
sudo pacman -S qt6-base qt6-declarative libgl

# Computer vision
sudo pacman -S opencv

# TensorFlow Lite (via AUR or local build)
# Consider using mediapipe-cpp from AUR or building from source

# Input device access
sudo pacman -S libinput
```

#### Debian/Ubuntu
```bash
# Essential build tools
sudo apt-get update
sudo apt-get install build-essential cmake ninja-build

# Graphics and UI
sudo apt-get install qt6-base-dev qt6-declarative-dev libgl1-mesa-dev

# Computer vision
sudo apt-get install libopencv-dev

# Input device access (Linux)
sudo apt-get install libinput-dev
```

#### Windows (Visual Studio 2022+)
- **Visual Studio 2022** with C++ workload (MSVC compiler)
- **CMake** (3.22+) – [Download](https://cmake.org/download/)
- **Qt 6** – [Download](https://www.qt.io/download-qt-installer)
- **OpenCV** – Pre-built binaries or build from source
- **TensorFlow Lite** – MediaPipe C++ SDK

### Dependencies
| Dependency | Version | Purpose |
|-----------|---------|---------|
| C++ Standard | C++17 | Core language |
| CMake | 3.22+ | Build system |
| Qt | 6.0+ | UI framework |
| OpenCV | 4.5+ | Image processing |
| TensorFlow Lite | Latest | ML inference |
| MediaPipe | Latest | Hand tracking models |

## Build Instructions

### Linux (Arch/Debian)

1. **Clone the repository**
   ```bash
   git clone https://github.com/yourusername/GestC.git
   cd GestC
   ```

2. **Install dependencies** (see Prerequisites section)

3. **Create build directory**
   ```bash
   mkdir -p build && cd build
   ```

4. **Configure with CMake**
   ```bash
   cmake -G Ninja -DCMAKE_BUILD_TYPE=Release ..
   ```

5. **Build the project**
   ```bash
   ninja
   ```

6. **Run the application**
   ```bash
   ./GestC
   ```

### Windows (Visual Studio)

1. **Clone the repository**
   ```bash
   git clone https://github.com/yourusername/GestC.git
   cd GestC
   ```

2. **Create build directory**
   ```bash
   mkdir build && cd build
   ```

3. **Configure with CMake**
   ```bash
   cmake -G "Visual Studio 17 2022" -A x64 -DCMAKE_BUILD_TYPE=Release ..
   ```

4. **Build the project**
   ```bash
   cmake --build . --config Release
   ```

5. **Run the application**
   ```bash
   Release\GestC.exe
   ```

### macOS (Coming Soon)
Full native support planned for future releases.

## Post-Build Configuration

### Linux: Setting up `/dev/uinput` Access

The application requires write access to `/dev/uinput` to control the mouse pointer. Follow these steps:

1. **Check your user groups**
   ```bash
   groups $USER
   ```

2. **Add your user to the `input` group** (if not already present)
   ```bash
   sudo usermod -a -G input $USER
   ```

3. **Create/modify udev rules** (recommended for permanent access)
   ```bash
   sudo tee /etc/udev/rules.d/99-uinput.rules > /dev/null <<EOF
   KERNEL=="uinput", MODE="0666", TAG+="uaccess"
   EOF
   ```

4. **Reload udev rules**
   ```bash
   sudo udevadm control --reload-rules
   sudo udevadm trigger
   ```

5. **Log out and log back in** (or restart) for group changes to take effect

### Windows: No Additional Configuration Required
Windows uses `SendInput` API which integrates directly with the Windows message queue. No additional setup is needed.

## Usage

### Running the Application

1. **Start the application**
   ```bash
   ./GestC  # Linux
   GestC.exe  # Windows
   ```

2. **Grant webcam permissions** (may be prompted by your OS)

3. **Adjust calibration settings** in the glassmorphism UI panel:
   - **Sensitivity**: Control cursor responsiveness to hand movement
   - **Dead Zone**: Prevent drift when hand is stationary
   - **X/Y Offsets**: Fine-tune cursor position mapping

### Gesture Manual

| Gesture | Action | Notes |
|---------|--------|-------|
| **Open hand** | Move cursor | Hand position maps to screen coordinates |
| **Pinch** (thumb + index) | Left click | Hold for drag-and-drop operations |
| **Raised middle finger** | Scroll | Upward/downward motion scrolls; speed is proportional |
| **Palm facing camera** | Calibration mode | Activate for gesture training (optional) |

### Tips for Best Results
- **Lighting**: Ensure adequate, even lighting on your hand and face
- **Distance**: Position camera 30-60cm away from your hand
- **Contrast**: Avoid wearing colors similar to your skin tone
- **Background**: Use a non-reflective background

## Roadmap

### ✅ Current (v1.0)
- Linux support with `/dev/uinput` backend
- Windows support with `SendInput` backend
- Core hand gesture recognition
- Glassmorphism UI calibration panel

### 🔜 Planned
- **v1.1**: Native macOS support with `Quartz` event API
- **v1.2**: Hand pose classification (peace sign, thumbs up, etc.) for custom actions
- **v2.0**: Multi-hand gesture recognition (two-hand gestures)
- **v2.1**: Voice command integration
- **v2.2**: Gesture recording and macro system
- **Distant Future**: GPU acceleration for hand tracking pipeline

## Contributing

We welcome contributions from the community! Whether it's bug reports, feature requests, or pull requests, your help makes this project better.

### Getting Started
1. Fork the repository
2. Create a feature branch: `git checkout -b feature/amazing-feature`
3. Commit your changes: `git commit -m 'Add amazing feature'`
4. Push to the branch: `git push origin feature/amazing-feature`
5. Open a Pull Request

### Code Style
- Follow C++17 conventions
- Use PascalCase for classes, camelCase for variables and methods
- Document complex logic with inline comments
- Ensure cross-platform compatibility

### Reporting Issues
- Use descriptive titles
- Include your OS version, build type, and steps to reproduce
- Attach relevant logs or screenshots
- Specify your hardware setup (webcam model, resolution, etc.)

### Development Setup
```bash
# Clone your fork
git clone https://github.com/yourusername/GestC.git

# Create a development branch
git checkout -b develop

# Build with debug symbols
mkdir build-debug && cd build-debug
cmake -DCMAKE_BUILD_TYPE=Debug ..
ninja
```

## License

This project is licensed under the **MIT License** – see the [LICENSE](LICENSE) file for details.

### Third-Party Licenses
- **Qt 6**: LGPL v3 / Commercial
- **OpenCV**: Apache 2 License
- **TensorFlow Lite**: Apache 2 License
- **MediaPipe**: Apache 2 License

## Acknowledgments

- **MediaPipe** for excellent hand-tracking models
- **Qt Project** for the cross-platform UI framework
- **OpenCV** for robust computer vision tools

---

**Questions?** Open an issue or start a discussion on GitHub.

**Want to support this project?** Star ⭐ us on GitHub!
