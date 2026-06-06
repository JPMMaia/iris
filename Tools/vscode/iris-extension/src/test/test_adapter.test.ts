import * as assert from 'assert';
import * as fs from 'fs';
import * as os from 'os';
import * as path from 'path';
import * as child_process from 'child_process';
import * as vscode from "vscode";

import * as test_adapter from '../test_adapter';

suite("Test Adapter", () => {

	test("Create test controller and add test suites", () => {
		const test_suites: test_adapter.Test_suite[] = [
			{
				suite_name: "Suite_0",
				test_cases: [
					{
						module_name: "Module_0",
						test_name: "Test_0",
						file_path: "/path/to/module_0",
						line: 10
					},
					{
						module_name: "Module_0",
						test_name: "Test_1",
						file_path: "/path/to/module_0",
						line: 50
					},
					{
						module_name: "Module_1",
						test_name: "Test_0",
						file_path: "/path/to/module_1",
						line: 15
					}
				]
			},
			{
				suite_name: "Suite_1",
				test_cases: [
					{
						module_name: "Module_2",
						test_name: "Test_0",
						file_path: "/path/to/module_2",
						line: 20
					},
					{
						module_name: "Module_2",
						test_name: "Test_1",
						file_path: "/path/to/module_2",
						line: 60
					}
				]
			}
		];
		
		const test_controller = test_adapter.create_test_controller("iris.test_controller");
		test_adapter.add_tests_to_controller(test_controller, "/path/to/executable", test_suites);

		assert.equal(test_controller.items.size, 1);
		if (test_controller.items.size != 1) {
			return;
		}

		const executable_item = test_controller.items.get("/path/to/executable");
		assert.notEqual(executable_item, undefined);
		if (executable_item === undefined) {
			return;
		}

		assert.equal(executable_item.label, "executable");
		assert.equal(executable_item.children.size, 3);

		for (const suite of test_suites) {
			for (const test_case of suite.test_cases) {
				const module_id = test_case.module_name;
				
				const module_item = executable_item.children.get(module_id);
				assert.notEqual(module_item, undefined);
				if (module_item === undefined) {
					continue;
				}

				assert.equal(module_item.label, test_case.module_name);

				const test_case_id = test_case.test_name;
				const test_case_item = module_item.children.get(test_case_id);
				assert.notEqual(test_case_item, undefined);
				if (test_case_item === undefined) {
					continue;
				}

				assert.equal(test_case_item.label, test_case.test_name);
				assert.notEqual(test_case_item.uri, undefined);
				if (test_case_item.uri !== undefined) {
					assert.equal(test_case_item.uri.fsPath, vscode.Uri.file(test_case.file_path).fsPath);
				}
				assert.deepEqual(test_case_item.range, new vscode.Range(test_case.line - 1, 0, test_case.line, 0))
			}
		}
	});

	test("Parses test list json correctly", () => {
		const json = {
			suites: [
				{
					name: "suite_name",
					tests: [
						{ name: "module.name.do_something", file: "C:/path/to/file.iris", line: 551 },
						{ name: "module.name.another_thing", file: "C:/path/to/file.iris", line: 800 }
					]
				}
			]
		};

		const expected: test_adapter.Test_suite[] = [
			{
				suite_name: "suite_name",
				test_cases: [
					{
						module_name: "module.name",
						test_name: "do_something",
						file_path: "C:/path/to/file.iris",
						line: 551
					},
					{
						module_name: "module.name",
						test_name: "another_thing",
						file_path: "C:/path/to/file.iris",
						line: 800
					}
				]
			}
		];

		const test_suites = test_adapter.parse_tests_list_json(json);
		assert.deepEqual(test_suites, expected);
	});


	test("Run executable stub and parse json tests", () => {
		
		// Sample json content matching expected format
		const json = JSON.stringify({
			suites: [
				{
					name: "suite_name",
					tests: [
						{ name: "module.name.do_something", file: "C:/path/to/file.iris", line: 551 }
					]
				}
			]
		});

		// Stub execFileSync to write the json to the requested output file
		const original = child_process.execFileSync;
		(child_process as any).execFileSync = (exe: string, args: string[], opts: any) => {
			const outArg = args.find(a => a.startsWith("--output-format=json:"));
			if (outArg) {
				const outFile = outArg.substring("--output-format=json:".length);
				fs.writeFileSync(outFile, json, "utf8");
			}
			// Simulate a successful run
			return Buffer.from("");
		};

		const suites = test_adapter.find_tests('dummy.exe');
		// Restore
		(child_process as any).execFileSync = original;

		assert.equal(suites.length, 1);
		if (suites.length < 1) {
			return;
		}
		
		const suite = suites[0];
		assert.equal(suite.suite_name, "suite_name");
		
		const test_case = suite.test_cases[0];
		assert.equal(test_case.module_name, "module.name");
		assert.equal(test_case.test_name, "do_something");
		assert.equal(test_case.file_path, "C:/path/to/file.iris");
		assert.equal(test_case.line, 551);
	});

	test.skip('parse_test_results can extract data from mixed output', () => {
		// import function locally to avoid circular dependencies
		const { parse_test_results } = require('../test_adapter');
		const output = "log line\n{" +
			"\"tests\":[{\"name\":\"suite::case\",\"status\":\"failed\",\"message\":\"oops\"}]}";
		let results = parse_test_results(output as string);
		assert.ok(results.has('suite::case'));
		let r = results.get('suite::case');
		assert.equal(r?.status, 'failed');
		assert.equal(r?.message, 'oops');

		// now test array format
		const arrOutput = "prefix\n" + JSON.stringify([{name:'foo',status:'passed'}]);
		results = parse_test_results(arrOutput as string);
		assert.ok(results.has('foo'));
		assert.equal(results.get('foo')?.status, 'passed');
	});

	test.skip('parse_test_results ignores stray braces before JSON', () => {
		const { parse_test_results } = require('../test_adapter');
		const output = "info {not json}\n{" +
			"\"tests\":[{\"name\":\"a\",\"status\":\"passed\"}]}";
		const results = parse_test_results(output as string);
		assert.ok(results.has('a'));
		assert.equal(results.get('a')?.status, 'passed');
	});

	test.skip('parse_test_results keys are original names, not sanitized', () => {
		const { parse_test_results, sanitize_id } = require('../test_adapter');
		const originalName = 'suite name::mod.name::test-name';
		const sanitized = sanitize_id(originalName);
		const json = JSON.stringify({tests:[{name: originalName, status:'passed'}]});
		const results = parse_test_results(json as string);
		assert.ok(results.has(originalName));
		assert.strictEqual(results.has(sanitized), false);
	});
});
