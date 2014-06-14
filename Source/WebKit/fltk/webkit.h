/*
WebkitFLTK
Copyright (C) 2014 Lauri Kasanen

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU Affero General Public License as published by
the Free Software Foundation, version 3 of the License.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU Affero General Public License for more details.

You should have received a copy of the GNU Affero General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef webkit_h
#define webkit_h

#include <FL/Fl.H>
#include <FL/Fl_Window.H>
#include <FL/Fl_Widget.H>

#include "webview.h"

// Init. Call this before show()ing anything.
void webkitInit();

// Use this for per-page useragents.
void wk_set_useragent_func(const char * (*func)(const char *));

// Content blocking. Return 0 for ok, 1 for block.
void wk_set_urlblock_func(int (*func)(const char *));

#endif
