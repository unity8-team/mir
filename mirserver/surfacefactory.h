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
 */

#ifndef SURFACESOURCE_H
#define SURFACESOURCE_H

#include <QObject>

#include "mir/shell/surface_factory.h"

Q_DECLARE_METATYPE(std::shared_ptr<mir::shell::Surface>)

class SurfaceFactory : public QObject, public mir::shell::SurfaceFactory
{
    Q_OBJECT

public:
    SurfaceFactory(std::shared_ptr<mir::shell::SurfaceFactory> const& surfaceFactory,
                  QObject *parent = 0);
    ~SurfaceFactory();

    //Called by Mir server
    std::shared_ptr<mir::shell::Surface> create_surface(
            mir::shell::Session* session,
            mir::shell::SurfaceCreationParameters const& params,
            mir::frontend::SurfaceId id,
            std::shared_ptr<mir::frontend::EventSink> const& sink) override;

    // Called by shell
    std::shared_ptr<mir::shell::Surface> shellSurface() const;

Q_SIGNALS:
    void shellSurfaceCreated(std::shared_ptr<mir::shell::Surface> const& surface);

private:
    const std::shared_ptr<mir::shell::SurfaceFactory> m_surfaceFactory;
    std::shared_ptr<mir::shell::Surface> m_shellSurface;
};

#endif // SURFACESOURCE_H
