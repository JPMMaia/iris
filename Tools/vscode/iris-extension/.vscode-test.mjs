import { defineConfig } from '@vscode/test-cli';
import path from 'path'

const current_directory = path.dirname(new URL(import.meta.url).pathname);

export default defineConfig(
    {
        files: 'out/test/**/*.test.js',
        workspaceFolder: "test_fixture",
        installExtensions: [],
        launchArgs: [
            "--disable-extensions"
        ],
        mocha: {
            timeout: 100000
        }
    }
);
