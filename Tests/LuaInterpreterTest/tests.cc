#include <gtest/gtest.h>
#include "Interpreter.h"

#include <memory>

using std::make_unique;

// Test that interpreter is created and destroyed without exceptions
TEST(LuaInterpreter, TestCreateDestroy) {
	auto interp = make_unique<Interpreter>();
}

TEST(LuaInterpreter, BasicScript) {
	auto interp = make_unique<Interpreter>();
	interp->Initialise();
	auto script = "print('Hello World')";
	auto result = interp->RunChunk(script, strlen(script));
	interp->EndExec();
	ASSERT_FALSE(result);
}