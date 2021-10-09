# Deus Console

A C++17 tiny, header only quake/source style console engine allowing for runtime modification of variables and calling of methods. Useful for game engines and GUI tools.

# Features

- Minimal use of exceptions to allow for easy error handling
- Static registering of exposed variables
- Runtime registering of variables and methods as lambda functions
- Console variable flags
- Help/description system
- Retrieve values as specific types

# Getting started

Drop the header into your project through whatever means you like and include it anywhere where you may want to define console variables/methods or process commands:
```c++
#include "deus-console.h"

static TDeusStaticConsoleVariable<int> CVarTestInteger(
  "test.integer",
  123,
  "A test integer variable"
);

int main(int argc, char* argv[]) {
  IDeusConsoleManager* console = IDeusConsoleManager::get();
  int myVar = console->getCVar<int>("test.integer");
  std::cout << "myVar value: " << myVar << std::endl;
  std::cout << "myVar value: " << CVarTestInteger.get() << std::endl;
  std::cout << console->runCommand("help") << std::endl;
}
```

There is no documentation (yet), but the code is pretty simple and self documenting.

# Examples

Currently most of the example code is in `test.cpp`. Plan to expand with more examples if there is interest.

# Testing

There is no extensive test framework or anything used, just simple if/else macros and variable comparison. To compile and run the tests with G++, you would run something like:

```bash
g++ test.cpp && ./a.out add 2 3 4
```

Any arguments will be treated as a command to be processed, for example: `add 2 3 4` will internally call the `add` method with those arguments.
