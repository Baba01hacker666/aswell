# Aswell Plugin Architecture

Aswell features a secure lifecycle hook system for modular extensions.

## Plugin Lifecycle Hooks

Plugins can listen to the following shell events:

- `ON_START`: Triggered on shell startup.
- `ON_PROMPT`: Triggered before redrawing the interactive prompt.
- `BEFORE_COMMAND`: Invoked immediately before a command executes, receiving the command line string.
- `AFTER_COMMAND`: Invoked after command completion with `[command_line, duration_ms, exit_status]`.
- `ON_ERROR`: Triggered when any command exits with a non-zero exit code.
- `ON_DIR_CHANGE`: Triggered when the current directory is changed via `cd`.
- `ON_EXIT`: Invoked when the shell exits.

## Built-in Plugins

- **`git_info`**: Inspects Git repository state and updates branch info.
- **`timer`**: Records execution duration of commands.
- **`sys_info`**: Tracks system load and memory.

## Security & Safe Mode

When running in safe mode (`aswell --safe-mode`), external third-party plugin scripts are disabled to ensure execution integrity.
