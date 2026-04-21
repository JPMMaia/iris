import subprocess
import os
from pathlib import Path
import shutil
import argparse
import tempfile
import time

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

def run_detached_program(directory: str, command: str) -> subprocess.Popen:

    DETACHED_PROCESS = 0x00000008
    stderr_file = tempfile.NamedTemporaryFile(mode="w", encoding="utf-8", delete=False, suffix=".stderr.log")
    stderr_path = Path(stderr_file.name)
    process = subprocess.Popen(
        command,
        shell=False,
        cwd=directory,
        text=True,
        close_fds=True,
        stdout=subprocess.DEVNULL,
        stderr=stderr_file,
        creationflags=DETACHED_PROCESS,
    )
    stderr_file.close()
    process.stderr_path = stderr_path
    print(f"Command: {command} (in {directory})")
    return process


def print_process_stderr(process: subprocess.Popen) -> None:
    stderr_path: Path | None = getattr(process, "stderr_path", None)
    if stderr_path is None:
        return

    try:
        stderr_output = stderr_path.read_text(encoding="utf-8")
        if stderr_output:
            print("<--- Begin Language Server stderr --->:\n", stderr_output)
            print("<--- End Language Server stderr --->\n")
    finally:
        stderr_path.unlink(missing_ok=True)


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
extension_directory = root_directory.joinpath("Tools/vscode/iris-extension")
parser_directory = root_directory.joinpath("Source/Parser/tree-sitter-iris")

def build_and_test() -> bool:
    if not run_command(parser_directory.as_posix(), "npm run test_tree_sitter"):
        return False
    
    if not run_command(extension_directory.as_posix(), "npm run test"):
        return False
    
    build_compiler("debug")
    generate_builtin()
    if not run_command(root_directory.joinpath("build").as_posix(), "ctest -j 8"):
        return False
    
    install_iris("debug", root_directory.joinpath("debug", "../iris_install"))
    if not test_language_server():
        return False
    
    return True


def test_language_server() -> bool:
    # TODO find executable and don't hardcode .exe
    language_server_path = root_directory.joinpath("build/Source/Language_server/iris_language_server.exe")
    os.environ["iris_language_server"] = language_server_path.as_posix()
    language_server_process = run_detached_program(root_directory.as_posix(), language_server_path.as_posix())

    # Run npm test via Popen so we can monitor both processes concurrently.
    # Use temp files to avoid pipe-buffer deadlocks.
    npm_stdout_file = tempfile.NamedTemporaryFile(mode="w", encoding="utf-8", delete=False, suffix=".npm.stdout.log")
    npm_stderr_file = tempfile.NamedTemporaryFile(mode="w", encoding="utf-8", delete=False, suffix=".npm.stderr.log")
    npm_stdout_path = Path(npm_stdout_file.name)
    npm_stderr_path = Path(npm_stderr_file.name)
    print(f"Command: npm run test (in {extension_directory.as_posix()})")
    npm_process = subprocess.Popen(
        "npm run test",
        shell=True,
        cwd=extension_directory.as_posix(),
        text=True,
        stdout=npm_stdout_file,
        stderr=npm_stderr_file,
    )
    npm_stdout_file.close()
    npm_stderr_file.close()

    abrupt_exit = False
    while npm_process.poll() is None:
        if language_server_process.poll() is not None:
            print(f"Language server terminated unexpectedly (code {language_server_process.returncode}); aborting npm test.")
            npm_process.kill()
            npm_process.wait()
            abrupt_exit = True
            break
        time.sleep(0.1)

    if language_server_process.poll() is None:
        language_server_process.kill()
    language_server_process.wait()
    print_process_stderr(language_server_process)

    npm_stdout = npm_stdout_path.read_text(encoding="utf-8")
    npm_stderr = npm_stderr_path.read_text(encoding="utf-8")
    npm_stdout_path.unlink(missing_ok=True)
    npm_stderr_path.unlink(missing_ok=True)

    print("Output:\n", npm_stdout)
    if npm_stderr:
        print("Error:\n", npm_stderr)

    if abrupt_exit:
        return False

    return npm_process.returncode == 0


def build_compiler(configuration: str) -> None:
    run_command(root_directory.as_posix(), "cmake -S . -B build")

def parse_file(directory: Path, source: Path, destination: Path) -> None:
    run_command(directory.as_posix(), "node " + parser_app_file_path.as_posix() + " write " + destination.as_posix() + " --input " + source.as_posix())

def generate_builtin() -> None:
    builtin_directory = root_directory.joinpath("Source/Compiler/Builtin")
    source_file = builtin_directory.joinpath("builtin.iris")
    destination_file = builtin_directory.joinpath("builtin.irisb")
    parse_file(builtin_directory, source_file, destination_file)

def install_iris(destination_directory: Path) -> None:
    run_command(root_directory.as_posix(), "cmake --install build --prefix " + destination_directory.as_posix())
        
# Execute commands
def main() -> None:

    parser = argparse.ArgumentParser(description="Helper scripts for building Iris.")
    subparsers = parser.add_subparsers(dest="command", required=True, help="Available commands")

    build_and_test_command = subparsers.add_parser("build_and_test", help="Build and test all")

    build_parser_command = subparsers.add_parser("build_parser", help="Build parser")

    generate_builtin_command = subparsers.add_parser("generate_builtin", help="Generate builtin")
    
    generate_examples_command = subparsers.add_parser("generate_examples", help="Generate examples")

    install_iris_command = subparsers.add_parser("install_iris", help="Install Iris")
    install_iris_command.add_argument("destination_directory")

    test_language_server_command = subparsers.add_parser("test_language_server", help="Tests the language server")

    args = parser.parse_args()

    if args.command == "build_and_test":
        build_and_test()
    elif args.command == "generate_builtin":
        generate_builtin()
    elif args.command == "install_iris":
        install_iris(Path(args.destination_directory))
    elif args.command == "test_language_server":
        if not test_language_server():
            exit(-1)

if __name__ == "__main__":
    main()
