import os
import random
import string


def random_module_name():
    return "".join(random.choices(string.ascii_lowercase, k=random.randint(4, 8)))


num_modules = 26
module_names = set()

while len(module_names) < num_modules:
    module_names.add(random_module_name())

module_names = list(module_names)
random.shuffle(module_names)

module_dependencies = {module_names[0]: []}

for module in module_names[1:]:
    possible_deps = module_names[: module_names.index(module)]
    num_deps = random.randint(1, min(3, len(possible_deps)))
    module_dependencies[module] = random.sample(possible_deps, num_deps)

test_modules = {
    f"{module}.ixx": f"module {module};\n"
    + "".join(f"import {dep};\n" for dep in module_dependencies[module])
    + f"export module {module};"
    for module in module_names
}

test_dir = "prototyping/modules/test_modules"
os.makedirs(test_dir, exist_ok=True)

for filename, content in test_modules.items():
    file_path = os.path.join(test_dir, filename)
    with open(file_path, "w", encoding="utf-8") as f:
        f.write(content)

print(f"Randomized test module files have been created in '{test_dir}' directory.")

print("\nGenerated Module Dependencies:")
for mod, deps in module_dependencies.items():
    print(f"{mod}: {', '.join(deps) if deps else 'No dependencies'}")
