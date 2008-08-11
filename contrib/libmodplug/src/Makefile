##
# libmodplug Makefile fragment
##

.PHONY: all clean

.SUFFIXES: .cpp

CXXFLAGS := -Wall
o := o

libmodplug_base = contrib/libmodplug/src

libmodplug_cflags = -DHAVE_INTTYPES_H -I${libmodplug_base} \
                    -I${libmodplug_base}/libmodplug

libmodplug_objs = \
    ${libmodplug_base}/fastmix.${o} ${libmodplug_base}/load_669.${o} \
    ${libmodplug_base}/load_amf.${o} ${libmodplug_base}/load_ams.${o} \
    ${libmodplug_base}/load_dbm.${o} ${libmodplug_base}/load_dmf.${o} \
    ${libmodplug_base}/load_dsm.${o} ${libmodplug_base}/load_far.${o} \
    ${libmodplug_base}/load_it.${o} ${libmodplug_base}/load_mdl.${o} \
    ${libmodplug_base}/load_med.${o} ${libmodplug_base}/load_mod.${o} \
    ${libmodplug_base}/load_mt2.${o} ${libmodplug_base}/load_mtm.${o} \
    ${libmodplug_base}/load_okt.${o} ${libmodplug_base}/load_psm.${o} \
    ${libmodplug_base}/load_ptm.${o} ${libmodplug_base}/load_s3m.${o} \
    ${libmodplug_base}/load_stm.${o} ${libmodplug_base}/load_ult.${o} \
    ${libmodplug_base}/load_umx.${o} ${libmodplug_base}/load_wav.${o} \
    ${libmodplug_base}/load_xm.${o} ${libmodplug_base}/mmcmp.${o} \
    ${libmodplug_base}/modplug.${o} ${libmodplug_base}/snd_dsp.${o} \
    ${libmodplug_base}/sndfile.${o} ${libmodplug_base}/snd_flt.${o} \
    ${libmodplug_base}/snd_fx.${o} ${libmodplug_base}/sndmix.${o} \
    ${libmodplug_base}/tables.${o}

libmodplug = ${libmodplug_base}/libmodplug.a

${libmodplug_base}/%.${o}: ${libmodplug_base}/%.cpp
	${CXX} ${CXXFLAGS} ${libmodplug_cflags} -c $< -o $@

${libmodplug}: ${libmodplug_objs}
	ar crus ${libmodplug} ${libmodplug_objs}

all: ${libmodplug}

clean:
	rm -f ${libmodplug_objs} ${libmodplug}
