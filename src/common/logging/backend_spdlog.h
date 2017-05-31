#include <memory>
#include <vector>
#include <spdlog/spdlog.h>
#include "common/logging/log.h"

namespace Log {

class SpdLogBackend {
public:
    static SpdLogBackend& Instance();

    SpdLogBackend(SpdLogBackend const&) = delete;
    const SpdLogBackend& operator=(SpdLogBackend const&) = delete;

private:
    SpdLogBackend();
    ~SpdLogBackend();
};
} // namespace Log