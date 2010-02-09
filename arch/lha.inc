archive: build
	${RM} -r build/dist/${SUBPLATFORM}
	${MKDIR} -p build/dist/${SUBPLATFORM}
	@cd build/${SUBPLATFORM} && \
	 lha a ../dist/${SUBPLATFORM}/${TARGET}-${SUBPLATFORM}.lha *
