/**
 * VS Dev Env Status Extension
 *
 * Checks if Visual Studio Developer Command Prompt environment variables
 * are set and displays the result in the footer status bar.
 *
 * Green (success)  = VS Dev Env is active
 * Red (error)      = VS Dev Env is NOT set
 *
 * Usage:
 *   Add to .pi/settings.json:
 *     "extensions": ["./.pi/extensions/vs-dev-env-status.ts"]
 *
 *   Or load via CLI:
 *     pi -e ./.pi/extensions/vs-dev-env-status.ts
 */

import type { ExtensionAPI } from "@earendil-works/pi-coding-agent";

export default function (pi: ExtensionAPI) {
  function checkVsDevEnv(ctx: Parameters<Parameters<ExtensionAPI["on"]>[1]>[1]) {
    const theme = ctx.ui.theme;
    const vc = process.env.VCINSTALLDIR;
    const vs = process.env.VSINSTALLDIR;
    const devEnv = process.env.DevEnvDir;

    const isSet = !!(vc || vs || devEnv);

    if (isSet) {
      const parts = [];
      if (vc) parts.push("VCINSTALLDIR");
      if (vs) parts.push("VSINSTALLDIR");
      if (devEnv) parts.push("DevEnvDir");
      ctx.ui.setStatus(
        "vs-dev-env",
        theme.fg("success", `✓ VS Dev Env active (${parts.join(", ")})`)
      );
    } else {
      ctx.ui.setStatus(
        "vs-dev-env",
        theme.fg("error", "✗ VS Dev Env NOT set — run vcvarsall.bat or VsDevCmd.bat first")
      );
    }
  }

  pi.on("session_start", async (_event, ctx) => {
    checkVsDevEnv(ctx);
  });

  // Also allow manual re-check via /vs-env command
  pi.registerCommand("vs-env", {
    description: "Re-check Visual Studio Dev Environment status",
    execute: async (_args, ctx) => {
      checkVsDevEnv(ctx);
      ctx.ui.notify("VS Dev Env status refreshed", "info");
    },
  });
}
