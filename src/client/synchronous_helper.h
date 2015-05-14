/*
 * Copyright © 2015 Canonical Ltd.
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License version 3,
 * as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Authored by: Christopher James Halse Rogers <christopher.halse.rogers@canonical.com>
 */

#ifndef MIR_CLIENT_SYNCHRONOUS_H_
#define MIR_CLIENT_SYNCHRONOUS_H_

#include "mir_connection.h"
#include "mir_wait_handle.h"

#include <functional>
#include <type_traits>

template<typename Result>
void assign_result(Result* result, void* ctx)
{
    auto assignee = reinterpret_cast<Result**>(ctx);
    if (assignee)
    {
        *assignee = result;
    }
}

template<typename Callback>
struct SynchronousContext
{
    Callback real_callback;
    bool complete;
    void* userdata;
};

template<typename Callable, class Tuple, std::size_t...I>
void apply_substituting_last_arg(
    Callable&& function,
    Tuple&& args,
    std::index_sequence<I...>,
    void* context)
{
    return std::forward<Callable>(function)(
        std::get<I>(std::forward<Tuple>(args))...,
        context);
}


template<typename... Args>
void synchronous_wrapper(Args... args)
{
    std::tuple<Args...> arguments{args...};
    constexpr std::size_t arg_count = sizeof...(Args);

    auto wrapped_context = reinterpret_cast<SynchronousContext<void(*)(Args...)>*>(std::get<arg_count - 1>(arguments));
    if (wrapped_context->real_callback)
    {
        apply_substituting_last_arg(
               wrapped_context->real_callback,
               std::forward_as_tuple(args...),
               std::make_index_sequence<arg_count - 1>(),
               wrapped_context->userdata);
    }
    wrapped_context->complete = true;
}


template<typename Callable, typename Tuple, std::size_t...I>
constexpr decltype(auto) call_impl(Callable&& function, Tuple&& args, std::index_sequence<I...>)
{
    return std::forward<Callable>(function)(std::get<I>(std::forward<Tuple>((args)))...);
}

template<typename Callable, typename Tuple>
constexpr decltype(auto) apply(Callable&& function, Tuple&& args)
{
    return call_impl(
        std::forward<Callable>(function),
        std::forward<Tuple>(args),
        std::make_index_sequence<std::tuple_size<std::decay_t<Tuple>>{}>{});
}

void dispatch_connection_until(MirConnection* connection, std::function<bool()> predicate);


/**
 * \brief Make a synchronous RPC call
 *
 * This wrapper takes care of manually dispatching the MirConnection if it is in
 * manual dispatch mode, or waiting for the operation to complete if in automatic
 * dispatch mode.
 *
 * Given a function mir_foo_do_thing(MirFoo* target, int arg, foo_callback callback, void* context)
 * the correct way to call this function is
 * make_synchronous_call(connection,
 *                       &mir_foo_do_thing,
 *                       target,
 *                       arg,
 *                       callback,
 *                       static_cast<void*>(context));
 *
 * The last two parameters must be the callback function and the void* to pass to that callback.
 * The last argument must have a pointer type; NULL or nullptr must be explicitly cast to
 * void*.
 */
template<typename Callable, typename... Args>
void make_synchronous_call(MirConnection* connection,
                           Callable&& function,
                           Args&&... args)
{
    static_assert(
        std::is_same<typename std::result_of<Callable(Args...)>::type, MirWaitHandle*>::value,
        "Second parameter must be a function that returns a MirWaitHandle*");

    if (connection->watch_fd() != mir::Fd::invalid)
    {
        std::tuple<Args...> arguments{args...};
        constexpr int arg_count = sizeof...(Args);
        auto callback = std::get<arg_count - 2>(arguments);
        auto context = std::get<arg_count - 1>(arguments);

        static_assert(
            std::is_pointer<typename std::tuple_element<arg_count - 1, std::tuple<Args...>>::type>::value,
            "The final argument must be a pointer type");
        static_assert(
            std::is_pointer<typename std::tuple_element<arg_count - 2, std::tuple<Args...>>::type>::value,
            "The second last argument must be a function pointer");

        SynchronousContext<decltype(callback)> wrapper_context {
            callback,
            false,
            context
        };

        std::get<arg_count - 2>(arguments) = &synchronous_wrapper;
        std::get<arg_count - 1>(arguments) =
            reinterpret_cast<typename std::tuple_element<arg_count - 1, std::tuple<Args...>>::type>(&wrapper_context);

        connection->process_next_request_first();
        apply(std::forward<Callable>(function), arguments);

        dispatch_connection_until(connection, [&wrapper_context](){ return wrapper_context.complete; });
    }
    else
    {
        mir_wait_for(std::forward<Callable>(function)(std::forward<Args>(args)...));
    }
}

#endif // MIR_CLIENT_SYNCHRONOUS_H_
