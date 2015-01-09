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
 */

#ifndef PROMPTSESSIONLISTENER_H
#define PROMPTSESSIONLISTENER_H

#include <QObject>

#include "mir/scene/prompt_session_listener.h"

class PromptSessionListener : public QObject, public mir::scene::PromptSessionListener
{
    Q_OBJECT
public:
    explicit PromptSessionListener(QObject *parent = 0);
    ~PromptSessionListener();

    void starting(std::shared_ptr<mir::scene::PromptSession> const& prompt_session) override;
    void stopping(std::shared_ptr<mir::scene::PromptSession> const& prompt_session) override;
    void suspending(std::shared_ptr<mir::scene::PromptSession> const& prompt_session) override;
    void resuming(std::shared_ptr<mir::scene::PromptSession> const& prompt_session) override;

    void prompt_provider_added(mir::scene::PromptSession const& prompt_session,
        std::shared_ptr<mir::scene::Session> const& prompt_provider) override;
    void prompt_provider_removed(mir::scene::PromptSession const& prompt_session,
        std::shared_ptr<mir::scene::Session> const& prompt_provider) override;

Q_SIGNALS:
    void promptSessionStarting(std::shared_ptr<mir::scene::PromptSession> const& session);
    void promptSessionStopping(std::shared_ptr<mir::scene::PromptSession> const& session);
    void promptSessionSuspending(std::shared_ptr<mir::scene::PromptSession> const& session);
    void promptSessionResuming(std::shared_ptr<mir::scene::PromptSession> const& session);

    void promptProviderAdded(mir::scene::PromptSession const*, std::shared_ptr<mir::scene::Session> const&);
    void promptProviderRemoved(mir::scene::PromptSession const*, std::shared_ptr<mir::scene::Session> const&);
};

#endif // SESSIONLISTENER_H
