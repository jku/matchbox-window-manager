#ifndef _EWMH_H_
#define _EWMH_H_

#include "structs.h" 
#include "wm.h"

/* Non aton defines */
#define _NET_WM_STATE_REMOVE        0    /* remove/unset property */
#define _NET_WM_STATE_ADD           1    /* add/set property */
#define _NET_WM_STATE_TOGGLE        2    /* toggle property  */

void 
ewmh_init (Wm *w);

void 
ewmh_init_props (Wm *w);

void 
ewmh_update (Wm *w);

void
ewmh_update_lists(Wm *w);

void
ewmh_update_rects(Wm *w);

void 
ewmh_set_active (Wm *w);

int  
ewmh_handle_root_message (Wm *w, XClientMessageEvent *e);

unsigned char *
ewmh_get_utf8_prop (Wm *w, Window win, Atom req_atom);

Bool 
ewmh_state_check (Client *c, Atom atom_state_wanted);

void 
ewmh_set_allowed_actions (Wm *w, Client *c);

void 
ewmh_hung_app_check (Wm *w);

#ifndef REDUCE_BLOAT
int 
*ewmh_get_icon_prop_data (Wm *w, Window win);
#endif

int 
ewmh_utf8_len(unsigned char *str);

int 
ewmh_utf8_get_byte_cnt(unsigned char *str, int num_chars);

Bool 
ewmh_utf8_validate(unsigned char *str, int max_len);

#endif
