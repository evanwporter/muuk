import os
import subprocess
import platform
import shutil

# Configuration Variables
APP_NAME = "MyTool"
APP_VERSION = "1.0.0"
BUILD_DIR = "build"
INSTALLER_NAME = f"{APP_NAME}_Installer.exe"
SNAP_NAME = f"{APP_NAME}_{APP_VERSION}.snap"
NSIS_SCRIPT = "installer.nsi"
SNAPCRAFT_FILE = "snapcraft.yaml"

def run_command(command, cwd=None):
    """Run a shell command and print output."""
    result = subprocess.run(command, shell=True, cwd=cwd, capture_output=True, text=True)
    if result.returncode != 0:
        print(f"Error running command: {command}")
        print(result.stderr)
    else:
        print(result.stdout)

def build_project():
    """Compile the C++ project (if needed)."""
    print("Building the project...")
    os.makedirs(BUILD_DIR, exist_ok=True)
    if platform.system() == "Windows":
        run_command("cmake .. -G Ninja", cwd=BUILD_DIR)
        run_command("cmake --build .", cwd=BUILD_DIR)
    else:
        run_command("make", cwd=BUILD_DIR)  # Adjust based on your build system

def create_nsis_script():
    """Generate NSIS script for Windows installer."""
    print("📦 Generating NSIS script...")
    nsis_content = f"""
    !define APP_NAME "{APP_NAME}"
    !define APP_VERSION "{APP_VERSION}"
    Outfile "{INSTALLER_NAME}"
    InstallDir "$PROGRAMFILES\\{APP_NAME}"

    Section "Install"
        SetOutPath "$INSTDIR"
        File "{BUILD_DIR}\\{APP_NAME}.exe"
        File /oname=ninja.exe "C:\\path\\to\\ninja.exe"
        File /oname=git.exe "C:\\path\\to\\git.exe"
        File /oname=g++.exe "C:\\path\\to\\g++.exe"
    SectionEnd
    """
    with open(NSIS_SCRIPT, "w") as file:
        file.write(nsis_content)

def create_snapcraft_yaml():
    """Generate Snapcraft YAML file."""
    print("Generating Snapcraft YAML...")
    snapcraft_content = f"""
    name: {APP_NAME.lower()}
    base: core20
    version: "{APP_VERSION}"
    summary: {APP_NAME} - A C++ command-line tool
    description: |
      {APP_NAME} is a powerful command-line tool that requires Ninja, Git, and a C++ compiler.

    grade: stable
    confinement: strict

    parts:
      {APP_NAME.lower()}:
        plugin: dump
        source: {BUILD_DIR}
        build-packages:
          - ninja-build
          - git
          - g++
    """
    with open(SNAPCRAFT_FILE, "w") as file:
        file.write(snapcraft_content)

def build_nsis():
    """Compile NSIS installer."""
    if platform.system() == "Windows":
        print("Building NSIS Installer...")
        run_command(f'makensis {NSIS_SCRIPT}')
    else:
        print("NSIS can only be built on Windows!")

def build_snap():
    """Build Snap package."""
    if platform.system() == "Linux":
        print("Building Snap package...")
        run_command("snapcraft")
        shutil.move(f"{APP_NAME.lower()}_{APP_VERSION}_amd64.snap", SNAP_NAME)
    else:
        print("Snapcraft can only be built on Linux!")

def main():
    """Main function to automate packaging."""
    build_project()
    
    if platform.system() == "Windows":
        create_nsis_script()
        build_nsis()
    
    elif platform.system() == "Linux":
        create_snapcraft_yaml()
        build_snap()
    
    print("✅ Packaging completed!")

if __name__ == "__main__":
    main()
