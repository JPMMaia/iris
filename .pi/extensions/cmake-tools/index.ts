import type { ExtensionAPI } from "@earendil-works/pi-coding-agent";
import { Type } from "typebox";
import { Text } from "@earendil-works/pi-tui";
import {
  filterConfigureOutput,
  filterBuildOutput,
} from "./output-filter";
import {
  validatePreset,
  runCmakeCommand,
  writeCMakeLog,
} from "./cmake";
import type { CMakeResult } from "./types";

// ============================================================
// Helper: Format cache variables for display
// ============================================================

function formatCacheVariables(vars: Map<string, string>): string {
  if (vars.size === 0) return "";

  const lines: string[] = [];
  const keyLabels: Record<string, string> = {
    CMAKE_BUILD_TYPE: "Build Type",
    BUILD_SHARED_LIBS: "Shared Libs",
    CMAKE_C_COMPILER: "C Compiler",
    CMAKE_CXX_COMPILER: "CXX Compiler",
    CMAKE_INSTALL_PREFIX: "Install Prefix",
    CMAKE_TOOLCHAIN_FILE: "Toolchain",
    VCPKG_TARGET_TRIPLET: "Vcpkg Triplet",
    CMAKE_EXPORT_COMPILE_COMMANDS: "Compile Commands",
  };

  for (const [key, value] of vars) {
    const label = keyLabels[key] || key;
    const displayValue = value.length > 60 ? `${value.slice(0, 57)}...` : value;
    lines.push(`    ${label}: ${displayValue}`);
  }

  return lines.join("\n");
}

// ============================================================
// Helper: Build result summary string
// ============================================================

function buildConfigureSummary(result: CMakeResult, summary: CMakeResult["configureSummary"]): string {
  const preset = (result as any).preset as string | undefined;
  const config = summary;

  if (!config) {
    return result.summary;
  }

  let text = `✓ Configure complete (preset: ${preset || "windows"})\n`;
  text += `  Generator:    ${config.generator}\n`;
  text += `  Compiler:     ${config.compiler} (${config.architecture})\n`;

  const buildType = config.cacheVariables.get("CMAKE_BUILD_TYPE");
  if (buildType) {
    text += `  Build Type:   ${buildType}\n`;
  }

  const sharedLibs = config.cacheVariables.get("BUILD_SHARED_LIBS");
  if (sharedLibs) {
    text += `  Shared Libs:  ${sharedLibs}\n`;
  }

  const cacheText = formatCacheVariables(config.cacheVariables);
  if (cacheText) {
    text += cacheText;
  }

  if (config.warnings.length > 0) {
    text += `\n⚠ ${config.warnings.length} warning(s):\n`;
    for (const w of config.warnings.slice(0, 5)) {
      text += `  - ${w}\n`;
    }
    if (config.warnings.length > 5) {
      text += `  - ... and ${config.warnings.length - 5} more\n`;
    }
  }

  if (config.errors.length > 0) {
    text += `\n✗ ${config.errors.length} error(s):\n`;
    for (const e of config.errors.slice(0, 5)) {
      text += `  - ${e}\n`;
    }
    if (config.errors.length > 5) {
      text += `  - ... and ${config.errors.length - 5} more\n`;
    }
  }

  if (result.logFile) {
    text += `\n📄 Full log: ${result.logFile}`;
  }

  return text;
}

function buildBuildSummary(result: CMakeResult, summary: CMakeResult["buildSummary"]): string {
  const target = (result as any).target as string | undefined;
  const build = summary;

  if (!build) {
    return result.summary;
  }

  if (build.wasIncremental) {
    return `✓ Nothing to build (all targets up-to-date)\n${result.logFile ? `\n📄 Full log: ${result.logFile}` : ""}`;
  }

  if (build.errors.length > 0) {
    let text = `✗ Build failed (target: ${target || "all"})\n`;
    text += `  Errors:\n`;
    for (const e of build.errors.slice(0, 5)) {
      const context = e.file ? `${e.file}:${e.line ?? "?"}: ` : "";
      text += `  - ${context}${e.message}\n`;
    }
    if (build.errors.length > 5) {
      text += `  - ... and ${build.errors.length - 5} more\n`;
    }

    if (build.warnings.length > 0) {
      text += `\n⚠ ${build.warnings.length} warning(s):\n`;
      for (const w of build.warnings.slice(0, 3)) {
        const context = w.file ? `${w.file}:${w.line ?? "?"}: ` : "";
        text += `  - ${context}${w.message}\n`;
      }
    }

    text += `\n📄 Full log: ${result.logFile}`;
    return text;
  }

  // Success
  let text = `✓ Build complete (target: ${target || "all"})\n`;

  if (build.builtTargets.length > 0) {
    text += `  Built targets (${build.builtTargets.length}):\n`;
    for (const t of build.builtTargets.slice(0, 10)) {
      text += `    - ${t}\n`;
    }
    if (build.builtTargets.length > 10) {
      text += `    - ... and ${build.builtTargets.length - 10} more\n`;
    }
  }

  if (build.executables.length > 0) {
    text += `  Executables:\n`;
    for (const e of build.executables.slice(0, 5)) {
      text += `    - ${e.outputPath}\n`;
    }
    if (build.executables.length > 5) {
      text += `    - ... and ${build.executables.length - 5} more\n`;
    }
  }

  if (build.libraries.length > 0) {
    text += `  Libraries:\n`;
    for (const l of build.libraries.slice(0, 5)) {
      text += `    - ${l.outputPath}\n`;
    }
    if (build.libraries.length > 5) {
      text += `    - ... and ${build.libraries.length - 5} more\n`;
    }
  }

  if (build.warnings.length > 0) {
    text += `\n⚠ ${build.warnings.length} warning(s):\n`;
    for (const w of build.warnings.slice(0, 3)) {
      const context = w.file ? `${w.file}:${w.line ?? "?"}: ` : "";
      text += `  - ${context}${w.message}\n`;
    }
    if (build.warnings.length > 3) {
      text += `  - ... and ${build.warnings.length - 3} more\n`;
    }
  }

  text += `\n📄 Full log: ${result.logFile}`;
  return text;
}

// ============================================================
// CMake Configure Tool
// ============================================================

function registerCMakeConfigure(pi: ExtensionAPI, projectRoot: string): void {
  pi.registerTool({
    name: "cmake_configure",
    label: "CMake Configure",
    description:
      "Configure the CMake project using a preset. Validates preset existence, " +
      "sets up VS Developer Environment on Windows, and provides a filtered summary " +
      "of compiler detection, cache variables, warnings, and errors.",
    promptSnippet: "Configure CMake project with a preset",
    promptGuidelines: [
      "Use cmake_configure to configure the CMake project before building.",
      "cmake_configure validates the preset exists in CMakePresets.json before running.",
      "Default preset is 'windows'. Specify a different preset if needed.",
    ],
    parameters: Type.Object({
      preset: Type.Optional(
        Type.String({
          description:
            "CMake preset name (default: 'windows'). Must exist in CMakePresets.json",
          default: "windows",
        }),
      ),
      verbose: Type.Optional(
        Type.Boolean({
          description: "Pass --debug-output to cmake for verbose output",
          default: false,
        }),
      ),
    }),

    renderCall(args, theme, context) {
      const preset = args.preset || "windows";
      const verbose = args.verbose ? " (verbose)" : "";
      let text = theme.fg("toolTitle", theme.bold("cmake_configure"));
      text += theme.fg("accent", ` --preset ${preset}`);
      if (verbose) {
        text += theme.fg("dim", verbose);
      }
      return new Text(text, 0, 0);
    },

    renderResult(result, { expanded, isPartial }, theme, context) {
      if (isPartial) {
        return new Text(theme.fg("warning", "Configuring CMake..."), 0, 0);
      }

      const content = result.content[0];
      if (content?.type !== "text") {
        return new Text(theme.fg("error", "No content"), 0, 0);
      }

      const isError = result.isError;
      const prefix = isError ? theme.fg("error", "✗ ") : theme.fg("success", "✓ ");

      if (expanded) {
        return new Text(prefix + content.text, 0, 0);
      }

      // Compact view: show first few lines
      const lines = content.text.split("\n").slice(0, 4);
      let text = prefix + lines.join("\n");
      const extraLines = content.text.split("\n").length - 4;
      if (extraLines > 0) {
        text += `\n${theme.fg("muted", `... ${extraLines} more lines`)}`;
      }
      return new Text(text, 0, 0);
    },

    async execute(_toolCallId, params, signal, onUpdate, ctx) {
      const preset = params.preset || "windows";
      const verbose = params.verbose || false;

      // Validate preset
      if (!validatePreset(projectRoot, preset)) {
        throw new Error(
          `Preset '${preset}' not found in CMakePresets.json.\n` +
            `Run 'cmake --list-presets' to see available presets.`,
        );
      }

      // Stream progress
      onUpdate?.({
        content: [{ type: "text", text: `Configuring with preset: ${preset}...` }],
        details: { status: "validating" },
      });

      // Build cmake args
      const cmakeArgs = ["--preset", preset];
      if (verbose) {
        cmakeArgs.push("--debug-output");
      }

      // Execute cmake configure
      const result = await runCmakeCommand(pi, ctx, cmakeArgs, projectRoot);

      // Check for errors
      if (result.exitCode !== 0) {
        // Save full log
        const logFile = writeCMakeLog(projectRoot, "configure", result.stdout + result.stderr);

        // Filter output for summary
        const summary = filterConfigureOutput(result.stdout, result.stderr);

        const errorSummary = `Configure failed (preset: ${preset})\n` +
          `Generator: ${summary.generator}\n` +
          `Compiler: ${summary.compiler}\n` +
          `Errors: ${summary.errors.length}\n` +
          `Warnings: ${summary.warnings.length}`;

        return {
          content: [{ type: "text", errorSummary }],
          details: {
            preset,
            logFile,
            configureSummary: summary,
            exitCode: result.exitCode,
          },
        };
      }

      // Save full log
      const logFile = writeCMakeLog(projectRoot, "configure", result.stdout + result.stderr);

      // Filter output for summary
      const summary = filterConfigureOutput(result.stdout, result.stderr);

      // Build user-friendly summary
      const userSummary = buildConfigureSummary(
        { success: true, summary: "", logFile } as CMakeResult,
        summary,
      );

      return {
        content: [{ type: "text", text: userSummary }],
        details: {
          preset,
          logFile,
          configureSummary: summary,
          exitCode: result.exitCode,
        },
      };
    },
  });
}

// ============================================================
// CMake Build Tool
// ============================================================

function registerCMakeBuild(pi: ExtensionAPI, projectRoot: string): void {
  pi.registerTool({
    name: "cmake_build",
    label: "CMake Build",
    description:
      "Build the CMake project. Supports building all targets or a specific target. " +
      "Detects VS Developer Environment on Windows and sets it up if needed. " +
      "Provides a filtered summary of built targets, output paths, warnings, and errors.",
    promptSnippet: "Build CMake project (all targets or specific target)",
    promptGuidelines: [
      "Use cmake_build to compile the project after configuring with cmake_configure.",
      "cmake_build builds the 'build' directory. Omit target to build all targets.",
      "Use cmake_build with a specific target to build only that target.",
    ],
    parameters: Type.Object({
      target: Type.Optional(
        Type.String({
          description:
            "Specific target to build. Omit to build all targets",
        }),
      ),
      verbose: Type.Optional(
        Type.Boolean({
          description: "Pass --verbose to cmake --build",
          default: false,
        }),
      ),
    }),

    renderCall(args, theme, context) {
      const target = args.target;
      let text = theme.fg("toolTitle", theme.bold("cmake_build"));
      if (target) {
        text += theme.fg("accent", ` --target ${target}`);
      }
      if (args.verbose) {
        text += theme.fg("dim", " (verbose)");
      }
      return new Text(text, 0, 0);
    },

    renderResult(result, { expanded, isPartial }, theme, context) {
      if (isPartial) {
        return new Text(theme.fg("warning", "Building..."), 0, 0);
      }

      const content = result.content[0];
      if (content?.type !== "text") {
        return new Text(theme.fg("error", "No content"), 0, 0);
      }

      const isError = result.isError;
      const prefix = isError ? theme.fg("error", "✗ ") : theme.fg("success", "✓ ");

      if (expanded) {
        return new Text(prefix + content.text, 0, 0);
      }

      // Compact view: show first few lines
      const lines = content.text.split("\n").slice(0, 4);
      let text = prefix + lines.join("\n");
      const extraLines = content.text.split("\n").length - 4;
      if (extraLines > 0) {
        text += `\n${theme.fg("muted", `... ${extraLines} more lines`)}`;
      }
      return new Text(text, 0, 0);
    },

    async execute(_toolCallId, params, signal, onUpdate, ctx) {
      const target = params.target;
      const verbose = params.verbose || false;

      // Stream progress
      const targetStr = target ? ` --target ${target}` : "";
      onUpdate?.({
        content: [{ type: "text", text: `Building${targetStr}...` }],
        details: { status: "starting" },
      });

      // Build cmake args
      const buildDir = "build";
      const cmakeArgs = ["--build", buildDir];
      if (target) {
        cmakeArgs.push("--target", target);
      }
      if (verbose) {
        cmakeArgs.push("--verbose");
      }

      // Execute cmake build
      const result = await runCmakeCommand(pi, ctx, cmakeArgs, projectRoot);

      // Save full log
      const logFile = writeCMakeLog(projectRoot, "build", result.stdout + result.stderr);

      // Filter output for summary
      const summary = filterBuildOutput(result.stdout, result.stderr);

      // Check for errors
      if (result.exitCode !== 0 || summary.errors.length > 0) {
        const userSummary = buildBuildSummary(
          { success: false, summary: "", logFile, buildSummary: summary } as CMakeResult,
          summary,
        );

        return {
          content: [{ type: "text", text: userSummary }],
          details: {
            target,
            buildDir,
            logFile,
            buildSummary: summary,
            exitCode: result.exitCode,
          },
        };
      }

      // Success
      const userSummary = buildBuildSummary(
        { success: true, summary: "", logFile, buildSummary: summary } as CMakeResult,
        summary,
      );

      return {
        content: [{ type: "text", text: userSummary }],
        details: {
          target,
          buildDir,
          logFile,
          buildSummary: summary,
          exitCode: result.exitCode,
        },
      };
    },
  });
}

// ============================================================
// Extension Entry Point
// ============================================================

export default function (pi: ExtensionAPI) {
  // Determine project root (cwd when extension loads)
  let projectRoot = process.cwd();

  pi.on("session_start", async (_event, ctx) => {
    projectRoot = ctx.cwd;
  });

  // Register tools
  registerCMakeConfigure(pi, projectRoot);
  registerCMakeBuild(pi, projectRoot);

  // Update project root on session changes
  pi.on("session_tree", async (_event, ctx) => {
    projectRoot = ctx.cwd;
  });
}
