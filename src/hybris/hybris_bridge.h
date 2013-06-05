/*
 * Copyright (C) 2013 Canonical Ltd
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License version 3 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Authored by: Thomas Voss <thomas.voss@canonical.com>
 *              Ricardo Mendoza <ricardo.mendoza@canonical.com>
 */

#ifndef UBUNTU_APPLICATION_API_HYBRIS_BRIDGE_H_
#define UBUNTU_APPLICATION_API_HYBRIS_BRIDGE_H_

namespace ubuntu
{
namespace hybris
{

struct Bridge
{
    Bridge();
    ~Bridge();

    static const char* path_to_library();
    static Bridge& instance();
    
    void* resolve_symbol(const char* symbol) const;

    void* lib_handle;
};

}
}

// Sweet and beautiful music.
#define DLSYM(fptr, sym) if (*(fptr) == NULL) { *((void**)fptr) = (void *) ubuntu::hybris::Bridge::instance().resolve_symbol(sym); }
    
#define IMPLEMENT_FUNCTION0(return_type, symbol)  \
    return_type symbol()                          \
    {                                             \
        static return_type (*f)() = NULL;         \
        DLSYM(&f, #symbol);                       \
        return f();}

#define IMPLEMENT_VOID_FUNCTION0(symbol)          \
    void symbol()                                 \
    {                                             \
        static void (*f)() = NULL;                \
        DLSYM(&f, #symbol);                       \
        f();}
    
#define IMPLEMENT_FUNCTION1(return_type, symbol, arg1) \
    return_type symbol(arg1 _1)                        \
    {                                                  \
        static return_type (*f)(arg1) = NULL;          \
        DLSYM(&f, #symbol);                     \
        return f(_1); }

#define IMPLEMENT_SF_FUNCTION1(return_type, symbol, arg1) \
    return_type symbol(arg1 _1)                        \
    {                                                  \
        static return_type (*f)(arg1) __attribute__((pcs("aapcs"))) = NULL; \
        DLSYM(&f, #symbol);                     \
        return f(_1); }


#define IMPLEMENT_VOID_FUNCTION1(symbol, arg1)               \
    void symbol(arg1 _1)                                     \
    {                                                        \
        static void (*f)(arg1) = NULL;                       \
        DLSYM(&f, #symbol);                           \
        f(_1); }

#define IMPLEMENT_FUNCTION2(return_type, symbol, arg1, arg2)    \
    return_type symbol(arg1 _1, arg2 _2)                        \
    {                                                           \
        static return_type (*f)(arg1, arg2) = NULL;             \
        DLSYM(&f, #symbol);                              \
        return f(_1, _2); }

#define IMPLEMENT_VOID_FUNCTION2(symbol, arg1, arg2)            \
    void symbol(arg1 _1, arg2 _2)                               \
    {                                                           \
        static void (*f)(arg1, arg2) = NULL;                    \
        DLSYM(&f, #symbol);                              \
        f(_1, _2); }

#define IMPLEMENT_FUNCTION3(return_type, symbol, arg1, arg2, arg3)    \
    return_type symbol(arg1 _1, arg2 _2, arg3 _3)                     \
    {                                                                 \
        static return_type (*f)(arg1, arg2, arg3) = NULL;             \
        DLSYM(&f, #symbol);                                           \
        return f(_1, _2, _3); } 

#define IMPLEMENT_VOID_FUNCTION3(symbol, arg1, arg2, arg3)      \
    void symbol(arg1 _1, arg2 _2, arg3 _3)                      \
    {                                                           \
        static void (*f)(arg1, arg2, arg3) = NULL;              \
        DLSYM(&f, #symbol);                                     \
        f(_1, _2, _3); }

#define IMPLEMENT_VOID_FUNCTION4(symbol, arg1, arg2, arg3, arg4) \
    void symbol(arg1 _1, arg2 _2, arg3 _3, arg4 _4)              \
    {                                                            \
        static void (*f)(arg1, arg2, arg3, arg4) = NULL;         \
        DLSYM(&f, #symbol);                                      \
        f(_1, _2, _3, _4); }

#define IMPLEMENT_FUNCTION4(return_type, symbol, arg1, arg2, arg3, arg4) \
    return_type symbol(arg1 _1, arg2 _2, arg3 _3, arg4 _4)               \
    {                                                                    \
        static return_type (*f)(arg1, arg2, arg3, arg4) = NULL;          \
        DLSYM(&f, #symbol);                                              \
        return f(_1, _2, _3, _4); }

#define IMPLEMENT_FUNCTION6(return_type, symbol, arg1, arg2, arg3, arg4, arg5, arg6) \
    return_type symbol(arg1 _1, arg2 _2, arg3 _3, arg4 _4, arg5 _5, arg6 _6)         \
    {                                                                                \
        static return_type (*f)(arg1, arg2, arg3, arg4, arg5, arg6) = NULL;          \
        DLSYM(&f, #symbol);                                                          \
        return f(_1, _2, _3, _4, _5, _6); }

#define IMPLEMENT_VOID_FUNCTION7(symbol, arg1, arg2, arg3, arg4, arg5, arg6, arg7) \
    void symbol(arg1 _1, arg2 _2, arg3 _3, arg4 _4, arg5 _5, arg6 _6, arg7 _7) \
    {                                                                   \
        static void (*f)(arg1, arg2, arg3, arg4, arg5, arg6, arg7) = NULL; \
        DLSYM(&f, #symbol);                                             \
        f(_1, _2, _3, _4, _5, _6, _7); }

#define IMPLEMENT_VOID_FUNCTION8(symbol, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8) \
    void symbol(arg1 _1, arg2 _2, arg3 _3, arg4 _4, arg5 _5, arg6 _6, arg7 _7, arg8 _8) \
    {                                                                   \
        static void (*f)(arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8) = NULL; \
        DLSYM(&f, #symbol);                                             \
        f(_1, _2, _3, _4, _5, _6, _7, _8); }

#endif // UBUNTU_APPLICATION_API_HYBRIS_BRIDGE_H_
