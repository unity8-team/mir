/*
 * Copyright (C) 2005 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

//
// Timer functions.
//
#include <utils/Timers.h>
#include <std/Log.h>

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/time.h>
#include <time.h>
#include <errno.h>
#include <limits.h>

#ifdef HAVE_WIN32_THREADS
#include <windows.h>
#endif

std::chrono::nanoseconds systemTime(int clock)
{
#if defined(HAVE_POSIX_CLOCKS)
    static const clockid_t clocks[] = {
            CLOCK_REALTIME,
            CLOCK_MONOTONIC,
            CLOCK_PROCESS_CPUTIME_ID,
            CLOCK_THREAD_CPUTIME_ID
    };
    struct timespec t;
    t.tv_sec = t.tv_nsec = 0;
    clock_gettime(clocks[clock], &t);
    return std::chrono::nanoseconds(t.tv_sec)*1000000000LL + t.tv_nsec;
#else
    // we don't support the clocks here.
    struct timeval t;
    t.tv_sec = t.tv_usec = 0;
    gettimeofday(&t, NULL);
    return std::chrono::nanoseconds(t.tv_sec)*1000000000LL + std::chrono::nanoseconds(t.tv_usec)*1000LL;
#endif
}

int toMillisecondTimeoutDelay(std::chrono::nanoseconds referenceTime, std::chrono::nanoseconds timeoutTime)
{
    int timeoutDelayMillis;
    if (timeoutTime > referenceTime) {
        uint64_t timeoutDelay = (timeoutTime - referenceTime).count();
        if (timeoutDelay > uint64_t((INT_MAX - 1) * 1000000LL)) {
            timeoutDelayMillis = -1;
        } else {
            timeoutDelayMillis = (timeoutDelay + 999999LL) / 1000000LL;
        }
    } else {
        timeoutDelayMillis = 0;
    }
    return timeoutDelayMillis;
}

