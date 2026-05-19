# CMake Tools Extension for pi

A project-local pi extension that provides two custom tools for CMake workflows:

- **`cmake_configure`** — Configure the CMake project using a preset
- **`cmake_build`** — Build the CMake project (all targets or specific target)

## Features

- **VS Dev Environment Auto-Setup** — Automatically detects and configures Visual Studio Developer Environment on Windows via `./Scripts/Enter-VsDevEnv.ps1`
- **Preset Validation** — Validates that the specified preset exists in `CMakePresets.json` before running
- **Filtered Output** — Surfaces only meaningful information (built targets, warnings, errors) while saving the full log
- **Log Management** — Full logs saved to `.pi/logs/` with automatic cleanup (keeps last 2 per action)
- **Custom TUI Rendering** — Compact default view with expanded detail on demand

## Tools

### `cmake_configure`

Configures the CMake project using a preset.

**Parameters:**
| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| `preset` | string | `"windows"` | CMake preset name (must exist in `CMakePresets.json`) |
| `verbose` | boolean | `false` | Pass `--debug-output` to cmake |

**Example:**
```
cmake_configure(preset: "windows", verbose: false)
```

**Output includes:**
- Generator name (e.g., "Ninja Multi-Config")
- Compiler detection (MSVC/clang-cl/g++)
- Key cache variables (BUILD_SHARED_LIBS, CMAKE_BUILD_TYPE, etc.)
- Warnings and errors with context
- Full log file path

### `cmake_build`

Builds the CMake project.

**Parameters:**
| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| `target` | string | (all) | Specific target to build. Omit to build all |
| `verbose` | boolean | `false` | Pass `--verbose` to cmake --build |

**Example:**
```
cmake_build(target: "iris_exe", verbose: false)
cmake_build()  # Build all targets
```

**Output includes:**
- Built targets list
- Executable and library output paths
- Compiler warnings with file:line context
- Compiler errors with file:line context
- Incremental build detection (up-to-date status)
- Full log file path

## Log Files

Full cmake output is saved to `.pi/logs/`:
- `cmake-configure-<timestamp>.log` — Configure logs
- `cmake-build-<timestamp>.log` — Build logs

Only the last 2 logs per action type are kept to minimize disk usage.

## Requirements

- **CMake** — Must be installed and in PATH
- **PowerShell (pwsh)** — Required for VS Dev Environment setup on Windows
- **`./Scripts/Enter-VsDevEnv.ps1`** — VS Dev Environment initialization script (project-specific)
- **`CMakePresets.json`** — CMake presets file in project root

## VS Dev Environment

On Windows, the extension checks for VS Developer Environment variables (`VCINSTALLDIR`, `VCToolsVersion`, `VisualStudioVersion`) before running cmake commands. If not detected, it runs `./Scripts/Enter-VsDevEnv.ps1` to set them up.

The script uses `vswhere.exe` to find the latest Visual Studio installation and sources `VsDevCmd.bat` for the x64 toolchain.

## Architecture

```
.pi/extensions/cmake-tools/
├── index.ts              # Extension entry point, tool registration
├── cmake.ts              # CMake execution logic, VS Env detection
├── output-filter.ts      # Output parsing and summary generation
├── types.ts              # Shared type definitions
├── package.json          # Package metadata
└── README.md             # This file
```
