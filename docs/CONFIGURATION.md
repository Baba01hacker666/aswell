# Aswell Configuration Guide

Aswell provides an intuitive, modular configuration model combining declarative HTML-like prompt structures and CSS styling.

## Configuration Directory Structure

Aswell stores user configurations in `~/.config/aswell/`:

```
~/.config/aswell/
├── config.txt         # Core shell preferences & component flags
├── aswellrc           # Shell startup commands, aliases, functions
├── themes/            # Custom themes (.css files)
└── plugins/           # Custom plugins (.sh files)
```

## Interactive Configuration Editor

Launch the interactive visual customization editor at any time:

```bash
aswell config
```

Features:
- Toggle prompt components: Username, Hostname, Directory, Git status, Execution duration, Active jobs, Exit status.
- Switch between themes (`modern`, `cyberpunk`, `nord`, `minimal`, `dracula`, `powerline`).
- Toggle real-time features: Animations, Syntax Highlighting, Autosuggestions.
- **Live prompt preview** displaying your actual current working directory and theme changes in real time.
- Press `S` to save directly to `~/.config/aswell/`.

## CLI Theme Commands

Quickly inspect or change themes directly from your terminal:

```bash
aswell theme list
aswell theme set cyberpunk
aswell theme set nord
aswell theme set modern
```

## Startup Commands (`~/.config/aswell/aswellrc`)

Add custom aliases, environment variables, or shell functions:

```bash
# Aliases
alias ll="ls -la"
alias g="git"
alias gs="git status"

# Environment
export EDITOR="vim"

# Custom prompt function
mkcd() {
    mkdir -p "$1" && cd "$1"
}
```
