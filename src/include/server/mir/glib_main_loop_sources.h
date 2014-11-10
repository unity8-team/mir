/*
 * Copyright © 2014 Canonical Ltd.
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 3,
 * as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Authored by: Alexandros Frantzis <alexandros.frantzis@canonical.com>
 */

#ifndef MIR_GLIB_MAIN_LOOP_SOURCES_H_
#define MIR_GLIB_MAIN_LOOP_SOURCES_H_

#include <functional>

#include <glib.h>

namespace mir
{
namespace detail
{

class GSourceHandle
{
public:
    explicit GSourceHandle(GSource* gsource);
    GSourceHandle(GSourceHandle&& other);
    ~GSourceHandle();

    void attach(GMainContext* main_context) const;

private:
    GSource* gsource;
};

GSourceHandle make_idle_gsource(int priority, std::function<void()> const& callback);
GSourceHandle make_signal_gsource(int sig, std::function<void(int)> const& callback);

}
}

#endif
