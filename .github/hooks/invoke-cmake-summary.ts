// echo '{"tool_name": "bash", "tool_input": { "command": "tsx invoke-cmake-summary.ts --build build" }, "tool_use_id": "tool-123"}' | tsx invoke-cmake-summary.ts

export {};

import * as fs from 'fs';
import * as path from 'path';
import { spawnSync } from 'child_process';
import * as helpers from './helpers';

function format_timestamp(date: Date): string {
    const yyyy = date.getFullYear().toString();
    const mm = (date.getMonth() + 1).toString().padStart(2, '0');
    const dd = date.getDate().toString().padStart(2, '0');
    const hh = date.getHours().toString().padStart(2, '0');
    const min = date.getMinutes().toString().padStart(2, '0');
    const ss = date.getSeconds().toString().padStart(2, '0');
    return `${yyyy}${mm}${dd}-${hh}${min}${ss}`;
}

function get_cmake_arguments(command: string): string[] {
    const args = command.split(" ");
    if (args[0] === "tsx") {
        return args.slice(2);
    }
    return args.slice(1);
}

function detect_subcommand(command_arguments: string[]): 'build' | 'configure' | 'other' {

    for (const arg of command_arguments) {
        if (arg === '--build') {
            return 'build';
        }

        if (
            arg === '--preset' ||
            arg === '-S' ||
            arg === '-B' ||
            arg === '--fresh' ||
            arg === '-P' ||
            arg === '--install'
        ) {
            return 'configure';
        }
    }

    return 'other';
}

interface CMake_results {
    exit_code: number;
    error_lines: string[];
    warning_count: number;
}

function parse_cmake_result(result: any): CMake_results {

    const stdout = result.stdout ?? '';
    const stderr = result.stderr ?? '';
    const all_output_text = `${stdout}${stderr}`;
    fs.writeFileSync(log_file, all_output_text, 'utf8');

    const all_output_lines = all_output_text.length > 0
        ? all_output_text.split(/\r?\n/)
        : [];

    const error_lines: string[] = [];
    let warning_count = 0;
    const max_errors = 10;

    const build_error_pattern = /(?:\): (?:error|fatal error) [CE]\d+|(?:^|\s)error LNK\d+|fatal error LNK\d+|^LINK\s*:.*\berror\b|^FAILED:|ninja: build stopped)/i;
    const build_warning_pattern = /\): warning [CE]\d+/i;

    const cfg_error_pattern = /^CMake Error/i;
    const cfg_warning_pattern = /^CMake Warning/i;

    const generic_error_pattern = /\berror\b/i;

    for (const line of all_output_lines) {
        if (subcommand === 'build') {
            if (build_error_pattern.test(line) && error_lines.length < max_errors) {
                error_lines.push(line.trim());
            } else if (build_warning_pattern.test(line)) {
                warning_count += 1;
            }
        } else if (subcommand === 'configure') {
            if (cfg_error_pattern.test(line) && error_lines.length < max_errors) {
                error_lines.push(line.trim());
            } else if (cfg_warning_pattern.test(line)) {
                warning_count += 1;
            }
        }
    }

    const exit_code = result.status === null ? 1 : result.status;

    if (error_lines.length === 0 && exit_code !== 0) {
        for (const line of all_output_lines) {
            if (generic_error_pattern.test(line) && error_lines.length < max_errors) {
                error_lines.push(line.trim());
            }
        }
    }

    return {
        exit_code: exit_code,
        error_lines: error_lines,
        warning_count: warning_count
    };
}

function print_cmake_results(cmake_results: CMake_results): void {
    console.log('');
    console.log('--- CMake Summary ---');
    if (cmake_results.exit_code === 0) {
        console.log('Status:   SUCCESS');
    } else {
        console.log('Status:   FAILED');
        console.log(`Errors:   ${cmake_results.error_lines.length}`);
        if (cmake_results.error_lines.length > 0) {
            console.log('');
            for (const err_line of cmake_results.error_lines) {
                console.log(`  ${err_line}`);
            }
        }
    }

    if (cmake_results.warning_count > 0) {
        console.log(`Warnings: ${cmake_results.warning_count}`);
    }

    console.log(`Full log: ${log_file}`);
    console.log('---------------------');
}

const cmake_arguments = process.argv.slice(2);
const subcommand = detect_subcommand(cmake_arguments);

const repository_root = helpers.get_repository_root();

const log_directory = path.join(repository_root, 'build', 'logs');
fs.mkdirSync(log_directory, { recursive: true });
const timestamp = format_timestamp(new Date());
const log_file = path.join(log_directory, `cmake-${timestamp}.log`);

const cmake_result = spawnSync('cmake', cmake_arguments, {
    encoding: 'utf8',
    shell: false,
    cwd: repository_root
});

const parsed_cmake_results = parse_cmake_result(cmake_result);
print_cmake_results(parsed_cmake_results);

process.exit(parsed_cmake_results.exit_code);
