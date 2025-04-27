import subprocess
import tempfile
import os
import textwrap
import pytest
import pathlib

THIS_DIR = os.path.dirname(__file__)  # test/external
PROJECT_ROOT = os.path.abspath(os.path.join(THIS_DIR, "../../"))


def print_directory_tree(root_path):
    print("\nDirectory Tree")
    for path in pathlib.Path(root_path).rglob("*"):
        rel_path = path.relative_to(root_path)
        if path.is_dir():
            print(f"[DIR]  {rel_path}")
        else:
            print(f"       {rel_path}")


@pytest.fixture
def simple_project_dir():
    with tempfile.TemporaryDirectory() as temp_dir:
        src_dir = os.path.join(temp_dir, "src")
        os.makedirs(src_dir, exist_ok=True)

        build_path = os.path.join(temp_dir, "build")
        os.makedirs(build_path, exist_ok=True)

        with open(os.path.join(src_dir, "main.cpp"), "w") as f:
            f.write("int main() { return 0; }")

        toml_content = textwrap.dedent("""\
            [package]
            name = "tiny"
            version = "0.1.0"
            edition = "20"

            [library]
            sources = ["src/main.cpp"]

            [build.bin]
            profile = ["debug"]
            sources = ["src/main.cpp"]

            [profile.debug]
            cflags = ["-g", "/FS"]
            lflags = []
            default = true
        """)

        with open(os.path.join(temp_dir, "muuk.toml"), "w") as f:
            f.write(toml_content)

        yield temp_dir


@pytest.fixture
def module_project_dir():
    with tempfile.TemporaryDirectory() as temp_dir:
        src_dir = os.path.join(temp_dir, "src")
        os.makedirs(src_dir, exist_ok=True)
        build_dir = os.path.join(temp_dir, "build")
        os.makedirs(build_dir, exist_ok=True)

        with open(os.path.join(src_dir, "math.ixx"), "w") as f:
            f.write(
                textwrap.dedent("""\
                export module math;

                export int add(int a, int b) {
                    return a + b;
                }
            """)
            )

        with open(os.path.join(src_dir, "main.cpp"), "w") as f:
            f.write(
                textwrap.dedent("""\
                import math;
                int main() {
                    return add(2, 3);
                }
            """)
            )

        toml_content = textwrap.dedent("""\
            [package]
            name = "tiny"
            version = "0.1.0"
            edition = "20"

            [library]
            modules = ["src/math.ixx"]

            [build.bin]
            profile = ["debug"]
            sources = ["src/main.cpp"]

            [profile.debug]
            cflags = ["-std:c++20", "/EHsc", "/FS", "/Zi"]
            lflags = []
            default = true
        """)
        with open(os.path.join(temp_dir, "muuk.toml"), "w") as f:
            f.write(toml_content)

        yield temp_dir


@pytest.mark.parametrize(
    "project_dir_fixture", ["simple_project_dir", "module_project_dir"]
)
def test_muuk_build(request, project_dir_fixture):
    project_dir = request.getfixturevalue(project_dir_fixture)

    muuk_executable = os.path.join(PROJECT_ROOT, "build", "debug", "bin", "bin.exe")

    result = subprocess.run(
        [muuk_executable, "build", "-c", "cl", "-p", "debug"],
        cwd=project_dir,
        capture_output=True,
        text=True,
    )

    print("STDOUT:\n", result.stdout)
    print("STDERR:\n", result.stderr)

    assert result.returncode == 0, "muuk build failed"

    if result.returncode == 0:
        print_directory_tree(project_dir)

        build_ninja_path = pathlib.Path(project_dir) / "build" / "debug" / "build.ninja"
        if build_ninja_path.exists():
            print("\n--- build.ninja content ---")
            with open(build_ninja_path, "r") as f:
                print(f.read())
        else:
            print("\n--- build.ninja not found ---")

    expected_bin = os.path.join(project_dir, "build", "debug", "bin", "bin.exe")
    assert os.path.isfile(expected_bin), f"Expected binary not found: {expected_bin}"


if __name__ == "__main__":
    pytest.main()
