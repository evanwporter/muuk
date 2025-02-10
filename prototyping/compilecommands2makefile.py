import json
import os

compile_commands_path = "build/compile_commands.json"
with open(compile_commands_path, "r") as f:
    commands = json.load(f)

makefile_content = """CC=g++
CFLAGS=-std=c++20 -m64 -march=native -O2 -fexceptions -finput-charset=UTF-8
AR=ar rcs

# Object files
OBJ_FILES = \\\n"""

obj_files = []
lib_targets = {}
link_targets = {}

for entry in commands:
    source_file = entry["file"]
    output_file = entry["output"]

    if "ar rcs" in entry["command"]:
        lib_name = output_file
        lib_targets[lib_name] = entry["command"]
    elif "g++ -m64" in entry["command"] and "-o" in entry["command"]:
        binary_name = os.path.basename(output_file)  # Get binary file name
        link_targets[f"link/{binary_name}"] = entry["command"]
    else:
        obj_files.append(output_file)
        makefile_content += f"    {output_file} \\\n"

makefile_content += "\n\n# Build object files\n"
for entry in commands:
    if "ar rcs" in entry["command"] or "g++ -m64" in entry["command"]:
        continue

    source_file = entry["file"]
    output_file = entry["output"]
    includes = " ".join(
        flag for flag in entry["command"].split() if flag.startswith("-I")
    )
    defines = " ".join(
        flag for flag in entry["command"].split() if flag.startswith("-D")
    )

    makefile_content += f"{output_file}: {source_file}\n"
    makefile_content += (
        f"\t$(CC) $(CFLAGS) {includes} {defines} -c {source_file} -o {output_file}\n\n"
    )

makefile_content += "# Archive libraries\n"
for lib, command in lib_targets.items():
    makefile_content += f"{lib}:\n\t{command}\n\n"

makefile_content += "# Link final binaries\n"
for link_name, link_command in link_targets.items():
    makefile_content += f"\n{link_name}:\n\t{link_command}\n"

makefile_content += """
clean:
\trm -f $(OBJ_FILES) 
"""

makefile_path = "Makefile"
with open(makefile_path, "w") as f:
    f.write(makefile_content)
