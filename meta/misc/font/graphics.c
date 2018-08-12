/* Copyright Statement:
 *
 * This software/firmware and related documentation ("MediaTek Software") are
 * protected under relevant copyright laws. The information contained herein
 * is confidential and proprietary to MediaTek Inc. and/or its licensors.
 * Without the prior written permission of MediaTek inc. and/or its licensors,
 * any reproduction, modification, use or disclosure of MediaTek Software,
 * and information contained herein, in whole or in part, shall be strictly prohibited.
 */
/* MediaTek Inc. (C) 2010. All rights reserved.
 *
 * BY OPENING THIS FILE, RECEIVER HEREBY UNEQUIVOCALLY ACKNOWLEDGES AND AGREES
 * THAT THE SOFTWARE/FIRMWARE AND ITS DOCUMENTATIONS ("MEDIATEK SOFTWARE")
 * RECEIVED FROM MEDIATEK AND/OR ITS REPRESENTATIVES ARE PROVIDED TO RECEIVER ON
 * AN "AS-IS" BASIS ONLY. MEDIATEK EXPRESSLY DISCLAIMS ANY AND ALL WARRANTIES,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE OR NONINFRINGEMENT.
 * NEITHER DOES MEDIATEK PROVIDE ANY WARRANTY WHATSOEVER WITH RESPECT TO THE
 * SOFTWARE OF ANY THIRD PARTY WHICH MAY BE USED BY, INCORPORATED IN, OR
 * SUPPLIED WITH THE MEDIATEK SOFTWARE, AND RECEIVER AGREES TO LOOK ONLY TO SUCH
 * THIRD PARTY FOR ANY WARRANTY CLAIM RELATING THERETO. RECEIVER EXPRESSLY ACKNOWLEDGES
 * THAT IT IS RECEIVER'S SOLE RESPONSIBILITY TO OBTAIN FROM ANY THIRD PARTY ALL PROPER LICENSES
 * CONTAINED IN MEDIATEK SOFTWARE. MEDIATEK SHALL ALSO NOT BE RESPONSIBLE FOR ANY MEDIATEK
 * SOFTWARE RELEASES MADE TO RECEIVER'S SPECIFICATION OR TO CONFORM TO A PARTICULAR
 * STANDARD OR OPEN FORUM. RECEIVER'S SOLE AND EXCLUSIVE REMEDY AND MEDIATEK'S ENTIRE AND
 * CUMULATIVE LIABILITY WITH RESPECT TO THE MEDIATEK SOFTWARE RELEASED HEREUNDER WILL BE,
 * AT MEDIATEK'S OPTION, TO REVISE OR REPLACE THE MEDIATEK SOFTWARE AT ISSUE,
 * OR REFUND ANY SOFTWARE LICENSE FEES OR SERVICE CHARGE PAID BY RECEIVER TO
 * MEDIATEK FOR SUCH MEDIATEK SOFTWARE AT ISSUE.
 *
 * The following software/firmware and/or related documentation ("MediaTek Software")
 * have been modified by MediaTek Inc. All revisions are subject to any receiver's
 * applicable license agreements with MediaTek Inc.
 */

/*
 * Copyright (C) 2007 The Android Open Source Project
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
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/types.h>

#include <linux/fb.h>
#include <linux/mtkfb.h>
#include <linux/kd.h>
#include <cutils/log.h>
#include <pixelflinger/pixelflinger.h>
#include <string.h>


#include "graphics.h"

#include <math.h>

const int GGL_SUBPIXEL_BITS = 4;

#define  TRI_FRACTION_BITS  (GGL_SUBPIXEL_BITS)
#define  TRI_ONE            (1 << TRI_FRACTION_BITS)
#define  TRI_HALF           (1 << (TRI_FRACTION_BITS-1))
#define  TRI_FROM_INT(x)    ((x) << TRI_FRACTION_BITS)




#include "font_12x22.h"
Font* Font_Small  = &font_12_22;
GRFont *gr_font_small  = NULL;

static GGLContext *gr_context = 0;
static GGLSurface gr_font_texture;
static GGLSurface gr_framebuffer[2];
static GGLSurface gr_mem_surface;
static unsigned gr_active_fb = 0;
static unsigned redoffset_32bit = 16;

static int gr_fb_fd = -1;
static int gr_vt_fd = -1;

static struct fb_var_screeninfo vi;


static void gr_init_font(Font *font, GRFont **gr_font)
{
    GGLSurface *ftex = NULL;
    unsigned char *bits = NULL;
	unsigned char *rle = NULL;
    unsigned char *in = NULL;
	unsigned char data = 0;

    *gr_font = (GRFont *)calloc(sizeof(GRFont), 1);
    ftex = &((*gr_font)->texture);

    bits = (unsigned char *)malloc(font->width * font->height);

    ftex->version = sizeof(*ftex);
    ftex->width = font->width;
    ftex->height = font->height;
    ftex->stride = font->width;
    ftex->data = (void*) bits;
    ftex->format = GGL_PIXEL_FORMAT_A_8;

    in = font->rundata;
    while((data = *in++)) {
        memset(bits, (data & 0x80) ? 255 : 0, data & 0x7f);
        bits += (data & 0x7f);
    }

    (*gr_font)->cwidth = font->cwidth;
    (*gr_font)->cheight = font->cheight;
    (*gr_font)->ascent = font->cheight - 2;
}

int gr_text(GRFont *gr_font, int x, int y, const char *s)
{
    GGLContext *gl = gr_context;
    GRFont *font = gr_font;
    unsigned int off = 0;

    y -= font->ascent;

    gl->bindTexture(gl, &font->texture);
    gl->texEnvi(gl, GGL_TEXTURE_ENV, GGL_TEXTURE_ENV_MODE, GGL_REPLACE);
    gl->texGeni(gl, GGL_S, GGL_TEXTURE_GEN_MODE, GGL_ONE_TO_ONE);
    gl->texGeni(gl, GGL_T, GGL_TEXTURE_GEN_MODE, GGL_ONE_TO_ONE);
    gl->enable(gl, GGL_TEXTURE_2D);

    while((off = *s++)) {
        off -= 32;
        if (off < 96) {
			gl->texCoord2i(gl, (off * font->cwidth) - x, 0 - y);
            gl->recti(gl, x, y, x + font->cwidth, y + font->cheight);
        }
        x += font->cwidth;
    }
    return x;
}

void gr_exit(void)
{
    if (gr_fb_fd != -1)
    {
        close(gr_fb_fd);
        gr_fb_fd = -1;
    }

    free(gr_mem_surface.data);
	gr_mem_surface.data = NULL;

}


int gr_measure(GRFont *gr_font,const char *s)
{
    return gr_font->cwidth * strlen(s);
}

static int get_framebuffer(GGLSurface *fb)
{
    int fd = -1;
    struct fb_fix_screeninfo fi;
	memset(&fi,0,sizeof(fi));
    void *bits = NULL;
    if(fb == NULL)
    {
        return -1;
    }

    fd = open("/dev/graphics/fb0", O_RDWR);
    if (fd < 0) {
        ALOGE("[META] cannot open fb0");
        return -1;
    }

    if (ioctl(fd, FBIOGET_FSCREENINFO, &fi) < 0) {
        ALOGE("[META] failed to get fb0 info");
        close(fd);
        return -1;
    }

    if (ioctl(fd, FBIOGET_VSCREENINFO, &vi) < 0) {
        ALOGE("[META] failed to get fb0 info");
        close(fd);
        return -1;
    }
#ifdef META_MODE_SUPPORT_RGBA_8888
    ALOGD("[META] META_MODE_SUPPORT_RGBA_8888");
    if (ioctl(fd, FBIOGET_VSCREENINFO, &vi) < 0) {
		ALOGE("[META] failed to get fb0 vinfo");
		close(fd);
		return -1;
    }
	if(32 == vi.bits_per_pixel && 0 == vi.red.offset)
	{
		//KK.95
		redoffset_32bit = 0;
	}
	else
	{
		redoffset_32bit = 16;
	    vi.bits_per_pixel = 32;
		vi.transp.offset	= 24;
		vi.transp.length	= 8;

		vi.red.length 	= 8;
		vi.green.length	= 8;
		vi.blue.length	= 8;

		vi.red.offset 	= 16;
		vi.green.offset	= 8;
		vi.blue.offset	= 0;
		}
	if (ioctl(fd, FBIOPUT_VSCREENINFO, &vi) < 0) 
	{
	    ALOGE("[META] failed to put fb0 vinfo");
	    close(fd);
	    return -1;
  }
#endif
	vi.activate		= FB_ACTIVATE_NOW;

    bits = mmap(0, fi.smem_len, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (bits == MAP_FAILED) {
        ALOGE("[META] failed to mmap framebuffer");
        close(fd);
        return -1;
    }

    fb->version = sizeof(*fb);
	if(0 == strncmp(MTK_LCM_PHYSICAL_ROTATION, "270", 3)
	||0 == strncmp(MTK_LCM_PHYSICAL_ROTATION, "90", 2)){
	    fb->width = vi.yres;
    	fb->height = vi.xres;
    	fb->stride = vi.yres;
	}
	else{
		fb->width = vi.xres;
    	fb->height = vi.yres;
    	fb->stride = vi.xres;
	}
	ALOGD("[META] fb->width=%d, fb->height=%d", fb->width, fb->height);
    fb->data = bits;
#if defined(META_MODE_SUPPORT_RGBA_8888)
    ALOGD("[META] fb->width=%d, fb->height=%d", fb->width, fb->height);
    ALOGD("[META] define META_MODE_SUPPORT_RGBA_8888");
        if(redoffset_32bit){
		fb->format = GGL_PIXEL_FORMAT_BGRA_8888;
	}else{
		fb->format = GGL_PIXEL_FORMAT_RGBA_8888;
	}
#else
    ALOGD("[META] not define META_MODE_SUPPORT_RGBA_8888");
    fb->format = GGL_PIXEL_FORMAT_RGB_565;
#endif

    fb++;

    fb->version = sizeof(*fb);
	if(0 == strncmp(MTK_LCM_PHYSICAL_ROTATION, "270", 3)
	||0 == strncmp(MTK_LCM_PHYSICAL_ROTATION, "90", 2)){
	    fb->width = vi.yres;
    	fb->height = vi.xres;
    	fb->stride = vi.yres;
	}
	else{
		fb->width = vi.xres;
    	fb->height = vi.yres;
    	fb->stride = vi.xres;
	}
    ALOGD("[META] fb->width=%d, fb->height=%d", fb->width, fb->height);
#if defined(META_MODE_SUPPORT_RGBA_8888)
    ALOGD("[META] vi.yres=%d, vi.xres_virtual=%d", vi.yres, vi.xres_virtual);
    fb->data = (void*) (((char*)bits) + vi.yres * vi.xres_virtual * 4);
#else
    fb->data = (void*) (((char*)bits) + vi.yres * vi.xres_virtual * 2);
#endif

#if defined(META_MODE_SUPPORT_RGBA_8888)
	if(redoffset_32bit){
		fb->format = GGL_PIXEL_FORMAT_BGRA_8888;
	}else{
		fb->format = GGL_PIXEL_FORMAT_RGBA_8888;
	}
#else
    fb->format = GGL_PIXEL_FORMAT_RGB_565;
#endif

    return fd;
}

static void get_memory_surface(GGLSurface* ms) {
  ms->version = sizeof(*ms);
  if(0 == strncmp(MTK_LCM_PHYSICAL_ROTATION, "270", 3)
	||0 == strncmp(MTK_LCM_PHYSICAL_ROTATION, "90", 2)){
	ms->width = vi.yres;
  	ms->height = vi.xres;
  	ms->stride = vi.yres;
  }
  else{
	ms->width = vi.xres;
  	ms->height = vi.yres;
  	ms->stride = vi.xres;
  }
  
#if defined(META_MODE_SUPPORT_RGBA_8888)
  ms->data = malloc(vi.xres * vi.yres * 4);
#else
  ms->data = malloc(vi.xres * vi.yres * 2);
#endif

#if defined(META_MODE_SUPPORT_RGBA_8888)
	if(redoffset_32bit){
		ms->format = GGL_PIXEL_FORMAT_BGRA_8888;
    }
    else
    {
    	ms->format = GGL_PIXEL_FORMAT_RGBA_8888;
    }
#else
  ms->format = GGL_PIXEL_FORMAT_RGB_565;
#endif
}

static void set_active_framebuffer(unsigned int n)
{
    if (n > 1) return;
    vi.yres_virtual = vi.yres * 2;
    vi.yoffset = n * vi.yres;
	
#if defined(META_MODE_SUPPORT_RGBA_8888)
    vi.bits_per_pixel = 32;
#else
    vi.bits_per_pixel = 16;
#endif
	vi.activate 	= FB_ACTIVATE_NOW;

    if (ioctl(gr_fb_fd, FBIOPUT_VSCREENINFO, &vi) < 0) {
        ALOGE("[META] active fb swap failed");
    }
}

void gr_flip(void)
{
    GGLContext *gl = gr_context;
    unsigned int j = 0;
	unsigned int k = 0;

	
#if defined(META_MODE_SUPPORT_RGBA_8888)
    unsigned fb_lineLength = vi.xres_virtual * 4;
#else
    unsigned fb_lineLength = vi.xres_virtual * 2;
#endif

#if defined(META_MODE_SUPPORT_RGBA_8888)
    unsigned mem_surface_lineLength = vi.xres * 4;
#else
	unsigned mem_surface_lineLength = vi.xres * 2;
#endif
    void *d = NULL;
    if (gr_mem_surface.data == NULL)
    {
        return;
    }
    void *s = gr_mem_surface.data;
	unsigned int width = vi.xres_virtual;
	unsigned int height = vi.yres;
    /* swap front and back buffers */
    gr_active_fb = (gr_active_fb + 1) & 1;
    d = gr_framebuffer[gr_active_fb].data;
    /* copy data from the in-memory surface to the buffer we're about
     * to make active. */
  
    unsigned int l;
    unsigned int lenTemp;
		
#ifdef META_MODE_SUPPORT_RGBA_8888
    unsigned int *s_temp;
    unsigned int *d_temp;
    unsigned int *d_data;		
    lenTemp = sizeof(unsigned int);
    s_temp = (unsigned int*)s;
    d_data = (unsigned int*)d;
#else
    unsigned short *s_temp;
    unsigned short *d_temp;
    unsigned short *d_data;
    lenTemp = sizeof(unsigned short);
    s_temp = (unsigned short*)s;
    d_data = (unsigned short*)d;
#endif		

    if(0 == strncmp(MTK_LCM_PHYSICAL_ROTATION, "270", 3))
    {		
        for (j=0; j<width; j++)
        {
            for (k=0, l=height-1; k<height; k++, l--)
            {
                d_temp = d_data + ((width * l + j) * lenTemp);
                *d_temp = *s_temp++;
            }
        }
    }
    else if(0 == strncmp(MTK_LCM_PHYSICAL_ROTATION, "90", 2))
    {
        for (j=width - 1; j!=0; j--)
        {
            for (k=0, l=0; k<height; k++, l++)
            {
                d_temp = d_data + ((width * l + j) * lenTemp);
                *d_temp = *s_temp++;
            }
        }
    }
    else if(0 == strncmp(MTK_LCM_PHYSICAL_ROTATION, "180", 3))
    {
        for (j=height - 1; j!=0; j--)
        {
            for (k=0, l=vi.xres-1; k<vi.xres; k++, l--)
            {
                d_temp = d_data + ((width * j + l) * lenTemp);
                *d_temp = *s_temp++;
            }
        }
    }
    else
    {
        for (j = 0; j < vi.yres; ++ j)
        {
            memcpy(d, s, mem_surface_lineLength);
            d = (unsigned char *)d + fb_lineLength;
            s = (unsigned char *)s + mem_surface_lineLength;
        }
    }
/*
    memcpy(gr_framebuffer[gr_active_fb].data, gr_mem_surface.data,
           vi.xres * vi.yres * 2);
*/
    /* inform the display driver */
    set_active_framebuffer(gr_active_fb);
}

void gr_color(unsigned char r, unsigned char g, unsigned char b, unsigned char a)
{
    GGLContext *gl = gr_context;
    GGLint color[4];
    color[0] = ((r << 8) | r) + 1;
    color[1] = ((g << 8) | g) + 1;
    color[2] = ((b << 8) | b) + 1;
    color[3] = ((a << 8) | a) + 1;
	if (gl != NULL)
    {
        gl->color4xv(gl, color);
	}
}

void gr_fill(int x, int y, int w, int h)
{
    GGLContext *gl = gr_context;
	if (gl != NULL)
    {
        gl->disable(gl, GGL_TEXTURE_2D);
        gl->recti(gl, x, y, w, h);
	}
}

unsigned int gr_get_width(gr_surface surface) {
    if (surface == NULL) {
        return 0;
    }
    return ((GGLSurface*) surface)->width;
}

unsigned int gr_get_height(gr_surface surface) {
    if (surface == NULL) {
        return 0;
    }
    return ((GGLSurface*) surface)->height;
}

int gr_init(void)
{
    gglInit(&gr_context);
    GGLContext *gl = gr_context;
  
    gr_init_font(Font_Small,  &gr_font_small);

    gr_fb_fd = get_framebuffer(gr_framebuffer);
    if (gr_fb_fd < 0) {
        gr_exit();
        return -1;
    }

    get_memory_surface(&gr_mem_surface);

    fprintf(stderr, "framebuffer: fd %d (%d x %d)\n",
            gr_fb_fd, gr_framebuffer[0].width, gr_framebuffer[0].height);

    /* start with 0 as front (displayed) and 1 as back (drawing) */
    gr_active_fb = 0;

    //set_active_framebuffer(0);
    { 
       vi.yres_virtual = vi.yres * 2;
       vi.yoffset = 0 * vi.yres;
	   
#if defined(META_MODE_SUPPORT_RGBA_8888)
       	vi.bits_per_pixel = 32;
#else
       vi.bits_per_pixel = 16;
#endif
    }

    gl->colorBuffer(gl, &gr_mem_surface);


    gl->activeTexture(gl, 0);
    gl->enable(gl, GGL_BLEND);
    gl->blendFunc(gl, GGL_SRC_ALPHA, GGL_ONE_MINUS_SRC_ALPHA);

    return 0;
}

int gr_fb_width(void)
{
    return gr_framebuffer[0].width;
}

int gr_fb_height(void)
{
    return gr_framebuffer[0].height;
}

gr_pixel *gr_fb_data(void)
{
    return (unsigned short *) gr_mem_surface.data;
}

void init_display()
{
    gr_init();
}

int display_string(int font_color,const char* msg)
{  
    // background color
    gr_color(0, 0, 0, 255);
    gr_fill(0, 0, gr_fb_width(), gr_fb_height());
	
    // printed text color 
    set_gr_color(font_color);
    if (msg[0] != '\0') {
        gr_text(gr_font_small, 1, (1)*CHAR_HEIGHT-1, msg);
    }

	gr_flip();
	ALOGD("[META] display_string end");
	return 0;
}

void exit_display()
{
    gr_exit();
}

