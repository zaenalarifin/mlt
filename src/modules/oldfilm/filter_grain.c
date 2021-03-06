/*
 * filter_grain.c -- grain filter
 * Copyright (c) 2007 Marco Gittler <g.marco@freenet.de>
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

#include <framework/mlt_filter.h>
#include <framework/mlt_frame.h>

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#define MIN(a,b) (a<b?a:b)
#define MAX(a,b) (a>b?a:b)

static int filter_get_image( mlt_frame this, uint8_t **image, mlt_image_format *format, int *width, int *height, int writable )
{

	mlt_filter filter = mlt_frame_pop_service( this );
	*format = mlt_image_yuv422;
	int error = mlt_frame_get_image( this, image, format, width, height, 1 );
	
	if ( error == 0 && *image )
	{
		int h = *height;
		int w = *width;
		
		mlt_position in = mlt_filter_get_in( filter );
		mlt_position out = mlt_filter_get_out( filter );
		mlt_position time = mlt_frame_get_position( this );
		double position = ( double )( time - in ) / ( double )( out - in + 1 );
		srand(position*10000);
		
		int noise = mlt_properties_get_int( MLT_FILTER_PROPERTIES( filter ), "noise" );
		double contrast = mlt_properties_get_double( MLT_FILTER_PROPERTIES( filter ), "contrast" )/100.0;
		double brightness = 127.0 * (mlt_properties_get_double( MLT_FILTER_PROPERTIES( filter ), "brightness" )-100.0)/100.0;
		
		int x=0,y=0,pix=0;
		for (x=0;x<w;x++)
			for(y=0;y<h;y++){
				uint8_t* pixel=(*image+(y)*w*2+(x)*2);
				if (*pixel>20){
					pix= MIN ( MAX ( ( (double)*pixel -127.0  ) * contrast + 127.0 + brightness , 0 ) , 255 ) ;
					if (noise>0)
					 pix-=(rand()%noise-noise);
					
					*pixel= MIN ( MAX ( pix , 0 ) , 255 );
				}
				//*(pixel+1)= MIN ( MAX ( ( (double)*(pixel+1) -127.0  ) * .5 + 127.0 , 0 ) , 255 ) ;
			}
	}

	return error;
}

static mlt_frame filter_process( mlt_filter this, mlt_frame frame )
{
	mlt_frame_push_service( frame, this );
	mlt_frame_push_get_image( frame, filter_get_image );
	return frame;
}

mlt_filter filter_grain_init( mlt_profile profile, mlt_service_type type, const char *id, char *arg )
{
	mlt_filter this = mlt_filter_new( );
	if ( this != NULL )
	{
		this->process = filter_process;
		mlt_properties_set( MLT_FILTER_PROPERTIES( this ), "noise", "40" );
		mlt_properties_set( MLT_FILTER_PROPERTIES( this ), "contrast", "160" );
		mlt_properties_set( MLT_FILTER_PROPERTIES( this ), "brightness", "70" );
	}
	return this;
}

