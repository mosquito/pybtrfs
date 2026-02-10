#!/usr/bin/env python3
"""Generate .pyi stub files for all C extension modules in pybtrfs."""

import glob
import importlib
import os
import re
import sys
import sysconfig


def parse_sig(doc: str | None) -> tuple[str, str] | None:
    """Extract (params, return_type) from docstring signature like
    'func_name(path, id=0) -> int'.  Handles multi-line signatures."""
    if not doc:
        return None
    # Join lines until we find the closing ')' to support multi-line sigs
    lines = doc.strip().splitlines()
    sig = lines[0]
    for line in lines[1:]:
        if re.match(r"\w+\(", sig) and ")" not in sig:
            sig += " " + line.strip()
        else:
            break
    m = re.match(r"\w+\(([^)]*)\)\s*(?:->\s*(.+))?", sig)
    if not m:
        return None
    params = m.group(1).strip()
    ret = m.group(2).strip() if m.group(2) else "None"
    return params, ret


def fmt_func(name: str, obj) -> str:
    doc = getattr(obj, "__doc__", None)
    parsed = parse_sig(doc)
    if parsed:
        params, ret = parsed
        return f"def {name}({params}) -> {ret}: ..."
    return f"def {name}(*args, **kwargs): ..."


def fmt_member(name: str, mdef) -> str:
    """Guess type from T_* member type codes or object type."""
    if "uuid" in name:
        return f"    {name}: bytes"
    if "time" in name and name not in ("stransid", "rtransid", "ctransid",
                                       "otransid"):
        return f"    {name}: float"
    return f"    {name}: int"


def collect_members(cls) -> list[str]:
    """Collect read-only data members defined via PyMemberDef."""
    annotations = getattr(cls, "__annotations__", {})
    lines = []
    for name in sorted(vars(cls)):
        if name.startswith("_"):
            continue
        obj = vars(cls)[name]
        tp = type(obj).__name__
        if tp == "member_descriptor":
            if name in annotations:
                lines.append(f"    {name}: {annotations[name].__name__}")
            else:
                lines.append(fmt_member(name, obj))
        elif tp == "getset_descriptor":
            type_name = annotations[name].__name__ if name in annotations else "int"
            lines.append("    @property")
            lines.append(f"    def {name}(self) -> {type_name}: ...")
    return lines


def collect_methods(cls) -> list[str]:
    lines = []
    for name in sorted(vars(cls)):
        if name.startswith("_") and name not in (
            "__init__", "__iter__", "__next__",
            "__enter__", "__exit__", "__str__", "__repr__",
        ):
            continue
        obj = vars(cls).get(name)
        if obj is None:
            continue
        tp = type(obj).__name__
        if tp in ("method_descriptor", "wrapper_descriptor",
                   "builtin_function_or_method"):
            doc = getattr(obj, "__doc__", None)
            parsed = parse_sig(doc)
            if parsed:
                params, ret = parsed
                lines.append(f"    def {name}(self, {params}) -> {ret}: ..."
                             if params
                             else f"    def {name}(self) -> {ret}: ...")
            elif name == "__init__":
                continue  # handled via class docstring fallback
            elif name == "__iter__":
                lines.append("    def __iter__(self) -> Self: ...")
            elif name == "__enter__":
                lines.append("    def __enter__(self) -> Self: ...")
            elif name == "__next__":
                lines.append("    def __next__(self): ...")
            else:
                lines.append(f"    def {name}(self, *args, **kwargs): ...")
    return lines


def generate_for_module(module) -> str:
    """Generate stub content for a single C extension module."""
    out: list[str] = []
    needs_self = False

    funcs = []
    constants = []
    classes = []

    for name in sorted(dir(module)):
        if name.startswith("_"):
            continue
        obj = getattr(module, name)
        if isinstance(obj, type):
            classes.append((name, obj))
        elif callable(obj):
            funcs.append((name, obj))
        elif isinstance(obj, int):
            constants.append((name, obj))

    # Check if we need Self import
    for _, cls in classes:
        for mname in vars(cls):
            if mname in ("__iter__", "__enter__"):
                needs_self = True
                break
        if needs_self:
            break

    if needs_self:
        out.append("from typing import Self")
        out.append("")

    # Constants
    for name, val in constants:
        out.append(f"{name}: int")
    if constants:
        out.append("")

    # Classes
    for name, cls in classes:
        bases = []
        for base in cls.__bases__:
            if base is object:
                continue
            bases.append(base.__qualname__)

        if bases:
            out.append(f"class {name}({', '.join(bases)}):")
        else:
            out.append(f"class {name}:")

        members = collect_members(cls)
        methods = collect_methods(cls)

        # If __init__ wasn't generated from method docstring,
        # try the class docstring (common for C extension types)
        has_init = any("def __init__" in line for line in methods)
        if not has_init:
            cls_doc = getattr(cls, "__doc__", None)
            init_parsed = parse_sig(cls_doc)
            if init_parsed:
                params, _ = init_parsed
                if params:
                    init_line = (
                        f"    def __init__(self, {params}) -> None: ..."
                    )
                else:
                    init_line = "    def __init__(self) -> None: ..."
                methods.insert(0, init_line)

        body = members + methods
        if body:
            out.extend(body)
        else:
            out.append("    ...")
        out.append("")

    # Module-level functions
    for name, obj in funcs:
        out.append(fmt_func(name, obj))
    if funcs:
        out.append("")

    return "\n".join(out) + "\n"


def discover_extensions(package_dir: str) -> list[str]:
    """Find all .so/.pyd extension modules under the package directory."""
    ext_suffix = sysconfig.get_config_var("EXT_SUFFIX") or ".so"
    pattern = os.path.join(package_dir, f"*{ext_suffix}")
    modules = []
    for path in sorted(glob.glob(pattern)):
        basename = os.path.basename(path)
        modname = basename.replace(ext_suffix, "")
        modules.append(modname)
    return modules


if __name__ == "__main__":
    package = "pybtrfs"
    package_dir = os.path.join(os.path.dirname(__file__) or ".", package)

    extensions = discover_extensions(package_dir)
    if not extensions:
        print(
            "No extension modules found. Build first with: make build",
            file=sys.stderr,
        )
        sys.exit(1)

    for modname in extensions:
        fqn = f"{package}.{modname}"
        module = importlib.import_module(fqn)
        stub = generate_for_module(module)
        dest = os.path.join(package_dir, f"{modname}.pyi")
        with open(dest, "w") as f:
            f.write(stub)
        print(f"wrote {dest}")
