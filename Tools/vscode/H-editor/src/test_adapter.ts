import { execFileSync } from 'child_process';
import * as fs from 'fs';
import * as os from 'os';
import * as path from 'path';
import * as vscode from 'vscode';
import * as crypto from 'crypto';

export interface Test_suite
{
    suite_name: string;
    test_cases: Test_case_data[];
}

export interface Test_case_data
{
    module_name: string;
    test_name: string;
    file_path: string;
    line: number;
}

interface Test_result
{
    name: string;
    status: 'passed' | 'failed' | 'skipped';
    duration_ms?: number;
    message?: string;
    stack_trace?: string;
}


export function find_tests(test_executable_file_path: string): Test_suite[]
{
    const results: Test_suite[] = [];

    if (!test_executable_file_path) {
        return results;
    }

    // create temporary json output file
    const tmpDir = os.tmpdir();
    const randomSuffix = crypto.randomBytes(8).toString('hex');
    const outFile = path.join(tmpDir, `hlang-tests-${randomSuffix}.json`);

    try {
        execFileSync(test_executable_file_path,
            ['--list-tests', `--output-format=json:${outFile}`],
            { timeout: 30000, stdio: 'pipe' });
    } catch (err) {
        const error_msg = err instanceof Error ? err.message : String(err);
        console.error(`error listing tests from ${test_executable_file_path}: ${error_msg}`);
        try { fs.unlinkSync(outFile); } catch {}
        return results;
    }

    let json: string;
    try {
        json = fs.readFileSync(outFile, 'utf8');
    } catch (err) {
        console.error(`could not read test output file at ${outFile}:`, err);
        try { fs.unlinkSync(outFile); } catch {}
        return results;
    }

    // expected JSON format:
    // {
    //   "suites": [
    //       {
    //          "name": "game",
    //          "tests": [
    //              { "name": "entry.do_something", "file": "path", "line": 551 }
    //          ]
    //       }
    //   ]
    // }

    try {
        const parsed = JSON.parse(json);
        const suites: any[] = parsed.suites || [];
        for (const s of suites) {
            const suiteName = s.name || '';
            const suite: Test_suite = { suite_name: suiteName, test_cases: [] };
            const tests: any[] = s.tests || [];
            for (const t of tests) {
                const fullName = t.name || '';
                const filePath = t.file || '';
                const lineNum = parseInt(t.line, 10) || 0;

                let moduleName = '';
                let testName = fullName;
                const lastDot = fullName.lastIndexOf('.');
                if (lastDot !== -1) {
                    moduleName = fullName.substring(0, lastDot);
                    testName = fullName.substring(lastDot + 1);
                }

                suite.test_cases.push({
                    module_name: moduleName,
                    test_name: testName,
                    file_path: filePath,
                    line: lineNum
                });
            }
            results.push(suite);
        }
    } catch (err) {
        console.error(`failed to parse test list JSON: ${err}`);
    }

    // cleanup file
    try { fs.unlinkSync(outFile); } catch {}

    return results;
}

let test_controller: vscode.TestController | undefined;
let run_profile_created = false;
const executable_map = new Map<string, string>(); // Maps root_id to executable path
// keep track of original test identifiers so we don't rely on sanitized IDs
const original_test_identifier = new WeakMap<vscode.TestItem, string>();

// Utility function to sanitize IDs for use in VS Code test items
export function sanitize_id(input: string): string {
    return input.replace(/[^a-zA-Z0-9._\-]/g, '_');
}

// Combines sanitized path with a short hash so that two different executables
// whose normalized paths only differ by characters replaced during sanitization
// still produce distinct IDs.
export function make_root_id(normalized_path: string): string {
    const sanitized = sanitize_id(normalized_path);
    const hash = crypto.createHash('sha1').update(normalized_path).digest('hex').substr(0, 8);
    return `${sanitized}_${hash}`;
}

// exported for testing purposes
export function get_executable_path(root_id: string): string | undefined {
    return executable_map.get(root_id);
}

// helpers for unit tests to manipulate internal map without invoking vscode
export function _test_add_executable(root_id: string, exePath: string): void {
    executable_map.set(root_id, exePath);
}
export function _test_remove_executable(root_id: string): void {
    executable_map.delete(root_id);
}

// Extract test identifier (suite::module::test) from test item ID
// Test IDs have format: root_id::suite_name::module_name::test_name
export function extract_test_identifier(test_id: string): { root_id: string; test_identifier: string } | null {
    const parts = test_id.split('::');
    if (parts.length < 2) {
        return null;
    }
    
    const root_id = parts[0];
    // Test identifier is everything after root_id with :: separator
    const test_identifier = parts.slice(1).join('::');
    
    return { root_id, test_identifier };
}

// Parse test results from JSON output
// Expected format:
// { "tests": [ { "name": "suite::module::test", "status": "passed|failed|skipped", "message": "...", "stack_trace": "...", "duration_ms": 123 } ] }
// OR array format:
// [ { "name": "suite::module::test", "status": "passed", ... } ]
export function parse_test_results(output: string): Map<string, Test_result> {
    const results = new Map<string, Test_result>();

    try {
        // Try to extract the first balanced JSON object or array from the output.
        // The previous regex approach could grab a stray '{...}' from log lines and
        // fail to parse.  We'll walk the string from the first '{' or '[' and keep
        // a depth counter so that we return a well-balanced substring.
        function extractBalanced(str: string): string | null {
            const start = str.search(/[\{\[]/);
            if (start === -1) {
                return null;
            }
            const openChar = str[start];
            const closeChar = openChar === '{' ? '}' : ']';
            let depth = 0;
            for (let i = start; i < str.length; ++i) {
                const ch = str[i];
                if (ch === openChar) {
                    depth++;
                } else if (ch === closeChar) {
                    depth--;
                    if (depth === 0) {
                        return str.substring(start, i + 1);
                    }
                }
            }
            return null;
        }

        const json_str = extractBalanced(output);
        if (!json_str) {
            console.warn('No JSON found in test output');
            return results;
        }

        const parsed = JSON.parse(json_str);
        const test_list: Array<any> = Array.isArray(parsed) ? parsed : (parsed.tests || []);

        for (const test of test_list) {
            if (!test.name || !test.status) {
                console.warn('Skipping malformed test result:', test);
                continue;
            }

            const result: Test_result = {
                name: test.name,
                status: test.status,
                duration_ms: test.duration_ms,
                message: test.message || '',
                stack_trace: test.stack_trace || ''
            };

            results.set(test.name, result);
        }
    } catch (err) {
        console.warn('Failed to parse test results JSON:', err);
    }

    return results;
}

export function show_tests_in_the_ui(test_executable_file_path: string, test_suites: Test_suite[]): void
{
    // Normalize path for consistent comparison
    const normalized_path = path.normalize(test_executable_file_path);

    // Create test controller if not exists
    if (!test_controller) {
        test_controller = vscode.tests.createTestController(
            'hlang-test-controller',
            'Hlang Tests'
        );
    }

    // Remove old tests for this executable if already displayed (to prevent duplicates)
    const root_id = make_root_id(normalized_path);
    {
        // Collect items to delete and delete them
        const items_to_delete: string[] = [];
        const items_iter = test_controller.items;
        (items_iter as any).forEach((item: vscode.TestItem, id: string) => {
            if (id === root_id) {
                items_to_delete.push(id);
            }
        });
        for (const id of items_to_delete) {
            test_controller.items.delete(id);
        }
    }
    // also clean up any stale executable mapping
    executable_map.delete(root_id);

    // Create a root test item for this executable
    const exec_name = path.basename(test_executable_file_path);
    const root_item = test_controller.createTestItem(root_id, exec_name);
    root_item.canResolveChildren = false;

    // Populate suites and test cases
    for (const suite of test_suites) {
        const suite_id = `${root_id}::${sanitize_id(suite.suite_name)}`;
        const suite_item = test_controller.createTestItem(
            suite_id,
            suite.suite_name
        );

        for (const test_case of suite.test_cases) {
            const sanitized_module = sanitize_id(test_case.module_name);
            const sanitized_test = sanitize_id(test_case.test_name);
            const test_id = `${suite_id}::${sanitized_module}::${sanitized_test}`;
            const test_item = test_controller.createTestItem(
                test_id,
                test_case.test_name
            );
            // store original identifier for execution/results lookup
            const original_id = `${suite.suite_name}::${test_case.module_name}::${test_case.test_name}`;
            original_test_identifier.set(test_item, original_id);

            try {
                // Set location info
                test_item.range = new vscode.Range(
                    new vscode.Position(Math.max(0, test_case.line - 1), 0),
                    new vscode.Position(test_case.line, 0)
                );
                // Uri cannot be set directly, it's read-only
                // The test location is determined by context and suite
            } catch (err) {
                // If range setup fails, just continue without location
                console.warn(`Failed to set range for test ${test_id}: ${err}`);
            }
            test_item.canResolveChildren = false;

            suite_item.children.add(test_item);
        }

        root_item.children.add(suite_item);
    }

    test_controller.items.add(root_item);

    // Store the mapping from root_id to executable path for later test execution
    executable_map.set(root_id, normalized_path);

    // Set up run handler (defined once globally, not per executable)
    if (!run_profile_created) {
        const run_handler = async (
            request: vscode.TestRunRequest,
            token: vscode.CancellationToken
        ) => {
            const run = test_controller!.createTestRun(request);

            try {
                // Collect all test items to run (leaf nodes only - actual tests, not suites)
                const items_to_run: vscode.TestItem[] = [];

                if (request.include && request.include.length > 0) {
                    // Run specific selected tests - collect leaf nodes
                    const queue = [...request.include];
                    while (queue.length > 0) {
                        const item = queue.shift()!;
                        // If item has children, add them to queue; otherwise it's a test
                        const has_children = (item.children as any).size > 0;
                        if (!has_children) {
                            items_to_run.push(item);
                        } else {
                            // It's a suite or root, add its children to queue
                            (item.children as any).forEach((child: vscode.TestItem) => {
                                queue.push(child);
                            });
                        }
                    }
                } else {
                    // Run all tests - traverse tree to find all leaf test items
                    const queue = Array.from(test_controller!.items).map(([, item]) => item);
                    while (queue.length > 0) {
                        const item = queue.shift()!;
                        const has_children = (item.children as any).size > 0;
                        if (!has_children) {
                            items_to_run.push(item);
                        } else {
                            (item.children as any).forEach((child: vscode.TestItem) => {
                                queue.push(child);
                            });
                        }
                    }
                }

                // Run each test individually
                for (const item of items_to_run) {
                    if (token.isCancellationRequested) {
                        run.skipped(item);
                        continue;
                    }

                    run.started(item);

                    // Extract test identifier and executable path
                    const orig_id = original_test_identifier.get(item);
                    let root_id: string;
                    let test_identifier: string;

                    if (orig_id) {
                        // when we have an original id store, keep sanitized root but original test name
                        const temp = extract_test_identifier((item as any).id);
                        if (!temp) {
                            run.failed(item, new vscode.TestMessage('Invalid test item ID format'));
                            continue;
                        }
                        root_id = temp.root_id;
                        test_identifier = orig_id;
                    } else {
                        const info = extract_test_identifier((item as any).id);
                        if (!info) {
                            run.failed(item, new vscode.TestMessage('Invalid test item ID format'));
                            continue;
                        }
                        root_id = info.root_id;
                        test_identifier = info.test_identifier;
                    }

                    const executable_path = executable_map.get(root_id);

                    if (!executable_path) {
                        run.failed(item, new vscode.TestMessage(
                            `Could not find executable for test (root: ${root_id})`
                        ));
                        continue;
                    }

                    try {
                        // Run the test executable with specific test filter and capture output
                        // pass the test name as a separate argument to avoid shell quoting issues
                        const stdout = execFileSync(executable_path, 
                            [`--test-name=${test_identifier}`], 
                            {
                                timeout: 30000,
                                stdio: 'pipe',
                                encoding: 'utf8'
                            }
                        );

                        // append raw output to run panel
                        if (stdout && stdout.length > 0) {
                            run.appendOutput(stdout);
                        }

                        // Parse test results from the output
                        const results = parse_test_results(stdout);
                        const test_result = results.get(test_identifier);

                        if (test_result) {
                            // Use parsed result
                            if (test_result.status === 'passed') {
                                run.passed(item, test_result.duration_ms);
                            } else if (test_result.status === 'skipped') {
                                run.skipped(item);
                            } else {
                                // failed - build detailed error message
                                let full_message = test_result.message || 'Test failed';
                                if (test_result.stack_trace) {
                                    full_message += '\n\n' + test_result.stack_trace;
                                }
                                run.failed(item, new vscode.TestMessage(full_message), test_result.duration_ms);
                            }
                        } else {
                            // No result found in output - assume passed if no exception
                            run.passed(item);
                        }
                    } catch (err) {
                        // Execution failed - try to parse results anyway
                        let error_msg = 'Test execution failed';
                        let stderrOutput: string | undefined;
                        if (err instanceof Error) {
                            error_msg = err.message;
                            if ('stdout' in err && typeof (err as any).stdout === 'string') {
                                const outstr = (err as any).stdout as string;
                                run.appendOutput(outstr);
                                const results = parse_test_results(outstr);
                                const test_result = results.get(test_identifier);
                                if (test_result && test_result.status === 'failed') {
                                    // Use the parsed failure info
                                    let message = test_result.message || error_msg;
                                    if (test_result.stack_trace) {
                                        message += '\n\n' + test_result.stack_trace;
                                    }
                                    run.failed(item, new vscode.TestMessage(message), test_result.duration_ms);
                                    continue;
                                }
                            }
                            if ('stderr' in err && typeof (err as any).stderr === 'string') {
                                stderrOutput = (err as any).stderr as string;
                                run.appendOutput(stderrOutput);
                            }
                        }
                        run.failed(item, new vscode.TestMessage(error_msg));
                    }
                }
            } finally {
                run.end();
            }
        };

        test_controller.createRunProfile(
            'Run',
            vscode.TestRunProfileKind.Run,
            run_handler,
            true
        );

        // Add a debug profile alongside the run profile
        const debug_handler = async (
            request: vscode.TestRunRequest,
            token: vscode.CancellationToken
        ) => {
            const run = test_controller!.createTestRun(request);

            // pick first leaf test item to debug
            let targetItem: vscode.TestItem | undefined;
            if (request.include && request.include.length > 0) {
                // find first leaf in the include list
                const queue = [...request.include];
                while (queue.length > 0 && !targetItem) {
                    const it = queue.shift()!;
                    const hasChildren = (it.children as any).size > 0;
                    if (hasChildren) {
                        (it.children as any).forEach((c: vscode.TestItem) => queue.push(c));
                    } else {
                        targetItem = it;
                        break;
                    }
                }
            } else {
                // get first leaf from all items
                const queue = Array.from(test_controller!.items).map(([, item]) => item);
                while (queue.length > 0 && !targetItem) {
                    const it = queue.shift()!;
                    const hasChildren = (it.children as any).size > 0;
                    if (hasChildren) {
                        (it.children as any).forEach((c: vscode.TestItem) => queue.push(c));
                    } else {
                        targetItem = it;
                        break;
                    }
                }
            }

            if (!targetItem) {
                run.end();
                return;
            }

            run.started(targetItem);
            const orig = original_test_identifier.get(targetItem);
            const orig_id = original_test_identifier.get(targetItem);
            let root_id: string;
            let test_identifier: string;
            if (orig_id) {
                const temp = extract_test_identifier((targetItem as any).id);
                if (!temp) {
                    run.failed(targetItem, new vscode.TestMessage('Invalid test item ID format'));
                    run.end();
                    return;
                }
                root_id = temp.root_id;
                test_identifier = orig_id;
            } else {
                const info = extract_test_identifier((targetItem as any).id);
                if (!info) {
                    run.failed(targetItem, new vscode.TestMessage('Invalid test item ID format'));
                    run.end();
                    return;
                }
                root_id = info.root_id;
                test_identifier = info.test_identifier;
            }

            const executable_path = executable_map.get(root_id);
            if (!executable_path) {
                run.failed(targetItem, new vscode.TestMessage(`Could not find executable for test (root: ${root_id})`));
                run.end();
                return;
            }

            const debug_config: vscode.DebugConfiguration = {
                type: 'cppvsdbg',
                request: 'launch',
                name: `Debug ${test_identifier}`,
                program: executable_path,
                args: [`--test-name=${test_identifier}`],
                cwd: '${workspaceFolder}'
            };

            try {
                await vscode.debug.startDebugging(undefined, debug_config);
                run.passed(targetItem);
            } catch (err) {
                const msg = err instanceof Error ? err.message : String(err);
                run.failed(targetItem, new vscode.TestMessage(`Debug session failed: ${msg}`));
            } finally {
                run.end();
            }
        };

        test_controller.createRunProfile(
            'Debug',
            vscode.TestRunProfileKind.Debug,
            debug_handler,
            true
        );

        run_profile_created = true;
    }
}

export function setup_test_executables_watcher(glob_pattern: string): vscode.Disposable
{
    // keep track of which executables we've already processed for this watcher
    const processed_executables = new Set<string>();

    // Helper to remove root item for a specific executable
    const remove_old_root = (exec_path: string) => {
        if (test_controller) {
            const normalized_path = path.normalize(exec_path);
            const root_id = make_root_id(normalized_path);
            test_controller.items.delete(root_id);
        }
    };

    // Helper to process an executable file
    const process_executable = (exec_path: string) => {
        const normalized_path = path.normalize(exec_path);
        if (processed_executables.has(normalized_path)) {
            // Already processed recently, skip to avoid redundant work
            return;
        }

        try {
            processed_executables.add(normalized_path);
            const suites = find_tests(normalized_path);
            show_tests_in_the_ui(normalized_path, suites);
        } catch (err) {
            console.error(`Error processing test executable ${exec_path}:`, err);
        }
    };

    // Create file system watcher for the glob pattern
    const watcher = vscode.workspace.createFileSystemWatcher(glob_pattern);

    // Handle newly created test executables
    const create_subscription = watcher.onDidCreate((uri) => {
        process_executable(uri.fsPath);
    });

    // Handle modified test executables (refresh tests)
    const change_subscription = watcher.onDidChange((uri) => {
        const normalized_path = path.normalize(uri.fsPath);
        processed_executables.delete(normalized_path);
        remove_old_root(normalized_path);
        process_executable(normalized_path);
    });

    // Handle deleted test executables (remove from UI)
    const delete_subscription = watcher.onDidDelete((uri) => {
        const normalized_path = path.normalize(uri.fsPath);
        processed_executables.delete(normalized_path);
        remove_old_root(normalized_path);
    });

    // Find all matching executables initially (and await)
    const initial_find = Promise.resolve(vscode.workspace.findFiles(glob_pattern)).then(uris => {
        for (const uri of uris) {
            process_executable(uri.fsPath);
        }
    }).catch(err => {
        console.error(`Error during initial test discovery: ${err}`);
    });

    // Return a disposable that cleans up all subscriptions and the watcher
    return vscode.Disposable.from(
        watcher,
        create_subscription,
        change_subscription,
        delete_subscription
    );
}
