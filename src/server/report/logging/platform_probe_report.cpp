#include "platform_probe_report.h"
#include "mir/logging/logger.h"

#include <sstream>
#include <string>

namespace ml = mir::logging;
namespace mg = mir::graphics;

mir::report::logging::PlatformProbeReport::PlatformProbeReport(std::shared_ptr<ml::Logger> const& logger)
    : logger{logger}
{
}

namespace
{
std::string priority_to_string(mg::PlatformPriority priority)
{
    switch (priority)
    {
    case mg::PlatformPriority::unsupported:
        return "unsupported";
    case mg::PlatformPriority::supported:
        return "supported";
    case mg::PlatformPriority::best:
        return "best";
    default:
        return std::to_string(static_cast<int>(priority));
    }
}
}


void mir::report::logging::PlatformProbeReport::module_probed(mg::ModuleProperties const& module, mg::PlatformPriority probe_value)
{
    std::stringstream ss;
    ss << "Module probed" << std::endl
       << "\tName: " << module.name << std::endl
       << "\tPriority: " << priority_to_string(probe_value);
    logger->log(ml::Logger::informational, ss.str(), "Platform");
}

void mir::report::logging::PlatformProbeReport::invalid_module_probed(std::exception const& error)
{
    std::stringstream ss;
    ss << "Failed to probe module. Error was: " << error.what();
    logger->log(ml::Logger::warning, ss.str(), "Platform");
}

void mir::report::logging::PlatformProbeReport::module_selected(mg::ModuleProperties const& module)
{
    std::stringstream ss;
    ss << "Selected module: " << module.name;
    logger->log(ml::Logger::informational, ss.str(), "Platform");
}
