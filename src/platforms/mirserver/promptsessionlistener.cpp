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

#include "promptsessionlistener.h"
#include "logging.h"

namespace ms = mir::scene;

Q_DECLARE_METATYPE(std::shared_ptr<ms::PromptSession>)

PromptSessionListener::PromptSessionListener(QObject *parent) :
    QObject(parent)
{
    qCDebug(QTMIR_MIR_MESSAGES) << "PromptSessionListener::PromptSessionListener - this=" << this;
    qRegisterMetaType<std::shared_ptr<ms::PromptSession>>("std::shared_ptr<mir::scene::PromptSession>");
}

PromptSessionListener::~PromptSessionListener()
{
    qCDebug(QTMIR_MIR_MESSAGES) << "PromptSessionListener::~PromptSessionListener - this=" << this;
}

void PromptSessionListener::starting(std::shared_ptr<ms::PromptSession> const& prompt_session)
{
    qCDebug(QTMIR_MIR_MESSAGES) << "PromptSessionListener::starting - this=" << this << "prompt_session=" << prompt_session.get();
    Q_EMIT promptSessionStarting(prompt_session);
}

void PromptSessionListener::stopping(std::shared_ptr<ms::PromptSession> const& prompt_session)
{
    qCDebug(QTMIR_MIR_MESSAGES) << "PromptSessionListener::stopping - this=" << this << "prompt_session=" << prompt_session.get();
    Q_EMIT promptSessionStopping(prompt_session);
}

void PromptSessionListener::suspending(std::shared_ptr<ms::PromptSession> const& prompt_session)
{
    qCDebug(QTMIR_MIR_MESSAGES) << "PromptSessionListener::suspending - this=" << this << "prompt_session=" << prompt_session.get();
    Q_EMIT promptSessionSuspending(prompt_session);
}

void PromptSessionListener::resuming(std::shared_ptr<ms::PromptSession> const& prompt_session)
{
    qCDebug(QTMIR_MIR_MESSAGES) << "PromptSessionListener::resuming - this=" << this << "prompt_session=" << prompt_session.get();
    Q_EMIT promptSessionResuming(prompt_session);
}

void PromptSessionListener::prompt_provider_added(ms::PromptSession const& prompt_session,
                                                  std::shared_ptr<ms::Session> const& prompt_provider)
{
    qCDebug(QTMIR_MIR_MESSAGES) << "PromptSessionListener::prompt_provider_added - this=" << this
                                << "prompt_session=" << &prompt_session
                                << "prompt_provider=" << prompt_provider.get();
    Q_EMIT promptProviderAdded(&prompt_session, prompt_provider);
}

void PromptSessionListener::prompt_provider_removed(ms::PromptSession const& prompt_session,
                                                    std::shared_ptr<ms::Session> const& prompt_provider)
{
    qCDebug(QTMIR_MIR_MESSAGES) << "PromptSessionListener::prompt_provider_removed - this=" << this
                                << "prompt_session=" << &prompt_session
                                << "prompt_provider=" << prompt_provider.get();
    Q_EMIT promptProviderRemoved(&prompt_session, prompt_provider);
}
