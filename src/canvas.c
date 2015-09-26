/**
 * Copyright (C) 2015 NIPE-SYSTEMS
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include <math.h>
#include <EGL/egl.h>
#include <bcm_host.h>
#include <VG/openvg.h>
#include <VG/vgu.h>
#include <VG/vgext.h>

#include "egl-util.h"
#include "canvas.h"
#include "color.h"

// Current fill and stroke paint
static paint_t fillPaint;
static paint_t strokePaint;

static VGPath immediatePath = 0;
static VGPath currentPath = 0;
static VGfloat currentPath_sx = 0;
static VGfloat currentPath_sy = 0;

// static VGfloat canvas_ellipse_px = 0;
// static VGfloat canvas_ellipse_py = 0;
// static VGfloat canvas_ellipse_vg_rotation = 0;
// static VGfloat canvas_ellipse_angle = 0;

static canvas_state_t currentState;
static canvas_state_t *stateStack = NULL;

static int init = 0;

void canvas__init(void)
{
	egl_init();
	
	printf("{ width: %i, height: %i }\n", egl_get_width(), egl_get_height());
	
	// immediate colors for fill and stroke
	//paint_createColor(&fillPaint, 1, 1, 1, 1);
	//paint_createColor(&strokePaint, 1, 1, 1, 1);
	
	// clear color
	VGfloat clearColor[4] = { 0.0f, 0.0f, 0.0f, 1.0f }; // black
	vgSetfv(VG_CLEAR_COLOR, 4, clearColor);
	
	// reset values
	canvas_lineWidth(1);
	canvas_lineCap(CANVAS_LINE_CAP_BUTT);
	canvas_lineJoin(CANVAS_LINE_JOIN_MITER);
	canvas_globalAlpha(1);
	canvas_lineDashOffset(0);
	canvas_setLineDash(0, NULL);
	currentState.clipping = VG_FALSE;
	currentState.savedLayer = 0;
	currentState.next = NULL;
	
	// immediate path for drawing rects, etc.
	immediatePath = vgCreatePath(VG_PATH_FORMAT_STANDARD, VG_PATH_DATATYPE_F, 1.0f, 0.0f, 0, 0, VG_PATH_CAPABILITY_ALL);
	
	// currentPath for path rendering (beginPath, etc.)
	currentPath = vgCreatePath(VG_PATH_FORMAT_STANDARD, VG_PATH_DATATYPE_F, 1.0f, 0.0f, 0, 0, VG_PATH_CAPABILITY_ALL);
	
	vgSeti(VG_SCISSORING, VG_FALSE);
	vgSeti(VG_MASKING, VG_FALSE);
	
	vgLoadIdentity();
	
	init = 1;
}

static void canvas__destroyState(canvas_state_t *state)
{
	if(state->savedLayer)
	{
		vgDestroyMaskLayer(state->savedLayer);
	}
	
	if(state->dashPattern)
	{
		free(state->dashPattern);
	}
		
}

void canvas__cleanup(void)
{
	if(!init)
	{
		return;
	}
	
	canvas_state_t *current = stateStack;
	while(current)
	{
		canvas_state_t *next = current->next;
		canvas__destroyState(current);
		free(current);
		current = next;
	}
	canvas__destroyState(&currentState);
	
	vgDestroyPath(immediatePath);
	vgDestroyPath(currentPath);
	
	egl_cleanup();
}

unsigned int canvas_stackSize(void)
{
	unsigned int size = 0;
	canvas_state_t *current = stateStack;
	
	while(current)
	{
		current = current->next;
		size++;
	}
	
	return size;
}

void canvas_clearRect(VGfloat x, VGfloat y, VGfloat width, VGfloat height)
{
	vgSeti(VG_SCISSORING, VG_FALSE);
	vgClear(x, y, width, height);
}

void canvas_fillRect(VGfloat x, VGfloat y, VGfloat width, VGfloat height)
{
	paint_activate(&fillPaint, VG_FILL_PATH);
	
	vgClearPath(immediatePath, VG_PATH_CAPABILITY_ALL);
	vguRect(immediatePath, x, y, width, height);
	vgDrawPath(immediatePath, VG_FILL_PATH);
	
}

void canvas_fillStyle(paint_t *paint)
{
	fillPaint = *paint;
}

void canvas_strokeStyle(paint_t *paint)
{
	strokePaint = *paint;
}

void canvas_strokeRect(VGfloat x, VGfloat y, VGfloat width, VGfloat height)
{
	paint_activate(&strokePaint, VG_STROKE_PATH);
	
	vgClearPath(immediatePath, VG_PATH_CAPABILITY_ALL);
	vguRect(immediatePath, x, y, width, height);
	vgDrawPath(immediatePath, VG_STROKE_PATH);
}

void canvas_lineWidth(VGfloat width)
{
	currentState.lineWidth = width;
	
	vgSetf(VG_STROKE_LINE_WIDTH, width);
}

void canvas_lineCap(canvas_line_cap_t line_cap)
{
	currentState.lineCap = line_cap;
	
	vgSeti(VG_STROKE_CAP_STYLE, line_cap);
}

void canvas_lineJoin(canvas_line_join_t line_join)
{
	currentState.lineJoin = line_join;
	
	vgSeti(VG_STROKE_JOIN_STYLE, line_join);
}

void canvas_globalAlpha(VGfloat alpha)
{
	if(alpha > 1 || alpha < 0)
	{
		alpha = 1;
	}
	
	currentState.globalAlpha = alpha;
}

void canvas_beginPath(void)
{
	vgClearPath(currentPath, VG_PATH_CAPABILITY_ALL);
}

void canvas_moveTo(VGfloat x, VGfloat y)
{
	VGubyte segment[1] = { VG_MOVE_TO_ABS };
	VGfloat data[2];
	
	data[0] = x;
	data[1] = y;
	
	currentPath_sx = x;
	currentPath_sy = y;
	
	vgAppendPathData(currentPath, 1, segment, (const void *)data);
}

void canvas_lineTo(VGfloat x, VGfloat y)
{
	VGubyte segment[1] = { VG_LINE_TO_ABS };
	VGfloat data[2];
	
	data[0] = x;
	data[1] = y;
	
	vgAppendPathData(currentPath, 1, segment, (const void *)data);
}

void canvas_quadraticCurveTo(VGfloat cpx, VGfloat cpy, VGfloat x, VGfloat y)
{
	VGubyte segment[1] = { VG_QUAD_TO_ABS };
	VGfloat data[4];
	
	data[0] = cpx;
	data[1] = cpy;
	data[2] = x;
	data[3] = y;
	
	vgAppendPathData(currentPath, 1, segment, (const void *)data);
}

void canvas_bezierCurveTo(VGfloat cp1x, VGfloat cp1y, VGfloat cp2x, VGfloat cp2y, VGfloat x, VGfloat y)
{
	VGubyte segment[1] = { VG_CUBIC_TO_ABS };
	VGfloat data[6];
	
	data[0] = cp1x;
	data[1] = cp1y;
	data[2] = cp2x;
	data[3] = cp2y;
	data[4] = x;
	data[5] = y;
	
	vgAppendPathData(currentPath, 1, segment, (const void *)data);
}

// static void canvas_ellipse_rotate_p(VGfloat rotation)
// {
// 	VGfloat canvas_ellipse_tx = canvas_ellipse_px * cos(rotation) - canvas_ellipse_py * sin(rotation);
// 	canvas_ellipse_py = canvas_ellipse_px * sin(rotation) + canvas_ellipse_py * cos(rotation);
// 	canvas_ellipse_px = canvas_ellipse_tx;
// }

// static void canvas_ellipse_add_arc(VGPathCommand command, VGfloat x, VGfloat y, VGfloat rotation, VGfloat radius_x, VGfloat radius_y)
// {
// 	VGubyte segment[1] = { command };
// 	VGfloat data[5];
	
// 	data[0] = radius_x;
// 	data[1] = radius_y;
// 	data[2] = rotation;
// 	data[3] = x;
// 	data[4] = y;
	
// 	printf("{ radius_x: %f, radius_y: %f, rotation: %f, x: %f, y: %f }\n", radius_x, radius_y, rotation, x, y);
	
// 	vgAppendPathData(currentPath, 1, segment, (const void *)data);
// }

// void canvas_ellipse(VGfloat x, VGfloat y, VGfloat radius_x, VGfloat radius_y, VGfloat rotation, VGfloat start_angle, VGfloat end_angle, VGboolean anticlockwise)
// {
// 	VGubyte segment[1];
// 	VGfloat data[5];
	
// 	// start_angle %= 2 * M_PI;
// 	// end_angle %= 2 * M_PI;
	
// 	if(abs(start_angle) == 2 * M_PI)
// 	{
// 		start_angle *= 0.999;
// 	}
	
// 	if(abs(end_angle) == 2 * M_PI)
// 	{
// 		end_angle *= 0.999;
// 	}
	
// 	if(anticlockwise)
// 	{
// 		if(abs(end_angle - start_angle) > M_PI)
// 		{
// 			segment[0] = VG_LCCWARC_TO_ABS;
// 		}
// 		else
// 		{
// 			segment[0] = VG_SCCWARC_TO_ABS;
// 		}
// 	}
// 	else
// 	{
// 		start_angle *= -1;
// 		end_angle *= -1;
		
// 		if(abs(end_angle - start_angle) > M_PI)
// 		{
// 			segment[0] = VG_LCWARC_TO_ABS;
// 		}
// 		else
// 		{
// 			segment[0] = VG_SCWARC_TO_ABS;
// 		}
// 	}
	
// 	// move from center point to start point at start angle
// 	canvas_moveTo(x + radius_x * cos(start_angle + (rotation * M_PI / 180)), y + radius_y * sin(start_angle + (rotation * M_PI / 180)));
	
// 	data[0] = radius_x;
// 	data[1] = radius_y;
// 	data[2] = rotation;
// 	data[3] = x + radius_x * cos(end_angle + (rotation * M_PI / 180));// + radius_x * cos(end_angle) - radius_x * cos(start_angle);
// 	data[4] = y + radius_y * sin(end_angle + (rotation * M_PI / 180));// + radius_y * sin(end_angle) - radius_y * sin(start_angle);
	
// 	printf("{ radius_x: %f, radius_y: %f, rotation: %f, x: %f, y: %f }\n", radius_x, radius_y, rotation, x, y);
	
// 	vgAppendPathData(currentPath, 1, segment, (const void *)data);
	
	// canvas_ellipse_px = radius_x * cos(start_angle);
	// canvas_ellipse_py = radius_y * sin(start_angle);
	
	// canvas_ellipse_rotate_p(rotation);
	// canvas_ellipse_vg_rotation = rotation * 180.0 / M_PI;
	
	// canvas_moveTo(x + canvas_ellipse_px, y + canvas_ellipse_py);
	
	// if(anticlockwise)
	// {
	// 	if(start_angle - end_angle >= 2 * M_PI)
	// 	{
	// 		start_angle = 2 * M_PI;
	// 		end_angle = 0;
	// 	}
		
	// 	while(end_angle > start_angle)
	// 	{
	// 		end_angle -= 2 * M_PI;
	// 	}
		
	// 	canvas_ellipse_angle = start_angle - M_PI;
		
	// 	while(canvas_ellipse_angle > end_angle)
	// 	{
	// 		canvas_ellipse_px = radius_x * cos(canvas_ellipse_angle);
	// 		canvas_ellipse_py = radius_y * sin(canvas_ellipse_angle);
			
	// 		canvas_ellipse_rotate_p(rotation);
			
	// 		canvas_ellipse_add_arc(VG_SCWARC_TO_ABS, x, y, rotation, radius_x, radius_y);
			
	// 		canvas_ellipse_angle -= 2 * M_PI;
	// 	}
		
	// 	canvas_ellipse_px = radius_x * cos(end_angle);
	// 	canvas_ellipse_py = radius_y * sin(end_angle);
		
	// 	canvas_ellipse_rotate_p(rotation);
		
	// 	canvas_ellipse_add_arc(VG_SCWARC_TO_ABS, x, y, rotation, radius_x, radius_y);
	// }
	// else
	// {
	// 	if(end_angle - start_angle >= 2 * M_PI)
	// 	{
	// 		end_angle = 2 * M_PI;
	// 		start_angle = 0;
	// 	}
		
	// 	while(end_angle < start_angle)
	// 	{
	// 		end_angle += 2 * M_PI;
	// 	}
		
	// 	canvas_ellipse_angle = start_angle + M_PI;
		
	// 	while(canvas_ellipse_angle < end_angle)
	// 	{
	// 		canvas_ellipse_px = radius_x * cos(canvas_ellipse_angle);
	// 		canvas_ellipse_py = radius_y * sin(canvas_ellipse_angle);
			
	// 		canvas_ellipse_rotate_p(rotation);
			
	// 		canvas_ellipse_add_arc(VG_SCCWARC_TO_ABS, x, y, rotation, radius_x, radius_y);
			
	// 		canvas_ellipse_angle += 2 * M_PI;
	// 	}
		
	// 	canvas_ellipse_px = radius_x * cos(end_angle);
	// 	canvas_ellipse_py = radius_y * sin(end_angle);
		
	// 	canvas_ellipse_rotate_p(rotation);
		
	// 	canvas_ellipse_add_arc(VG_SCCWARC_TO_ABS, x, y, rotation, radius_x, radius_y);
	// }
//}

void canvas_arc(VGfloat x, VGfloat y, VGfloat radius, VGfloat start_angle, VGfloat end_angle, VGboolean anticlockwise)
{
	VGfloat angle_extent = 0;
	
	// radian to degrees
	start_angle *= 180 / M_PI;
	end_angle *= 180 / M_PI;
	
	// calculate angle extent
	if(anticlockwise == VG_TRUE)
	{
		angle_extent = 360 - (end_angle - start_angle);
	}
	else
	{
		angle_extent = 0 - (end_angle - start_angle);
	}
	
	vguArc(currentPath, x, y, radius * 2, radius * 2, start_angle, angle_extent, VGU_ARC_OPEN);
}

// static canvas_arcTo_transform(VGfloat px, VGfloat py, VGfloat *x, VGfloat *y)
// {
// 	*x = (px * cos_rotation - py * sin_rotation) * scale_x;
// }

// // radius_y must default to radius_x, rotation must default to 0
// void canvas_arcTo(VGfloat x1, VGfloat y1, VGfloat x2, VGfloat y2, VGfloat radius_x, VGfloat radius_y, VGfloat rotation)
// {
// 	VGfloat scale_x = radius_x / radius_y;
// 	VGfloat cos_rotation = cos(-rotation);
// 	VGfloat sin_rotation = sin(-rotation);
	
// 	VGfloat p0_x = (x * cos_rotation - py * sin_rotation) * scale_x;
// }

void canvas_rect(VGfloat x, VGfloat y, VGfloat width, VGfloat height)
{
	vguRect(currentPath, x, y, width, height);
}

void canvas_setLineDash(VGint count, const VGfloat *data)
{
	currentState.dashCount = count;
	if(count > 0)
	{
		currentState.dashPattern = realloc(currentState.dashPattern, count * sizeof(data));
		if(!currentState.dashPattern) 
		{
			printf("realloc failed\n");
			exit(1);
		}
			
		memcpy(currentState.dashPattern, data, count * sizeof(VGfloat));
	}
	else
	{
		currentState.dashPattern = 0;
	}
		
	vgSetfv(VG_STROKE_DASH_PATTERN, count, data);
}

void canvas_lineDashOffset(VGfloat offset)
{
	currentState.dashOffset = offset;
	vgSetf(VG_STROKE_DASH_PHASE, offset);
}

void canvas_closePath(void)
{
	VGubyte segment[1] = { VG_CLOSE_PATH };
	VGfloat data[2];
	
	data[0] = currentPath_sx;
	data[1] = currentPath_sy;
	
	vgAppendPathData(currentPath, 1, segment, (const void *)data);
}

void canvas_clip(void)
{
	if(!currentState.clipping)
	{
		vgMask(VG_INVALID_HANDLE, VG_FILL_MASK, 0, 0, egl_get_width(), egl_get_height());
	}
	
	vgRenderToMask(currentPath, VG_FILL_PATH, VG_INTERSECT_MASK);
	
	vgSeti(VG_MASKING, VG_TRUE);
	
	currentState.clipping = VG_TRUE;
}

void canvas_save(void)
{
	canvas_state_t *savedState = malloc(sizeof(canvas_state_t));
	if(!savedState)
	{
		printf("malloc failed\n");
		exit(1);
	}
	
	memcpy(savedState, &currentState, sizeof(canvas_state_t));
	
	// may be NULL
	savedState->next = stateStack;
	
	stateStack = savedState;
	
	if(currentState.clipping)
	{	
		savedState->savedLayer = vgCreateMaskLayer(egl_get_width(), egl_get_height());
		vgCopyMask(savedState->savedLayer, 0, 0, 0, 0, egl_get_width(), egl_get_height());
	}
	
	if(currentState.dashCount > 0)
	{
		savedState->dashPattern = malloc(currentState.dashCount * sizeof(VGfloat));
		if(!savedState->dashPattern)
		{
			printf("malloc failed\n");
			exit(1);
		}
		
		memcpy(savedState->dashPattern, &currentState.dashPattern, currentState.dashCount * sizeof(VGfloat));
	}
	
	
}

void canvas_restore(void)
{
	if(!stateStack) 
	{
		printf("Can't restore state. No state on stack.\n");
		return;
	}
	
	canvas_state_t *restore = stateStack;
	
	if(restore->clipping)
	{
		vgMask(restore->savedLayer, VG_SET_MASK, 0, 0, egl_get_width(), egl_get_height());
		vgDestroyMaskLayer(restore->savedLayer);
	}
	
	vgSeti(VG_MASKING, restore->clipping);
	currentState.clipping = restore->clipping;
	canvas_lineWidth(restore->lineWidth);
	canvas_lineCap(restore->lineCap);
	canvas_lineJoin(restore->lineJoin);
	canvas_globalAlpha(restore->globalAlpha);
	canvas_lineDashOffset(restore->dashOffset);
	//currentState.fillPaint = restore->fillPaint;
	//currentState.strokeColor = restore->strokeColor;
	
	if(currentState.dashPattern)
		free(currentState.dashPattern);
	
	currentState.dashPattern = restore->dashPattern;
	currentState.dashCount = restore->dashCount;
	
	stateStack = restore->next;
	free(restore);
	
}

canvas_state_t* canvas_getState(void)
{
	return &currentState;
}



void canvas_stroke(void)
{
	paint_activate(&strokePaint, VG_STROKE_PATH);
	
	vgDrawPath(currentPath, VG_STROKE_PATH);
}

void canvas_fill(void)
{
	paint_activate(&fillPaint, VG_FILL_PATH);
	
	vgDrawPath(currentPath, VG_FILL_PATH);
}
