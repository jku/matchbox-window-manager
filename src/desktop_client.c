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


#include "desktop_client.h"

Client*
desktop_client_new(Wm *w, Window win)
{
   Client *c = NULL; 
   if (w->flags & DESKTOP_DECOR_FLAG)
     {
       c = main_client_new(w, win);
       /* c->type = desktop; */
       c->flags  |= CLIENT_IS_DESKTOP_FLAG; /* XXX Needed ? */
       return c;
     }

   c = base_client_new(w, win); 
   c->type         = desktop;
   c->configure    = &desktop_client_configure;
   c->reparent     = &desktop_client_reparent;
   c->move_resize  = &desktop_client_move_resize;
   c->show         = &desktop_client_show;
   c->destroy      = &desktop_client_destroy;

   if (w->main_client && (c->wm->flags & SINGLE_FLAG))
   {
      main_client_redraw(c->wm->main_client, False);
   }

   return c;
}

void
desktop_client_reparent(Client *c)
{
   c->frame = c->window;
}

void
desktop_client_move_resize(Client *c)
{
   XMoveResizeWindow(c->wm->dpy, c->window, c->x, c->y, c->width, c->height );
}

void
desktop_client_configure(Client *c)
{   
   c->width = c->wm->dpy_width;
   c->height = c->wm->dpy_height;

   c->x = 0;
   c->y = 0;

}

void
desktop_client_show(Client *c)
{
  base_client_show(c);
  wm_toggle_desktop(c->wm);
}

void
desktop_client_destroy(Client *c)
{
   Wm *w = c->wm;

   base_client_destroy(c); 

   /* Below is need to make sure app window task menu button gets updated */
   if (w->main_client 
       &&  w->main_client == client_get_prev(w->main_client, mainwin))
   {
     main_client_redraw(w->main_client, False);
   }

   /* Make sure desktop flag is unset */
   w->flags &= ~DESKTOP_RAISED_FLAG;
}
