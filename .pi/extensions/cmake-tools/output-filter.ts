import type { ConfigureSummary, BuildSummary } from "./types";

// ============================================================
// Configure Output Filter
// ============================================================

export function filterConfigureOutput(
  stdout: string,
  stderr: string,
): ConfigureSummary {
  const fullOutput = [stdout, stderr].join("\n");
  const warnings: string[] = [];
  const errors: string[] = [];
  const features: { name: string; status: string }[] = [];
  const cacheVariables = new Map<string, string>();

  let cmakeVersion = "";
  let generator = "";
  let hostSystem = "";
  let compiler = "";
  let architecture = "";

  const lines = fullOutput.split("\n");

  for (const line of lines) {
    // CMake version
    const versionMatch = line.match(/CMake version:\s*(\d+\.\d+\.\d+)/i);
    if (versionMatch) cmakeVersion = versionMatch[1];

    // Generator
    const genMatch = line.match(/Selecting Windows SDK version.*to target Windows (\S+)/i);
    if (genMatch) architecture = genMatch[1];
    const genMatch2 = line.match(/-- The (C|CXX) compiler is.*\[(.+?)\]/i);
    if (genMatch2) {
      const lang = genMatch2[1];
      const path = genMatch2[2];
      const compilerName = path.split("/").pop() || path;
      // Extract version from cl output
      const versionMatch2 = path.match(/(\d+\.\d+\.\d+)/);
      if (lang === "CXX") {
        compiler = `${compilerName}${versionMatch2 ? ` ${versionMatch2[1]}` : ""}`;
      }
    }

    // Generator detection
    const genMatch3 = line.match(/-- Using: (.+?) generator/i);
    if (genMatch3) generator = genMatch3[1];
    const genMatch4 = line.match(/-- Found (Ninja|Makefiles): (.+)/i);
    if (genMatch4) generator = genMatch4[1];

    // Host system
    const hostMatch = line.match(/-- System:\s*(\S+)/i);
    if (hostMatch) hostSystem = hostMatch[1];

    // Cache variables - CMakeCache.txt entries
    const cacheVarMatch = line.match(/^>?\s*([A-Z_][A-Z0-9_]+):([A-Z_]+)=(.+)$/m);
    if (cacheVarMatch) {
      const key = cacheVarMatch[1];
      const type = cacheVarMatch[2];
      const value = cacheVarMatch[3].trim();
      const importantVars = [
        "CMAKE_BUILD_TYPE",
        "BUILD_SHARED_LIBS",
        "CMAKE_C_COMPILER",
        "CMAKE_CXX_COMPILER",
        "CMAKE_INSTALL_PREFIX",
        "CMAKE_TOOLCHAIN_FILE",
        "VCPKG_TARGET_TRIPLET",
        "CMAKE_EXPORT_COMPILE_COMMANDS",
      ];
      if (importantVars.includes(key)) {
        cacheVariables.set(key, value);
      }
    }

    // Compiler detection lines
    const compilerLine = line.match(/-- The (C|CXX) language compiler .+ is (.+)/i);
    if (compilerLine) {
      const lang = compilerLine[1];
      const version = compilerLine[2].trim();
      if (lang === "CXX") {
        compiler = version;
      }
    }

    // Architecture
    const archMatch = line.match(/-- Architecture:\s*(\S+)/i);
    if (archMatch) architecture = archMatch[1];

    // Warnings
    if (/CMake Warning/i.test(line) || /warning:/i.test(line)) {
      const trimmed = line.trim();
      if (trimmed && !trimmed.startsWith("--")) {
        warnings.push(trimmed);
      }
    }

    // Errors
    if (/CMake Error/i.test(line) || /error:/i.test(line)) {
      const trimmed = line.trim();
      if (trimmed && !trimmed.startsWith("--")) {
        errors.push(trimmed);
      }
    }

    // Features
    const featureMatch = line.match(/-- (?:Checking for working (C|CXX) compiler:.*|Detected feature:|Feature (.+?):\s*(enabled|disabled))/i);
    if (featureMatch) {
      if (featureMatch[3]) {
        features.push({ name: featureMatch[1], status: featureMatch[3] });
      }
    }
  }

  // Try to extract from stderr too
  const stderrLines = stderr.split("\n");
  for (const line of stderrLines) {
    if (!cmakeVersion) {
      const vm = line.match(/cmake version (\d+\.\d+\.\d+)/i);
      if (vm) cmakeVersion = vm[1];
    }
    if (!generator) {
      const gm = line.match(/generator[:\s]+(.+?)(?:\n|$)/i);
      if (gm) generator = gm[1].trim();
    }
  }

  return {
    cmakeVersion: cmakeVersion || "unknown",
    generator: generator || "unknown",
    hostSystem: hostSystem || "unknown",
    compiler: compiler || "unknown",
    architecture: architecture || "unknown",
    cacheVariables,
    warnings,
    errors,
    features,
  };
}

// ============================================================
// Build Output Filter
// ============================================================

export function filterBuildOutput(
  stdout: string,
  stderr: string,
): BuildSummary {
  const fullOutput = [stdout, stderr].join("\n");
  const builtTargets: string[] = [];
  const executables: { name: string; outputPath: string }[] = [];
  const libraries: { name: string; outputPath: string }[] = [];
  const warnings: { message: string; file?: string; line?: number }[] = [];
  const errors: { message: string; file?: string; line?: number }[] = [];

  const lines = fullOutput.split("\n");

  // Ninja patterns
  const ninjaBuiltTarget = /Built target\s+(\S+)/i;
  // MSBuild patterns
  const msbuildProject = /Project.*:\s*(.+?)\s*\(.*->.*\\([^\\]+\.exe|[^\\]+\.lib|[^\\]+\.dll)\)/i;
  // Clang/ GCC warning patterns
  const warningPattern = /(.+?):(\d+):\s*(warning:|note:)\s*(.*)/i;
  // Clang/ GCC error patterns
  const errorPattern = /(.+?):(\d+):\s*(error:)\s*(.*)/i;
  // Up-to-date pattern
  const upToDatePattern = /is up to date/i;

  let isIncremental = false;

  for (const line of lines) {
    // Check for incremental build
    if (upToDatePattern.test(line)) {
      isIncremental = true;
    }

    // Ninja: Built target <name>
    const ninjaMatch = line.match(ninjaBuiltTarget);
    if (ninjaMatch) {
      const targetName = ninjaMatch[1];
      if (!builtTargets.includes(targetName)) {
        builtTargets.push(targetName);
      }
      continue;
    }

    // MSBuild: Building target... or <target> -> <path>
    const msbuildTarget = /Building\s+(?:executable|library|project)\s+['"]?([^'"]+)['"]/i;
    if (msbuildTarget) {
      const targetName = msbuildTarget[1];
      if (!builtTargets.includes(targetName)) {
        builtTargets.push(targetName);
      }
    }

    // Extract output paths from MSBuild lines like:
    // iris.exe -> C:\path\to\build\Source\Debug\iris.exe
    const outputPathMatch = line.match(/->\s*(.+\.exe|.+\.lib|.+\.dll)/i);
    if (outputPathMatch) {
      const outputPath = outputPathMatch[1];
      const fileName = outputPath.split(/[/\\]/).pop() || outputPath;
      if (outputPath.endsWith(".exe")) {
        executables.push({ name: fileName, outputPath });
      } else if (outputPath.endsWith(".lib") || outputPath.endsWith(".dll")) {
        libraries.push({ name: fileName, outputPath });
      }
    }

    // Compiler warnings: file:line: warning: message
    const warnMatch = line.match(warningPattern);
    if (warnMatch) {
      warnings.push({
        file: warnMatch[1],
        line: parseInt(warnMatch[2], 10),
        message: warnMatch[4],
      });
      continue;
    }

    // Compiler errors: file:line: error: message
    const errMatch = line.match(errorPattern);
    if (errMatch) {
      errors.push({
        file: errMatch[1],
        line: parseInt(errMatch[2], 10),
        message: errMatch[4],
      });
      continue;
    }

    // CMake-level errors
    if (/CMake Error/i.test(line)) {
      errors.push({ message: line.trim() });
    }
  }

  return {
    builtTargets,
    executables,
    libraries,
    warnings,
    errors,
    wasIncremental: isIncremental,
  };
}
