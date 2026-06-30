# Don't edit makefile, edit makefile.bas instead.
#

RM=  rm -f

ALL	=	os-a32.c os-aix.c os-bsd.c os-bsi.c os-cvx.c os-dos.c \
		os-dyn.c os-gen.c os-hpp.c os-isc.c os-lnx.c os-nxt.c \
		os-osf.c os-ptx.c os-s40.c os-sgi.c \
		os-sun.c os-sv4.c os-ult.c os-aux.c

.SUFFIXES: .ic

.ic.c:
		./includer < $*.ic > $*.c

all:		includer $(ALL)

includer:	includer.c
		$(CC) -o includer includer.c

clean:
		$(RM) $(ALL) includer

# You don't have to run this unless you change a .ic file.
depend:
		./makedep

# Makedep only catches 1-level deep includes.  If something depends on a
# 2nd-level include, put it here.
os-a32.c: sunquota
os-dyn.c: sunquota
os-hpp.c: sunquota
os-osf.c: sunquota
os-ptx.c: sunquota
os-s40.c: sunquota
os-sgi.c: sunquota
os-sun.c: sunquota
os-sv4.c: sunquota



































