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

/*
  $Id: main_client.c,v 1.3 2004/02/25 13:10:48 mallum Exp $
*/

#include "main_client.h"

#define VBW(c) 5
#define SBW(c) 5

Client*
main_client_new(Wm *w, Window win)
{
   Client *c = base_client_new(w, win); 

   if (!c) return NULL;
   
   c->type = mainwin;
   c->reparent     = &main_client_reparent;
   c->redraw       = &main_client_redraw;
   c->button_press = &main_client_button_press;
   c->move_resize  = &main_client_move_resize;
   c->get_coverage = &main_client_get_coverage;
   c->hide         = &main_client_hide;
   c->show         = &main_client_show;
   c->configure    = &main_client_configure;
   c->destroy      = &main_client_destroy;
   c->iconize      = &main_client_iconize;

   main_client_check_for_state_hints(c);

   main_client_check_for_single(c);
   
   return c;
}

void
main_client_check_for_state_hints(Client *c)
{
  dbg("%s() checking for fullscreen: %s\n", __func__,
      XGetAtomName(c->wm->dpy, c->wm->atoms[WINDOW_STATE_FULLSCREEN]));

  if (ewmh_state_check(c, c->wm->atoms[WINDOW_STATE_FULLSCREEN]))
    {
      c->flags ^= CLIENT_FULLSCREEN_FLAG;
      dbg("%s() client is fullscreen\n", __func__);
    }
}

void
main_client_check_for_single(Client *c)
{
   if (c->wm->flags & SINGLE_FLAG)
   {
      /* There was only one client till this came along */
      c->wm->flags ^= SINGLE_FLAG; /* turn off single flag */
      main_client_redraw(c->wm->main_client, False);
   } else if (!c->wm->main_client) /* This must be the only client*/
      c->wm->flags ^= SINGLE_FLAG; /* turn on single flag */      
}

void
main_client_configure(Client *c)
{
  int frm_size = main_client_title_height(c);
  int offset_south = theme_frame_defined_height_get(c->wm->mbtheme, 
						    FRAME_MAIN_SOUTH);
  int offset_east  = theme_frame_defined_width_get(c->wm->mbtheme, 
						   FRAME_MAIN_EAST );
  int offset_west  = theme_frame_defined_width_get(c->wm->mbtheme, 
						   FRAME_MAIN_WEST );

  int h = wm_get_offsets_size(c->wm, SOUTH, NULL, True);

   if ( c->flags & CLIENT_FULLSCREEN_FLAG )
     { 
       dbg("%s() window is fullscreen\n", __func__ );
       c->y = 0;  
       c->x = 0;
       c->height = c->wm->dpy_height;
       c->width  = c->wm->dpy_width;
     }
   else
     {
       c->y = wm_get_offsets_size(c->wm, NORTH, NULL, False) + frm_size;
       c->x = wm_get_offsets_size(c->wm, WEST,  NULL, False) + offset_west;
       c->width  = c->wm->dpy_width - ( offset_east + offset_west ) 
	 - wm_get_offsets_size(c->wm, EAST,  NULL, False)
	 - wm_get_offsets_size(c->wm, WEST,  NULL, False);

       c->height = c->wm->dpy_height - c->y - h - offset_south;
     }

   dbg("%s() configured as %i*%i+%i+%i, frame size is %i\n", __func__, c->width, c->height, c->x, c->y, frm_size);
   
   c->wm->main_client = c;  /* XXX Should this be here ?  */
}

int
main_client_title_height(Client *c)
{
   if ( (!c->wm->config->use_title)
	|| c->flags & CLIENT_FULLSCREEN_FLAG) return 0;

   if ((c->wm->flags & TITLE_HIDDEN_FLAG) && c->type == mainwin)
      return TITLE_HIDDEN_SZ;

   return theme_frame_defined_height_get(c->wm->mbtheme, FRAME_MAIN);

}

void
main_client_get_coverage(Client *c, int *x, int *y, int *w, int *h)
{
  int offset_south = theme_frame_defined_height_get(c->wm->mbtheme, 
						    FRAME_MAIN_SOUTH);
  int offset_east  = theme_frame_defined_width_get(c->wm->mbtheme, 
						   FRAME_MAIN_EAST );
  int offset_west  = theme_frame_defined_width_get(c->wm->mbtheme, 
						   FRAME_MAIN_WEST );

   *x = c->x - offset_west; 
   *y = c->y - main_client_title_height(c);
   *w = c->width + offset_east + offset_west;
   *h = c->height + main_client_title_height(c) + offset_south;

   dbg("%s() +%i+%i, %ix%i\n", __func__, *x, *y, *w, *h);
}

void
main_client_reparent(Client *c)
{
    XSetWindowAttributes attr;

    int frm_size = main_client_title_height(c);
    int offset_south = theme_frame_defined_height_get(c->wm->mbtheme, 
						      FRAME_MAIN_SOUTH);
    int offset_east  = theme_frame_defined_width_get(c->wm->mbtheme, 
						     FRAME_MAIN_EAST );
    int offset_west  = theme_frame_defined_width_get(c->wm->mbtheme, 
						     FRAME_MAIN_WEST );
    attr.override_redirect = True;
    attr.background_pixel  = BlackPixel(c->wm->dpy, c->wm->screen);
    attr.event_mask        = ChildMask|ButtonMask|ExposureMask;
    
    c->frame =
       XCreateWindow(c->wm->dpy, c->wm->root, 
		     c->x - offset_west, 
		     c->y - frm_size,
		     c->width + offset_east + offset_west, 
		     c->height + frm_size + offset_south, 
		     0,
		     CopyFromParent, CopyFromParent, CopyFromParent,
		     CWOverrideRedirect|CWEventMask|CWBackPixel,
		     &attr);

   dbg("%s frame created : %i*%i+%i+%i\n", __func__, c->width, c->height + frm_size, c->x, c->y);

    attr.background_pixel = BlackPixel(c->wm->dpy, c->wm->screen);
    
    c->title_frame =
       XCreateWindow(c->wm->dpy, c->frame, 0, 0, 
		     c->width + ( offset_east + offset_west), 
		     frm_size + c->height + offset_south, 0,
		     CopyFromParent, CopyFromParent, CopyFromParent,
		     /*CWOverrideRedirect|*/CWBackPixel|CWEventMask, &attr);
    
   XSetWindowBorderWidth(c->wm->dpy, c->window, 0);
   XAddToSaveSet(c->wm->dpy, c->window);
   XSelectInput(c->wm->dpy, c->window,
		/*ButtonPressMask|*/ ColormapChangeMask|PropertyChangeMask);
   
   XReparentWindow(c->wm->dpy, c->window, c->frame, offset_west, frm_size);
}

void
main_client_move_resize(Client *c)
{
  int offset_south = theme_frame_defined_height_get(c->wm->mbtheme, 
						    FRAME_MAIN_SOUTH);
  int offset_east  = theme_frame_defined_width_get(c->wm->mbtheme, 
						   FRAME_MAIN_EAST );
  int offset_west  = theme_frame_defined_width_get(c->wm->mbtheme, 
						   FRAME_MAIN_WEST );
   base_client_move_resize(c);
   //XResizeWindow(c->wm->dpy, c->window, c->width, c->height);

   XMoveResizeWindow(c->wm->dpy, c->window, 
		     offset_west, main_client_title_height(c), 
		     c->width, c->height);

   XResizeWindow(c->wm->dpy, c->title_frame, 
		 c->width + (offset_east + offset_west),
		 c->height + main_client_title_height(c) + offset_south);

   XMoveResizeWindow(c->wm->dpy, c->frame, 
		     c->x - offset_west,
		     c->y - main_client_title_height(c), 
		     c->width + ( offset_east + offset_west),
		     c->height + main_client_title_height(c) + offset_south);

}

void
main_client_toggle_fullscreen(Client *c)
{
  XGrabServer(c->wm->dpy);

  c->flags ^= CLIENT_FULLSCREEN_FLAG;
  main_client_configure(c);
  main_client_move_resize(c);

  if (c->wm->have_titlebar_panel 
      && mbtheme_has_titlebar_panel(c->wm->mbtheme))
    {
      if (c->flags & CLIENT_FULLSCREEN_FLAG)
	{
	  c->wm->have_titlebar_panel->ignore_unmap++;
	  XUnmapWindow(c->wm->dpy, c->wm->have_titlebar_panel->frame);
	}
      else
	{
	  XMapRaised(c->wm->dpy, c->wm->have_titlebar_panel->frame);
	}

    }

  XUngrabServer(c->wm->dpy);
}

/* redraws the frame */
void
main_client_redraw(Client *c, Bool use_cache)
{
  Bool is_shaped = False;
  int w = 0, h = 0;
  int offset_south = theme_frame_defined_height_get(c->wm->mbtheme, 
						      FRAME_MAIN_SOUTH);
  int offset_east  = theme_frame_defined_width_get(c->wm->mbtheme, 
						   FRAME_MAIN_EAST );
  int offset_west  = theme_frame_defined_width_get(c->wm->mbtheme, 
						   FRAME_MAIN_WEST );
 
  w = c->width + offset_east + offset_west;
  h = theme_frame_defined_height_get(c->wm->mbtheme, FRAME_MAIN);

  dbg("%s() called on %s\n", __func__, c->name);

   if (!c->wm->config->use_title) return;

   if (c->wm->flags & TITLE_HIDDEN_FLAG)
   {
      XUnmapWindow(c->wm->dpy, c->title_frame);
      return;
   }

   if (use_cache && c->have_set_bg)  return ;
   
   is_shaped = theme_frame_wants_shaped_window( c->wm->mbtheme, FRAME_MAIN);

   dbg("%s() cache failed, actual redraw on %s\n", __func__, c->name);

   if (c->backing == None)
      client_init_backing(c, c->width + offset_east + offset_west, 
			     c->height + offset_south + h);

   if (is_shaped) 
     client_init_backing_mask(c, c->width + offset_east + offset_west, 
			      c->height, 
			      h , offset_south,
			      w - offset_east, offset_west);

   dbg("%s() calling theme_frame_paint()\n", __func__); 
   theme_frame_paint(c->wm->mbtheme, c, FRAME_MAIN, 0, 0, w, h); 

   theme_frame_paint(c->wm->mbtheme, c, FRAME_MAIN_WEST, 
		     0, h, offset_west, c->height); 
  
   theme_frame_paint(c->wm->mbtheme, c, FRAME_MAIN_EAST, 
		     c->width + offset_west, h, 
		     offset_east, c->height); 

   dbg("%s() painting south %i+%i - %ix%i \n", __func__, 
       		     0, c->height + h, 
		     c->width + offset_east + offset_west, offset_south);

   theme_frame_paint(c->wm->mbtheme, c, FRAME_MAIN_SOUTH, 
		     0, c->height + h, 
		     c->width + offset_east + offset_west, offset_south); 

   if (!(c->flags & CLIENT_IS_DESKTOP_FLAG))
     theme_frame_button_paint(c->wm->mbtheme, c, BUTTON_ACTION_CLOSE, 
			      INACTIVE, FRAME_MAIN, w, h);

   theme_frame_button_paint(c->wm->mbtheme, c, BUTTON_ACTION_HIDE, 
			    INACTIVE, FRAME_MAIN, w, h);

   if (!(c->wm->flags & SINGLE_FLAG))
   {
      dbg("%s() painting next / prev buttons\n", __func__);
      theme_frame_button_paint(c->wm->mbtheme, c, BUTTON_ACTION_MENU, 
			       INACTIVE, FRAME_MAIN, w, h);
      theme_frame_button_paint(c->wm->mbtheme, c, BUTTON_ACTION_NEXT, 
			       INACTIVE, FRAME_MAIN, w, h);
      theme_frame_button_paint(c->wm->mbtheme, c, BUTTON_ACTION_PREV, 
			       INACTIVE, FRAME_MAIN, w, h);
   } else {
     client_button_remove(c, BUTTON_ACTION_NEXT);
     client_button_remove(c, BUTTON_ACTION_PREV);

     if (!(c->wm->flags & DESKTOP_DECOR_FLAG)
	   && wm_get_desktop(c->wm)) /* Paint the dropdown for the desktop */
       {
	 dbg("%s() have desktop\n", __func__);
	 theme_frame_button_paint(c->wm->mbtheme, c, BUTTON_ACTION_MENU, 
				  INACTIVE, FRAME_MAIN, w, h);

	 theme_frame_button_paint(c->wm->mbtheme, c, BUTTON_ACTION_DESKTOP, 
				  INACTIVE, FRAME_MAIN, w, h);
       }
     else 
       {
	 dbg("%s() removing menu button\n", __func__ );
	 client_button_remove(c, BUTTON_ACTION_MENU);
       }
   }

   if (c->flags & CLIENT_ACCEPT_BUTTON_FLAG)
      theme_frame_button_paint(c->wm->mbtheme, c, BUTTON_ACTION_ACCEPT, 
			       INACTIVE, FRAME_MAIN, w, h);

   if (c->flags & CLIENT_HELP_BUTTON_FLAG)
     {
       dbg("%s() painting help button\n", __func__);
       theme_frame_button_paint(c->wm->mbtheme, c, BUTTON_ACTION_HELP, 
				INACTIVE, FRAME_MAIN, w, h);
     }

   if (c->flags & CLIENT_CUSTOM_BUTTON_FLAG)
     {
       dbg("%s() painting help button\n", __func__);
       theme_frame_button_paint(c->wm->mbtheme, c, BUTTON_ACTION_CUSTOM, 
				INACTIVE, FRAME_MAIN, w, h);
     }

  if (is_shaped)   /* XXX do we really need titleframe here ? */
    {
      XRectangle rects[1];

      rects[0].x = 0;
      rects[0].y = h;
      rects[0].width = w;
      rects[0].height = c->height;

      XShapeCombineRectangles ( c->wm->dpy, c->title_frame, 
				ShapeBounding,
				0, 0, rects, 1, ShapeSet, 0 );

      XShapeCombineMask( c->wm->dpy, c->title_frame, 
			 ShapeBounding, 0, 0, 
			 c->backing_masks[MSK_NORTH], ShapeUnion);

      XShapeCombineMask( c->wm->dpy, c->title_frame, 
			 ShapeBounding, 0, c->height + h, 
			 c->backing_masks[MSK_SOUTH], ShapeUnion);

      XShapeCombineShape ( c->wm->dpy, 
			   c->frame,
			   ShapeBounding, 0, 0, 
			   c->title_frame,
			   ShapeBounding, ShapeSet);
    }

#ifdef STANDALONE
   XSetWindowBackgroundPixmap(c->wm->dpy, c->title_frame, c->backing);
#else
   XSetWindowBackgroundPixmap(c->wm->dpy, c->title_frame, 
			      mb_drawable_pixmap(c->backing));
#endif

   XClearWindow(c->wm->dpy, c->title_frame);

#ifdef STANDALONE
   XFreePixmap(c->wm->dpy, c->backing);
   c->backing = None;
#else
   mb_drawable_unref(c->backing);
   c->backing = NULL;
#endif

   c->have_set_bg = True;

}

void main_client_button_press(Client *c, XButtonEvent *e)
{
  int ch = 0;

   if (!c->wm->config->use_title) return;

   if (c->wm->flags & TITLE_HIDDEN_FLAG)
   {
      main_client_toggle_title_bar(c);
      XMapWindow(c->wm->dpy, c->title_frame);
      XMapSubwindows(c->wm->dpy, c->title_frame);
      return;
   }

   ch = theme_frame_defined_height_get(c->wm->mbtheme, FRAME_MAIN);

   switch (client_button_do_ops(c, e, FRAME_MAIN, c->width, ch))
     {
      case BUTTON_ACTION_DESKTOP:
	 wm_toggle_desktop(c->wm);
	 break;
      case BUTTON_ACTION_CLOSE:
	 client_deliver_delete(c);
	 break;
      case BUTTON_ACTION_NEXT:
	base_client_hide_transients(c->wm->main_client);
	wm_activate_client(client_get_next(c, mainwin));
	 break;
      case BUTTON_ACTION_PREV:
	base_client_hide_transients(c->wm->main_client);
	wm_activate_client(client_get_prev(c, mainwin));
	 break;
      case BUTTON_ACTION_MENU:
	 select_client_new(c->wm);
	 break;
      case BUTTON_ACTION_HIDE:
	 main_client_toggle_title_bar(c);
	 break;
      case BUTTON_ACTION_HELP:
	client_deliver_wm_protocol(c, c->wm->atoms[_NET_WM_CONTEXT_HELP]);
	 break;
      case BUTTON_ACTION_ACCEPT:
	client_deliver_wm_protocol(c, c->wm->atoms[_NET_WM_CONTEXT_ACCEPT]);
	 break;
      case BUTTON_ACTION_CUSTOM:
	client_deliver_wm_protocol(c, c->wm->atoms[_NET_WM_CONTEXT_CUSTOM]);
	 break;

      case -1: 		 /* Cancelled  */
	 break;
      case 0:
	 /* Not on button */
	 break;
   }
}

void
main_client_toggle_title_bar(Client *c)
{
   Client *p;
   int prev_height = main_client_title_height(c);
   int y_offset = wm_get_offsets_size(c->wm, NORTH, NULL, False);

   c->wm->flags ^= TITLE_HIDDEN_FLAG;

   dbg("%s() called\n", __func__);
   
   XGrabServer(c->wm->dpy);

   theme_img_cache_clear( c->wm->mbtheme,  FRAME_MAIN );

   START_CLIENT_LOOP(c->wm,p);
   if (p->type == mainwin)
   {
      if (c->wm->flags & TITLE_HIDDEN_FLAG)
      {  /* hide */
	 p->height += (prev_height - TITLE_HIDDEN_SZ );
	 p->y = y_offset + TITLE_HIDDEN_SZ;

	 if (c->wm->have_titlebar_panel 
	     && mbtheme_has_titlebar_panel(c->wm->mbtheme))
	   {
	     c->wm->have_titlebar_panel->ignore_unmap++;
	     XUnmapWindow(c->wm->dpy, c->wm->have_titlebar_panel->frame);
	   }

      } else {
	 /* show */
	 p->y = main_client_title_height(p) + y_offset;
	 p->height -= ( main_client_title_height(p) - TITLE_HIDDEN_SZ );
	 XMapWindow(c->wm->dpy, p->title_frame); /* prev will have unmapped */

	 if (c->wm->have_titlebar_panel
	     && mbtheme_has_titlebar_panel(c->wm->mbtheme))
	   XMapRaised(c->wm->dpy, c->wm->have_titlebar_panel->frame);

      }
      p->move_resize(p);
      p->redraw(p, False);
   }
   END_CLIENT_LOOP(c->wm,p);
   
   XUngrabServer(c->wm->dpy);
}

void
main_client_hide(Client *c)
{
   base_client_hide(c);

   /* lower window to bottom of stack */
   XLowerWindow(c->wm->dpy, c->frame);
}

void
main_client_iconize(Client *c)
{
  Client *p;
  base_client_iconize(c);
  p = client_get_prev(c, mainwin);
  if (p) { p->show(p); }

}


void
main_client_show(Client *c)
{
  Client *desktop = NULL;
   dbg("%s() called on %s\n", __func__, c->name);
   
   /* client_set_state(c, NormalState); XXX moved to wm_client_new */
   
   if (c->wm->flags & DESKTOP_RAISED_FLAG) 
     {
       c->wm->flags ^= DESKTOP_RAISED_FLAG; /* desktop not raised anymore */
       c->flags |= CLIENT_NEW_FOR_DESKTOP;
     }
   else
     {
       c->flags &= ~CLIENT_NEW_FOR_DESKTOP;
     }


   c->wm->main_client = c;
   c->mapped = True;

   /* Make sure the desktop is always at the bottom */
   if (!(c->wm->flags & DESKTOP_DECOR_FLAG)
       && (desktop = wm_get_desktop(c->wm)) != NULL)
     XLowerWindow(c->wm->dpy, desktop->window);


   XMapRaised(c->wm->dpy, c->frame);
   XMapSubwindows(c->wm->dpy, c->frame);

   if (c->wm->have_titlebar_panel 
       && mbtheme_has_titlebar_panel(c->wm->mbtheme)
       && !(c->flags & CLIENT_FULLSCREEN_FLAG))
     XMapRaised(c->wm->dpy, c->wm->have_titlebar_panel->frame);

   /* check input focus */
   if (client_want_focus(c))
   {
      XSetInputFocus(c->wm->dpy, c->window,
		     RevertToPointerRoot, CurrentTime);
      c->wm->focused_client = c;
   }

   /* deal with transients etc */
   base_client_show(c);
}

void
main_client_destroy(Client *c)
{
   Wm *w = c->wm;

   dbg("%s called for %s\n", __func__, c->name);
  
   /* What main clients are left?, need to decide what nav buttons appear */

   if (c == w->main_client)
   {
      w->main_client = client_get_prev(c, mainwin);
      
      if(w->main_client == c)  /* Is this the only main client left? */
      {
	 w->main_client = NULL;
	 if (c->wm->flags & SINGLE_FLAG)
	    c->wm->flags ^= SINGLE_FLAG; /* turn off single flag */
	 base_client_destroy(c);

	 /* Check for a desktop win and update  */
	 if (!(w->flags & DESKTOP_RAISED_FLAG))
	   {
	     wm_toggle_desktop(w);
	   }
	 else wm_activate_client(w->head_client);

	 /* Hide a dock in titlebar of exists, should call hide() ? */
	 if (w->have_titlebar_panel)
	   {
	     dbg("%s() unmapping panel\n", __func__);
	     XUnmapWindow(w->dpy, w->have_titlebar_panel->frame); 
	   }
	 return;
      }
      /* If we came from the desktop, make sure we go back there */
      if (c->flags & CLIENT_NEW_FOR_DESKTOP)
	{
	  base_client_destroy(c); 
	  wm_toggle_desktop(w);
	  return;
	}
   }

   base_client_destroy(c); 

   wm_activate_client(w->main_client);   

   if (w->main_client == client_get_prev(w->main_client, mainwin))
     {  /* Is only 1 main_client left  */
       w->flags ^= SINGLE_FLAG; /* turn on single flag */      
       main_client_redraw(w->main_client, False);
     }

}




