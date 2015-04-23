/*
 * Copyright © 2014-2015 Canonical Ltd.
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 3,
 * as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 *
 * Authored by: Christopher James Halse Rogers <christopher.halse.rogers@canonical.com>
 *              Alberto Aguirre <alberto.aguirre@canonical.com>
 */

#include "mir_test_doubles/mock_frame_dropping_policy_factory.h"

namespace mc = mir::compositor;
namespace mtd = mir::test::doubles;

mtd::MockFrameDroppingPolicy::MockFrameDroppingPolicy(std::function<void()> const& callback,
                                                      std::function<void()> const& lock,
                                                      std::function<void()> const& unlock,
                                                      MockFrameDroppingPolicyFactory const* parent)
        : callback{callback},
          lock{lock},
          unlock{unlock},
          parent{parent}
{
}

mtd::MockFrameDroppingPolicy::~MockFrameDroppingPolicy()
{
    if (parent)
        parent->policy_destroyed(this);
}

void mtd::MockFrameDroppingPolicy::trigger()
{
    lock();
    callback();
    unlock();
}

void mtd::MockFrameDroppingPolicy::parent_destroyed()
{
    parent = nullptr;
}

std::unique_ptr<mc::FrameDroppingPolicy>
mtd::MockFrameDroppingPolicyFactory::create_policy(std::function<void()> const& drop_frame,
    std::function<void()> const& lock, std::function<void()> const& unlock) const
{
    auto policy = new ::testing::NiceMock<MockFrameDroppingPolicy>{drop_frame, lock, unlock, this};
    policies.insert(policy);
    return std::unique_ptr<mc::FrameDroppingPolicy>{policy};
}

mtd::MockFrameDroppingPolicyFactory::~MockFrameDroppingPolicyFactory()
{
    for (auto policy : policies)
        policy->parent_destroyed();
}

void mtd::MockFrameDroppingPolicyFactory::trigger_policies() const
{
    for (auto policy : policies)
        policy->trigger();
}

void mtd::MockFrameDroppingPolicyFactory::policy_destroyed(MockFrameDroppingPolicy* policy) const
{
    policies.erase(policy);
}
