# Lexico Compiler (C++ + Flex/Bison + LLVM C API)

This document is a practical guide to implement the **compiler core** for your project **Lexico: an AI-assisted human-oriented programming language**, using:

- **C++** for the compiler core
- **Flex** for lexical analysis
- **Bison** for parsing (syntax + locations for good error messages)
- **LLVM C API** for IR generation and execution via `lli` or `clang`

It matches your current language features:

- Variable declarations:
  - `create int a set value 20;`
  - `create int a,b,c set value 23,24,25;` (values assigned respectively)
  - Also supports `string` (`"..."`), `char` (`'c'`), `float` (compiled as **double**, printed with **2 digits** after decimal), and `bool` (`true`/`false`)
- Print statements:
  - `print a;`
  - `print 'c';`
  - `print "asasz";`
  - `print a,'f',"fgff",20,12.35;` (mixed args)
- File system statements:
  - `open "path.txt" file do ...`
  - `open "path.txt" file make do ...`
  - line-based `write`, `read`, `clear`, and file `title` update inside file blocks

---

## 0) RAG-first reading mode (explicit and tolerant)

This guide is written to be consumed by both humans and Retrieval-Augmented Generation (RAG) systems.

### 0.1) RAG objective

- Maximize correct retrieval for noisy prompts, partial prompts, typo-heavy prompts, and mixed wording.
- Keep one canonical answer per concept while preserving common aliases.
- Route natural-language intent to exact compiler components.

### 0.2) Canonical retrieval units

Treat each top-level section as a retrieval unit with metadata:

- `section_id`: stable numeric id in this file (`0`, `1`, `2`, `3`, `4`, `5`, `16`, `17`).
- `topic`: short canonical name (example: `module-load`, `convert-expr`, `file-io`).
- `source_of_truth`: grammar, AST, semantics, runtime, or build.
- `owner_files`: concrete code files implementing behavior.
- `query_aliases`: equivalent human phrases and typo variants.

### 0.3) Tolerant query aliases (recommended index dictionary)

Use these aliases during indexing and query expansion:

- `convert`, `cast`, `type change`, `change variable type`, `convert literal` -> conversion feature
- `load`, `include`, `import file`, `link file`, `reuse file` -> module loading
- `routine`, `function`, `method`, `procedure` -> routine system
- `mesh`, `array`, `table`, `list`, `collection` -> collection system
- `matrix`, `2d table`, `grid` -> matrix system
- `file`, `open`, `read`, `write`, `clear`, `rename title` -> file I/O
- `if`, `condition`, `branch`, `otherwise`, `else` -> condition flow
- `loop`, `while`, `for`, `repeat`, `attempt` -> loop family
- `lexer`, `scanner`, `tokenizer` -> lexical stage
- `parser`, `grammar`, `bison`, `syntax` -> syntax stage
- `semantic`, `type check`, `symbol table` -> semantic stage
- `codegen`, `llvm`, `jit`, `ir` -> backend stage

Tolerance policy for production retrieval:

- Lowercase and trim input.
- Normalize punctuation and repeated whitespace.
- Apply lightweight spelling tolerance for close-edit-distance terms.
- Expand query terms with alias dictionary before vector + keyword retrieval.

### 0.4) Best-method RAG pipeline for this project

Use a hybrid retrieval pipeline:

1. Query normalization
- Lowercase, strip punctuation noise, normalize whitespace.
- Expand with alias dictionary from section 0.3.

2. Dual retrieval
- Dense retrieval over section chunks.
- BM25/keyword retrieval over exact tokens (`convert`, `load`, `STMT_*`, file names).

3. Reciprocal-rank fusion
- Merge dense + keyword results to avoid missing exact syntax questions.

4. Metadata filtering
- Filter by `source_of_truth` when prompt asks for grammar, codegen, runtime, or build.

5. Answer grounding
- Always answer from matched sections and implementation files.
- Prefer canonical syntax from section 3 when wording conflicts.

### 0.5) Chunking strategy for this file

- Chunk target size: 600-1200 tokens.
- Overlap: 80-120 tokens.
- Split boundaries: headings (`##`, `###`, `####`) only.
- Keep code blocks intact; do not split in the middle of grammar productions.

### 0.6) Minimal metadata schema (JSON)

```json
{
  "doc_id": "compiler_guide",
  "section_id": "4",
  "topic": "compiler-internals",
  "source_of_truth": "implementation",
  "owner_files": ["src/driver.cpp", "src/lexico.y", "src/codegen.cpp"],
  "query_aliases": ["pipeline", "frontend", "backend", "jit"],
  "last_verified": "2026-03-12"
}
```

### 0.7) Prompting rule for tolerant answers

When asked vague questions, answer with this order:

1. Canonical behavior (what Lexico does now)
2. Implementation location (which source files enforce it)
3. Limits and edge cases (what can fail)
4. Extension path (how to add feature safely)

This keeps responses stable for both beginner and expert prompts.

---

## 1) Recommended architecture (fits your cahier des charges)

Your overall system is hybrid (AI + compiler):

1. User writes code in human-like language.
2. Python AI normalizes it.
3. Normalized output is stored (JSON).
4. C compiler reads and compiles.

**Recommendation (simple & robust):**

- Keep Flex/Bison parsing a **strict normalized source text** (your DSL). This is the easiest way to build a real compiler.
- For the JSON requirement: Python can output JSON with a field like:
  ```json
  {"normalized_source": "create int a set value 20;\nprint a;\n"}
  ```
  The C++ core reads JSON, extracts `normalized_source` into a temp string/file, then runs the normal Flex/Bison pipeline.

This preserves separation: Python does “understanding”; C++ does “compilation”.

---

## 2) Minimal language spec (what the compiler should accept)

### Keywords
- `create`
- `set`
- `value`
- `print`
- Types: `int`, `float`, `char`, `string`, `bool`
- Boolean literals: `true`, `false`

### Statements
1. Declaration (initialized or empty):
   ```
   create <type> <idlist> set value <vallist>;
  create <type> <idlist>;
   ```
2. Print:
   ```
   print <arglist>;
   ```

### Lists
- `<idlist>`: identifiers separated by commas
- `<vallist>`: literals separated by commas
- **Semantic rule (initialized form):** number of identifiers must equal number of values.

For the empty form `create <type> <idlist>;`, each variable is initialized to a typed default:
- `int` → `0`
- `float` → `0.0`
- `char` → `'\0'`
- `string` → `""`
- `bool` → `false`

### Literals
- `int`: `20`
- `float`: `12.35` (compile as double; **print** with `%.2f`)
- `char`: `'c'` (single character; optionally support escape `\n`, `\t`, `\'`)
- `string`: `"hello"` (optionally support escapes)
- `bool`: `true` / `false`

### Print arguments
Allow a mix of:
- identifier
- literal (`int`, `float`, `char`, `string`, `bool`)

You can also print **arithmetic expressions**, not just raw variables or literals. The expression syntax is the same as for `set`:

- `plus`, `minus`, `times`, `divided by`, `mod`
- Parentheses `(` `)` to control precedence.

Examples:
- `print a plus 3;`                         → prints the value of `a + 3`
- `print (a plus 3) times 2;`               → prints `(a + 3) * 2`
- `print a plus 3 times (4 divided by 2);`  → prints `a + 3 * (4 / 2)` (with correct precedence)
- `print a plus 3, "result is";`          → prints the computed value then a string

Semantics:
- Each print argument is evaluated **from left to right**.
- If the argument is an expression, it is fully evaluated, then printed according to its type (int, float, char, string).
- Type rules for expressions are the same as everywhere else (e.g., `int` plus `int` is `int`, `int` plus `float` becomes `float`, etc.).

---

## 2.1) Operations & expressions (human-readable representation)

To keep Lexico **humanly readable** while staying **compiler-friendly**, use a *controlled natural language* for operations:

### Canonical assignment (update)
Prefer a single, consistent form that is easy to parse:

```
set <identifier> to <expr>;
```

Examples:
- `set a to 10;`
- `set a to a plus 1;`
- `set total to price times quantity;`
- `set avg to (a plus b) divided by 2;`

This gives you variable “updates” without inventing many statement forms.

### Arithmetic operators (keywords)
Define operations as keywords (optionally allow symbol aliases as “sugar” later):

- `plus` (optional alias: `+`)
- `minus` (optional alias: `-`)
- `times` (optional alias: `*`)
- `divided by` (optional alias: `/`)
- `mod` (optional alias: `%`)

Examples:
- `set r to x mod 2;`
- `set speed to distance divided by time;`

### Explicit scalar conversion (`convert`)

Lexico supports in-place scalar type conversion:

```lexico
convert <identifier> to int;
convert <identifier> to float;
convert <identifier> to char;
convert <identifier> to string;
```

Example:

```lexico
create int n set value 65;
convert n to char;
print n;          # A

convert n to string;
print n;          # "A"
```

Rules:

- `convert` target must be a scalar variable.
- Destination type currently supports: `int`, `float`, `char`, `string`.
- Conversions are applied in-place; later statements see the converted type.
- String to numeric/char conversions are validated at runtime:
  - invalid numeric text raises a runtime error,
  - string to char requires exactly one character.

### String operators (implemented)

Lexico now supports string binary operations using the same operator words:

- `<string1> plus <string2>`: concatenates two strings.
- `<string1> minus <string2>`: removes the first occurrence of `<string2>` from `<string1>`.

Examples:

- `set s to "hello" plus "world";` -> `"helloworld"`
- `set s to "banana" minus "na";` -> `"bana"`
- `set s to "abc" minus "z";` -> `"abc"` (no change if substring not found)

Rules:

- Both operands must be `string`.
- `plus` and `minus` are the only string binary operators.
- Other arithmetic operators (`times`, `divided by`, `mod`) are not valid for strings.

### Comparisons (for if/loops later)
When you add control flow, use readable comparisons:

- `is equal to`
- `is not equal to`
- `is greater than`
- `is less than`
- `is greater or equal to`
- `is less or equal to`

Examples:
- `if a is greater than 10 then ...`
- `if name is equal to "Ali" then ...`

### Logical operators (for if/loops later)
- `and`, `or`, `not`

### Lexer notes (Flex)
- Multi-word operators like `divided by` should be tokenized as a single token. A common rule is:
  - Match: `divided[ \t]+by` and return token `DIV`
- Keep the canonical keywords stable; if you want many English variants (“add”, “sum”, “increase”), do it in the AI normalizer layer and rewrite them to the canonical tokens (`plus`, `set ... to ...`).

### Parser notes (Bison)
Parser rules are centralized in a **single canonical grammar** below (see section **3**).
This avoids drift between partial snippets and the actual language accepted by the compiler.

---

## 2.1.1) Compound self-assignment shortcut (`<var> <op> <expr>;`)

### Motivation

The canonical form `set a to a plus 3;` is already readable, but when the variable on the left and the first operand on the right are the **same name**, it is redundant to write it twice. Lexico allows a shorter form that drops `set … to` entirely:

```
<identifier> <op> <expr>;
```

This is only valid when the operation **applies the result back to the same variable**. It is sugar over the canonical form and is completely equivalent — the compiler desugars it before any code generation.

---

### 2.1.1.1) Syntax rule

The statement starts with a **bare identifier** (not `set`, not `create`, not a keyword) followed directly by an arithmetic operator and an expression:

| Short form | Desugared canonical form |
|---|---|
| `a plus 3;` | `set a to a plus 3;` |
| `a minus 1;` | `set a to a minus 1;` |
| `a times 2;` | `set a to a times 2;` |
| `a divided by 4;` | `set a to a divided by 4;` |
| `a mod 5;` | `set a to a mod 5;` |

All five arithmetic operators (`plus`, `minus`, `times`, `divided by`, `mod`) are supported.

---

### 2.1.1.2) Expression on the right-hand side

The right-hand side is a **full expression** with the same precedence rules as the rest of Lexico:

1. Parentheses are evaluated first.
2. Then `times`, `divided by`, `mod`.
3. Then `plus`, `minus`.

#### Long example

```
a plus 3 times (4 divided by 2);
```

Step-by-step reading:

1. `a` — the variable being updated.
2. `plus` — the outer operator applied to `a`.
3. `3 times (4 divided by 2)` — the right-hand expression.
   - Inner: `4 divided by 2` → `2`
   - Then: `3 times 2` → `6`
4. Final: `a = a + 6`

Desugared equivalent:

```
set a to a plus 3 times (4 divided by 2);
```

Both forms compile to exactly the same LLVM IR.

---

### 2.1.1.3) Operator precedence recap (matters for the RHS)

```
a plus 3 times 4;       →  a = a + (3 * 4)   = a + 12
a plus 3 times (4 divided by 2);  →  a = a + (3 * 2)   = a + 6
a times 2 plus 1;       →  a = (a * 2) + 1
```

Parentheses follow the same `(` `)` syntax already supported by the `factor` rule.

---

### 2.1.1.4) What it is NOT

- It does **not** support a different variable on the left and the right:
  ```
  b plus a;   ✗  (this would try to do b = b + a, fine)
  b plus a times 2;   ✓  means b = b + (a * 2)
  ```
  Actually any identifier is valid as the target — the key rule is:  
  **the identifier that begins the statement is always BOTH the target of the assignment AND implicitly the left-hand side of the expression with `set <id> to <id> <op> <rhs>`**.

- It does **not** allow chaining more than one operator at the top level without parentheses in a confusing way — precedence rules resolve everything:
  ```
  a plus b times c;   →  set a to a plus (b times c);   ✓
  ```

---

### 2.1.1.5) Grammar addition (Bison)
Canonical grammar for this feature is included in section **3** (single source of truth).
`compound_stmt` remains parser sugar that lowers to canonical `set` AST form.

---

### 2.1.1.6) Semantic rules

The same rules as `set_stmt` apply:

1. The identifier must already be declared (`create` was called before).
2. The RHS expression must be type-compatible with the variable's declared type.
3. All identifiers used inside the RHS expression must also be declared.

No new semantic rules are required — the desugaring happens at the AST construction level, so the semantic analyser and code generator never see the short form at all.

---

## 2.1.2) User input model (single canonical method)

Lexico now uses one canonical input approach based on `take user input`:

1. **Declaration-time input** for scalar variables.
2. **Assignment-time input** for scalar updates and collection slots.

Standalone `ask` forms are removed.

---

### 2.1.2.1) Scalar input

Canonical forms:

```
create <type> <id> set value take user input;
create <type> <id> set value take user input with message <message_parts>;

set <id> to take user input;
set <id> to take user input with message <message_parts>;
```

Examples:

- `create int age set value take user input;`
- `create int limit set value 10;`
- `create int age set value take user input with message "Age must be > ", limit, " : ";`
- `set age to take user input with message "Retry age: ";`

---

### 2.1.2.2) Collection-slot input

Explicit target:

```
set <collection> at <index_expr> to take user input;
set <collection> at <index_expr> to take user input with message <message_parts>;
```

Contextual target (inside collection-bound loops only):

```
set value to take user input;
set value to take user input with message <message_parts>;
```

Example:

```
for i in nums from 0 to 3 do
> set value to take user input with message "nums[", i, "] : ";
```

---

### 2.1.2.3) Message interpolation (`with message`)

`<message_parts>` is a comma-separated list of expressions evaluated and printed left-to-right before reading input.

Canonical form:

```
with message <expr> (',' <expr>)*;
```

This means:

- String-only is valid: `with message "Age: ";`
- Variable/number-only is valid: `with message i;`
- Mixed forms are valid: `with message i, " : ";`
- Mixed string-variable-string is valid: `with message "numbers [", i, "]:";`
- A trailing comma before `;` is invalid.

Valid parts include:

- String literals
- Identifiers
- Arithmetic expressions

Example:

```
create int b set value 2;
create int s set value take user input with message "give S greater than ", 5 divided by b, " : ";
```

---

### 2.1.2.4) Recommended semantic rules

1. **Typed read**
  - `int` → integer read
  - `float` → floating read
  - `char` → single-character read
  - `string` → string read
2. **Declaration behavior**
  - `create <type> <id> set value take user input ...` follows normal duplicate declaration checks.
3. **Scalar assignment behavior**
  - `set <id> to take user input ...` requires `<id>` to be declared and scalar.
4. **Collection assignment behavior**
  - `set <collection> at <index> ...` requires declared mesh and int-compatible index.
  - `set value to ...` requires active collection context from `for ... in <collection> ...`.
5. **No implicit type changes**
  - Input is parsed/coerced to the destination type.

---

### 2.1.2.5) Grammar sketch (Bison)
Canonical grammar for user input appears in section **3**.

---

### 2.1.2.6) Interaction with loops (recommended style)

```
create int tries set value 0;
create int age set value take user input with message "Age: ";

attempt up to 3 times while age is less than 0 do
> set age to take user input with message "Age must be >= 0. Try again: ";
> set tries to tries plus 1;
on failure
> print "gave up after", tries, "tries";
```

---

## 2.2) Creative control flow: `if ... then` with `>` block markers (no `end if`)

Lexico’s goal is to feel “human-written”, so here is a creative (but still compiler-friendly) block structure:

- The `if` header is written on its own line.
- The statements *inside* the `if` are written on the following lines.
- Each line that belongs to the block starts with one or more `>` characters.
- The number of `>` characters indicates the nesting depth (like indentation).
- The block ends automatically when the next non-empty line has a *smaller* depth (or no `>` at all) or at EOF.

### 2.2.1) Canonical syntax

**Header:**

```
if <condition> then
```

**Body (depth = 1):**

```
> <statement>;
> <statement>;
```

**Nested body (depth = 2):**

```
>> <statement>;
```

This means:

- `>` starts a statement that is inside the nearest open block at depth 1.
- `>>` starts a statement inside a nested block at depth 2.
- In general, depth is the count of consecutive `>` at the start of the line.

### 2.2.2) Full example (nested `if`)

This example demonstrates entering a block with `>` and nesting with `>>`:

```
if A is greater than 20 then
> print "L1";
> if A is greater than 30 then
>> print "L2";
> print "back to L1";
print "outside";
```

How to read it:

- `if A is greater than 20 then` starts an `if` at depth 0.
- The next two `>` lines are inside that `if` (depth 1).
- The `> if A is greater than 30 then` line starts a nested `if` *inside* the first `if`.
- The `>> print "L2";` line belongs to the nested `if` (depth 2).
- `> print "back to L1";` returns to depth 1 (still inside the outer `if`).
- `print "outside";` has no `>`, so it is outside all `if` blocks.

### 2.2.3) What exactly ends a block?

Define these rules precisely so your compiler behaves predictably:

1. **Only the start of a line matters** for block depth.
   - Depth is computed from consecutive `>` characters at the beginning of the line.
   - Optional spaces after the markers are allowed.
2. **A block continues** while subsequent non-empty lines have depth **greater than or equal to** the block’s depth.
3. **A block ends** when you encounter a non-empty line with depth **less than** the current block depth.
4. **Empty/blank lines** do not change depth (they can be ignored for indentation logic).
5. **EOF** ends all open blocks.

Practical interpretation:

- After an `if ... then` header, you must see at least one depth-1 line (`>`). If not, that is a syntax error.
- A nested `if` header at depth 1 must be followed by at least one depth-2 line (`>>`).

### 2.2.4) Statement termination and formatting rules

To keep parsing simple and errors clear:

- Every statement still ends with `;`.
- The `>` markers are **not** statement terminators; they only describe block membership.
- Inside blocks you still write normal Lexico statements (declarations, `set`, `print`, and nested `if`).

Valid:

```
if A is greater than 20 then
> print "ok";
print "done";
```

Invalid (missing `;`):

```
if A is greater than 20 then
> print "ok"
```

### 2.2.5) Conditions (human-readable comparisons)

Use controlled phrases so the AI normalizer and the compiler agree:

- `is equal to`
- `is not equal to`
- `is greater than`
- `is less than`
- `is greater or equal to`
- `is less or equal to`

Boolean connectives:

- `and`, `or`, `not`

Examples:

```
if (A is greater than 0) and not (A is equal to 10) then
> print A;
```

### 2.2.6) Implementation notes (how to compile `>` blocks)

This syntax is “layout-sensitive”. The simplest robust approach is:

1. The lexer must be aware of **start-of-line**.
2. The lexer must count leading `>` markers to compute a numeric depth.
3. The lexer should emit **virtual indentation tokens** so Bison stays simple.

Recommended virtual tokens:

- `INDENT` when depth increases (e.g., from 0 to 1, or 1 to 2)
- `DEDENT` when depth decreases
- `NEWLINE` (optional) if you want to structure header vs block lines cleanly

High-level algorithm (conceptual):

- Keep an `indent_stack` of depths (start with `[0]`).
- At the beginning of each non-empty line:
  - compute `depth = count('>')`
  - if `depth > indent_stack.top`: push depth and emit `INDENT`
  - if `depth < indent_stack.top`: pop until top == depth, emitting `DEDENT` for each pop
  - if `depth` is not equal to any depth on the stack after popping: error (“invalid block alignment”)
- After consuming the `>` markers, continue lexing the rest of the line normally.
- At EOF: emit `DEDENT` until the stack returns to `[0]`.

Why this matters:

- It makes nested `if` blocks *unambiguous*.
- Bison can use an ordinary block grammar instead of trying to reason about raw `>` characters.

### 2.2.7) Important warning: terminal `>` vs Lexico `>`

If you type `if ... then` directly into PowerShell, you may see `>` prompts. That is PowerShell’s continuation prompt and is unrelated.

Lexico’s `>` block markers must be written in a `.lx` source file and compiled by your `lexico` compiler.

---

## 2.3) Loops: `while`, `for`, `repeat until`, and `attempt`

Loops reuse the same **line-structured block** idea as `if`:

- Loop header on its own line (no trailing `;`).
- Body = consecutive lines starting with one or more `>` characters.
- Nesting = more `>` characters (`>`, `>>`, `>>>`, …).
- A loop body ends when the next non-empty line has a **smaller depth** (or at EOF).

Conditions use the same controlled language as `if`:

- Comparisons: `is equal to`, `is not equal to`, `is greater than`, `is less than`,
  `is greater or equal to`, `is less or equal to`.
- Boolean: `and`, `or`, `not` (with parentheses allowed).

Below are the four core loop forms.

---

### 2.3.1) `while` – pre-condition loop

**Idea:** “As long as this condition is true, keep repeating these steps.”

#### Syntax

Header at some depth (typically 0):

```lexico
while <condition> do
```

Body at depth 1:

```lexico
> <statement>;
> <statement>;
```

Nested blocks at deeper depths:

```lexico
>> <statement>;
```

#### Example – simple counter

```lexico
create int i set value 0;

while i is less than 5 do
> print i;
> set i to i plus 1;

print "done";
```

#### Semantics

- Evaluate `<condition>` **before each iteration**.
- If the condition is **true**, execute the body, then go back and test again.
- If the condition is **false** at the start, the body may execute **zero times**.

Conceptually equivalent to:

```text
while (condition) {
  body;
}
```

The compiler will internally translate this to a conditional branch with one or more loop basic blocks.

---

### 2.3.2) `for` – counting loop

**Idea:** “Repeat these steps a fixed number of times, using a loop variable.”

This is sugar over a `while` with an explicit counter, but it is much clearer for beginners.

#### Syntax

Numeric loop form:

```lexico
for i from <start_expr> to <end_expr> do
> <statement>;
```

Numeric loop with explicit step:

```lexico
for i from <start_expr> to <end_expr> step <step_expr> do
> <statement>;
```

Collection-bound loop form:

```lexico
for i in <mesh_name> from <start_expr> to <end_expr> do
> <statement>;
```

Collection-bound loop with explicit step:

```lexico
for i in <mesh_name> from <start_expr> to <end_expr> step <step_expr> do
> <statement>;
```

The step is a normal Lexico expression written using Lexico's own keywords:

- **positive step:** use `step plus 1`, `step plus 2`.
- **negative step:** use `step minus 1`, `step minus 2`.

If `step` is omitted in either numeric or collection form, Lexico defaults it to `step plus 1`.

#### Examples

Numeric count up by 1 (explicit):

```lexico
for i from 1 to 5 step plus 1 do
> print i;
```

Numeric count up by 2:

```lexico
for i from 0 to 10 step plus 2 do
> print i;
```

Numeric count down by 1:

```lexico
for i from 5 to 1 step minus 1 do
> print i;
```

Numeric count down by 2:

```lexico
for i from 10 to 0 step minus 2 do
> print i;
```

Collection-bound example:

```lexico
for i in nums from 0 to s step plus 1 do
> set value at i to nums at i plus 1;
```

#### Semantics

Let `start`, `end`, `step` be the evaluated expressions (computed **once** before the loop):

1. Initialize `i = start`.
2. If `step > 0`: keep looping while `i is less or equal to end`.
   If `step < 0`: keep looping while `i is greater or equal to end`.
3. On each iteration:
   - execute the body;
   - set `i = i plus step`.
4. When the comparison fails, exit the loop.

Additional constraints:
- `start`, `end`, and `step` must be `int`-compatible.
- `step == 0` is a semantic error.
- In collection-bound loops, index bounds must remain valid for the bound collection.

This behaves like:

```text
i = start;

#### Loop-local iterator variable

The identifier that appears after `for` (for example `i` in `for i from 1 to 5 ...` or `for i in nums from ...`) is a
**loop-local iterator**:

- You do **not** need a separate declaration such as `create int i set value 0;`.
- The loop implicitly creates an `int` variable `i` whose lifetime is limited to:
  - the `from` / `to` / `step` expressions of that `for`, and
  - the `>` body lines that belong to that `for`.
- After the loop finishes, `i` is **no longer accessible**; using it outside the loop is
  a compile-time error (“use of undeclared identifier”).
- You may reuse the same name in another `for` header; each `for` gets its own iterator.

Example:

```lexico
for i from 1 to 3 step plus 1 do
> print i;

print i;  -- ERROR: i does not exist here

for i from 10 to 12 step plus 1 do
> print i;  -- a new loop-local i
```
if (step > 0) {
  while (i <= end) { body; i += step; }
} else if (step < 0) {
  while (i >= end) { body; i += step; }
} else {
  // step == 0 is a static error in Lexico
}
```

The compiler will typically lower `for` into an equivalent `while` in the AST or codegen.

---

### 2.3.3) `repeat until` – post-condition loop

**Idea:** “Do these steps, and **then** check the condition. Stop when the condition becomes true.”

This is the opposite of `while` in timing:

- `while` checks **before** the body (may run **0 times**).
- `repeat until` checks **after** the body (runs **at least 1 time**).

#### Syntax with `>` blocks (no `end repeat;`)

Because Lexico avoids explicit `end repeat;`, we use depth and an `until` line:

1. `repeat` header at some depth (usually 0).
2. Body lines at **greater depth** (`>`).
3. An `until <condition> then` line at the **same depth as `repeat`**.

Canonical form:

```lexico
repeat
> <statement>;
> <statement>;
until <condition> then

<following code at depth 0>
```

#### Example

```lexico
create int x set value 0;

repeat
> print x;
> set x to x plus 1;
until x is greater or equal to 3 then

print "after repeat";
```

**How to read this:**

- `repeat` starts a loop header at depth 0.
- The `>` lines belong to the repeat-body (depth 1).
- `until x is greater or equal to 3 then` closes the repeat and gives its exit condition.
  It must appear at the **same depth** as `repeat`.

#### Semantics

For:

```lexico
repeat
> body...
until <condition> then
```

the behavior is:

1. Execute `body` once.
2. Evaluate `<condition>`.
3. If `<condition>` is **true** → exit the loop.
4. If `<condition>` is **false** → execute `body` again, then re-check.

Equivalently:

```text
do {
  body;
} while (!condition);
```

So the body **always runs at least once**, even if the condition is already true.

---

### 2.3.4) `attempt` – bounded retry loop with failure block

**Idea:** A safe, purpose-specific loop for **retrying** an action a limited number of times.

Run a block **at most N times while a condition remains true**. If you hit the limit and the
condition is still true, run a separate **failure block** exactly once.

This pattern is useful for “try a few times, then give up” scenarios, and it prevents
infinite loops by construction.

#### Syntax

Header:

```lexico
attempt up to <max_expr> while <condition> do
```

Body at depth 1:

```lexico
> <statement>;
> <statement>;
```

Optional failure block:

```lexico
on failure
> <statement>;
> <statement>;
```

If `on failure` is omitted, the loop simply stops when it either succeeds or reaches the limit.

#### Example – bounded retry with failure

```lexico
create int tries  set value 0;
create int secret set value 7;
create int guess  set value 0;

attempt up to 3 while guess is not equal to secret do
> print "Trying...";
> set guess to guess plus 3;
> set tries to tries plus 1;

on failure
> print "I gave up after", tries, "tries.";
```

Behavior:

- Loop runs while `guess is not equal to secret`, but **no more than 3 iterations**.
- If `guess` becomes equal to `secret` before 3 attempts → loop exits early, `on failure` is **skipped**.
- If after 3 iterations `guess` is still not equal to `secret` → `on failure` runs once.

#### Example – no failure block

```lexico
create int x set value 0;

attempt up to 5 while x is less than 10 do
> print "x is", x;
> set x to x plus 3;
```

Loop stops either when `x` is no longer `< 10` or when it has executed 5 times, whichever comes first.

#### Semantics

For the general form:

```lexico
attempt up to N while C do
> body...
on failure
> fail_body...
```

let `N` be the evaluated value of `<max_expr>` (non-negative integer) and `C` the condition.

1. Set `counter = 0`.
2. While `counter is less than N` **and** `C` is true:
   - execute `body`;
   - set `counter` to `counter plus 1`.
3. After the loop:
   - If `counter is equal to N` **and** `C` is still true → execute `fail_body` once.
   - Otherwise (exited early because `C` became false) → skip `fail_body`.

This is equivalent to:

```text
counter = 0;
while (counter < N && C) {
  body;
  counter++;
}
if (counter == N && C) {
  fail_body;
}
```

In the compiler, `attempt` can be lowered to a `while` loop with an explicit counter and
an optional trailing block for the `on failure` part.

---

### 2.3.5) Comparing `while`, `repeat until`, and `attempt`

| Loop            | Condition timing                     | Minimum iterations | Built-in safety              | Failure path |
|-----------------|--------------------------------------|--------------------|------------------------------|-------------|
| `while`         | Check **before** each iteration      | 0                  | None                         | No          |
| `repeat until`  | Check **after** each iteration       | 1                  | None                         | No          |
| `attempt`       | Check before (like `while`), but
|                 | also bounded by max attempts         | 0–N                | Hard upper bound on attempts | Optional    |

This table is a useful mental model when choosing which loop to use:

- Use **`while`** when you only care about “as long as this is true, keep going”.
- Use **`repeat until`** when you need the body to run **at least once**.
- Use **`for`** when you are counting over a numeric range.
- Use **`attempt`** when you want a **limited retry** with an explicit “gave up” branch.

---

### 2.4) Routines (implemented)

In Lexico, a **routine** is a named block of logic that may:

- accept typed input parameters,
- execute statements,
- optionally produce a typed result.

Routines unify the callable model used across the language. The routine system is integrated into parsing, semantic type checking, and LLVM code generation.

#### 2.4.1) Routine definition

Use:

```lexico
define routine <name> [with args in take <typed_param_list>] returns <type> do
> <statements>
```

Canonical grammar rule:

```text
define routine IDENT opt_param_clause returns type do block
```

Every routine definition has:

- a name,
- an optional parameter list,
- an explicit return type,
- an indented block body,
- optional `giveback` for returning a value.

#### 2.4.2) Parameters

Parameter clause format:

```lexico
with args in take <type> <name>, <type> <name>, ...
```

Rules:

- Each parameter has a type and a name.
- Parameter names are local to routine scope.
- Call arguments must match parameter count and types.
- Parameter order is significant.

Example:

```lexico
define routine add with args in take int a, int b returns int do
> giveback a plus b;
```

#### 2.4.3) Return type and `giveback`

Return type is explicit in definition:

```lexico
returns <type>
```

Return statement:

```lexico
giveback <expr>;
```

Rules:

- `giveback` is valid only inside routine bodies.
- Return expression type must be compatible with declared return type.

#### 2.4.4) Calling routines

Side-effect style (discard result):

```lexico
emit <name> [with args in take <expr_list>];
```

Value style (use result in expressions):

```lexico
outcome of <name> [with args in take <expr_list>]
```

`outcome of` can appear in assignments, print arguments, arithmetic expressions, and nested calls.

Examples:

```lexico
emit add with args in take 1, 2;
create int z set value outcome of add with args in take 5, 7;
print outcome of add with args in take z, 3;
```

#### 2.4.5) No-argument routines

Example:

```lexico
define routine magic returns int do
> giveback 42;
```

Usage:

```lexico
print outcome of magic;
emit magic;
```

#### 2.4.6) Routine body

Routine bodies are normal Lexico blocks and can include declarations, assignments, prints, loops, conditionals, collection/matrix operations, input statements, and nested routine calls.

#### 2.4.7) Type behavior

Routine typing follows Lexico type rules (`int`, `float`, `char`, `string`, `bool`). Semantic checks validate argument types/count, infer expression types, and enforce return compatibility.

#### 2.4.8) Error conditions

Typical semantic errors:

- calling undeclared routine,
- wrong argument count,
- incompatible argument types,
- `giveback` type mismatch,
- incompatible use of routine result.

#### 2.4.9) Execution model

- Parsing builds routine AST nodes.
- Semantic phase registers routine signatures and validates bodies.
- Code generation emits LLVM functions and call sites.
- `emit` generates a call whose result is discarded.
- `outcome of` generates a call expression value.

#### 2.4.10) Syntax policy

- Global routines use `define routine ...`.
- Class member routines use `routine ...` inside `define class ... include` blocks.
- `define function ...` is not accepted.

#### 2.4.11) Class routines, inheritance, and override

Lexico supports a simple object-oriented class style:

```lexico
define class Animal include
> create string name;
> routine speak do
>> print "Animal", name;

define class Dog inherits Animal include
> override create string name;
> override routine speak do
>> print "Dog", name;
```

Rules:
- Class declaration: `define class <ClassName> include`.
- Class inheritance: `define class <ChildClass> inherits <ParentClass> include`.
- Properties: `create <type> <name>;`.
- Class routines: `routine <name> do ...` (or `routine <name> ... returns <type> do ...`).
- `override create <type> <name>;` replaces an inherited property.
- `override routine <name> ... do ...` replaces an inherited routine body/signature.
- Inherited classes gain parent properties and routines automatically.
- `override` fails if the named parent member does not exist.
- Profile names must use non-keyword identifiers (for example, use `custom` instead of `user`).
- Profiles are class-local and are not inherited automatically; define child profiles explicitly.

Validated integration example (`build/test1.lx` style):

```lexico
define class Shape include
> create string label;

> profile base do
>> set label to "shape";

> routine show do
>> print "Shape", label;

define class Circle inherits Shape include
> override create string label;
> create int radius;
> create int area;

> profile default do
>> set label to "circle";
>> set radius to 2;
>> set area to radius times radius times 3;

> profile custom do
>> set label to "user-circle";
>> set radius to take user input with message "Enter radius: ";
>> set area to radius times radius times 3;

> override routine show do
>> print "Circle", label, radius, area;

> routine describe returns int do
>> emit show;
>> giveback area;

create Shape s using base;
create Circle c1 using default;
create Circle c2 using custom;

emit s_show;
emit c1_show;

create int area1 set value outcome of c1_describe;
print area1;

emit c2_show;
```

With input `5`, expected output is:

```text
Enter radius: Shape shape
Circle circle 2 12
Circle circle 2 12
12
Circle user-circle 5 75
```

---

### 2.5) Mesh (canonical collection type)

Lexico now converges on a single collection type: `mesh`.

Language policy:
- Collection types are unified under `mesh` in canonical user-facing syntax.
- Internally, runtime may still use typed/dynamic storage strategies for performance and flexibility.

#### 2.5.1) Mesh declaration forms

Typed mesh (single primitive type):

```lexico
create int mesh nums sized 10;
create string mesh names sized n;
create float mesh prices sized base plus 5;
```

Dynamic mesh (mixed primitive types):

```lexico
create mesh mixed sized 12;
```

Auto-sized mesh initialization by value list:

```lexico
create int mesh nums set values 12,32,12,54,25;
create string mesh words set values "alpha","beta","gamma";
create mesh mixedAuto set values 12,"hi",'x',3.14,true;
```

Rules:
- `sized <expr>` accepts integer-compatible expressions (literal, variable, arithmetic expression).
- Typed mesh enforces value type compatibility on writes/appends.
- Dynamic mesh accepts mixed scalar values (`int`, `float`, `char`, `string`, `bool`).
- `set values` form infers mesh length from the number of provided values.
- For typed meshes, each provided value must be compatible with the mesh type.

#### 2.5.2) Access, size, and loops

Read by index:

```lexico
print nums at 2;
```

String character indexing uses the same `at` operator:

```lexico
create string title set value "lexico";
print title at 2;    # prints 'x'
```

Size expression:

```lexico
print size of nums;
set last to size of nums minus 1;

create string title set value "lexico";
print length of title;
```

String length expression:

```lexico
length of <string_expression>
```

Rules:
- `size of <meshname>` returns mesh length (`int`).
- `length of <string_expression>` returns string length (`int`).
- `length` accepts string literals, string variables, and string expressions.
- `length` on non-string expressions is a semantic error.
- `<string> at <index>` returns `char`; index must be `int` and in bounds.

Collection loop:

```lexico
for i in nums from 0 to size of nums minus 1 do
> print nums at i;
```

Step behavior:
- If `step` is omitted, default step is `step plus 1`.
- If `step` is specified, the loop follows it.
- `step` must be `int`-compatible and must not be zero.

#### 2.5.3) Set forms (canonical)

Outside mesh loop context:

```lexico
set <meshname> value at <position> to <expression>;
```

Inside bound mesh loop context:

```lexico
set value to <expression>;
```

Context rule:
- `set value to ...` is valid only inside `for i in <meshname> ...` blocks.
- Inside loop context, it targets the bound mesh at the current loop index (`i`).
- Inside loop context, forms with explicit mesh name or explicit position are rejected.

#### 2.5.4) Append forms (canonical)

Inside bound mesh loop context:

```lexico
append <value>;
```

Outside loop context:

```lexico
append <value> to <meshname>;
append <value> to <meshname> at <position>;
```

Append semantics:
- `append <value>;` appends at end of the bound mesh.
- `append <value> to <meshname>;` appends at end.
- `append <value> to <meshname> at <position>;` inserts at position and shifts following values to the right.
- Inside loop context, append forms that mention mesh name or position are rejected.
- For typed `string` meshes, appended/set values are coerced from scalar input:
  - `int` -> decimal text (`55` -> `"55"`)
  - `float` -> text form (`8.25` -> `"8.25"`)
  - `char` -> one-character string (`'z'` -> `"z"`)
  - `bool` -> `"true"` / `"false"`
- For typed `char` meshes, appended/set values are coerced to a single character:
  - `char` stays char
  - `string` takes first character (or `\0` for empty string)
  - `int`/`float` cast to char code
  - `bool` maps to `1` or `0` char code

#### 2.5.5) Print behavior

Predefined compact mesh print is supported for mesh identifiers in print context:

```lexico
print nums;
```

Canonical shape:

```text
nums[3]
idx   val
0     10
1     20
2     30
```

#### 2.5.6) Count expression (`count <value> in <meshname>`)

Lexico provides a mesh-count expression that returns how many elements in a mesh are equal to a target value.

Canonical syntax:

```lexico
count <value_expr> in <meshname>
```

This is an expression, so it can be used anywhere expressions are accepted:

```lexico
print count 3 in nums;
set total to count "ok" in words;
if count i in nums is greater than 0 then
> print "found";
```

Allowed `<value_expr>` forms:

- integer literal (`20`)
- float literal (`12.35`)
- char literal (`'a'`)
- string literal (`"hello"`)
- identifier/variable (`x`, `target`)
- arithmetic expression (`a plus 2`, `(x times 2) minus 1`)
- any expression that resolves to a scalar value

Return type:

- always `int`

Matching behavior:

- For typed meshes (`create int mesh ...`, `create string mesh ...`, etc.):
  - For non-text meshes (`int`, `float`, `bool`) `<value_expr>` must be type-compatible.
  - For `string` and `char` meshes, scalar values are accepted and coerced using the same text rules as append/set.
  - count compares each element against the evaluated `<value_expr>` and returns number of matches.
- For dynamic meshes (`create mesh ...`):
  - runtime compares both type tag and value.
  - values of different tags are not equal (for example, `1` is not equal to `'1'` and not equal to `"1"`).

String and char behavior:

- string comparison is exact and case-sensitive.
- char comparison is exact by codepoint/byte value.

Float behavior:

- floating-point matches use exact runtime equality of stored values.
- if approximate matching is needed, normalize values before insertion/counting.

Evaluation notes:

- `<value_expr>` is evaluated once for the `count` expression.
- mesh traversal scans current mesh length from index `0` to `size of <meshname> minus 1`.

Examples:

```lexico
create int mesh nums sized 6;
set nums value at 0 to 3;
set nums value at 1 to 7;
set nums value at 2 to 3;
set nums value at 3 to 1;
set nums value at 4 to 3;
set nums value at 5 to 9;

print count 3 in nums;            # 3

create int target set value 7;
print count target in nums;       # 1
print count target minus 4 in nums; # 3
```

Invalid usage examples:

```lexico
print count 3 in unknownMesh;      # undeclared mesh
print count "x" in intMesh;       # type mismatch for typed mesh
```

#### 2.5.7) Membership expression (`check <value> in <meshname>`)

Lexico provides a mesh-membership expression that returns whether a target value exists in a mesh.

Canonical syntax:

```lexico
check <value_expr> in <meshname>
```

This is an expression, so it can be used anywhere expressions are accepted:

```lexico
print check 3 in nums;
set exists to check target in nums;
if check i plus 1 in nums then
> print "present";
```

Allowed `<value_expr>` forms:

- integer, float, char, string, and bool literals
- identifiers/variables
- arithmetic expressions
- any scalar expression accepted by the language

Return type:

- always `bool` (`true` if found at least once, otherwise `false`)

Matching behavior:

- For typed meshes:
  - For non-text meshes (`int`, `float`, `bool`) `<value_expr>` must be type-compatible.
  - For `string` and `char` meshes, scalar values are accepted and coerced using the same text rules as append/set.
  - membership is true if at least one equal element exists.
- For dynamic meshes:
  - runtime compares both type tag and value.
  - values with different tags are not equal (`1`, `'1'`, `"1"`, and `true` are all distinct).

Evaluation notes:

- `<value_expr>` is evaluated once per `check` expression.
- mesh traversal scans from index `0` to `size of <meshname> minus 1`.
- implementation-wise, `check` is equivalent to `count ... in ... is greater than 0`.

Examples:

```lexico
create int mesh nums sized 5;
set nums value at 0 to 2;
set nums value at 1 to 4;
set nums value at 2 to 6;
set nums value at 3 to 8;
set nums value at 4 to 10;

create int needle set value 6;
print check 6 in nums;             # true
print check needle in nums;        # true
print check needle plus 1 in nums; # false
```

Bool mesh example:

```lexico
create bool mesh flags sized 3;
set flags value at 0 to true;
set flags value at 1 to false;
set flags value at 2 to true;

print check true in flags;   # true
print check false in flags;  # true
```

Invalid usage examples:

```lexico
print check 3 in unknownMesh;      # undeclared mesh
print check "x" in intMesh;       # type mismatch for typed mesh
```

#### 2.5.8) Position expression (`position of <value> in <meshname>`)

Lexico provides a position lookup expression for meshes.

Canonical syntax:

```lexico
position of <value_expr> in <meshname>
```

Return type:

- always `int`
- returns first matching index
- returns `-1` when no match is found

Matching behavior:

- For typed meshes:
  - non-text meshes (`int`, `float`, `bool`) require type-compatible values.
  - `string` and `char` meshes accept scalar values and coerce them using the same rules as append/set.
- For dynamic meshes:
  - runtime compares both type tag and value.

Examples:

```lexico
print position of "banana" in words;
print position of 55 in labels;
print position of 'z' in labels;
print position of 999 in nums;   # -1 when missing
```

#### 2.5.9) Sort statement (`sort <meshname> ascendantly|descendantly;`)

Lexico supports in-place sorting for typed meshes.

Canonical syntax:

```lexico
sort <meshname> ascendantly;
sort <meshname> descendantly;
```

Rules:

- Sort works only on typed meshes (`create <type> mesh ...`).
- Sort is rejected for dynamic meshes (`create mesh ...`).
- Supported element types: `int`, `float`, `char`, `string`.

Type-specific ordering behavior:

- `int`: numeric ascending/descending.
- `char`: character code order.
- `string`: compares first character first, then full string as tie-breaker.
- `float`: compares fractional part first, then full value as tie-breaker.

Examples:

```lexico
sort nums ascendantly;
sort words descendantly;
```

#### 2.5.10) Boolean type (`bool`) details

Lexico now supports a first-class boolean scalar type.

Declaration and assignment:

```lexico
create bool ok set value true;
set ok to false;
```

Default initialization:

```lexico
create bool ready;
print ready;    # false
```

Literals:

- `true`
- `false`

Where booleans are valid:

- scalar declarations and assignments
- mesh declarations and mesh writes/appends (`create bool mesh ...`, `set ... to true/false`, `append true ...`)
- print arguments
- `count true in <mesh>` and `check true in <mesh>`

Type rules:

- bool values are not numeric in arithmetic expressions.
- using bool directly with `plus`, `minus`, `times`, `divided by`, or `mod` is a semantic error.
- unary minus on bool is a semantic error.

Input behavior:

- scalar/mesh bool input is parsed as integer-like input and normalized to boolean:
  - `0` => `false`
  - non-zero => `true`

Print behavior:

- bool values print as textual `true` / `false`.

#### 2.5.11) Runtime model and safety

Mesh storage remains pointer-backed and dynamically managed:
- Explicit `len` and `cap` tracking.
- Checked growth with safe `realloc` logic.
- Bounds checks on every indexed read/write.
- Clear ownership for heap-managed strings.

#### 2.5.12) Compatibility note

- Canonical documentation is mesh-first.
- If legacy collection parsing exists during transition, treat it as compatibility syntax and normalize to mesh semantics.

## 2.6) File system (implemented)

Lexico includes a first-class file block model where file operations are explicit statements and all normal Lexico statements can run inside the block.

### 2.6.1) Design model

- A file is activated by `open ... file ... do`.
- File operations are valid only while an active file context exists.
- Inside a file block, you can still use normal language constructs (`create`, `set`, `print`, loops, conditionals, routines, collection/matrix ops, etc.).

### 2.6.2) Open forms

```lexico
open <path_expr> file do
> ...

open <path_expr> file make do
> ...
```

Rules:
- `<path_expr>` must be a `string` expression.
- `open ... file do` requires the file to already exist.
- `open ... file make do` creates the file if it does not exist.
- At block end, the compiler/runtime closes the active file automatically.

### 2.6.3) Write operations

```lexico
write <string_expr> in file;
write <string_expr> in line <int_expr> in file;
```

Behavior:
- `write ... in file` appends a new line at the end.
- `write ... in line n in file` writes/replaces line `n`.
- Line indexing is zero-based.
- If `n` is beyond current line count, the file is expanded with empty lines until line `n` exists.

Type rules:
- Written value must be `string`.
- `line` index must be `int`.

### 2.6.4) Read operations

```lexico
read file into <target>;
read line <int_expr> in file into <target>;
read <int_expr> lines in file into <target>;
read <int_expr> lines from line <int_expr> in file into <target>;
```

Behavior:
- `read file into t` reads all lines.
- `read line n ...` reads one line.
- `read k lines ...` reads first `k` lines.
- `read k lines from line s ...` reads a range window starting at `s`.

Target typing:
- Single-line read target must be `string` scalar.
- Multi-line read targets must be `table` (runtime stores each line as string cell).

Index/count typing:
- `line`, `from line`, and `lines` count expressions must be `int`.

### 2.6.5) Clear operations

```lexico
clear file;
clear line <int_expr> in file;
clear from line <int_expr> to line <int_expr> in file;
```

Behavior:
- `clear file` truncates the file to empty.
- `clear line n` removes line `n` and shifts following lines upward.
- `clear from line a to line b` removes an inclusive range and shifts remaining lines.
- Out-of-range clear operations are treated safely (no memory corruption; the runtime keeps valid file state).

### 2.6.6) File title (rename)

```lexico
set file title to <string_expr>;
```

Behavior:
- Renames the active file path using host rename semantics.
- On OS rename failure (for example target already exists or permissions), runtime reports an error and execution fails.

### 2.6.7) Close operation

```lexico
close file;
```

Behavior:
- Closes the active file handle in the current file context.
- End-of-block auto-close still runs; implementation is idempotent-safe.

### 2.6.8) Context and semantic restrictions

- `write/read/clear/set file title/close file` are valid only inside an active file block.
- Using those statements outside file context raises semantic errors.
- File path must be `string`.
- Numeric indices/counts must be `int`.

### 2.6.9) Keyword and identifier note

The lexer reserves these file-related keywords:
- `open`, `make`, `file`, `write`, `read`, `line`, `lines`, `clear`, `begin`, `close`, `title`, `into`

Avoid using these as identifiers (for example, prefer `file_rows` instead of `lines`).

### 2.6.10) End-to-end example

```lexico
create string one_line set value "";
create table rows with size 0;

open "notes.txt" file make do
> clear file;
> write "alpha" in file;
> write "beta" in file;
> write "BETA-UPDATED" in line 1 in file;
>
> read line 1 in file into one_line;
> print one_line;
>
> read file into rows;
> print rows;
>
> clear line 0 in file;
> read file into rows;
> print rows;
>
> set file title to "notes_renamed.txt";

open "notes_renamed.txt" file do
> read file into rows;
> print rows;
```

## 2.7) Module loading with `load` (implemented)

Lexico supports source-level module loading so one `.lx` file can include and use definitions from other `.lx` files.

### 2.7.1) Syntax

```lexico
load "<path>.lx";
```

Example:

```lexico
load "lib/classes.lx";
load "lib/routines.lx";

create Dog d using default;
emit d_bark;
print outcome of add with args in take 4, 6;
```

### 2.7.2) What can be loaded

`load` is generic. Loaded files may contain any valid top-level Lexico code, including:

- class declarations,
- routine declarations,
- declarations and statements.

Loaded symbols are available to the loading file as part of one compilation unit.

### 2.7.3) Path rules

- Loaded target must have `.lx` extension.
- Relative paths are resolved from the directory of the file that contains the `load` line.
- Absolute paths are accepted.

### 2.7.4) Resolution behavior

- Duplicate loads are ignored (same normalized path is loaded once).
- Circular loads are rejected with a compiler error.
- Expansion is recursive: if `a.lx` loads `b.lx`, and `b.lx` loads `c.lx`, all are merged.

### 2.7.5) Best practices

- Put `load` statements at top-level, usually near the beginning of the file.
- Keep reusable classes/routines in library-style files.
- Keep entry logic in a separate main file.

## 3) Canonical grammar (single source of truth)

Use the following as the authoritative grammar outline for implemented language syntax.

```bison
program
  : /* empty */
  | stmt_list
  ;

stmt_list
  : stmt
  | stmt_list stmt
  ;

stmt
  : load_stmt ';'
  | decl_stmt ';'
  | print_stmt ';'
  | set_stmt ';'
  | convert_stmt ';'
  | emit_stmt ';'
  | giveback_stmt ';'
  | compound_stmt ';'
  | function_stmt
  | class_stmt
  | file_open_stmt
  | file_stmt ';'
  | if_stmt
  | while_stmt
  | for_stmt
  | repeat_stmt
  | attempt_stmt
  ;

load_stmt
  : LOAD STRING
  ;

file_open_stmt
  : OPEN expr FILE_T DO block
  | OPEN expr FILE_T MAKE DO block
  ;

file_stmt
  : WRITE expr IN FILE_T
  | WRITE expr IN LINE expr IN FILE_T
  | READ FILE_T INTO IDENT
  | READ LINE expr IN FILE_T INTO IDENT
  | READ expr LINES IN FILE_T INTO IDENT
  | READ expr LINES FROM LINE expr IN FILE_T INTO IDENT
  | CLEAR FILE_T
  | CLEAR LINE expr IN FILE_T
  | CLEAR FROM LINE expr TO LINE expr IN FILE_T
  | SET FILE_T TITLE TO expr
  | CLOSE FILE_T
  ;

function_stmt
  : DEFINE ROUTINE IDENT opt_param_clause RETURNS type DO block
  ;

opt_param_clause
  : /* empty */
  | WITH ARGS IN TAKE param_list
  ;

param_list
  : type IDENT
  | param_list ',' type IDENT
  ;

emit_stmt
  : EMIT IDENT opt_call_args
  ;

giveback_stmt
  : GIVEBACK expr
  ;

opt_call_args
  : /* empty */
  | WITH ARGS IN TAKE call_args
  | WITH ARGS IN TAKE '(' call_args ')'
  ;

call_args
  : expr
  | call_args ',' expr
  ;

decl_stmt
  : CREATE IDENT IDENT USING IDENT
  | CREATE type id_list SET VALUE val_list
  | CREATE type id_list
  | CREATE type IDENT SET VALUE TAKE USER INPUT opt_message
  ;

class_stmt
  : DEFINE CLASS IDENT INCLUDE INDENT class_body DEDENT
  | DEFINE CLASS IDENT INHERITS IDENT INCLUDE INDENT class_body DEDENT
  ;

class_body
  : class_member
  | class_body class_member
  ;

class_member
  : CREATE type IDENT ';'
  | OVERRIDE CREATE type IDENT ';'
  | PROFILE IDENT DO INDENT profile_body DEDENT
  | ROUTINE IDENT opt_param_clause RETURNS type DO block
  | ROUTINE IDENT opt_param_clause DO block
  | OVERRIDE ROUTINE IDENT opt_param_clause RETURNS type DO block
  | OVERRIDE ROUTINE IDENT opt_param_clause DO block
  ;

profile_body
  : profile_line
  | profile_body profile_line
  ;

profile_line
  : SET IDENT TO expr ';'
  | SET IDENT TO TAKE USER INPUT opt_message ';'
  ;

opt_message
  : /* empty */
  | WITH MESSAGE message_parts
  ;

message_parts
  : expr
  | message_parts ',' expr
  ;

print_stmt
  : PRINT print_args
  ;

print_args
  : expr
  | print_args ',' expr
  ;

set_stmt
  : SET IDENT TO expr
  | SET IDENT TO TAKE USER INPUT opt_message
  ;

convert_stmt
  : CONVERT IDENT TO type
  ;

compound_stmt
  : IDENT PLUS  expr
  | IDENT MINUS expr
  | IDENT TIMES expr
  | IDENT DIV   expr
  | IDENT MOD   expr
  ;

if_stmt
  : IF condition THEN block
  | IF condition THEN block else_part
  ;

else_part
  : OTHERWISE block
  | OTHERWISE WHEN condition THEN block
  | OTHERWISE WHEN condition THEN block else_part
  ;

while_stmt
  : WHILE condition DO block
  ;

for_stmt
  : FOR IDENT FROM expr TO expr DO block
  | FOR IDENT FROM expr TO expr STEP expr DO block
  | FOR IDENT IN IDENT FROM expr TO expr DO block
  | FOR IDENT IN IDENT FROM expr TO expr STEP expr DO block
  ;

repeat_stmt
  : REPEAT block UNTIL condition THEN
  ;

attempt_stmt
  : ATTEMPT UPTO expr WHILE condition DO block
  | ATTEMPT UPTO expr WHILE condition DO block ONFAILURE block
  | ATTEMPT UPTO expr TIMES WHILE condition DO block
  | ATTEMPT UPTO expr TIMES WHILE condition DO block ONFAILURE block
  ;

condition
  : cond_or
  ;

cond_or
  : cond_or OR cond_and
  | cond_and
  ;

cond_and
  : cond_and AND cond_not
  | cond_not
  ;

cond_not
  : NOT cond_atom
  | cond_atom
  ;

cond_atom
  : expr CMP_EQ  expr
  | expr CMP_NEQ expr
  | expr CMP_GT  expr
  | expr CMP_LT  expr
  | expr CMP_GTE expr
  | expr CMP_LTE expr
  | '(' condition ')'
  ;

expr
  : expr PLUS term
  | expr MINUS term
  | term
  ;

term
  : term TIMES factor
  | term DIV   factor
  | term MOD   factor
  | factor
  ;

factor
  : call_expr
  | IDENT AT factor
  | SIZE OF IDENT
  | LENGTH OF factor
  | COUNT expr IN IDENT
  | CHECK expr IN IDENT
  | POSITION OF expr IN IDENT
  | IDENT
  | literal
  | '(' expr ')'
  | MINUS factor
  | PLUS factor
  ;

call_expr
  : OUTCOME OF IDENT opt_call_args
  ;
```

Notes:
- `for` iterator is loop-local and implicit.
- `create <type> <id_list>;` is valid and uses typed defaults.
- User input is unified under `take user input` forms.
- Routines use `define routine` (or `routine` inside class bodies), return with `giveback`, and invoke with `emit` or `outcome of`.
- `>`-based blocks map to lexer-emitted `INDENT`/`DEDENT` tokens.

### 3.1) Approved grammar extension (mesh + contextual set/append)

The following grammar extension defines the mesh-first collection syntax.

```bison
stmt
  : mesh_decl ';'
  | mesh_set ';'
  | mesh_set_ctx ';'
  | mesh_append ';'
  | mesh_append_ctx ';'
  | ...existing stmt forms...
  ;

mesh_decl
  : CREATE type MESH IDENT SIZED expr
  | CREATE type MESH IDENT SET VALUES mesh_init_values
  | CREATE MESH IDENT SIZED expr
  | CREATE MESH IDENT SET VALUES mesh_init_values
  ;

mesh_set
  : SET IDENT VALUE AT expr TO expr
  | SET IDENT VALUE AT expr TO TAKE USER INPUT opt_message
  ;

mesh_set_ctx
  : SET VALUE TO expr
  | SET VALUE TO TAKE USER INPUT opt_message
  ;

mesh_append
  : APPEND expr TO IDENT
  | APPEND expr TO IDENT AT expr
  ;

mesh_sort
  : SORT IDENT ASCENDANTLY
  | SORT IDENT DESCENDANTLY
  ;

mesh_append_ctx
  : APPEND expr
  ;

for_stmt
  : FOR IDENT FROM expr TO expr DO block
  | FOR IDENT FROM expr TO expr STEP expr DO block
  | FOR IDENT IN IDENT FROM expr TO expr DO block
  | FOR IDENT IN IDENT FROM expr TO expr STEP expr DO block
  ;

factor
  : IDENT AT factor
  | SIZE OF IDENT
  | LENGTH OF factor
  | COUNT expr IN IDENT
  | CHECK expr IN IDENT
  | POSITION OF expr IN IDENT
  | ...existing factor forms...
  ;
```

Semantic constraints for this extension:
- `mesh_set_ctx` and `mesh_append_ctx` are only valid when a bound mesh context exists from `for IDENT IN IDENT FROM ... TO ...`.
- `SET VALUE TO expr` in context targets the current loop index of the bound mesh.
- In context, explicit mesh-name or explicit-position forms are rejected (`set <mesh> ...`, `set value at ...`, `append ... at ...`, `append ... to <mesh> ...`).
- `APPEND expr TO IDENT AT expr` inserts and shifts following values to the right.
- `mesh_sort` works only on typed meshes and supports `int`, `float`, `char`, and `string`.
- `COUNT expr IN IDENT` is a scalar expression that returns the number of occurrences of evaluated `expr` in the target mesh.
- `CHECK expr IN IDENT` is a scalar expression that returns `true` if at least one occurrence of evaluated `expr` exists in the target mesh, else `false`.
- `POSITION OF expr IN IDENT` is a scalar expression that returns first index or `-1` if not found.
- `LENGTH OF factor` expects string expression input; `SIZE OF IDENT` is for mesh length.

---

## 4) Implementation notes (current codebase, explicit)

This section is the file-by-file source of truth for compiler behavior.

### 4.1) End-to-end pipeline

1. Entry validation
- `src/main.cpp` validates argument count and `.lx` extension.

2. Driver orchestration
- `src/driver.cpp` performs load-preprocessing, then parse, semantic analysis, codegen, and execution.

3. Frontend
- `src/lexico.l` tokenizes canonical Lexico input.
- `src/lexico.y` builds AST through Bison actions.

4. Semantic stage
- `src/symtab.cpp` validates declarations, types, function signatures, collection constraints, and statement context.

5. Backend
- `src/codegen.cpp` emits LLVM IR, declares runtime helpers, maps native functions for JIT, and executes.

6. AST lifecycle
- `src/ast.hpp` defines node shapes.
- `src/ast.cpp` allocates, clones, and frees all nodes.

### 4.2) Core files and responsibilities

- `src/main.cpp`
  - CLI entry point.
  - User-facing usage and extension checks.

- `src/driver.cpp`
  - Pipeline control (`lexico_compile`).
  - Recursive `load "...";` expansion with:
    - duplicate suppression,
    - cycle detection,
    - relative-path resolution,
    - `.lx` enforcement.
  - Safe cleanup on error paths.

- `src/lexico.l`
  - Keyword and literal tokenization.
  - Multi-word token handling (example: `divided by`).
  - Indentation-sensitive block token support (`INDENT`/`DEDENT`) for `>` block style.

- `src/lexico.y`
  - Canonical grammar.
  - Expression precedence and statement forms.
  - AST construction for declarations, conversion, routines, loops, classes, file operations, and collections.

- `src/ast.hpp` and `src/ast.cpp`
  - Type system (`TYPE_INT`, `TYPE_FLOAT`, `TYPE_CHAR`, `TYPE_STRING`, `TYPE_BOOL`).
  - Expression kinds (including `EXPR_CONVERT`, `EXPR_COUNT`, `EXPR_CHECK`, `EXPR_POSITION`).
  - Statement kinds (declarations, assignment, conversion, loops, routines, file operations, matrix/mesh operations).
  - Ownership-safe free routines.

- `src/symtab.cpp`
  - Symbol registration and duplicate detection.
  - Type inference and compatibility checks.
  - Context constraints for operations that are only valid inside specific blocks.
  - Conversion validation (`convert` statement and expression paths).

- `src/codegen.cpp`
  - LLVM type mapping and value coercion.
  - IR emission for expressions/statements.
  - Runtime helper declarations and definitions.
  - JIT function mapping (must include every helper that IR may call).

### 4.3) Runtime conventions and ABI mapping

- Lexico scalar to LLVM:
  - `int` -> `i64`
  - `float` -> `double`
  - `char` -> `i8`
  - `string` -> `i8*`
  - `bool` -> `i1`

- Print behavior:
  - Numeric and char printed via type-specific formats.
  - Bool printed as human text (`true` or `false`).

- Input behavior:
  - Scalar input uses type-aware scanning/parsing.
  - String-to-numeric conversion should fail with explicit runtime diagnostics.

### 4.4) Conversion feature implementation map

- Grammar layer
  - Statement conversion: `convert <identifier> to <type>;`
  - Expression conversion: `convert <expr> to <type>`

- AST layer
  - `STMT_CONVERT`
  - `EXPR_CONVERT`

- Semantic layer
  - Target type validation.
  - Source/target compatibility checks.
  - In-place symbol type update for statement conversion.

- Codegen layer
  - Runtime helpers for:
    - numeric/char/bool to string,
    - string to int/float/char,
    - conversion in expression context.
  - JIT global mappings must include every conversion helper.

### 4.5) Module loading implementation map

- Driver preprocess pass recognizes exact one-line form:
  - `load "path.lx";`
- Loads recursively before parse.
- Prevents re-loading same normalized path.
- Detects circular loads and aborts with explicit error.

### 4.6) Error model and diagnostics priorities

Diagnostic order should stay stable:

1. Lexical errors (invalid tokens, malformed literals)
2. Parse errors (unexpected token, malformed statement)
3. Semantic errors (unknown symbol, type mismatch, invalid context)
4. Runtime errors (invalid conversion input, out-of-range access, file failures)

Recommended diagnostic payload:

- stage (`lexer`, `parser`, `semantic`, `runtime`)
- file path
- line number
- concise reason
- one actionable fix hint

### 4.7) RAG retrieval anchors for implementation questions

Use these anchors when indexing this section:

- `pipeline-orchestration` -> `src/main.cpp`, `src/driver.cpp`
- `grammar-and-parser` -> `src/lexico.l`, `src/lexico.y`
- `ast-model` -> `src/ast.hpp`, `src/ast.cpp`
- `type-and-symbol-checks` -> `src/symtab.cpp`
- `llvm-backend-and-jit` -> `src/codegen.cpp`
- `load-preprocessing` -> `src/driver.cpp`
- `convert-feature` -> `src/lexico.y`, `src/ast.hpp`, `src/symtab.cpp`, `src/codegen.cpp`

These anchors are intentionally redundant so tolerant retrieval can still hit correct content.

---

## 5) Build and verification (Windows)

Recommended toolchain:
- MSYS2 (`g++`, `bison`, `flex`) + LLVM C API

Typical build flow:

```powershell
$env:PATH = "C:\msys64\mingw64\bin;C:\msys64\usr\bin;" + $env:PATH

C:\msys64\usr\bin\bison.exe -d --defines=src/lexico.tab.hpp -o src/lexico.tab.cpp src/lexico.y
C:\msys64\usr\bin\flex.exe  -o src/lexico.lex.cpp src/lexico.l

C:\msys64\mingw64\bin\g++.exe -std=c++17 -Wall -Wextra -fpermissive `
  "-IC:\PROGRA~1\LLVM\include" -Isrc -D_GNU_SOURCE `
  -o build\lexico.exe `
  src\main.cpp src\driver.cpp src\ast.cpp src\symtab.cpp src\codegen.cpp src\lexico.tab.cpp src\lexico.lex.cpp `
  "-LC:\PROGRA~1\LLVM\lib" -lLLVM-C
```

---

## 16) Memory safety and security requirements

The Lexico compiler must be implemented with a **strong focus on memory safety and robustness**.

### General principles
- Always check the result of `malloc` / `calloc` / `realloc` and handle out-of-memory conditions gracefully (print an error and abort compilation cleanly).
- Define clear ownership for every heap allocation (who allocates, who frees) and avoid shared mutable ownership without a clear convention.
- Avoid fixed-size buffers for input where lengths are unbounded; prefer dynamically sized allocations based on actual input size.
- Do not use inherently unsafe C functions such as `gets`; use `fgets` or equivalent and always limit reads by buffer size.
- Centralize cleanup: provide a single shutdown path that frees the AST, symbol table, strings, and LLVM objects even on error.

### Parsing and AST
- Limit maximum token length and reject overlong identifiers/strings with a clear error instead of risking buffer overflows.
- When copying lexemes (identifiers, string contents) use length-aware copying (`strndup`-style helpers) and always ensure null termination.
- Ensure every AST node and string allocated during parsing is freed after code generation or when a parse/semantic error aborts compilation.

### Symbol table
- Use dynamically growing arrays or other containers that track `size` and `capacity`; never write beyond allocated bounds.
- Validate indices and pointers before dereferencing; never assume they are valid if they come from external or computed data.
- Reject duplicate declarations explicitly instead of overwriting entries, and report them with precise locations.

### LLVM integration
- Check for errors from LLVM verification functions (e.g., module verification in debug builds) and abort code generation if the IR is invalid.
- On unrecoverable LLVM errors, print a diagnostic and free compiler data structures before exiting to avoid leaks.

### Input and executable behavior
- The compiler executable must only accept input files with the `.lx` extension by default; other extensions should be rejected with a clear message.
- Treat `.lx` files strictly as data; never execute code obtained from source files directly within the compiler process.
- If temporary files are used (e.g., for `.ll` output), choose non-conflicting names and delete them after use when possible.

### Defensive compilation and testing
- During development, compile with strict warnings enabled and treat them as errors (e.g., `-Wall -Wextra -Werror` or `/W4`).
- Regularly run the compiler under tools like AddressSanitizer (on compatible platforms) or Valgrind (Linux/WSL) to detect leaks and invalid memory access.
- Maintain regression tests for known parsing, semantic, and runtime edge cases to prevent reintroducing memory-related bugs.

---

## 17) Matrix support (implemented)

Lexico now supports fixed-size typed and untyped 2D mesh matrices.

### Declaration

```lexico
create <type> matrix mesh <name> rows <expr> cols <expr>;
create matrix mesh <name> rows <expr> cols <expr>;
create <type> matrix mesh <name> set values <row_values> / <row_values> ...;
create matrix mesh <name> set values <row_values> / <row_values> ...;
```

Examples:

```lexico
create int matrix mesh m rows 2 cols 3;
create string matrix mesh labels rows 4 cols 2;
create matrix mesh mixed rows 2 cols 2;
create int matrix mesh a set values 1,2,3 / 4,5,6;
create matrix mesh dm set values "A",42 / true,'z';
```

Rules:
- `<expr>` for `rows` and `cols` must be `int`-compatible.
- Dimensions must be non-negative at runtime.
- In typed form, matrix element type is fixed (`int`, `float`, `char`, `string`, or `bool`).
- In untyped form (`create matrix mesh ...`), each cell can store a different scalar type at runtime.
- In `set values ... / ...` form, all rows must have the same number of values.
- `set value` and `set values` are both accepted for matrix inline initialization.

### Set and read/index access

```lexico
set <name> at <row_expr>,<col_expr> to <value_expr>;
set <value_expr> to <name> at <row_expr>,<col_expr>;
set <value_expr> at <row_expr>,<col_expr>;

set <var_name> to <name> at <row_expr>,<col_expr>;
set <var_name> at <row_expr>,<col_expr>;

<name> at <row_expr>,<col_expr>
```

Examples:

```lexico
set m at 1,2 to 6;
set 6 to m at 1,2;
set labels at 0,1 to "ok";
set labels at 1,0 to 7;

set x to m at 2,1;

for i j in m i from 0 to 2 and j from 0 to 2 do
> set i plus j at i,j;    # contextual matrix write
> set x at i,j;           # contextual matrix read
```

Semantic/runtime behavior:
- `<row_expr>` and `<col_expr>` must be `int`-compatible.
- Indexing is zero-based.
- Out-of-bounds access raises a runtime error.
- For non-text matrix types, assigned values must be type-compatible.
- For `string` and `char` matrices, scalar values are coerced with the same text coercion rules used for text meshes.
- For untyped matrices, set values are accepted from any scalar type.
- `set <value_expr> at <row>,<col>` (contextual matrix write) is valid only inside a matrix loop bound to a matrix.
- `set <var_name> at <row>,<col>` (contextual matrix read) is valid only inside a matrix loop bound to a matrix.
- Current limitation: direct scalar indexing from untyped matrices in expressions is rejected by semantics (printing the matrix itself is supported).

### Matrix predefined functions

The following predefined expressions are implemented:

- `size of <matrix_name>` returns total occupied slots (`len`), including appended values.
- `row length of <matrix_name>` returns effective row count.
- `column length of <matrix_name>` returns matrix column count.

Examples:

```lexico
print size of m;
print row length of m;
print column length of m;
```

### Size and print

- `print <matrix_name>;` prints a tabular ASCII view row-by-row.

If append operations produce a partially filled trailing row, print shows `<empty>` cells in remaining columns of that row.

Example output shape:

```text
m[2x3]
1       2       3
4       5       6
```

With appended values beyond original dimensions:

```text
m[4x3]
1       2       3
4       5       6
7       8       9
10      <empty> <empty>
```

### Count and check on matrix mesh

Matrix meshes support the same membership expressions as meshes/tables:

```lexico
count <value_expr> in <matrix_name>
check <value_expr> in <matrix_name>
```

Examples:

```lexico
print count 2 in m;
set ok to check 8 in m;
print count true in dm;
set ok to check "done" in dm;
```

Type behavior follows matrix write compatibility rules (typed matrices enforce type compatibility; untyped matrices accept mixed scalar values).

### Append on matrix mesh

Matrix meshes support explicit append forms:

```lexico
append <value_expr> to <matrix_name>;
append <value_expr> to <matrix_name> at <index_expr>;
```

Examples:

```lexico
append 10 to m;
append 11 to m at 1;

append "later" to dm;
append "first" to dm at 0;
```

Behavior:
- Append uses linear row-major ordering over matrix storage.
- `append ... to <matrix>;` appends at end.
- `append ... to <matrix> at <index>;` inserts at index and shifts following values to the right.
- Effective matrix row count grows as needed: `effective_rows = ceil(size / cols)`.
- Appending to typed matrices enforces typed compatibility (with string/char coercion rules).
- Appending to untyped matrices accepts mixed scalar values.

### Matrix loop

You can iterate matrix coordinates without declaring loop variables first:

```lexico
for i j in m i from 0 to 1 and j from 0 to 2 do
> /* body */
```

Behavior:
- `i` is the row iterator and `j` is the column iterator.
- Both are implicit `int` loop-local variables (same idea as normal loop iterator scope).
- Iteration order is row-major: `(row_from,col_from) .. (row_to,col_to)` with row major traversal.
- The loop target must be a declared matrix mesh.
- Row loop uses `i from <row_from> to <row_to>` and column loop uses `j from <col_from> to <col_to>` (inclusive bounds).
- Bare form `for i j in m do` is intentionally not supported; bounded form is required.

---

## 18) Appendix: Deduplicated Expansion Policy

The previous generated expansion sections were removed because they contained high repetition and low information density.

Use this policy for future documentation expansion:

- Add new sections only when they contain feature-specific examples tied to existing grammar and implementation files.
- Avoid template-only repeated bullets across many headings.
- For diagnostics, prefer a small curated catalog with real Lexico snippets instead of placeholder patterns.
- For RAG cards, include only high-value cards that differ by syntax, semantics, or runtime behavior.
- Keep one source-of-truth explanation per feature and link to implementation anchors.

If needed, detailed appendices can be re-added incrementally with concrete, non-repetitive examples.

