find_package(Python3 COMPONENTS Interpreter)

# --- C++ unit tests -------------------------------------------------------

add_executable(dcmsg_tests cpp/test_dcmsg.cpp)
target_link_libraries(dcmsg_tests PRIVATE dcmsg::dcmsg)

add_test(NAME cpp_unit_tests COMMAND dcmsg_tests)

# --- Python unit tests ------------------------------------------------------

if(Python3_Interpreter_FOUND)
    add_test(
        NAME python_unit_tests
        COMMAND ${Python3_EXECUTABLE} -m unittest discover -s ${CMAKE_CURRENT_SOURCE_DIR}/python -v
    )

    # --- Cross-language (C++ <-> Python) wire-compatibility test -----------

    add_executable(cross_lang_helper cross_lang/cross_lang_helper.cpp)
    target_link_libraries(cross_lang_helper PRIVATE dcmsg::dcmsg)

    add_test(
        NAME cross_language_wire_compat
        COMMAND ${Python3_EXECUTABLE}
                ${CMAKE_CURRENT_SOURCE_DIR}/cross_lang/test_cross_lang.py
                $<TARGET_FILE:cross_lang_helper>
    )
else()
    message(STATUS "Python3 interpreter not found — skipping Python and cross-language tests")
endif()
