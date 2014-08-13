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

#ifndef QT_MIR_TEST_FRAMEWORK_H
#define QT_MIR_TEST_FRAMEWORK_H

#include <gtest/gtest.h>

#include <core/posix/linux/proc/process/oom_score_adj.h>

#include <Unity/Application/application_manager.h>
#include <Unity/Application/applicationcontroller.h>
#include <Unity/Application/taskcontroller.h>
#include <Unity/Application/proc_info.h>
#include <mirserverconfiguration.h>

#include "mock_application_controller.h"
#include "mock_desktop_file_reader.h"
#include "mock_oom_controller.h"
#include "mock_process_controller.h"
#include "mock_proc_info.h"
#include "mock_session.h"
#include "mock_focus_controller.h"
#include "mock_prompt_session_manager.h"
#include "mock_prompt_session.h"

namespace ms = mir::scene;
using namespace qtmir;

namespace testing
{

class QtMirTestConfiguration: public MirServerConfiguration
{
public:
    QtMirTestConfiguration()
    : MirServerConfiguration(0, nullptr)
    , mock_prompt_session_manager(std::make_shared<testing::MockPromptSessionManager>())
    {
    }

    std::shared_ptr<ms::PromptSessionManager> the_prompt_session_manager() override
    {
        return prompt_session_manager([this]()
           ->std::shared_ptr<ms::PromptSessionManager>
           {
               return the_mock_prompt_session_manager();
           });
    }

    std::shared_ptr<testing::MockPromptSessionManager> the_mock_prompt_session_manager()
    {
        return mock_prompt_session_manager;
    }

    std::shared_ptr<testing::MockPromptSessionManager> mock_prompt_session_manager;
};

class QtMirTest : public ::testing::Test
{
public:
    QtMirTest()
        : processController{
            QSharedPointer<ProcessController::OomController> (
                &oomController,
                [](ProcessController::OomController*){})
        }
        , mirConfig{
            QSharedPointer<QtMirTestConfiguration> (new QtMirTestConfiguration)
        }
        , taskController{
              QSharedPointer<TaskController> (
                  new TaskController(
                      nullptr,
                      QSharedPointer<ApplicationController>(
                          &appController,
                          [](ApplicationController*){}),
                      QSharedPointer<ProcessController>(
                          &processController,
                          [](ProcessController*){})
                  )
              )
        }
        , applicationManager{
            mirConfig,
            taskController,
            QSharedPointer<DesktopFileReader::Factory>(
                &desktopFileReaderFactory,
                [](DesktopFileReader::Factory*){}),
            QSharedPointer<ProcInfo>(&procInfo,[](ProcInfo *){})
        }
    {
    }

    testing::NiceMock<testing::MockOomController> oomController;
    testing::NiceMock<testing::MockProcessController> processController;
    testing::NiceMock<testing::MockApplicationController> appController;
    testing::NiceMock<testing::MockProcInfo> procInfo;
    testing::NiceMock<testing::MockDesktopFileReaderFactory> desktopFileReaderFactory;
    QSharedPointer<QtMirTestConfiguration> mirConfig;
    QSharedPointer<TaskController> taskController;
    ApplicationManager applicationManager;
};
} // namespace testing

#endif // QT_MIR_TEST_FRAMEWORK_H
