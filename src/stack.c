#include "stack.h"

void
stack_add_below_client(Client *client, Client *client_above)
{
  Wm *w = client->wm;

  if (client_above == NULL)
    {
      /* nothing above raising to top */
      if (w->stack_top)
	{
	  client->prev = w->stack_top;
	  w->stack_top->prev = client;
	}

      w->stack_top = client;
    }
  else
    {
      client->next = client_above;
      client->prev = client_above->prev;

      if (client_above->prev) client_above->prev->next = client;
      client_above->prev = client;
    }

  if (client_above == w->stack_bottom)
    w->stack_bottom = client;

  w->stack_n_items++;
}


void
stack_add_above_client(Client *client, Client *client_below)
{
  Wm *w = client->wm;

  if (client_below == NULL)
    {
      /* NULL so nothing below add at bottom */
      if (w->stack_bottom) 
	{
	  client->next = w->stack_bottom;
	  w->stack_bottom->prev = client;
	}

      w->stack_bottom = client;
    }
  else
    {
      client->prev = client_below;
      client->next = client_below->next;
      if (client->prev) client->prev->next = client;
      if (client->next) client->next->prev = client;


    }

  if (client_below == w->stack_top)
    w->stack_top = client;

  w->stack_n_items++;
}


void   /* can this call above client ? */
stack_append_top(Client *client)
{
  Wm *w = client->wm;

  stack_add_above_client(client, w->stack_top);
}

void   /* can this call above client ? */
stack_prepend_bottom(Client *client)
{
  Wm *w = client->wm;

  stack_add_below_client(client, w->stack_bottom);
}



void
stack_remove(Client *client)
{
  Wm *w = client->wm;

  if (w->stack_top == w->stack_bottom)
    {
      w->stack_top = w->stack_bottom = NULL;
    }
  else
    {
      if (client == w->stack_top)
	w->stack_top = client->prev;

      if (client == w->stack_bottom)
	w->stack_bottom = client->next;

      if (client->prev != NULL) client->prev->next = client->next;
      if (client->next != NULL) client->next->prev = client->prev;
    }

  client->next = client->prev = NULL;

  w->stack_n_items--;
}

void
stack_preppend(Client *client)
{

}

void
stack_update_transients(Client *client)
{
  Wm     *w = client->wm;
  Client *client_cur;

  stack_enumerate_transients(w,client_cur,client)
    {
      stack_move_above_client(client_cur, client);
    }
}

void
stack_move_above_type(Client *client, int type, int flags)
{
  Wm     *w = client->wm;
  Client *client_below = NULL, *client_cur;

  stack_enumerate(w, client_cur)
    {
      if (client_cur->type == type)
	{
	  if (flags)
	    {
	      if (client_cur->flags & flags)
		client_below = client_cur;
	    }
	  else client_below = client_cur;
	}
    }

  if (client_below)  /* TODO: what if this is NULL ?? */
    stack_move_above_client(client, client_below);
}

void
stack_move_below_type(Client *client, int type, int flags)
{
  Wm     *w = client->wm;
  Client *client_above = NULL, *client_cur;

  stack_enumerate_reverse(w, client_cur)
    {
      if (client_cur->type == type)
	{
	  if (flags)
	    {
	      if (client_cur->flags & flags)
		client_above = client_cur;
	    }
	  else client_above = client_cur;
	}
    }

  if (client_above)  /* TODO: what if this is NULL ?? */
    stack_move_below_client(client, client_above);


  /* must move any transients too */
}

void
stack_move_below_client(Client *client, Client *client_above)
{
  stack_remove(client);
  stack_add_below_client(client, client_above);

  /* must move any transients too */

}

void
stack_move_above_client(Client *client, Client *client_below)
{

  stack_remove(client);
  stack_add_above_client(client, client_below);

  /* must move any transients too */

}


/* returns top -> bottom */
Window*
stack_get_window_list(Wm *w)
{
  Window *win_list;
  Client *c;
  int     i = 0;

  if (!w->stack_n_items) return NULL;

  win_list = malloc(sizeof(Window)*w->stack_n_items);

  stack_enumerate_reverse(w, c)
  {
    win_list[i++] = c->frame;
  }

  return win_list;
}

void
stack_sync_to_display(Wm *w)
{
  /*

    int XRestackWindows(Display *display, Window windows[], int nwindows);

    The XRestackWindows function restacks the windows in the order speci-
    fied, from top to bottom.  The stacking order of the first window in
    the windows array is unaffected, but the other windows in the array are
    stacked underneath the first window, in the order of the array.  The
    stacking order of the other windows is not affected.  For each window
    in the window array that is not a child of the specified window, a Bad-
    Match error results.


  */

  Window *win_list = stack_get_window_list(w);
  
  if (win_list)
    {
      XRestackWindows(w->dpy, win_list, w->stack_n_items);
      free(win_list);
    }

}

#if 0

/* Test bits for stack  */

int
main(int argc, char **argv)
{
  Wm *w;
  int i = 0; 
  Client *c, *midc, *midc2; 


  w = malloc(sizeof(Wm));
  memset(w, 0, sizeof(Wm));

  for (i=0; i<10; i++)
    {
      char    buf[64];

      sprintf(buf, "Client-%i", i);
      
      c = client_new(w, buf);

      stack_append_top(c);

      if (i == 5) midc = c;
      if (i == 6) midc2 = c;
    }

  printf("w->stack_top : %s, next %p   w->stack_bottom : %s, prev %p\n", 
	 w->stack_top->name, w->stack_top->next, 
	 w->stack_bottom->name, w->stack_bottom->prev);


  stack_move_below_client(w->stack_top, w->stack_bottom);
  stack_move_above_client(w->stack_bottom, w->stack_top);

  printf("w->stack_top : %s, next %p   w->stack_bottom : %s, prev %p\n", 
	 w->stack_top->name, w->stack_top->next, 
	 w->stack_bottom->name, w->stack_bottom->prev);


  stack_enumerate(w,c)
    {
      printf("%s\n", c->name);
    }

  printf("\n");


  printf("\n");



  stack_remove(w->stack_top);

  stack_remove(w->stack_bottom);



  stack_enumerate(w,c)
    {
      printf("%s\n", c->name);
    }

  stack_move_below_client(w->stack_top, w->stack_bottom);
  stack_move_above_client(midc, midc2);

  printf("\n");


  stack_enumerate(w,c)
    {
      printf("%s\n", c->name);
    }

}

#endif
