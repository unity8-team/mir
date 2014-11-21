/*
 * Copyright © 2013 Canonical Ltd.
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
 * Authored by: Alexandros Frantzis <alexandros.frantzis@canonical.com>
 */

#include "display_buffer.h"
#include "platform.h"
#include "kms_output.h"
#include "mir/graphics/display_report.h"
#include "bypass.h"
#include "gbm_buffer.h"
#include "mir/fatal.h"
#include "mir/time/steady_clock.h"
#include "mir/logging/logger.h"

#include <boost/throw_exception.hpp>
#include <GLES2/gl2.h>

#include <stdexcept>

namespace mgm = mir::graphics::mesa;
namespace geom = mir::geometry;

class mgm::BufferObject
{
public:
    BufferObject(gbm_surface* surface, gbm_bo* bo, uint32_t drm_fb_id)
        : surface{surface}, bo{bo}, drm_fb_id{drm_fb_id}
    {
    }

    ~BufferObject()
    {
        if (drm_fb_id)
        {
            int drm_fd = gbm_device_get_fd(gbm_bo_get_device(bo));
            drmModeRmFB(drm_fd, drm_fb_id);
        }
    }

    void release() const
    {
        gbm_surface_release_buffer(surface, bo);
    }

    uint32_t get_drm_fb_id() const
    {
        return drm_fb_id;
    }

private:
    gbm_surface *surface;
    gbm_bo *bo;
    uint32_t drm_fb_id;
};

namespace
{

void bo_user_data_destroy(gbm_bo* /*bo*/, void *data)
{
    auto bufobj = static_cast<mgm::BufferObject*>(data);
    delete bufobj;
}

void ensure_egl_image_extensions()
{
    std::string ext_string;
    const char* exts = reinterpret_cast<const char*>(glGetString(GL_EXTENSIONS));
    if (exts)
        ext_string = exts;

    if (ext_string.find("GL_OES_EGL_image") == std::string::npos)
        BOOST_THROW_EXCEPTION(std::runtime_error("GLES2 implementation doesn't support GL_OES_EGL_image extension"));
}

}

mgm::DisplayBuffer::DisplayBuffer(
    std::shared_ptr<Platform> const& platform,
    std::shared_ptr<DisplayReport> const& listener,
    std::vector<std::shared_ptr<KMSOutput>> const& outputs,
    GBMSurfaceUPtr surface_gbm_param,
    geom::Rectangle const& area,
    MirOrientation rot,
    GLConfig const& gl_config,
    EGLContext shared_context)
    : last_flipped_bufobj{nullptr},
      scheduled_bufobj{nullptr},
      platform(platform),
      listener(listener),
      // Making clock mockable via the constructor chain appears to increase
      // coupling unacceptably. Maybe instantiate a universal "Clock" per
      // process that is still mockable...
      clock(std::make_shared<time::SteadyClock>()),
      drm(*platform->drm),
      outputs(outputs),
      surface_gbm{std::move(surface_gbm_param)},
      egl{gl_config},
      area(area),
      rotation(rot),
      needs_set_crtc{false},
      page_flips_pending{false},
      last_report(clock->now())
{
    uint32_t area_width = area.size.width.as_uint32_t();
    uint32_t area_height = area.size.height.as_uint32_t();
    if (rotation == mir_orientation_left || rotation == mir_orientation_right)
    {
        fb_width = area_height;
        fb_height = area_width;
    }
    else
    {
        fb_width = area_width;
        fb_height = area_height;
    }

    egl.setup(platform->gbm, surface_gbm.get(), shared_context);

    listener->report_successful_setup_of_native_resources();

    make_current();

    listener->report_successful_egl_make_current_on_construction();

    ensure_egl_image_extensions();

    glClear(GL_COLOR_BUFFER_BIT);

    if (!egl.swap_buffers())
        fatal_error("Failed to perform initial surface buffer swap");

    listener->report_successful_egl_buffer_swap_on_construction();

    scheduled_bufobj = get_front_buffer_object();
    if (!scheduled_bufobj)
        fatal_error("Failed to get frontbuffer");

    for (auto& output : outputs)
    {
        if (!output->set_crtc(scheduled_bufobj->get_drm_fb_id()))
            fatal_error("Failed to set DRM crtc");
    }

    egl.release_current();

    listener->report_successful_drm_mode_set_crtc_on_construction();
    listener->report_successful_display_construction();
    egl.report_egl_configuration(
        [&listener] (EGLDisplay disp, EGLConfig cfg)
        {
            listener->report_egl_configuration(disp, cfg);
        });
}

mgm::DisplayBuffer::~DisplayBuffer()
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

geom::Rectangle mgm::DisplayBuffer::view_area() const
{
    return area;
}

MirOrientation mgm::DisplayBuffer::orientation() const
{
    // Tell the renderer to do the rotation, since we're not doing it here.
    return rotation;
}

bool mgm::DisplayBuffer::uses_alpha() const
{
    return false;
}

bool mgm::DisplayBuffer::post_renderables_if_optimizable(RenderableList const& renderable_list)
{
    if ((rotation == mir_orientation_normal) &&
       (platform->bypass_option() == mgm::BypassOption::allowed))
    {
        mgm::BypassMatch bypass_match(area);
        auto bypass_it = std::find_if(renderable_list.rbegin(), renderable_list.rend(), bypass_match);
        if (bypass_it != renderable_list.rend())
        {
            auto bypass_buf = (*bypass_it)->buffer();
            if (bypass_buf->can_bypass())
            {
                post_update(bypass_buf);
                return true;
            }
        }
    }

    return false;
}

void mgm::DisplayBuffer::post_update()
{
    post_update(nullptr);
}

void mgm::DisplayBuffer::post_update(
    std::shared_ptr<graphics::Buffer> bypass_buf)
{
    /*
     * If the last frame was composited then we haven't waited for the
     * page flips yet. This is good because it maximizes the time available
     * to spend rendering each frame. However we have to wait here, because
     * it will be unsafe to swap_buffers before guaranteeing the previous
     * page flip finished.
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

    mgm::BufferObject *bufobj;
    if (bypass_buf)
    {
        auto native = bypass_buf->native_buffer_handle();
        auto gbm_native = static_cast<mgm::GBMNativeBuffer*>(native.get());
        bufobj = get_buffer_object(gbm_native->bo);
    }
    else
    {
        if (!egl.swap_buffers())
            fatal_error("Failed to perform buffer swap");
        bufobj = get_front_buffer_object();
    }

    if (!bufobj)
        fatal_error("Failed to get front buffer object");

    /*
     * Schedule the current front buffer object for display, and wait
     * for it to be actually displayed (flipped).
     *
     * If the flip fails, release the buffer object to make it available
     * for future rendering.
     */
    if (!needs_set_crtc && !schedule_page_flip(bufobj))
    {
        if (!bypass_buf)
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

    if (bypass_buf)
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
        last_flipped_bypass_buf = bypass_buf;
    }
    else
    {
        if (outputs.size() == 1) wait_for_page_flip(); // XXX
        scheduled_bufobj = bufobj;
    }
}

mgm::BufferObject* mgm::DisplayBuffer::get_front_buffer_object()
{
    auto front = gbm_surface_lock_front_buffer(surface_gbm.get());
    auto ret = get_buffer_object(front);

    if (!ret)
        gbm_surface_release_buffer(surface_gbm.get(), front);

    return ret;
}

mgm::BufferObject* mgm::DisplayBuffer::get_buffer_object(
    struct gbm_bo *bo)
{
    if (!bo)
        return nullptr;

    /*
     * Check if we have already set up this gbm_bo (the gbm implementation is
     * free to reuse gbm_bos). If so, return the associated BufferObject.
     */
    auto bufobj = static_cast<BufferObject*>(gbm_bo_get_user_data(bo));
    if (bufobj)
        return bufobj;

    uint32_t fb_id{0};
    auto handle = gbm_bo_get_handle(bo).u32;
    auto stride = gbm_bo_get_stride(bo);

    /* Create a KMS FB object with the gbm_bo attached to it. */
    auto ret = drmModeAddFB(drm.fd, fb_width, fb_height,
                            24, 32, stride, handle, &fb_id);
    if (ret)
        return nullptr;

    /* Create a BufferObject and associate it with the gbm_bo */
    bufobj = new BufferObject{surface_gbm.get(), bo, fb_id};
    gbm_bo_set_user_data(bo, bufobj, bo_user_data_destroy);

    return bufobj;
}


bool mgm::DisplayBuffer::schedule_page_flip(BufferObject* bufobj)
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

void mgm::DisplayBuffer::wait_for_page_flip()
{
    if (page_flips_pending)
    {
        unsigned int max_delta = 0;

        for (auto& output : outputs)
        {
            auto d = output->wait_for_page_flip();
            if (d > max_delta)
                max_delta = d;
        }

        auto now = clock->now();
        // Render time _rounded_up_ to the nearest frame time, since we had
        // to include the wait for page flip...
        auto render_time = now - render_start;
        int render_ms =
            std::chrono::duration_cast<std::chrono::milliseconds>
                        (render_time).count();

        /*
         * We can reliably detect skipped frames (max_delta > 1) but need
         * to be careful to check that's not just due to being idle.
         * If the display has been idle then we'll fail the max_delta test,
         * so we need to check the render time too, to avoid reporting idling
         * as a frame skip.
         */
        int average_frame_time = 16; // TODO
        if (max_delta > 1 && render_ms >= 2*average_frame_time)
            ++skips;

        if (now - last_report > std::chrono::seconds(10))
        {
            last_report = now;
            if (skips > 0)
            {
                skips = 0;
                mir::logging::log(mir::logging::Severity::warning,
                                  "BUG - frames are being skipped. "
                                  "Either your compositor is too slow or "
                                  "you have a graphics driver bug.");
            }
        }

        page_flips_pending = false;
    }

    // Conservative start of GL or overlay/bypass rendering
    render_start = clock->now();
}

void mgm::DisplayBuffer::make_current()
{
    if (!egl.make_current())
    {
        fatal_error("Failed to make EGL surface current");
    }

    // More precise start of GL rendering (modulo the time spent idle waiting
    // for a frame to be scheduled).
    render_start = clock->now();
}

void mgm::DisplayBuffer::release_current()
{
    egl.release_current();
}

void mgm::DisplayBuffer::schedule_set_crtc()
{
    needs_set_crtc = true;
}

