# Aswell Modern UI & Plugin Architecture

Aswell brings the modern web triad—**HTML (Structure)**, **CSS (Presentation & Animations)**, and **Scripting (Dynamic State, Effects & Logic)**—directly to the terminal.

It is not restricted to `PS1`. The engine provides a complete Terminal Cockpit:
- **Left Prompt (`<prompt>`)**: Multi-line or single-line prompt with glowing status glyphs.
- **Right Prompt (`<rprompt>`)**: Pushed flush against the right margin with dynamic width calculation.
- **Status Bar Dock (`<statusbar>`)**: Dedicated top/bottom dashboard displaying live metrics across the terminal.
- **Autocomplete Popup Cards**: Interactive floating menus categorizing suggestions into badges (`[CMD]`, `[DIR/]`, `[BUILT]`, `[ALIAS]`, `[FUNC]`, `[FLAG]`, `[VAR]`).
- **Command Execution Ribbon**: Post-command summary showing exit code pills (`[✓ 0]` or `[✘ 127]`), execution duration (`[⏱ 350ms]`), and timestamps.

---

## The "JS" Scripting Layer: Dynamic State & Hooks

In web engineering, JavaScript powers interactivity and state. In Aswell, you write shell functions or drop scripts into `~/.config/aswell/plugins/` to drive the UI reactively.

### Lifecycle Hooks
Plugins and scripts in `~/.config/aswell/aswellrc` can define:

| Hook Function | When Triggered | Arguments |
|:---|:---|:---|
| `aswell_on_prompt()` / `precmd()` | Before redrawing the prompt | None |
| `aswell_before_command()` / `preexec()` | Immediately before a command executes | `$1 = command_line` |
| `aswell_after_command()` / `postexec()` | Right after command execution completes | `$1 = command_line`, `$2 = duration_ms`, `$3 = exit_status` |
| `aswell_on_error()` | When any command exits with a non-zero code | `$1 = command_line`, `$2 = exit_status` |
| `aswell_on_dir_change()` / `chpwd()` | When the current working directory changes | `$1 = new_directory` |
| `aswell_on_start()` | When the shell starts | None |
| `aswell_on_exit()` | When the shell exits | `$1 = exit_status` |

### Example Hook Script (`~/.config/aswell/plugins/sys_status.sh`):
```sh
#!/bin/sh
aswell_on_prompt() {
    # Compute live load and memory
    export SYS_LOAD=$(cut -d' ' -f1 /proc/loadavg 2>/dev/null)
    total=$(awk '/MemTotal/ {print $2}' /proc/meminfo 2>/dev/null)
    avail=$(awk '/MemAvailable/ {print $2}' /proc/meminfo 2>/dev/null)
    if [ -n "$total" ] && [ -n "$avail" ] && [ "$total" -gt 0 ]; then
        export SYS_MEM="$(( (total - avail) * 100 / total ))%"
    fi
}
```

---

## Reactive DOM Binding & Conditional Directives

Any variable exported in shell scripts or lifecycle hooks is automatically bound to HTML tags and CSS classes:

### 1. `$VAR` and `${VAR}` Expansion
```html
<badge class="load">⚡ $SYS_LOAD</badge>
<badge class="mem">💾 $SYS_MEM</badge>
<badge class="k8s">☸ $KUBE_CTX</badge>
```

### 2. Conditional Directives (`show-if` and `hide-if`)
Like `v-if` in modern web frameworks:
```html
<!-- Only show error badge when last command failed -->
<badge class="err" show-if="$STATUS != 0">✘ $STATUS</badge>

<!-- Only show git badge when inside a git branch -->
<badge class="git" show-if="$GIT_BRANCH != ''">⎇ $GIT_BRANCH</badge>

<!-- Only show jobs badge when background jobs exist -->
<badge class="job" show-if="$JOBS != 0">⚙ $JOBS</badge>
```

### 3. Dynamic Class Binding
```html
<badge class="status-$STATUS">Exit: $STATUS</badge>
```

---

## Complete Cyber Cockpit Example (`prompt.html`)

```html
<prompt class="cockpit">
  <statusbar class="dock">
    <badge class="os"> 🐧 LINUX </badge>
    <badge class="load"> ⚡ CPU: $SYS_LOAD </badge>
    <badge class="mem"> 💾 MEM: $SYS_MEM </badge>
    <badge class="job" show-if="$JOBS != 0"> ⚙ $JOBS </badge>
    <badge class="err" show-if="$STATUS != 0"> ✘ $STATUS </badge>
  </statusbar>
  <newline />
  <segment class="top-line">
    <text class="bracket">╭─ </text>
    <badge class="usr">$USER</badge>
    <text class="at">@</text>
    <badge class="host">$HOSTNAME</badge>
    <text class="sep"> in </text>
    <badge class="path">$CWD</badge>
  </segment>
  <rprompt class="right">
    <badge class="git" show-if="$GIT_BRANCH != ''">⎇ $GIT_BRANCH</badge>
    <badge class="clock">🕒 $TIME</badge>
  </rprompt>
  <newline />
  <segment class="bottom-line">
    <text class="bracket">╰─</text>
    <symbol>❯</symbol>
    <text> </text>
  </segment>
</prompt>
```

---

## Security & Safe Mode

When running in safe mode (`aswell --safe-mode`), external third-party plugin scripts in `~/.config/aswell/plugins/` are skipped to protect execution integrity.
