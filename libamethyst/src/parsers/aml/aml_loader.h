/**
 * @file aml_loader.h
 * @brief Converts parsed AML node trees into Amethyst Instance hierarchies.
 */

#ifndef AMETHYST__AML_LOADER_H
#define AMETHYST__AML_LOADER_H

#include "parsers/aml/aml_parser.h"

#include <filesystem>
#include <memory>
#include <string>
#include <vector>

namespace Amethyst {

class Instance;

struct AmlLoadResult {
    std::vector<std::unique_ptr<Instance>> instances;
    std::string error;

    ~AmlLoadResult();
    AmlLoadResult() = default;
    AmlLoadResult(AmlLoadResult &&) = default;
    AmlLoadResult &operator=(AmlLoadResult &&) = default;

    bool ok() const { return error.empty(); }

    Instance *root() const { return instances.empty() ? nullptr : instances[0].get(); }
};

class AmlLoader {
  public:
    static AmlLoadResult loadFile(const std::filesystem::path &path);
    static AmlLoadResult loadString(const std::string &source);
    static AmlLoadResult loadNodes(const std::vector<AmlNode> &roots);
};

} // namespace Amethyst

#endif // AMETHYST__AML_LOADER_H
