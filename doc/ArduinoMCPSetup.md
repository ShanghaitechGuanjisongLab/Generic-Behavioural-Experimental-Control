# Arduino MCP Server Integration

This repository now bundles a repeatable way to run the [arduino-mcp-server-simple](https://github.com/amahpour/arduino-mcp-server-simple) service so AI agents can compile/upload sketches and exchange serial data with the lab hardware.

## Components that get installed

| Component | Location | Notes |
| --- | --- | --- |
| Micromamba-managed Python | `.micromamba/` | Provides Python 3.11, pip, and the MCP package without touching the system interpreter. |
| Arduino CLI | `tools/arduino-cli.exe` | Downloaded directly from Arduino, configured to store board packages under `.arduino-data/`. |
| MCP entry point | Python package `arduino-mcp-tool` inside the micromamba environment. |
| Wrapper scripts | `scripts/setup-arduino-mcp.ps1`, `scripts/run-arduino-mcp.ps1` | Automate installation and server startup. |
| MCP client config | `mcp/arduino-mcp.config.json` | Example snippet for MCP-compatible IDEs. |

All binary artifacts live outside of version control (`.micromamba/`, `tools/`, `.arduino-data/`, and `.arduino-downloads/` are ignored). Re-running the setup script recreates everything from scratch.

## One-time or refresh setup

```powershell
# From the repository root
powershell -ExecutionPolicy Bypass -File .\scripts\setup-arduino-mcp.ps1
```

This script performs the following:

1. Downloads micromamba (Win64) and creates a local Python 3.11 environment.
2. Installs the Arduino MCP server package from GitHub and pins `pydantic[email]==2.11.7` to avoid a compatibility bug introduced in later releases.
3. Downloads the latest Arduino CLI build and generates `arduino-cli.yaml` that keeps all CLI data and downloads inside the repo (`.arduino-data/` and `.arduino-downloads/`).
4. Adds `tools/`, `tools/Library/bin`, and `.micromamba/Scripts` to the **current user PATH** via the Windows registry so `arduino-cli`, `micromamba`, and `arduino-mcp-tool` are callable from any terminal.
5. Leaves the repo ready for the runtime script so no global tools need to be installed.

> If you ever want to reset everything, delete `.micromamba/`, `.arduino-data/`, `.arduino-downloads/`, and `tools/`, then rerun the setup script.

## Launching the MCP server

```powershell
# From the repository root
powershell -ExecutionPolicy Bypass -File .\scripts\run-arduino-mcp.ps1
```

What the launcher does:

1. Optionally bootstraps the environment (`-SetupIfMissing`).
2. Injects `tools/` into `PATH` so the bundled `arduino-cli.exe` is discovered by the MCP server.
3. Exports `WORKSPACE` to the repository root and `ARDUINO_CLI_CONFIG_FILE` to `arduino-cli.yaml`.
4. Delegates execution to `micromamba run -p .\.micromamba arduino-mcp-tool`, which starts the FastMCP stdio transport.

Stop the server with `Ctrl+C`. Any MCP-compatible client can connect through stdio (Cursor, Claude Desktop, etc.).

## MCP client example

Point your client at `mcp/arduino-mcp.config.json` or replicate the snippet below:

```json
{
  "command": "powershell",
  "args": [
    "-ExecutionPolicy",
    "Bypass",
    "-File",
    "${workspaceFolder}\\scripts\\run-arduino-mcp.ps1",
    "-SetupIfMissing"
  ]
}
```

## Verifying the installation

1. Run `scripts/run-arduino-mcp.ps1` in one terminal; you should see the FastMCP banner.
2. In another terminal, send the handshake that your MCP client would send or simply observe that `arduino-mcp-tool` stays up without stack traces.
3. (Optional) Test Arduino CLI access: `.\tools\arduino-cli.exe board list --config-file arduino-cli.yaml`.

## Known considerations

- **Arduino CLI cores**: the CLI is installed but no board cores are downloaded yet. Use `arduino-cli core update-index` followed by `arduino-cli core install <vendor>:<arch>` as needed.
- **Serial permissions**: ensure the current Windows user can access the USB COM ports used by the lab hardware.
- **Pydantic pin**: FastMCP 2.10.6 currently raises `TypeError: cannot specify both default and default_factory` with pydantic 2.12+. The setup script pins to 2.11.7 until a patched release arrives.
- **Workspace variable**: the server respects the `WORKSPACE` env var for relative sketch paths. The run script sets it, but if you invoke the binary manually, remember to set it yourself.
- **PATH refresh**: the setup script persists the necessary directories to your user PATH. Open a new PowerShell window (or restart IDEs) after running it so the updated PATH takes effect.

## Troubleshooting

| Symptom | Fix |
| --- | --- |
| `arduino-mcp-tool` crashes complaining about `arduino-cli` | Ensure `tools/` is on `PATH` or set `ARDUINO_CLI_PATH` manually before launching. |
| `micromamba.exe` missing | Re-run the setup script or download micromamba into `tools/Library/bin/`. |
| `TypeError: cannot specify both default and default_factory` | Run `powershell -ExecutionPolicy Bypass -File .\scripts\setup-arduino-mcp.ps1` to re-pin pydantic. |
| Need to update the MCP package | Re-run the setup script; it always pulls the latest main branch. |

Enjoy automated Arduino workflows within the MCP ecosystem! 
