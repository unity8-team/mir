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
 * Author: Kevin DuBois <kevin.dubois@canonical.com>
 */

#include <sys/time.h>

static inline double time_delta(struct timeval* e, struct timeval* s)
{
    double start = ((double) s->tv_sec) + ((double) s->tv_usec / 1000000);
    double end = ((double) e->tv_sec) + ((double) e->tv_usec / 1000000);
    return end-start; 
}

