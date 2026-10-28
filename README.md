# Qt6 Control Application for a Customized VNA System

Desktop application for controlling a self-built, low-cost measurement system for **real-time resonance analysis without a commercial Vector Network Analyzer (VNA)**. Built with C++ and Qt6, it runs as a standalone program on any supported system, with no Qt installation required for end users.

<!-- Add a screenshot or GIF of the main window here: -->
<!-- ![Main window](docs/screenshot.png) -->

## System overview

```
 ┌────────────────────┐     <UART>      ┌──────────────┐        ┌───────────────┐
 │  Qt6 control app   │ ◄─────────────► │  STM32F407   │ ─────► │   ADF4351     │ ──► Device under test
 │  (this repository) │                 │  firmware    │        │  synthesizer  │     (resonant structure)
 └────────────────────┘                 └──────────────┘        └───────────────┘
```

## Features

- Control of the synthesizer: frequency range, step size and sweep settings
- Real-time measurement and live plotting of the frequency response
- Resonance analysis without a VNA
- Standalone deployment: runs without a Qt installation

## Download

Ready-to-run packages are available on the **[Releases page](../../releases)**.

1. Download the package for your system (for example `VNA-control-app-v1.0.0.zip`).
2. Extract the archive.
3. Run the executable inside the extracted folder.

## Deployment (Windows)
 
Follow these steps to create a standalone package that runs on systems without Qt installed. The folder names below are examples, so replace them with your own paths.
 
1. **Build in Release mode.** In Qt Creator, select the *Release* configuration with the *Desktop Qt 6.x MSVC 64-bit* kit and build the project. A Debug build produces a larger package that needs debug DLLs.
2. **Copy the executable to an empty folder.** `windeployqt` copies all required files next to the executable, so do not deploy inside the build directory:
```
   <deploy-dir>\<app-name>\<app-name>.exe
```
 
3. **Open the Qt command prompt** for your kit, for example *Qt 6.x (MSVC 2022 64-bit)* from the Start menu. It has the Qt and compiler paths already set. The location the prompt opens in does not matter.
4. **Run `windeployqt`** on the executable:
```
   cd /d <deploy-dir>\<app-name>
   windeployqt --release --no-translations <app-name>.exe
```
 
   Or without changing the directory:
 
```
   windeployqt --release --no-translations <deploy-dir>\<app-name>\<app-name>.exe
```

## Usage

1. Connect the hardware and power the system.
2. Start the application and select the connection port.
3. Set the frequency range and step size.
4. Start the measurement and observe the response in real time.

## Project context

This application was developed as part of a research project on **metamaterial-based frequency sensing** at the University of Stuttgart / Vietnamese-German University. The goal was a measurement concept that determines resonances without a commercial VNA, using a custom synthesizer-based setup.

<!-- Add a short note if the sensing concept was evaluated only conceptually, for glucose sensing. -->

## Related repositories

- Firmware (STM32F407, ADF4351 control): [https://github.com/<your-username>/stm32f407-synthesizer](https://github.com/ThienBui2x/Synthesizer-for-customized-VNA)
