/*
 * Copyright 2021-2025 Samuel Hellawell. All rights reserved.
 * License: https://github.com/cykoder/deusconsole/blob/master/LICENSE
 */

#ifndef DEUS_CONSOLE_MGR
#define DEUS_CONSOLE_MGR

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
#include <cstdint>

#define TEXT(txt) txt \

// Base exception helper
struct DeusConsoleException : public std::exception {
   std::string s;
   DeusConsoleException(const std::string& ss) : s(ss) {}
   ~DeusConsoleException() throw () {}
   const char* what() const throw() {
     return s.c_str();
   }
};

// Flags that can be set on defined console variables
enum EDeusCVarFlags {
  DEUS_CVAR_DEFAULT      = 0, // Default, no flags are set, the value is set by the constructor
  DEUS_CVAR_DEVELOPER    = (1 << 1), // Console variables marked with this flag can't be changed in a final build
  DEUS_CVAR_READONLY     = (1 << 2), // Console variables cannot be changed by the user
  DEUS_CVAR_UNREGISTERED = (1 << 3), // doesn't get registered to console manager
};

// Flags that can be set on defined console variables
enum EDeusVarTypeFlags {
  DEUS_VARTYPE_STRING     = 0,
  DEUS_VARTYPE_INT        = 1,
  DEUS_VARTYPE_DEC        = 2,
  DEUS_VARTYPE_BOOL_FALSE = 3,
  DEUS_VARTYPE_BOOL_TRUE  = 4,
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
typedef std::function<std::string()> TDeusConsoleFuncToString;
typedef std::function<void*()> TDeusConsoleFuncRead;
typedef std::function<void(void*)> TDeusConsoleFuncVoid;
typedef std::function<void(char*)> TDeusConsoleFuncWriteChar;
typedef std::unordered_map<const char*, const char*> DeusConsoleHelpTable;

// Wrapper for console variables and their flags/methods
struct DeusConsoleVariable {
  TDeusConsoleFuncVoid write;
  TDeusConsoleFuncWriteChar writeIntFromBuffer;
  TDeusConsoleFuncWriteChar writeDecimalFromBuffer;
  TDeusConsoleFuncRead read;
  TDeusConsoleFuncVoid onUpdate;
  TDeusConsoleFuncToString toString;
  int flags;
};

// Checks if an input buffer of n length could be a numeric string
inline int isNumericStr(char* str, size_t len) {
  if (len == 0) {
    return 0;
  }
  bool hasPeriod = false;
  for (size_t i = 0; i < len; i++) {
    char cChar = str[i];
    if (cChar == '.') { // check if is a decimal number
      if (hasPeriod) {
        return 0; // cannot be a numeric string with two periods
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

// Trim all whitespace from string
// taken from https://stackoverflow.com/questions/122616/how-do-i-trim-leading-trailing-whitespace-in-a-standard-way
inline static void trimStr(char* str) {
  char *end;

  // Trim leading space
  while(isspace((unsigned char)*str)) str++;

  if(*str == 0)  // All spaces?
    return;

  // Trim trailing space
  end = str + strlen(str) - 1;
  while(end > str && isspace((unsigned char)*end)) end--;

  // Write new null terminator character
  end[1] = '\0';
}

// Default helper implementation to convert type to string
// if you register custom types, you will need to create an override
// like below. See std::string and const char* representations
template <typename T>
struct TConsoleTypeHelper {
  static std::string toString(T& val) {
    return std::to_string(val);
  }
};

// Convert c string to std::string
template <>
struct TConsoleTypeHelper<const char*> {
  static std::string toString(const char* val) {
    return std::string(val);
  }
};

// Convert std::string to std::string
template <>
struct TConsoleTypeHelper<std::string> {
  static std::string toString(std::string& val) {
    return val;
  }
};

// Class to manage all console variables and commands
// Does not do any input processing
class IDeusConsoleManager {
  private:
    std::unordered_map<std::string, DeusConsoleVariable> variableTable;
    std::unordered_map<std::string, TDeusConsoleFunc> methodTable;
    std::unordered_map<std::string, std::string> helpTable;

    // Gets a variable reference by name
    DeusConsoleVariable& getVariable(const std::string& name) {
      auto it = this->variableTable.find(name);
      if (it != this->variableTable.end()) {
        return it->second;
      }
      throw DeusConsoleException("Console variable does not exist: " + name);
    }

    // Gets a method function object reference by name
    TDeusConsoleFunc& getMethod(const std::string& name) {
      auto it = this->methodTable.find(name);
      if (it != this->methodTable.end()) {
        return it->second;
      }
      throw DeusConsoleException("Console method does not exist: " + name);
    }

  public:
    IDeusConsoleManager() {};

    // Binds base commands that may be useful, call as an initializer
    void bindBaseCommands() {
      this->registerMethod("help", [this](DeusCommandType& cmd) {
        std::string result = "Method/variable list:\n";
        for (auto& kv : this->helpTable) {
          result += kv.first + "\t\t" + kv.second + "\n";
        }
        cmd.returnStr = result;
      }, "Returns a list of variables/methods and their descriptions");
    }

    // Returns a reference to the help table itself, useful for iterating over potential cmds
    std::unordered_map<std::string, std::string>& getHelpTable() {
      return this->helpTable;
    }

    // Returns help text for a specific variable or method
    std::string getHelp(const std::string& key) {
      auto it = this->helpTable.find(key);
      if (it != this->helpTable.end()) {
        return it->second.c_str();
      }
      return nullptr;
    }

    // Checks whether a variable with that name exists in the table
    bool variableExists(const std::string& name) {
      return this->variableTable.find(name) != this->variableTable.end();
    }

    // Checks whether a method with that name exists in the table
    bool methodExists(const std::string& name) {
      return this->methodTable.find(name) != this->methodTable.end();
    }

    // Registers a void function object that takes DeusCommandType as its only argument
    void registerMethod(const std::string& name, TDeusConsoleFunc func, const std::string& description = "") {
      if (this->methodTable.find(name) == this->methodTable.end()) {
        this->methodTable[name] = func;
        this->helpTable[name] = description;
      }
    }

    // This method will take a name, value reference, help description and flags for a console variable
    // and will assign it to the various tables required for reading/writing/remembering arguments
    template <typename T>
    void registerCVar(const std::string& name, T& value, const std::string& description = "", int flags = DEUS_CVAR_DEFAULT, TDeusConsoleFuncVoid onUpdate = nullptr) {
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
        variable.onUpdate = onUpdate;
        variable.toString = [&value]() {
          return TConsoleTypeHelper<T>::toString(value);
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
        T tokenValue = static_cast<T>(atof(data));
        value = tokenValue;
      };
      variable.writeIntFromBuffer = [&value](char* data) {
        T tokenValue = static_cast<T>(atol(data));
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
      variable.writeIntFromBuffer = [&value](char* data) {
        value = T(data);
      };
      variable.writeDecimalFromBuffer = [&value](char* data) {
        value = T(data);
      };
    }

    // This method will take the ptr of the value and cast to its native type as a reference
    template <typename T>
    T& getCVar(const std::string& name) {
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

      // Trim whitespace from command
      trimStr(command);

      // Split string into tokens
      const char* whitespaceStr = " ";
      bool isStringParsing = false;
      char* cmdToken = strtok(command, whitespaceStr);
      DeusCommandToken commandToken;
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
            const bool isStringStart = !isStringParsing && (cmdToken[0] == '\'' || cmdToken[0] == '"');
            const bool isStringEnd = isStringParsing && (cmdToken[tokenLength - 1] == '\'' || cmdToken[tokenLength - 1] == '"');

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
      std::string cmdTarget = commandResult.target;
      const bool methodExists = this->methodExists(cmdTarget);

      // Check if target is a variable to write/read
      if (this->variableExists(cmdTarget)) {
        if (commandResult.argc == 0) { // Zero tokens is a read op
          // TODO: FIX: Reading variable doesn't add its value to returnStr
          DeusConsoleVariable& variable = this->getVariable(cmdTarget);
          commandResult.returnStr = variable.toString();
          return this->getCVar<T>(cmdTarget);
        } else if (commandResult.argc == 1) { // One token is write op
          // Get the variable for our intended target
          DeusConsoleVariable& variable = this->getVariable(cmdTarget);

          // Disallow writing to constants
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
            variable.writeIntFromBuffer(tokenInput);
          } else if (tokenType == DEUS_VARTYPE_BOOL_TRUE) {
            variable.writeIntFromBuffer(tokenInput);
          } else { // Should never happen
            assert(false);
          }

          // Fire on update hook
          if (variable.onUpdate) {
            variable.onUpdate(variable.read());
          }

          // Return new value
          return static_cast<T>(this->getCVar<T>(cmdTarget));
        } else if (!methodExists) { // More than 1 token is a no-op on a variable
          throw DeusConsoleException("Too many arguments");
        }
      }

      // Check if a method exists, since variable read/write didn't pass
      if (methodExists) {
        TDeusConsoleFunc& method = this->getMethod(cmdTarget);
        method(commandResult);
      } else {
        throw DeusConsoleException("No variable or method found: " + cmdTarget);
      }

      return T{};
    }

    // This method will take a command string and return its result typecasted to the supplied type
    // the supplied command must be a single command only, line pre-processing would be done at another step
    template <typename T>
    T runCommandAs(const char* command) {
      DeusCommandType commandResult;
      return this->runCommandAs<T>(command, commandResult);
    }

    // Return static console ref as a pointer for runtime usage
    static IDeusConsoleManager* get() {
      // Static console member, it has to be static to support TDeusStaticConsoleVariable static defs
      static IDeusConsoleManager deusStaticConsole;
      return &deusStaticConsole;
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
    TDeusStaticConsoleVariable(const char* name, T value, const char* description = "", int flags = DEUS_CVAR_DEFAULT, TDeusConsoleFuncVoid onUpdate = NULL) {
      this->rawValue = value;
      IDeusConsoleManager::get()->registerCVar(name, this->rawValue, description, flags, onUpdate);
    }

    void set(T value) {
      this->rawValue = value;
    }

    T& get() {
      return this->rawValue;
    }
};

#endif
