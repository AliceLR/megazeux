ifeq (${SUBARCH},)
SUBARCH := ${SUBPLATFORM}
endif

archive: build
	${RM} -r build/dist/${SUBPLATFORM}
	${MKDIR} -p build/dist/${SUBPLATFORM}
	@cd build/${SUBPLATFORM} && \
	if command -v 7za >/dev/null 2>/dev/null; then \
		7za a -mx9 -tzip "../dist/${SUBPLATFORM}/${TARGET}-${SUBARCH}.zip" *; \
	else \
		zip -r9 "../dist/${SUBPLATFORM}/${TARGET}-${SUBARCH}.zip" *; \
	fi
