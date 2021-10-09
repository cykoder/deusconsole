#include "deus-console.h"
#include <iostream>

// Test console variables
static TDeusStaticConsoleVariable<const char*> CVarTestCString(
  "test.cstring",
  "mystr",
  "A test C string variable",
  // This should be immutable, as its a const char, it can only change to other const chars otherwise memory issues will occur
  // in order to have dynamic string variables, use std::string like below
  DEUS_CVAR_DEFAULT | DEUS_CVAR_READONLY
);

static TDeusStaticConsoleVariable<std::string> CVarTestString(
  "test.string",
  (std::string)"cppstring",
  "A test string variable"
);

static TDeusStaticConsoleVariable<int> CVarTestInteger(
  "test.integer",
  123,
  "A test integer variable"
);

static TDeusStaticConsoleVariable<float> CVarTestFloat(
  "test.float",
  3.142f,
  "A test float variable"
);

static TDeusStaticConsoleVariable<uint8_t> CVarTestUint(
  "test.uint",
  200,
  "A test uint8_t variable"
);

static TDeusStaticConsoleVariable<bool> CVarTestBool(
  "test.bool",
  true,
  "A test bool variable"
);

// Test helpers
#define expectEqual(what, value, msg) std::cout << msg << ": "; if (what == value) { std::cout << "SUCCESS" << std::endl; } else { std::cout << "FAILED" << std::endl << "Got: " << what << std::endl << "Expected: " << value << std::endl; exit(1); }

// Entry point
int main(int argc, char* argv[]) {
  IDeusConsoleManager* console = IDeusConsoleManager::get();

  // Test default values from static references
  expectEqual(CVarTestUint.get(), 200, "Test uint equals 200");
  expectEqual(CVarTestInteger.get(), 123, "Test integer equals 123");
  expectEqual(CVarTestBool.get(), true, "Test bool equals true");
  expectEqual(CVarTestCString.get(), "mystr", "Test c string equals mystr");
  expectEqual(CVarTestString.get(), "cppstring", "Test string equals cppstring");

  // Test modifying values by static reference (bool)
  CVarTestBool.set(false);
  expectEqual(CVarTestBool.get(), false, "Test bool equals false");

  // Test modifying values by static reference (int)
  CVarTestInteger.set(321);
  expectEqual(CVarTestInteger.get(), 321, "Test integer equals 321");

  // Test modifying values by static reference (c string)
  CVarTestCString.set("hello world");
  expectEqual(CVarTestCString.get(), "hello world", "Test c string equals hello world");

  // Test modifying values by static reference (string)
  CVarTestString.set("hello cpp");
  expectEqual(CVarTestString.get(), "hello cpp", "Test string equals hello cpp");


  // Test reading statically assigned variables from console manager
  expectEqual(console->getCVar<uint8_t>("test.uint"), 200, "Reading uint from console with getCVar");
  expectEqual(console->getCVar<const char*>("test.cstring"), "hello world", "Reading c string from console with getCVar");
  expectEqual(console->getCVar<std::string>("test.string"), "hello cpp", "Reading string from console with getCVar");
  expectEqual(console->getCVar<float>("test.float"), 3.142f, "Reading float from console with getCVar");

  // Test reading statically assigned variables with command strings
  // expectEqual(console->runCommand("test.string"), "hello cpp", "Reading string from with runCommand");
  // expectEqual(console->runCommand("test.uint"), "200", "Reading uint from console with command");
  expectEqual(console->runCommandAs<uint8_t>("test.uint"), 200, "Reading uint from console with command");
  expectEqual(console->runCommandAs<const char*>("test.cstring"), "hello world", "Reading c string from console with command");
  expectEqual(console->runCommandAs<std::string>("test.string"), "hello cpp", "Reading string from console with command");
  expectEqual(console->runCommandAs<float>("test.float"), 3.142f, "Reading float from console with getCVar");

  // Test cannot read variables that dont exist
  bool didThrow = false;
  try {
    console->getCVar<uint8_t>("this.doesnt.exist");
  } catch (DeusConsoleException e) {
    didThrow = true;
  }
  expectEqual(didThrow, true, "Reading non-existant variable throws exception");

  // Test cannot malform string input
  didThrow = false;
  try {
    console->runCommand("test.string invalid string");
  } catch (DeusConsoleException e) {
    didThrow = true;
  }
  expectEqual(didThrow, true, "Cannot malform string input");

  // Test cannot modify constant variable
  didThrow = false;
  try {
    console->runCommand("test.cstring constantchange");
  } catch (DeusConsoleException e) {
    didThrow = true;
  }
  expectEqual(didThrow, true, "Cannot modify constant variable");

  // Changing bool with console commands
  console->runCommand("test.bool true");
  expectEqual(console->getCVar<bool>("test.bool"), true, "Changing bool from console command (true)");
  console->runCommand("test.bool false");
  expectEqual(console->getCVar<bool>("test.bool"), false, "Changing bool from console command (false)");

  // Changing numbers with console commands
  console->runCommand("test.integer 12345");
  expectEqual(console->getCVar<int>("test.integer"), 12345, "Changing integer from console command");

  console->runCommand("test.uint 1");
  expectEqual(console->getCVar<uint8_t>("test.uint"), 1, "Changing uint from console command");
  expectEqual(CVarTestUint.get(), 1, "Changing uint from console command and dereferencing from static var");

  console->runCommand("test.float 4.21");
  expectEqual(console->getCVar<float>("test.float"), 4.21f, "Changing float from console command");

  // Changing strings with console commands
  console->runCommand("test.string consoleiscool");
  expectEqual(console->getCVar<std::string>("test.string"), "consoleiscool", "Changing string to single word from console command");

  console->runCommand("test.string 'this is a string'");
  expectEqual(console->getCVar<std::string>("test.string"), "this is a string", "Changing string to multiple words from console command");

  // Test creating runtime variables
  float myRuntimeVar = 100.0f;
  console->registerCVar("test.runtimefloat", myRuntimeVar, "Runtime variable to test", DEUS_CVAR_DEFAULT);
  expectEqual(console->getCVar<float>("test.runtimefloat"), myRuntimeVar, "Reading runtime float from console with getCVar");

  // Test modifying runtime variables
  console->runCommand("test.runtimefloat 64.0");
  expectEqual(myRuntimeVar, 64.0f, "Modfying runtime float from command");

  // Can get help text for a console variable
  expectEqual(console->getHelp("test.uint"), "A test uint8_t variable", "test.uint help text is correct");
  expectEqual(console->getHelp("test.cstring"), "A test C string variable", "test.cstring help text is correct");

  // Binding and running basic commands
  console->registerMethod("myMethod", [](DeusCommandType& cmd) {
    cmd.returnStr = "returned";
  }, "This description is optional");

  // Run without arguments or expecting a return value
  console->runCommand("myMethod");
  expectEqual(true, true, "Simple myMethod command can be ran without return value");

  // Run without arguments and expecting a return value
  std::string returnValue;
  console->runCommand("myMethod", returnValue);
  expectEqual(returnValue, "returned", "Simple myMethod command returns correct string");

  // Binding and running a more advanced command that takes arguments
  console->registerMethod("add", [](DeusCommandType& cmd) {
    if (cmd.argc <= 1) {
      throw DeusConsoleException("add method requires more than 1 argument");
    }

    int result = 0;
    for (int i = 0; i < cmd.argc; i++) {
      result += cmd.tokens[i].toInt();
    }

    cmd.returnStr = std::to_string(result);
  }, "Adds together a sequence of numbers");
  console->runCommand("add 3 5", returnValue);
  console->runCommand("add 10 20 30", returnValue);
  expectEqual(returnValue, "60", "Advanced add command that takes arguments returns correct value");

  // Test error from within method
  didThrow = false;
  try {
    console->runCommand("add 2");
  } catch (DeusConsoleException e) {
    didThrow = true;
  }
  expectEqual(didThrow, true, "Cannot modify constant variable");

  std::cout << std::endl << "Running base commands..." << std::endl;
  console->bindBaseCommands();
  std::cout << console->runCommand("help") << std::endl;

  // Run user provided command from cli
  if (argc > 1) {
    std::string inputStr;
    for (int i = 1; i < argc; i++) {
      inputStr += (std::string)argv[i];
      if (i < argc - 1) {
        inputStr += " ";
      }
    }

    std::cout << "Running input command: " << inputStr << std::endl;

    try {
      std::cout << console->runCommand(inputStr.c_str()) << std::endl;
    } catch (DeusConsoleException e) {
      std::cout << "Error: " << e.what() << std::endl;
      return 1;
    }
  }

  return 0;
}
