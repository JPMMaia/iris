// Shared type definitions for CMake Tools extension

export interface PresetConfig {
  name: string;
  generator?: string;
  binaryDir?: string;
  cacheVariables?: Record<string, string>;
  environment?: Record<string, string>;
}

export interface CMakePresets {
  configurePresets: PresetConfig[];
}

export interface ConfigureSummary {
  cmakeVersion: string;
  generator: string;
  hostSystem: string;
  compiler: string;
  architecture: string;
  cacheVariables: Map<string, string>;
  warnings: string[];
  errors: string[];
  features: { name: string; status: string }[];
}

export interface BuildSummary {
  builtTargets: string[];
  executables: { name: string; outputPath: string }[];
  libraries: { name: string; outputPath: string }[];
  warnings: { message: string; file?: string; line?: number }[];
  errors: { message: string; file?: string; line?: number }[];
  wasIncremental: boolean;
}

export interface CMakeLog {
  action: "configure" | "build";
  timestamp: string;
  content: string;
}

export interface CMakeResult {
  success: boolean;
  summary: string;
  logFile?: string;
  configureSummary?: ConfigureSummary;
  buildSummary?: BuildSummary;
}
