if ENABLE_SCHEMAS
  SUBDIRS = themes schemas
else
  SUBDIRS = themes
endif

defaultsdir = $(datadir)/matchbox

if WANT_STANDALONE
  defaults_DATA = kbdconfig
else
  defaults_DATA = mbnoapp.xpm kbdconfig
endif

EXTRA_DIST = mbnoapp.xpm kbdconfig

