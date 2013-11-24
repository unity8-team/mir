/*
 * Copyright (C) 2013 Canonical, Ltd.
 *
 * This program is free software: you can redistribute it and/or modify it under
 * the terms of the GNU Lesser General Public License version 3, as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranties of MERCHANTABILITY,
 * SATISFACTORY QUALITY, or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Author: Gerry Boland <gerry.boland@canonical.com>
 */

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

    QSizeF physicalSize() const override { return m_physicalSize; }

    qreal refreshRate() const override { return m_refreshRate; }

private:
    void readMirDisplayConfiguration(mir::graphics::DisplayConfigurationOutput const&);

    QRect m_geometry;
    int m_depth;
    QImage::Format m_format;
    QSizeF m_physicalSize;
    qreal m_refreshRate;
};

#endif // SCREEN_H
