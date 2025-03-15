import subprocess
import os
import pytest
import tempfile


MUUK_EXECUTABLE = "build/muuk/release/bin/muuk.exe"


# @pytest.mark.parametrize(
#     "args",
#     [
#         [],  # No arguments
#         ["invalid_command"],  # Invalid command
#         ["--muuk-path", "non_existent_file.toml"],  # Non-existent config
#         ["build", "--release", "--unknown-flag"],  # Unknown flag
#         ["install"],  # Missing required argument
#         ["run"],  # Missing script name
#         ["upload-patch", "--dry-run", "--invalid"],  # Invalid flag for upload-patch
#     ],
# )
# def test_invalid_arguments(args):
#     assert True
#     # result = subprocess.run([MUUK_EXECUTABLE] + args, capture_output=True, text=True)
#     # assert result.returncode != 0, f"Expected failure, but got success: {result.stdout}"


def test_it():
    assert True


# def test_empty_muuk_toml():
#     with tempfile.TemporaryDirectory() as temp_dir:
#         muuk_path = os.path.join(temp_dir, "muuk.toml")
#         open(muuk_path, "w").close()  # Create an empty file

#         result = subprocess.run(
#             [MUUK_EXECUTABLE, "--muuk-path", muuk_path, "build"],
#             capture_output=True,
#             text=True,
#         )
#         assert result.returncode != 0, (
#             "Program should fail when provided with an empty config file"
#         )


# def test_invalid_toml():
#     with tempfile.TemporaryDirectory() as temp_dir:
#         muuk_path = os.path.join(temp_dir, "muuk.toml")
#         with open(muuk_path, "w") as f:
#             f.write("invalid_toml = [unclosed_string")  # Malformed TOML

#         result = subprocess.run(
#             [MUUK_EXECUTABLE, "--muuk-path", muuk_path, "build"],
#             capture_output=True,
#             text=True,
#         )
#         assert result.returncode != 0, (
#             "Program should fail when provided with malformed TOML"
#         )


# def test_missing_muuk_toml():
#     result = subprocess.run(
#         [MUUK_EXECUTABLE, "--muuk-path", "non_existent.toml", "build"],
#         capture_output=True,
#         text=True,
#     )
#     assert result.returncode != 0, (
#         "Program should fail when the configuration file is missing"
#     )


# def test_permission_error():
#     with tempfile.TemporaryDirectory() as temp_dir:
#         muuk_path = os.path.join(temp_dir, "muuk.toml")
#         with open(muuk_path, "w") as f:
#             f.write("[package]\nname = 'test'\nversion = '1.0.0'")  # Valid TOML

#         os.chmod(muuk_path, 0o000)  # Remove all permissions

#         result = subprocess.run(
#             [MUUK_EXECUTABLE, "--muuk-path", muuk_path, "build"],
#             capture_output=True,
#             text=True,
#         )

#         os.chmod(muuk_path, 0o644)  # Restore permissions for cleanup

#         assert result.returncode != 0, (
#             "Program should fail when it cannot read the config file"
#         )


# def test_large_muuk_toml():
#     with tempfile.TemporaryDirectory() as temp_dir:
#         muuk_path = os.path.join(temp_dir, "muuk.toml")
#         with open(muuk_path, "w") as f:
#             f.write("[package]\nname = 'test'\nversion = '1.0.0'\n")
#             for i in range(100000):
#                 f.write(
#                     f"entry{i} = '{''.join(random.choices(string.ascii_letters, k=50))}'\n"
#                 )

#         result = subprocess.run(
#             [MUUK_EXECUTABLE, "--muuk-path", muuk_path, "build"],
#             capture_output=True,
#             text=True,
#         )
#         assert result.returncode != 0, (
#             "Program should fail or handle large TOML files correctly"
#         )


# def test_random_binary_input():
#     with tempfile.TemporaryDirectory() as temp_dir:
#         muuk_path = os.path.join(temp_dir, "muuk.toml")
#         with open(muuk_path, "wb") as f:
#             f.write(os.urandom(1024))  # Write random binary data

#         result = subprocess.run(
#             [MUUK_EXECUTABLE, "--muuk-path", muuk_path, "build"],
#             capture_output=True,
#             text=True,
#         )
#         assert result.returncode != 0, (
#             "Program should fail when given a binary input file"
#         )


# def test_circular_dependency():
#     with tempfile.TemporaryDirectory() as temp_dir:
#         muuk_path = os.path.join(temp_dir, "muuk.toml")
#         with open(muuk_path, "w") as f:
#             f.write(
#                 """
#             [package]
#             name = "test"
#             version = "1.0.0"

#             [library.test]
#             dependencies = { test = "1.0.0" }  # Circular dependency
#             """
#             )

#         result = subprocess.run(
#             [MUUK_EXECUTABLE, "--muuk-path", muuk_path, "build"],
#             capture_output=True,
#             text=True,
#         )
#         assert result.returncode != 0, (
#             "Program should detect and handle circular dependencies"
#         )


# @pytest.mark.parametrize(
#     "garbage_input",
#     [
#         "???",
#         "!!invalid!!",
#         "DROP TABLE users;",
#         '{"key": "value"}',  # JSON instead of TOML
#         "<xml><error></xml>",  # XML instead of TOML
#     ],
# )
# def test_garbage_inputs(garbage_input):
#     with tempfile.TemporaryDirectory() as temp_dir:
#         muuk_path = os.path.join(temp_dir, "muuk.toml")
#         with open(muuk_path, "w") as f:
#             f.write(garbage_input)

#         result = subprocess.run(
#             [MUUK_EXECUTABLE, "--muuk-path", muuk_path, "build"],
#             capture_output=True,
#             text=True,
#         )
#         assert result.returncode != 0, (
#             f"Program should fail with garbage input: {garbage_input}"
#         )


# def test_no_disk_space():
#     with tempfile.TemporaryDirectory() as temp_dir:
#         muuk_path = os.path.join(temp_dir, "muuk.toml")
#         with open(muuk_path, "w") as f:
#             f.write("[package]\nname = 'test'\nversion = '1.0.0'")

#         # Mock "no disk space" by mounting a full tmpfs (Linux only)
#         if shutil.which("mount"):
#             subprocess.run(
#                 ["mount", "-t", "tmpfs", "-o", "size=1K", "tmpfs", temp_dir],
#                 check=False,
#             )

#         result = subprocess.run(
#             [MUUK_EXECUTABLE, "--muuk-path", muuk_path, "build"],
#             capture_output=True,
#             text=True,
#         )

#         if shutil.which("umount"):
#             subprocess.run(["umount", temp_dir], check=False)

#         assert result.returncode != 0, "Program should fail when there is no disk space"


if __name__ == "__main__":
    pytest.main()
