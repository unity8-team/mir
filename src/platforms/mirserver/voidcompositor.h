#ifndef VOIDDISPLAYBUFFERCOMPOSITORFACTORY_H
#define VOIDDISPLAYBUFFERCOMPOSITORFACTORY_H

#include "mir/compositor/compositor.h"

class VoidCompositor : public mir::compositor::Compositor
{
public:
    VoidCompositor();

    void start();
    void stop();
};

#endif // VOIDDISPLAYBUFFERCOMPOSITORFACTORY_H
