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

#define MIR_LOG_COMPONENT "Mesa DisplayBuffer"
#include "display_buffer.h"
#include "platform.h"
#include "kms_output.h"
#include "mir/graphics/display_report.h"
#include "bypass.h"
#include "gbm_buffer.h"
#include "mir/fatal.h"
#include "mir/log.h"

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
      drm(*platform->drm),
      outputs(outputs),
      surface_gbm{std::move(surface_gbm_param)},
      egl{gl_config},
      area(area),
      rotation(rot),
      needs_set_crtc{false},
      page_flips_pending{false}
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

    set_crtc(*scheduled_bufobj);

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

void mgm::DisplayBuffer::set_orientation(MirOrientation const rot, geometry::Rectangle const& a)
{
    rotation = rot;
    area = a;
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
            bypass_candidate = *bypass_it;
            return true;
        }
    }

    return false;
}

void mgm::DisplayBuffer::for_each_display_buffer(
    std::function<void(graphics::DisplayBuffer&)> const& f)
{
    f(*this);
}

void mgm::DisplayBuffer::gl_swap_buffers()
{
    if (!egl.swap_buffers())
        fatal_error("Failed to perform buffer swap");
    bypass_candidate.reset();
}

void mgm::DisplayBuffer::post()
{
    /*
     * We might not have waited for the previous frame to page flip yet.
     * This is good because it maximizes the time available to spend rendering
     * each frame. Just remember wait_for_page_flip() must be called at some
     * point before the next schedule_page_flip().
     */
    wait_for_page_flip();

    last_flipped_bypass_buf = scheduled_bypass_buf;
    scheduled_bypass_buf = nullptr;

    if (last_flipped_bufobj) last_flipped_bufobj->release();
    last_flipped_bufobj = scheduled_bufobj;
    scheduled_bufobj = nullptr;

    if (bypass_candidate)
        post_bypass();
    else
        post_egl();
}

void mgm::DisplayBuffer::post_bypass()
{
    if (outputs.size() == 1)
    {
        //mir::log_info("Start wait");
        //outputs.front()->wait_for_vblank();
        //mir::log_info("Done wait");
    }

    auto bypass_buf = bypass_candidate->buffer();
    auto native = bypass_buf->native_buffer_handle();
    auto gbm_native = static_cast<mgm::GBMNativeBuffer*>(native.get());
    auto bufobj = get_buffer_object(gbm_native->bo);
    if (!bufobj ||
        !(native->flags & mir_buffer_flag_can_scanout) ||
        bypass_buf->size() != geom::Size{fb_width,fb_height})
    {
        // prediction failed
        post_egl();
        return;
    }

    if (needs_set_crtc)
    {
        set_crtc(*bufobj);  // regardless of outputs.size()
    }
    else
    {
        mir::log_info("Start reset");
#if 1
        if (!schedule_page_flip(bufobj))
            fatal_error("Failed to schedule page flip");
#else
        set_crtc(*bufobj);
#endif
        mir::log_info("Done reset");
    }

    scheduled_bypass_buf = bypass_buf;
}

void mgm::DisplayBuffer::post_egl()
{
    mgm::BufferObject *bufobj = get_front_buffer_object();
    if (!bufobj)
        fatal_error("Failed to get front buffer object");

    if (needs_set_crtc)
    {
        set_crtc(*bufobj);
    }
    else if (!schedule_page_flip(bufobj))
    {
        bufobj->release();
        fatal_error("Failed to schedule page flip");
    }

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
    scheduled_bypass_buf = nullptr;
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
    uint32_t handles[4] = {gbm_bo_get_handle(bo).u32, 0, 0, 0};
    uint32_t strides[4] = {gbm_bo_get_stride(bo), 0, 0, 0};
    uint32_t offsets[4] = {0, 0, 0, 0};

    auto format = gbm_bo_get_format(bo);
    /*
     * Mir might use the old GBM_BO_ enum formats, but KMS and the rest of
     * the world need fourcc formats, so convert...
     */
    if (format == GBM_BO_FORMAT_XRGB8888)
        format = GBM_FORMAT_XRGB8888;
    else if (format == GBM_BO_FORMAT_ARGB8888)
        format = GBM_FORMAT_ARGB8888;

    /* Create a KMS FB object with the gbm_bo attached to it. */
    auto ret = drmModeAddFB2(drm.fd, fb_width, fb_height, format,
                             handles, strides, offsets, &fb_id, 0);
    if (ret)
        return nullptr;

    /* Create a BufferObject and associate it with the gbm_bo */
    bufobj = new BufferObject{surface_gbm.get(), bo, fb_id};
    gbm_bo_set_user_data(bo, bufobj, bo_user_data_destroy);

    return bufobj;
}

void mgm::DisplayBuffer::set_crtc(BufferObject const& bufobj)
{
    for (auto& output : outputs)
    {
        if (!output->set_crtc(bufobj.get_drm_fb_id()))
            fatal_error("Failed to set DRM CRTC");
    }
    needs_set_crtc = false;
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
        for (auto& output : outputs)
            output->wait_for_page_flip();

        page_flips_pending = false;
    }
}

void mgm::DisplayBuffer::make_current()
{
    if (!egl.make_current())
    {
        fatal_error("Failed to make EGL surface current");
    }
}

void mgm::DisplayBuffer::release_current()
{
    egl.release_current();
}

void mgm::DisplayBuffer::schedule_set_crtc()
{
    needs_set_crtc = true;
}

