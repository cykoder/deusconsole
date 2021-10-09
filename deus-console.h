/*
 * Copyright 2021-2021 Samuel Hellawell. All rights reserved.
 * License: https://github.com/SamHellawell/deusconsole/blob/master/LICENSE
 */

#ifndef DEUS_CONSOLE_MGR
#define DEUS_CONSOLE_MGR

#include <iostream>
#include <unordered_map>
#include <deque>
#include <functional>
#include <sstream>
#include <memory>
#include <vector>
#include <exception>
#include <cassert>
#include <string.h>
#include <type_traits>

#define TEXT(txt) txt \

// Base exception helper
struct DeusConsoleException : public std::exception {
   std::string s;
   DeusConsoleException(std::string ss) : s(ss) {}
   ~DeusConsoleException() throw () {}
   const char* what() const throw() {
     return s.c_str();
   }
};

// Flags that can be set on defined console variables
enum EDeusCVarFlags {
  DEUS_CVAR_DEFAULT                     = 0, // Default, no flags are set, the value is set by the constructor
  DEUS_CVAR_DEVELOPER                   = (1 << 1), // Console variables marked with this flag cant be changed in a final build
  DEUS_CVAR_READONLY                    = (1 << 2), // Console variables cannot be changed by the user
  DEUS_CVAR_UNREGISTERED                = (1 << 3), // Doesnt get registered to console manager
};

// Flags that can be set on defined console variables
enum EDeusVarTypeFlags {
  DEUS_VARTYPE_STRING =     0,
  DEUS_VARTYPE_INT =        1,
  DEUS_VARTYPE_DEC =        2,
  DEUS_VARTYPE_BOOL_FALSE = 3,
  DEUS_VARTYPE_BOOL_TRUE =  4,
};

// Parsed command tokens from string input
struct DeusCommandToken {
  char str[512];
  uint8_t type = 0;

  int toInt() {
    return atol(str);
  }

  float toFloat() {
    return atof(str);
  }
};

// The parsed command containing tokens and a string for return value
struct DeusCommandType {
  char target[512];
  size_t argc = 0;
  std::vector<DeusCommandToken> tokens;
  std::string returnStr;
};

// Readability typedefs
typedef std::function<void(DeusCommandType&)> TDeusConsoleFunc;
typedef std::function<void*()> TDeusConsoleFuncRead;
typedef std::function<void(void*)> TDeusConsoleFuncVoid;
typedef std::function<void(char*)> TDeusConsoleFuncWriteInt;

// Wrapper for console variables and their flags/methods
struct DeusConsoleVariable {
  int flags;

  TDeusConsoleFuncWriteInt writeIntFromBuffer;
  TDeusConsoleFuncWriteInt writeDecimalFromBuffer;
  TDeusConsoleFuncVoid write;
  TDeusConsoleFuncRead read;
};

// Checks if an input buffer of n length could be a numeric string
inline int isNumericStr(char* str, size_t len) {
  if (len == 0) {
    return 0;
  }
  bool hasPeriod = false;
  for (int i = 0; i < len; i++) {
    char cChar = str[i];
    if (cChar == '.') { // check if is a decimal number
      if (hasPeriod) {
        return 0; // cannot be a numberic string with two periods
      } else {
        hasPeriod = true;
      }
    }
    if (cChar != '.' && !isdigit(cChar)) {
      return 0;
    }
  }
  return hasPeriod ? 2 : 1;
}

// Class to manage all console variables and commands
// Does not do any input processing
class IDeusConsoleManager {
  private:
    std::unordered_map<const char*, DeusConsoleVariable> variableTable;
    std::unordered_map<const char*, TDeusConsoleFunc> methodTable;
    std::unordered_map<const char*, const char*> helpTable;

    // Gets a variable reference by name
    DeusConsoleVariable& getVariable(const char* name) {
      for (auto kv : this->variableTable) {
        if (strcmp(kv.first, name) == 0) {
          return this->variableTable[kv.first];
        }
      }

      throw DeusConsoleException("Console variable does not exist: " + (std::string)(name));
      return this->variableTable[name];
    }

    // Gets a method function object reference by name
    TDeusConsoleFunc& getMethod(const char* name) {
      for (auto kv : this->methodTable) {
        if (strcmp(kv.first, name) == 0) {
          return this->methodTable[kv.first];
        }
      }

      throw DeusConsoleException("Console method does not exist: " + (std::string)(name));
      return this->methodTable[name];
    }

  public:
    static IDeusConsoleManager* get();
    IDeusConsoleManager() {};

    // Binds base commands that may be useful, call as an initializer
    void bindBaseCommands() {
      this->registerMethod("help", [this](DeusCommandType& cmd) {
        std::string result = "Method/variable list:\n";
        for (auto kv : this->helpTable) {
          result += (std::string)(kv.first) + "\t\t" + (std::string)(kv.second) + "\n";
        }
        cmd.returnStr = result;
      }, "Returns a list of variables/methods and their descriptions");
    }

    // Returns help text for a specific variable or method
    const char* getHelp(const char* key) {
      return this->helpTable[key];
    }

    // Checks whether a variable with that name exists in the table
    bool variableExists(const char* name) {
      for (auto kv : this->variableTable) {
        if (strcmp(kv.first, name) == 0) {
          return true;
        }
      }
      return false;
    }

    // Checks whether a method with that name exists in the table
    bool methodExists(const char* name) {
      for (auto kv : this->methodTable) {
        if (strcmp(kv.first, name) == 0) {
          return true;
        }
      }
      return false;
    }

    // Registers a void function object that takes DeusCommandType as its only argument
    void registerMethod(const char* name, TDeusConsoleFunc func, const char* description = "") {
      if (this->methodTable.find(name) == this->methodTable.end()) {
        this->methodTable[name] = func;
        this->helpTable[name] = description;
      }
    }

    // This method will take a name, value reference, help description and flags for a console variable
    // and will assign it to the various tables required for reading/writing/remembering arguments
    template <typename T>
    void registerCVar(const char* name, T& value, const char* description = "", int flags = DEUS_CVAR_DEFAULT) {
      // Skip registration if flag defined
      if (flags & DEUS_CVAR_UNREGISTERED) {
        return;
      }

      // Don't register if already exists
      if (this->variableTable.find(name) == this->variableTable.end()) {
        DeusConsoleVariable variable;
        variable.flags = flags;
        variable.read = [&value]() {
          return &value;
        };
        if (!(flags & DEUS_CVAR_READONLY)) {
          this->bindWriteMethods(value, variable);
        }
        this->variableTable[name] = variable;
        this->helpTable[name] = description;
      }
    }

    // Write methods for arithmetic types
    template <typename T, std::enable_if_t<std::is_arithmetic_v<std::remove_reference_t<T>>> * = nullptr> inline
    void bindWriteMethods(T& value, DeusConsoleVariable& variable) {
      variable.writeDecimalFromBuffer = [&value](char* data) {
        T tokenValue = atof(data);
        value = tokenValue;
      };
      variable.writeIntFromBuffer = [&value](char* data) {
        T tokenValue = atol(data);
        value = tokenValue;
      };
      variable.write = [&value](void* data) {
        value = *static_cast<T*>(data);
      };
    }

    // Write methods for non-arithmetic types
    template <typename T, std::enable_if_t<!std::is_arithmetic_v<std::remove_reference_t<T>>> * = nullptr> inline
    void bindWriteMethods(T& value, DeusConsoleVariable& variable) {
      variable.write = [&value](void* data) {
        value = *static_cast<T*>(data);
      };
    }

    // This method will take the ptr of the value and cast to its native type as a reference
    template <typename T>
    T& getCVar(const char* name) {
      DeusConsoleVariable& variable = this->getVariable(name);
      TDeusConsoleFuncRead& readFunc = variable.read;
      return *static_cast<T*>(readFunc());
    }

    // Parses an input string by splitting it into tokens by whitespace characters returning
    // a method or variable name, supplied arguments and types for those arguments
    DeusCommandType parseCommand(const char* inputCmd, DeusCommandType& commandResult) {
      if (strlen(inputCmd) >= 256) {
        throw DeusConsoleException("Input command buffer is too large, cannot parse.");
      }

      // Copy input buffer into workable memory
      char command[512];
      strcpy(command, inputCmd);

      // Split string into tokens
      const char* whitespaceStr = " ";
      bool isStringParsing = false;
      char* cmdToken = strtok(command, whitespaceStr);
      DeusCommandToken commandToken;
      size_t parseStrOffset = 0;
      bool isFirstToken = true;
      while (cmdToken != NULL) {
        if (isFirstToken) { // First token is always the target
          strcpy(commandResult.target, cmdToken);
          isFirstToken = false;
        } else {
          // Detect if boolean true/false strings
          const bool isBoolPositive = !isStringParsing && strcmp(cmdToken, "true") == 0;
          const bool isBoolNegative = !isStringParsing && strcmp(cmdToken, "false") == 0;
          if (isBoolPositive) {
            commandToken.type = DEUS_VARTYPE_BOOL_TRUE;
            strcpy(commandToken.str, "1");
          } else if (isBoolNegative) {
            commandToken.type = DEUS_VARTYPE_BOOL_FALSE;
            strcpy(commandToken.str, "0");
          } else {
            const size_t tokenLength = strlen(cmdToken);
            const bool isStringStart = !isStringParsing && cmdToken[0] == '\'';
            const bool isStringEnd = isStringParsing && cmdToken[tokenLength - 1] == '\'';

            // Detect if string is numeric, if not already string parsing
            const int tokenNumericalType = !isStringParsing && !isStringStart && !isStringEnd ? isNumericStr(cmdToken, tokenLength) : 0;
            commandToken.type = tokenNumericalType; // 0 for strings, 1 for int/uint, 2 for double/float

            // Did a previous token start a string sequence?
            // Or is this the start of a new string? Numbers can skip this step
            if (tokenNumericalType) {
              strcpy(commandToken.str, cmdToken);
            } else {
              if (isStringStart) {
                isStringParsing = true;
                strcpy(commandToken.str, &cmdToken[1]); // trim off first quotation
                strcat(commandToken.str, whitespaceStr);
              } else if (isStringEnd) {
                isStringParsing = false;
                strcat(commandToken.str, cmdToken);
                commandToken.str[strlen(commandToken.str)-1] = 0; // pop off last quotation
              } else if (isStringParsing) {
                strcat(commandToken.str, cmdToken);
                strcat(commandToken.str, whitespaceStr);
              } else if (!isStringParsing) {
                // not parsing a string, just copy token value
                strcpy(commandToken.str, cmdToken);
              }
            }
          }

          // Push as an individual token if not parsing a string
          if (!isStringParsing) {
            commandResult.tokens.push_back(commandToken);
          }
        }

        // Onto the next token
        cmdToken = strtok(NULL, whitespaceStr);

        // Dont go over allowed token count
        if (commandResult.argc >= 16) {
          break;
        }
      }

      // Make sure we're not somehow stuck parsing after loop
      assert(isStringParsing == false);
      commandResult.argc = commandResult.tokens.size();
      return commandResult;
    }

    // This method will take a command string and run it, returning result as string
    // the supplied command must be a single command only, line pre-processing would be done at another step
    std::string runCommand(const char* command) {
      std::string result;
      this->runCommand(command, result);
      return result;
    }

    // This method will take a command string and run it, putting returned string into the referenced output string
    // the supplied command must be a single command only, line pre-processing would be done at another step
    void runCommand(const char* command, std::string& outputStr) {
      DeusCommandType commandResult;
      this->runCommandAs<const char*>(command, commandResult);
      outputStr = commandResult.returnStr;
    }

    // This method will take a command string and return its result typecasted to the supplied type
    // the supplied command must be a single command only, line pre-processing would be done at another step
    template <typename T>
    T runCommandAs(const char* command, DeusCommandType& commandResult) {
      this->parseCommand(command, commandResult);
      const char* cmdTarget = (const char*)commandResult.target;
      const bool methodExists = this->methodExists(cmdTarget);

      // Check if target is a variable to write/read
      if (this->variableExists(cmdTarget)) {
        if (commandResult.argc == 0) { // Zero tokens is a read op
          return static_cast<T>(this->getCVar<T>(cmdTarget));
        } else if (commandResult.argc == 1) { // One token is write op
          // Get the variable for our intended target
          DeusConsoleVariable& variable = this->getVariable(cmdTarget);

          // Disallow writing to constants
          // TODO: disallow writing if production mode
          if (variable.flags & DEUS_CVAR_READONLY) {
            throw DeusConsoleException("Cannot write to a constant variable");
          }

          // Gather input buffer and type
          char* tokenInput = commandResult.tokens[0].str;
          const int tokenType = commandResult.tokens[0].type;

          // Write to variable depending on if string, integer or decimal
          if (tokenType == DEUS_VARTYPE_STRING) { // Write string
            std::string tokenStr(tokenInput);
            variable.write(&tokenStr);
          } else if (tokenType == DEUS_VARTYPE_INT || tokenType == DEUS_VARTYPE_DEC) {
            if (tokenType == DEUS_VARTYPE_INT) { // Integer
              variable.writeIntFromBuffer(tokenInput);
            } else if (tokenType == DEUS_VARTYPE_DEC) { // Decimal number
              variable.writeDecimalFromBuffer(tokenInput);
            }
          } else if (tokenType == DEUS_VARTYPE_BOOL_FALSE) {
            std::cout << "DEUS_VARTYPE_BOOL_FALSE " << tokenInput << std::endl;
            variable.writeIntFromBuffer(tokenInput);
          } else if (tokenType == DEUS_VARTYPE_BOOL_TRUE) {
            std::cout << "DEUS_VARTYPE_BOOL_TRUE " << tokenInput << std::endl;
            variable.writeIntFromBuffer(tokenInput);
          } else { // Should never happen
            assert(false);
          }

          // Return new value
          return static_cast<T>(this->getCVar<T>(cmdTarget));
        } else if (!methodExists) { // More than 1 token is a no-op on a variable
          throw DeusConsoleException("Too many arguments");
        }
      }

      // Check if a method exists, since variable read/write didnt pass
      if (methodExists) {
        TDeusConsoleFunc& method = this->getMethod(cmdTarget);
        method(commandResult);
      } else {
        throw DeusConsoleException("No variable or method found");
      }

      return static_cast<T>(NULL);
    }

    // This method will take a command string and return its result typecasted to the supplied type
    // the supplied command must be a single command only, line pre-processing would be done at another step
    template <typename T>
    T runCommandAs(const char* command) {
      DeusCommandType commandResult;
      return this->runCommandAs<T>(command, commandResult);
    }
};

// Helper for statically declared console variables at compile time
// on construction, the static/default value is copied to "rawValue" which
// is then used as a reference throughout the rest of the system
template<typename T>
class TDeusStaticConsoleVariable {
  private:
    T rawValue;

  public:
    TDeusStaticConsoleVariable(const char* name, T value, const char* description = "", int flags = DEUS_CVAR_DEFAULT) {
      this->rawValue = value;
      IDeusConsoleManager::get()->registerCVar(name, this->rawValue, description, flags);
    }

    void set(T value) {
      this->rawValue = value;
    }

    T& get() {
      return this->rawValue;
    }
};


static IDeusConsoleManager deusStaticConsole;

IDeusConsoleManager* IDeusConsoleManager::get() {
  return &deusStaticConsole;
}

#endif
