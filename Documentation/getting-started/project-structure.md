---
sidebar_position: 3
---

# Project Structure

Iris projects are described by two JSON configuration files: **artifact files** and **repository files**.

## Artifact File — `iris_artifact.json`

An artifact is a single build target: either a **library** or an **executable**. The artifact file lives in the root of the artifact's source directory.

### Full Field Reference

```json
{
    "name": "my_app",
    "version": "0.1.0",
    "type": "executable",
    "dependencies": [
        { "name": "my_library" }
    ],
    "sources": [
        {
            "type": "iris",
            "include": [ "./**/*.iris" ]
        }
    ],
    "executable": {
        "source": "my_app.iris"
    }
}
```

| Field | Type | Required | Description |
|---|---|---|---|
| `name` | string | ✓ | Unique artifact name used when declaring it as a dependency |
| `version` | string | ✓ | Semantic version string |
| `type` | `"executable"` \| `"library"` | ✓ | Whether to produce an executable or a linkable library |
| `dependencies` | array | — | List of other artifacts this one depends on |
| `sources` | array | ✓ | Source file groups (see below) |
| `executable.source` | string | When `type` is `"executable"` | Path to the entry-point module |

### Source Types

| `type` value | Meaning |
|---|---|
| `"iris"` | Compile the matched `.iris` files |
| `"export_c_header"` | Also generate a C header file for matched Iris modules |
| `"import_c_header"` | Imports a C header file so that it can be used with Iris |

`include` supports standard glob patterns. `"./**/*.iris"` includes all `.iris` files anywhere under the artifact root.

### Library Artifact Example

```json
{
    "name": "my_library",
    "version": "0.1.0",
    "type": "library",
    "sources": [
        {
            "type": "iris",
            "include": [ "./**/*.iris" ]
        }
    ]
}
```

---

## Repository File — `iris_repository.json`

A repository indicates the location of artifacts.

```json
{
    "name": "Link_with_library",
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

| Field | Description |
|---|---|
| `name` | Repository name |
| `artifacts` | Ordered list of artifacts; each entry names the artifact and gives the relative path to its artifact file |

---

## Typical Multi-Artifact Layout

```
my_project/
├── iris_repository.json
├── my_library/
│   ├── iris_artifact.json
│   └── lib.iris
└── my_app/
    ├── iris_artifact.json
    └── main.iris
```

`my_app` lists `my_library` as a dependency; `my_app.iris` can then `import` any exported module from `my_library`.
