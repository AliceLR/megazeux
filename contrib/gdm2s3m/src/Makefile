##
# gdm2s3m Makefile fragment
##

.PHONY: all clean

.SUFFIXES: .c

CFLAGS := -Wall -std=c99
o := o

gdm2s3m_base = contrib/gdm2s3m/src
gdm2s3m_cflags := -W -Wmissing-format-attribute -Wpointer-arith -Wcast-align
gdm2s3m_cflags += -Wwrite-strings -pedantic -I${gdm2s3m_base}
gdm2s3m = ${gdm2s3m_base}/libgdm2s3m.a

gdm2s3m_objs = \
    ${gdm2s3m_base}/error.${o} ${gdm2s3m_base}/gdm.${o} \
    ${gdm2s3m_base}/s3m.${o} ${gdm2s3m_base}/utility.${o} \
    ${gdm2s3m_base}/gdm2s3m.${o}

${gdm2s3m_base}/%.${o}: ${gdm2s3m_base}/%.c
	${CC} ${CFLAGS} ${gdm2s3m_cflags} -c $< -o $@

all: ${gdm2s3m}

${gdm2s3m}: ${gdm2s3m_objs}
	ar crus ${gdm2s3m} ${gdm2s3m_objs}

clean:
	rm -f ${gdm2s3m} ${gdm2s3m_objs}
