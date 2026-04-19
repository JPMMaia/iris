export { };

import * as path from 'path';
import * as helpers from './helpers';

// Test with:
// echo '{"tool_name": "bash", "tool_input": { "command": "cmake --build build" }, "tool_use_id": "tool-123"}' | tsx use-cmake.ts

function is_cmake_command(command: string): boolean {
    const regexp = new RegExp('cmake(\.exe)?');
    return regexp.test(command);
}

function get_cmake_command_arguments(command: string): string[] {
    const args = command.split(" ");
    for (let index = 0; index < args.length; ++index) {
        if (args[index] === "cmake") {
            const end_index = args.findIndex(v => v === ";");
            if (end_index !== -1) {
                return args.slice(index + 1, end_index);
            }
            return args.slice(index + 1);
        }
    }

    return args;
}

function is_vs_dev_env_set(): boolean {
    return process.env.VCToolsVersion !== undefined;
}


// 1. Get input JSON
const input = helpers.get_input_json();

// 2. Check if cmake is being used. If not, then return
const command = input.tool_input.command;
if (!is_cmake_command(command)) {
    helpers.output_continue_json();
    process.exit(0);
}

let new_command = "";

// 3. Check if VsDevEnv is set. If not set, then modify command.
if (!is_vs_dev_env_set()) {
    const repository_root = helpers.get_repository_root();
    const script_path = path.resolve(repository_root, "Scripts", "Enter-VsDevEnv.ps1");
    new_command += ". " + script_path as string + " ; Enter-VsDevEnv ; ";
}

// 4. Modify command to run new python script that will create summary
const invoke_cmake_summary_script_path = path.resolve(helpers.get_repository_root(), ".github", "hooks", "invoke-cmake-summary.ts");
new_command += "tsx " + invoke_cmake_summary_script_path as string + " ";
const command_arguments = get_cmake_command_arguments(command);
new_command += command_arguments.join(" ")

// 5. Output result:
const hook_specific_output: helpers.Pre_tool_use_output_parameters = {
    hookEventName: "PreToolUse",
    permissionDecision: "allow",
    permissionDecisionReason: "",
    updatedInput: { command: new_command },
    additionalContext: ""
};

const output = {
    continue: true,
    hookSpecificOutput: hook_specific_output
};

helpers.output_json(output);
process.exit(0);
