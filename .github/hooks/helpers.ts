import * as fs from 'fs';
import * as path from 'path';
import * as url from 'url';

export interface Tool_input {
    command: string;
}

export interface Input_parameters {
    timestamp: string;
    cwd: string;
    sessionId: string;
    hookEventName: string;
    transcript_path: string;

    tool_name: string;
    tool_input: Tool_input;
    tool_use_id: string;
}

export interface Pre_tool_use_output_parameters {
    hookEventName: string;
    permissionDecision: string;
    permissionDecisionReason: string;
    updatedInput: any;
    additionalContext: string;
}

export function get_input_json(): Input_parameters {
    const input = fs.readFileSync(0, "utf-8");
    const json = JSON.parse(input);
    return json as Input_parameters;
}

export function output_json(output: any): void {
    const json = JSON.stringify(output);
    fs.writeFileSync(1, json, "utf-8");
}

export function output_continue_json(): void {
    output_json({ "continue": true });
}

export function get_repository_root(): string {
    const this_file_path = url.fileURLToPath(import.meta.url);
    const hooks_directory = path.dirname(this_file_path);
    const repository_root = path.resolve(hooks_directory, '..', '..');
    return repository_root;
}
