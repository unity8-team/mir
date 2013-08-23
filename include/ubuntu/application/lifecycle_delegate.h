/*
 * Copyright © 2013 Canonical Ltd.
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
 * Authored by: Ricardo Mendoza <ricardo.mendoza@canonical.com>
 *              Thomas Voß <thomas.voss@canonical.com>
 */

#ifndef UBUNTU_APPLICATION_LIFECYCLE_DELEGATE_H_
#define UBUNTU_APPLICATION_LIFECYCLE_DELEGATE_H_

#include <ubuntu/visibility.h>

#include <ubuntu/application/archive.h>
#include <ubuntu/application/options.h>

#ifdef __cplusplus
extern "C" {
#endif

    /**
     * \brief Prototype for the callback that is invoked whenever the app has been resumed.
     * \ingroup application_support
     * \param[in] options Application instance options
     * \param[in] context The callback context as specified by the application
     */
    typedef void (*u_on_application_resumed)(const UApplicationOptions *options, void *context);
    /**
     * \brief Prototype for the callback that is invoked whenever the app is about to be stopped.
     * Applications can serialize their state to the supplied archive.
     * \ingroup application_support
     */
    typedef void (*u_on_application_about_to_stop)(UApplicationArchive *archive, void *context);

    /**
     * \brief Opaque type encapsulating all app-specific callback functions.
     * \ingroup application_support
     */
    typedef void UApplicationLifecycleDelegate;

    /**
     * \brief Creates a new instance of the lifecycle delegate with an initial refernce count of 1.
     * \ingroup application_support
     * \returns A new instance of the lifecycle delegate or NULL if no memory is available.
     */
    UBUNTU_DLL_PUBLIC UApplicationLifecycleDelegate*
    u_application_lifecycle_delegate_new();

    /**
     * \brief Destroys an instance of the lifecycle delegate and releases all of its resources.
     * \ingroup application_support
     * \param[in] delegate The instance to be destroyed.
     */
    UBUNTU_DLL_PUBLIC void
    u_application_lifecycle_delegate_destroy(
        UApplicationLifecycleDelegate *delegate);

    /**
     * \brief Increments the reference count of the supplied lifecycle delegate.
     * \ingroup application_support
     * \param[in] delegate The lifecycle delegate to increment the reference count for.
     */
    UBUNTU_DLL_PUBLIC void
    u_application_lifecycle_delegate_ref(
        UApplicationLifecycleDelegate *delegate);

    /**
     * \brief Decrements the reference count of the supplied lifecycle delegate and destroys it if the count reaches 0.
     * \ingroup application_support
     * \param[in] delegate The lifecycle delegate to decrement the reference count for.
     */
    UBUNTU_DLL_PUBLIC void
    u_application_lifecycle_delegate_unref(
        UApplicationLifecycleDelegate *delegate);

    /**
     * \brief Sets the resumed cb for the supplied delegate.
     * \ingroup application_support
     * \param[in] delegate The lifecycle delegate to adjust the cb for.
     * \param[in] cb The new callback to be invoked whenever the app resumes.
     */
    UBUNTU_DLL_PUBLIC void
    u_application_lifecycle_delegate_set_application_resumed_cb(
        UApplicationLifecycleDelegate *delegate,
        u_on_application_resumed cb);

    /**
     * \brief Queries the resumed cb from the supplied delegate.
     * \ingroup application_support
     * \returns The callback to be invoked whenever the app resumes.
     * \param[in] delegate The lifecycle delegate to query the callback from.
     */
    UBUNTU_DLL_PUBLIC u_on_application_resumed
    u_application_lifecycle_delegate_get_application_resumed_cb(
        UApplicationLifecycleDelegate *delegate);

    /**
     * \brief Sets the about-to-stop cb for the supplied delegate.
     * \ingroup application_support
     * \param[in] delegate The lifecycle delegate to adjust the cb for.
     * \param[in] cb The new callback to be invoked whenever the app is about to be stopped..
     */
    UBUNTU_DLL_PUBLIC void
    u_application_lifecycle_delegate_set_application_about_to_stop_cb(
        UApplicationLifecycleDelegate *delegate,
        u_on_application_about_to_stop cb);

    /**
     * \brief Queries the about-to-be-stopped cb from the supplied delegate.
     * \ingroup application_support
     * \returns The callback to be invoked whenever the app is about to be stopped.
     * \param[in] delegate The lifecycle delegate to query the callback from.
     */
    UBUNTU_DLL_PUBLIC u_on_application_about_to_stop
    u_application_lifecycle_delegate_get_application_about_to_stop_cb(
        UApplicationLifecycleDelegate *delegate);

    /**
     * \brief Sets the cb context for the supplied delegate.
     * \ingroup application_support
     * \param[in] delegate The lifecycle delegate to adjust the context for.
     * \param[in] context The new cb context.
     */
    UBUNTU_DLL_PUBLIC void
    u_application_lifecycle_delegate_set_context(
        UApplicationLifecycleDelegate *delegate,
        void *context);

    /**
     * \brief Queries the cb context from the supplied delegate.
     * \ingroup application_support
     * \returns The context that is passed to callbacks of this delegate.
     * \param[in] delegate The lifecycle delegate to query the context from.
     * \param[in] context Unused.
     */
    UBUNTU_DLL_PUBLIC void*
    u_application_lifecycle_delegate_get_context(
        UApplicationLifecycleDelegate *delegate,
        void *context);

#ifdef __cplusplus
}
#endif

#endif  /* UBUNTU_APPLICATION_LIFECYCLE_DELEGATE_H_ */
