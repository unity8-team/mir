/*
 * Copyright © 2015 Canonical Ltd.
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
 * Authored by: Kevin DuBois <kevin.dubois@canonical.com>
 */

#include "display_group.h"
#include "buffer_object.h"

#include "platform.h"
#include "kms_output.h"
#include "mir/graphics/display_report.h"
#include "bypass.h"
#include "gbm_buffer.h"
#include "mir/fatal.h"
#include <boost/throw_exception.hpp>
#include <GLES2/gl2.h>

#include <stdexcept>

namespace mgm = mir::graphics::mesa;
namespace geom = mir::geometry;

mgm::DisplayGroup::DisplayGroup(
    std::shared_ptr<Platform> const& platform,
    std::shared_ptr<DisplayReport> const& listener,
    std::vector<std::shared_ptr<KMSOutput>> const& outputs,
    GBMSurfaceUPtr surface_gbm,
    geometry::Rectangle const& area,
    MirOrientation rot,
    GLConfig const& gl_config,
    EGLContext shared_context) :
    db{platform, listener, std::move(surface_gbm), area, rot, gl_config, shared_context},
    platform(platform),
    listener(listener),
    needs_set_crtc{false},
    page_flips_pending{false},
    outputs(outputs),
    last_flipped_bufobj{nullptr},
    scheduled_bufobj{db.buffer_obj_to_be_posted()},
    last_flipped_bypass_buf{nullptr}
{
    for (auto& output : outputs)
    {
        if (!output->set_crtc(scheduled_bufobj->get_drm_fb_id()))
            fatal_error("Failed to set DRM crtc");
    }
}

mgm::DisplayGroup::~DisplayGroup()
{
    /*
     * There is no need to destroy last_flipped_bufobj manually.
     * It will be destroyed when its gbm_surface gets destroyed.
     */
    if (last_flipped_bufobj)
        last_flipped_bufobj->release();

    if (scheduled_bufobj)
        scheduled_bufobj->release();
}

void mgm::DisplayGroup::for_each_display_buffer(
    std::function<void(graphics::DisplayBuffer&)> const& f)
{
    f(db);
}

void mgm::DisplayGroup::post()
{
    /*
     * We might not have waited for the previous frame to page flip yet.
     * This is good because it maximizes the time available to spend rendering
     * each frame. Just remember wait_for_page_flip() must be called at some
     * point before the next schedule_page_flip().
     */
    wait_for_page_flip();

    /*
     * Switching from bypass to compositing? Now is the earliest safe time
     * we can unreference the bypass buffer...
     */
    if (scheduled_bufobj)
        last_flipped_bypass_buf = nullptr;
    /*
     * Release the last flipped buffer object (which is not displayed anymore)
     * to make it available for future rendering.
     */
    if (last_flipped_bufobj)
        last_flipped_bufobj->release();

    last_flipped_bufobj = scheduled_bufobj;
    scheduled_bufobj = nullptr;

    mgm::BufferObject *bufobj = db.buffer_obj_to_be_posted();
    auto bypass_buffer = db.bypass_buffer();
    /*
     * Schedule the current front buffer object for display, and wait
     * for it to be actually displayed (flipped).
     *
     * If the flip fails, release the buffer object to make it available
     * for future rendering.
     */
    if (!needs_set_crtc && !schedule_page_flip(bufobj))
    {
        if (!bypass_buffer)
            bufobj->release();
        fatal_error("Failed to schedule page flip");
    }
    else if (needs_set_crtc)
    {
        for (auto& output : outputs)
        {
            if (!output->set_crtc(bufobj->get_drm_fb_id()))
                fatal_error("Failed to set DRM crtc");
        }
        needs_set_crtc = false;
    }

    if (bypass_buffer)
    {
        /*
         * For composited frames we defer wait_for_page_flip till just before
         * the next frame, but not for bypass frames. Deferring the flip of
         * bypass frames would increase the time we held
         * last_flipped_bypass_buf unacceptably, resulting in client stuttering
         * unless we allocate more buffers (which I'm trying to avoid).
         * Also, bypass does not need the deferred page flip because it has
         * no compositing/rendering step for which to save time for.
         */
        wait_for_page_flip();
        scheduled_bufobj = nullptr;

        /*
         * Keep a reference to the buffer being bypassed for the entire
         * duration of the frame. This ensures the buffer doesn't get reused by
         * the client while its on-screen, which would be seen as tearing or
         * worse.
         */
        last_flipped_bypass_buf = db.bypass_buffer();
    }
    else
    {
        /*
         * Not in clone mode? We can afford to wait for the page flip then,
         * making us double-buffered (noticeably less laggy than the triple
         * buffering that clone mode requires).
         */
        if (outputs.size() == 1)
        {
            wait_for_page_flip();

            /*
             * bufobj is now physically on screen. Release the old frame...
             */
            if (last_flipped_bufobj)
            {
                last_flipped_bufobj->release();
                last_flipped_bufobj = nullptr;
            }

            /*
             * last_flipped_bufobj will be set correctly on the next iteration
             * Don't do it here or else bufobj would be released while still
             * on screen (hence tearing and artefacts).
             */
        }

        scheduled_bufobj = bufobj;
    }
}

void mgm::DisplayGroup::schedule_set_crtc()
{
    needs_set_crtc = true;
}

bool mgm::DisplayGroup::schedule_page_flip(BufferObject* bufobj)
{
    /*
     * Schedule the current front buffer object for display. Note that
     * the page flip is asynchronous and synchronized with vertical refresh.
     */
    for (auto& output : outputs)
    {
        if (output->schedule_page_flip(bufobj->get_drm_fb_id()))
            page_flips_pending = true;
    }

    return page_flips_pending;
}

void mgm::DisplayGroup::wait_for_page_flip()
{
    if (page_flips_pending)
    {
        for (auto& output : outputs)
            output->wait_for_page_flip();

        page_flips_pending = false;
    }
}
