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
 * Authored by: Christopher James Halse Rogers <christopher.halse.rogers@canonical.com>
 */

#ifndef MIR_PROCESS_SPAWNER_H_
#define MIR_PROCESS_SPAWNER_H_

#include <memory>
#include <future>
#include <initializer_list>

namespace mir
{
namespace process
{

class Handle;

class Spawner
{
public:
    virtual ~Spawner() = default;
    /**
     * \brief Run a binary, respecting $PATH
     * \note All open file descriptors other than stdin, stdout, and stderr are closed before
     *       running the binary.
     * \param [in] binary_name  The name of the binary to run, such as “Xorg”
     * \return                  A future that will contain the handle to the spawned process.
     *                          The future will be populated after the process has been exec()d,
     *                          or the call to exec() has failed.
     *                          If spawning the binary fails the future will contain a
     *                          std::runtime_error.
     */
    virtual std::future<std::shared_ptr<Handle>> run_from_path(char const* binary_name) const = 0;
    /**
     * \brief Run a binary with arguments, respecting $PATH
     * \note All open file descriptors other than stdin, stdout, and stderr are closed before
     *       running the binary.
     * \param [in] binary_name  The name of the binary to run, such as “Xorg”
     * \param [in] args         The arguments to be passed to the binary. No processing is done on
     *                          these strings; each one will appear verbatim as a separate char* in
     *                          the binary's argv[].
     * \return                  A future that will contain the handle to the spawned process.
     *                          The future will be populated after the process has been exec()d,
     *                          or the call to exec() has failed.
     *                          If spawning the binary fails the future will contain a
     *                          std::runtime_error.
     */
    virtual std::future<std::shared_ptr<Handle>> run_from_path(char const* binary_name, std::initializer_list<char const*> args) const = 0;
    /**
     * \brief Run a binary, respecting $PATH
     * \note All open file descriptors other than stdin, stdout, stderr, and the fds listed in
     *       fds are closed before running the binary.
     * \param [in] binary_name  The name of the binary to run, such as “Xorg”
     * \param [in] args         The arguments to be passed to the binary. No processing is done on
     *                          these strings; each one will appear verbatim as a separate char* in
     *                          the binary's argv[].
     * \param [in] fds          The set of fds that will remain available to the child process.
     * \return                  A future that will contain the handle to the spawned process.
     *                          The future will be populated after the process has been exec()d,
     *                          or the call to exec() has failed.
     *                          If spawning the binary fails the future will contain a
     *                          std::runtime_error.
     */
    virtual std::future<std::shared_ptr<Handle>> run_from_path(char const* binary_name, std::initializer_list<char const*> args, std::initializer_list<int> fds) const = 0;
};
}
}
#endif  // MIR_PROCESS_SPAWNER_H_
