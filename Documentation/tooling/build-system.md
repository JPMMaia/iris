---
sidebar_position: 1
---

# Build System

The Iris build system is driven by two JSON configuration files: `iris_artifact.json` and `iris_repository.json`. The command-line entry point is the `iris` binary.

You can also define optional local defaults in `iris_presets.json` to reduce repeated CLI arguments. The VS Code language server reads the workspace root presets file too, including repository paths and artifact substitution variables.

## CLI Usage

```powershell
iris build [artifact_name] --build-directory build [--repository /path/to/repository --repository ...]
iris list --build-directory build
```

`artifact_name` is optional. If omitted, Iris discovers all `iris_artifact.json` files recursively from the current directory while skipping `build` and hidden subdirectories.

You can specify multiple repository files and these indicate where dependencies are resolved.

Supported commands that read presets are:
- `iris build`
- `iris build-tests`
- `iris test`
- `iris generate-compile-commands`
- `iris download-dependencies`
- `iris build-dependencies`

---

## `iris_presets.json` — Local Defaults

`iris_presets.json` is optional and intended to be local-only (typically not committed). The CLI looks for this file in the current working directory, and the VS Code language server looks for it in each workspace folder root.

### Merge Rules

- Explicit CLI arguments win over presets.
- For scalar values, presets apply when the CLI value remains the command default.
- For arrays (`repository_paths`, `header_search_paths`), Iris merges as `preset values + CLI values` and deduplicates normalized paths while preserving first occurrence order.

### Supported Fields

```json
{
    "build_directory": "build",
    "repository_paths": [
        "../iris_local_repository/iris_repository.json"
    ],
    "header_search_paths": [
        "C:/Program Files (x86)/Windows Kits/10/Include/10.0.22000.0/ucrt/"
    ],
    "function_contracts": "log_error_and_abort",
    "output_llvm_ir": false,
    "environment_variables": {
        "PROJECT_ROOT": "C:/src/my_project",
        "VULKAN_SDK": "C:/VulkanSDK/1.4.321.1"
    }
}
```

| Field | Type | Description |
|---|---|---|
| `build_directory` | string | Default build directory for supported commands |
| `repository_paths` | array of strings | Default repository file paths |
| `header_search_paths` | array of strings | Default C header include search paths |
| `function_contracts` | string | One of `"disabled"`, `"log_error_and_abort"` |
| `output_llvm_ir` | boolean | Default for LLVM-IR output flag |
| `environment_variables` | object of string to string | Variables usable in `iris_artifact.json` as `${NAME}` |

### Environment Variable Substitution in Artifacts

- Artifact files can reference presets variables with `${NAME}`.
- Variables are resolved only from `iris_presets.json.environment_variables`.
- Missing variables are a hard error.
- There is no fallback to process or system environment variables.

Supported substitution fields in `iris_artifact.json`:

- `public_include_directories[]`
- `executable.source`
- `copy[].source`
- `copy[].destination`
- `sources[type=import_c_header].search_paths[]`
- `sources[type=import_c_header].headers[].header`
- `sources[type=export_c_header].output_directory`
- `sources[].additional_flags[]`
- `library.external_libraries` keys and values

Not substituted:

- `sources[].include`

---

## `iris_project.json` — External Dependencies

`iris_project.json` is an optional file that declares external library dependencies. When present, it enables the `download-dependencies` and `build-dependencies` commands to automatically download and build third-party libraries.

### Example

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
        }
    ],
    "dependencies_storage_path": "external",
    "dependencies_build_path": "build_deps"
}
```

### Relationship to `iris_presets.json`

When `build-dependencies` completes, it automatically updates `iris_presets.json` with environment variables for each built dependency:

```json
{
    "environment_variables": {
        "SDL_root_path": "build_deps/SDL-2.30.0/install"
    }
}
```

These variables can then be used in `iris_artifact.json` for linking and header search paths.

### CLI Commands

| Command | Description |
|---------|-------------|
| `iris download-dependencies [--project=<file>] [--target=<name>]` | Download dependency source archives |
| `iris build-dependencies [--project=<file>] [--target=<name>]` | Build dependencies and update presets |

See [Dependencies](./dependencies.md) for the full reference.

---

## `iris_artifact.json` — Full Reference

---

## `iris_artifact.json` — Full Reference

```json
{
    "name": "my_artifact",
    "version": "0.1.0",
    "type": "executable",
    "dependencies": [
        { "name": "other_artifact" }
    ],
    "sources": [
        {
            "type": "iris",
            "include": [ "./**/*.iris" ]
        },
        {
            "type": "export_c_header",
            "include": [ "./public_api.iris" ]
        }
    ],
    "executable": {
        "source": "main.iris"
    }
}
```

### Top-Level Fields

| Field | Type | Required | Description |
|---|---|---|---|
| `name` | string | ✓ | Unique name within the repository |
| `version` | string | ✓ | Semantic version (`"major.minor.patch"`) |
| `type` | string | ✓ | `"executable"` or `"library"` |
| `dependencies` | array | — | Artifacts this one depends on (by name) |
| `sources` | array | ✓ | One or more source groups |
| `executable` | object | When `type == "executable"` | Entry-point configuration |

### Source Group Types

| `type` value | Meaning |
|---|---|
| `"iris"` | Compile all matched `.iris` files as Iris modules |
| `"export_c_header"` | Additionally emit a C header for each matched module |
| `"import_c_header"` | Parse a C header and make it importable as an Iris module |
| `"cpp"` | Compile C++ source files (linked with the artifact) |

### `include` Glob Patterns

| Pattern | Matches |
|---|---|
| `./**/*.iris` | All `.iris` files recursively under the artifact root |
| `./src/*.iris` | Only `.iris` files directly in `src/` |
| `./api.iris` | A single specific file |

### Dependency Resolution

Dependencies are resolved by name. The repository file must list all artifacts before the one that depends on them.  
`C_standard_library` is a built-in artifact that provides some C standard headers.

---

## `iris_repository.json` — Full Reference

```json
{
    "name": "my_project",
    "artifacts": [
        {
            "name": "my_library",
            "location": "my_library/iris_artifact.json"
        },
        {
            "name": "my_app",
            "location": "my_app/iris_artifact.json"
        }
    ]
}
```

| Field | Type | Required | Description |
|---|---|---|---|
| `name` | string | ✓ | Project name |
| `artifacts` | array | ✓ | Ordered list of artifact entries |
| `artifacts[].name` | string | ✓ | Must match the `name` in the referenced `iris_artifact.json` |
| `artifacts[].location` | string | ✓ | Relative path from the repository file to the artifact file |
