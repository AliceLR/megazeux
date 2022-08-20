#
# install/uninstall Makefile
#
# These recipes are intended primarily for use with the unix and darwin
# platforms, but may be useful for other platforms. Include this in any arch
# Makefile.in to enable 'make install' and 'make uninstall' for that platform.
#
# An 'install-arch' and 'uninstall-arch' need to be provided after including
# this file to handle platform-specific install files (if any).
#

.PHONY: install install-arch uninstall uninstall-arch install-check

gzip = gzip
ifeq (${USER},root)
install = install -o root
else
install = install
endif

ifneq (${V},1)
install := @${install}
gzip    := @${gzip}
endif

#
# Check to make sure 'make install' and 'make uninstall' aren't accidentally
# used for platforms/subplatforms that don't support it.
#

ERROR_PLATFORM = Attempted 'make install' or 'make uninstall' for invalid platform!

install-check:
	@echo Checking ability to install/uninstall...
	@echo GAMESDIR: ${GAMESDIR}
ifeq (${GAMESDIR},.)
	$(error ${ERROR_PLATFORM} (GAMESDIR))
endif
	@echo SYSCONFDIR: ${SYSCONFDIR}
ifeq (${SYSCONFDIR},.)
	$(error ${ERROR_PLATFORM} (SYSCONFDIR))
endif
	@echo SHAREDIR: ${SHAREDIR}
ifeq (${SHAREDIR},.)
	$(error ${ERROR_PLATFORM} (SHAREDIR))
endif
	@echo LICENSEDIR: ${LICENSEDIR}
ifeq (${LICENSEDIR},.)
	$(error ${ERROR_PLATFORM} (LICENSEDIR))
endif
ifneq (${BUILD_MODULAR},)
	@echo LIBDIR: ${LIBDIR}
ifeq (${LIBDIR},.)
	$(error ${ERROR_PLATFORM} (LIBDIR, modular enabled))
endif
endif
ifneq (${BUILD_UTILS},)
	@echo BINDIR: ${BINDIR}
ifeq (${BINDIR},.)
	$(error ${ERROR_PLATFORM} (BINDIR, utils enabled))
endif
endif

#
# Install the current build to the system.
#
# Note regarding EXTRA_LICENSES: library licenses from Makefile aren't included
# here as they're generally linked dynamically and have their own licenses
# available for platforms that support make install.
#

install: install-check install-arch
	@echo Installing...
	${install} -m 0755 -d \
		${DESTDIR}${SYSCONFDIR} \
		${DESTDIR}${SHAREDIR} \
		${DESTDIR}${SHAREDIR}/doc \
		${DESTDIR}${SHAREDIR}/doc/megazeux \
		${DESTDIR}${SHAREDIR}/megazeux \
		${DESTDIR}${SHAREDIR}/megazeux/assets \
		${DESTDIR}${LICENSEDIR} \
		${DESTDIR}${LICENSEDIR}/megazeux
	${install} -m 0644 \
		assets/default.chr \
		assets/edit.chr \
		assets/smzx.pal \
		${DESTDIR}${SHAREDIR}/megazeux/assets
	${install} -m 0644 \
		config.txt \
		${DESTDIR}${SYSCONFDIR}/megazeux-config
	${gzip} -c docs/changelog.txt > docs/changelog.txt.gz
	${gzip} -c9 docs/mzxhelp.html > docs/mzxhelp.html.gz
	${install} -m 0644 \
		README.md \
		docs/changelog.txt.gz \
		docs/mzxhelp.html.gz \
		docs/macro.txt \
		docs/fileform.html \
		docs/keycodes.html \
		docs/joystick.html \
		docs/cycles_and_commands.txt \
		${DESTDIR}${SHAREDIR}/doc/megazeux/
	${install} -m 0644 \
		LICENSE \
		arch/LICENSE.3rd \
		${EXTRA_LICENSES} \
		${DESTDIR}${LICENSEDIR}/megazeux/
	${RM} docs/changelog.txt.gz
	${RM} docs/mzxhelp.html.gz
ifeq (${BUILD_HELPSYS},1)
	${install} -m 0644 assets/help.fil \
		${DESTDIR}${SHAREDIR}/megazeux/assets
endif
ifeq (${BUILD_MODULAR},1)
	${install} -m 0755 -d ${DESTDIR}${LIBDIR}
	${install} -m 0755 ${core_target} ${DESTDIR}${LIBDIR}
	${install} -m 0755 ${editor_target} ${DESTDIR}${LIBDIR}
endif
ifeq (${BUILD_MZXRUN},1)
	${install} -m 0755 -d ${DESTDIR}${GAMESDIR}
	${install} -m 0755 ${mzxrun} ${DESTDIR}${GAMESDIR}/
endif
ifeq (${BUILD_EDITOR},1)
	${install} -m 0755 -d ${DESTDIR}${GAMESDIR}
	${install} -m 0755 ${mzx} ${DESTDIR}${GAMESDIR}/
	${install} -m 0644 \
		assets/ascii.chr \
		assets/blank.chr \
		assets/smzx.chr \
		assets/smzx2.chr \
		${DESTDIR}${SHAREDIR}/megazeux/assets
endif
ifeq (${BUILD_UTILS},1)
	${install} -m 0755 -d ${DESTDIR}${BINDIR}
	${install} -m 0755 \
		${checkres} \
		${downver} \
		${hlp2txt} \
		${txt2hlp} \
		${png2smzx} \
		${ccv} \
		${DESTDIR}${BINDIR}/
endif
ifeq (${BUILD_RENDER_GL_PROGRAM},1)
	${install} -m 0755 -d \
		${DESTDIR}${SHAREDIR}/megazeux/assets/glsl
	${install} -m 0644 assets/glsl/cursor.frag \
		assets/glsl/cursor.vert \
		assets/glsl/mouse.frag \
		assets/glsl/mouse.vert \
		assets/glsl/README.md \
		assets/glsl/scaler.vert \
		assets/glsl/tilemap.frag \
		assets/glsl/tilemap.vert \
		assets/glsl/tilemap.smzx.frag \
		${DESTDIR}${SHAREDIR}/megazeux/assets/glsl/
endif
ifeq (${BUILD_RENDER_GL_PROGRAM},1)
	${install} -m 0755 -d \
		${DESTDIR}${SHAREDIR}/megazeux/assets/glsl/scalers
	${install} -m 0644 \
		assets/glsl/scalers/crt-wave.frag \
		assets/glsl/scalers/crt.frag \
		assets/glsl/scalers/emboss.frag \
		assets/glsl/scalers/epx.frag \
		assets/glsl/scalers/greyscale.frag \
		assets/glsl/scalers/hqscale.frag \
		assets/glsl/scalers/hqscale.vert \
		assets/glsl/scalers/nearest.frag \
		assets/glsl/scalers/semisoft.frag \
		assets/glsl/scalers/sepia.frag \
		assets/glsl/scalers/simple.frag \
		${DESTDIR}${SHAREDIR}/megazeux/assets/glsl/scalers/
endif
ifeq (${BUILD_GAMECONTROLLERDB},1)
	${install} -m 0644 \
		assets/gamecontrollerdb.txt \
		${DESTDIR}${SHAREDIR}/megazeux/assets/
endif
	@echo Done.

#
# Uninstall a build from the system.
#

uninstall: install-check uninstall-arch
	@echo Uninstalling...
	${RM} -r \
		${DESTDIR}${SHAREDIR}/megazeux \
		${DESTDIR}${SHAREDIR}/megazeux/assets \
		${DESTDIR}${SHAREDIR}/doc/megazeux \
		${DESTDIR}${LICENSEDIR}/megazeux \
		${DESTDIR}${SYSCONFDIR}/megazeux-config
ifeq (${BUILD_MODULAR},1)
	${RM} \
		${DESTDIR}${LIBDIR}/${core_target} \
		${DESTDIR}${LIBDIR}/${editor_target}
endif
ifeq (${BUILD_MZXRUN},1)
	${RM} \
		${DESTDIR}${GAMESDIR}/${mzxrun}
endif
ifeq (${BUILD_EDITOR},1)
	${RM} \
		${DESTDIR}${GAMESDIR}/${mzx}
endif
ifeq (${BUILD_UTILS},1)
	${RM} \
		${DESTDIR}${BINDIR}/checkres \
		${DESTDIR}${BINDIR}/downver \
		${DESTDIR}${BINDIR}/hlp2txt \
		${DESTDIR}${BINDIR}/txt2hlp \
		${DESTDIR}${BINDIR}/png2smzx \
		${DESTDIR}${BINDIR}/ccv
endif
	@echo Done.
