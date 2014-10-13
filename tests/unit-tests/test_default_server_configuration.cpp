/*
 * Copyright © 2014 Canonical Ltd.
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 3,
 * as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Authored by: Andreas Pokorny <andreas.pokorny@canonical.com>
 */

#include "mir/default_server_configuration.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace
{

class ExampleInterface
{
public:
    constexpr static char const interface_name[] = "ExampleInterface";
    virtual void do_something() = 0;
    virtual ~ExampleInterface() = default;
};

class ExampleInterfaceMock : public ::testing::NiceMock<ExampleInterface>
{
public:
    MOCK_METHOD0(Die, void());
    virtual void do_something() {}
    virtual ~ExampleInterfaceMock() { Die(); }
};

class AnotherExampleInterface
{
public:
    constexpr static char const interface_name[] = "AnotherExampleInterface";
    virtual void do_something_different() = 0;
    virtual ~AnotherExampleInterface() = default;
};

constexpr char const ExampleInterface::interface_name[];
constexpr char const AnotherExampleInterface::interface_name[];

class ExampleImplementation : public ExampleInterface
{
public:
    virtual void do_something() {}
};

class WrappedExampleImplementation: public ExampleInterface
{
public:
    std::shared_ptr<ExampleInterface> default_implementation;
    WrappedExampleImplementation(std::shared_ptr<ExampleInterface> base)
        : default_implementation(base)
    {}
    virtual void do_something() {}
};

class MultipleInheritanceCase : public ExampleInterface, public AnotherExampleInterface
{
public:
    virtual void do_something() {}
    virtual void do_something_different() {}
};

}

struct DefaultServerConfigurationTest : public ::testing::Test
{
    mir::DefaultServerConfiguration conf{0,nullptr};

    void register_example_implementation()
    {
        conf.provide<ExampleImplementation,ExampleInterface>(
            []()
            {
                return std::make_shared<ExampleImplementation>();
            });
    }

};

TEST_F(DefaultServerConfigurationTest,unkown_interface_request_throws)
{
    using namespace testing;
    mir::DefaultServerConfiguration conf(0,nullptr);
    EXPECT_THROW({
    auto var = conf.the<ExampleInterface>();
    },std::exception);
}

TEST_F(DefaultServerConfigurationTest,registered_constructor_yields_object_of_type)
{
    using namespace testing;
    register_example_implementation();
    auto var = conf.the<ExampleInterface>();

    EXPECT_THAT(std::dynamic_pointer_cast<ExampleImplementation>(var),
                Ne(std::shared_ptr<ExampleImplementation>(nullptr)));
}

TEST_F(DefaultServerConfigurationTest, requesting_an_instance_while_keeping_a_reference_yields_the_same_instance)
{
    using namespace testing;
    register_example_implementation();
    auto first_instance = conf.the<ExampleInterface>();
    auto second_instance = conf.the<ExampleInterface>();
    EXPECT_THAT(second_instance,
                Eq(first_instance));
}

TEST_F(DefaultServerConfigurationTest, stores_only_a_weak_reference)
{
    using namespace testing;
    register_example_implementation();
    std::weak_ptr<ExampleInterface> dead_instance = conf.the<ExampleInterface>();
    std::shared_ptr<ExampleInterface> gone = dead_instance.lock();
    EXPECT_THAT(gone,
                Eq(std::shared_ptr<ExampleInterface>(nullptr)));
}

TEST_F(DefaultServerConfigurationTest, existing_constructor_can_bewrapped)
{
    using namespace testing;
    register_example_implementation();

    conf.wrap<WrappedExampleImplementation,ExampleInterface>(
        [](std::shared_ptr<ExampleInterface> const& default_implementation)
        {
            return std::make_shared<WrappedExampleImplementation>(default_implementation);
        }
        );

    auto var = conf.the<ExampleInterface>();
    EXPECT_THAT(std::dynamic_pointer_cast<WrappedExampleImplementation>(var),
                Ne(std::shared_ptr<WrappedExampleImplementation>(nullptr)));
}

TEST_F(DefaultServerConfigurationTest, destructor_is_called)
{
    using namespace testing;
    conf.provide<ExampleInterfaceMock, ExampleInterface>(
        []()
        {
            return std::make_shared<ExampleInterfaceMock>();
        });

    {
        auto example_mock = std::dynamic_pointer_cast<ExampleInterfaceMock>(conf.the<ExampleInterface>());
        EXPECT_CALL(*example_mock, Die());
    }
}

TEST_F(DefaultServerConfigurationTest, one_implementation_can_provie_multiple_interfaces)
{
    using namespace testing;

    conf.provide_multiple<MultipleInheritanceCase,ExampleInterface,AnotherExampleInterface>(
        []()
        {
            return std::make_shared<MultipleInheritanceCase>();
        });

    auto var1 = conf.the<ExampleInterface>();
    auto var2 = conf.the<AnotherExampleInterface>();
    auto implv1 = std::dynamic_pointer_cast<MultipleInheritanceCase>(var1);
    auto implv2 = std::dynamic_pointer_cast<MultipleInheritanceCase>(var2);

    EXPECT_THAT(implv1,Eq(implv2));
}
