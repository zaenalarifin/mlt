/*
 * transition_composite.h -- compose one image over another using alpha channel
 * Copyright (C) 2003-2004 Ushodaya Enterprises Limited
 * Author: Dan Dennedy <dan@dennedy.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef _TRANSITION_COMPOSITE_H_
#define _TRANSITION_COMPOSITE_H_

#include <framework/mlt_transition.h>

extern mlt_transition transition_composite_init( mlt_profile profile, mlt_service_type type, const char *id, char *arg );

// Courtesy functionality - allows regionalised filtering
extern mlt_frame composite_copy_region( mlt_transition, mlt_frame, mlt_position );

#endif
