#
# Makefile for libnids.
#
# Dug Song <dugsong@monkey.org>


all: static 
static:
	cd src ; $(MAKE) $(AM_MAKEFLAGS) $@
clean:
	cd src ; $(MAKE) $(AM_MAKEFLAGS) $@
	
distclean: clean
	rm -f Makefile */Makefile */config.h config.status config.cache config.log *~ 

# EOF
