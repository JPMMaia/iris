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

		if (suites.length !== 1) {
			throw new Error('expected one suite');
		}
		const suite = suites[0];
		if (suite.suite_name !== 'game') {
			throw new Error('suite name mismatch');
		}
		if (suite.test_cases.length !== 1) {
			throw new Error('expected one test case');
		}
		const tc = suite.test_cases[0];
		if (tc.module_name !== 'entry' || tc.test_name !== 'do_something') {
			throw new Error('test case name split wrong');
		}
		if (tc.file_path !== 'C:/path/to/file.hltxt' || tc.line !== 551) {
			throw new Error('file or line incorrect');
		}
	});

	test('sanitizes and splits ids correctly', () => {
		const { sanitize_id, extract_test_identifier } = require('../test_adapter');
		// sanitize
		if (sanitize_id('foo/bar baz') !== 'foo_bar_baz') {
			throw new Error('sanitize_id failed');
		}
		// extract
		const info = extract_test_identifier('root::suite::test');
		if (!info || info.root_id !== 'root' || info.test_identifier !== 'suite::test') {
			throw new Error('extract_test_identifier failed');
		}
	});

	test('parse_test_results can extract data from mixed output', () => {
		// import function locally to avoid circular dependencies
		const { parse_test_results } = require('../test_adapter');
		const output = "log line\n{" +
			"\"tests\":[{\"name\":\"suite::case\",\"status\":\"failed\",\"message\":\"oops\"}]}";
		let results = parse_test_results(output as string);
		if (!results.has('suite::case')) {
			throw new Error('missing entry');
		}
		let r = results.get('suite::case');
		if (r?.status !== 'failed' || r?.message !== 'oops') {
			throw new Error('incorrect parse result');
		}

		// now test array format
		const arrOutput = "prefix\n" + JSON.stringify([{name:'foo',status:'passed'}]);
		results = parse_test_results(arrOutput as string);
		if (!results.has('foo') || results.get('foo')?.status !== 'passed') {
			throw new Error('array format parse failed');
		}
	});

	test('make_root_id generates distinct ids for paths that sanitize the same', () => {
		const { make_root_id } = require('../test_adapter');
		const a = make_root_id(path.normalize('C:/foo?bar'));
		const b = make_root_id(path.normalize('C:/foo/bar'));
		if (a === b) {
			throw new Error('make_root_id should differentiate paths even when sanitization collides');
		}
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

		if (fs.existsSync(outFileName)) {
			throw new Error('temporary file was not removed on error');
		}
	});

	test('parse_test_results ignores stray braces before JSON', () => {
		const { parse_test_results } = require('../test_adapter');
		const output = "info {not json}\n{" +
			"\"tests\":[{\"name\":\"a\",\"status\":\"passed\"}]}";
		const results = parse_test_results(output as string);
		if (!results.has('a') || results.get('a')?.status !== 'passed') {
			throw new Error('failed to extract balanced JSON');
		}
	});

	test('parse_test_results keys are original names, not sanitized', () => {
		const { parse_test_results, sanitize_id } = require('../test_adapter');
		const originalName = 'suite name::mod.name::test-name';
		const sanitized = sanitize_id(originalName);
		const json = JSON.stringify({tests:[{name: originalName, status:'passed'}]});
		const results = parse_test_results(json as string);
		if (!results.has(originalName)) {
			throw new Error('expected original name key');
		}
		if (results.has(sanitized)) {
			throw new Error('sanitized name should not be used as key');
		}
	});

	test('executable_map helpers behave correctly', () => {
		const { make_root_id, _test_add_executable, _test_remove_executable, get_executable_path } = require('../test_adapter');
		const root = make_root_id(path.normalize('C:/foo/bar'));
		if (get_executable_path(root) !== undefined) {
			throw new Error('map should start empty');
		}
		_test_add_executable(root, 'C:/foo/bar');
		if (get_executable_path(root) !== 'C:/foo/bar') {
			throw new Error('map add failed');
		}
		_test_remove_executable(root);
		if (get_executable_path(root) !== undefined) {
			throw new Error('map remove failed');
		}
	});
});
