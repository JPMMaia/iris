import { existsSync, mkdirSync, readdirSync, writeFileSync, unlinkSync, readFileSync } from "node:fs";
import { join } from "node:path";
import type { ExtensionAPI, ExtensionContext } from "@earendil-works/pi-coding-agent";
import type { CMakePresets, PresetConfig } from "./types";

// ============================================================
// VS Dev Environment Detection & Setup
// ============================================================

/**
 * Check if VS Developer Environment variables are set.
 * Returns true if any of VCINSTALLDIR, VCToolsVersion, or VisualStudioVersion is present.
 */
export async function checkVsDevEnv(ctx: ExtensionContext): Promise<boolean> {
  return (
    !!process.env.VCINSTALLDIR ||
    !!process.env.VCToolsVersion ||
    !!process.env.VisualStudioVersion
  );
}

// ============================================================
// CMake Preset Management
// ============================================================

/**
 * Read and parse CMakePresets.json from the project root.
 */
export function readCMakePresets(projectRoot: string): CMakePresets {
  const presetsPath = join(projectRoot, "CMakePresets.json");

  if (!existsSync(presetsPath)) {
    throw new Error(
      `CMakePresets.json not found in project root: ${projectRoot}\n` +
        "Please create a CMakePresets.json file or run cmake manually first.",
    );
  }

  const content = readFileSync(presetsPath, "utf-8");
  return JSON.parse(content) as CMakePresets;
}

/**
 * Validate that a preset exists in CMakePresets.json.
 */
export function validatePreset(
  projectRoot: string,
  presetName: string,
): boolean {
  const presets = readCMakePresets(projectRoot);
  return presets.configurePresets.some((p) => p.name === presetName);
}

/**
 * Get preset configuration details.
 */
export function getPresetConfig(
  projectRoot: string,
  presetName: string,
): PresetConfig {
  const presets = readCMakePresets(projectRoot);
  const preset = presets.configurePresets.find((p) => p.name === presetName);

  if (!preset) {
    const available = presets.configurePresets.map((p) => p.name).join(", ");
    throw new Error(
      `Preset '${presetName}' not found in CMakePresets.json.\n` +
        `Available presets: ${available}`,
    );
  }

  return preset;
}

// ============================================================
// CMake Command Execution
// ============================================================

/**
 * Execute a cmake command with VS Dev Environment setup (Windows only).
 * On Windows, runs cmake in the same pwsh invocation so env vars persist.
 */
export async function runCmakeCommand(
  pi: ExtensionAPI,
  ctx: ExtensionContext,
  args: string[],
  projectRoot: string,
): Promise<{ stdout: string; stderr: string; exitCode: number }> {
  const isWindows = process.platform === "win32";
  const cmakeArgs = args.map((a) => a.replace(/'/g, "''")).join(" ");

  if (isWindows) {
    const hasVsEnv = await checkVsDevEnv(ctx);
    if (!hasVsEnv) {
      const scriptPath = join(projectRoot, "Scripts", "Enter-VsDevEnv.ps1");
      if (!existsSync(scriptPath)) {
        throw new Error(
          `VS Dev Environment script not found: ${scriptPath}\n` +
            "Please ensure ./Scripts/Enter-VsDevEnv.ps1 exists in the project root.",
        );
      }
      // Run setup + cmake in a single pwsh invocation so env vars persist
      const escapedScript = scriptPath.replace(/'/g, "''");
      const fullCmd = `pwsh -NoProfile -Command "& '${escapedScript}'; cmake ${cmakeArgs}"`;
      const result = await pi.exec("pwsh", ["-NoProfile", "-Command", fullCmd], {
        cwd: projectRoot,
        signal: ctx.signal,
        timeout: 300000,
      });
      return {
        stdout: result.stdout,
        stderr: result.stderr,
        exitCode: result.code,
      };
    }
  }

  // No VS env setup needed (Windows with env set, or non-Windows)
  try {
    const result = await pi.exec("cmake", args, {
      cwd: projectRoot,
      signal: ctx.signal,
      timeout: 300000,
    });
    return {
      stdout: result.stdout,
      stderr: result.stderr,
      exitCode: result.code,
    };
  } catch (error: any) {
    if (error.message?.includes("command not found") || error.code === "ENOENT") {
      throw new Error(
        "cmake command not found. Please install CMake and ensure it's in your PATH.",
      );
    }
    throw error;
  }
}

// ============================================================
// Log File Management
// ============================================================

/**
 * Ensure the .pi/logs directory exists and return its path.
 */
function ensureLogsDir(projectRoot: string): string {
  const logsDir = join(projectRoot, ".pi", "logs");
  if (!existsSync(logsDir)) {
    mkdirSync(logsDir, { recursive: true });
  }
  return logsDir;
}

/**
 * Write full cmake output to a timestamped log file.
 * Keeps only the last 2 logs per action type.
 */
export function writeCMakeLog(
  projectRoot: string,
  action: "configure" | "build",
  content: string,
): string {
  const logsDir = ensureLogsDir(projectRoot);
  const timestamp = new Date().toISOString().replace(/[:.]/g, "-").slice(0, 19);
  const fileName = `cmake-${action}-${timestamp}.log`;
  const filePath = join(logsDir, fileName);

  writeFileSync(filePath, content, "utf-8");

  // Clean up old logs (keep only last 2 per action)
  cleanupOldLogs(logsDir, action);

  return filePath;
}

function cleanupOldLogs(logsDir: string, action: "configure" | "build"): void {
  try {
    const files = readdirSync(logsDir)
      .filter((f) => f.startsWith(`cmake-${action}-`) && f.endsWith(".log"))
      .sort()
      .reverse(); // newest first

    // Remove files beyond the last 2
    for (let i = 2; i < files.length; i++) {
      const oldPath = join(logsDir, files[i]);
      unlinkSync(oldPath);
    }
  } catch {
    // Silently ignore cleanup errors
  }
}
