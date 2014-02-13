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
#include <stdlib.h>
#include <string.h>

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

/* By default we load the backend from /system/lib/libubuntu_application_api.so
 * Programs can select a different backend with $UBUNTU_PLATFORM_API_BACKEND,
 * which either needs to be a full path or just the file name (then it will be
 * looked up in the usual library search path, see dlopen(3)).
 */
struct HIDDEN_SYMBOL ToApplication
{
    static const char* path()
    {
        static const char* cache = NULL;

        if (cache == NULL) {
            cache = secure_getenv("UBUNTU_PLATFORM_API_BACKEND");
            if (cache == NULL)
                cache = "/system/lib/libubuntu_application_api.so";
        }

        return cache;
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
        return dlsym_fn(lib_handle, symbol);
    }

  protected:
    Bridge() : lib_handle(android_dlopen(Scope::path(), RTLD_LAZY))
    {
        const char* path = Scope::path();
        /* use Android dl functions for Android libs in /system/, glibc dl
         * functions for others */
        if (strncmp(path, "/system/", 8) == 0) {
            lib_handle = android_dlopen(path, RTLD_LAZY);
            dlsym_fn = android_dlsym;
        } else {
            lib_handle = dlopen(path, RTLD_LAZY);
            dlsym_fn = dlsym;
        }
    }

    ~Bridge()
    {
        // TODO android_dlclose(libcamera_handle);
    }

    void* lib_handle;
    void* (*dlsym_fn) (void*, const char*);
};

}

#ifdef __cplusplus
extern "C" {
#endif

/**********************************************************/
/*********** Implementation starts here *******************/
/**********************************************************/

#define DLSYM(fptr, sym) if (*(fptr) == NULL) { *((void**)fptr) = (void *) internal::Bridge<>::instance().resolve_symbol(sym); }

// this allows DLSYM to return NULL (happens if the backend is not available),
// and returns NULL in that case; return_type must be a pointer!
#define IMPLEMENT_CTOR0(return_type, symbol)  \
    return_type symbol()                          \
    {                                             \
        static return_type (*f)() = NULL;         \
        DLSYM(&f, #symbol);                       \
        return f ? f() : NULL;}

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
