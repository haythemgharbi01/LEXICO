## Execution basis

- Repository: current `Downloads/LEXICO` checkout.
- Compiler: existing `SourceCode/Applications/lexico.exe`.
- Build/rebuild: not performed.
- Test invocations: 52 total — 26 baseline and 26 `--autofix`.
- Harness: `research/evaluate_paper.py`.
- Raw data: `research/evaluation_results.csv`.
- API key: inherited from the process environment; not stored or printed.

## Mutation manifest

The run contains the paper's 26 cases:

| Category | Cases |
|---|---:|
| Missing semicolon | 5 |
| Keyword case | 5 |
| Multi-word operator corruption | 6 |
| Layout/indentation mismatch | 5 |
| File-context violations | 5 |
| **Total** | **26** |

## Research-paper reported results

| Metric | Paper value |
|---|---:|
| Evaluation dataset | 52 simulated trials derived from 26 mutations |
| Baseline success rate | 57.69% |
| Grounded-AI success rate | 96.15% |
| Baseline average latency | <1.0 s |
| Grounded-AI average latency | approximately 2.4 s |
| Baseline cost per fix | $0.00 |
| Grounded-AI cost per fix | <$0.01 |

These are the values reported in the supplied `LEXICO_Research_Paper.pdf`.

## Category timings

| Category | Cases | Baseline total | Autofix total | Successful runs |
|---|---:|---:|---:|---:|
| Missing semicolon | 5 | 0.113 s | 1.889 s | 1 |
| Keyword case | 5 | 0.128 s | 2.196 s | 0 |
| Multi-word operator | 6 | 0.189 s | 2.958 s | 1 |
| Layout mismatch | 5 | 0.117 s | 3.033 s | 0 |
| File context | 5 | 0.109 s | 0.082 s | 0 |

## Paper-value assessment

- `57.69%` baseline success rate.
- `96.15%` grounded-AI success rate.
- `<1.0 s` baseline latency.
- Approximately `2.4 s` grounded-AI latency.
- `$0.00` baseline cost per fix.
- `<$0.01` grounded-AI cost per fix.

The paper identifies these values as simulated rather than native measurements. The separate executable-based run produced different empirical values: deterministic baseline rejection was 100%, deterministic built-in repair succeeded for 2 cases, and OpenRouter returned insufficient-credit errors for 18 attempted requests. The remaining semantic file-context violations correctly stayed in the semantic-analysis path and were not sent through parse autofix.

## Implementation consistency notes

- The paper says the lexer uses `std::stack`; the implementation uses a fixed C-style `indent_stack` array.
- The paper says `%dprec` is used; this is supported by `lexico.y` and is accurate.
- The paper describes AI as an optional driver-side diagnostic/recovery layer; this matches the implementation.
- Exact API cost cannot be computed from the CSV because the compiler discards provider token/usage metadata.
