#ifndef QPA_MIR_INPUT_CHANNEL_H
#define QPA_MIR_INPUT_CHANNEL_H

#include <mir/input/input_channel.h>
#include <androidfw/InputTransport.h>

class MirInputChannel : public mir::input::InputChannel
{
public:
    MirInputChannel() {
        android::InputChannel::openInputFdPair(mServerFd, mClientFd);
    }

    int client_fd() const override { return mClientFd; }
    int server_fd() const override { return mServerFd; }
private:
    int mClientFd;
    int mServerFd;
};

#endif // QPA_MIR_INPUT_CHANNEL_H
