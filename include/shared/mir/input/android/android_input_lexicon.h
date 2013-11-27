/*
 * Copyright © 2012 Canonical Ltd.
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License version 3,
 * as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Authored by: Robert Carr <robert.carr@canonical.com>
 *              Daniel d'Andradra <daniel.dandrada@canonical.com>
 */

#ifndef MIR_INPUT_ANDROID_INPUT_LEXICON_H_
#define MIR_INPUT_ANDROID_INPUT_LEXICON_H_

#include "mir_toolkit/event.h"

namespace android
{
class InputEvent;
}

namespace droidinput = android;

namespace mir
{
namespace input
{
namespace android
{

/// The Lexicon translates droidinput event types to MirEvent types prior to
/// shell or client handling.
class Lexicon
{
public:
    static void translate(const droidinput::InputEvent *android_event, MirEvent &mir_event);
};

}
}
}

#endif // MIR_INPUT_ANDROID_INPUT_LEXICON_H_
