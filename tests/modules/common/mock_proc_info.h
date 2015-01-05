/*
 * Copyright (C) 2014 Canonical, Ltd.
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
 *
 */

#ifndef MOCK_PROC_INFO_H
#define MOCK_PROC_INFO_H

#include <Unity/Application/proc_info.h>

#include <gmock/gmock.h>

namespace testing
{
struct MockProcInfo : public qtmir::ProcInfo
{
    MOCK_METHOD1(command_line, QByteArray(quint64));
    std::unique_ptr<CommandLine> commandLine(quint64 pid)
    {
      return std::unique_ptr<CommandLine>(new CommandLine{command_line(pid)});
    }
};
}

#endif // MOCK_PROC_INFO_H
