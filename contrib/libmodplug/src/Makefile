#
# src/ makefile
#

include ../../../Makefile.in
include ../../../Makefile.platform

# need to include local headers
INCLUDES = -I. -Ilibmodplug

#
# objects to build
#

objs  = fastmix.${o} load_669.${o} load_amf.${o} load_ams.${o} load_dbm.${o} \
        load_dmf.${o} load_dsm.${o} load_far.${o} load_it.${o} load_mdl.${o} \
        load_med.${o} load_mod.${o} load_mt2.${o} load_mtm.${o} load_okt.${o} \
        load_psm.${o} load_ptm.${o} load_s3m.${o} load_stm.${o} load_ult.${o} \
        load_umx.${o} load_wav.${o} load_xm.${o} mmcmp.${o} modplug.${o} \
        snd_dsp.${o} sndfile.${o} snd_flt.${o} snd_fx.${o} sndmix.${o} \
        tables.${o}

libs:	${objs}
	${AR} crus libmodplug.a ${objs}

clean:
	rm -f ${objs} *~ libmodplug.a
