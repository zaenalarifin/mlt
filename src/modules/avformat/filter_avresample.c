/*
 * filter_avresample.c -- adjust audio sample frequency
 * Copyright (C) 2003-2004 Ushodaya Enterprises Limited
 * Author: Charles Yates <charles.yates@pandora.be>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include "filter_avresample.h"

#include <framework/mlt_frame.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// ffmpeg Header files
#include <avformat.h>

/** Get the audio.
*/

static int resample_get_audio( mlt_frame frame, int16_t **buffer, mlt_audio_format *format, int *frequency, int *channels, int *samples )
{
	// Get the properties of the frame
	mlt_properties properties = mlt_frame_properties( frame );

	// Get the filter service
	mlt_filter filter = mlt_frame_pop_audio( frame );

	// Get the filter properties
	mlt_properties filter_properties = mlt_filter_properties( filter );

	// Get the resample information
	int output_rate = mlt_properties_get_int( filter_properties, "frequency" );
	int16_t *sample_buffer = mlt_properties_get_data( filter_properties, "buffer", NULL );

	// Obtain the resample context if it exists
	ReSampleContext *resample = mlt_properties_get_data( filter_properties, "audio_resample", NULL );

	// Used to return number of channels in the source
	int channels_avail = *channels;

	// Loop variable
	int i;

	// If no resample frequency is specified, default to requested value
	if ( output_rate == 0 )
		output_rate = *frequency;

	// Restore the original get_audio
	frame->get_audio = mlt_frame_pop_audio( frame );

	// Get the producer's audio
	mlt_frame_get_audio( frame, buffer, format, frequency, &channels_avail, samples );

	// Duplicate channels as necessary
	if ( channels_avail < *channels )
	{
		int size = *channels * *samples * sizeof( int16_t );
		int16_t *new_buffer = mlt_pool_alloc( size );
		int j, k = 0;
		
		// Duplicate the existing channels
		for ( i = 0; i < *samples; i++ )
		{
			for ( j = 0; j < *channels; j++ )
			{
				new_buffer[ ( i * *channels ) + j ] = (*buffer)[ ( i * channels_avail ) + k ];
				k = ( k + 1 ) % channels_avail;
			}
		}
		
		// Update the audio buffer now - destroys the old
		mlt_properties_set_data( properties, "audio", new_buffer, size, ( mlt_destructor )mlt_pool_release, NULL );
		
		*buffer = new_buffer;
	}
	else if ( channels_avail == 6 && *channels == 2 )
	{
		// Nasty hack for ac3 5.1 audio - may be a cause of failure?
		int size = *channels * *samples * sizeof( int16_t );
		int16_t *new_buffer = mlt_pool_alloc( size );
		
		// Drop all but the first *channels
		for ( i = 0; i < *samples; i++ )
		{
			new_buffer[ ( i * *channels ) + 0 ] = (*buffer)[ ( i * channels_avail ) + 2 ];
			new_buffer[ ( i * *channels ) + 1 ] = (*buffer)[ ( i * channels_avail ) + 3 ];
		}

		// Update the audio buffer now - destroys the old
		mlt_properties_set_data( properties, "audio", new_buffer, size, ( mlt_destructor )mlt_pool_release, NULL );
		
		*buffer = new_buffer;
	}

	// Return now if no work to do
	if ( output_rate != *frequency )
	{
		// Will store number of samples created
		int used = 0;

		// Create a resampler if nececessary
		if ( resample == NULL || *frequency != mlt_properties_get_int( filter_properties, "last_frequency" ) )
		{
			// Create the resampler
			resample = audio_resample_init( *channels, *channels, output_rate, *frequency );

			// And store it on properties
			mlt_properties_set_data( filter_properties, "audio_resample", resample, 0, ( mlt_destructor )audio_resample_close, NULL );

			// And remember what it was created for
			mlt_properties_set_int( filter_properties, "last_frequency", *frequency );
		}

		// Resample the audio
		used = audio_resample( resample, sample_buffer, *buffer, *samples );

		// Resize if necessary
		if ( used > *samples )
		{
			*buffer = mlt_pool_realloc( *buffer, *samples * *channels * sizeof( int16_t ) );
			mlt_properties_set_data( properties, "audio", *buffer, *channels * used * sizeof( int16_t ), mlt_pool_release, NULL );
		}

		// Copy samples
		memcpy( *buffer, sample_buffer, *channels * used * sizeof( int16_t ) );

		// Update output variables
		*samples = used;
		*frequency = output_rate;
	}

	return 0;
}

/** Filter processing.
*/

static mlt_frame filter_process( mlt_filter this, mlt_frame frame )
{
	// Only call this if we have a means to get audio
	if ( frame->get_audio != NULL )
	{
		// Push the original method on to the stack
		mlt_frame_push_audio( frame, frame->get_audio );

		// Push the filter on to the stack
		mlt_frame_push_audio( frame, this );

		// Assign our get_audio method
		frame->get_audio = resample_get_audio;
	}

	return frame;
}

/** Constructor for the filter.
*/

mlt_filter filter_avresample_init( char *arg )
{
	// Create a filter
	mlt_filter this = mlt_filter_new( );

	// Initialise if successful
	if ( this != NULL )
	{
		// Calculate size of the buffer
		int size = AVCODEC_MAX_AUDIO_FRAME_SIZE * sizeof( int16_t );

		// Allocate the buffer
		int16_t *buffer = mlt_pool_alloc( size );

		// Assign the process method
		this->process = filter_process;

		// Deal with argument
		if ( arg != NULL )
			mlt_properties_set( mlt_filter_properties( this ), "frequency", arg );

		// Default to 2 channel output
		mlt_properties_set_int( mlt_filter_properties( this ), "channels", 2 );

		// Store the buffer
		mlt_properties_set_data( mlt_filter_properties( this ), "buffer", buffer, size, mlt_pool_release, NULL );
	}

	return this;
}