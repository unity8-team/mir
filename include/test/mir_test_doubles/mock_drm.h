/*
 * Copyright © 2012 Canonical Ltd.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 3 as
 * published by the Free Software Foundation.
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

#ifndef MIR_TEST_DOUBLES_MOCK_DRM_H_
#define MIR_TEST_DOUBLES_MOCK_DRM_H_

#include <gmock/gmock.h>

#include <xf86drm.h>
#include <xf86drmMode.h>

namespace mir
{
namespace geometry
{
struct Size;
}

namespace test
{
namespace doubles
{

class FakeDRMResources
{
public:
    FakeDRMResources();
    ~FakeDRMResources();

    int fd() const;
    int write_fd() const;
    drmModeRes* resources_ptr();

    void add_crtc(uint32_t id, drmModeModeInfo mode);
    void add_encoder(uint32_t encoder_id, uint32_t crtc_id, uint32_t possible_crtcs_mask);
    void add_connector(uint32_t connector_id, uint32_t type, drmModeConnection connection,
                       uint32_t encoder_id, std::vector<drmModeModeInfo>& modes,
                       std::vector<uint32_t>& possible_encoder_ids,
                       geometry::Size const& physical_size);

    void prepare();
    void reset();

    drmModeCrtc* find_crtc(uint32_t id);
    drmModeEncoder* find_encoder(uint32_t id);
    drmModeConnector* find_connector(uint32_t id);

    enum ModePreference {NormalMode, PreferredMode};
    static drmModeModeInfo create_mode(uint16_t hdisplay, uint16_t vdisplay,
                                       uint32_t clock, uint16_t htotal, uint16_t vtotal,
                                       ModePreference preferred);

private:
    int pipe_fds[2];

    drmModeRes resources;
    std::vector<drmModeCrtc> crtcs;
    std::vector<drmModeEncoder> encoders;
    std::vector<drmModeConnector> connectors;

    std::vector<uint32_t> crtc_ids;
    std::vector<uint32_t> encoder_ids;
    std::vector<uint32_t> connector_ids;

    std::vector<drmModeModeInfo> modes;
    std::vector<drmModeModeInfo> modes_empty;
    std::vector<uint32_t> connector_encoder_ids;
};

class MockDRM
{
public:
    MockDRM();
    ~MockDRM() noexcept;

    MOCK_METHOD3(open, int(char const* path, int flags, mode_t mode));
    MOCK_METHOD2(drmOpen, int(const char *name, const char *busid));
    MOCK_METHOD1(drmClose, int(int fd));
    MOCK_METHOD3(drmIoctl, int(int fd, unsigned long request, void *arg));

    MOCK_METHOD1(drmModeGetResources, drmModeResPtr(int fd));
    MOCK_METHOD2(drmModeGetConnector, drmModeConnectorPtr(int fd, uint32_t connectorId));
    MOCK_METHOD2(drmModeGetEncoder, drmModeEncoderPtr(int fd, uint32_t encoder_id));
    MOCK_METHOD2(drmModeGetCrtc, drmModeCrtcPtr(int fd, uint32_t crtcId));
    MOCK_METHOD8(drmModeSetCrtc, int(int fd, uint32_t crtcId, uint32_t bufferId,
                                     uint32_t x, uint32_t y, uint32_t *connectors,
                                     int count, drmModeModeInfoPtr mode));

    MOCK_METHOD1(drmModeFreeResources, void(drmModeResPtr ptr));
    MOCK_METHOD1(drmModeFreeConnector, void(drmModeConnectorPtr ptr));
    MOCK_METHOD1(drmModeFreeEncoder, void(drmModeEncoderPtr ptr));
    MOCK_METHOD1(drmModeFreeCrtc, void(drmModeCrtcPtr ptr));

    MOCK_METHOD8(drmModeAddFB, int(int fd, uint32_t width, uint32_t height,
                                   uint8_t depth, uint8_t bpp, uint32_t pitch,
                                   uint32_t bo_handle, uint32_t *buf_id));
    MOCK_METHOD2(drmModeRmFB, int(int fd, uint32_t bufferId));

    MOCK_METHOD5(drmModePageFlip, int(int fd, uint32_t crtc_id, uint32_t fb_id,
                                                  uint32_t flags, void *user_data));
    MOCK_METHOD2(drmHandleEvent, int(int fd, drmEventContextPtr evctx));

    MOCK_METHOD2(drmGetMagic, int(int fd, drm_magic_t *magic));
    MOCK_METHOD2(drmAuthMagic, int(int fd, drm_magic_t magic));

    MOCK_METHOD4(drmPrimeHandleToFD, int(int fd, uint32_t handle, uint32_t flags, int *prime_fd));
    MOCK_METHOD3(drmPrimeFDToHandle, int(int fd, int prime_fd, uint32_t *handle));

    MOCK_METHOD1(drmSetMaster, int(int fd));
    MOCK_METHOD1(drmDropMaster, int(int fd));

    MOCK_METHOD5(drmModeSetCursor, int (int fd, uint32_t crtcId, uint32_t bo_handle, uint32_t width, uint32_t height));
    MOCK_METHOD4(drmModeMoveCursor,int (int fd, uint32_t crtcId, int x, int y));

    MOCK_METHOD2(drmSetInterfaceVersion, int (int fd, drmSetVersion* sv));
    MOCK_METHOD1(drmGetBusid, char* (int fd));

    FakeDRMResources fake_drm;
};

}
}
}

#endif /* MIR_TEST_DOUBLES_DRM_MOCK_H_ */
