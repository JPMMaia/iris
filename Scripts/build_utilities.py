import subprocess
import os
from pathlib import Path
import shutil
import argparse

def run_command(directory: str, command: str) -> bool:
    """Run a shell command in a specific directory."""
    try:
        result: subprocess.CompletedProcess = subprocess.run(
            command, shell=True, cwd=directory, capture_output=True, text=True
        )
        print(f"Command: {command} (in {directory})")
        print("Output:\n", result.stdout)
        if result.stderr:
            print("Error:\n", result.stderr)
            return False
        return True
    except Exception as e:
        print(f"Failed to run command in {directory}: {e}")
        return False


def copy_folder(source: str, destination: str) -> None:
    """Copy an entire folder from source to destination."""
    try:
        shutil.copytree(source, destination, dirs_exist_ok=True)
        print(f"Copied folder from {source} to {destination}")
    except Exception as error:
        print(f"Failed to copy folder: {error}")

def copy_file(source: str, destination: str) -> None:
    """Copy a single file from source to destination."""
    try:
        shutil.copy2(source, destination)  # Preserves metadata
        print(f"Copied file from {source} to {destination}")
    except Exception as error:
        print(f"Failed to copy file: {error}")

root_directory = Path(__file__).resolve().parent.parent
examples_directory = root_directory.joinpath("Examples")
extension_directory = root_directory.joinpath("Tools/vscode/H-editor")
parser_directory = root_directory.joinpath("Source/Parser/tree-sitter-hlang")

def build_and_test() -> bool:
    if not run_command(parser_directory.as_posix(), "npm run test_tree_sitter"):
        return False
    
    if not run_command(extension_directory.as_posix(), "npm run test"):
        return False
    
    build_compiler("debug")
    generate_builtin()
    generate_examples()
    if not run_command(root_directory.joinpath("build").as_posix(), "ctest -j 8"):
        return False
    
    install_hlang("debug", root_directory.joinpath("debug", "../Hlang_install"))
    if not test_language_server():
        return False
    
    return True


def test_language_server() -> bool:
    run_command(extension_directory.as_posix(), "rpm run test")

def build_compiler(configuration: str) -> None:
    run_command(root_directory.as_posix(), "cmake -S . -B build")

def parse_file(directory: Path, source: Path, destination: Path) -> None:
    run_command(directory.as_posix(), "node " + parser_app_file_path.as_posix() + " write " + destination.as_posix() + " --input " + source.as_posix())

def generate_builtin() -> None:
    builtin_directory = root_directory.joinpath("Source/Compiler/Builtin")
    source_file = builtin_directory.joinpath("builtin.hltxt")
    destination_file = builtin_directory.joinpath("builtin.hl")
    parse_file(builtin_directory, source_file, destination_file)

def generate_examples() -> None:
    text_directory = examples_directory.joinpath("txt")
    source_files = list(text_directory.glob(f"*.hltxt"))

    for source_file in source_files:
        destination_file = examples_directory.joinpath("hl").joinpath(source_file.stem + ".hl")
        parse_file(examples_directory, source_file, destination_file)

def install_hlang(destination_directory: Path) -> None:
    run_command(root_directory.as_posix(), "cmake --install build --prefix " + destination_directory.as_posix())
        
# Execute commands
def main() -> None:

    parser = argparse.ArgumentParser(description="Helper scripts for building Hlang.")
    subparsers = parser.add_subparsers(dest="command", required=True, help="Available commands")

    build_and_test_command = subparsers.add_parser("build_and_test", help="Build and test all")

    build_parser_command = subparsers.add_parser("build_parser", help="Build parser")

    generate_builtin_command = subparsers.add_parser("generate_builtin", help="Generate builtin")
    
    generate_examples_command = subparsers.add_parser("generate_examples", help="Generate examples")

    install_hlang_command = subparsers.add_parser("install_hlang", help="Install Hlang")
    install_hlang_command.add_argument("destination_directory")

    test_language_server_command = subparsers.add_parser("test_language_server", help="Tests the language server")

    args = parser.parse_args()

    if args.command == "build_and_test":
        build_and_test()
    elif args.command == "generate_builtin":
        generate_builtin()
    elif args.command == "generate_examples":
        generate_examples()
    elif args.command == "install_hlang":
        install_hlang(Path(args.destination_directory))
    elif args.command == "test_language_server":
        if not test_language_server():
            exit(-1)

if __name__ == "__main__":
    main()
