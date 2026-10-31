# Daedalus / Ikarus / LeGo audit

`analyze.py` is the compatibility entry point for `audit.py`. The tool translates
Daedalus source declarations and calls into JSONL IR and compares the compiled
DAT calls with the current C++ bindings. It does not execute the game or scripts.
Python 3 and its standard library are sufficient.

```powershell
python tools/ikarus/extract_dat.py 'C:/Games/Archolos/Data/KM_ScriptsPL.mod' build/archolos-input/GOTHIC.DAT
python tools/ikarus/analyze.py --ikarus C:/Sources/Ikarus --lego C:/Sources/LeGo --mod C:/Sources/tcom/Source --dat build/archolos-input/GOTHIC.DAT --repo . --out build/ikarus-audit --disassemble
```

The extractor supports uncompressed Gothic VDF/MOD volumes (flags 80), validates
catalog/file bounds and refuses to overwrite an existing DAT. Use ZenKit for
compressed volumes. Sources and DAT can be supplied independently. Source text is
read as Latin-1; source manifests contain SHA-256 hashes. Keep third-party source,
DAT and full disassembly local, with their original licenses.

| Output | Meaning |
| --- | --- |
| `summary.json` | Counts, DAT hash and limitations |
| `functions.csv` | Every function, including zero direct calls; binding and distinct callers |
| `callgraph.csv` | Caller/callee edges with static call-site counts |
| `calls.jsonl` | Call IR with source location or bytecode PC; native source arguments |
| `definitions.jsonl` | Source declaration IR, including instances and prototypes |
| `sources.json` | Source paths, origins and content hashes |
| `native-and-hooks.csv` | Native and HookEngine sites; DAT and each source origin kept separate |
| `addresses.csv` | Distinct address/status pairs from DAT, or mod sources without DAT |
| `missing-functions.csv` | Called externals without a binding; unresolved symbols in source-only mode |
| `implementation-review.csv` | Empty, fixed-return or marked C++ handlers requiring semantic review |
| `markers.csv` | TODO/FIXME/unimplemented/placeholder/suppression markers in `common/*.cpp` recursively |
| `GOTHIC.disasm.txt` | Optional full named bytecode disassembly |

Counts measure **static call sites**, not how often a function runs. Source and
DAT counts are not added together: that would count the same calls twice and mix
different library versions. DAT takes precedence for the function/callgraph tables
when supplied. Direct BL/BE calls are counted throughout bytecode, including
instance/prototype initializers. Dynamic callback invocation is not resolved into
callgraph edges; zero direct calls do not prove that a function is unused.

`registered_*`, `overridden_review_semantics` and `hook_declared_review_semantics`
mean that a binding/declaration was found, not that its implementation is complete.
`registered_trivial` identifies empty/fixed-return native handlers. Native event
adapters for `_FF_HOOK` and `_AI_FUNCTION_EVENT` have a separate review status.
Hook correctness also requires checking the event emitter and callback register/
stack contract. An empty wrapper can be deliberate (for example a naked VM stack
adapter); inspect its callers before replacing it. Numeric native addresses are
resolved conservatively; expressions without a proven value remain dynamic.

For selected functions, instances or prototypes:

```powershell
python tools/ikarus/disasm.py build/archolos-input/GOTHIC.DAT MEM_WRITESTRING DEFAULTMUSIC VIL
```

To see which classes of vobs the scripts search by name (the compatibility layer mirrors only the movers):

```powershell
python -I tools/ikarus/vobnames.py --mod C:/Sources/tcom/Source --worlds C:/Work/worlds
```

The constant names are taken from the source of the scripts and the class from the nearest class header of the
binary ZEN; a name with no mover in any world is listed. A dynamic name (a variable) is only reported by its caller.

This is an audit front end and bytecode disassembler, not a Daedalus-to-C++ compiler.
Passing the audit cannot certify that the whole campaign works or cannot crash.
