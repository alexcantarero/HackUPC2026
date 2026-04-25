#!/usr/bin/env python3

import sys
from pathlib import Path


EXCLUDED_DIR_NAMES = {
    ".git",
    ".venv",
    "venv",
    "__pycache__",
    ".mypy_cache",
    ".pytest_cache",
    ".cache",
    "node_modules",
    "build",
    "dist",
    "out",
    "bin",
}

EXCLUDED_DIR_PREFIXES = {
    "cmake-build-",
}


def is_excluded_path(path: Path, root: Path) -> bool:
    rel_parts = path.relative_to(root).parts
    return any(
        part in EXCLUDED_DIR_NAMES or any(part.startswith(prefix) for prefix in EXCLUDED_DIR_PREFIXES)
        for part in rel_parts
    )


def concatenate_code(directory, output_file="combined_code.txt"):
    directory = Path(directory)
    
    if not directory.exists():
        print(f"Error: Directory '{directory}' does not exist")
        sys.exit(1)
    
    # Find all .cpp and .hpp files
    extensions = ["*.cpp", "*.hpp", "*.py", "Makefile"]
    
    files = []
    for ext in extensions:
        files.extend(
            file_path
            for file_path in directory.rglob(ext)
            if not is_excluded_path(file_path, directory)
        )
    
    # Sort files for consistent output
    files.sort()
    
    with open(output_file, 'w') as outfile:
        for file_path in files:
            # Get relative path
            rel_path = file_path.relative_to(directory)
            
            # Write header
            outfile.write("=" * 60 + "\n")
            outfile.write(f"File: {rel_path}\n")
            outfile.write("=" * 60 + "\n\n")
            
            # Write file content
            try:
                with open(file_path, 'r') as infile:
                    outfile.write(infile.read())
                outfile.write("\n\n")
            except Exception as e:
                outfile.write(f"Error reading file: {e}\n\n")
    
    print(f"✅ Combined {len(files)} files into '{output_file}'")

if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("Usage: python3 combine_code.py <directory> [output_file]")
        print("Example: python3 combine_code.py ./src output.txt")
        sys.exit(1)
    
    output = sys.argv[2] if len(sys.argv) > 2 else "combined_code.txt"
    concatenate_code(sys.argv[1], output)