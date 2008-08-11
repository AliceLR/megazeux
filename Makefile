#
# Slave MegaZeux makefile
#
include Makefile.platform
include Makefile.in

SUBDIRS = contrib/gdm2s3m/src contrib/libmodplug/src src

all:	subdir

subdir:
	list='$(SUBDIRS)';                 \
	for subdir in $$list; do           \
	  pwd=`pwd`;                       \
	  cd $$subdir && make && cd $$pwd; \
	done;

clean:
	list='$(SUBDIRS)';                       \
	for subdir in $$list; do                 \
	  pwd=`pwd`;                             \
	  cd $$subdir && make clean && cd $$pwd; \
	done;

install:
	mkdir -p ${PREFIX}/share/megazeux && \
	chown root:root ${PREFIX}/share/megazeux && \
	chmod 0755 ${PREFIX}/share/megazeux && \
	install -o root -m 0644 mzx_default.chr ${PREFIX}/share/megazeux && \
	install -o root -m 0644 mzx_blank.chr ${PREFIX}/share/megazeux && \
	install -o root -m 0644 mzx_smzx.chr ${PREFIX}/share/megazeux && \
	install -o root -m 0644 mzx_ascii.chr ${PREFIX}/share/megazeux && \
	install -o root -m 0644 smzx.pal ${PREFIX}/share/megazeux && \
	install -o root -m 0644 mzx_help.fil ${PREFIX}/share/megazeux && \
	install -o root -m 0644 config.txt /etc/megazeux-config && \
	install -o root -m 0755 ${TARGET} ${PREFIX}/bin && \
	ln -sf ${TARGET} ${PREFIX}/bin/megazeux
