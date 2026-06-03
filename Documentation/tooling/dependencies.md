# Project Dependencies

The Iris build system supports managing external library dependencies through an `iris_project.json` configuration file. This allows you to declare, download, and build external dependencies automatically.

## iris_project.json

The `iris_project.json` file declares your project's dependencies and where they should be stored and built.

### Structure

```json
{
    "name": "my_project",
    "version": "0.0.1",
    "dependencies": [
        {
            "name": "SDL",
            "version": "2.30.0",
            "source_url": "https://github.com/libsdl-org/SDL/releases/download/release-2.30.0/SDL2-2.30.0.zip",
            "build_commands": [
                "cmake -S . -B build -G \"Ninja Multi-Config\"",
                "cmake --build build --config release",
                "cmake --install build --prefix install"
            ],
            "install_path": "install"
        }
    ],
    "dependencies_storage_path": "external/source",
    "dependencies_build_path": "build_dependencies"
}
```

### Fields

#### Project-level fields

| Field | Required | Description |
|-------|----------|-------------|
| `name` | Yes | Name of your project |
| `version` | Yes | Version of your project |
| `dependencies` | No | Array of dependency objects (can be omitted or empty) |
| `dependencies_storage_path` | Yes | Directory where dependency source archives are stored (relative to project root) |
| `dependencies_build_path` | Yes | Directory where dependencies are extracted and built (relative to project root) |

#### Dependency fields

| Field | Required | Description |
|-------|----------|-------------|
| `name` | Yes | Name of the dependency (used for the environment variable, e.g., `SDL_root_path`) |
| `version` | Yes | Version string of the dependency |
| `source_url` | Yes | URL to download the dependency source archive (typically a `.zip` file) |
| `build_commands` | No | Array of shell commands to build the dependency |
| `install_path` | No | Subdirectory within the extracted archive where build artifacts are installed. Defaults to `"install"` |

## CLI Commands

### download-dependencies

Downloads all dependency source archives.

```
iris download-dependencies [--project=<project_file>] [--target=<dep_name>]
```

| Argument | Description | Default |
|----------|-------------|---------|
| `--project` | Path to `iris_project.json` | `iris_project.json` in the current directory |
| `--target` | Download only this dependency (repeatable) | All dependencies |

**Example:**

```bash
# Download all dependencies
iris download-dependencies

# Download a specific dependency
iris download-dependencies --target SDL

# Use a custom project file
iris download-dependencies --project=my_project/iris_project.json
```

### build-dependencies

Builds all dependencies. This command:

1. Searches for the source archive in `dependencies_storage_path` (errors if not found)
2. Extracts the archive to `dependencies_build_path/<name>-<version>`
3. Runs all `build_commands` in the extracted directory
4. Updates `iris_presets.json` with a `<name>_root_path` environment variable pointing to the install directory

```
iris build-dependencies [--project=<project_file>] [--target=<dep_name>]
```

| Argument | Description | Default |
|----------|-------------|---------|
| `--project` | Path to `iris_project.json` | `iris_project.json` in the current directory |
| `--target` | Build only this dependency (repeatable) | All dependencies |

**Example:**

```bash
# Build all dependencies
iris build-dependencies

# Build a specific dependency
iris build-dependencies --target SDL

# Build dependencies in order (download first, then build)
iris download-dependencies
iris build-dependencies
```

## iris_presets.json Integration

When you run `build-dependencies`, the tool automatically updates `iris_presets.json` with environment variables for each built dependency.

For a dependency named `SDL`, the variable `SDL_root_path` is added, pointing to the install directory:

```json
{
    "environment_variables": {
        "SDL_root_path": "build_dependencies/SDL-2.30.0/install"
    }
}
```

These environment variables are then available to your Iris project for linking and header search paths.

## Example Workflow

Here's a complete example of using dependencies with an Iris project:

### 1. Create iris_project.json

```json
{
    "name": "my_game",
    "version": "1.0.0",
    "dependencies": [
        {
            "name": "SDL",
            "version": "2.30.0",
            "source_url": "https://github.com/libsdl-org/SDL/releases/download/release-2.30.0/SDL2-2.30.0.zip",
            "build_commands": [
                "cmake -S . -B build -G \"Ninja Multi-Config\"",
                "cmake --build build --config release",
                "cmake --install build --prefix install"
            ],
            "install_path": "install"
        },
        {
            "name": "fmt",
            "version": "10.1.1",
            "source_url": "https://github.com/fmtlib/fmt/archive/refs/tags/10.1.1.zip",
            "build_commands": [
                "cmake -S . -B build -G \"Ninja Multi-Config\"",
                "cmake --build build --config release",
                "cmake --install build --prefix install"
            ],
            "install_path": "install"
        }
    ],
    "dependencies_storage_path": "external",
    "dependencies_build_path": "build_deps"
}
```

### 2. Download dependencies

```bash
iris download-dependencies
```

This downloads SDL and fmt source archives to `external/`.

### 3. Build dependencies

```bash
iris build-dependencies
```

This extracts and builds both dependencies, and updates `iris_presets.json`:

```json
{
    "environment_variables": {
        "SDL_root_path": "build_deps/SDL-2.30.0/install",
        "fmt_root_path": "build_deps/fmt-10.1.1/install"
    }
}
```

### 4. Use in your Iris project

In your `iris_artifact.json`, you can now reference the dependencies using the environment variables:

```json
{
    "name": "my_game",
    "sources": {
        "include": ["**/*.iris", "**/*.cpp"]
    },
    "info": {
        "executable": {
            "entry_point": "main"
        }
    },
    "external_libraries": {
        "SDL": ["SDL2"],
        "fmt": ["fmt"]
    }
}
```

## Tips for Writing build_commands

### CMake projects

Most C++ libraries use CMake. A typical build sequence is:

```json
"build_commands": [
    "cmake -S . -B build -G \"Ninja Multi-Config\"",
    "cmake --build build --config release",
    "cmake --install build --prefix install"
]
```

### Make projects

For libraries that use Make:

```json
"build_commands": [
    "make -j$(nproc)",
    "make install PREFIX=install"
]
```

### Meson projects

For libraries that use Meson:

```json
"build_commands": [
    "meson setup build",
    "meson compile -C build",
    "meson install -C build --destdir install"
]
```

## Notes

- `iris_project.json` is optional — existing projects without it continue to work
- Archives are cached: if an archive already exists in `dependencies_storage_path`, it is not re-downloaded
- Dependencies are built in the order declared in the JSON array
- The `--target` flag can be used multiple times to build specific dependencies
- Environment variable names use the dependency name as-is (e.g., `SDL` → `SDL_root_path`)
