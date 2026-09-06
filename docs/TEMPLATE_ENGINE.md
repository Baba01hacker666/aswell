# Aswell Template Engine Guide

The **Aswell Template Engine** is a high-performance, declarative terminal markup and expression engine built from scratch in C++20. It allows users to design prompts, cockpit interfaces, animations, and event-driven effects (like fire burning on `rm` commands) **completely through template files** without modifying or compiling any C++ code.

---

## 🌟 Philosophy: Zero C++ Code Required

Instead of hardcoding features or animations into the shell's C++ source code, Aswell delegates styling and visual event hooks to templates located in `~/.config/aswell/templates/`.

Users can customize colors, characters, animation durations, mathematical physics formulas, and event responses purely by editing `.html` or `.tmpl` files.

---

## 📁 Template Directory Structure

```
~/.config/aswell/templates/
├── events.html         # Shell lifecycle and command event handlers
├── fire.html           # Standalone procedural fire rising animation
└── prompt.html         # Declarative prompt layout
```

---

## 🏷️ Supported Markup Tags

### 1. Variables & Expressions
- `{{ var }}`: Output context variable (e.g. `{{ target }}`, `{{ user }}`, `{{ cmd }}`).
- `{{ var | upper }}`: Uppercase filter.
- `{{ var | lower }}`: Lowercase filter.
- `{{ var | trim }}`: Whitespace trim filter.
- `{{ var | len }}`: String length.
- `{{ math_expr }}`: Any mathematical expression: `{{ round(255 * sin(x)) }}`.

### 2. Mathematics & Functions
The built-in expression parser supports:
- Operators: `+`, `-`, `*`, `/`, `%`, `==`, `!=`, `<`, `>`, `<=`, `>=`, `&&`, `||`, `!`
- Functions: `sin(x)`, `cos(x)`, `abs(x)`, `round(x)`, `floor(x)`, `min(a, b)`, `max(a, b)`, `clamp(val, min, max)`, `if(condition, true_val, false_val)`

### 3. Flow Control
- `<if condition="..."> ... <elif condition="..."> ... <else> ... </if>`: Conditional branches.
- `<for var="i" from="0" to="7" step="1"> ... </for>`: Numeric iteration loops.
- `<repeat count="20" var="idx"> ... </repeat>`: Simple repetition.
- `<let var="name" val="expr" />`: Variable declaration & evaluation.

### 4. Terminal Styling & Color
- `<color fg="#ffaa00" bg="#220000" bold="true"> ... </color>`: Emits ANSI 24-bit TrueColor sequences.
- `<text> ... </text>`: Raw text block.
- `<badge> ... </badge>`: Styled status banner line.
- `<cursor action="up|down|clear_line|hide|show" count="N" />`: Terminal cursor movement.

### 5. Procedural Animation & Grids
- `<animation frames="19" delay="35ms" height="8" clear="true">`: Executes an interactive keyframed animation loop.
- `<grid rows="8" cols="54" row_var="y" col_var="x">`: Evaluates a 2D matrix for procedural effects (fire, matrix rain, waves).

---

## 🛠️ CLI Builtin Commands

```bash
# List available templates
aswell template list

# Evaluate a template snippet on the fly
aswell template eval '<color fg="#ff5500">Hello {{ name | upper }}!</color>' name=user

# Render a template file with variables
aswell template render events.html target=myfile.txt

# Trigger the declarative fire effect
aswell template fire <filename>
# or directly:
aswell fire <filename>
```
