/*
 * producer_libdv.c -- a DV encoder based on libdv
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

// Local header files
#include "consumer_libdv.h"

// mlt Header files
#include <framework/mlt_frame.h>

// System header files
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

// libdv header files
#include <libdv/dv.h>

// Forward references.
static int consumer_encode_video( mlt_consumer this, uint8_t *dv_frame, mlt_frame frame );
static void consumer_encode_audio( mlt_consumer this, uint8_t *dv_frame, mlt_frame frame );
static void consumer_output( mlt_consumer this, uint8_t *dv_frame, int size, mlt_frame frame );
static void *consumer_thread( void *arg );
static void consumer_close( mlt_consumer this );

/** Initialise the dv consumer.
*/

mlt_consumer consumer_libdv_init( char *arg )
{
	// Allocate the consumer
	mlt_consumer this = calloc( 1, sizeof( struct mlt_consumer_s ) );

	// If memory allocated and initialises without error
	if ( this != NULL && mlt_consumer_init( this, NULL ) == 0 )
	{
		// Get properties from the consumer
		mlt_properties properties = mlt_consumer_properties( this );

		// Allocate a thread
		pthread_t *thread = calloc( 1, sizeof( pthread_t ) );

		// Assign close callback
		this->close = consumer_close;

		// Assign all properties
		if ( arg == NULL || !strcmp( arg, "PAL" ) )
			mlt_properties_set_double( properties, "fps", 25 );
		else
			mlt_properties_set_double( properties, "fps", 29.97 );

		mlt_properties_set_data( properties, "thread", thread, sizeof( pthread_t ), free, NULL );
		mlt_properties_set_data( properties, "video", consumer_encode_video, 0, NULL, NULL );
		mlt_properties_set_data( properties, "audio", consumer_encode_audio, 0, NULL, NULL );
		mlt_properties_set_data( properties, "output", consumer_output, 0, NULL, NULL );

		// Create the thread (this should not happen immediately)
		mlt_properties_set_int( properties, "running", 1 );
		pthread_create( thread, NULL, consumer_thread, this );
	}
	else
	{
		// Clean up in case of init failure
		free( this );
		this = NULL;
	}

	// Return this
	return this;
}

/** Get or create a new libdv encoder.
*/

static dv_encoder_t *libdv_get_encoder( mlt_consumer this, mlt_frame frame )
{
	// Get the properties of the consumer
	mlt_properties this_properties = mlt_consumer_properties( this );

	// Obtain the dv_encoder
	dv_encoder_t *encoder = mlt_properties_get_data( this_properties, "dv_encoder", NULL );

	// Construct one if we don't have one
	if ( encoder == NULL )
	{
		// Get the fps of the consumer (for now - should be from frame)
		double fps = mlt_properties_get_double( this_properties, "fps" );

		// Create the encoder
		encoder = dv_encoder_new( 0, 0, 0 );

		// Encoder settings
		encoder->isPAL = fps == 25;
		encoder->is16x9 = 0;
		encoder->vlc_encode_passes = 1;
		encoder->static_qno = 0;
		encoder->force_dct = DV_DCT_AUTO;

		// Store the encoder on the properties
		mlt_properties_set_data( this_properties, "dv_encoder", encoder, 0, ( mlt_destructor )dv_encoder_free, NULL );

		// Convenience for image dimensions
		mlt_properties_set_int( this_properties, "width", 720 );
		mlt_properties_set_int( this_properties, "height", fps == 25 ? 576 : 480 );
	}

	// Return the encoder
	return encoder;
}


/** The libdv encode video method.
*/

static int consumer_encode_video( mlt_consumer this, uint8_t *dv_frame, mlt_frame frame )
{
	// Obtain the dv_encoder
	dv_encoder_t *encoder = libdv_get_encoder( this, frame );

	// Get the properties of the consumer
	mlt_properties this_properties = mlt_consumer_properties( this );

	// This will hold the size of the dv frame
	int size = 0;

	// determine if this a test card
	int is_test = mlt_frame_is_test_card( frame );

	// If we get an encoder, then encode the image
	if ( encoder != NULL )
	{
		// Specify desired image properties
		mlt_image_format fmt = mlt_image_yuv422;
		int width = mlt_properties_get_int( this_properties, "width" );
		int height = mlt_properties_get_int( this_properties, "height" );
		uint8_t *image = NULL;

		// Get the image
		mlt_frame_get_image( frame, &image, &fmt, &width, &height, 0 );

		// Check that we get what we expected
		if ( fmt != mlt_image_yuv422 || 
			 width != mlt_properties_get_int( this_properties, "width" ) ||
			 height != mlt_properties_get_int( this_properties, "height" ) ||
			 image == NULL )
		{
			// We should try to recover here
			fprintf( stderr, "We have a problem houston...\n" );
		}
		else
		{
			// Calculate the size of the dv frame
			size = height == 576 ? frame_size_625_50 : frame_size_525_60;
		}

		// Process the frame
		if ( size != 0 && !( mlt_properties_get_int( this_properties, "was_test_card" ) && is_test ) )
		{
			// Encode the image
			dv_encode_full_frame( encoder, &image, e_dv_color_yuv, dv_frame );

			// Note test card status
			mlt_properties_set_int( this_properties, "was_test_card", is_test );
		}
	}
	
	return size;
}

/** The libdv encode audio method.
*/

static void consumer_encode_audio( mlt_consumer this, uint8_t *dv_frame, mlt_frame frame )
{
	// Get the properties of the consumer
	mlt_properties this_properties = mlt_consumer_properties( this );

	// Obtain the dv_encoder
	dv_encoder_t *encoder = libdv_get_encoder( this, frame );

	// Only continue if we have an encoder
	if ( encoder != NULL )
	{
		// Get the frame count
		int count = mlt_properties_get_int( this_properties, "count" );

		// Default audio args
		mlt_audio_format fmt = mlt_audio_pcm;
		int channels = 2;
		int frequency = 48000;
		int samples = mlt_sample_calculator( mlt_properties_get_double( this_properties, "fps" ), frequency, count );
		int16_t *pcm = NULL;

		// Get the frame number
		time_t start = time( NULL );
		int height = mlt_properties_get_int( this_properties, "height" );
		int is_pal = height == 576;
		int is_wide = 0;

		// Temporary - audio buffer allocation
		int16_t *audio_buffers[ 4 ];
		int i = 0;
		int j = 0;
		for ( i = 0 ; i < 4; i ++ )
			audio_buffers[ i ] = malloc( 2 * DV_AUDIO_MAX_SAMPLES );

		// Get the audio
		mlt_frame_get_audio( frame, &pcm, &fmt, &frequency, &channels, &samples );

		// Inform the encoder of the number of audio samples
		encoder->samples_this_frame = samples;

		// Fill the audio buffers correctly
		for ( i = 0; i < samples; i ++ )
			for ( j = 0; j < channels; j++ )
				audio_buffers[ j ][ i ] = *pcm ++;

		// Encode audio on frame
		dv_encode_full_audio( encoder, audio_buffers, channels, frequency, dv_frame );

		// Specify meta data on the frame
		dv_encode_metadata( dv_frame, is_pal, is_wide, &start, count );
		dv_encode_timecode( dv_frame, is_pal, count );

		// Update properties
		mlt_properties_set_int( this_properties, "count", ++ count );

		// Temporary - free audio buffers
		for ( i = 0 ; i < 4; i ++ )
			free( audio_buffers[ i ] );
	}
}

/** The libdv output method.
*/

static void consumer_output( mlt_consumer this, uint8_t *dv_frame, int size, mlt_frame frame )
{
	fwrite( dv_frame, size, 1, stdout );
	fflush( stdout );
}

/** The main thread - the argument is simply the consumer.
*/

static void *consumer_thread( void *arg )
{
	// Map the argument to the object
	mlt_consumer this = arg;

	// Get the properties
	mlt_properties properties = mlt_consumer_properties( this );

	// Get the handling methods
	int ( *video )( mlt_consumer, uint8_t *, mlt_frame ) = mlt_properties_get_data( properties, "video", NULL );
	int ( *audio )( mlt_consumer, uint8_t *, mlt_frame ) = mlt_properties_get_data( properties, "audio", NULL );
	int ( *output )( mlt_consumer, uint8_t *, int, mlt_frame ) = mlt_properties_get_data( properties, "output", NULL );

	// Allocate a single PAL frame for encoding
	uint8_t *dv_frame = malloc( frame_size_625_50 );

	// Get the service associated to the consumer
	mlt_service service = mlt_consumer_service( this );

	// Define a frame pointer
	mlt_frame frame;

	// Loop while running
	while( mlt_properties_get_int( properties, "running" ) )
	{
		// Get the frame
		if ( mlt_service_get_frame( service, &frame, 0 ) == 0 )
		{
			// Encode the image
			int size = video( this, dv_frame, frame );

			// Encode the audio
			if ( size > 0 )
				audio( this, dv_frame, frame );

			// Output the frame
			output( this, dv_frame, size, frame );

			// Close the frame
			mlt_frame_close( frame );
		}
	}

	// Tidy up
	free( dv_frame );

	return NULL;
}

/** Close the consumer.
*/

static void consumer_close( mlt_consumer this )
{
	// Get the properties
	mlt_properties properties = mlt_consumer_properties( this );

	// Get the thread
	pthread_t *thread = mlt_properties_get_data( properties, "thread", NULL );

	// Stop the thread
	mlt_properties_set_int( properties, "running", 0 );

	// Wait for termination
	pthread_join( *thread, NULL );

	// Close the parent
	mlt_consumer_close( this );

	// Free the memory
	free( this );
}
