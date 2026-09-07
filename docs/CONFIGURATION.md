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
- Toggle real-time features: Animations, Syntax Highlighting, Autosuggestions, Colored Command Outputs.
- **Live prompt preview** displaying your actual current working directory and theme changes in real time.
- Press `S` to save directly to `~/.config/aswell/`.

## Colored Outputs & Commands

Aswell provides native commands and builtins for colored output:

```bash
# Print styled text with named colors or TrueColor hex
color red "Error message"
color green --bold "Success!"
color --bg "#222222" "#00f0ff" "Cyberpunk status"

# Smooth gradients and rainbow spectrums
color gradient cyan magenta "Gradient text"
color rainbow "Rainbow text"

# Pipe command output through color
cat /var/log/syslog | color cyan

# Inspect all built-in palettes
color list

# Also available via CLI:
aswell color red "Error from script"
aswell color list
```

### Configuration Options (`~/.config/aswell/config.txt`)

```ini
colored_output=true        # Enable default color aliases (ls, grep, diff) and truecolor env
```

## Custom Username & Hostname

Customize the user and host names displayed in your prompt without altering your system login or system hostname:

```bash
# Set via CLI
aswell config username Doraemon
aswell config hostname CyberDeck

# View current settings
aswell config username
aswell config hostname

# Reset to system defaults
aswell config username default
aswell config hostname default
```

Or persist them in `~/.config/aswell/config.txt`:

```ini
username=Doraemon
hostname=CyberDeck
```

You can also override them via environment variables (`ASWELL_USER` / `ASWELL_USERNAME` and `ASWELL_HOSTNAME` / `ASWELL_HOST`), or specify inline attributes in your `~/.config/aswell/prompt.html` (e.g. `<user name="Doraemon" />`, `<hostname name="CyberDeck" />`).


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
