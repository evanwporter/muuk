import os
import subprocess
import shutil
import pytest

# Test directory and files
TEST_DIR = os.path.dirname(os.path.abspath(__file__))
MUUK_TOML = os.path.join(TEST_DIR, "test_data", "muuk.toml")


@pytest.fixture(scope="module")
def setup_muuk_toml():
    """Fixture to create a temporary muuk.toml file for testing."""
    test_toml_content = """\
[package]
name = "test_project"
version = "1.0.0"

[scripts]
hello = "echo Hello, World!"
fail_script = "invalid_command"

[clean]
patterns = ["*.tmp"]
"""

    # Ensure test_data directory exists
    os.makedirs(os.path.join(TEST_DIR, "test_data"), exist_ok=True)

    # Write muuk.toml for testing
    with open(MUUK_TOML, "w") as f:
        f.write(test_toml_content)

    yield  # Let the test run

    # Cleanup after tests
    os.remove(MUUK_TOML)


def run_muuk_command(*args):
    """Helper function to execute muuk and return output."""
    cmd = ["../muuk"] + list(args)  # Adjust the path if needed
    result = subprocess.run(cmd, cwd=TEST_DIR, capture_output=True, text=True)
    return result.returncode, result.stdout, result.stderr


def test_muuk_help():
    """Test `muuk --help`."""
    returncode, stdout, stderr = run_muuk_command("--help")
    assert returncode == 0
    assert "usage" in stdout.lower()


def test_muuk_clean_no_files(setup_muuk_toml):
    """Test `muuk clean` when there are no files to remove."""
    returncode, stdout, stderr = run_muuk_command("clean")
    assert returncode == 0
    assert "clean operation completed" in stdout.lower()


def test_muuk_clean_with_files(setup_muuk_toml):
    """Test `muuk clean` when there are matching files."""
    tmp_file = os.path.join(TEST_DIR, "test_data", "temp.tmp")

    # Create a temporary file that should be cleaned
    with open(tmp_file, "w") as f:
        f.write("Temporary file content")

    assert os.path.exists(tmp_file)

    returncode, stdout, stderr = run_muuk_command("clean")

    assert returncode == 0
    assert not os.path.exists(tmp_file)  # File should be deleted


def test_muuk_run_valid_script(setup_muuk_toml):
    """Test `muuk run hello`."""
    returncode, stdout, stderr = run_muuk_command("run", "hello")
    assert returncode == 0
    assert "Hello, World!" in stdout


def test_muuk_run_invalid_script(setup_muuk_toml):
    """Test `muuk run fail_script` with a non-existent command."""
    returncode, stdout, stderr = run_muuk_command("run", "fail_script")
    assert returncode != 0
    assert "not found" in stderr.lower() or "command not found" in stderr.lower()


def test_muuk_build(setup_muuk_toml):
    """Test `muuk build`."""
    returncode, stdout, stderr = run_muuk_command("build")
    assert returncode == 0 or returncode == 1  # Build may fail if setup isn't complete
    assert "starting build operation" in stdout.lower()


def test_muuk_install_invalid_repo(setup_muuk_toml):
    """Test `muuk install` with an invalid GitHub repo."""
    returncode, stdout, stderr = run_muuk_command("install", "invalid/repo")
    assert returncode != 0
    assert (
        "error fetching latest release" in stdout.lower()
        or "invalid repository format" in stderr.lower()
    )
