import os
import re
import glob
import toml

from dataclasses import dataclass

EXPORT_MODULE_PATTERN = re.compile(r"^\s*export\s+module\s+([\w.]+);", re.MULTILINE)
IMPORT_PATTERN = re.compile(r"^\s*(?:export\s+)?import\s+([\w.]+);", re.MULTILINE)

SINGLE_LINE_COMMENT = re.compile(r"//.*")
MULTI_LINE_COMMENT = re.compile(r"/\*.*?\*/", re.DOTALL)


@dataclass
class Module:
    exported_module: str | None
    imported_modules: list[str]
    file_path: str


def remove_comments(content: str) -> str:
    content = re.sub(MULTI_LINE_COMMENT, "", content)
    content = re.sub(SINGLE_LINE_COMMENT, "", content)
    return content


def parse_cpp_modules(file_path: str) -> tuple[str | None, list[str]]:
    with open(file_path, "r", encoding="utf-8") as file:
        content = file.read()

    content = remove_comments(content)

    export_match = EXPORT_MODULE_PATTERN.search(content)
    module_name: str | None = export_match.group(1) if export_match else None

    imports: list[str] = IMPORT_PATTERN.findall(content)

    return module_name, imports


class ModuleParser:
    def __init__(self, toml_path):
        self.toml_path = toml_path
        self.modules_dict = {}
        self.resolved_modules = []
        self.visiting = set()
        self.config = self.load_toml()

    def load_toml(self):
        with open(
            os.path.join(self.toml_path, "muuk.toml"), "r", encoding="utf-8"
        ) as file:
            return toml.load(file)

    def get_module_paths(self):
        library_config = self.config.get("library", {}).get("muuk", {})
        module_patterns = library_config.get("modules", [])

        print(self.config)

        module_paths = []
        for pattern in module_patterns:
            full_pattern = os.path.join(self.toml_path, pattern)
            module_paths.extend(glob.glob(full_pattern, recursive=True))

        return module_paths

    def parse_all_modules(self):
        print("Scanning for C++20 modules based on muuk.toml...")

        module_paths = self.get_module_paths()

        for file_path in module_paths:
            module_name, dependencies = parse_cpp_modules(file_path)

            if module_name:
                self.modules_dict[module_name] = Module(
                    module_name, dependencies, file_path
                )
                print(f"Detected module: {module_name}")

    def resolve_module_order(self, module_name):
        """Recursively resolves module dependencies in a DFS-based approach."""
        if module_name in self.resolved_modules:
            return  # Already resolved

        if module_name in self.visiting:
            raise ValueError(f"Circular dependency detected: {module_name}")

        if module_name not in self.modules_dict:
            print(f"Warning: {module_name} is imported but not found in modules!")
            return

        self.visiting.add(module_name)

        module = self.modules_dict[module_name]
        for dep in module.imported_modules:
            self.resolve_module_order(dep)

        self.visiting.remove(module_name)
        self.resolved_modules.append(module_name)

    def resolve_all_modules(self):
        """Resolves the compilation order for all modules."""
        print("\nResolving module compilation order...")

        for module_name in self.modules_dict.keys():
            self.resolve_module_order(module_name)

        print("\nCorrect Compilation Order:")
        for mod in self.resolved_modules:
            print(f"{mod} -> {self.modules_dict[mod].file_path}")

        return self.resolved_modules

    def generate_lockfile(self):
        print(self.resolved_modules)


base_path = "./"
muuk_parser = ModuleParser(base_path)

muuk_parser.parse_all_modules()

try:
    muuk_parser.resolve_all_modules()
except ValueError as e:
    print(f"Error: {e}")

muuk_parser.generate_lockfile()

print("muuk.lock.toml generation complete!")
