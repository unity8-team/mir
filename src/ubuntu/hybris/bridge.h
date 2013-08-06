/*
 * Copyright (C) 2012 Canonical Ltd
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
#ifndef BRIDGE_H_
#define BRIDGE_H_

#include <assert.h>
#include <dlfcn.h>
#include <stddef.h>

#define HIDDEN_SYMBOL __attribute__ ((visibility ("hidden")))

#ifdef __cplusplus
extern "C" {
#endif

extern void *android_dlopen(const char *filename, int flag);
extern void *android_dlsym(void *handle, const char *symbol);

#ifdef __cplusplus
}
#endif

namespace internal
{

struct HIDDEN_SYMBOL ToApplication
{
    static const char* path()
    {
        return "/system/lib/libubuntu_application_api.so";
    }
};

struct HIDDEN_SYMBOL ToHardware
{
    static const char* path()
    {
        return "/system/lib/libubuntu_platform_hardware_api.so";
    }
};

template<typename Scope = ToApplication>
class HIDDEN_SYMBOL Bridge
{
  public:
    static Bridge<Scope>& instance()
    { 
        static Bridge<Scope> bridge; 
        return bridge; 
    }

    void* resolve_symbol(const char* symbol) const
    {
        return android_dlsym(lib_handle, symbol);
    }

  protected:
    Bridge() : lib_handle(android_dlopen(Scope::path(), RTLD_LAZY))
    {
        assert(lib_handle && "Error loading ubuntu_application_api");
    }

    ~Bridge()
    {
        // TODO android_dlclose(libcamera_handle);
    }

    void* lib_handle;
};

}

#ifdef __cplusplus
extern "C" {
#endif

/**********************************************************/
/*********** Implementation starts here *******************/
/**********************************************************/

#define DLSYM(fptr, sym) if (*(fptr) == NULL) { *((void**)fptr) = (void *) internal::Bridge<>::instance().resolve_symbol(sym); }
    
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

#ifdef __cplusplus
}
#endif

#endif // BRIDGE_H_
