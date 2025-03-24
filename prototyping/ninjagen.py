import os
import toml


class NinjaGenerator:
    def __init__(self, lockfile_path, build_type):
        self.lockfile_path = lockfile_path
        self.build_type = build_type
        self.compiler = "cl"
        self.archiver = "lib"
        self.build_dir = os.path.join("build", build_type)
        self.data = self.load_toml()
        self.ninja_file = "build.ninja"

    def load_toml(self):
        if os.path.exists(self.lockfile_path):
            return toml.load(self.lockfile_path)
        raise Exception(f"Lockfile '{self.lockfile_path}' not found!")

    def generate_ninja_file(self):
        if not self.data:
            print("Error: No TOML data loaded.")
            return

        os.makedirs(self.build_dir, exist_ok=True)

        with open(self.ninja_file, "w") as out:
            print("Generating build rules...")
            self.write_ninja_header(out)

            objects, libraries = self.compile_objects(out)
            self.archive_libraries(out, objects, libraries)

            # Link each defined build separately
            for build_name in self.data.get("build", {}).keys():
                self.link_executable(out, objects, libraries, build_name)

        print(f"Ninja build file '{self.ninja_file}' has ben generated!")

    def write_ninja_header(self, out):
        out.write(
            f"""# Auto-generated Ninja build file

cxx = {self.compiler}
ar = {self.archiver}

rule compile
  command = $cxx -c $in /Fo$out $cflags
  description = Compiling $in

rule archive
  command = $ar /OUT:$out $in
  description = Archiving $out

rule link
  command = $cxx $in /Fe$out $lflags
  description = Linking $out\n\n"""
        )

    def compile_objects(self, out):
        objects, libraries = {}, []

        for pkg_type, packages in self.data.items():
            if pkg_type not in ["library", "build"]:
                continue

            for pkg_name, pkg_info in packages.items():
                print(f"Processing package: {pkg_name}")
                module_dir = os.path.join(self.build_dir, pkg_name)
                os.makedirs(module_dir, exist_ok=True)

                obj_files = [
                    os.path.join(
                        module_dir, os.path.splitext(os.path.basename(src))[0] + ".obj"
                    )
                    for src in pkg_info.get("sources", [])
                ]

                cflags_common = " ".join(
                    [
                        f"/I{os.path.normpath(inc).replace('\\', '/')}"
                        for inc in pkg_info.get("include", [])
                    ]
                    + pkg_info.get("libflags", [])
                )

                for src, obj in zip(pkg_info.get("sources", []), obj_files):
                    print(f"  Compiling: {src} -> {obj}")
                    out.write(
                        f"build {obj}: compile {os.path.normpath(src).replace('\\', '/')}\n  cflags = {cflags_common}\n"
                    )

                objects[pkg_name] = obj_files

        return objects, libraries

    def archive_libraries(self, out, objects, libraries):
        for pkg_name, obj_files in objects.items():
            if not obj_files:  # Skip if no object files exist
                continue

            lib_name = os.path.join(self.build_dir, pkg_name, f"{pkg_name}.lib")
            print(f"  Creating library: {lib_name}")
            out.write(f"build {lib_name}: archive {' '.join(obj_files)}\n")
            libraries.append(lib_name)

    def link_executable(self, out, objects, libraries, build_name):
        exe_output = os.path.join(self.build_dir, f"{build_name}.exe")

        # Include all object files for this build target
        build_objs = " ".join(objects.get(build_name, []))
        muuk_libs = " ".join(libraries)
        muuk_lflags = " ".join(self.data["build"].get(build_name, {}).get("lflags", []))

        print(f"Linking executable for '{build_name}': {exe_output}")

        out.write(
            f"\nbuild {os.path.normpath(exe_output).replace('\\', '/')}: link {build_objs} {muuk_libs}\n  lflags = {muuk_lflags}\n"
        )


# Usage Example:
if __name__ == "__main__":
    generator = NinjaGenerator("muuk.lock.toml", "bin")
    generator.generate_ninja_file()
