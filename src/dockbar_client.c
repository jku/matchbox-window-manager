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

#include "dockbar_client.h"

static int dockbar_client_orientation_calc(Client *c);

Client*
dockbar_client_new(Wm *w, Window win)
{
   Client *c = base_client_new(w, win); 

   if (!c) return NULL;

   c->type         = dock;
   c->configure    = &dockbar_client_configure;
   c->show         = &dockbar_client_show;
   c->hide         = &dockbar_client_hide;
   c->move_resize  = &dockbar_client_move_resize;
   c->destroy      = &dockbar_client_destroy;

   c->flags        = dockbar_client_orientation_calc(c);
   
   c->frame = c->window;
   return c;
}

static int
dockbar_client_orientation_calc(Client *c)
{
  Wm *w = c->wm;

  dbg("%s() called, x:%i, y:%i, w:%i h:%i\n", __func__, 
      c->x, c->y, c->width, c->height);

  /*   - Can only have one titlebar panel  
   *   - if theme does not have title bar it wont get mapped. 
   */
  if (!w->have_titlebar_panel) /* && mbtheme_has_titlebar_panel(w->mbtheme)) */
    {
      if (ewmh_state_check(c, c->wm->atoms[MB_WM_STATE_DOCK_TITLEBAR]))
	{
	  w->have_titlebar_panel = c;

	  /* Does the panel still want to be shown when we map the desktop ? */
	  if (ewmh_state_check(c, c->wm->atoms[MB_DOCK_TITLEBAR_SHOW_ON_DESKTOP]))
	    return CLIENT_DOCK_TITLEBAR|CLIENT_DOCK_TITLEBAR_SHOW_ON_DESKTOP;

	  return CLIENT_DOCK_TITLEBAR;
	}
    }

  if (c->width > c->height)	/* Assume Horizonal north/south Dock */
    {
      if (c->y < (w->dpy_height/2))
	return CLIENT_DOCK_NORTH;
      else
	return CLIENT_DOCK_SOUTH;
    }
  else
    {
      if (c->x < (w->dpy_width/2))
	return CLIENT_DOCK_WEST;
      else
	return CLIENT_DOCK_EAST;
    }
}

void
dockbar_client_configure(Client *c)
{
   int n_offset = wm_get_offsets_size(c->wm, NORTH, c, False);
   int s_offset = wm_get_offsets_size(c->wm, SOUTH, c, False);
   int e_offset = wm_get_offsets_size(c->wm, EAST,  NULL, True);
   int w_offset = wm_get_offsets_size(c->wm, WEST,  c, True);
   
   /* XXX - we should check for overlapping and if this happens
            change em to normal clients. 
   */

   if (c->flags & CLIENT_DOCK_NORTH)
     {
       c->y = n_offset;
       c->x = w_offset;
       c->width  = c->wm->dpy_width - e_offset - w_offset;
     }
   else if (c->flags & CLIENT_DOCK_SOUTH)
     {
       c->y = c->wm->dpy_height - s_offset - c->height;
       c->x = w_offset;
       c->width  = c->wm->dpy_width - e_offset - w_offset;
     }
   else if (c->flags & CLIENT_DOCK_WEST)
     {
       c->y = 0;
       c->x = w_offset;
       c->height = c->wm->dpy_height;
     }
   else if (c->flags & CLIENT_DOCK_EAST)
     {
       c->y = 0;
       c->x = c->wm->dpy_width - e_offset;
       c->height = c->wm->dpy_height;
     }
   else if (c->flags & CLIENT_DOCK_TITLEBAR)
     {
       XRectangle rect;

       mbtheme_get_titlebar_panel_rect(c->wm->mbtheme, &rect, NULL);
 
       c->x      = rect.x + w_offset; 
       c->y      = rect.y + n_offset;
       c->width  = rect.width;
       c->height = rect.height;
     }
   else
     {
       dbg("%s() : EEEK no dock type flag set !\n", __func__ );
     }

   dbg("%s() sizing as %i %i %i %i", __func__, c->x, c->y, 
       c->width, c->height);
   
   XSetWindowBorderWidth(c->wm->dpy, c->window, 0);
   XSetWindowBorder(c->wm->dpy, c->window, 0);
   
}

void
dockbar_client_move_resize(Client *c)
{
   base_client_move_resize(c);

   dbg("%s() to %s  x: %i , y: %i w: %i h: %i \n", 
       __func__, c->name, c->x, c->y, c->width, c->height);

   XResizeWindow(c->wm->dpy, c->window, c->width, c->height);
   XMoveWindow(c->wm->dpy, c->window, c->x, c->y);
}

void
dockbar_client_show(Client *c) /*TODO: show and hide share common static func*/
{
   if (client_get_state(c) == NormalState) return;

   dbg("%s() called\n", __func__);

   XGrabServer(c->wm->dpy);
   
   if (c->flags & CLIENT_DOCK_EAST || c->flags & CLIENT_DOCK_WEST)
     wm_restack(c->wm, c, - c->width);
   else if ( c->flags & CLIENT_DOCK_NORTH )
     {
       wm_restack(c->wm, c, - c->height);
     }
   else if ( c->flags & CLIENT_DOCK_SOUTH )
     wm_restack(c->wm, c, - c->height);
   
   client_set_state(c, NormalState);

   if (c->flags & CLIENT_DOCK_TITLEBAR)
     {

       dbg("%s() is titlebar dock\n", __func__);

       if (!mbtheme_has_titlebar_panel(c->wm->mbtheme))
	 {
	   /* Theme does not have titlebar so we dont actually map it */
	   dbg("%s() not mapping titlebar dock\n", __func__);

	   XUngrabServer(c->wm->dpy);
	   dockbar_client_hide(c);
	   return;
	 }

       if (c->wm->main_client 
	   && !(c->wm->main_client->flags & CLIENT_FULLSCREEN_FLAG)
	   && !(c->wm->flags & DESKTOP_RAISED_FLAG))
	 XMapRaised(c->wm->dpy, c->window);

       if (c->wm->flags & DESKTOP_RAISED_FLAG 
	   && c->flags & CLIENT_DOCK_TITLEBAR_SHOW_ON_DESKTOP)
	 XMapRaised(c->wm->dpy, c->window);
     }
   else XMapRaised(c->wm->dpy, c->window);

   base_client_show(c);

   XUngrabServer(c->wm->dpy);
}

void
dockbar_client_hide(Client *c)
{

   if (client_get_state(c) == IconicState) return;
   XGrabServer(c->wm->dpy);
   client_set_state(c, IconicState);

   if (c->flags & CLIENT_DOCK_EAST || c->flags & CLIENT_DOCK_WEST)
     wm_restack(c->wm, c, c->width);
   else if (!(c->flags & CLIENT_DOCK_TITLEBAR))
     wm_restack(c->wm, c, c->height);

   base_client_hide(c);

   XUngrabServer(c->wm->dpy);
}

void
dockbar_client_destroy(Client *c)
{
   //dockbar_client_hide(c);
  if (c == c->wm->have_titlebar_panel)
    c->wm->have_titlebar_panel = NULL;

   if (c->flags & CLIENT_DOCK_EAST || c->flags & CLIENT_DOCK_WEST)
     wm_restack(c->wm, c, c->width );
   else if (!(c->flags & CLIENT_DOCK_TITLEBAR))
     wm_restack(c->wm, c, c->height);

   base_client_destroy(c);
}

