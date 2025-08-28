// Code taken from https://github.com/ocornut/imgui/ SDL2+OpenGL3 emscripten example
// and modified to demo the deus console engine with ImGui

#include "../deus-console.h"

#include "imgui.h"
#include "imgui_impl_sdl.h"
#include "imgui_impl_opengl3.h"
#include <stdio.h>
#include <sstream>

#include <SDL.h>
#include <SDL_opengles2.h>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

// Emscripten requires to have full control over the main loop. We're going to store our SDL book-keeping variables globally.
// Having a single function that acts as a loop prevents us to store state in the stack of said function. So we need some location for this.
SDL_Window*     g_Window = NULL;
SDL_GLContext   g_GLContext = NULL;
inline void setGUIStyles();

// We will also declare some demo variables globally so we can keep this program procedural for simplification purposes
// in your real implementation of deus console you will probably want to wrap into a class
static std::vector<std::string> outputStream; // Output from the console engine
static std::vector<std::string> commandHistory; // All user inputs
static char commandBuffer[512]; // User input text
static int historyPos = 0;

// Static variable for imgui font scale
static TDeusStaticConsoleVariable<float> fontScale(
  "imgui.fontScale",
  1.0f,
  "Controls imgui font scaling parameter for the window"
);

// Static variable for imgui font scale
static TDeusStaticConsoleVariable<bool> imguiShowDemo(
  "imgui.showDemo",
  false,
  "Controls showing imgui demo window"
);

// Binds commands for this demo
inline void bindBaseCommands() {
  IDeusConsoleManager* console = IDeusConsoleManager::get();
  console->bindBaseCommands();

  // Toggles the imgui demo window
  console->registerMethod("toggleDemo", [](DeusCommandType& cmd) {
    imguiShowDemo.set(!imguiShowDemo.get());
  }, "Toggles the demo window");

  // Clears the console buffer
  console->registerMethod("clear", [](DeusCommandType& cmd) {
    outputStream.clear();
  }, "Clears the output buffer");

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
}

// Processes a command and outputs the result or error to "outputStream"
inline void processCommand(char* cmd) {
  IDeusConsoleManager* console = IDeusConsoleManager::get();
  std::string cmdStr = std::string(cmd);
  commandHistory.push_back(cmdStr);
  outputStream.push_back((std::string)"> " + cmdStr);
  try {
    std::string returnOutput = console->runCommand(cmd);
    outputStream.push_back(returnOutput);
  } catch (DeusConsoleException e) {
    std::string errorOutput = (std::string)"ERROR: " + e.what();
    outputStream.push_back(errorOutput);
  }
}

// Compare two strings (case-insensitive) up to n characters
inline static int compareStrs(const std::string& s1, const std::string& s2, int n) {
  int d = 0;
  int i = 0;
  while (i < n && (
    d = std::tolower(static_cast<unsigned char>(s2[i])) -
      std::tolower(static_cast<unsigned char>(s1[i]))
    ) == 0 && i < static_cast<int>(s1.size())) {
    ++i;
  }
  return d;
}

// Called when user requests text completion
inline void textCompletionCallback(ImGuiInputTextCallbackData* data) {
  IDeusConsoleManager* console = IDeusConsoleManager::get();
  const char* strEnd = data->Buf + data->CursorPos;
  const char* strStart = data->Buf;

  // Read help table and compare strings with input to see if any match
  std::string candidate;
  DeusConsoleHelpTable& helpTable = console->getHelpTable();
  for (auto it = helpTable.begin(); it != helpTable.end(); it++) {
    if (compareStrs(it->first, strStart, (int)(strEnd - strStart)) == 0) {
      candidate = std::string(it->first);
      break;
    }
  }

  // If candiate was found, insert it at current position
  if (!candidate.empty()) {
    data->DeleteChars((int)(strStart - data->Buf), (int)(strEnd - strStart));
    data->InsertChars(data->CursorPos, candidate.c_str());
    data->InsertChars(data->CursorPos, " ");
  }
}

// Fired when pressing up/down arrows for history
inline void historyCallback(ImGuiInputTextCallbackData* data) {
  const size_t historyCount = commandHistory.size();
  if (historyCount == 0) {
    return;
  }

  if (data->EventKey == ImGuiKey_UpArrow) {
    historyPos--;
    if (historyPos < 0) {
      historyPos = 0;
    }
  } else if (data->EventKey == ImGuiKey_DownArrow) {
    historyPos++;
    if (historyPos >= historyCount) {
      historyPos = historyCount - 1;
    }
  }

  data->DeleteChars(0, data->BufTextLen);
  data->InsertChars(0, commandHistory[historyPos].c_str());
  data->SelectAll();
}

// Callback from ImGui when text editing
inline static int ImGuiTextEditCallback(ImGuiInputTextCallbackData* data) {
  if (data->EventFlag == ImGuiInputTextFlags_CallbackCompletion) {
    textCompletionCallback(data);
  } else if (data->EventFlag == ImGuiInputTextFlags_CallbackHistory) {
    historyCallback(data);
  }

  return 0;
}

// Renders ImGui input text field with completion callbacks
inline void renderInput() {
  bool shouldFocus = false;
  ImGui::SetNextItemWidth(-1.0f);

  if (ImGui::InputText(
    "##ConsoleInput",
    commandBuffer,
    IM_ARRAYSIZE(commandBuffer),
    ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_CallbackCompletion | ImGuiInputTextFlags_CallbackHistory,
    &ImGuiTextEditCallback)
  ) {
    // Get input and trim end whitespace
    char* str = commandBuffer;

    // Process the command buffer, obviously
    processCommand(str);

    // Clear input
    strcpy(str, "");
    shouldFocus = true;
    historyPos = commandHistory.size() - 1;
  }

  // Auto-focus on window apparition
  ImGui::SetItemDefaultFocus();
  if (shouldFocus) { // Auto focus previous widget
    ImGui::SetKeyboardFocusHere(-1);
  }
}

// Renders output strings into imgui text and performs auto scrolling
inline void renderOutput() {
  const size_t lineCount = outputStream.size();
  for (size_t i = 0; i < lineCount; i++) {
    std::string& line = outputStream[i];
    ImGui::Text(line.c_str());
  }

  if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY()) {
    ImGui::SetScrollHereY(1.0f);
  }
}

// Not sure what to say, its a main loop?
static void main_loop(void* arg) {
  IDeusConsoleManager* console = IDeusConsoleManager::get();
  ImGuiIO& io = ImGui::GetIO();
  IM_UNUSED(arg); // We can pass this argument as the second parameter of emscripten_set_main_loop_arg(), but we don't use that.

  // Our state (make them static = more or less global) as a convenience to keep the example terse.
  static ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);

  // Poll and handle events (inputs, window resize, etc.)
  // You can read the io.WantCaptureMouse, io.WantCaptureKeyboard flags to tell if dear imgui wants to use your inputs.
  // - When io.WantCaptureMouse is true, do not dispatch mouse input data to your main application.
  // - When io.WantCaptureKeyboard is true, do not dispatch keyboard input data to your main application.
  // Generally you may always pass all inputs to dear imgui, and hide them from your application based on those two flags.
  SDL_Event event;
  while (SDL_PollEvent(&event)) {
    #ifndef __EMSCRIPTEN__
    if (event.type == SDL_QUIT) {
      SDL_Quit();
      exit(0);
      return;
    }
    #endif

    ImGui_ImplSDL2_ProcessEvent(&event);
  }

  // Start the Dear ImGui frame
  ImGui_ImplOpenGL3_NewFrame();
  ImGui_ImplSDL2_NewFrame();
  ImGui::NewFrame();

  // Begin console window
  ImGui::SetNextWindowSize(ImVec2(600, 400), ImGuiCond_FirstUseEver);
  ImGui::Begin("ImGui Console Example");
  ImGui::SetWindowFontScale(fontScale.get());

  // Begin output scrolling region
  const float resFooterHeight = ImGui::GetFrameHeightWithSpacing() + ImGui::GetStyle().ItemSpacing.y;
  ImGui::BeginChild("OutputRegion", ImVec2(0, -resFooterHeight), false, ImGuiWindowFlags_HorizontalScrollbar);
  if (ImGui::BeginPopupContextWindow()) {
    if (ImGui::Selectable("Clear")) {
      outputStream.clear();
    }
    ImGui::EndPopup();
  }

  // Render console lines as text
  renderOutput();

  // End scrolling region
  ImGui::EndChild();

  // Render input text field and process commands
  renderInput();

  // End console window
  ImGui::End();

  // Show demo if selected
  if (imguiShowDemo.get()) {
    ImGui::ShowDemoWindow();
  }

  // Rendering
  ImGui::Render();
  SDL_GL_MakeCurrent(g_Window, g_GLContext);
  glViewport(0, 0, (int)io.DisplaySize.x, (int)io.DisplaySize.y);
  glClearColor(clear_color.x * clear_color.w, clear_color.y * clear_color.w, clear_color.z * clear_color.w, clear_color.w);
  glClear(GL_COLOR_BUFFER_BIT);
  ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
  SDL_GL_SwapWindow(g_Window);
}

// Initialize SDL/GL/window and start rendering
// you can ignore most of this method for understanding how deusconsole works
int main(int, char**) {
  // Bind test console commands
  bindBaseCommands();

  // Run first "help" command to output something
  processCommand((char*)"help");

  // Setup SDL
  if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_GAMECONTROLLER) != 0) {
    printf("Error: %s\n", SDL_GetError());
    return -1;
  }

  // For the browser using Emscripten, we are going to use WebGL1 with GL ES2. See the Makefile. for requirement details.
  // It is very likely the generated file won't work in many browsers. Firefox is the only sure bet, but I have successfully
  // run this code on Chrome for Android for example.
  const char* glsl_version = "#version 100";
  //const char* glsl_version = "#version 300 es";
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, 0);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);

  // Create window with graphics context
  SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
  SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
  SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
  SDL_DisplayMode current;
  SDL_GetCurrentDisplayMode(0, &current);
  SDL_WindowFlags window_flags = (SDL_WindowFlags)(SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
  g_Window = SDL_CreateWindow("Deus Console ImGui example", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 1280, 720, window_flags);
  g_GLContext = SDL_GL_CreateContext(g_Window);
  if (!g_GLContext) {
    fprintf(stderr, "Failed to initialize WebGL context!\n");
    return 1;
  }
  SDL_GL_SetSwapInterval(1); // Enable vsync

  // Setup Dear ImGui context
  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGuiIO& io = ImGui::GetIO(); (void)io;

  // For an Emscripten build we are disabling file-system access, so let's not attempt to do a fopen() of the imgui.ini file.
  // You may manually call LoadIniSettingsFromMemory() to load settings from your own storage.
  io.IniFilename = NULL;

  // Setup Dear ImGui style
  ImGui::StyleColorsDark();
  //ImGui::StyleColorsClassic();
  setGUIStyles();

  // Setup Platform/Renderer backends
  ImGui_ImplSDL2_InitForOpenGL(g_Window, g_GLContext);
  ImGui_ImplOpenGL3_Init(glsl_version);

  #ifdef __EMSCRIPTEN__
  // This function call won't return, and will engage in an infinite loop, processing events from the browser, and dispatching them.
  emscripten_set_main_loop_arg(main_loop, NULL, 0, true);
  #else
  while (true) {
    main_loop(nullptr);
  }
  #endif
}

// Misc styling
inline void setGUIStyles() {
	ImGuiStyle* style = &ImGui::GetStyle();
	ImVec4* colors = style->Colors;

  colors[ImGuiCol_Text]                   = ImVec4(1.00f, 1.00f, 1.00f, 1.00f);
  colors[ImGuiCol_TextDisabled]           = ImVec4(0.50f, 0.50f, 0.50f, 1.00f);
  colors[ImGuiCol_WindowBg]               = ImVec4(0.10f, 0.10f, 0.10f, 1.00f);
  colors[ImGuiCol_ChildBg]                = ImVec4(0.28f, 0.28f, 0.28f, 0.00f);
  colors[ImGuiCol_PopupBg]                = ImVec4(0.15f, 0.15f, 0.15f, 1.00f);
  colors[ImGuiCol_Border]                 = ImVec4(0.19f, 0.19f, 0.19f, 1.00f);
  colors[ImGuiCol_BorderShadow]           = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
  colors[ImGuiCol_FrameBg]                = ImVec4(0.00f, 0.00f, 0.00f, 0.13f);
  colors[ImGuiCol_FrameBgHovered]         = ImVec4(0.20f, 0.20f, 0.20f, 1.00f);
  colors[ImGuiCol_FrameBgActive]          = ImVec4(0.28f, 0.28f, 0.28f, 1.00f);
  colors[ImGuiCol_TitleBg]                = ImVec4(0.10f, 0.10f, 0.10f, 1.00f);
  colors[ImGuiCol_TitleBgActive]          = ImVec4(0.15f, 0.15f, 0.15f, 1.00f);
  colors[ImGuiCol_TitleBgCollapsed]       = ImVec4(0.15f, 0.15f, 0.15f, 1.00f);
  colors[ImGuiCol_MenuBarBg]              = ImVec4(0.08f, 0.08f, 0.08f, 1.00f);
  colors[ImGuiCol_ScrollbarBg]            = ImVec4(0.16f, 0.16f, 0.16f, 0.00f);
  colors[ImGuiCol_ScrollbarGrab]          = ImVec4(0.28f, 0.28f, 0.28f, 1.00f);
  colors[ImGuiCol_ScrollbarGrabHovered]   = ImVec4(0.30f, 0.30f, 0.30f, 1.00f);
  colors[ImGuiCol_ScrollbarGrabActive]    = ImVec4(1.00f, 0.37f, 0.17f, 1.00f);
  colors[ImGuiCol_CheckMark]              = ImVec4(1.00f, 0.37f, 0.17f, 1.00f);
  colors[ImGuiCol_SliderGrab]             = ImVec4(0.39f, 0.39f, 0.39f, 1.00f);
  colors[ImGuiCol_SliderGrabActive]       = ImVec4(1.00f, 0.37f, 0.17f, 1.00f);
  colors[ImGuiCol_Button]                 = ImVec4(1.00f, 1.00f, 1.00f, 0.00f);
  colors[ImGuiCol_ButtonHovered]          = ImVec4(1.00f, 0.37f, 0.17f, 0.25f);
  colors[ImGuiCol_ButtonActive]           = ImVec4(1.00f, 0.37f, 0.17f, 1.00f);
  colors[ImGuiCol_Header]                 = ImVec4(0.75f, 0.75f, 0.75f, 0.25f);
  colors[ImGuiCol_HeaderHovered]          = ImVec4(1.00f, 0.37f, 0.17f, 0.25f);
  colors[ImGuiCol_HeaderActive]           = ImVec4(1.00f, 0.37f, 0.17f, 1.00f);
  colors[ImGuiCol_Separator]              = ImVec4(0.19f, 0.19f, 0.19f, 1.00f);
  colors[ImGuiCol_SeparatorHovered]       = ImVec4(0.39f, 0.39f, 0.39f, 1.00f);
  colors[ImGuiCol_SeparatorActive]        = ImVec4(1.00f, 0.39f, 0.00f, 1.00f);
  colors[ImGuiCol_ResizeGrip]             = ImVec4(1.00f, 1.00f, 1.00f, 0.25f);
  colors[ImGuiCol_ResizeGripHovered]      = ImVec4(1.00f, 1.00f, 1.00f, 0.67f);
  colors[ImGuiCol_ResizeGripActive]       = ImVec4(1.00f, 0.37f, 0.17f, 1.00f);
  colors[ImGuiCol_Tab]                    = ImVec4(0.15f, 0.15f, 0.15f, 1.00f);
  colors[ImGuiCol_TabHovered]             = ImVec4(0.24f, 0.24f, 0.24f, 1.00f);
  colors[ImGuiCol_TabActive]              = ImVec4(0.24f, 0.24f, 0.24f, 1.00f);
  colors[ImGuiCol_TabUnfocused]           = ImVec4(0.15f, 0.15f, 0.15f, 1.00f);
  colors[ImGuiCol_TabUnfocusedActive]     = ImVec4(0.24f, 0.24f, 0.24f, 1.00f);
  colors[ImGuiCol_PlotLines]              = ImVec4(1.00f, 0.37f, 0.17f, 1.00f);
  colors[ImGuiCol_PlotLinesHovered]       = ImVec4(1.00f, 0.39f, 0.00f, 1.00f);
  colors[ImGuiCol_PlotHistogram]          = ImVec4(0.59f, 0.59f, 0.59f, 1.00f);
  colors[ImGuiCol_PlotHistogramHovered]   = ImVec4(1.00f, 0.39f, 0.00f, 1.00f);
  colors[ImGuiCol_TableHeaderBg]          = ImVec4(0.19f, 0.19f, 0.20f, 1.00f);
  colors[ImGuiCol_TableBorderStrong]      = ImVec4(0.31f, 0.31f, 0.35f, 1.00f);
  colors[ImGuiCol_TableBorderLight]       = ImVec4(0.23f, 0.23f, 0.25f, 1.00f);
  colors[ImGuiCol_TableRowBg]             = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
  colors[ImGuiCol_TableRowBgAlt]          = ImVec4(1.00f, 1.00f, 1.00f, 0.06f);
  colors[ImGuiCol_TextSelectedBg]         = ImVec4(1.00f, 0.37f, 0.17f, 0.25f);
  colors[ImGuiCol_DragDropTarget]         = ImVec4(1.00f, 0.39f, 0.00f, 1.00f);
  colors[ImGuiCol_NavHighlight]           = ImVec4(1.00f, 0.39f, 0.00f, 1.00f);
  colors[ImGuiCol_NavWindowingHighlight]  = ImVec4(1.00f, 0.39f, 0.00f, 1.00f);
  colors[ImGuiCol_NavWindowingDimBg]      = ImVec4(0.00f, 0.00f, 0.00f, 0.59f);
  colors[ImGuiCol_ModalWindowDimBg]       = ImVec4(0.00f, 0.00f, 0.00f, 0.59f);


	style->ChildRounding = 4.0f;
	style->FrameBorderSize = 1.0f;
	style->FrameRounding = 4.0f;
	style->PopupRounding = 4.0f;
	style->ScrollbarRounding = 12.0f;
	style->WindowRounding = 4.0f;
	style->GrabMinSize = 16.0f;
	style->ScrollbarSize = 12.0f;
	style->TabBorderSize = 0.0f;
	style->TabRounding = 0.0f;
	style->WindowPadding = ImVec2(6.0, 6.0);
	style->FramePadding = ImVec2(10.5, 10.5);
	style->CellPadding = ImVec2(10.5, 10.5);
	style->ItemSpacing = ImVec2(10.0, 8.0);
	style->ItemInnerSpacing = ImVec2(0.0, 6.0);
  style->IndentSpacing = 12.0f;
  style->WindowBorderSize = 1.0f;
  style->ChildBorderSize = 1.0f;
  style->PopupBorderSize = 1.0f;
  style->GrabRounding = 4.0f;
  style->WindowMenuButtonPosition = ImGuiDir_None;
  style->WindowTitleAlign = ImVec2(0.5, 0.5);
}
