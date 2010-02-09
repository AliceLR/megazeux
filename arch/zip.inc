ifeq (${SUBARCH},)
SUBARCH := ${SUBPLATFORM}
endif

archive: build
	${RM} -r build/dist/${SUBPLATFORM}
	${MKDIR} -p build/dist/${SUBPLATFORM}
	@cd build/${SUBPLATFORM} && \
	 7za a -tzip ../dist/${SUBPLATFORM}/${TARGET}-${SUBARCH}.zip *
