---
name: 'language-server'
description: 'Describes the Iris Language Server and Language Client. Use this skill when you need to understand or make changes to the Language Server or Language Client.'
---

There are two components: the Language Server and the Language Client.

## Language Server

The Language Server code is located in `Source/Language_server`. It uses the Language Server Protocol. We use the `leon-bckl/lsp-framework` library for this. Source code for this library is located in `build/_deps/extern_lsp_framework-build/generated/lsp` and `build/_deps/extern_lsp_framework-src/lsp`.

Components:
- `Source/Language_server/Message_handler.cpp`: Handles LSP requests and notifications.
- `Source/Language_server/Server.cpp`: Main logic. It reads the workspace for artifact and repository files that it can use to figure out dependencies between Core Modules. Parses Iris source files and caches them as Core Modules. Handles Text Document changes. Ties all components together.
  - Workspace discovery lives in `rebuild_workspaces_data`, which is run once from
    `set_workspace_folder_configurations` and again whenever a watched file event changes the set
    of sources. It re-creates the `Declaration_database` from scratch, because that database holds
    raw pointers into the `iris::Module` objects and cannot be patched in place. Parse trees and
    versions of open documents are carried over, since a rebuild otherwise re-reads from disk and
    `text_document_did_change` applies *incremental* edits against the stored tree.
  - The `Server` struct owns parallel vectors indexed by `core_module_index`; every lookup goes
    through `find_workspace_core_module_index`, which returns nothing for a file the workspace does
    not know about, in which case each feature silently no-ops.

### Comparing paths and uris

Never compare a `lsp::DocumentUri` or a `std::filesystem::path` with `==`. A path spelled by the
client (`c:/foo/bar.iris`) and the same path produced by expanding an artifact glob
(`C:\foo\bar.iris`) denote one file but compare unequal, and the symptom is silent: the lookup
fails and the feature does nothing at all.

Use `compare_document_uris` from `iris.language_server.core` (`Source/Language_server/Core.cpp`),
which ignores case and separator differences. `Server.cpp` wraps it in `are_file_paths_equal` for
path-to-path comparisons, and `Diagnostics.cpp` converts with `lsp::DocumentUri::fromPath` before
comparing.

When writing tests, note that `lsp::DocumentUri::fromPath` runs `std::filesystem::canonical` on a
file that exists (`fileuri.cpp`), which quietly restores the real drive-letter case. A test that
builds a differently cased uri that way proves nothing. Parse the uri instead, the way one
arriving over the wire is handled:

```cpp
lsp::DocumentUri const uri = lsp::DocumentUri{lsp::Uri::parse("file:///c:/foo/bar.iris")};
```

Also note `lsp::Uri::path()` keeps the leading `/` while `lsp::FileUri::path()` strips it on
Windows, so `to_filesystem_path` taking a `lsp::Uri const&` is deliberate.
- `Source/Language_server/Code_action.cpp`: Handles Code Action requests.
- `Source/Language_server/Completion.cpp`: Handles Completion requests.
- `Source/Language_server/Diagnostics.cpp`: Uses the Core Module Validation to create Diagnostic reports.
- `Source/Language_server/Go_to_location.cpp`: Handles Go to Location requests.
- `Source/Language_server/Inlay_hints.cpp`: Handles Inlay Hints requests.

### Build the Language Server

The Language Server is built using CMake. The CMake target is `Iris_language_server_app`.

## Language Client

The Language Client code is located in `Tools/vscode/iris-extension`. This is a VSCode extension built using Node.js and Typescript.

The main file is `Tools/vscode/iris-extension/src/extension.ts` which simply tries to connect to a Language Server (or spawn one).

### Build the Language Client

```
cd Tools/vscode/iris-extension
npm run compile
```

## Tests

There are two test suites.

### Catch2 tests (fast)

`Source/Language_server/Server.tests.cpp` and `Signature_help.tests.cpp` call the exported
`Server` functions directly, with no editor involved. They build real workspaces in a temporary
directory (see `write_two_module_workspace` and `create_configured_server`) and drive the server
through helpers such as `open_document`, `change_document_full_text` and
`pull_document_diagnostics`.

Prefer these whenever the behaviour can be reached through a `Server` function: they run in
seconds and are not flaky. Note that `Workspace_data` is not exported, so tests reach into
`server.workspaces_data` via `auto`.

```powershell
cmake --build build --target Iris_language_server_tests
./build/bin/Debug/Iris_language_server_tests.exe "[Language_server]"
```

### Language Client tests (end-to-end)

Use these when the behaviour depends on the client, such as file watching or the editor's own
notifications. The test files are located in `Tools/vscode/iris-extension/src/test` and are
discovered automatically by filename (`out/test/**/*.test.js`). The test fixture is located in
`Tools/vscode/iris-extension/test_fixture`.

Build the server first, then run the extension tests against it:

```powershell
cmake --build build --target Iris_language_server_app

# The extension spawns the server from this environment variable.
$env:iris_language_server = "$PWD/build/bin/Debug/iris_language_server.exe"
cd Tools/vscode/iris-extension
npm run test
```

`CMAKE_RUNTIME_OUTPUT_DIRECTORY` is set to `<build>/bin` in the top-level `CMakeLists.txt`, so
the executable is at `build/bin/<Config>/iris_language_server.exe` with the multi-config
`windows` preset and at `build/bin/iris_language_server` with the single-config presets.

To run a single test, pass a pattern through to the runner:

```powershell
npx vscode-test --grep "Diagnoses parsing error"
```

Do not use `python ./Scripts/build_utilities.py test_language_server`. It is currently broken on
Windows in two independent ways: it hardcodes `build/Source/Language_server/iris_language_server.exe`,
which no preset produces (hence the `TODO find executable` on that line), and even with the path
corrected it crashes printing the test output, because mocha's `✔` cannot be encoded in the
console's cp1252 codec.

### Before every run: kill stray servers

Always check for a leftover `iris_language_server` process first:

```powershell
Get-CimInstance Win32_Process | Where-Object { $_.Name -eq "iris_language_server.exe" } |
    ForEach-Object { Stop-Process -Id $_.ProcessId -Force }
```

The server binds port 12345 in `process_messages` *before* printing `Listening...`, and the
extension waits for that line in a poll loop with no timeout. So a leftover server does not
produce a useful error: VS Code opens and hangs with no tests running. A stale instance can also
surface as `Error: mutex already exists`.

### Debugging the Language Server

It might be useful to print the Parser tree and nodes.

To print a iris::parser::Parse_tree:

```
std::string const debug_string = iris::parser::print_tree(parse_tree);
write_debug_message(debug_string);
```

To print a iris::parser::Parse_node:

```
std::string const debug_string = iris::parser::print_node(parse_tree, node);
write_debug_message(debug_string);
```

You can also print any messages using `write_debug_message()`. The output will be located in `build/logs/debug-language-server.log`.

Make sure that `iris.language_server.debug` is imported.

## Making changes to the Language Server

1. Add a test. Prefer a Catch2 test in `Source/Language_server/Server.tests.cpp` if the behaviour
   is reachable through an exported `Server` function; only reach for a Language Client test in
   `Tools/vscode/iris-extension/src/test` when the client itself is part of what you are testing.
   Create a fixture file in `Tools/vscode/iris-extension/test_fixture/projects/other` if needed —
   that artifact includes `./**/*.iris`, so a new file there is picked up automatically.
2. Run the tests to confirm that the new test is failing.
3. Make the required changes to the Language Server.
4. Run the tests again and confirm that the new test is passing. If not, go to step 3.
5. Confirm the test fails for the right reason: once it passes, temporarily undo the change and
   check that it fails again. A test that would pass without the change is not testing anything.

A Language Client test that writes into the fixture must delete the file in `teardown`, not at the
end of the test, so that a failed assertion cannot leak it into the checked-in fixture.
