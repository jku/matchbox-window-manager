#include "composite-engine.h"

#define DO_TIMINGS 1

#if DO_TIMINGS
#include <sys/time.h>
#include <time.h>
#endif


#ifdef USE_COMPOSITE

#include <math.h>

static void
comp_engine_add_damage (Wm *w, XserverRegion damage);

typedef struct _conv {
    int	    size;
    double  *data;
} conv;


typedef struct StackItem 
{
  Client           *client;
  struct StackItem *next;

} StackItem;

/* XXX Ideally get rid of these globals */

static conv      *gussianMap;
static StackItem *comp_stack; 

/* List for stack rendering of dialogs etc */

#define stack_enumerate(c) for((c) = comp_stack; (c); (c) = (c)->next)

static StackItem*
stack_new(Client *client)
{
  StackItem* list;
  list = malloc(sizeof(StackItem));
  memset(list, 0, sizeof(StackItem));

  list->client = client;
  list->next   = NULL;

  return list;
}

static void
stack_push(Client *client)
{
  StackItem* new = NULL;

  if (comp_stack == NULL) 
    {
      comp_stack = stack_new(client);
      return;
    }

  new          = stack_new(client);
  new->next    = comp_stack;
  comp_stack = new;
}

static StackItem*
stack_remove(Client *client)
{
  StackItem* cur = NULL, *ret = NULL;

  if ((cur = comp_stack) != NULL)
    {
      if (cur->client == client)
	{
	  ret = comp_stack;
	  comp_stack = comp_stack->next;
	  return ret;
	}
      while (cur != NULL)
	{
	  if (cur->next && cur->next->client == client)
	    {
	      ret = cur->next;
	      cur->next = cur->next->next;
	      return ret;
	    }
	  cur = cur->next;
	}
    }
  return NULL;
}

static void
stack_top(Client *client)
{
  StackItem* cur = NULL;  

  if ((cur = stack_remove(client)) != NULL)
    {
      cur->next = comp_stack;
      comp_stack = cur;
    }
}


/* Shadow Generation */

static double
gaussian (double r, double x, double y)
{
    return ((1 / (sqrt (2 * M_PI * r))) *
	    exp ((- (x * x + y * y)) / (2 * r * r)));
}


static conv *
make_gaussian_map (double r)
{
    conv	    *c;
    int		    size = ((int) ceil ((r * 3)) + 1) & ~1;
    int		    center = size / 2;
    int		    x, y;
    double	    t = 0.0;
    double	    g;
    
    c = malloc (sizeof (conv) + size * size * sizeof (double));
    c->size = size;

    dbg("%s() map size is %i\n", __func__, size);

    c->data = (double *) (c + 1);
 
   for (y = 0; y < size; y++)
	for (x = 0; x < size; x++)
	{
	    g = gaussian (r, (double) (x - center), (double) (y - center));
	    t += g;
	    c->data[y * size + x] = g;
	}

    for (y = 0; y < size; y++)
	for (x = 0; x < size; x++)
	    c->data[y*size + x] /= t;

    return c;
}
 
static unsigned char
sum_gaussian (conv *map, double opacity, int x, int y, int width, int height)
{
    int	    fx, fy;
    double  *g_data;
    double  *g_line = map->data;
    int	    g_size = map->size;
    int	    center = g_size / 2;
    int	    fx_start, fx_end;
    int	    fy_start, fy_end;
    double  v;
    
    /*
     * Compute set of filter values which are "in range",
     * that's the set with:
     *	0 <= x + (fx-center) && x + (fx-center) < width &&
     *  0 <= y + (fy-center) && y + (fy-center) < height
     *
     *  0 <= x + (fx - center)	x + fx - center < width
     *  center - x <= fx	fx < width + center - x
     */

    fx_start = center - x;
    if (fx_start < 0)
	fx_start = 0;
    fx_end = width + center - x;
    if (fx_end > g_size)
	fx_end = g_size;

    fy_start = center - y;
    if (fy_start < 0)
	fy_start = 0;
    fy_end = height + center - y;
    if (fy_end > g_size)
	fy_end = g_size;

    g_line = g_line + fy_start * g_size + fx_start;
    
    v = 0;
    for (fy = fy_start; fy < fy_end; fy++)
    {
	g_data = g_line;
	g_line += g_size;
	
	for (fx = fx_start; fx < fx_end; fx++)
	    v += *g_data++;
    }
    if (v > 1)
	v = 1;
    
    return ((unsigned int) (v * opacity * 255.0));
}

#define MAX_TILE_SZ 16 	/* make sure size/2 < MAX_TILE_SZ */
#define WIDTH  320
#define HEIGHT 320

static void
shadow_setup_part (Wm      *w, 
		   XImage **ximage, 
		   Picture *pic, 
		   Pixmap  *pxm,
		   int      width,
		   int      height)
{

   *ximage = XCreateImage (w->dpy, DefaultVisual(w->dpy,DefaultScreen(w->dpy)),
			   8, ZPixmap, 0, 0,
			   width, height, 8, width * sizeof (unsigned char));

   (*ximage)->data = malloc (width * height * sizeof (unsigned char)); 

   *pxm    = XCreatePixmap (w->dpy, w->root, width, height, 8); 
   *pic    = XRenderCreatePicture (w->dpy, *pxm,
				   XRenderFindStandardFormat (w->dpy, 
							      PictStandardA8),
				   0, 0);
}

static void
shadow_finalise_part (Wm      *w,
		      XImage  *ximage, 
		      Picture  pic, 
		      Pixmap   pxm,
		      int      width,
		      int      height)
{
  GC gc = XCreateGC (w->dpy, pxm, 0, 0);
  XPutImage (w->dpy, pxm, gc, ximage, 0, 0, 0, 0, width, height); 
  XDestroyImage (ximage);
  XFreeGC (w->dpy, gc);
  XFreePixmap (w->dpy, pxm);
}

static void
shadow_setup (Wm *w)
{

  XImage	  *ximage;
  Pixmap           pxm;
  unsigned char   *data;
  int		   size;
  int		   center;
  int		   x, y;
  unsigned char    d;
  int              pwidth, pheight;
  double           opacity = SHADOW_OPACITY; 


  if (w->config->shadow_style == SHADOW_STYLE_NONE) return;

  if (w->config->shadow_style == SHADOW_STYLE_SIMPLE)
    {
      w->config->shadow_padding_width  = 0;
      w->config->shadow_padding_height = 0;
      return;
    }

  /* SHADOW_STYLE_GAUSSIAN */

  gussianMap = make_gaussian_map (SHADOW_RADIUS); /* XXX must free */

  w->config->shadow_padding_width  = gussianMap->size;
  w->config->shadow_padding_height = gussianMap->size;

  size   = gussianMap->size;
  center = size / 2;

  dbg("%s() gussian size is %i\n", __func__, size);

  /* Top & bottom */
  
  pwidth  = MAX_TILE_SZ; 
  pheight = size/2;
  shadow_setup_part(w, &ximage, &w->shadow_n_pic, &pxm, pwidth, pheight);

  data = (unsigned char*)ximage->data;
  
  for (y = 0; y < pheight; y++)
    {
      d = sum_gaussian (gussianMap, opacity, center, y - center, WIDTH, HEIGHT);
      for (x = 0; x < pwidth; x++)
	data[y * pwidth + x] = d;
    }
  
  shadow_finalise_part (w, ximage, w->shadow_n_pic, pxm, pwidth, pheight);
  
  pwidth = MAX_TILE_SZ; pheight = MAX_TILE_SZ;
  shadow_setup_part(w, &ximage, &w->shadow_s_pic, &pxm, pwidth, pheight);
  
  data = (unsigned char*)ximage->data;

  for (y = 0; y < pheight; y++)
    {
      d = sum_gaussian (gussianMap, opacity, center, y - center, WIDTH, HEIGHT);
      for (x = 0; x < pwidth; x++)
	data[(pheight - y - 1) * pwidth + x] = d;
    }
  
  shadow_finalise_part (w, ximage, w->shadow_s_pic, pxm, pwidth, pheight);
  
  /* Sides */
  
  pwidth = MAX_TILE_SZ; pheight = MAX_TILE_SZ;
  shadow_setup_part(w, &ximage, &w->shadow_w_pic, &pxm, pwidth, pheight);
  
  data = (unsigned char*)ximage->data;

  for (x = 0; x < pwidth; x++)
    {
      d = sum_gaussian (gussianMap, opacity, x - center, center, WIDTH, HEIGHT);
      for (y = 0; y < pheight; y++)
	data[y * pwidth + (pwidth - x - 1)] = d;
    }
  
  shadow_finalise_part (w, ximage, w->shadow_w_pic, pxm, pwidth, pheight);
  
  shadow_setup_part(w, &ximage, &w->shadow_e_pic, &pxm, pwidth, pheight);
  
  data = (unsigned char*)ximage->data;

  for (x = 0; x < pwidth; x++)
    {
      d = sum_gaussian (gussianMap, opacity, x - center, center, WIDTH, HEIGHT);
      for (y = 0; y < pheight; y++)
	data[y * pwidth + x] = d;
    }
  
  shadow_finalise_part (w, ximage, w->shadow_e_pic, pxm, pwidth, pheight);
  
  
  /* Corners */
  
  pwidth = MAX_TILE_SZ; pheight = MAX_TILE_SZ;
  shadow_setup_part(w, &ximage, &w->shadow_nw_pic, &pxm, pwidth, pheight);
  data = (unsigned char*)ximage->data;

  for (x = 0; x < pwidth; x++)
    for (y = 0; y < pheight; y++)
      {
	d = sum_gaussian (gussianMap, opacity, x - center, y - center, WIDTH, HEIGHT);
	
	data[y * pwidth + x] = d;
      }
  
  shadow_finalise_part (w, ximage, w->shadow_nw_pic, pxm, pwidth, pheight);
  
  shadow_setup_part(w, &ximage, &w->shadow_sw_pic, &pxm, pwidth, pheight);

  data = (unsigned char*)ximage->data;

  for (x = 0; x < pwidth; x++)
    for (y = 0; y < pheight; y++)
      {
	d = sum_gaussian (gussianMap, opacity, x - center, y - center, WIDTH, HEIGHT);
	
	data[(pheight - y - 1) * pwidth + x] = d;
      }
  
  shadow_finalise_part (w, ximage, w->shadow_sw_pic, pxm, pwidth, pheight);
  
  shadow_setup_part(w, &ximage, &w->shadow_se_pic, &pxm, pwidth, pheight);

  data = (unsigned char*)ximage->data;  

  for (x = 0; x < pwidth; x++)
    for (y = 0; y < pheight; y++)
      {
	d = sum_gaussian (gussianMap, opacity, x - center, y - center, WIDTH, HEIGHT);
	
	data[(pheight - y - 1) * pwidth + (pwidth - x -1)] = d;
      }
  
  shadow_finalise_part (w, ximage, w->shadow_se_pic, pxm, pwidth, pheight);
  
  shadow_setup_part(w, &ximage, &w->shadow_ne_pic, &pxm, pwidth, pheight);
  
  data = (unsigned char*)ximage->data;

  for (x = 0; x < pwidth; x++)
    for (y = 0; y < pheight; y++)
      {
	d = sum_gaussian (gussianMap, opacity, x - center, y - center, WIDTH, HEIGHT);
	
	data[y * pwidth + (pwidth - x -1)] = d;
      }
  
  shadow_finalise_part (w, ximage, w->shadow_ne_pic, pxm, pwidth, pheight);
  
  /* Finally center */
  
  pwidth = MAX_TILE_SZ; pheight = MAX_TILE_SZ;
  shadow_setup_part(w, &ximage, &w->shadow_pic, &pxm, pwidth, pheight);

  data = (unsigned char*)ximage->data;  

  d = sum_gaussian (gussianMap, opacity, center, center, WIDTH, HEIGHT);
  
  for (x = 0; x < pwidth; x++)
    for (y = 0; y < pheight; y++)
      data[y * pwidth + x] = d; 
  
  shadow_finalise_part (w, ximage, w->shadow_pic, pxm, pwidth, pheight);

}

static Picture
shadow_gaussian_make_picture (Wm *w, int width, int height)
{
  Picture pic;
  Pixmap  pxm;

  int     pwidth, pheight, x, y, dw, dh;

  pxm  = XCreatePixmap (w->dpy, w->root, width, height, 8); 
  pic = XRenderCreatePicture (w->dpy, pxm,
			      XRenderFindStandardFormat (w->dpy, 
							 PictStandardA8), 0,0);

  pwidth = MAX_TILE_SZ; pheight = MAX_TILE_SZ;

  for (x=0; x < width; x += pwidth)
    for (y=0; y < height; y += pheight)
      {
	if ( (y + pheight) > height )
	  dh = pheight - ((y + pheight)-height);
	else
	  dh = pheight;

	if ( (x + pwidth) > width )
	  dw = pwidth - ((x + pwidth)-width);
	else
	  dw = pwidth;

	XRenderComposite (w->dpy, PictOpSrc,
			  w->shadow_pic, None, pic,
			  0, 0, 0, 0, x, y, dw, dh);
      }
  /* Top & bottom */

  if ( width > (MAX_TILE_SZ*2) )
    {
      pwidth = MAX_TILE_SZ; pheight = MAX_TILE_SZ;
      
      for (x=0; x < width; x += pwidth )
	{
	  if ( (x + pwidth) > width )
	    dw = pwidth - ((x + pwidth)-width);
	  else
	    dw = pwidth;
	  
	  XRenderComposite (w->dpy, PictOpSrc,
			    w->shadow_n_pic, None, pic,
			    0, 0, 0, 0, x, 0, dw, pheight);
	  XRenderComposite (w->dpy, PictOpSrc,
			    w->shadow_s_pic, None, pic,
			    0, 0, 0, 0, x, height - pheight, dw, pheight);
	}
    }

  /* Sides */

  if ( height > (MAX_TILE_SZ*2) )
    {
      pwidth = MAX_TILE_SZ; pheight = MAX_TILE_SZ;
      
      for (y=0; y < height; y += pheight)
	{
	  if ( (y + pheight) > height )
	    dh = pheight - ((y + pheight)-height);
	  else
	    dh = pheight;
	  
	  XRenderComposite (w->dpy, PictOpSrc /* PictOpIn */, 
			    w->shadow_e_pic, None, pic,
			    0, 0, 0, 0, 0, y, pwidth, dh);
	  XRenderComposite (w->dpy, PictOpSrc /* PictOpIn */,
			    w->shadow_w_pic, None, pic,
			    0, 0, 0, 0, width - pwidth, y, pwidth, dh);
	}
    }

  /* Corners */

  pwidth = MAX_TILE_SZ; pheight = MAX_TILE_SZ;

  XRenderComposite (w->dpy, PictOpSrc, w->shadow_nw_pic, None, pic,
		    0, 0, 0, 0, 0, 0, pwidth, pheight);

  XRenderComposite (w->dpy, PictOpSrc, w->shadow_ne_pic, None, pic,
		    0, 0, 0, 0, width - pwidth, 0, pwidth, pheight);

  XRenderComposite (w->dpy, PictOpSrc, w->shadow_sw_pic, None, pic,
		    0, 0, 0, 0, 0, height - pheight, pwidth, pheight);

  XRenderComposite (w->dpy, PictOpSrc, w->shadow_se_pic, None, pic,
		    0, 0, 0, 0, width - pwidth, height - pheight, 
		    pwidth, pheight);

  XFreePixmap (w->dpy, pxm);
  return pic;
}

static XserverRegion
client_win_extents (Wm *w, Client *client)
{
  int x, y, width, height;
  XRectangle	    r;

  /* XXX make coverage much fast as its now getting called all the time */
  client->get_coverage(client, &x, &y, &width, &height);  

  r.x = x;
  r.y = y; 
  r.width = width;
  r.height = height;

  if (w->config->shadow_style)
    {
      if (client->type == dialog 
	  || client->type == menu 
	  || client->type == MBCLIENT_TYPE_OVERRIDE)
	{
	  if (w->config->shadow_style == SHADOW_STYLE_SIMPLE)
	    {
	      r.width  += w->config->shadow_dx;
	      r.height += w->config->shadow_dy;
	    } else {
	      r.x      += w->config->shadow_dx;
	      r.y      += w->config->shadow_dy;
	      r.width  += w->config->shadow_padding_width;
	      r.height += w->config->shadow_padding_height;
	    }
	}
    }

  dbg("comp %s() +%i+%i , %ix%i\n", __func__, x, y, width, height);

  return XFixesCreateRegion (w->dpy, &r, 1);
}

static XserverRegion
client_border_size (Wm *w, Client *c, int x, int y)
{
    XserverRegion   border;
    border = XFixesCreateRegionFromWindow (w->dpy, c->frame, WindowRegionBounding );
    /* translate this */
    XFixesTranslateRegion (w->dpy, border, x, y);
    return border;
}

void
comp_engine_set_defualts(Wm *w)
{
  w->config->shadow_style = SHADOW_STYLE_GAUSSIAN;

  w->config->shadow_color[0] = 0;
  w->config->shadow_color[1] = 0;
  w->config->shadow_color[2] = 0;
  w->config->shadow_color[3] = 0xff;
  w->config->shadow_dx = SHADOW_OFFSET_X;
  w->config->shadow_dy = SHADOW_OFFSET_Y;

  /* Not really used yet */
  w->config->shadow_padding_width = 0;
  w->config->shadow_padding_height = 0;
}


void
comp_engine_theme_init(Wm *w)
{
  Pixmap                        transPixmap, blackPixmap, lowlightPixmap;
  XRenderPictureAttributes	pa;
  XRenderColor                  c;
  int                           i;
  Picture	                pics_to_free[] = { w->trans_picture,
						   w->black_picture,
						   w->lowlight_picture,
						   w->shadow_n_pic,
						   w->shadow_e_pic,
						   w->shadow_s_pic,
						   w->shadow_w_pic,
						   w->shadow_ne_pic,
						   w->shadow_nw_pic,
						   w->shadow_se_pic,
						   w->shadow_sw_pic,
						   w->shadow_pic };

  if (!w->have_comp_engine) return;

  for (i=0; i < (sizeof(pics_to_free)/sizeof(Picture)); i++)
    if (pics_to_free[i] != None) XRenderFreePicture (w->dpy, pics_to_free[i]);

  if (w->config->shadow_style == SHADOW_STYLE_NONE) return;

  if (w->config->shadow_style == SHADOW_STYLE_GAUSSIAN)
    shadow_setup (w);

  pa.subwindow_mode = IncludeInferiors;
  pa.repeat         = True;
  
  transPixmap = XCreatePixmap (w->dpy, w->root, 1, 1, 8);

  w->trans_picture 
    = XRenderCreatePicture (w->dpy, transPixmap,
			    XRenderFindStandardFormat (w->dpy, PictStandardA8),
			    CPRepeat,
			    &pa);

  c.red = c.green = c.blue = 0;
  c.alpha = 0xb0b0; 

  XRenderFillRectangle (w->dpy, PictOpSrc, w->trans_picture, &c, 0, 0, 1, 1);

  /* black pixmap used for shadows */

  blackPixmap = XCreatePixmap (w->dpy, w->root, 1, 1, 32);

  w->black_picture 
    = XRenderCreatePicture (w->dpy, blackPixmap,
			    XRenderFindStandardFormat (w->dpy, PictStandardARGB32),
			    CPRepeat,
			    &pa);

  c.red   = w->config->shadow_color[0] << 8;
  c.green = w->config->shadow_color[1] << 8;
  c.blue  = w->config->shadow_color[2] << 8;

  if (w->config->shadow_style == SHADOW_STYLE_GAUSSIAN)
    c.alpha = 0xffff;
  else
    c.alpha = w->config->shadow_color[3] << 8;

  dbg("%s() shadow alpha is %i\n", __func__, c.alpha);

  XRenderFillRectangle (w->dpy, PictOpSrc, w->black_picture, &c, 0, 0, 1, 1);

  /* Used for lowlights */

  lowlightPixmap = XCreatePixmap (w->dpy, w->root, 1, 1, 32);
  w->lowlight_picture 
    = XRenderCreatePicture (w->dpy, lowlightPixmap,
			    XRenderFindStandardFormat (w->dpy, PictStandardARGB32),
			    CPRepeat,
			    &pa);

  if (w->config->dialog_shade)
    {
      c.red   = w->config->lowlight_params[0] << 8;
      c.green = w->config->lowlight_params[1] << 8;
      c.blue  = w->config->lowlight_params[2] << 8;
      c.alpha = w->config->lowlight_params[3] << 8;
    } else {
      c.red =  0;
      c.green = 0;
      c.blue  = 0;
      c.alpha = 0x8d8d;
    }

  XRenderFillRectangle (w->dpy, PictOpSrc, w->lowlight_picture,
                        &c, 0, 0, 1, 1);
}

/* Shuts the compositing down */
void
comp_engine_deinit(Wm *w)
{
  Client *c = NULL;

  if (!w->have_comp_engine) 
    {
      w->comp_engine_disabled = True;
      return;
    }

  if (w->comp_engine_disabled) return;

  /* 
   *  really shut down the composite engine. 
   *
   */

  XCompositeUnredirectSubwindows (w->dpy, w->root, CompositeRedirectManual);

  if (w->root_picture) XRenderFreePicture (w->dpy, w->root_picture);
  if (w->root_buffer)  XRenderFreePicture (w->dpy, w->root_buffer);

  w->root_buffer  = None;
  w->root_picture = None;

  if (w->all_damage) XDamageDestroy (w->dpy, w->all_damage);

  /* Free up any client composite resources */

  START_CLIENT_LOOP(w, c) 
    {
      comp_engine_client_destroy(w, c);
    } 
  END_CLIENT_LOOP(w, c);

  /* XXX should free up any client picture data ? */

  w->comp_engine_disabled = True;
  w->have_comp_engine     = False; /* bad ? */
}

void
comp_engine_reinit(Wm *w)
{
  Client *c = NULL;

  w->comp_engine_disabled = False;
  comp_engine_init (w);

  XSync(w->dpy, False);

  if (w->head_client)
    {
      START_CLIENT_LOOP(w, c) 
	{

	  dbg("%s() calling init for '%s'\n", __func__, c->name);
	  
	  comp_engine_client_init(w, c);
	  comp_engine_client_show(w, c);

	} 
      END_CLIENT_LOOP(w, c);

      comp_engine_render(w, None);  
    }
}

Bool
comp_engine_init (Wm *w)
{
  int		                event_base,   error_base;
  int		                damage_error;
  int		                xfixes_event, xfixes_error;
  int		                render_event, render_error;
  XRenderPictureAttributes	pa;

  dbg("%s() called\n", __func__);

  if (w->comp_engine_disabled) return False;
  if (w->have_comp_engine)     return False; /* already running */

  w->comp_engine_disabled = True;

  if (!XRenderQueryExtension (w->dpy, &render_event, &render_error))
    {
      fprintf (stderr, "matchbox: No render extension\n");
      w->have_comp_engine = False;
      return False;
    }
  
  if (!XCompositeQueryExtension (w->dpy, &event_base, &error_base))
    {
      fprintf (stderr, "matchbox: No composite extension\n");
      w->have_comp_engine = False;
      return False;
    }
  
  if (!XDamageQueryExtension (w->dpy, &w->damage_event, &damage_error))
    {
      fprintf (stderr, "matchbox: No damage extension\n");
      w->have_comp_engine = False;
      return False;
    }
  
  if (!XFixesQueryExtension (w->dpy, &xfixes_event, &xfixes_error))
    {
      fprintf (stderr, "matchbox: No XFixes extension\n");
      w->have_comp_engine = False;
      return False;
    }
  
  w->have_comp_engine     = True;
  w->comp_engine_disabled = False;

  comp_stack = NULL;

  /* Make the shadow tiles */

  pa.subwindow_mode = IncludeInferiors;

  w->root_picture 
    = XRenderCreatePicture (w->dpy, w->root, 
			    XRenderFindVisualFormat (w->dpy,
						     DefaultVisual (w->dpy, w->screen)),
			    CPSubwindowMode,
			    &pa);
  
  w->all_damage = None;
  
  XCompositeRedirectSubwindows (w->dpy, w->root, CompositeRedirectManual);

  dbg("%s() success \n", __func__);

   return True;
}

void
comp_engine_client_init(Wm *w, Client *client)
{
  if (!w->have_comp_engine) return;

  client->damaged      = 0;
  client->damage       = None;
  client->picture      = None;
  client->shadow       = None;
  client->borderSize   = None;
  client->extents      = None;
  client->transparency = -1;

  comp_engine_client_get_trans_prop(w, client);

  stack_push(client);
}

int
comp_engine_client_get_trans_prop(Wm *w, Client *client)
{
   Atom actual;
   int format;
   unsigned long n, left;
   char *data;

    XGetWindowProperty(w->dpy, client->window, w->atoms[CM_TRANSLUCENCY], 
		       0L, 1L, False, w->atoms[INTEGER], &actual, &format, 
		       &n, &left, (unsigned char **) &data);

    if (data != None)
    {
      client->transparency  = (int) *data;
      XFree( (void *) data);
      return client->transparency;
    }
    return -1;
}


void
comp_engine_client_show(Wm *w, Client *client)
{
  XserverRegion   region;
  XRenderPictureAttributes pa;

  if (!w->have_comp_engine) return;

  dbg("%s() called\n", __func__);

  /* 
   *  Destroying / Recreating the client pictures should hopefully save
   *  some memory in the server.
   */

  if (client->picture == None)
    {
      pa.subwindow_mode = IncludeInferiors;

      client->picture = XRenderCreatePicture (w->dpy, 
					      client->frame,
					      XRenderFindVisualFormat (w->dpy, 
								       client->visual),
					      CPSubwindowMode,
					      &pa);
    }

  if (client->damage != None)
    XDamageDestroy (w->dpy, client->damage);

  client->damage = XDamageCreate (w->dpy, client->frame, 
				  XDamageReportNonEmpty);
  region = client_win_extents (w, client);
  comp_engine_add_damage (w, region);

  stack_top(client);
}

void
comp_engine_client_hide(Wm *w, Client *client)
{
  Client *t = NULL;

  if (!w->have_comp_engine) return;

  dbg("%s() called\n", __func__);

  if (client->flags & CLIENT_IS_MODAL_FLAG
      && ((t = wm_get_visible_main_client(w)) != NULL))
    {
      /* We need to make sure the any lowlighting on a 'parent' 
       * modal for app gets cleared. This is kind of a sledgehammer	 
       * approach to it, but more suttle attempts oddly fail at times.
       *
       * FIXME: keep an eye on this for future revisions of composite
       *        - there may be a better way.
       */
      comp_engine_client_repair (w, t); 
      comp_engine_add_damage (w, t->extents); 
    }

  if (client->damage != None)
    {
      XDamageDestroy (w->dpy, client->damage);
      client->damage = None;
    }

  if (client->extents != None)
    {
      comp_engine_add_damage (w, client->extents); 
      client->extents = None;
    }

  if (client->picture)
    {
      XRenderFreePicture (w->dpy, client->picture);
      client->picture = None;
    }
}

void
comp_engine_client_destroy(Wm *w, Client *client)
{
  if (!w->have_comp_engine) return;

  dbg("%s() called\n", __func__);

  comp_engine_client_hide(w, client);

  if (client->picture)
    XRenderFreePicture (w->dpy, client->picture);

  if (client->border_clip != None)
    XFixesDestroyRegion (w->dpy, client->border_clip);

  stack_remove(client);
}

static void
comp_engine_add_damage (Wm *w, XserverRegion damage)
{
  if (!w->have_comp_engine) return;

  dbg("%s() called\n", __func__);

    if (w->all_damage)
    {
      XFixesUnionRegion (w->dpy, w->all_damage, w->all_damage, damage);
      XFixesDestroyRegion (w->dpy, damage);
    }
    else
      w->all_damage = damage;
}

void
comp_engine_client_repair (Wm *w, Client *client)

{
  XserverRegion   parts;
  int x, y, width, height;

  if (!w->have_comp_engine) return;

  dbg("%s() called for client '%s'\n", __func__, client->name);
  
  parts = XFixesCreateRegion (w->dpy, 0, 0);
  
  /* translate region */
  dbg("%s() client damage is %li\n", __func__, client->damage);

  XDamageSubtract (w->dpy, client->damage, None, parts);

  client->get_coverage(client, &x, &y, &width, &height);  

  XFixesTranslateRegion (w->dpy, parts, x, y);

  comp_engine_add_damage (w, parts);
}


void
comp_engine_client_configure(Wm *w, Client *client)
{
  /* XXX not sure what to do here, if we even need anything */
}


void
comp_engine_handle_events(Wm *w, XEvent *ev)
{
  if (!w->have_comp_engine) return;

  if (ev->type == w->damage_event + XDamageNotify)
    {
      XDamageNotifyEvent *de;
      Client *c;
      
      dbg("%s() called have damage event \n", __func__);

      de = (XDamageNotifyEvent *)ev;
      
      c = wm_find_client(w, de->drawable, FRAME);

      if (c)
	comp_engine_client_repair(w, c);
      else
	{
	  dbg("%s() failed to find damaged window \n", __func__);
	}
    }
}

static void
_render_a_client(Wm           *w, 
		 Client       *client, 
		 XserverRegion region, 
		 Bool          want_lowlight)
{
  int x,y,width,height;
  XserverRegion winborder;

  if (client->picture == None) {
    dbg("%s() no pixture for %s\n", __func__, client->name);
    return;
  }

  if (client->extents)
    XFixesDestroyRegion (w->dpy, client->extents);

  client->extents = client_win_extents (w, client);

  client->get_coverage(client, &x, &y, &width, &height);  

  winborder = client_border_size (w, client, x, y);


  /* Transparency only done for dialogs and overides */

  if ( client->transparency == -1 
       || client->type == mainwin 
       || client->type == desktop
       || client->type == toolbar
       || client->type == dock)
    {
      XFixesSetPictureClipRegion (w->dpy, w->root_buffer, 0, 0, region);
      XFixesSubtractRegion (w->dpy, region, region, winborder);

      XRenderComposite (w->dpy, PictOpSrc, client->picture, None, w->root_buffer,
			0, 0, 0, 0, x, y, width, height);

    }

  if (want_lowlight && (client->type == mainwin || client->type == desktop))
    {
      int title_offset = 0;

      /* XXX maybe it would make more sense to calc geom of client->win */

      /* XXX Should modal for root lowlight entire display ? */

      if (client->type == mainwin) 
	title_offset = main_client_title_height(client);

      XRenderComposite (w->dpy, PictOpOver, w->lowlight_picture, None, 
			w->root_buffer,
			0, 0, 0, 0, x, y + title_offset,
			width, height - title_offset);
    }
	
  if (client->border_clip != None)
    {
      XFixesDestroyRegion (w->dpy, client->border_clip);
      client->border_clip = None;
    }

  client->border_clip = XFixesCreateRegion (w->dpy, 0, 0);
  XFixesCopyRegion (w->dpy, client->border_clip, region);

  XFixesDestroyRegion (w->dpy, winborder); /* XXX the leak plugged ? */
}

void
comp_engine_destroy_root_buffer(Wm *w)
{
  if (w->root_buffer)
    {
      XRenderFreePicture (w->dpy, w->root_buffer);
      w->root_buffer = None;
    }
}




void
comp_engine_render(Wm *w, XserverRegion region)
{

  Client       *c = NULL, *t = NULL;
  int           x,y,width,height;
  Bool          have_modal = False;
  StackItem    *item; 

  if (!w->have_comp_engine || !w->head_client) return;

  dbg("%s() called\n", __func__);

  if (!region) 
    {
      XRectangle  r;
      r.x = 0;
      r.y = 0;
      r.width = w->dpy_width;
      r.height = w->dpy_height;
      region = XFixesCreateRegion (w->dpy, &r, 1);
    }

  if (w->flags & DESKTOP_RAISED_FLAG)
    c = wm_get_desktop(w);
  else if (w->main_client)
    c = w->main_client;
  else
    c = w->head_client;

  if (!w->root_buffer)
    {
      Pixmap rootPixmap = XCreatePixmap (w->dpy, w->root, 
					 w->dpy_width, w->dpy_height,
					 DefaultDepth (w->dpy, w->screen));

      w->root_buffer = XRenderCreatePicture (w->dpy, rootPixmap,
					     XRenderFindVisualFormat (w->dpy,
								      DefaultVisual (w->dpy, w->screen)),
					     0, 0);

      XRenderComposite (w->dpy, PictOpSrc, w->black_picture, 
      			None, w->root_buffer, 0, 0, 0, 0, 0, 0, 
      			w->dpy_width, w->dpy_height);

      XFreePixmap (w->dpy, rootPixmap);
    }

  XFixesSetPictureClipRegion (w->dpy, w->root_picture, 0, 0, region);


  XFixesSetPictureClipRegion (w->dpy, w->root_buffer, 0, 0, region);

  /*
#if MONITOR_REPAINT
  XRenderComposite (w->dpy, PictOpSrc, w->black_picture, None, w->root_picture,
		    0, 0, 0, 0, 0, 0, w->dpy_width, w->dpy_height);
#endif
  */

  /* Render top -> bottom */

  /* Menu's */

  START_CLIENT_LOOP(w, t) 
    {
      if ((t->type == menu || t->type == MBCLIENT_TYPE_OVERRIDE))
	_render_a_client(w, t, region, False);
    } 
  END_CLIENT_LOOP(w, t);

  /* Dialogs */

  stack_enumerate(item)
    {
      t = item->client;
      if (t->type == dialog 
	  && (t->trans == c || t->trans == NULL)  
	  && t->mapped )
	{
	  if (t->flags & CLIENT_IS_MODAL_FLAG)
	    have_modal = True; 
	  _render_a_client(w, t, region, False);
	}
    } 

  /* panels + toolbars */

  if (!(c && c->flags & CLIENT_FULLSCREEN_FLAG))
    {
      START_CLIENT_LOOP(w, t) 
	{
	  if (t->type == dock || t->type == toolbar)
	    {
	      /* dont render hidden titlebar panels */
	      if (t->type == dock && t->flags & CLIENT_DOCK_TITLEBAR
		  && (w->flags & DESKTOP_RAISED_FLAG 
		      || !mbtheme_has_titlebar_panel(w->mbtheme)))
		continue;

	      dbg("%s() rendering toolbar\n", __func__);
	      _render_a_client(w, t, region, False);
	    }
	} 
      END_CLIENT_LOOP(w, t);
    }

  if (c && ( c->type == mainwin || c->type == desktop ) && c->picture)
    {
      _render_a_client(w, c, region, have_modal);
    }
  else
    {
      /* No main client, Render block of boring black color */

      dbg("%s() rendering darkness\n", __func__);

      XFixesSetPictureClipRegion (w->dpy, w->root_buffer, 0, 0, region);

      XRenderComposite (w->dpy, PictOpSrc, w->black_picture, 
      			None, w->root_buffer, 0, 0, 0, 0, 0, 0, 
      			w->dpy_width, w->dpy_height);
    }

  XFixesSetPictureClipRegion (w->dpy, w->root_buffer, 0, 0, None);

  /* Now render shadows */

  stack_enumerate(item)
    {
      t = item->client;
      if ((t->type == dialog 
	   && (t->trans == c || t->trans == NULL)  
	   && t->mapped) || t->type == menu || t->type == MBCLIENT_TYPE_OVERRIDE)
	{
	  if (!t->picture) {
	    dbg("%s() no pixture for %s\n", __func__, t->name);
	    continue;
	  }
	
	  if (w->config->shadow_style)
	    {
	      Picture shadow_pic;
	  
	      t->get_coverage(t, &x, &y, &width, &height);  
	  
	      if (w->config->shadow_style == SHADOW_STYLE_SIMPLE)
		{
		  XserverRegion shadow_region;
		  
		  /* Grab 'shape' region of window */
		  shadow_region = client_border_size (w, t, x, y);
		  
		  /* Offset it. */
		  XFixesTranslateRegion (w->dpy, shadow_region, 
					 w->config->shadow_dx, 
					 w->config->shadow_dy);
		  
		  /* Intersect it, so only border remains */
		  XFixesIntersectRegion (w->dpy, shadow_region,
					 t->border_clip, shadow_region );
		  
		  XFixesSetPictureClipRegion (w->dpy, w->root_buffer, 
					      0, 0, shadow_region);
		  
		  XRenderComposite (w->dpy, PictOpOver, w->black_picture, 
				    None, 
				    w->root_buffer,
				    0, 0, 0, 0,
				    x + w->config->shadow_dx,
				    y + w->config->shadow_dy,
				    width  + w->config->shadow_padding_width, 
				    height + w->config->shadow_padding_height);

		  if (t->transparency != -1)
		    XRenderComposite (w->dpy, PictOpOver, t->picture, w->trans_picture,
				      w->root_buffer, 0, 0, 0, 0, x, y, width, height);
		  
		  XFixesDestroyRegion (w->dpy, shadow_region);
		}
	      else 		/* GAUSSIAN */
		{
		  
		  /* Combine pregenerated shadow tiles */
		  shadow_pic 
		    = shadow_gaussian_make_picture (w, 
						    width + w->config->shadow_padding_width, 
						    height + w->config->shadow_padding_height);
		  
		  XFixesSetPictureClipRegion (w->dpy, w->root_buffer, 
					      0, 0, t->border_clip);
		  
		  XRenderComposite (w->dpy, PictOpOver, w->black_picture, 
				    shadow_pic, 
				    w->root_buffer,
				    0, 0, 0, 0,
				    x + w->config->shadow_dx,
				    y + w->config->shadow_dy,
				    width + w->config->shadow_padding_width, 
				    height + w->config->shadow_padding_height);
		  
		  if (t->transparency != -1)
		    XRenderComposite (w->dpy, PictOpOver, t->picture, w->trans_picture,
				      w->root_buffer, 0, 0, 0, 0, x, y, width, height);


		  XRenderFreePicture (w->dpy, shadow_pic);
		  
		}
	      
	    }

	}
    }
  
  XFixesSetPictureClipRegion (w->dpy, w->root_buffer, 0, 0, None);

  XRenderComposite (w->dpy, PictOpSrc, w->root_buffer, None, w->root_picture,
		    0, 0, 0, 0, 0, 0, w->dpy_width, w->dpy_height);

  XSync(w->dpy, False);
}

#endif

void
comp_engine_time(Wm *w)
{

#ifdef STANDALONE   	/* No timings for standalone */
  return;
#endif /* STANDALONE */

#if DO_TIMINGS 
  struct timeval       tv_start, tv_end;
  struct timezone      tz;
  long                 diff;

#ifdef USE_COMPOSITE

  XSync(w->dpy, False);
  gettimeofday(&tv_start, &tz);
  
  XFixesSetPictureClipRegion (w->dpy, w->root_buffer, 0, 0, None);
  XFixesSetPictureClipRegion (w->dpy, w->root_picture, 0, 0, None);

  XRenderComposite (w->dpy, PictOpOver, w->lowlight_picture, None, 
		    w->root_buffer,
		    0, 0, 0, 0, 0, 0, w->dpy_width, w->dpy_height);

  XRenderComposite (w->dpy, PictOpSrc, w->root_buffer, None, w->root_picture,
		    0, 0, 0, 0, 0, 0, w->dpy_width, w->dpy_height);

  XSync(w->dpy, False);
  gettimeofday(&tv_end, &tz);
  
  diff = ((tv_end.tv_sec * 1000000) + tv_end.tv_usec) - ((tv_start.tv_sec * 1000000) + tv_start.tv_usec);
  fprintf(stderr, "COMPOSITE LOWLIGHT TIMING: %li us\n", diff); 

  sleep(1);

  comp_engine_render(w, None);

#else

#ifndef STANDALONE

  MBPixbufImage       *img;
  int                  x, y;
  Pixmap               pxm_tmp;
  Window               win;
  XSetWindowAttributes attr;
      
  attr.override_redirect = True;
  attr.event_mask = ChildMask|ButtonPressMask|ExposureMask;

  XSync(w->dpy, False);
  gettimeofday(&tv_start, &tz);
       
  win     = XCreateWindow(w->dpy, w->root, 0, 0,
			   w->dpy_width, w->dpy_height, 0,
			   CopyFromParent, 
			   CopyFromParent, 
			   CopyFromParent,
			   CWOverrideRedirect|CWEventMask,
			   &attr);

  pxm_tmp = XCreatePixmap(w->dpy, win,  
			  w->dpy_width, 
			  w->dpy_height ,
			  w->pb->depth);
  
  img = mb_pixbuf_img_new_from_x_drawable(w->pb, w->root, 
					  None, 0, 0,
					  w->dpy_width, 
					  w->dpy_height,
					  True);

  if (!img) 
    {
      fprintf(stderr, "%s(): failed to grab root image\n", __func__);
      return;
    }



  for (x = 0; x < w->dpy_width; x++)
    for (y = 0; y < w->dpy_height; y++)
      mb_pixbuf_img_plot_pixel_with_alpha(w->pb,
					  img, x, y, 
					  w->config->lowlight_params[0],
					  w->config->lowlight_params[1],
					  w->config->lowlight_params[2],
					  w->config->lowlight_params[3] 
					  );

  mb_pixbuf_img_render_to_drawable(w->pb, img, pxm_tmp, 0, 0);
  
  XSetWindowBackgroundPixmap(w->dpy, win, pxm_tmp);
  XClearWindow(w->dpy, win);

  XMapRaised(w->dpy, win);  

  XSync(w->dpy, False);
  gettimeofday(&tv_end, &tz);
  
  diff = ((tv_end.tv_sec * 1000000) + tv_end.tv_usec) - ((tv_start.tv_sec * 1000000) + tv_start.tv_usec);
  fprintf(stderr, "XIMAGE LOWLIGHT TIMING: %li us\n", diff); 
  
  mb_pixbuf_img_free(w->pb, img);
  XFreePixmap(w->dpy, pxm_tmp);

  XSync(w->dpy, False);

  sleep(1);

  XDestroyWindow(w->dpy, win);

#endif /*  STANDALONE */

#endif /* USE_COMPOSITE */

#endif
}
