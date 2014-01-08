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
#include "gbm_buffer.h"

#include <boost/throw_exception.hpp>
#include <GLES2/gl2.h>
#include <glm/gtx/rotate_vector.hpp>

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
    const char* exts = eglQueryString(eglGetCurrentDisplay(), EGL_EXTENSIONS);
    if (exts)
        ext_string = exts;

    if (ext_string.find("EGL_MESA_drm_image") == std::string::npos)
        BOOST_THROW_EXCEPTION(std::runtime_error("EGL implementation doesn't support EGL_MESA_drm_image extension"));

    exts = reinterpret_cast<const char*>(glGetString(GL_EXTENSIONS));
    if (exts)
        ext_string = exts;
    else
        ext_string.clear();

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
    EGLContext shared_context)
    : last_flipped_bufobj{nullptr},
      platform(platform),
      listener(listener),
      drm(platform->drm),
      outputs(outputs),
      surface_gbm{std::move(surface_gbm_param)},
      area(area),
      needs_set_crtc{false}
{
    egl.setup(platform->gbm, surface_gbm.get(), shared_context);

    listener->report_successful_setup_of_native_resources();

    make_current();

    listener->report_successful_egl_make_current_on_construction();

    ensure_egl_image_extensions();

    glClear(GL_COLOR_BUFFER_BIT);

    if (!egl.swap_buffers())
        BOOST_THROW_EXCEPTION(std::runtime_error("Failed to perform initial surface buffer swap"));

    listener->report_successful_egl_buffer_swap_on_construction();

    last_flipped_bufobj = get_front_buffer_object();
    if (!last_flipped_bufobj)
        BOOST_THROW_EXCEPTION(std::runtime_error("Failed to get frontbuffer"));

    for (auto& output : outputs)
    {
        if (!output->set_crtc(last_flipped_bufobj->get_drm_fb_id()))
            BOOST_THROW_EXCEPTION(std::runtime_error("Failed to set DRM crtc"));
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
}

geom::Rectangle mgm::DisplayBuffer::view_area() const
{
    return area;
}

bool mgm::DisplayBuffer::can_bypass() const
{
    return true;
}

void mgm::DisplayBuffer::post_update()
{
    post_update(nullptr);
}

void mgm::DisplayBuffer::post_update(
    std::shared_ptr<graphics::Buffer> bypass_buf)
{
    /*
     * Bring the back buffer to the front and get the buffer object
     * corresponding to the front buffer.
     */
    if (!bypass_buf && !egl.swap_buffers())
        BOOST_THROW_EXCEPTION(std::runtime_error("Failed to perform initial surface buffer swap"));

    mgm::BufferObject *bufobj;
    if (bypass_buf)
    {
        auto native = bypass_buf->native_buffer_handle();
        auto gbm_native = static_cast<mgm::GBMNativeBuffer*>(native.get());
        bufobj = get_buffer_object(gbm_native->bo);
    }
    else
    {
        bufobj = get_front_buffer_object();
    }

    if (!bufobj)
        BOOST_THROW_EXCEPTION(std::runtime_error("Failed to get front buffer object"));

    /*
     * Schedule the current front buffer object for display, and wait
     * for it to be actually displayed (flipped).
     *
     * If the flip fails, release the buffer object to make it available
     * for future rendering.
     */
    if (!needs_set_crtc && !schedule_and_wait_for_page_flip(bufobj))
    {
        if (!bypass_buf)
            bufobj->release();
        BOOST_THROW_EXCEPTION(std::runtime_error("Failed to schedule page flip"));
    }
    else if (needs_set_crtc)
    {
        for (auto& output : outputs)
        {
            if (!output->set_crtc(bufobj->get_drm_fb_id()))
                BOOST_THROW_EXCEPTION(std::runtime_error("Failed to set DRM crtc"));
        }
        needs_set_crtc = false;
    }

    /*
     * Release the last flipped buffer object (which is not displayed anymore)
     * to make it available for future rendering.
     */
    if (last_flipped_bufobj)
        last_flipped_bufobj->release();

    last_flipped_bufobj = bypass_buf ? nullptr : bufobj;

    /*
     * Keep a reference to the buffer being bypassed for the entire duration
     * of the frame. This ensures the buffer doesn't get reused by the client
     * prematurely, which would be seen as tearing.
     * If not bypassing, then bypass_buf will be nullptr.
     */
    last_flipped_bypass_buf = bypass_buf;
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
    auto ret = drmModeAddFB(drm.fd, area.size.width.as_uint32_t(), area.size.height.as_uint32_t(),
                            24, 32, stride, handle, &fb_id);
    if (ret)
        return nullptr;

    /* Create a BufferObject and associate it with the gbm_bo */
    bufobj = new BufferObject{surface_gbm.get(), bo, fb_id};
    gbm_bo_set_user_data(bo, bufobj, bo_user_data_destroy);

    return bufobj;
}


bool mgm::DisplayBuffer::schedule_and_wait_for_page_flip(BufferObject* bufobj)
{
    int page_flips_pending{0};

    /*
     * Schedule the current front buffer object for display. Note that
     * the page flip is asynchronous and synchronized with vertical refresh.
     */
    for (auto& output : outputs)
    {
        if (output->schedule_page_flip(bufobj->get_drm_fb_id()))
            ++page_flips_pending;
    }

    if (page_flips_pending == 0)
        return false;

    for (auto& output : outputs)
    {
        output->wait_for_page_flip();
    }

    return true;
}

void mgm::DisplayBuffer::make_current()
{
    if (!egl.make_current())
    {
        BOOST_THROW_EXCEPTION(std::runtime_error("Failed to make EGL surface current"));
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

void mgm::DisplayBuffer::orient(int degrees)
{
    static const glm::vec3 z_axis(0.0f, 0.0f, 1.0f);

    display_transform = glm::rotate(glm::mat4(), float(degrees), z_axis);

#if 0
    fprintf(stderr, "Orient %d:\n", degrees);
    for (int i = 0; i < 4; ++i)
    {
        fprintf(stderr, "[%3.1f][%3.1f][%3.1f][%3.1f]\n",
               display_transform[i][0],
               display_transform[i][1],
               display_transform[i][2],
               display_transform[i][3]);
    }
#endif
}
