#include "mir/glib_main_loop.h"
#include "mir/fd.h"

#include <sys/signalfd.h>
#include <unistd.h>
#include <boost/throw_exception.hpp>
#include <iostream>
#include <algorithm>
#include <atomic>
#include <future>
#include "mir/thread_safe_list.h"
#include <glib-unix.h>

#define ALFDEBUG if (false)

namespace
{

template<typename T> using Handlers = mir::ThreadSafeList<T>;

class MainLoopSource
{
public:
    MainLoopSource(GSource* gsource) : gsource{gsource} {}

    virtual ~MainLoopSource() = default;

    virtual gboolean prepare(gint* timeout) = 0;
    virtual gboolean check() = 0;
    virtual gboolean dispatch() = 0;

protected:
    GSource* gsource;
};

class FdMainLoopSource : public MainLoopSource
{
public:
    FdMainLoopSource(GSource* source, int fd)
        : MainLoopSource{source}, fd_{fd},
          fd_tag{g_source_add_unix_fd(source, fd, G_IO_IN)}
    {
        if (fd_tag == nullptr)
            BOOST_THROW_EXCEPTION(std::runtime_error("g source fd failed"));
    }

    void add_handler(std::function<void(int)> const& handler, void const* owner)
    {
        handlers.add({handler, owner});
        ++num_handlers;
    }

    void remove_handlers_of(void const* owner)
    {
        num_handlers -= handlers.remove_all({{}, owner});
    }

    bool has_handlers() const
    {
        return num_handlers > 0;
    }

    int fd() const
    {
        return fd_;
    }

    gboolean prepare(gint* timeout) override
    {
        *timeout = -1;
        return (g_source_query_unix_fd(gsource, fd_tag) == G_IO_IN);
    }

    gboolean check() override
    {
        return (g_source_query_unix_fd(gsource, fd_tag) == G_IO_IN);
    }

    gboolean dispatch() override
    {
        if (g_source_query_unix_fd(gsource, fd_tag) == G_IO_IN)
        {
            handlers.for_each(
                [&] (HandlerElement const& element)
                {
                    element.handler(fd_);
                });
        }

        return TRUE;
    }

private:
    struct HandlerElement
    {
        HandlerElement() : owner{nullptr} {}
        HandlerElement(std::function<void(int)> const& handler,
                       void const* owner) : handler{handler}, owner{owner} {}

        operator bool() const { return owner != nullptr; }
        bool operator==(HandlerElement const& other) const { return other.owner == owner; }
        bool operator!=(HandlerElement const& other) const { return other.owner != owner; }

        std::function<void(int)> handler;
        void const* owner;
    };

    Handlers<HandlerElement> handlers;
    std::atomic<int> num_handlers{0};
    int const fd_;
    gpointer fd_tag;
};

class TimerMainLoopSource : public MainLoopSource
{
public:
    TimerMainLoopSource(GSource* source,
                        std::shared_ptr<mir::time::Clock> const& clock,
                        mir::time::Timestamp target)
        : MainLoopSource{source}, clock{clock},
          target{target}
    {
        ALFDEBUG std::cerr << this << " TimeSource create " << std::endl;
    }

    void add_handler(std::function<void()> const& handler)
    {
        handlers.add({handler});
    }

    void clear_handlers()
    {
        handlers.clear();
    }

    gboolean prepare(gint* timeout) override
    {
        auto const now = clock->sample();
        bool const ready = (now >= target);
        if (ready)
            *timeout = -1;
        else
            *timeout = std::chrono::duration_cast<std::chrono::milliseconds>(
                clock->timeout_until(target)).count();

        ALFDEBUG std::cerr << this << " TimeSource prepare: " << ready << " " << *timeout << std::endl;
        return ready;
    }

    gboolean check() override
    {
        auto const now = clock->sample();
        bool const ready = (now >= target);
        ALFDEBUG std::cerr << this << " TimeSource check: " << ready << std::endl;
        return ready;
    }

    gboolean dispatch() override
    {
        ALFDEBUG std::cerr << this << " TimeSource dispatch: " << std::endl;
        handlers.for_each(
            [&] (HandlerElement const& element)
            {
                element.handler();
            });

        return FALSE;
    }

private:
    struct HandlerElement
    {
        HandlerElement() = default;
        HandlerElement(std::function<void(void)> const& handler) : handler{handler} {}
        operator bool() const { return !!handler; }
        std::function<void(void)> handler;
    };

    Handlers<HandlerElement> handlers;
    std::shared_ptr<mir::time::Clock> const clock;
    mir::time::Timestamp const target;
};

class ServerActionMainLoopSource : public MainLoopSource
{
public:
    ServerActionMainLoopSource(
        GSource* gsource, void const* owner, mir::ServerAction const& action,
        std::function<bool(void const*)> const& should_dispatch)
        : MainLoopSource{gsource}, owner{owner}, action{action},
          should_dispatch{should_dispatch}
    {
        ALFDEBUG std::cerr << this << " TimeSource create " << std::endl;
    }

    gboolean prepare(gint* timeout) override
    {
        ALFDEBUG std::cerr << this << " ServerAction prepare: " << std::endl;
        *timeout = -1;
        return should_dispatch(owner);
    }

    gboolean check() override
    {
        ALFDEBUG std::cerr << this << " ServerAction check: " << std::endl;
        return should_dispatch(owner);
    }

    gboolean dispatch() override
    {
        ALFDEBUG std::cerr << this << " ServerAction dispatch: " << std::endl;
        action();
        return FALSE;
    }

private:
    void const* const owner;
    mir::ServerAction const action;
    std::function<bool(void const*)> const should_dispatch;
};



struct MainLoopGSource
{
    GSource gsource;
    MainLoopSource* source;

    void attach(GMainContext* main_context)
    {
        g_source_attach(&gsource, main_context);
    }
};

gboolean ml_prepare(GSource* source, gint *timeout)
{
    ALFDEBUG std::cerr << "ml_prepare" << std::endl;
    auto ml_source = reinterpret_cast<MainLoopGSource*>(source);
    if (!g_source_is_destroyed(source))
        return ml_source->source->prepare(timeout);
    return FALSE;
}

gboolean ml_check(GSource* source)
{
    ALFDEBUG std::cerr << "ml_check" << std::endl;
    auto ml_source = reinterpret_cast<MainLoopGSource*>(source);
    if (!g_source_is_destroyed(source))
    return ml_source->source->check();
    return FALSE;
}

gboolean ml_dispatch(GSource* source, GSourceFunc, gpointer)
{
    ALFDEBUG std::cerr << "ml_dispatch" << std::endl;
    auto ml_source = reinterpret_cast<MainLoopGSource*>(source);
    if (!g_source_is_destroyed(source))
        return ml_source->source->dispatch();
    return FALSE;
}

void ml_finalize(GSource* source)
{
    ALFDEBUG std::cerr << "ml_finalize" << std::endl;
    auto ml_source = reinterpret_cast<MainLoopGSource*>(source);
    delete ml_source->source;
}

GSourceFuncs gsource_funcs {
    ml_prepare,
    ml_check,
    ml_dispatch,
    ml_finalize,
    nullptr,
    nullptr
};

}

class mir::detail::SignalDispatch
{
public:
    SignalDispatch(GMainContext* main_context)
        : main_context{main_context}
    {
    }

    void add_handler(std::vector<int> const& sigs, std::function<void(int)> const& handler)
    {
        for (auto sig : sigs)
            ensure_handle_signal(sig);
        handlers.add({sigs, handler});
    }

    void ensure_handle_signal(int sig)
    {
        std::lock_guard<std::mutex> lock{handled_signals_mutex};

        if (handled_signals.find(sig) != handled_signals.end())
            return;

        auto const gsource = g_unix_signal_source_new(sig);
        auto sc = new SignalContext{this, sig};
        g_source_set_callback(
            gsource,
            reinterpret_cast<GSourceFunc>(static_dispatch), sc,
            reinterpret_cast<GDestroyNotify>(destroy_sc));
        g_source_attach(gsource, main_context);
        g_source_unref(gsource);
    }

    void dispatch(int sig)
    {
        handlers.for_each(
            [&] (HandlerElement const& element)
            {
                ALFDEBUG std::cerr << this << " SignalDispatch calling handler sig " << sig << std::endl;
                if (std::find(element.sigs.begin(), element.sigs.end(), sig) != element.sigs.end())
                    element.handler(sig);
            });
    }

private:
    struct SignalContext
    {
        mir::detail::SignalDispatch* sd;
        int sig;
    };

    static void destroy_sc(SignalContext* d) { delete d; }

    struct HandlerElement
    {
        operator bool() const { return !!handler; }
        std::vector<int> sigs;
        std::function<void(int)> handler;
    };

    static gboolean static_dispatch(SignalContext* sc)
    {
        sc->sd->dispatch(sc->sig);
        return TRUE;
    }


    GMainContext* const main_context;
    Handlers<HandlerElement> handlers;
    std::mutex handled_signals_mutex;
    std::unordered_set<int> handled_signals;
};

struct mir::detail::FdGSource : MainLoopGSource
{
    FdMainLoopSource* fd_source()
    {
        return static_cast<FdMainLoopSource*>(source);
    }
};

std::shared_ptr<mir::detail::FdGSource> make_fd_gsource(int fd)
{
    auto const gsource = g_source_new(&gsource_funcs, sizeof(mir::detail::FdGSource));
    auto const fd_gsource = reinterpret_cast<mir::detail::FdGSource*>(gsource);

    fd_gsource->source = new FdMainLoopSource{gsource, fd};

    return {
        fd_gsource,
        [] (mir::detail::FdGSource* s)
        { 
            auto const gsource = reinterpret_cast<GSource*>(s);
            g_source_destroy(gsource);
            g_source_unref(gsource);
        }};
}

struct mir::detail::TimerGSource : MainLoopGSource
{
    TimerMainLoopSource* timer_source()
    {
        return static_cast<TimerMainLoopSource*>(source);
    }
};

std::shared_ptr<mir::detail::TimerGSource> make_timer_gsource(
    std::shared_ptr<mir::time::Clock> const& clock,
    mir::time::Timestamp target)
{
    auto const gsource = g_source_new(&gsource_funcs, sizeof(mir::detail::TimerGSource));
    auto const timer_gsource = reinterpret_cast<mir::detail::TimerGSource*>(gsource);

    timer_gsource->source = new TimerMainLoopSource{gsource, clock, target};

    return {
        timer_gsource,
        [] (mir::detail::TimerGSource* s)
        { 
            // Clear handlers to ensure that no handlers will be called after this object
            // has been destroyed (in case we are destroying while the source is being 
            // dispatched).
            s->timer_source()->clear_handlers();
            auto const gsource = reinterpret_cast<GSource*>(s);
            g_source_destroy(gsource);
            g_source_unref(gsource);
        }};
}

struct mir::detail::ServerActionGSource : MainLoopGSource
{
};

std::shared_ptr<mir::detail::ServerActionGSource> make_server_action_gsource(
    void const* owner, mir::ServerAction const& action,
    std::function<bool(void const*)> const& should_dispatch)
{
    auto const gsource = g_source_new(&gsource_funcs, sizeof(mir::detail::ServerActionGSource));
    auto const server_action_gsource = reinterpret_cast<mir::detail::ServerActionGSource*>(gsource);

    server_action_gsource->source =
        new ServerActionMainLoopSource{gsource, owner, action, should_dispatch};

    return {
        server_action_gsource,
        [] (mir::detail::ServerActionGSource* s)
        { 
            auto const gsource = reinterpret_cast<GSource*>(s);
            // NB: we shouldn't destroy the gsource
            //g_source_destroy(gsource);
            g_source_unref(gsource);
        }};
}

class AlarmImpl : public mir::time::Alarm
{
public:
    AlarmImpl(
        GMainContext* main_context,
        std::shared_ptr<mir::time::Clock> const& clock,
        std::function<void()> const& callback)
        : clock{clock},
          callback{callback},
          state_{State::cancelled},
          main_context{main_context}
    {
    }

    bool cancel() override
    {
        std::lock_guard<std::mutex> lock{alarm_mutex};

        source.reset();
        state_ = State::cancelled;
        return true;
    }

    State state() const override
    {
        std::lock_guard<std::mutex> lock{alarm_mutex};

        return state_;
    }

    bool reschedule_in(std::chrono::milliseconds delay) override
    {
        return reschedule_for(clock->sample() + delay);
    }

    bool reschedule_for(mir::time::Timestamp time_point) override
    {
        std::lock_guard<std::mutex> lock{alarm_mutex};

        state_ = State::pending;
        source = make_timer_gsource(clock, time_point);
        source->timer_source()->add_handler(
            [&] { state_ = State::triggered; callback(); } );
        source->attach(main_context);
        return true;
    }

private:
    mutable std::mutex alarm_mutex;
    std::shared_ptr<mir::time::Clock> const clock;
    std::function<void()> const callback;
    State state_;
    std::shared_ptr<mir::detail::TimerGSource> source;
    GMainContext* main_context;
};



mir::GLibMainLoop::GLibMainLoop(
    std::shared_ptr<time::Clock> const& clock)
    : clock{clock},
      main_context{g_main_context_new(),
            [] (GMainContext* ctx)
            { 
                if (ctx) g_main_context_unref(ctx);
            }},
      signal_dispatch{new detail::SignalDispatch{main_context.get()}},
      before_next_iteration{[]{}}
{
}

mir::GLibMainLoop::~GLibMainLoop()
{
    stop();
}

void mir::GLibMainLoop::run()
{
    running = true;
    ALFDEBUG std::cerr << "run start" << std::endl;
    while (running)
    {
        ALFDEBUG std::cerr << "run iter start " << running << std::endl;
        {
        std::lock_guard<std::mutex> lock{before_next_iteration_mutex};
        before_next_iteration();
        }
        ALFDEBUG std::cerr << "run iter 2 " << running << std::endl;
        g_main_context_iteration(main_context.get(), TRUE);
        ALFDEBUG std::cerr << "run iter end " << running << std::endl;
    }
    ALFDEBUG std::cerr << "run end" << std::endl;
}

struct StopContext
{
    std::atomic<bool>* running;
    GMainContext* main_context;
};

gboolean stop_func(gpointer data)
{
    auto stop_context = static_cast<StopContext*>(data);

    *stop_context->running = false;
    g_main_context_wakeup(stop_context->main_context);
    return FALSE;
}


void delete_stop_context(StopContext* ctx)
{
    delete ctx;
}

void mir::GLibMainLoop::stop()
{
    ALFDEBUG std::cerr << "stop() " << running << std::endl;
    int const very_high_priority = -1000;
    auto source = g_idle_source_new();

    g_source_set_priority(source, very_high_priority);
    g_source_set_callback(
        source, stop_func,
        new StopContext{&running, main_context.get()},
        reinterpret_cast<GDestroyNotify>(delete_stop_context));
    g_source_attach(source, main_context.get());
    g_source_unref(source);
}

void mir::GLibMainLoop::register_signal_handler(
    std::initializer_list<int> signals,
    std::function<void(int)> const& handler)
{
    signal_dispatch->add_handler(signals, handler);
}

void mir::GLibMainLoop::register_fd_handler(
    std::initializer_list<int> fds,
    void const* owner,
    std::function<void(int)> const& handler)
{
    std::lock_guard<std::mutex> lock{fd_gsources_mutex};

    for (auto fd : fds)
    {
        std::shared_ptr<detail::FdGSource> gsource;
        bool new_gsource{false};

        auto iter = std::find_if(
            fd_gsources.begin(),
            fd_gsources.end(),
            [&] (std::shared_ptr<detail::FdGSource> const& f)
            { 
                return f->fd_source()->fd() == fd;
            });

        if (iter == fd_gsources.end())
        {
            gsource = make_fd_gsource(fd);
            new_gsource = true;
            fd_gsources.push_back(gsource);
        }
        else
        {
            gsource = *iter;
        }

        gsource->fd_source()->add_handler(handler, owner);
        if (new_gsource)
            gsource->attach(main_context.get());
    }
}

void mir::GLibMainLoop::unregister_fd_handler(
    void const* owner)
{
    std::lock_guard<std::mutex> lock{fd_gsources_mutex};

    ALFDEBUG std::cerr << "unregister_fd_handler " << owner << std::endl;

    auto it = fd_gsources.rbegin();

    while (it != fd_gsources.rend())
    {
        auto const gsource = *it;
        gsource->fd_source()->remove_handlers_of(owner);

        ALFDEBUG std::cerr << "removed handler" << std::endl;
        if (!gsource->fd_source()->has_handlers())
        {
            ALFDEBUG std::cerr << "no handler left " << std::endl;
            it = decltype(fd_gsources)::reverse_iterator(
                    fd_gsources.erase(std::prev(it.base())));
        }
        else
        {
            ++it;
        }
    }
}

std::unique_ptr<mir::time::Alarm> mir::GLibMainLoop::notify_in(
    std::chrono::milliseconds delay,
    std::function<void()> callback)
{
    auto alarm = std::unique_ptr<mir::time::Alarm>{
        new AlarmImpl{main_context.get(), clock, callback}};
    
    alarm->reschedule_in(delay);

    return alarm;
}

std::unique_ptr<mir::time::Alarm> mir::GLibMainLoop::notify_at(
    mir::time::Timestamp t,
    std::function<void()> callback)
{
    auto alarm = std::unique_ptr<mir::time::Alarm>{
        new AlarmImpl{main_context.get(), clock, callback}};
    
    alarm->reschedule_for(t);

    return alarm;
}

std::unique_ptr<mir::time::Alarm> mir::GLibMainLoop::create_alarm(
    std::function<void()> callback)
{
    return std::unique_ptr<mir::time::Alarm>{
        new AlarmImpl{main_context.get(), clock, callback}};
}

void mir::GLibMainLoop::enqueue(void const* owner, ServerAction const& action)
{
    auto gsource = make_server_action_gsource(owner, action,
        [&] (void const* owner)
        {
            std::lock_guard<std::mutex> lock{server_actions_mutex};
            return (do_not_process.find(owner) == do_not_process.end());
        });
    gsource->attach(main_context.get());
}

void mir::GLibMainLoop::pause_processing_for(void const* owner)
{
    std::lock_guard<std::mutex> lock{server_actions_mutex};
    do_not_process.insert(owner);
}

void mir::GLibMainLoop::resume_processing_for(void const* owner)
{
    std::lock_guard<std::mutex> lock{server_actions_mutex};
    do_not_process.erase(owner);

    g_main_context_wakeup(main_context.get());
}

gboolean flush_func(gpointer data)
{
    ALFDEBUG std::cerr << "Flush!" << std::endl;
    auto to_flush = static_cast<std::promise<void>*>(data);
    to_flush->set_value();

    return FALSE;
}

void mir::GLibMainLoop::flush()
{
    std::promise<void> to_flush;
    auto flushed = to_flush.get_future();

    {
        std::lock_guard<std::mutex> lock{before_next_iteration_mutex};
        before_next_iteration = [&] {
            ALFDEBUG std::cerr << "g_idle_add_full" << std::endl;

            auto source = g_idle_source_new();
            g_source_set_priority(source, G_PRIORITY_LOW);
            g_source_set_callback(source, flush_func, &to_flush, nullptr);
            g_source_attach(source, main_context.get());
            g_source_unref(source);

            before_next_iteration = []{};
        };
    }

    g_main_context_wakeup(main_context.get());

    ALFDEBUG std::cerr << "Waiting for before_next_iteration done" << std::endl;

    flushed.get();
    ALFDEBUG std::cerr << "flush finished" << std::endl;
}
