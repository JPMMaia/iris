"""
Module-aware .def file generator for MSVC C++ DLLs.

Replacement for `cmake -E __create_def` that correctly handles C++ module
linkage symbols (e.g. ?foo@...::<!my.module>) which the CMake built-in tool
silently drops. These symbols are required when a C++ module is compiled into
a DLL and consumed by another module via `import`.

Usage:
    python generate_module_def.py <output.def> <obj_dir> [dumpbin_path]
"""

import subprocess
import sys
import re
import os
import glob

# Matches the ::<!module.name> suffix appended by MSVC to module-exported symbols.
# Group 1 = plain mangled name, Group 2 = module name.
_MODULE_SUFFIX_RE = re.compile(r'^(.+)::<!([^>]+)>$')


def get_symbols_from_obj(dumpbin: str, obj_file: str) -> list[tuple[str, bool, bool]]:
    """Return (symbol_name, is_data, is_defined) for External symbols.

    is_defined=True  → SECT<n>: the symbol is *defined* in this obj file.
    is_defined=False → UNDEF: the symbol is an external *reference* from this obj.
                       Only module-annotated (::<!...>) UNDEF symbols are returned;
                       plain UNDEF references are discarded.
    """
    try:
        result = subprocess.run(
            [dumpbin, '/SYMBOLS', '/NOLOGO', obj_file],
            capture_output=True, text=True, check=False,
        )
    except FileNotFoundError:
        print(f'error: dumpbin not found at "{dumpbin}"', file=sys.stderr)
        sys.exit(1)

    symbols: list[tuple[str, bool, bool]] = []
    pending_meta: str | None = None
    for line in result.stdout.splitlines():
        # dumpbin /SYMBOLS format per line (fields separated by whitespace):
        #   INDEX  VALUE  SECTION  TYPE  [()] STORAGECLASS  |  SYMBOLNAME
        # Examples:
        #   005 00000000 SECT3  notype ()    External     | ?foo@bar@@YAXXZ
        #   006 00000010 SECT3  notype       External     | ?g_count@bar@@3HA
        #   007 00000000 UNDEF  notype ()    External     | ?foo@bar@@YAXXZ::<!mod>
        meta: str
        rhs: str

        parts = line.split('|', 1)
        if len(parts) == 2:
            meta = parts[0]
            rhs = parts[1].strip()

            # dumpbin can place long symbol names on the next line after '|'.
            # Keep metadata and consume the next physical line as the symbol text.
            if not rhs:
                pending_meta = meta
                continue
        elif pending_meta is not None:
            meta = pending_meta
            rhs = line.strip()
            pending_meta = None
        else:
            continue

        # dumpbin may append the demangled name after the mangled one on the same
        # line (e.g. "?foo@@YAXXZ (void __cdecl foo(void))").  Take only the
        # first whitespace-separated token so we always get the raw mangled name.
        tokens = rhs.split()
        if not tokens:
            continue
        name = tokens[0]
        if not name:
            continue

        # Only public symbols
        if 'External' not in meta:
            continue

        is_defined = bool(re.search(r'\bSECT\w+\b', meta))
        is_undef   = 'UNDEF' in meta

        if not is_defined:
            # Keep UNDEF External symbols only if they carry a module annotation.
            # These are the cross-module references that MSVC's linker resolves
            # internally; we need to export the same names from our DLL so that
            # consumers across the DLL boundary can find them.
            if not (is_undef and _MODULE_SUFFIX_RE.match(name)):
                continue

        # '()' in the type field indicates a function; its absence indicates data
        is_data = '()' not in meta

        symbols.append((name, is_data, is_defined))

    return symbols


def should_export(symbol: str) -> bool:
    """Return False for compiler/linker bookkeeping symbols that must not be exported."""
    if symbol.startswith('@'):   # linker directive pseudo-symbols
        return False
    if symbol.startswith('__'):  # compiler-internal double-underscore symbols
        return False
    # Compiler-generated lambda types (STL internal); their COMDAT bodies have
    # unsatisfied dependencies when forced into DLL exports.
    if '<lambda_' in symbol:
        return False
    return True


def should_export_module_symbol(symbol: str, module_name: str, allowed_modules: set[str] | None) -> bool:
    """Return True for module symbols we want to export from this target.

    The module_name derived from the ::<!module.name> annotation is the primary
    gate.  We intentionally do NOT check for '@iris@@' in the symbol: that
    pattern matches member/named functions (e.g. ?foo@iris@@) but silently
    drops standalone namespace-level operators such as
      ??8iris@@ (operator==)  ??6iris@@ (operator<<)  ??Yiris@@ (operator+) …
    because those lack a '@' before the namespace token.
    """
    # Compiler-generated metadata helpers begin with '$' (e.g. $unwind$, $pdata$).
    if symbol.startswith('$'):
        return False
    if symbol.startswith('?dtor$'):
        return False
    if symbol.startswith('??_G'):  # deleting destructors — never export from DLLs
        return False
    if allowed_modules is None:
        return module_name.startswith('iris.')
    return module_name in allowed_modules


def _quote(symbol: str) -> str:
    needs_quotes = any(c in symbol for c in ' <>"!`()')
    return f'"{symbol}"' if needs_quotes else symbol


def generate_def(output_def: str, obj_dir: str, dumpbin: str, allowed_modules: set[str] | None) -> None:
    obj_files = glob.glob(os.path.join(obj_dir, '**', '*.obj'), recursive=True)

    if not obj_files:
        print(f'warning: no .obj files found in {obj_dir}', file=sys.stderr)

    # plain            : strongly-defined (SECT) plain symbols
    # annotated_defined: strongly-defined (SECT) annotated symbols → direct export
    # annotated_undef  : UNDEF annotated symbols                  → alias export
    #
    # In MSVC modules, some exported functions are emitted directly as annotated
    # symbols in implementation objects. Those must be exported as-is.
    # UNDEF annotated references can be exported as aliases when a matching
    # implementation symbol exists in this DLL.
    plain: dict[str, bool] = {}          # name → is_data
    annotated_defined: dict[str, tuple[str, str]] = {}  # symbol -> (base_name, module_name)
    annotated_undef: dict[str, tuple[str, str]] = {}    # symbol -> (base_name, module_name)

    for obj_file in obj_files:
        for symbol, is_data, is_defined in get_symbols_from_obj(dumpbin, obj_file):
            m = _MODULE_SUFFIX_RE.match(symbol)
            if m:
                base_name = m.group(1)
                module_name = m.group(2)
                if is_defined:
                    if symbol not in annotated_defined:
                        annotated_defined[symbol] = (base_name, module_name)
                elif symbol not in annotated_undef:
                    annotated_undef[symbol] = (base_name, module_name)
            else:
                # Plain defined symbol
                if is_defined and symbol not in plain:
                    plain[symbol] = is_data

    os.makedirs(os.path.dirname(os.path.abspath(output_def)), exist_ok=True)

    exported = 0
    with open(output_def, 'w') as f:
        f.write('EXPORTS\n')

        # --- Annotated Iris module exports ---
        # Export the annotated SECT External directly for module consumers, and
        # also export the plain base when it exists for non-module consumers (e.g.
        # regular .cpp test binaries that reference operator== through Catch2
        # macros without using `import`).
        #
        # Module implementation units (.cpp with `module X;`) emit ONLY the
        # annotated form, so base_name often does not exist.  Guarding the plain
        # export on `base_name in plain` avoids the LNK2001 that an unconditional
        # alias `symbol=base_name` would produce in that case.
        already_exported_plain: set[str] = set()
        for symbol in sorted(annotated_defined):
            if not should_export(symbol):
                continue
            base_name, module_name = annotated_defined[symbol]
            if not should_export_module_symbol(symbol, module_name, allowed_modules):
                continue
            f.write(f'\t{symbol}\n')               # annotated — for module consumers
            if base_name in plain and base_name not in already_exported_plain:
                f.write(f'\t{base_name}\n')        # plain exists in OBJ — for non-module consumers
                already_exported_plain.add(base_name)
            exported += 1

        # --- UNDEF annotated Iris symbols exported as aliases ---
        # Syntax:  "exported_name"=internal_name
        # The exported_name is what the import lib presents to consumers;
        # internal_name is the actual strong-definition symbol in the obj files.
        for ann_sym, (base_name, module_name) in annotated_undef.items():
            if '<lambda_' in ann_sym:
                continue  # STL lambda annotations — never export
            if not should_export_module_symbol(ann_sym, module_name, allowed_modules):
                continue
            if ann_sym in annotated_defined:
                continue  # already exported
            if base_name not in plain:
                continue  # alias target not defined as External in this DLL; would cause LNK2001
            f.write(f'\t{ann_sym}={base_name}\n')
            exported += 1

    print(f'[module_def] {exported} symbols -> {output_def}')


if __name__ == '__main__':
    if len(sys.argv) < 3:
        print(f'Usage: {sys.argv[0]} <output.def> <obj_dir> [dumpbin_path]')
        sys.exit(1)

    generate_def(
        output_def=sys.argv[1],
        obj_dir=sys.argv[2],
        dumpbin=sys.argv[3] if len(sys.argv) > 3 else 'dumpbin',
        allowed_modules=(set(sys.argv[4].split(';')) if len(sys.argv) > 4 and sys.argv[4] else None),
    )
