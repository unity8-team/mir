#ifndef MIR_X_NULL_SERVER_SPAWNER_H_
#define MIR_X_NULL_SERVER_SPAWNER_H_

#include <mir/xserver/xserver_launcher.h>

namespace mir
{
namespace X
{
class NullServerContext : public ServerContext
{
public:
    char const* client_connection_string() override;
};

class NullServerSpawner : public ServerSpawner
{
public:
    std::future<std::unique_ptr<ServerContext>> create_server(mir::process::Spawner const& spawner) override;
};

}
}

#endif // MIR_X_NULL_SERVER_SPAWNER_H_
