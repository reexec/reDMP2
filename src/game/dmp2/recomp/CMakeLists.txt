if(REDMP2_RECOMP_SRCS)
    add_library(redmp2_recomp STATIC
        ${REDMP2_RECOMP_SRCS}
    )
    target_include_directories(redmp2_recomp PUBLIC
        $<BUILD_INTERFACE:${REDMP2_GENERATED_DIR}>
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/third_party/psprecomp/include>
    )
    target_link_libraries(redmp2_recomp PUBLIC
        redmp2_recomp_abi
    )
    target_compile_definitions(redmp2_recomp PRIVATE PSPRECOMP_LOOP_HOOK)
    if(REDMP2_TRACE_GUEST)
        target_compile_definitions(redmp2_recomp PRIVATE PSPRECOMP_TRACE)
    endif()
    if(MSVC)
        target_compile_options(redmp2_recomp PRIVATE /W0 /MP /utf-8 /bigobj $<$<CONFIG:Release>:/Z7>)
        target_compile_definitions(redmp2_recomp PRIVATE _CRT_SECURE_NO_WARNINGS)
    else()
        target_compile_options(redmp2_recomp PRIVATE -w)
    endif()
endif()
