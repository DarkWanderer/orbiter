add_executable(LuaInterpreterTest tests.cc)

set_target_properties( LuaInterpreterTest PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${ORBITER_BINARY_ROOT_DIR}" )

target_include_directories(LuaInterpreterTest
	PUBLIC ${ORBITER_SOURCE_SDK_INCLUDE_DIR}
	PUBLIC ${MODULE_COMMON_DIR}
)

target_link_libraries(LuaInterpreterTest
	${ORBITER_SDK_LIB}
	${LUA_LIB}
	${LUAINTERPRETER_LIB}
    gtest_main
)

add_dependencies(LuaInterpreterTest
	Orbitersdk
	LuaInterpreter
)

include(GoogleTest)
gtest_discover_tests(LuaInterpreterTest)
