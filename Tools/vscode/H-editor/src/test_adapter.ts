import * as child_process from 'child_process';
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

export function parse_tests_list_json(json: any): Test_suite[] {

    // expected JSON format:
    // {
    //   "suites": [
    //       {
    //          "name": "suite_name",
    //          "tests": [
    //              { "name": "module.name.do_something", "file": "path", "line": 551 }
    //          ]
    //       }
    //   ]
    // }

    const results: Test_suite[] = [];

    const suites: any[] = json.suites || [];
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

    return results;
}

export function find_tests(test_executable_file_path: string): Test_suite[]
{
    // create temporary json output file
    const tmpDir = os.tmpdir();
    const randomSuffix = crypto.randomBytes(8).toString('hex');
    const outFile = path.join(tmpDir, `hlang-tests-${randomSuffix}.json`);

    try {
        child_process.execFileSync(test_executable_file_path, ['--list-tests', `--output-format=json:${outFile}`], { timeout: 30000, stdio: 'pipe' });
    } catch (err) {
        const error_msg = err instanceof Error ? err.message : String(err);
        console.error(`error listing tests from ${test_executable_file_path}: ${error_msg}`);
        try { fs.unlinkSync(outFile); } catch {}
        return [];
    }

    let json: string;
    try {
        json = fs.readFileSync(outFile, 'utf8');
    } catch (err) {
        console.error(`could not read test output file at ${outFile}:`, err);
        try { fs.unlinkSync(outFile); } catch {}
        return [];
    }

    let results: Test_suite[] = [];
    try {
        const parsed = JSON.parse(json);
        results = parse_tests_list_json(parsed);
    } catch (err) {
        console.error(`failed to parse test list JSON: ${err}`);
    }

    // cleanup file
    try { fs.unlinkSync(outFile); } catch {}

    return results;
}

// Utility function to sanitize IDs for use in VS Code test items
export function sanitize_id(input: string): string {
    return input.replace(/[^a-zA-Z0-9._\-/]/g, '_');
}

export function make_root_id(path: string): string {
    const normalized_path = path.normalize().replace("\\", "/");
    const sanitized = sanitize_id(normalized_path);
    return sanitized;
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

async function find_test_executables(glob_pattern: string): Promise<vscode.Uri[]> {
    const uris = await vscode.workspace.findFiles(glob_pattern);
    
    return uris.filter((uri: vscode.Uri): boolean => {
        if (uri.fsPath.endsWith(".pdb")) {
            return false;
        }
        return true;
    });
}

export function create_test_controller(id: string): vscode.TestController {
    const test_controller = vscode.tests.createTestController(
        id,
        "Hlang Tests"
    );

    const run_handler = async (request: vscode.TestRunRequest, token: vscode.CancellationToken) => {
        const run = test_controller.createTestRun(request);
        try {
            await on_run_tests(test_controller, run, request, token);
        }
        finally {
            run.end();
        }
    };

    test_controller.createRunProfile(
        'Run',
        vscode.TestRunProfileKind.Run,
        run_handler,
        true
    );

    const debug_handler = async (request: vscode.TestRunRequest, token: vscode.CancellationToken) => {
        const run = test_controller.createTestRun(request);
        try {
            await on_debug_tests(test_controller, run, request, token);
        }
        finally {
            run.end();
        }
    };

    test_controller.createRunProfile(
        'Debug',
        vscode.TestRunProfileKind.Debug,
        debug_handler,
        true
    );

    test_controller.refreshHandler = async () => {
        const tests_configuration = vscode.workspace.getConfiguration("hlang");
        const glob_pattern = tests_configuration.get<string>("test_executable_glob", "build/bin/*.hlang.test*");
        const uris = await find_test_executables(glob_pattern);
        for (const uri of uris) {
            const suites = find_tests(uri.fsPath);
            add_tests_to_controller(test_controller, uri.fsPath, suites);
        }        
    };

    return test_controller;
}

function get_leaf_test_items(items: readonly vscode.TestItem[]): vscode.TestItem[] {
    const leaf_items: vscode.TestItem[] = [];

    const queue = [...items];
     while (queue.length > 0) {
        const item = queue.shift()!;
        
        // If item has children, add them to queue; otherwise it's a test
        const has_children = (item.children as any).size > 0;
        if (!has_children) {
            leaf_items.push(item);
        } else {
            item.children.forEach((child: vscode.TestItem) => queue.push(child));
        }
    }

    return leaf_items;
}

function exclude_test_items(items: readonly vscode.TestItem[], exclude: readonly vscode.TestItem[] | undefined): readonly vscode.TestItem[] {
    if (exclude === undefined) {
        return items;
    }

    return items.filter((item: vscode.TestItem) => exclude.find((current: vscode.TestItem) => current.id === item.id) === undefined);
}

function get_items_to_run(test_controller: vscode.TestController, request: vscode.TestRunRequest): vscode.TestItem[] {
    if (request.include && request.include.length > 0) {
        const items = exclude_test_items(request.include, request.exclude);
        return get_leaf_test_items(items);
    }
    else {
        const items = Array.from(test_controller.items).map(([, item]) => item);
        const filtered_items = exclude_test_items(items, request.exclude);
        return get_leaf_test_items(filtered_items);
    }
}

interface Process_result {
    code: number | null;
    output: string;
    duration_milliseconds: number;
    user_data: any;
}

function run_process(command: string, args: string[], spawn_options: child_process.SpawnOptionsWithoutStdio, user_data: any, spawn_callback: () => void, close_callback: (error: boolean, code: number | null, duration_milliseconds: number) => void, token: vscode.CancellationToken): Promise<Process_result> {
  return new Promise<Process_result>((resolve, reject) => {

    if (token.isCancellationRequested) {
        reject(new Error("Canceled"));
    }

    const start_time = performance.now();
    const child = child_process.spawn(command, args, spawn_options);

    let output = "";

    child.on("spawn", spawn_callback);

    child.stdout.on("data", (data) => {
        output += data.toString();
    });

    child.stderr.on("data", (data) => {
        output += data.toString();
    });

    child.on("error", (err) => {
        const end_time = performance.now();
        const duration_milliseconds = end_time - start_time;
        close_callback(true, null, duration_milliseconds);
        reject(err);
    });

    child.on("close", (code) => {
        const end_time = performance.now();
        const duration_milliseconds = end_time - start_time;
        close_callback(false, code, duration_milliseconds);
        resolve({ code, output, duration_milliseconds, user_data });
    });
  });
}

function run_test(run: vscode.TestRun, item: vscode.TestItem, spawn_options: child_process.SpawnOptionsWithoutStdio, token: vscode.CancellationToken): Promise<Process_result> {
    run.started(item);

    const module_item = item.parent as vscode.TestItem;
    const executable_item = module_item.parent as vscode.TestItem;
    const executable_uri = executable_item.uri;
    if (executable_uri === undefined) {
        run.failed(item, new vscode.TestMessage("Could not find executable path corresponding to this test."));
        return Promise.reject(new Error("Could not find executable path"));
    }

    run.enqueued(item);

    const spawn_callback = (): void => {
        run.started(item);
    };

    const close_callback = (error: boolean, code: number | null, duration_milliseconds: number): void => {
        if (error) {
            run.errored(item, new vscode.TestMessage("Unexpected error while executing test."), duration_milliseconds);
        }
        else if (code === 0) {
            run.passed(item, duration_milliseconds);
        }
        else {
            run.failed(item, new vscode.TestMessage("Test failed."), duration_milliseconds);
        }
    };

    return run_process(executable_uri.fsPath, [`--test-name=${module_item.id}.${item.id}`], spawn_options, item, spawn_callback, close_callback, token);
}

async function on_run_tests(test_controller: vscode.TestController, run: vscode.TestRun, request: vscode.TestRunRequest, token: vscode.CancellationToken): Promise<void> {
    
    const items_to_run: vscode.TestItem[] = get_items_to_run(test_controller, request);

    const spawn_options: child_process.SpawnOptionsWithoutStdio = {
        timeout: 30000,
        stdio: 'pipe'
    };

    const promisses: Promise<Process_result>[] = [];

    for (const item of items_to_run) {
        if (token.isCancellationRequested) {
            await Promise.all(promisses);
            return;
        }

        const promise = run_test(run, item, spawn_options, token);
        promisses.push(promise);
    }

    const results = await Promise.all(promisses);

    for (const result of results) {
        run.appendOutput(result.output, undefined, result.user_data as vscode.TestItem);
    }

    for (const item of items_to_run) {
        run.passed(item);
    }
}

async function on_debug_tests(test_controller: vscode.TestController, run: vscode.TestRun, request: vscode.TestRunRequest, token: vscode.CancellationToken): Promise<void> {

    const items_to_run = get_items_to_run(test_controller, request);
    if (items_to_run.length === 0) {
        return;
    }

    const item = items_to_run[0];

    const module_item = item.parent as vscode.TestItem;
    const executable_item = module_item.parent as vscode.TestItem;
    const executable_uri = executable_item.uri;
    if (executable_uri === undefined) {
        run.failed(item, new vscode.TestMessage("Could not find executable path corresponding to this test."));
        return Promise.reject(new Error("Could not find executable path"));
    }

    run.started(item);
    
    const test_name = `${module_item.id}.${item.id}`;
    const debug_config: vscode.DebugConfiguration = {
        type: "cppvsdbg",
        request: "launch",
        name: `Debug ${test_name}`,
        program: executable_uri.fsPath,
        args: [`--test-name=${test_name}`],
        cwd: "${workspaceFolder}"
    };

    try {
        await vscode.debug.startDebugging(undefined, debug_config);
        run.passed(item);
    } catch (err) {
        const msg = err instanceof Error ? err.message : String(err);
        run.failed(item, new vscode.TestMessage(`Debug session failed: ${msg}`));
    }
}

export function add_tests_to_controller(test_controller: vscode.TestController, test_executable_file_path: string, test_suites: Test_suite[]): void {
    
    const executable_id = make_root_id(test_executable_file_path);
    const executable_name = path.basename(test_executable_file_path);
    const executable_item = test_controller.createTestItem(executable_id, executable_name, vscode.Uri.file(test_executable_file_path));
    test_controller.items.add(executable_item);

    for (const suite of test_suites) {
        for (const test_case of suite.test_cases) {
            const module_id = sanitize_id(test_case.module_name);

            let module_item = executable_item.children.get(module_id);
            if (module_item === undefined) {
                module_item = test_controller.createTestItem(module_id, test_case.module_name, vscode.Uri.file(test_case.file_path));
                executable_item.children.add(module_item);
            }

            const test_case_id = sanitize_id(test_case.test_name);
            const test_case_item = test_controller.createTestItem(test_case_id, test_case.test_name, vscode.Uri.file(test_case.file_path));
            test_case_item.range = new vscode.Range(test_case.line - 1, 0, test_case.line, 0);
            module_item.children.add(test_case_item);
        }
    }
}

export async function setup_test_executables_watcher(test_controller: vscode.TestController, glob_pattern: string): Promise<vscode.Disposable>
{
    // Helper to process an executable file
    const process_executable = (executable_path: string) => {
        const suites = find_tests(executable_path);
        add_tests_to_controller(test_controller, executable_path, suites);
    };

    // Create file system watcher for the glob pattern
    const watcher = vscode.workspace.createFileSystemWatcher(glob_pattern);

    // Handle newly created test executables
    const create_subscription = watcher.onDidCreate((uri) => {
        process_executable(uri.fsPath);
    });

    // Handle modified test executables (refresh tests)
    const change_subscription = watcher.onDidChange((uri) => {
        process_executable(uri.fsPath);
    });

    // Handle deleted test executables (remove from UI)
    const delete_subscription = watcher.onDidDelete((uri) => {
        const root_id = make_root_id(uri.fsPath);
        test_controller.items.delete(root_id);
    });

    // Find all matching executables initially
    const uris = await find_test_executables(glob_pattern);
    for (const uri of uris) {
        process_executable(uri.fsPath);
    }

    // Return a disposable that cleans up all subscriptions and the watcher
    return vscode.Disposable.from(
        watcher,
        create_subscription,
        change_subscription,
        delete_subscription
    );
}
