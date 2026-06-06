# Iris Programming Language

Iris is a compiled, statically-typed systems programming language designed to interoperate seamlessly with C and C++. It compiles via LLVM to native code and targets performance-sensitive applications.

## ⚠️ Early Development Status

**Iris is not ready for production use.** This is an actively-developed project with frequent breaking changes, numerous bugs, and incomplete features. The language syntax and standard library are in constant flux.

## Quick Example

```iris
module hello_world;

import c.stdio as stdio;

@unique_name("main")
export function main() -> (result: Int32)
{
    stdio.puts("Hello world!"c);
    return 0;
}
```

## Key Features

- **C/C++ Interoperability** — Call C libraries directly, export Iris as C headers
- **Low-level Memory Management** — Pointers, manual allocation, Structure of Arrays
- **Compile-time Programming** — Compile-time conditionals, reflection
- **Function Contracts** — Preconditions and postconditions for functions
- **Native Test Framework** — Built-in test support

## Known Limitations

Iris is still in early development. Key limitations include:

- **Platform:** x64 Windows only (no Linux or ARM support yet)
- **Build System:** Custom format (`iris_artifact.json`/`iris_repository.json`), no CMake integration for Iris code
- **Standard Library:** Minimal; most functionality requires C interop
- **Language Stability:** Syntax and features change frequently
- **Memory Safety:** No memory safety features yet
- **External Dependencies:** Difficult to integrate; requires manual configuration

See [Caveats in the full documentation](Documentation/index.md#caveats) for more details.

## Quick Links

| Goal | Resource |
|---|---|
| **Install & build from source** | [Getting Started → Installation](Documentation/getting-started/installation.md) |
| **Create your first program** | [Getting Started → First Project](Documentation/getting-started/first-project.md) |
| **Learn the language** | [Language Documentation](Documentation/language/) |
| **Call C libraries** | [Interoperability Guide](Documentation/interop/importing-c.md) |
| **Write generic code** | [Generics & Templates](Documentation/generics/) |
| **Explore examples** | [Examples Directory](Examples/) |
| **Full documentation** | [Documentation/index.md](Documentation/index.md) |

## Building Iris

### Prerequisites

Install the build toolchain:

| Tool | Notes |
|---|---|
| **Visual Studio 2022** | Windows only; requires **Desktop development with C++** workload |
| **CMake** ≥ 3.25 | https://cmake.org/download/ |
| **Ninja** | `winget install Ninja-build.Ninja` or bundled with Visual Studio |
| **Python** ≥ 3.10 | For build scripts |
| **Node.js & npm** | For tree-sitter grammar generation and documentation |

### Step 1: Clone Repository

```powershell
git clone https://github.com/iris-lang/iris
cd iris
```

### Step 2: Generate Tree-sitter Grammar

The parser relies on a generated tree-sitter grammar. Generate it first:

```powershell
cd Source/Parser/tree-sitter-iris
npm install
npm run generate
cd ..\..\..
```

### Step 3: Configure & Build Compiler

From the repository root, initialize the Visual Studio environment and build:

```powershell
. ./Scripts/Enter-VsDevEnv.ps1
Enter-VsDevEnv
cmake --preset windows-debug
cmake --build build
```

For a release build:

```powershell
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

### Step 4: Install Binaries

```powershell
python Scripts/build_utilities.py install_iris ../iris_install
```

Add `../iris_install/bin` to your `PATH` so the `iris` command is available globally.

### Building Additional Components

**Language Server:**

```powershell
cmake --build build --target Iris_language_server_app
```

**VSCode Extension:**

```powershell
cd Tools/vscode/iris-extension
npm install
npm run compile
```

**Documentation:**

```powershell
cd Website
npm install
npm run start
```

The documentation site will launch at http://localhost:3000.

## Project Structure

- **Source/** — Compiler implementation (Parser, Core module representation, Compiler, Language Server)
- **Tools/** — Tooling (VSCode extension, code generators)
- **Examples/** — Example programs demonstrating language features
- **Documentation/** — Main documentation
- **Website/** — Docusaurus configuration for hosting documentation
