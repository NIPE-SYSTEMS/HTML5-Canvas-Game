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

#ifndef __CANVAS_SETLINEDASH_H__
#define __CANVAS_SETLINEDASH_H__

#include <VG/openvg.h>

void canvas_setLineDash(VGint count, VGfloat *data);
void canvas_setLineDash_cleanup(void);
VGfloat *canvas_setLineDash_get_data(void);
VGint canvas_setLineDash_get_count(void);

#endif /* __CANVAS_SETLINEDASH_H__ */
