#pragma once

#include "memory_scan.hpp"
#include "process.hpp"

#include <cstddef>
#include <string>
#include <vector>

struct NetvarRequest {
    const char *className;
    const char *fieldName;
    std::ptrdiff_t *target;
};

bool resolveNetvars(const Process &process, const ModuleImage &schemaSystem,
                    const std::vector<NetvarRequest> &requests, std::string &error);
