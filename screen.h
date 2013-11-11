#ifndef SCREEN_H
#define SCREEN_H

#include <qpa/qplatformscreen.h>
#include "mir/graphics/display_configuration.h"

class Screen : public QPlatformScreen
{
public:
    Screen(mir::graphics::DisplayConfigurationOutput const&);

    QRect geometry() const override { return m_geometry; }

    int depth() const override { return m_depth; }
    QImage::Format format() const override { return m_format; }

    virtual QSizeF physicalSize() const { return m_physicalSize; }

    virtual qreal refreshRate() const { return m_refreshRate; }

private:
    void readMirDisplayConfiguration(mir::graphics::DisplayConfigurationOutput const&);

    QRect m_geometry;
    int m_depth;
    QImage::Format m_format;
    QSizeF m_physicalSize;
    qreal m_refreshRate;
};

#endif // SCREEN_H
