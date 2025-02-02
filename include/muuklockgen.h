#ifndef MUUKLOCKGEN_H
#define MUUKLOCKGEN_H

#include <string>
#include <unordered_map>
#include <vector>
#include <set>
#include <memory>
#include <spdlog/spdlog.h>
#include "toml.hpp"

class MuukLockGenerator {
public:
    MuukLockGenerator(const std::string& modules_path, const std::string& lockfile_path, const std::string& root_toml);
    void generateLockFile();

private:
    struct Package {
        std::vector<std::string> include, sources, libflags, lflags, dependencies;
    };

    std::string modules_dir, lockfile, root_toml_path, base_package;
    std::unordered_map<std::string, Package> packages;
    std::vector<std::string> build_order;
    std::shared_ptr<spdlog::logger> logger_;

    void loadPackages();
    void resolveDependencies();
    void writeLockFile();
};

#endif // MUUKLOCKGEN_H
