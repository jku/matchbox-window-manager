/* matchbox - a lightweight window manager

   Copyright 2002 Matthew Allum

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
*/

#ifndef _DIALOG_CLIENT_H_
#define _DIALOG_CLIENT_H_

#include "structs.h"
#include "wm.h"
#include "client_common.h"
#include "base_client.h"
#include "main_client.h"
#include "dialog_client.h"
#include "misc.h"

Client* 
dialog_client_new (Wm *w, Window win, Client *trans);

void 
dialog_client_get_coverage (Client *c, int *x, int *y, int *w, int *h);

void 
dialog_client_move_resize (Client *c);

void 
dialog_client_reparent (Client *c);

void 
dialog_client_hide (Client *c);

void 
dialog_client_show (Client *c);

void 
dialog_client_configure (Client *c);

void 
dialog_client_redraw (Client *c, Bool use_cache);

void 
dialog_client_button_press (Client *c, XButtonEvent *e);

void 
dialog_client_destroy (Client *c);

int  
dialog_client_title_height (Client *c);

#endif
