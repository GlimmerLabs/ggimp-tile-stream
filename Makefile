# ggimp-tile-stream/Makefile
#   Support for some Gimp PDB functions that make it easier to
#   deal with tile streams.

# +----------+--------------------------------------------------------
# | Settings |
# +----------+

CFLAGS = -g -Wall -DDEBUG

LDFLAGS = -L. -ltilestream

# +----------+--------------------------------------------------------
# | Commands |
# +----------+

# Instructions for building a plugin.  Note that gimptool-2.0 only
# takes CFLAGS and LDFLAGS from the environment, so we need to put
# them into the environment.
BUILD_PLUGIN = \
        export CFLAGS="$(CFLAGS)"; \
        export LDFLAGS="$(LDFLAGS)"; \
        gimptool-2.0 --build


# +-------+-----------------------------------------------------------
# | Files |
# +-------+

PLUGINS = 

INSTALL = $(addsuffix .install,$(PLUGINS))
LOCAL = $(addsuffix .local,$(PLUGINS))

LIBRARIES=libtilestream.a


# +------------------+------------------------------------------------
# | Standard Targets |
# +------------------+

default: install-local

install-local: $(PLUGINS) $(LOCAL)

install: $(INSTALL)

clean:
	rm -f $(PLUGINS) $(LIBRARIES) $(LOCAL) $(INSTALL)

distclean: clean

# +------------------+------------------------------------------------
# | Building Plugins |
# +------------------+

%: %.c
	$(BUILD_PLUGIN) $^

# +--------------------+----------------------------------------------
# | Installing Plugins |
# +--------------------+

%.install: %
	gimptool-2.0 --install-admin-bin $<
	touch $@

%.local: %
	gimptool-2.0 --install-bin $<
	touch $@

%.uninstall: %
	gimptool-2.0 --uninstall-admin-bin $<
	gimptool-2.0 --uninstall-bin $<
	rm -f $*.install
	rm -f $*.local

# +-----------+-------------------------------------------------------
# | Libraries |
# +-----------+

tile-stream.o: tile-stream.c tile-stream.h
	$(CC) $(CFLAGS) $< -c -o $@ $(shell pkg-config --cflags gimp-2.0)

libtilestream.a: tile-stream.o
	ar -r $@ $^
	ranlib $@


# +-------------+-----------------------------------------------------
# | Experiments |
# +-------------+

ts-expt: ts-expt.c tile-stream.h libtilestream.a 
	$(BUILD_PLUGIN) $<

install-expts: ts-expt
	gimptool-2.0 --install-bin $<
