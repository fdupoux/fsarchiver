SUBDIRS = src doc

EXTRA_DIST = AUTHORS COPYING ChangeLog INSTALL NEWS README website distrib internals autogen.sh

static:
	rm -f src/fsarchiver
	$(MAKE) LDFLAGS="$(LDFLAGS) -static"

distclean-local:
	rm -rf autom4te.cache
	rm -f config.h.in~
	
dist-hook:
	rm -rf `find $(distdir) -name .svn`

MAINTAINERCLEANFILES = Makefile.in configure aclocal.m4 install-sh missing depcomp
