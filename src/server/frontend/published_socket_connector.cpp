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
 * Authored by: Thomas Guest <thomas.guest@canonical.com>
 */

#include "published_socket_connector.h"
#include "mir/frontend/protobuf_connection_creator.h"

#include "mir/frontend/connection_context.h"
#include "mir/frontend/connector_report.h"
#include "mir/emergency_cleanup_registry.h"
#include "mir/thread_name.h"

#include <boost/exception/errinfo_errno.hpp>
#include <boost/throw_exception.hpp>

#include <sys/socket.h>
#include <sys/stat.h>

#include <cstdio>
#include <fstream>
#include <future>

namespace mf = mir::frontend;
namespace mfd = mir::frontend::detail;
namespace ba = boost::asio;

namespace
{

bool socket_file_exists(std::string const& filename)
{
    struct stat statbuf;
    bool exists = (0 == stat(filename.c_str(), &statbuf));
    /* Avoid removing non-socket files */
    bool is_socket_type = (statbuf.st_mode & S_IFMT) == S_IFSOCK;
    return exists && is_socket_type;
}

bool socket_exists(std::string const& socket_name)
{
    try
    {
        std::string socket_path{socket_name};

        /* In case an abstract socket name exists with the same name*/
        socket_path.insert(std::begin(socket_path), ' ');

        /* If the name is contained in this table, it signifies
         * a process is truly using that socket connection
         */
        std::ifstream socket_names_file("/proc/net/unix");
        std::string line;
        while (std::getline(socket_names_file, line))
        {
           auto index = line.find(socket_path);
           /* check for complete match */
           if (index != std::string::npos &&
               (index + socket_path.length()) == line.length())
           {
               return true;
           }
        }
    }
    catch (...)
    {
        /* Assume the socket exists */
        return true;
    }
    return false;
}

std::string remove_if_stale(std::string const& socket_name)
{
    if (socket_file_exists(socket_name) && !socket_exists(socket_name))
    {
        if (std::remove(socket_name.c_str()) != 0)
        {
            BOOST_THROW_EXCEPTION(
                boost::enable_error_info(
                    std::runtime_error("Failed removing stale socket file")) << boost::errinfo_errno(errno));
        }
    }
    return socket_name;
}
}

mf::PublishedSocketConnector::PublishedSocketConnector(
    const std::string& socket_file,
    std::shared_ptr<ConnectionCreator> const& connection_creator,
    int threads,
    EmergencyCleanupRegistry& emergency_cleanup_registry,
    std::shared_ptr<ConnectorReport> const& report)
:   BasicConnector(connection_creator, threads, report),
    socket_file(remove_if_stale(socket_file)),
    acceptor(io_service, socket_file)
{
    emergency_cleanup_registry.add(
        [socket_file] { std::remove(socket_file.c_str()); });
    start_accept();
}

mf::PublishedSocketConnector::~PublishedSocketConnector() noexcept
{
    std::remove(socket_file.c_str());
}

void mf::PublishedSocketConnector::start_accept()
{
    report->listening_on(socket_file);

    auto socket = std::make_shared<boost::asio::local::stream_protocol::socket>(io_service);

    acceptor.async_accept(
        *socket,
        [this,socket](boost::system::error_code const& ec)
        {
            on_new_connection(socket, ec);
        });
}

void mf::PublishedSocketConnector::on_new_connection(
    std::shared_ptr<boost::asio::local::stream_protocol::socket> const& socket,
    boost::system::error_code const& ec)
{
    if (!ec)
    {
        create_session_for(socket, [](std::shared_ptr<mf::Session> const&) {});
    }
    start_accept();
}

mf::BasicConnector::BasicConnector(
    std::shared_ptr<ConnectionCreator> const& connection_creator,
    int threads,
    std::shared_ptr<ConnectorReport> const& report)
:   work(io_service),
    report(report),
    nthreads(threads),
    running(false),
    connection_creator{connection_creator}
{
}

void mf::BasicConnector::start()
{
    report->starting_threads(nthreads);
    running = true;
    manager = std::thread([this]()
    {
        mir::set_thread_name("Mir/IPC");
        report->thread_start();

        std::function<void(int)> worker = [this,&worker](int depth){
            if (io_service.poll_one() && depth < nthreads)
            {
                std::thread child([this,&worker,depth]{ worker(depth+1); });
                child.detach();
            }
            io_service.run_one();
        };

        while (running)
        {
            worker(1);
        }

        report->thread_end();
    });
}

void mf::BasicConnector::stop()
{
    /* Stop processing new requests */
    io_service.stop();

    report->stopping_threads(nthreads);

    running = false;
    if (manager.joinable())
        manager.join();

    /* Prepare for a potential restart */
    io_service.reset();
}

void mf::BasicConnector::create_session_for(
    std::shared_ptr<boost::asio::local::stream_protocol::socket> const& server_socket,
    std::function<void(std::shared_ptr<Session> const& session)> const& connect_handler) const
{
    report->creating_session_for(server_socket->native_handle());

    connection_creator->create_connection_for(server_socket, {connect_handler, this});
}

int mf::BasicConnector::client_socket_fd() const
{
    return client_socket_fd([](std::shared_ptr<mf::Session> const&) {});
}

int mf::BasicConnector::client_socket_fd(std::function<void(std::shared_ptr<Session> const& session)> const& connect_handler) const
{
    enum { server, client, size };
    int socket_fd[size];

    if (socketpair(AF_LOCAL, SOCK_STREAM, 0, socket_fd))
    {
        BOOST_THROW_EXCEPTION(
            boost::enable_error_info(
                std::runtime_error("Could not create socket pair")) << boost::errinfo_errno(errno));
    }

    auto const server_socket = std::make_shared<boost::asio::local::stream_protocol::socket>(
        io_service, boost::asio::local::stream_protocol(), socket_fd[server]);

    report->creating_socket_pair(socket_fd[server], socket_fd[client]);

    create_session_for(server_socket, connect_handler);

    return socket_fd[client];
}

mf::BasicConnector::~BasicConnector() noexcept
{
    stop();
}

