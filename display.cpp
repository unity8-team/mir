#include "display.h"

#include "screen.h"

#include <mir/graphics/display_configuration.h>
#include <QDebug>

namespace mg = mir::graphics;

// TODO: Listen for display changes and update the list accordingly

Display::Display(mir::DefaultServerConfiguration *config, QObject *parent)
  : QObject(parent)
  , m_mirConfig(config)
{
    std::shared_ptr<mir::graphics::DisplayConfiguration> displayConfig = m_mirConfig->the_display()->configuration();

    displayConfig->for_each_output([this](mg::DisplayConfigurationOutput const& output) {
        if (output.used) {
            auto screen = new Screen(output);
            m_screens.push_back(screen);
        }
    });
}

Display::~Display()
{
    for (auto screen : m_screens)
        delete screen;
    m_screens.clear();
}
