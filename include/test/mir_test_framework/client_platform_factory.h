#ifndef MIR_TEST_FRAMEWORK_CLIENT_PLATFORM_FACTORY_H_
#define MIR_TEST_FRAMEWORK_CLIENT_PLATFORM_FACTORY_H_


#include "mir/shared_library.h"
#include "src/client/client_platform_factory.h"
#include "executable_path.h"
#include "mir_test_doubles/mock_client_context.h"

namespace mtd = mir::test::doubles;

namespace mir_test_framework
{
std::shared_ptr<mir::SharedLibrary> platform_library;

std::shared_ptr<mir::client::ClientPlatform> create_android_client_platform()
{
    using namespace testing;
    mtd::MockClientContext ctx;
    ON_CALL(ctx, populate(_))
        .WillByDefault(Invoke([](MirPlatformPackage& package) { ::memset(&package, 0, sizeof(package)); }));
    platform_library = std::make_shared<mir::SharedLibrary>(library_path() + "/client-platform-android.so");
    auto platform_factory = platform_library->load_function<mir::client::CreateClientPlatform>("create_client_platform");
    return platform_factory(&ctx);
}

std::shared_ptr<mir::client::ClientPlatform> create_mesa_client_platform()
{
    using namespace testing;
    mtd::MockClientContext ctx;
    ON_CALL(ctx, populate(_))
        .WillByDefault(Invoke([](MirPlatformPackage& package)
                              {
                                  ::memset(&package, 0, sizeof(package));
                                  package.fd_items = 1;
                              }));
    platform_library = std::make_shared<mir::SharedLibrary>(library_path() + "/client-platform-mesa.so");
    auto platform_factory = platform_library->load_function<mir::client::CreateClientPlatform>("create_client_platform");
    return platform_factory(&ctx);
}

std::shared_ptr<mir::SharedLibrary>
get_platform_library()
{
    if (!platform_library)
    {
        throw std::logic_error{"Must call one of create_*_client_platform() before calling get_platform_library()"};
    }
    return platform_library;
}

}

#endif // MIR_TEST_FRAMEWORK_CLIENT_PLATFORM_FACTORY_H_
