#!/usr/bin/env python3
"""c_parse_check.py - REAL C syntax check without a compiler (Kiwi has pycparser, no gcc)

Mini preprocessor: inlines project headers, drops system headers (replaced by typedef stubs),
expands object-like #defines, honours #ifndef/#ifdef/#if 0/#else/#endif. Then pycparser parses
the translation unit and an AST pass flags C89 violations:
  * declaration after statement inside a block
  * for-loop init declarations
  * // comments (pre-scan)
  * calls to undeclared functions (implicit declaration)

  python3 tools/c_parse_check.py src/tfmx_player.c src/mod_player.c ...
"""
import sys, os, re
from pycparser import c_parser, c_ast

STUBS = """
typedef signed char int8_t; typedef unsigned char uint8_t; typedef short int16_t; typedef unsigned short uint16_t;
typedef int int32_t; typedef unsigned int uint32_t; typedef long long int64_t; typedef unsigned long long uint64_t;
typedef unsigned long size_t; typedef struct _FILE FILE; typedef long time_t;
"""
LIBC = set("""memset memcpy memcmp strlen strncpy strncat strcat strcpy strcmp strncmp strrchr strchr
fopen fclose fread fwrite fseek ftell printf fprintf snprintf sprintf sscanf puts malloc free calloc realloc atoi abs
exit fflush getchar putchar tolower isdigit isspace pow sin cos tan sqrt exp log tanh tanhf fabsf""".split())
LIBC_CONST = {"NULL": "((void*)0)", "SEEK_SET": "0", "SEEK_END": "2", "SEEK_CUR": "1", "stdout": "((FILE*)0)", "stderr": "((FILE*)0)"}

def preprocess(path, defines, seen, out):
    base = os.path.dirname(path)
    stack = []                       # True = active
    def active(): return all(stack)
    lines = open(path, encoding="utf-8", errors="replace").read().replace("\\\n", " ").split("\n")
    for ln in lines:
        s = ln.strip()
        if s.startswith("#"):
            d = s[1:].strip(); kw = d.split()[0] if d.split() else ""
            arg = d[len(kw):].strip()
            if kw == "ifndef": stack.append(arg.split()[0] not in defines); continue
            if kw == "ifdef": stack.append(arg.split()[0] in defines); continue
            if kw == "if": stack.append(arg.strip() not in ("0",)); continue
            if kw == "else": stack[-1] = not stack[-1]; continue
            if kw == "endif": stack.pop(); continue
            if not active(): continue
            if kw == "define":
                m = re.match(r"(\w+)(\([^)]*\))?\s*(.*)", arg)
                if m.group(2): defines[m.group(1)] = ("fn", m.group(2), m.group(3))
                else: defines[m.group(1)] = re.sub(r"/\*.*?\*/", "", m.group(3)).strip()
                continue
            if kw == "include":
                m = re.match(r'"([^"]+)"', arg)
                if m:
                    p = os.path.normpath(os.path.join(base, m.group(1)))
                    if p not in seen: seen.add(p); preprocess(p, defines, seen, out)
                continue
            continue
        if active(): out.append(ln)

def expand(src, defines):
    src = re.sub(r"/\*.*?\*/", " ", src, flags=re.S)
    simple = {k: v for k, v in defines.items() if isinstance(v, str)}
    simple.update(LIBC_CONST)
    for _ in range(6):
        new = re.sub(r"\b([A-Za-z_]\w*)\b", lambda m: "(" + simple[m.group(1)] + ")" if simple.get(m.group(1)) not in (None, "") and not m.group(1).startswith("_") else m.group(0), src)
        if new == src: break
        src = new
    return src

class C89Visitor(c_ast.NodeVisitor):
    def __init__(s, declared): s.issues = []; s.declared = declared
    def visit_Compound(s, node):
        seen_stmt = False
        for it in node.block_items or []:
            if isinstance(it, c_ast.Decl):
                if seen_stmt: s.issues.append("decl after statement: %s (%s)" % (it.name, it.coord))
            else: seen_stmt = True
        s.generic_visit(node)
    def visit_For(s, node):
        if isinstance(node.init, c_ast.DeclList): s.issues.append("for-init declaration (%s)" % node.coord)
        s.generic_visit(node)
    def visit_FuncCall(s, node):
        if isinstance(node.name, c_ast.ID) and node.name.name not in s.declared and node.name.name not in LIBC:
            s.issues.append("implicit declaration: %s (%s)" % (node.name.name, node.coord))
        s.generic_visit(node)

def check(path):
    raw = open(path, encoding="utf-8", errors="replace").read()
    nocom = re.sub(r"/\*.*?\*/", "", raw, flags=re.S); nocom = re.sub(r'"(\\.|[^"\\])*"', '""', nocom)
    issues = ["// comment"] if "//" in nocom else []
    defines = {}; out = []; preprocess(os.path.abspath(path), defines, {os.path.abspath(path)}, out)
    src = STUBS + expand("\n".join(out), defines)
    try:
        ast = c_parser.CParser().parse(src, filename=os.path.basename(path))
    except Exception as e:
        return issues + ["PARSE ERROR: %s" % e]
    declared = set()
    for ext in ast.ext:
        if isinstance(ext, c_ast.FuncDef): declared.add(ext.decl.name)
        elif isinstance(ext, c_ast.Decl) and ext.name: declared.add(ext.name)
    v = C89Visitor(declared); v.visit(ast)
    return issues + v.issues

if __name__ == "__main__":
    bad = 0
    for f in sys.argv[1:]:
        iss = check(f); bad += bool(iss)
        print("  [%s] %s %s" % ("OK" if not iss else "FAIL", f, "; ".join(iss[:6])))
    print("C_PARSE_CHECK files=%d failing=%d" % (len(sys.argv) - 1, bad)); sys.exit(1 if bad else 0)
