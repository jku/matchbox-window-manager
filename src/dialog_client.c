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


#include "dialog_client.h"

static void dialog_client_check_for_state_hints(Client *c);
static void dialog_client_drag(Client *c);
static void _get_mouse_position(Wm *w, int *x, int *y);
static void _draw_outline(Client *c, int x, int y, int w, int h);

Client*
dialog_client_new(Wm *w, Window win, Client *trans)
{

   Client *c = base_client_new(w, win); 

   if (!c) return NULL;

   c->type = dialog;
   
   c->reparent     = &dialog_client_reparent;
   c->move_resize  = &dialog_client_move_resize;
   c->hide         = &dialog_client_hide;
   
   c->configure    = &dialog_client_configure;
   c->button_press = &dialog_client_button_press;
   c->redraw       = &dialog_client_redraw;
   c->show         = &dialog_client_show;
   c->destroy      = &dialog_client_destroy;
   c->get_coverage = &dialog_client_get_coverage;
   c->trans = trans;

   dialog_client_check_for_state_hints(c);

   return c;
}

static void
dialog_client_get_offsets(Client *c, int *e, int *s, int *w)
{
#ifdef USE_MSG_WIN
  if (c->flags & CLIENT_IS_MESSAGE_DIALOG)
    {
      *s = theme_frame_defined_height_get(c->wm->mbtheme, 
				       FRAME_MSG_SOUTH);
      *e = theme_frame_defined_width_get(c->wm->mbtheme, 
					 FRAME_MSG_EAST );
      *w = theme_frame_defined_width_get(c->wm->mbtheme, 
					 FRAME_MSG_WEST );
      return;
    }
#endif 
   *s = theme_frame_defined_height_get(c->wm->mbtheme, 
				       FRAME_DIALOG_SOUTH);
   *e = theme_frame_defined_width_get(c->wm->mbtheme, 
				      FRAME_DIALOG_EAST );
   *w = theme_frame_defined_width_get(c->wm->mbtheme, 
				      FRAME_DIALOG_WEST );
}

static void
dialog_client_check_for_state_hints(Client *c)
{
  if (ewmh_state_check(c, c->wm->atoms[WINDOW_STATE_MODAL]))
    {
      Client *damaged;

      dbg("%s() got modal hint, setting flag\n", __func__);
      c->flags ^= CLIENT_IS_MODAL_FLAG;

      /* Call comp_engine_client_show to add damage to main window 
       * so it gets fully lowlighted ok. 
       *
       */
      if ((damaged = wm_get_visible_main_client(c->wm)) != NULL)
	{
	  comp_engine_client_show(c->wm, damaged);
	}
    }
}


void
dialog_client_get_coverage(Client *c, int *x, int *y, int *w, int *h)
{
   int frm_size = dialog_client_title_height(c);
   int east,south,west;

   dialog_client_get_offsets(c, &east, &south, &west);

   *x = c->x - west;
   *y = c->y - frm_size;
   *w = c->width + east + west;
   *h = c->height + frm_size + south;
}

void
dialog_client_move_resize(Client *c)
{
   int frm_size     = dialog_client_title_height(c);
   int offset_south = 0, offset_west = 0, offset_east = 0;

   dialog_client_get_offsets(c, &offset_east, &offset_south, &offset_west);

   base_client_move_resize(c);

   XMoveResizeWindow(c->wm->dpy, c->window, 
		     offset_west,
		     frm_size,
		     c->width, c->height);

#ifndef USE_COMPOSITE
   if (c->wm->config->dialog_shade && (c->flags & CLIENT_IS_MODAL_FLAG))
     {
       XMoveResizeWindow(c->wm->dpy, 
			 c->title_frame, 
			 c->x - offset_west, 
			 c->y - frm_size, 
			 c->width + offset_east + offset_west,
			 c->height + frm_size + offset_south );
       XMoveWindow(c->wm->dpy, 
		   c->window, 
		   c->x, 
		   c->y);

     }
   else
#endif
     {
       XMoveResizeWindow(c->wm->dpy, 
			 c->frame, 
			 c->x - offset_west, 
			 c->y - frm_size, 
			 c->width + offset_west + offset_east,
			 c->height + frm_size + offset_south
			 );
       XResizeWindow(c->wm->dpy, 
			 c->title_frame, 
			 c->width + offset_east + offset_west,
			 c->height + frm_size + offset_south );
     }
}

void
dialog_client_hide(Client *c)
{
  Client *damaged;
  dbg("%s() called for %s\n", __func__, c->name);
  XLowerWindow(c->wm->dpy, c->frame);

  if (c->flags & CLIENT_IS_MODAL_FLAG 
      && (damaged = wm_get_visible_main_client(c->wm)) != NULL)
    {
      comp_engine_client_show(c->wm, damaged);
    }

  comp_engine_client_hide(c->wm, c);
}

int
dialog_client_title_height(Client *c)
{
   if (c->flags & CLIENT_TITLE_HIDDEN_FLAG)
      return 0;

#ifdef USE_MSG_WIN
  if (c->flags & CLIENT_IS_MESSAGE_DIALOG)
    {
      return theme_frame_defined_height_get(c->wm->mbtheme, FRAME_MSG);
    }
#endif

  if (c->flags & CLIENT_BORDERS_ONLY_FLAG
      && theme_has_frame_type_defined(c->wm->mbtheme, FRAME_DIALOG_NORTH))    
    return theme_frame_defined_height_get(c->wm->mbtheme, FRAME_DIALOG_NORTH);

   return theme_frame_defined_height_get(c->wm->mbtheme, FRAME_DIALOG);
}

void
dialog_client_show(Client *c)
{
  dbg("%s() called for %s\n", __func__, c->name);

  /* XXX paint before map so dialog gets shape 
         can probably be more optimised / experimental at the moment. 
  */
  if (client_get_state(c) != NormalState)
    dialog_client_redraw(c, False);

  XFlush(c->wm->dpy);

  client_set_state(c, NormalState); 
  XMapSubwindows(c->wm->dpy, c->frame);
  XMapRaised(c->wm->dpy, c->frame);


  if (client_want_focus(c) && (!(c->flags & CLIENT_IS_MESSAGE_DIALOG)))
    {
      XSetInputFocus(c->wm->dpy, c->window,
		     RevertToPointerRoot, CurrentTime);
      c->wm->focused_client = c;
    }

#ifdef USE_MSG_WIN 		/* Ewe! mesg window hacks :/  */
  if (c->wm->config->dialog_shade 
      && (c->flags & CLIENT_IS_MODAL_FLAG)
      && c->wm->msg_win_queue_head)
    {
        Client *msg_client = NULL;
	if ((msg_client = wm_find_client(c->wm, 
					 c->wm->msg_win_queue_head->win, 
					 WINDOW)) != NULL)
	  msg_client->show(msg_client);
    }
#endif

  comp_engine_client_show(c->wm, c);

  c->mapped = True;
}


void
dialog_client_reparent(Client *c)
{
   XSetWindowAttributes attr;

   int offset_north = dialog_client_title_height(c);
   int offset_south = 0, offset_west = 0, offset_east = 0;

   dialog_client_get_offsets(c, &offset_east, &offset_south, &offset_west);

   attr.override_redirect = True; 
   attr.background_pixel  = BlackPixel(c->wm->dpy, c->wm->screen);
   attr.event_mask        = ChildMask|ButtonPressMask|ExposureMask;

   dbg("%s() want lowlight : wm:%i , client:%i\n", __func__,
       c->wm->config->dialog_shade, (c->flags & CLIENT_IS_MODAL_FLAG));
#ifndef USE_COMPOSITE
   if (c->wm->config->dialog_shade && (c->flags & CLIENT_IS_MODAL_FLAG))
     {
       dbg("%s() LOWLIGHTING\n", __func__);
       wm_lowlight(c->wm, c);
     }
   else
#endif
     {
       if (c->flags & CLIENT_TITLE_HIDDEN_FLAG) 
	 {
	   c->frame = c->window;
	 }
       else c->frame = XCreateWindow(c->wm->dpy, 
				     c->wm->root, 
				     0, 0,
				     c->width + offset_east + offset_west, 
				     c->height + offset_north + offset_south, 
				     0,
				     CopyFromParent, 
				     CopyFromParent, 
				     CopyFromParent,
				     CWOverrideRedirect|CWEventMask|CWBackPixel,
				     &attr);
     }


   attr.background_pixel = BlackPixel(c->wm->dpy, c->wm->screen);

   if (c->flags & CLIENT_TITLE_HIDDEN_FLAG)
     {
       c->title_frame = c->window;
     }
   else
     {
       c->title_frame =
	 XCreateWindow(c->wm->dpy, 
		       c->frame, 
		       0, 0, 
		       c->width + offset_east + offset_west, 
		       c->height + offset_north + offset_south, 
		       0,
		       CopyFromParent, 
		       CopyFromParent, 
		       CopyFromParent,
		       CWOverrideRedirect|CWBackPixel|CWEventMask, 
		       &attr);

     }
   
   XSetWindowBorderWidth(c->wm->dpy, c->window, 0);

   XAddToSaveSet(c->wm->dpy, c->window);

   XSelectInput(c->wm->dpy, c->window,
		ButtonPressMask|ColormapChangeMask|PropertyChangeMask);

   if (c->frame != c->window)
     XReparentWindow(c->wm->dpy, c->window, c->frame, 
		     offset_west, offset_north);
}


void
dialog_client_configure(Client *c)
{

#define MAX_PADDING 8 		/*Used to make sure there is always some free
				  border space around a dialog */

  int side_frame_width = 0, bottom_frame_width = 0;
  int max_w = 0, max_h = 0, toolbar_y = 0; 
  int offset_south = 0, offset_west = 0, offset_east = 0, offset_north = 0;

  /* Check if we actually want to perform any sizing intervention */
  if (c->wm->config->dialog_stratergy == WM_DIALOGS_STRATERGY_FREE
      || (c->flags & CLIENT_IS_MESSAGE_DIALOG))
    return;

  /* Figure out window border offsets */
  dialog_client_get_offsets(c, &offset_east, &offset_south, &offset_west);
  offset_north = dialog_client_title_height(c);

  /* Check window has decoration */
  if (!(c->flags & CLIENT_TITLE_HIDDEN_FLAG))
    {
      side_frame_width = offset_east + offset_west;
      bottom_frame_width = offset_south;
    }

  /* Figure out where the toolbar/dock is.
     - Toolbars dont effect messages and splash windows */
  if ((c->flags & CLIENT_TITLE_HIDDEN_FLAG) 
      || (c->flags & CLIENT_IS_MESSAGE_DIALOG))
    toolbar_y = c->wm->dpy_height; 
  else
    toolbar_y = c->wm->dpy_height 
      - wm_get_offsets_size(c->wm, SOUTH, NULL,True);

  /* Calculate maximum allowed width of dialog */
  max_w = c->wm->dpy_width - side_frame_width - MAX_PADDING;

  /* Figure out max dialog size */
  max_h = toolbar_y - bottom_frame_width - offset_north - MAX_PADDING;

  dbg("dialog wants %i x %i, %i x %i\n", c->x, c->y, c->width, c->height);

  /* Resize vertical + move out of the way of any toolbars 
     ( Toolbars assumed to be input methods + we dont wanna cover dock ) 
  */
  if (c->height > max_h) 
    {
      c->height = max_h;
      c->flags |=  CLIENT_SHRUNK_FOR_TB_FLAG;
      c->y = offset_north + ( MAX_PADDING/2 );
      dbg("%s() c->height > max_h . c->y is now %i\n", __func__, c->y);
    }
  else
    {
      /* move dialog up above any toolbars  */
      if ( ( c->y + c->height + bottom_frame_width ) > toolbar_y)
	{
	  c->y -= (( c->y + c->height + bottom_frame_width ) - toolbar_y );

	  dbg("%s() overlapping toolbar . c->y is now %i\n", __func__, c->y);

	  /* check c->y is now not off screen */
	  if ((c->y - offset_north) <= 0)
	    {
	      c->height += c->y;
	      c->y = offset_north + ( MAX_PADDING/2 );
	    }
	}
    }

  /* Now horiz constraints */

  /* horiz contarined mode - force dialog to be full width*/
  if (c->wm->config->dialog_stratergy == WM_DIALOGS_STRATERGY_CONSTRAINED_HORIZ
      && !(c->flags & CLIENT_TITLE_HIDDEN_FLAG) )
    {
      c->x = 0 - offset_east;
      c->width = c->wm->dpy_width - side_frame_width;
      
      return;
    }

  /* dont try and fix decoration free dialogs 
  if (c->flags & CLIENT_TITLE_HIDDEN_FLAG)
    {
      dbg("%s() title hidden, repositioning\n", __func__);
      if ((c->x + c->width) > c->wm->dpy_width)
	c->x = c->wm->dpy_width - c->width;

      if (c->x < 0)
	c->x = 0;

      return;
    }
  */

  /* Clip horizonal width */
  if (c->width > max_w) c->width = max_w;
   
  /* Finally reposition dialog ( centered ) if ;
      + positioned at 0,0
      + positioned offscreen
    */
  if ( (c->x - offset_west) <= 0 
       || (c->x + c->width + side_frame_width) > c->wm->dpy_width)
    {
      dbg("%s() centering x pos\n", __func__);
      c->x = (c->wm->dpy_width  - (c->width + side_frame_width))/2 
	+ offset_west;
    }

  if ( (c->y - offset_north) <= 0 
       || (c->y + c->height + offset_south) > c->wm->dpy_height)
    {
      dbg("%s() centering y pos\n", __func__);
      c->y = (c->wm->dpy_height - (c->height + offset_south))/2;
    }

}

void
dialog_client_redraw(Client *c, Bool use_cache)
{
  Bool is_shaped = False;

  int offset_north = dialog_client_title_height(c);
  int offset_south = 0, offset_west = 0, offset_east = 0;
  int total_w = 0, total_h = 0;

  int frame_ref_top   = FRAME_DIALOG;
  int frame_ref_east  = FRAME_DIALOG_EAST;
  int frame_ref_west  = FRAME_DIALOG_WEST;
  int frame_ref_south = FRAME_DIALOG_SOUTH;

  dialog_client_get_offsets(c, &offset_east, &offset_south, &offset_west);

  total_w = offset_east  + offset_west + c->width;
  total_h = offset_north + offset_south + c->height;


  if (c->flags & CLIENT_BORDERS_ONLY_FLAG 
      && theme_has_frame_type_defined(c->wm->mbtheme, FRAME_DIALOG_NORTH))
    frame_ref_top   = FRAME_DIALOG_NORTH;

  /* 'message dialogs have there own decorations */
  if (c->flags & CLIENT_IS_MESSAGE_DIALOG)
    {
      frame_ref_top   = FRAME_MSG;
      frame_ref_east  = FRAME_MSG_EAST;
      frame_ref_west  = FRAME_MSG_WEST;
      frame_ref_south = FRAME_MSG_SOUTH;
    }


  dbg("%s() c->width : %i , offset_east : %i, offset_west : %i\n",
      __func__, c->width, offset_east, offset_west );

  if (c->flags & CLIENT_TITLE_HIDDEN_FLAG) return;

   if (use_cache && c->backing != None) return;

  is_shaped = theme_frame_wants_shaped_window( c->wm->mbtheme, frame_ref_top);
  
  if (c->backing == (Pixmap)NULL)
    client_init_backing(c, total_w, total_h);

  if (is_shaped) client_init_backing_mask(c, total_w, c->height, 
					  offset_north, offset_south,
					  offset_east, offset_west);

#ifdef STANDALONE
  /* Should prevent some flicker */
  XSetForeground(c->wm->dpy, c->wm->mbtheme->gc, 
		 BlackPixel(c->wm->dpy, c->wm->screen));
  XFillRectangle(c->wm->dpy, c->backing, c->wm->mbtheme->gc, 
		 0, 0, total_w, total_h);
#endif

  theme_frame_paint(c->wm->mbtheme, c, frame_ref_top, 
		    0, 0, total_w, offset_north); 
    
  theme_frame_paint(c->wm->mbtheme, c, frame_ref_west, 
		    0, offset_north, offset_west, c->height); 
  
  theme_frame_paint(c->wm->mbtheme, c, frame_ref_east, 
		    total_w - offset_east, offset_north, 
		    offset_east, c->height); 

  theme_frame_paint(c->wm->mbtheme, c, frame_ref_south, 
		    0, total_h - offset_south, 
		    total_w, offset_south); 

  /* We dont paint buttons for borderonly and message dialogs */
  if (!(c->flags & CLIENT_BORDERS_ONLY_FLAG
	|| c->flags & CLIENT_IS_MESSAGE_DIALOG))
    {
      theme_frame_button_paint(c->wm->mbtheme, c, 
			       BUTTON_ACTION_CLOSE, 
			       INACTIVE, FRAME_DIALOG, 
			       total_w, offset_north);


      if (c->flags & CLIENT_ACCEPT_BUTTON_FLAG)
	theme_frame_button_paint(c->wm->mbtheme, c, BUTTON_ACTION_ACCEPT, 
				 INACTIVE, FRAME_DIALOG,total_w, offset_north);

      if (c->flags & CLIENT_HELP_BUTTON_FLAG)
	theme_frame_button_paint(c->wm->mbtheme, c, BUTTON_ACTION_HELP, 
				 INACTIVE, FRAME_DIALOG,total_w, offset_north);
    }

  /* XXXX ifdef HAVE_SHAPE */
  if (is_shaped)
    {
      XRectangle rects[1];

      rects[0].x = offset_west;  
      rects[0].y = offset_north;
      rects[0].width = total_w - offset_west - offset_east;
      rects[0].height = total_h - offset_south - offset_north;

      XShapeCombineRectangles ( c->wm->dpy, c->title_frame, 
				ShapeBounding,
				0, 0, rects, 1, ShapeSet, 0 );

#ifndef USE_COMPOSITE
      if (c->wm->config->dialog_shade && (c->flags & CLIENT_IS_MODAL_FLAG)) 
	{
	  XShapeCombineMask( c->wm->dpy, c->title_frame, 
			     ShapeBounding, 0, 0, 
			     c->backing_masks[MSK_NORTH], ShapeUnion);
	  XShapeCombineMask( c->wm->dpy, c->title_frame, 
			     ShapeBounding, 0, total_h - offset_south, 
			     c->backing_masks[MSK_SOUTH], ShapeUnion);

	  XShapeCombineMask( c->wm->dpy, c->title_frame, 
			     ShapeBounding, 0, offset_north, 
			     c->backing_masks[MSK_WEST], ShapeUnion);
	  XShapeCombineMask( c->wm->dpy, c->title_frame, 
			     ShapeBounding, 
			     total_w - offset_east, offset_north,
			     c->backing_masks[MSK_EAST], ShapeUnion);
	}
      else
#endif
	{
	  XShapeCombineMask( c->wm->dpy, c->title_frame, 
			     ShapeBounding, 0, 0, 
			     c->backing_masks[MSK_NORTH], ShapeUnion);

	  XShapeCombineMask( c->wm->dpy, c->title_frame, 
			     ShapeBounding, 0, total_h - offset_south, 
			     c->backing_masks[MSK_SOUTH], ShapeUnion);

	  XShapeCombineMask( c->wm->dpy, c->title_frame, 
			     ShapeBounding, 0, offset_north, 
			     c->backing_masks[MSK_WEST], ShapeUnion);
	  XShapeCombineMask( c->wm->dpy, c->title_frame, 
			     ShapeBounding, 
			     total_w - offset_east, offset_north,
			     c->backing_masks[MSK_EAST], ShapeUnion);

	  XShapeCombineShape ( c->wm->dpy, 
			       c->frame,
			       ShapeBounding, 0, 0, 
			       c->title_frame,
			       ShapeBounding, ShapeSet);

	}
    }

#ifdef STANDALONE
   XSetWindowBackgroundPixmap(c->wm->dpy, c->title_frame, c->backing);
#else
   XSetWindowBackgroundPixmap(c->wm->dpy, c->title_frame, 
			      mb_drawable_pixmap(c->backing));
#endif

   XClearWindow(c->wm->dpy, c->title_frame);

   XFlush(c->wm->dpy);
}

void
dialog_client_button_press(Client *c, XButtonEvent *e)
{
  int offset_north = dialog_client_title_height(c);
  int offset_south = 0, offset_west = 0, offset_east = 0;

  if (c->flags & CLIENT_IS_MESSAGE_DIALOG) return;

  dialog_client_get_offsets(c, &offset_east, &offset_south, &offset_west);

  dbg("%s() c->width : %i , offset_east : %i, offset_west : %i\n",
      __func__, c->width, offset_east, offset_west );

   switch (client_button_do_ops(c, e, FRAME_DIALOG, 
				c->width + offset_east + offset_west, 
				offset_north))
   {
      case BUTTON_ACTION_CLOSE:
	 client_deliver_delete(c);
	 break;
      case BUTTON_ACTION_HELP:
	client_deliver_wm_protocol(c, c->wm->atoms[_NET_WM_CONTEXT_HELP]);
	 break;
      case BUTTON_ACTION_ACCEPT:
	client_deliver_wm_protocol(c, c->wm->atoms[_NET_WM_CONTEXT_ACCEPT]);
      case -1: 		 /* Cancelled  */
	 break;
      case 0:
	 dialog_client_drag(c);     /* Not on button */
	 break;
   }
}

static void
dialog_client_drag(Client *c) /* drag box */
{
  XEvent ev;
  int x1, y1;
  int old_cx = c->x;
  int old_cy = c->y;
    
  int frm_size     = dialog_client_title_height(c);
  int offset_south = 0, offset_west = 0, offset_east = 0;

#ifdef USE_MSG_WIN
  Client *t = NULL;
#endif 

  dialog_client_get_offsets(c, &offset_east, &offset_south, &offset_west);

  if (XGrabPointer(c->wm->dpy, c->wm->root, False,
		   (ButtonPressMask|ButtonReleaseMask|PointerMotionMask),
		   GrabModeAsync,
		   GrabModeAsync, None, c->wm->curs_drag, CurrentTime)
      != GrabSuccess)
    return;
    
  XGrabServer(c->wm->dpy);

  /* Let the comp know theres gonna be damage in out old position.
   * XXX Must be a better way need to figure it out.    
   */
  comp_engine_client_show(c->wm, c); 

  c->flags |= CLIENT_IS_MOVING;

  _get_mouse_position(c->wm, &x1, &y1);

  _draw_outline(c, c->x - offset_west, c->y - frm_size,
		c->width + offset_west + offset_east,
		c->height + frm_size + offset_south);
    
  for (;;) 
    {

    XMaskEvent(c->wm->dpy, ButtonPressMask|ButtonReleaseMask|PointerMotionMask,
	       &ev);

    switch (ev.type) 
      {
      case MotionNotify:
	_draw_outline(c, c->x - offset_west, c->y - frm_size,
		      c->width + offset_west + offset_east,
		      c->height + frm_size + offset_south);
	if (c->wm->config->dialog_stratergy != WM_DIALOGS_STRATERGY_CONSTRAINED_HORIZ)
	  c->x = old_cx + (ev.xmotion.x - x1);
	c->y = old_cy + (ev.xmotion.y - y1);

	_draw_outline(c, c->x - offset_west, c->y - frm_size,
		      c->width + offset_west + offset_east,
		      c->height + frm_size + offset_south);
	break;
	
      case ButtonRelease:
	dbg("drag, got release");
	_draw_outline(c, c->x - offset_west, c->y - frm_size,
		      c->width + offset_west + offset_east,
		      c->height + frm_size + offset_south);

#ifndef USE_COMPOSITE	
	if (c->wm->config->dialog_shade && (c->flags & CLIENT_IS_MODAL_FLAG))
	  {
	    XMoveResizeWindow(c->wm->dpy, 
			      c->title_frame, 
			      c->x - offset_west, 
			      c->y - frm_size, 
			      c->width + offset_east + offset_west,
			      c->height + frm_size + offset_south);
	    XMoveResizeWindow(c->wm->dpy, 
			      c->window, 
			      c->x, 
			      c->y, 
			      c->width,
			      c->height);
	  } 
	else
#endif 
	  {
	    XMoveWindow(c->wm->dpy, c->frame, c->x - offset_west,
			c->y - dialog_client_title_height(c));
	  }
	
	c->show(c);
	
#ifdef USE_MSG_WIN
	/* Check for message super above dialogs */
	START_CLIENT_LOOP(c->wm, t)
	  {
	    if (t->flags & CLIENT_IS_MESSAGE_DIALOG)
	      {
		t->show(t);
		break;
	      }
	  }
	END_CLIENT_LOOP(c->wm, t);
#endif 
	c->flags &= ~ CLIENT_IS_MOVING;
	
	XUngrabPointer(c->wm->dpy, CurrentTime);
	XUngrabServer(c->wm->dpy);
	return;
      }

    }
  client_deliver_config(c);
}

static void
_get_mouse_position(Wm *w, int *x, int *y)
{
    Window mouse_root, mouse_win;
    int win_x, win_y;
    unsigned int mask;

    XQueryPointer(w->dpy, w->root, &mouse_root, &mouse_win,
        x, y, &win_x, &win_y, &mask);
}

static void
_draw_outline(Client *c, int x, int y, int width, int height)
{
  XDrawRectangle(c->wm->dpy, c->wm->root, c->wm->mbtheme->band_gc, x, y, 
		 width, height);
}
 
void dialog_client_destroy(Client *c)
{
  Wm     *w = c->wm; 
  Client *d = NULL;
#ifdef USE_MSG_WIN
  int was_msg = (c->flags & CLIENT_IS_MESSAGE_DIALOG);
#endif 

  base_client_destroy(c);

  /* We could be a dialog over a desktop, in which case we
     need to give it back keyboard focus - this is a bit
     ugly. XXX Improve XXX
  */

  if ((d = wm_get_visible_main_client(w)) != NULL)
    {
      if (client_want_focus(d))
	{
	  XSetInputFocus(w->dpy, d->window,
			 RevertToPointerRoot, CurrentTime);
	  w->focused_client = d;
	}

      if (w->flags & DESKTOP_RAISED_FLAG)
	comp_engine_client_show (w, d); 
    }


  /* Old way of above
  if (w->flags & DESKTOP_RAISED_FLAG)
    {
      Client *d = wm_get_desktop(w);

      if (d)
	{
	  if (client_want_focus(d))
	    {
	      XSetInputFocus(w->dpy, d->window,
			     RevertToPointerRoot, CurrentTime);
	      c->wm->focused_client = d;
	    }

	  comp_engine_client_show (w, d);
	}
    }
  */

#ifdef USE_MSG_WIN

   if (was_msg) 
     {
       dbg("%s() was message poping queue\n", __func__);
       wm_msg_win_queue_pop(w);
     }
#endif 

   
}
