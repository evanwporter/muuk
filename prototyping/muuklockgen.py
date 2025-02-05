import os
import toml


class Package:
    def __init__(self, name, version, base_path, package_type):
        self.name = name
        self.version = version
        self.base_path = base_path
        self.package_type = package_type  # "library" or "build"
        self.include = set()
        self.libflags = set()
        self.lflags = set()
        self.sources = []
        self.dependencies = {}

    def merge(self, child_pkg):
        print(f"Merging {child_pkg.name} into {self.name}")

        updated_paths = lambda paths: {
            os.path.join(child_pkg.base_path, p) for p in paths
        }
        self.include.update(updated_paths(child_pkg.include))
        self.libflags.update(child_pkg.libflags)
        self.lflags.update(child_pkg.lflags)

        # Merge dependencies while avoiding circular ones
        self.dependencies.update(child_pkg.dependencies)

        # print(child_pkg.serialize())
        # print(self.serialize())

    def serialize(self):
        return {
            "include": [
                os.path.normpath(os.path.join(self.base_path, path))
                for path in self.include
            ],
            "libflags": list(self.libflags),
            "lflags": list(self.lflags),
            "sources": [
                os.path.normpath(os.path.join(self.base_path, path))
                for path in self.sources
            ],
            "base_path": os.path.normpath(self.base_path),
        }


class MuukParser:
    def __init__(self, base_path):
        self.base_path = base_path
        self.resolved_packages = {"library": {}, "build": {}}

    def parse_muuk_toml(self, path, is_base=False):
        if not os.path.exists(path):
            print(f"Error: {path} not found!")
            return

        data = toml.load(path)
        package_name, package_version = (
            data["package"]["name"],
            data["package"]["version"],
        )
        print(f"Parsing {path}: {package_name} {package_version}")

        package = Package(
            package_name, package_version, os.path.dirname(path), "library"
        )
        self._parse_section(data.get("library", {}).get(package_name, {}), package)

        self.resolved_packages["library"][package_name] = package

        if is_base and "build" in data:
            for build_name, build_info in data["build"].items():
                build_package = Package(
                    build_name, package_version, os.path.dirname(path), "build"
                )
                self._parse_section(build_info, build_package)
                self.resolved_packages["build"][build_name] = build_package

    def _parse_section(self, section, package):
        """Parse and store include paths, flags, sources, and dependencies for a package."""
        package.include.update(section.get("include", []))
        package.sources.extend(section.get("sources", []))
        package.libflags.update(section.get("libflags", []))
        package.lflags.update(section.get("lflags", []))
        package.dependencies.update(section.get("dependencies", {}))

        # if package.package_type == "build":
        #     lib_package = self.resolved_packages["library"].get(package.name)
        #     if lib_package:
        #         print(
        #             f"Inheriting properties from {lib_package.name} into {package.name}"
        #         )
        #         package.merge(lib_package)

    def resolve_dependencies(self, package_name):
        """Recursively resolve dependencies and merge includes, libflags, and lflags."""
        package = self.resolved_packages["library"].get(
            package_name
        ) or self.resolved_packages["build"].get(package_name)

        # Search for package in modules/
        if not package:
            self._search_and_parse_dependency(package_name)
            package = self.resolved_packages["library"].get(package_name)
            if not package:
                print(f"Error: Package {package_name} not found.")
                return

        for dep_name, dep_version in list(package.dependencies.items()):
            self.resolve_dependencies(dep_name)

            if dep_name in self.resolved_packages["library"]:
                package.merge(self.resolved_packages["library"][dep_name])

    def _search_and_parse_dependency(self, package_name):
        modules_dir = os.path.join(self.base_path, "modules")
        for mod_dir in os.listdir(modules_dir):
            if package_name in mod_dir:
                dep_path = os.path.join(modules_dir, mod_dir, "muuk.toml")
                if os.path.exists(dep_path):
                    self.parse_muuk_toml(dep_path)
                    return

    def generate_lockfile(self, output_path):
        print("Generating muuk.lock.toml...")
        lock_data = {
            pkg_type: {pkg_name: pkg.serialize() for pkg_name, pkg in packages.items()}
            for pkg_type, packages in self.resolved_packages.items()
        }
        with open(output_path, "w") as lockfile:
            toml.dump(lock_data, lockfile)
        print(f"muuk.lock.toml written to {output_path}")


base_path = "./"
muuk_parser = MuukParser(base_path)

muuk_toml_path = os.path.join(base_path, "muuk.toml")
if os.path.exists(muuk_toml_path):
    muuk_parser.parse_muuk_toml(muuk_toml_path, is_base=True)
else:
    print(f"Error: {muuk_toml_path} does not exist!")

print("RESOLVED PKGS", muuk_parser.resolved_packages)
for package_name in list(muuk_parser.resolved_packages["build"].keys()):
    print(package_name)
    muuk_parser.resolve_dependencies(package_name)

muuk_parser.generate_lockfile(os.path.join(base_path, "muuk.lock.toml"))

print("muuk.lock.toml generation complete!")
