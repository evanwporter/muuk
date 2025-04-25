#include <string>
#include <unordered_map>

// { Dependency { Versioning { T }}}
template <typename T>
using DependencyVersionMap = std::unordered_map<std::string, std::unordered_map<std::string, T>>;
