/*
 * Copyright © 2013 Canonical Ltd.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License version 3 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Authored by: Ricardo Mendoza <ricardo.mendoza@canonical.com>
 *              Thomas Voß <thomas.voss@canonical.com>
 */

#ifndef UBUNTU_APPLICATION_ARCHIVE_H_
#define UBUNTU_APPLICATION_ARCHIVE_H_

#include <ubuntu/status.h>
#include <ubuntu/visibility.h>

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct UbuntuApplicationArchive_ UApplicationArchive;

/**
 * \brief Creates a new archive, ownership is transferred to caller.
 * \ingroup application_support
 * \sa u_application_archive_destroy
 * \returns A new archive instance or NULL if not enough memory is available.
 */
UBUNTU_DLL_PUBLIC UApplicationArchive*
u_application_archive_new();

/**
 * \brief Destroys the given archive instance and releases all resources held by the instance.
 * \ingroup application_support
 * \param[in] archive The instance to be destroyed.
 * \post All resources held by the instance are released. The result of any operation invoked on the destroyed instance are undefined.
 */
UBUNTU_DLL_PUBLIC void
u_application_archive_destroy(
    UApplicationArchive *archive);

/**
 * \brief Writes a signed 64-bit integer to the supplied archive
 * \ingroup application_support
 * \returns U_STATUS_SUCCESS if successful, and U_STATUS_ERROR in case of failures.
 * \param[in] archive The archive to write to.
 * \param[in] s The signed 64-bit integer to write to the archive.
 */
UBUNTU_DLL_PUBLIC UStatus
u_application_archive_write(
    UApplicationArchive *archive,
    int64_t s);

/**
 * \brief Writes a string of characters to the supplied archive
 * \ingroup application_support
 * \returns U_STATUS_SUCCESS if successful, and U_STATUS_ERROR in case of failures.
 * \param[in] archive The archive to write to.
 * \param[in] s The string to write.
 * \param[in] size The number of characters to write to the archive.
 */
UBUNTU_DLL_PUBLIC UStatus
u_application_archive_write_stringn(
    UApplicationArchive *archive,
    const char *s,
    size_t size);

/**
 * \brief Writes a string of wide characters to the supplied archive
 * \ingroup application_support
 * \returns U_STATUS_SUCCESS if successful, and U_STATUS_ERROR in case of failures.
 * \param[in] archive The archive to write to.
 * \param[in] s The string to write.
 * \param[in] size The number of characters to write to the archive.
 */
UBUNTU_DLL_PUBLIC UStatus
u_application_archive_write_wstringn(
    UApplicationArchive *archive,
    const wchar_t *s,
    size_t size);

/**
 * \brief Writes a blob of binary data to the supplied archive
 * \ingroup application_support
 * \returns U_STATUS_SUCCESS if successful, and U_STATUS_ERROR in case of failures.
 * \param[in] archive The archive to write to.
 * \param[in] data The binary blob to write.
 * \param[in] size The size of the blob.
 */
UBUNTU_DLL_PUBLIC UStatus
u_application_archive_write_bytes(
    UApplicationArchive *archive,
    const intptr_t *data,
    size_t size);

UBUNTU_DLL_PUBLIC UStatus
u_application_archive_write_begin_blockn(
    UApplicationArchive* archive,
    const char *name,
    size_t size);

UBUNTU_DLL_PUBLIC UStatus
u_application_archive_write_end_blockn(
    UApplicationArchive* archive,
    const char *name,
    size_t size);

/**
 * \brief Reads a signed 64-bit integer from the supplied archive
 * \ingroup application_support
 * \returns U_STATUS_SUCCESS if successful, and U_STATUS_ERROR in case of failures.
 * \param[in] archive The archive to read from.
 * \param[out] s Pointer to memory that receives the signed 64-bit integer.
 */
UBUNTU_DLL_PUBLIC UStatus
u_application_archive_read(
    const UApplicationArchive *archive,
    int64_t *s);

/**
 * \brief Reads a string of characters from the supplied archive
 * \ingroup application_support
 * \returns U_STATUS_SUCCESS if successful, and U_STATUS_ERROR in case of failures.
 * \param[in] archive The archive to read from.
 * \param[out] s Pointer to memory that receives the string.
 * \param[out] size Pointer to memory that receives the size of the string.
 */
UBUNTU_DLL_PUBLIC UStatus
u_application_archive_read_stringn(
    const UApplicationArchive *archive,
    const char **s,
    size_t *size);

/**
 * \brief Reads a string of wide characters from the supplied archive
 * \ingroup application_support
 * \returns U_STATUS_SUCCESS if successful, and U_STATUS_ERROR in case of failures.
 * \param[in] archive The archive to read from.
 * \param[out] s Pointer to memory that receives the wide string.
 * \param[out] size Pointer to memory that receives the size of the string.
 */
UBUNTU_DLL_PUBLIC UStatus
u_application_archive_read_wstringn(
    UApplicationArchive *archive,
    const wchar_t *s,
    size_t size);

/**
 * \brief Reads a blob of binary data from the supplied archive
 * \ingroup application_support
 * \returns U_STATUS_SUCCESS if successful, and U_STATUS_ERROR in case of failures.
 * \param[in] archive The archive to read from.
 * \param[out] data Pointer to memory that receives the binary data.
 * \param[out] size Pointer to memory that receives the size of the blob.
 */
UBUNTU_DLL_PUBLIC UStatus
u_application_archive_read_bytes(
    UApplicationArchive *archive,
    const intptr_t *data,
    size_t size);

#ifdef __cplusplus
}
#endif

#endif /* UBUNTU_APPLICATION_ARCHIVE_H_ */
