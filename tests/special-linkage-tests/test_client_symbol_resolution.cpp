/*
 * Copyright © 2013 Canonical Ltd.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 3 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Authored by: Alexandros Frantzis <alexandros.frantzis@canonical.com>
 */

#include <gtest/gtest.h>
#include <dlfcn.h>

typedef void (ensure_global_symbol_resolution_func_t)();

TEST(ClientSymbolResolutionTest, can_find_client_symbols_after_ensuring_global_symbol_resolution)
{
    char const* const client_symbol = "mir_connect_sync";
    void* client_lib = dlopen(MIRCLIENT_LIBRARY_FILE, RTLD_LAZY);
    void* program = dlopen(NULL, RTLD_LAZY);

    ASSERT_TRUE(client_lib != nullptr);
    ASSERT_TRUE(program != nullptr);

    /* Client symbols aren't available yet through the program's handle */
    EXPECT_TRUE(dlsym(program, client_symbol) == nullptr);

    void* sym = dlsym(client_lib, "mir_client_ensure_global_symbol_resolution");
    /*
     * This ugly trick is needed to silence warnings about converting from void* to
     * function pointer.
     */
    auto ensure_global_symbol_resolution =
        *reinterpret_cast<ensure_global_symbol_resolution_func_t**>(&sym);
    ASSERT_TRUE(ensure_global_symbol_resolution != nullptr);

    ensure_global_symbol_resolution();
    EXPECT_TRUE(dlsym(program, client_symbol) != nullptr);

    dlclose(program);
    dlclose(client_lib);
}
