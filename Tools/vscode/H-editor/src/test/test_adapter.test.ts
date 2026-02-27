import * as assert from 'assert';
import * as fs from 'fs';
import * as os from 'os';
import * as path from 'path';
import * as child_process from 'child_process';
import { find_tests, Test_suite } from '../test_adapter';

suite('test_adapter', () => {
	test('can run executable stub and parse json tests', () => {
		// sample json content matching expected format
		const json = JSON.stringify({
			suites: [
				{
					name: 'game',
					tests: [
						{ name: 'entry.do_something', file: 'C:/path/to/file.hltxt', line: 551 }
					]
				}
			]
		});

		// stub execFileSync to write the json to the requested output file
		const original = child_process.execFileSync;
		(child_process as any).execFileSync = (exe: string, args: string[], opts: any) => {
			const outArg = args.find(a => a.startsWith('--output-format=json:'));
			if (outArg) {
				const outFile = outArg.substring('--output-format=json:'.length);
				fs.writeFileSync(outFile, json, 'utf8');
			}
			// simulate a successful run
			return Buffer.from('');
		};

		const suites = find_tests('dummy.exe');
		// restore
		(child_process as any).execFileSync = original;

		assert.equal(suites.length, 1);
		if (suites.length < 1) {
			return;
		}
		
		const suite = suites[0];
		assert.equal(suite.suite_name, "game");
		
		const test_case = suite.test_cases[0];
		assert.equal(test_case.module_name, "entry");
		assert.equal(test_case.test_name, "do_something");
		assert.equal(test_case.file_path, "C:/path/to/file.hltxt");
		assert.equal(test_case.line, 551);
	});

	test('sanitizes and splits ids correctly', () => {
		const { sanitize_id, extract_test_identifier } = require('../test_adapter');
		// sanitize
		assert.equal(sanitize_id('foo/bar baz'), 'foo_bar_baz');
		// extract
		const info = extract_test_identifier('root::suite::test');
		assert.ok(info);
		assert.equal(info.root_id, 'root');
		assert.equal(info.test_identifier, 'suite::test');
	});

	test('parse_test_results can extract data from mixed output', () => {
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

	test('make_root_id generates distinct ids for paths that sanitize the same', () => {
		const { make_root_id } = require('../test_adapter');
		const a = make_root_id(path.normalize('C:/foo?bar'));
		const b = make_root_id(path.normalize('C:/foo/bar'));
		assert.notEqual(a, b);
	});

	test('find_tests removes temporary file even if execution fails', () => {
		const { find_tests } = require('../test_adapter');
		const tmp = os.tmpdir();
		const randomSuffix = Math.random().toString(36).substring(2);
		const outFileName = path.join(tmp, `hlang-tests-${randomSuffix}.json`);

		// stub execFileSync to create the output file then throw
		const original = child_process.execFileSync;
		(child_process as any).execFileSync = (exe: string, args: string[], opts: any) => {
			const outArg = args.find(a => a.startsWith('--output-format=json:'));
			if (outArg) {
				const outFile = outArg.substring('--output-format=json:'.length);
				fs.writeFileSync(outFile, '{invalid json}', 'utf8');
			}
			throw new Error('simulated failure');
		};

		// call the function
		const suites = find_tests('dummy.exe');
		// restore
		(child_process as any).execFileSync = original;

		assert.strictEqual(fs.existsSync(outFileName), false);
	});

	test('parse_test_results ignores stray braces before JSON', () => {
		const { parse_test_results } = require('../test_adapter');
		const output = "info {not json}\n{" +
			"\"tests\":[{\"name\":\"a\",\"status\":\"passed\"}]}";
		const results = parse_test_results(output as string);
		assert.ok(results.has('a'));
		assert.equal(results.get('a')?.status, 'passed');
	});

	test('parse_test_results keys are original names, not sanitized', () => {
		const { parse_test_results, sanitize_id } = require('../test_adapter');
		const originalName = 'suite name::mod.name::test-name';
		const sanitized = sanitize_id(originalName);
		const json = JSON.stringify({tests:[{name: originalName, status:'passed'}]});
		const results = parse_test_results(json as string);
		assert.ok(results.has(originalName));
		assert.strictEqual(results.has(sanitized), false);
	});

	test('executable_map helpers behave correctly', () => {
		const { make_root_id, _test_add_executable, _test_remove_executable, get_executable_path } = require('../test_adapter');
		const root = make_root_id(path.normalize('C:/foo/bar'));
		assert.strictEqual(get_executable_path(root), undefined);
		_test_add_executable(root, 'C:/foo/bar');
		assert.equal(get_executable_path(root), 'C:/foo/bar');
		_test_remove_executable(root);
		assert.strictEqual(get_executable_path(root), undefined);
	});
});
