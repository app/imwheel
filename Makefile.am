SUBDIRS=jax @getopt@ @mdetect@ @gpm_imwheel@

bin_PROGRAMS=imwheel

AM_CFLAGS=@CFLAGS@ @GETOPT_INCS@ @X_CFLAGS@ -I@srcdir@/jax -I. -Wall
imwheel_SOURCES=imwheel.c util.c cfg.c util.h imwheel.h cfg.h
imwheel_LDADD=@GETOPT_LIBS@ -L$(top_builddir)/jax -ljax @X_LIBS@ 
imwheel_DEPENDENCIES=imwheel.o util.o cfg.o jax/libjax.a

EXTRA_PROGRAMS=setimps2 setmmplus setps2 getmdt mdump setps2rate
noinst_PROGRAMS=@extras@ @mdump@

man_MANS=imwheel.1

EXTRA_DIST=BUGS FREEBSD EMACS M-BA47 BUGS see imwheel.1.html imwheel.1 TODO imwheelrc @extras_dist@ @mdump_dist@ lefty.sh imwheel.spec slack.sh

dist-hook:
	-for DIR in @gpm_dist@ @mdetect_dist@ @getopt_dist@ ; \
	do \
		make -C $$DIR distclean ; \
		cp -Rfp @srcdir@/$$DIR $(distdir)/$$DIR ; \
		find $(distdir) -name CVS -exec rm -rvf '{}' ';' ; \
	done

ETCDIR=/etc/X11/imwheel

install-exec-hook:
if SUID
	progname=$(DESTDIR)$(bindir)/`echo imwheel|sed 's/$(EXEEXT)$$//'|sed '$(transform)'|sed 's/$$/$(EXEEXT)/'` ; \
	chown @suid@ $$progname ; \
	chmod u+s $$progname
endif
	$(mkinstalldirs) $(ETCDIR)
	-[ -f /etc/imwheelrc ] && mv /etc/imwheelrc $(ETCDIR)/imwheelrc || true
	-[ -f /etc/X11/imwheelrc ] && mv /etc/X11/imwheelrc $(ETCDIR)/imwheelrc || true
	-[ -f $(ETCDIR)/imwheelrc ] && install -m 644 imwheelrc $(ETCDIR)/imwheelrc.new || install -m 644 imwheelrc $(ETCDIR)/imwheelrc

extras: $(EXTRA_PROGRAMS)

if NO_GPM_DOC
Makefile.am: .premake
.premake:
	@#Start
	@touch .premake
	@rm -f @gpm_imwheel@/doc/Makefile
all: .postmake
distclean: .postmake
.postmake:
	@#End
	@rm -f .premake
endif
