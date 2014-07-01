/*
 * Copyright (C) 2013 Canonical, Ltd.
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

#ifndef MOCK_DESKTOP_FILE_READER_H
#define MOCK_DESKTOP_FILE_READER_H

#include <Unity/Application/desktopfilereader.h>

#include <gmock/gmock.h>

namespace testing
{
struct MockDesktopFileReader : public qtmir::DesktopFileReader
{
    MockDesktopFileReader(const QString &appId, const QFileInfo& fileInfo)
        : DesktopFileReader(appId, fileInfo)
    {
        using namespace ::testing;

        ON_CALL(*this, file()).WillByDefault(Invoke(this, &MockDesktopFileReader::doFile));
        ON_CALL(*this, appId()).WillByDefault(Invoke(this, &MockDesktopFileReader::doAppId));
        ON_CALL(*this, name()).WillByDefault(Invoke(this, &MockDesktopFileReader::doName));
        ON_CALL(*this, comment()).WillByDefault(Invoke(this, &MockDesktopFileReader::doComment));
        ON_CALL(*this, icon()).WillByDefault(Invoke(this, &MockDesktopFileReader::doIcon));
        ON_CALL(*this, exec()).WillByDefault(Invoke(this, &MockDesktopFileReader::doExec));
        ON_CALL(*this, path()).WillByDefault(Invoke(this, &MockDesktopFileReader::doPath));
        ON_CALL(*this, stageHint()).WillByDefault(Invoke(this, &MockDesktopFileReader::doStageHint));
        ON_CALL(*this, loaded()).WillByDefault(Invoke(this, &MockDesktopFileReader::doLoaded));
    }

    MOCK_CONST_METHOD0(file, QString());
    MOCK_CONST_METHOD0(appId, QString ());
    MOCK_CONST_METHOD0(name, QString());
    MOCK_CONST_METHOD0(comment, QString());
    MOCK_CONST_METHOD0(icon, QString());
    MOCK_CONST_METHOD0(exec, QString());
    MOCK_CONST_METHOD0(path, QString());
    MOCK_CONST_METHOD0(stageHint, QString());
    MOCK_CONST_METHOD0(loaded, bool());

    QString doFile() const
    {
        return DesktopFileReader::file();
    }

    QString doAppId() const
    {
        return DesktopFileReader::appId();
    }

    QString doName() const
    {
        return DesktopFileReader::name();
    }

    QString doComment() const
    {
        return DesktopFileReader::comment();
    }

    QString doIcon() const
    {
        return DesktopFileReader::icon();
    }

    QString doExec() const
    {
        return DesktopFileReader::exec();
    }

    QString doPath() const
    {
        return DesktopFileReader::path();
    }

    QString doStageHint() const
    {
        return DesktopFileReader::stageHint();
    }

    bool doLoaded() const
    {
        return DesktopFileReader::loaded();
    }
};

struct MockDesktopFileReaderFactory : public qtmir::DesktopFileReader::Factory
{
    MockDesktopFileReaderFactory()
    {
        using namespace ::testing;
        ON_CALL(*this, createInstance(_, _))
                .WillByDefault(
                    Invoke(
                        this,
                        &MockDesktopFileReaderFactory::doCreateInstance));
    }

    virtual qtmir::DesktopFileReader* doCreateInstance(const QString &appId, const QFileInfo &fi)
    {
        using namespace ::testing;
        auto instance = new NiceMock<MockDesktopFileReader>(appId, fi);
        ON_CALL(*instance, loaded()).WillByDefault(Return(true));

        return instance;
    }

    MOCK_METHOD2(createInstance, qtmir::DesktopFileReader*(const QString &appId, const QFileInfo &fi));
};
}

#endif // MOCK_DESKTOP_FILE_READER_H
