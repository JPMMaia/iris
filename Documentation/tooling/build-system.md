---
sidebar_position: 1
---

# Build System

The Iris build system is driven by two JSON configuration files: `iris_artifact.json` and `iris_repository.json`. The command-line entry point is the `iris` binary.

## CLI Usage

```powershell
iris build-artifact --artifact-file path/to/iris_artifact.json --build-directory build [--repository /path/to/repository --repository ...]
```

You can specify multiple repository files and these indicate where the dependencies specified in `iris_artifact.json` are located.

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
