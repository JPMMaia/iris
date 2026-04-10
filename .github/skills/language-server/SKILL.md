---
name: 'language-server'
description: 'Describes the H Language Server and Language Client. Use this skill when you need to understand or make changes to the Language Server or Language Client.'
---

There are two components: the Language Server and the Language Client.

## Language Server

The Language Server code is located in `Source/Language_server`. It uses the Language Server Protocol. We use the `leon-bckl/lsp-framework` library for this. Source code for this library is located in `build/_deps/extern_lsp_framework-build/generated/lsp` and `build/_deps/extern_lsp_framework-src/lsp`.

Components:
- `Source/Language_server/Message_handler.cpp`: Handles LSP requests and notifications.
- `Source/Language_server/Server.cpp`: Main logic. It reads the workspace for artifact and repository files that it can use to figure out dependencies between Core Modules. Parses H source files and caches them as Core Modules. Handles Text Document changes. Ties all components together.
- `Source/Language_server/Code_action.cpp`: Handles Code Action requests.
- `Source/Language_server/Completion.cpp`: Handles Completion requests.
- `Source/Language_server/Diagnostics.cpp`: Uses the Core Module Validation to create Diagnostic reports.
- `Source/Language_server/Go_to_location.cpp`: Handles Go to Location requests.
- `Source/Language_server/Inlay_hints.cpp`: Handles Inlay Hints requests.

### Build the Language Server

The Language Server is built using CMake. The CMake target is `H_language_server_app`.

## Language Client

The Language Client code is located in `Tools/vscode/H-editor`. This is a VSCode extension built using Node.js and Typescript.

The main file is `Tools/vscode/H-editor/src/extension.ts` which simply tries to connect to a Language Server (or spawn one).

### Build the Language Client

```
cd Tools/vscode/H-editor
npm run compile
```

## Tests

The Language Server is mainly tested through the Language Client tests. The test files are located in `Tools/vscode/H-editor/src/test`. The test fixture is located in `Tools/vscode/H-editor/test_fixture`.

Run the tests using PowerShell from the workspace root:

```powershell
Enter-VsDevEnv
cmake --build build --target H_language_server_app
cd Tools/vscode/H-editor
npm run compile
$env:hlang_language_server = "${pwd}/../../../build/Source/Language_server/hlang_language_server.exe"
npm run test
```

Note: do **not** set `mode=debug` when running tests. That mode makes the extension try to attach to a server already listening on port 12345 instead of spawning one, causing all tests to fail with `ECONNREFUSED`.

## Making changes to the Language Server

1. Create a new file for testing in `Tools/vscode/H-editor/test_fixture/projects/other` if needed
2. Add a new test to one of the files in `Tools/vscode/H-editor/src/test`.
3. Run the tests to confirm that the new test is failing.
4. Make the required changes to the Language Server.
5. Run the tests again and confirm that the the new test is passing. If not, go to step 4.
