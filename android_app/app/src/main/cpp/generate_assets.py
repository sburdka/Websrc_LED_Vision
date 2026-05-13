#!/usr/bin/env python3
"""
generate_assets.py  —  Embed web files as C++ byte arrays

Usage:
    python3 generate_assets.py <web_root_dir> <output_header>

Converts every HTML / JS / CSS / font / image in <web_root_dir> into a
const byte array compiled directly into the native library.
The resulting APK assets/ folder is empty — there is nothing to extract.
"""

import os
import sys
import re

INCLUDE_EXTS = {
    '.html': 'text/html; charset=utf-8',
    '.js':   'application/javascript',
    '.css':  'text/css',
    '.ttf':  'font/ttf',
    '.jpg':  'image/jpeg',
    '.jpeg': 'image/jpeg',
    '.png':  'image/png',
    '.gif':  'image/gif',
    '.ico':  'image/x-icon',
    '.svg':  'image/svg+xml',
}

EXCLUDE_DIRS = {'.git', 'android_app', '__pycache__', 'node_modules'}


def to_varname(rel_path: str) -> str:
    """Turn a relative file path into a valid C identifier."""
    name = re.sub(r'[^a-zA-Z0-9]', '_', rel_path)
    if name and name[0].isdigit():
        name = 'f_' + name
    return name


def collect_files(web_root: str):
    """Walk web_root and return list of (url_path, abs_path, mime)."""
    results = []
    for dirpath, dirnames, filenames in os.walk(web_root):
        # Prune excluded directories in-place
        dirnames[:] = [d for d in dirnames if d not in EXCLUDE_DIRS]

        for fname in sorted(filenames):
            ext = os.path.splitext(fname)[1].lower()
            if ext not in INCLUDE_EXTS:
                continue

            abs_path = os.path.join(dirpath, fname)
            rel      = os.path.relpath(abs_path, web_root)
            url_path = '/' + rel.replace(os.sep, '/')
            results.append((url_path, abs_path, INCLUDE_EXTS[ext]))

    return results


def bytes_to_c_array(data: bytes) -> str:
    """Format raw bytes as a C hex initialiser list, 16 bytes per line."""
    lines = []
    for i in range(0, len(data), 16):
        chunk = data[i:i + 16]
        lines.append('    ' + ', '.join(f'0x{b:02x}' for b in chunk))
    return ',\n'.join(lines)


def generate(web_root: str, output_path: str):
    files = collect_files(web_root)
    if not files:
        print(f"WARNING: no web files found under {web_root}", file=sys.stderr)

    os.makedirs(os.path.dirname(output_path) or '.', exist_ok=True)

    with open(output_path, 'w', encoding='utf-8') as out:
        out.write('// AUTO-GENERATED — do not edit.\n')
        out.write('// Produced by generate_assets.py at build time.\n')
        out.write('// All web content is compiled into this binary; nothing resides in APK assets.\n\n')
        out.write('#pragma once\n')
        out.write('#include <cstddef>\n')
        out.write('#include <string>\n')
        out.write('#include <unordered_map>\n\n')
        out.write('namespace embedded {\n\n')

        var_entries = []
        for url_path, abs_path, mime in files:
            varname = to_varname(url_path)
            with open(abs_path, 'rb') as f:
                data = f.read()

            out.write(f'// {url_path}  ({len(data)} bytes)\n')
            out.write(f'static const unsigned char {varname}[] = {{\n')
            out.write(bytes_to_c_array(data))
            out.write('\n};\n\n')
            var_entries.append((url_path, varname, mime, len(data)))

        # FileEntry struct
        out.write('struct FileEntry {\n')
        out.write('    const unsigned char* data;\n')
        out.write('    std::size_t          len;\n')
        out.write('    const char*          mime;\n')
        out.write('};\n\n')

        # Build the lookup map
        out.write('inline std::unordered_map<std::string, FileEntry> buildFileMap() {\n')
        out.write('    return {\n')
        for url_path, varname, mime, _ in var_entries:
            out.write(f'        {{"{url_path}", {{{varname}, sizeof({varname}), "{mime}"}}}},\n')
        out.write('    };\n')
        out.write('}\n\n')

        out.write('} // namespace embedded\n')

    total = sum(sz for _, _, _, sz in var_entries)
    print(f"Embedded {len(var_entries)} files ({total:,} bytes) → {output_path}")


if __name__ == '__main__':
    if len(sys.argv) != 3:
        print(f"Usage: {sys.argv[0]} <web_root> <output_header>", file=sys.stderr)
        sys.exit(1)
    generate(sys.argv[1], sys.argv[2])
