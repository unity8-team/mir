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

#include <stdint.h>
#include "ubuntu/status.h"

struct UApplicationArchive;

UApplicationArchive*
u_application_archive_new();

void
u_application_archive_destroy(
	UApplicationArchive *archive);

UStatus
u_application_archive_write(
	UApplicationArchive *archive,
	int8_t c);

UStatus
u_application_archive_write(
	UApplicationArchive *archive,
	uint8_t c);

UStatus
u_application_archive_write(
	UApplicationArchive *archive,
	int16_t s);

UStatus
u_application_archive_write(
	UApplicationArchive *archive,
	uint16_t s);

UStatus
u_application_archive_write(
	UApplicationArchive *archive,
	int32_t s);

UStatus
u_application_archive_write(
	UApplicationArchive *archive,
	uint32_t s);

UStatus
u_application_archive_write(
	UApplicationArchive *archive,
	int64_t s);

UStatus
u_application_archive_write(
	UApplicationArchive *archive,
	uint64_t s);

UStatus
u_application_archive_write_stringn(
	UApplicationArchive *archive,
	const char *s,
	size_t size);

UStatus
u_application_archive_write_wstringn(
	UApplicationArchive *archive,
	const wchar_t *s,
	size_t size);

UStatus
u_application_archive_write_bytes(
	UApplicationArchive *archive,
	const intptr_t *data,
	size_t size);

UStatus
u_application_archive_write_begin_blockn(
	UApplicationArchive* archive,
	const char *name,
	size_t size);

UStatus
u_application_archive_write_end_blockn(
	UApplicationArchive* archive,
	const char *name,
	size_t size);

UStatus
u_application_archive_read(
	const UApplicationArchive *archive,
	int8_t *c);

UStatus
u_application_archive_read(
	const UApplicationArchive *archive,
	uint8_t *c);

UStatus
u_application_archive_read(
	const UApplicationArchive *archive,
	int16_t *s);

UStatus
u_application_archive_read(
	const UApplicationArchive *archive,
	uint16_t *s);

UStatus
u_application_archive_read(
	const UApplicationArchive *archive,
	int32_t *s);

UStatus
u_application_archive_read(
	const UApplicationArchive *archive,
	uint32_t *s);

UStatus
u_application_archive_read(
	const UApplicationArchive *archive,
	int64_t *s);

UStatus
u_application_archive_read(
	const UApplicationArchive *archive,
	uint64_t *s);

UStatus
u_application_archive_read_stringn(
	const UApplicationArchive *archive,
	const char **s,
	size_t *size);

UStatus
u_application_archive_read_wstringn(
	UApplicationArchive *archive,
	const wchar_t *s,
	size_t size);

UStatus
u_application_archive_read_bytes(
	UApplicationArchive *archive,
	const intptr_t *data,
	size_t size);

#endif /* UBUNTU_APPLICATION_ARCHIVE_H_ */
