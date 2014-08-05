#include "probing_client_platform_factory.h"

#include <stdexcept>

#include <iostream>

namespace mcl = mir::client;

mcl::ProbingClientPlatformFactory::ProbingClientPlatformFactory(std::vector<std::shared_ptr<mir::SharedLibrary>> const& modules)
    : platform_modules{modules}
{
    if (modules.empty())
    {
        throw std::runtime_error{"Attempted to create a ClientPlatformFactory with no platform modules"};
    }
}

std::shared_ptr<mcl::ClientPlatform>
mcl::ProbingClientPlatformFactory::create_client_platform(mcl::ClientContext* context)
{

    std::cout<<"Probing client platforms, count: "<<platform_modules.size()<<std::endl;
    for (auto& module : platform_modules)
    {
        std::cout<<"\tProbing module..."<<std::endl;
        auto factory = module->load_function<mir::client::CreateClientPlatform>("create_client_platform");
        std::cout<<"\tdone, trying to create platform..."<<std::endl;
        try
        {
            return factory(context);
        }
        catch(...)
        {

        }
    }
    throw std::runtime_error{"No appropriate client platform module found"};
}
