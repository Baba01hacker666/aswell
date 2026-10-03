# Aswell Configuration Guide

Aswell provides an intuitive, modular configuration model combining declarative HTML-like prompt structures and CSS styling.

## Configuration Directory Structure

Aswell stores user configurations in `~/.config/aswell/`:

```
~/.config/aswell/
├── config.txt         # All shell settings (documented by `aswell config help`)
├── aswellrc           # Shell startup commands, aliases, functions
├── prompt.html        # Optional: declarative prompt layout
├── theme.css          # Optional: legacy single-file stylesheet override
├── themes/            # Custom themes (<name>.css)
├── templates/         # Declarative HTML/CSS event animations
├── plugins/           # Custom plugins (.sh files)
└── commands/          # Custom commands, put on $PATH automatically
```

`aswell config path` prints the active directory; set `ASWELL_CONFIG_DIR`
(or `aswell --config PATH`) to use a different one for tests or a second setup.

## Interactive Configuration Editor

Launch the visual customization editor at any time, from inside Aswell or from
your terminal:

```bash
aswell config            # or: aswell config edit
```

Every menu row is generated from the settings registry, so the editor always
covers all of it — prompt components, themes, animations, highlighting,
autosuggestions, history policy, bash compatibility — with a **live prompt
preview** rendered from your real environment.

| Key | Action |
|---|---|
| `↑`/`↓`, `j`/`k`, `PgUp`/`PgDn` | navigate |
| `Space`, `x`, `t` | toggle the highlighted switch |
| `y` / `n` | force the highlighted switch on / off |
| `←`/`→` | cycle values (the theme picker) |
| `Enter` | edit a text or number setting inline |
| `a` | show/hide the advanced settings |
| `d` | restore every default |
| `s` | save to `~/.config/aswell/config.txt` |
| `q`, `Esc` | quit (discards unsaved changes) |

From a pipe or a script, plain `aswell config` prints your non-default settings
instead, which keeps it usable over SSH and inside dotfiles.

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

## The Settings Hub (`aswell config`)

Every knob of the shell lives in one registry, and `aswell config` is the front
door to all of it — from a running shell **or** straight from your terminal.
Changes are saved to `~/.config/aswell/config.txt` *and* applied to the session
you are in, so there is nothing to restart.

```bash
aswell config                        # interactive TUI editor (live prompt preview)
aswell config list                   # every setting that differs from its default
aswell config list --all             # the full table, defaults included
aswell config list --json            # machine readable (dotfiles, statuslines, CI)
aswell config get theme              # just the value, for scripting:  V=$(aswell config get theme)
aswell config get show_git theme     # several at once as `key=value` lines
aswell config set show_git false     # validate, save, apply
aswell config set vi_mode=yes show_runtime=0   # several at once (atomic)
aswell config toggle autosuggestions # flip a boolean
aswell config unset show_git         # this one back to its default
aswell config reset                  # everything back to defaults
aswell config help                   # what each setting does, incl. aliases & value lists
aswell config help history_size      # one setting in detail
aswell config show                   # config.txt exactly as it sits on disk
aswell config path                   # which directory is in use
aswell config export --to aswell.cfg # a shareable, commented config file
aswell config import aswell.cfg      # validate + install someone else's config
```

Values are forgiving but never silent: `true/1/yes/on` and `false/0/no/off` all
work, aliases are accepted (`vim` → `vi_mode`, `hot_reload` → `auto_reload`),
and anything invalid is refused with a reason and a pasteable fix — instead of
being dropped the way a typo'd config file used to be:

```console
$ aswell config set animtion true
aswell: unknown setting 'animtion' (did you mean 'animation'?)
  List every setting with: aswell config help
$ aswell config set animation maybe
aswell: cannot set animation: 'maybe' is not a boolean (use true/false, 1/0, yes/no or on/off)
```

### All Settings

`~/.config/aswell/config.txt` is `key=value`, one per line; `#` starts a comment.
`aswell config help` prints this table from the same registry the code uses, so
the documentation cannot drift.

| Setting | Type | Default | Effect |
|---|---|---|---|
| `theme` | enum | `modern` | Prompt theme: built-in preset or `~/.config/aswell/themes/<name>.css` |
| `animation` | bool | `true` | Allow CSS animations (pulse, wave, rainbow, fire, spin, scramble) |
| `command_animation` | bool | `false` | Animate the command line itself while typing |
| `command_banner` | bool | `false` | Print an execution summary ribbon after each command |
| `colored_output` | bool | `true` | Export TrueColor hints and colored `ls`/`grep`/`diff` aliases |
| `show_username` | bool | `true` | Show `<user>` in generated prompts |
| `show_hostname` | bool | `true` | Show `<hostname>` in generated prompts |
| `show_directory` | bool | `true` | Show `<directory>` in generated prompts |
| `show_git` | bool | `true` | Show the `<git>` branch badge |
| `show_runtime` | bool | `true` | Show the `<runtime>` (last command duration) badge |
| `show_jobs` | bool | `true` | Show the `<jobs>` (background job count) badge |
| `show_status` | bool | `true` | Show the `<status>` (last exit code) badge |
| `username` | text | _empty_ | Display name replacing `$USER` (empty = system user) |
| `hostname` | text | _empty_ | Display name replacing the host (empty = real host) |
| `time_format` | text | `%H:%M:%S` | strftime format for the `<time>` prompt element |
| `syntax_highlighting` | bool | `true` | Colorize the command line as you type |
| `autosuggestions` | bool | `true` | Ghost-text history autosuggestions |
| `vi_mode` | bool | `false` | Start the line editor in Vi normal/insert modes |
| `history_size` | int | `10000` | Maximum saved history entries (10..1000000) |
| `history_ignore_dups` | bool | `false` | Drop repeated commands anywhere in history, not only consecutively |
| `auto_reload` | bool | `true` | Re-read theme, prompt template and config.txt when they change |
| `import_bashrc` | bool | `true` | Import `~/.bashrc` aliases, exports and functions on startup |

### Environment Variables

| Variable | Effect |
|---|---|
| `ASWELL_CONFIG_DIR` | Use another configuration directory (what `aswell --config PATH` sets) |
| `ASWELL_THEME` | Use a theme for this session only, without touching `config.txt` |
| `ASWELL_NO_BASHRC=1` | Skip the `~/.bashrc` import for this session |
| `ASWELL_USER`, `ASWELL_HOSTNAME` | Prompt identity (set for you by `aswell config set username …`) |

## Live Reload (no restarts while customizing)

With `auto_reload=true` (the default) the shell stats `config.txt`, `theme.css`,
`themes/<current>.css` and `prompt.html` before drawing each prompt; when any of
them changed it re-reads and re-applies everything, printing one dim line such
as `aswell: reloaded prompt.html`. Two safety properties:

- A stylesheet that yields **no** CSS rules (half-typed, saved by accident) is
  refused and the previous prompt is kept, with a warning.
- Typos in `config.txt` are reported at startup (`aswell: config.txt:3:
  show_git=maybe ...`) instead of vanishing silently.

Turn it off with `aswell config set auto_reload false`, or force one with
`aswell reload` (`aswell config set` / `aswell theme set` already apply live).

## Diagnosing Your Setup (`aswell doctor`)

```bash
aswell doctor            # full report: ✓ passed, ! hints, ✗ problems
aswell doctor --quiet    # only problems, for dotfiles/CI: aswell doctor -q || exit 1
```

It checks `config.txt` (unknown keys, bad values — each with a pasteable fix),
the active theme (resolvable? non-empty? valid CSS properties?), `prompt.html`
(unknown elements, unbalanced tags, and whether it renders at all), the custom
command and plugin directories, `$PATH`, TrueColor support, `auto_reload`, and
`/etc/shells` registration for `chsh`. The exit status is `1` only when
something is actually broken, so it is safe to put in a CI job.

## Themes (`aswell theme`)

The same commands work inside Aswell and from your terminal, and both persist
your choice:

```bash
aswell theme list                      # built-ins + your own themes/<name>.css
aswell theme set cyberpunk             # switch now and after restarts
aswell theme preview nord              # render your real prompt with that theme
aswell theme preview --all             # every theme, success + failure states
aswell theme preview cyberpunk --animate   # watch time-based animations (2.6s)
aswell theme show                      # print the active stylesheet and its source
aswell theme new mytheme               # scaffold themes/mytheme.css from your current theme
aswell theme new mytheme --from dracula --force
aswell theme reset                     # back to `modern`
```

Built-ins: `modern`, `cyberpunk`, `matrix`, `nord`, `minimal`, `dracula`,
`powerline`. Custom stylesheets live in `~/.config/aswell/themes/*.css`, and the
first `/* … */` comment of a file becomes its description in `aswell theme list`.

A theme name is resolved in this order:

1. `~/.config/aswell/themes/<name>.css` — your file, so a user theme can shadow a built-in
2. the built-in preset `<name>`
3. the legacy `~/.config/aswell/theme.css` (used when the name is unknown, `custom` or `theme.css`)
4. `modern`

`aswell theme set <typo>` fails with a suggestion instead of silently doing
nothing, and `ASWELL_THEME=nord aswell` tries a theme for one session.

### Prompt recipes worth knowing

```bash
# Minimal prompt for small screens / SSH
aswell config set show_hostname false show_runtime false show_jobs false
aswell theme set minimal

# 12-hour clock, and a date element in prompt.html: <time /> <date />
aswell config set time_format "%I:%M %p"

# History policy
aswell config set history_ignore_dups true history_size 50000

# A prompt widget without touching C++: compute it in a hook, print the variable
#   ~/.config/aswell/aswellrc:
#     aswell_on_prompt() { export MY_BRANCH="$(git branch --show-current 2>/dev/null)"; }
#   prompt.html:
#     <text show-if="$MY_BRANCH != ''">$MY_BRANCH</text>
```

### Bash Compatibility (`~/.bashrc` import)

On interactive startup Aswell imports your `~/.bashrc` so existing aliases
(`ll`, `gs`, ...), `export`ed variables (`EDITOR`, `PATH` entries, API keys)
and POSIX-compatible shell functions work immediately. `~/.aswellrc` is loaded
afterwards and always wins on conflicts. Completion also sees bash aliases.

```bash
aswell bash status         # show import status and alias count
aswell bash import         # re-import (e.g. after editing ~/.bashrc)
```

Disable via `import_bashrc=false` in `~/.config/aswell/config.txt`,
`--safe-mode`, or `ASWELL_NO_BASHRC=1`. Prompt internals (`PS1`,
`PROMPT_COMMAND`), history tuning and completion machinery (`_*` functions,
`COMPREPLY`) are never imported.

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


## Smart cd (unique prefix matching)

Interactively, `cd` accepts a half-typed directory name when it matches exactly
one directory in the current folder (case-insensitive fallback included):

```bash
cd Doc     # → jumps to Documents, prints where it went
cd D       # ambiguous → lists matches instead of guessing
```

Exact paths always win, and scripts / `aswell -c` keep strict POSIX behavior
(no guessing outside interactive use).

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
