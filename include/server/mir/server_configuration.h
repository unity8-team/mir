/*
 * Copyright © 2012 Canonical Ltd.
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
 * Authored by: Alan Griffiths <alan@octopull.co.uk>
 */
#ifndef MIR_SERVER_CONFIGURATION_H_
#define MIR_SERVER_CONFIGURATION_H_

#include <memory>
#include <typeinfo>

namespace mir
{
namespace compositor
{
class Compositor;
}
namespace frontend
{
class Connector;
class Shell;
}
namespace shell
{
class SessionContainer;
}
namespace graphics
{
class Display;
class DisplayConfigurationPolicy;
class Platform;
}
namespace input
{
class InputManager;
class InputDispatcher;
class EventFilter;
class InputConfiguration;
}

class MainLoop;
class ServerStatusListener;
class DisplayChanger;
class EmergencyCleanup;

class ServerConfiguration
{
    template<typename T>
    struct identity {typedef T type; };
public:

    /*!
     * \name Interface registration and query methods
     * \{
     */
    /*!
     * \brief Query for an implementation of a mir interface.
     *
     * If a cached instance is still present it will be returned, otherwise a new one is
     * constructed. An exception is thrown if the interface is unknown.
     */
    template<typename Interface>
    inline std::shared_ptr<Interface> the()
    {
        return std::static_pointer_cast<Interface>(get(typeid(Interface)));
    }

    /*!
     * \brief Wrap an existing implementation of an interface \a WrappedInterface by constructing another.
     *
     * The function \a constructor will be used as needed to create an instance of \a WrappedType
     */
    template <typename WrappingType, typename WrappedInterface>
    inline void wrap(
        std::function<std::shared_ptr<WrappingType>(std::shared_ptr<WrappedInterface> const&)> const& constructor)
    {
        wrap_existing_interface(
            [constructor](std::shared_ptr<void> wrapped_object)
            {
                return constructor(std::static_pointer_cast<WrappedInterface>(wrapped_object));
            },
            typeid(WrappedInterface),
            typeid(WrappedInterface));
    }
    /*!
     * \brief Provide an implementation (\a ImplementationType) of an interface \a InterfaceType through a
     * construction function.
     *
     * The function \a constructor will be used as needed to create an instance of \a ImplementationType
     */
    template<typename ImplementationType, typename InterfaceType>
    inline void provide(std::function<std::shared_ptr<ImplementationType>()> const& constructor)
    {
        store_constructor(
            [constructor]()
            {
                return std::static_pointer_cast<InterfaceType>(constructor());
            },
            typeid(InterfaceType));
    }

    /*!
     * \brief Register a construction function for a type that implements several mir interfaces at once.
     *
     * The first template parameter \a ImplementationType is the type that implements the interfaces
     * listed in all further parameters.
     */
    template<typename ImplementationType, typename FirstInterface, typename... InterfaceTs>
    inline void provide_multiple(std::function<std::shared_ptr<ImplementationType>()> const& constructor)
    {
        store_constructor(
            [constructor]()
            {
               return std::static_pointer_cast<FirstInterface>(constructor());
            },
            typeid(FirstInterface)
            );
        unroll_wrapping_constructors<ImplementationType,FirstInterface>(identity<InterfaceTs>()...);
    }
    /*!
     * \}
     */
    virtual std::shared_ptr<frontend::Connector> the_connector() = 0;
    virtual std::shared_ptr<frontend::Connector> the_prompt_connector() = 0;
    virtual std::shared_ptr<graphics::Display> the_display() = 0;
    virtual std::shared_ptr<compositor::Compositor> the_compositor() = 0;
    virtual std::shared_ptr<input::InputManager> the_input_manager() = 0;
    virtual std::shared_ptr<input::InputDispatcher> the_input_dispatcher() = 0;
    virtual std::shared_ptr<MainLoop> the_main_loop() = 0;
    virtual std::shared_ptr<ServerStatusListener> the_server_status_listener() = 0;
    virtual std::shared_ptr<DisplayChanger> the_display_changer() = 0;
    virtual std::shared_ptr<graphics::Platform>  the_graphics_platform() = 0;
    virtual std::shared_ptr<EmergencyCleanup> the_emergency_cleanup() = 0;
    virtual auto the_fatal_error_strategy() -> void (*)(char const* reason, ...) = 0;

private:
    /*!
     * \brief Registers a constructor function that provides an implementation
     * of the interface specified by \a interface_name.
     */
    virtual void store_constructor(std::function<std::shared_ptr<void>()> const&& constructor, std::type_info const& interface) = 0;

    /*!
     * \brief Registers a wrapping constructor to provide an implementation of \a interface_name.
     * It receives the base interface \a base_interface as a parameter.
     *
     * Note: \a base_interface may also be identical to \a interface_name
     */
    virtual void wrap_existing_interface(std::function<std::shared_ptr<void>(std::shared_ptr<void>)> const&& constructor, std::type_info const& base_interface, std::type_info const& interface) = 0;

    /*!
     * \brief Query the ServerConfiguration for a specified interface.
     */
    virtual std::shared_ptr<void> get(std::type_info const& interface) = 0;

    template<typename Implementation, typename StoredBaseInterface, typename NextInterface>
    void unroll_wrapping_constructors(identity<NextInterface>)
    {
        wrap_existing_interface(
            [](std::shared_ptr<void> instance_of_base_interface)
            {
                // The system either stores type erased weak references to the destination interfaces
                // or creates them but immediately casts away the Implementation type
                // hence we need to cast back to the Implementation
                // to know the right offset of NextInterface
                return std::static_pointer_cast<NextInterface>(
                    std::static_pointer_cast<Implementation>(
                        std::static_pointer_cast<StoredBaseInterface>(
                            instance_of_base_interface
                            )
                    )
                );
            },
            typeid(StoredBaseInterface),
            typeid(NextInterface)
            );
    }

    template<typename Implementation, typename StoredBaseInterface, typename NextInterface, typename... RemainingInterfaceTs>
    void unroll_wrapping_constructors(identity<NextInterface>, identity<RemainingInterfaceTs>...)
    {
        unroll_wrapping_constructors<Implementation, StoredBaseInterface>(identity<NextInterface>());
        unroll_wrapping_constructors<Implementation, StoredBaseInterface>(identity<RemainingInterfaceTs>()...);
    }
protected:
    ServerConfiguration() = default;
    virtual ~ServerConfiguration() = default;

    ServerConfiguration(ServerConfiguration const&) = delete;
    ServerConfiguration& operator=(ServerConfiguration const&) = delete;
};
}


#endif /* MIR_SERVER_CONFIGURATION_H_ */
