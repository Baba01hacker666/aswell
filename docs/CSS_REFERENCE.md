# Aswell CSS & HTML Customization Reference

Aswell features a lightweight native styling engine that translates CSS rules into TrueColor ANSI sequences and terminal layout.

## Try It Without Restarting

Customization is a loop, so Aswell makes the loop short:

```bash
aswell theme preview cyberpunk       # render your real prompt in any theme
aswell theme new mytheme --from nord # scaffold ~/.config/aswell/themes/mytheme.css
$EDITOR ~/.config/aswell/themes/mytheme.css
aswell theme set mytheme             # live + persistent; your open shell updates too
aswell doctor                        # validate config.txt, CSS and prompt.html
```

With `auto_reload=true` (the default) a running shell re-reads `theme.css`,
`themes/<current>.css`, `prompt.html` and `config.txt` before drawing the next
prompt — save the file, look at your prompt. `aswell reload` forces it, and a
stylesheet that produces no rules at all is rejected so a half-typed file can
never blank your prompt.

## Supported HTML Tags

| Tag | Description | Default Content |
|---|---|---|
| `<prompt>` | Root container for the prompt | - |
| `<segment>` | Logical block or row | - |
| `<user>` | Current username | `$USER` |
| `<hostname>` | Current host machine name | Hostname |
| `<directory>` | Current working directory | `~/path/to/dir` |
| `<git>` | Current git branch and dirty status | `git:(main*)` |
| `<runtime>` | Last command execution time | `120ms` / `1.4s` |
| `<status>` | Last exit code | `[1]` |
| `<jobs>` | Count of background jobs | `{1}` |
| `<mode>` | Vi / Emacs editing mode | `[NORMAL]` / `[INSERT]` |
| `<symbol>` | Prompt input symbol | `❯` |
| `<text>` | Arbitrary text string | Text content |
| `<newline>` / `<br>` | Line break for multi-line prompts | `\n` |
| `<rprompt>` | Right-aligned prompt, column-aware | - |
| `<statusbar>` | Full-width dock rendered above the prompt | - |
| `<time>` | Current time, formatted by `time_format` | `14:05:09` |
| `<date>` | Current date | `2026-10-03` |
| `<cwd>` | Alias of `<directory>` | `~/path/to/dir` |

## Selectors

- **Tag selector**: `directory { ... }`
- **Class selector**: `.bracket { ... }`, `git.dirty { ... }`
- **Pseudoclass selector**:
  - `status.error` / `status:error` (active when `$? != 0`)
  - `status:success` (active when `$? == 0`)
  - `git.dirty` / `git:dirty` (active when working tree is uncommitted)
  - `git.clean` / `git:clean` (active when repository is clean)
  - `mode.normal` (active in Vi normal mode)

## Supported CSS Properties

### Colors
- `color`: Text color (`#00f0ff`, `rgb(255, 80, 100)`, or named: `cyan`, `magenta`, `nord0..15`, `dracula-cyan`, etc.)
- `background` / `background-color`: Background color

### Typography & Decorations
- `font-weight`: `bold`, `normal`
- `font-style`: `italic`, `normal`
- `text-decoration`: `underline`, `strikethrough`, `none`

### Box Model & Spacing
- `padding`: In characters, e.g. `padding: 0 1;` (vertical horizontal)
- `padding-left`, `padding-right`
- `margin`: In characters, e.g. `margin-left: 1;`
- `margin-left`, `margin-right`
- `border`: `[style] [color]`, e.g. `border: rounded cyan;` or `border: solid #ff5555;`
- `border-radius`: Enables rounded unicode corners (`╭`, `╮`, `╰`, `╯`)

### Content Overrides
- `content`: Overrides the text glyph, e.g. `content: "➜ ";`

### Animations
- `animation: [name] [duration] [infinite]`
  - Names:
    - `pulse`: Smooth brightness oscillation
    - `rainbow`: 360° HSV hue cycling
    - `fire`: Flickering red-orange-yellow gradient
    - `spin`: Animated rotating spinner glyphs
    - `wave`: Sweeping intensity highlight
    - `scramble`: Decodes text from random glyphs (aliases: `glitch`, `matrix`)
  - Example: `animation: pulse 1200ms infinite;`

## Example Theme

```css
prompt.main {
    display: block;
}
user {
    color: #8be9fd;
    font-weight: bold;
}
directory {
    color: #50fa7b;
    font-weight: bold;
    margin-left: 1;
}
git {
    color: #f1fa8c;
    margin-left: 1;
}
git.dirty {
    color: #ffb86c;
}
symbol {
    color: #ff79c6;
    animation: pulse 1s infinite;
}
```
