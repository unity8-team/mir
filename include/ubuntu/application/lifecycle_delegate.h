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

#include "archive.h"
#include "options.h"

#ifdef __cplusplus
extern "C" {
#endif
    
    typedef void (*u_on_application_resumed)(const UApplicationOptions *options, void *context);
    typedef void (*u_on_application_about_to_stop)(UApplicationArchive *archive, void *context);
    
    typedef void UApplicationLifecycleDelegate;
    
    UApplicationLifecycleDelegate*
    u_application_lifecycle_delegate_new();
    
    void
    u_application_lifecycle_delegate_destroy(
        UApplicationLifecycleDelegate *delegate);
    
    void
    u_application_lifecycle_delegate_ref(
        UApplicationLifecycleDelegate *delegate);
    
    void
    u_application_lifecycle_delegate_unref(
        UApplicationLifecycleDelegate *delegate);
    
    void
    u_application_lifecycle_delegate_set_application_resumed_cb(
        UApplicationLifecycleDelegate *delegate,
        u_on_application_resumed cb);
    
    u_on_application_resumed
    u_application_lifecycle_delegate_get_application_resumed_cb(
        UApplicationLifecycleDelegate *delegate);
    
    void
    u_application_lifecycle_delegate_set_application_about_to_stop_cb(
        UApplicationLifecycleDelegate *delegate,
        u_on_application_about_to_stop cb);
    
    u_on_application_about_to_stop
    u_application_lifecycle_delegate_get_application_about_to_stop_cb(
        UApplicationLifecycleDelegate *delegate);
    
    void
    u_application_lifecycle_delegate_set_context(
        UApplicationLifecycleDelegate *delegate,
        void *context);
    
    void*
    u_application_lifecycle_delegate_get_context(
        UApplicationLifecycleDelegate *delegate,
        void *context);
    
#ifdef __cplusplus
}
#endif

#endif  /* UBUNTU_APPLICATION_LIFECYCLE_DELEGATE_H_ */
