find_package(Doxygen)
find_package(Python3 COMPONENTS Interpreter)

if(DOXYGEN_FOUND AND Python3_FOUND)
    add_custom_target(lavista-docs
        COMMAND ${Python3_EXECUTABLE} -m mkdocs build
            -f ${CMAKE_SOURCE_DIR}/docs/mkdocs.yml
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/docs
        COMMENT "Building MkDocs site (mkdoxy API + prose)"
    )
    add_custom_target(lavista-docs-serve
        COMMAND ${Python3_EXECUTABLE} -m mkdocs serve
            -f ${CMAKE_SOURCE_DIR}/docs/mkdocs.yml
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/docs
        COMMENT "Serve MkDocs site locally"
    )
endif()
