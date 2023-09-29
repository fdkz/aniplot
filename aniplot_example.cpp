// Elmo Trolla, 2019
// License: pick one - public domain / UNLICENSE (https://www.unlicense.org) / MIT (https://opensource.org/licenses/MIT).

#include <memory> // required for linux std::shared_ptr
#include <math.h>

#define IMGUI_DEFINE_MATH_OPERATORS

#include "imgui.h"
#include "imgui_impl_sdl2.h"
#include "imgui_impl_opengl3.h"
#include "imgui_internal.h" // for custom graph renderer

#include "SDL.h"
#if defined(IMGUI_IMPL_OPENGL_ES2)
	#include "SDL_opengles2.h"
#else
	#include "SDL_opengl.h"
#endif

#include "aniplotlib.h"


SDL_Window* window = NULL;

#define INITIAL_SCREEN_WIDTH 800
#define INITIAL_SCREEN_HEIGHT 600

typedef std::shared_ptr<GraphWidget>  GraphWidgetPtr;
typedef std::shared_ptr<GraphChannel> GraphChannelPtr;
typedef std::shared_ptr<GraphVisual>  GraphVisualPtr;

// graph widget 1
GraphWidget  graph_widget1;
GraphChannel graph_channel1_1;
GraphChannel graph_channel1_2;
GraphVisual  graph_visual1_1(&graph_channel1_1);
GraphVisual  graph_visual1_2(&graph_channel1_2);

// graph widget 2
GraphWidget  graph_widget2;
GraphChannel graph_channel2_1;
GraphVisual  graph_visual2_1(&graph_channel2_1);


void init_graphs()
{
	//
	// widget 1
	//

	graph_widget1.add_graph(&graph_visual1_1);
	graph_widget1.add_graph(&graph_visual1_2);

	graph_visual1_1.line_color = ImColor(1.0f, 1.0f, 0.8f);
	graph_visual1_2.line_color = ImColor(1.0f, 0.5f, 0.5f);
	graph_visual1_1.line_width = 3;

	// How samplevalues should be mapped to a unit-box in valuespace.
	// Map 60 samples to 1 second and keep values unchanged (meaning 0 keeps being 0 and 1 keeps being 1)
	graph_channel1_1.set_value_samplespace_mapping(ImRect(0, 0, 60, 1));
	graph_channel1_2.set_value_samplespace_mapping(ImRect(0, 0, 60, 1));

	// set which slice of the world is initially visible through this visual.
	// from 0..10 seconds, with values -5..+5
	graph_visual1_1.set_visual_valuespace_mapping(ImRect(0, -5, 10, 5));

	// visual guides that indicate min/max possible values
	graph_channel1_1.value_min = -4;
	graph_channel1_1.value_max = 4;
	graph_channel1_1.name = "line1";
	graph_channel1_1.unit = "m/s2";

	graph_channel1_2.name = "line2";

	//
	// widget 2
	//

	// use default values for most of the things.

	graph_widget2.add_graph(&graph_visual2_1);

	graph_visual2_1.line_color = ImColor(0.5f, 1.0f, 0.5f);
	graph_channel2_1.name = "speed";
	graph_channel2_1.unit = "m/s";
	graph_channel2_1.set_value_samplespace_mapping(ImRect(0, 0, 60, 1));
}

void append_samples()
{
	static double t;
	const float dt = 1.f/60;
	t += dt;
	float y1 = (float)(sin(t) * sin(t*1.3) + sin(t*3.4));
	float y2 = (float)(sin(t*12.) * sin(t*1.1) + 2.*sin(t*11.4));
	float y3 = (float)(sin(t*3.1) * sin(t*7.3) + sin(t*9.4));
	//y4 = math.sin(t*8.1) * math.sin(t*14.3) + math.sin(t*2.4)
	graph_channel1_1.append_sample(y1);
	graph_channel1_2.append_sample(y2);
	graph_channel2_1.append_sample(y3);
}

int main(int, char**)
{
	SDL_Log("initializing SDL");
	SDL_version compiled;
	SDL_version linked;

	#ifdef __WIN32__
		// without this, windows just scales the window up if using a hidgpi monitor. horrible.
		// with this, image is pixel-perfect, but text is tiny. sorry..
		SetProcessDPIAware();
	#endif

	SDL_VERSION(&compiled);
	SDL_GetVersion(&linked);

	SDL_Log("Compiled against SDL version %d.%d.%d", compiled.major, compiled.minor, compiled.patch);
	SDL_Log("Linking against SDL version %d.%d.%d.", linked.major, linked.minor, linked.patch);

	//if (SDL_Init(SDL_INIT_VIDEO) != 0) {
	if (SDL_Init(SDL_INIT_EVERYTHING) != 0) {
		SDL_Log("SDL_Init error: %s", SDL_GetError());
		return -1;
	}

	// Decide GL+GLSL versions

	// opengl version 4.1 is max for yosemite. 2015-07-14

	#if defined(IMGUI_IMPL_OPENGL_ES2)
		// GL ES 2.0 + GLSL 100
		const char* glsl_version = "#version 100";
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, 0);
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
	#elif defined(__APPLE__)
		// GL 3.2 Core + GLSL 150
		const char* glsl_version = "#version 150";
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG); // Always required on Mac
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 2);
	#else
		// GL 3.0 + GLSL 130
		const char* glsl_version = "#version 130";
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, 0);
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
	#endif

	// Create window with graphics context

	SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
	SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
	SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);

	window = SDL_CreateWindow(
			"Telemetry",
			SDL_WINDOWPOS_UNDEFINED,
			SDL_WINDOWPOS_UNDEFINED,
			INITIAL_SCREEN_WIDTH,
			INITIAL_SCREEN_HEIGHT,
			SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
	if (window == NULL) {
		SDL_Log("SDL_CreateWindow error: %s\n", SDL_GetError());
		return -1;
	}

	SDL_GLContext gl_context = SDL_GL_CreateContext(window);
	if (gl_context == NULL) {
		SDL_Log("SDL_GL_CreateContext error: %s\n", SDL_GetError());
		return -1;
	}

	SDL_GL_MakeCurrent(window, gl_context);

	SDL_Log("Supported GLSL version is %s.\n", glGetString(GL_SHADING_LANGUAGE_VERSION));

	// Use Vsync, set for late swap tearing.
	if (SDL_GL_SetSwapInterval(-1) < 0) {
		SDL_Log("Warning: Unable to set VSync for late swap tearing! SDL Error: %s\n", SDL_GetError());
	}

	// Setup Dear ImGui context
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO(); (void)io;
	//io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
	//io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls

	// Setup Dear ImGui style
	ImGui::StyleColorsDark();
	//ImGui::StyleColorsClassic();

	// Setup Platform/Renderer backends
	ImGui_ImplSDL2_InitForOpenGL(window, gl_context);
	ImGui_ImplOpenGL3_Init(glsl_version);

	// --------------------------------------------------------------------

	init_graphs();

	bool window_hidden = false;
	int frame_num = -1;

	// Main loop
	bool done = false;
	while (!done) {
		SDL_Event event;
		while (SDL_PollEvent(&event)) {
			ImGui_ImplSDL2_ProcessEvent(&event);
			if (event.type == SDL_QUIT) done = true;

			if (event.type == SDL_WINDOWEVENT) {
				if (event.window.event == SDL_WINDOWEVENT_HIDDEN) {
					//SDL_Log("event SDL_WINDOWEVENT_HIDDEN");
					window_hidden = true;
				}
				if (event.window.event == SDL_WINDOWEVENT_SHOWN) {
					//SDL_Log("event SDL_WINDOWEVENT_SHOWN");
					window_hidden = false;
				}
				if (event.window.event == SDL_WINDOWEVENT_CLOSE && event.window.windowID == SDL_GetWindowID(window)) {
					done = true;
				}
			}
			if (event.type == SDL_KEYDOWN) {
				if (event.key.keysym.sym == SDLK_ESCAPE) done = true;
			}
		}

		// Start the Dear ImGui frame
		ImGui_ImplOpenGL3_NewFrame();
		ImGui_ImplSDL2_NewFrame();
		ImGui::NewFrame();

		if (!window_hidden) {
			frame_num++;
			append_samples();

			ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_FirstUseEver);
			if (frame_num == 0) {
				// Don't want to use the ImGuiCond_FirstUseEver flag because that works if no imgui.ini file exists yet.
				ImGui::SetNextWindowSize(ImVec2(INITIAL_SCREEN_WIDTH - 20, INITIAL_SCREEN_HEIGHT - 20));
			}
			ImGui::Begin("usualwindow", NULL, ImGuiWindowFlags_NoScrollWithMouse);

			ImVec2 size_avail = ImGui::GetContentRegionAvail();
			ImGuiStyle style = ImGui::GetStyle();

			float graph_w = 0; // 0 to fill available space
			float graph_h = (size_avail.y - style.ItemSpacing.y - style.FramePadding.y * 2) / 2;
			if (graph_h > 0) {
				graph_widget1.DoGraph("robota-1", ImVec2(graph_w, graph_h));
				graph_widget2.DoGraph("robota-2", ImVec2(graph_w, graph_h));
			}

			ImGui::End();

			//static bool show_test_window;
			//ImGui::ShowTestWindow(&show_test_window);
		}

		//
		// Finally give the GPU something to do. Clear the screen and render the accumulated vertex buffers.
		//

		if (!window_hidden) {
			ImGuiIO &io = ImGui::GetIO();
			ImVec4 clear_color(0.5, 0.5, 0.5, 1.0);
			ImGui::Render();
			glViewport(0, 0, (int)io.DisplaySize.x, (int)io.DisplaySize.y);
			glClearColor(clear_color.x, clear_color.y, clear_color.z, clear_color.w);
			glClear(GL_COLOR_BUFFER_BIT);
			ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
		} else {
			// NewFrame() now asserts if neither Render or EndFrame have been called. Exposed EndFrame(). Made it legal to call EndFrame() more than one. (#1423)
			// ImGui_ImplSDL2_NewFrame calls NewFrame inside, so we need to end frame even when window_hidden
			ImGui::EndFrame();
		}

		//
		// Swap the backbuffer to monitor. Just a simple SDL_GL_SwapWindow() call for windows and linux,
		// but macos has to have some magic.
		//

		#if defined(__APPLE__)
			// test if 10 last swaps were instantaneous and conclude that on macos the aniplot window is
			// currently occluded and disable all rendering.

			// Without this, as soon as the aniplot window is occluded by a window, cpu usage raises to
			// 100% on macos because why not. SDL_GL_SwapWindow returns immediately. Also something to do
			// with the App Nap feature.
			//
			// If we'd care to be "responsible" here, we'd use the new windowDidChangeOcclusionState API,
			// but to get that notification from the objective c runtime requires a lot of swearing and
			// sacrificing our whole "build" system. yeah, not gonna consider it. The bad side-effect of
			// this is that with vsync turned off, nothing gets rendered at all. Let's wait until SDL2
			// add support for the necessary feature.. glfw seems to have it, but is too big.

			// num of consequent fast SDL_GL_SwapWindow calls, hopefully indicating that the window is occluded.
			static int num_immediate_swaps;
			// num of consequent slow SDL_GL_SwapWindow calls, hopefully indicating a visible window.
			static int num_slow_swaps;

			uint64_t t1 = SDL_GetPerformanceCounter();
			SDL_GL_SwapWindow(window);
			uint64_t t2 = SDL_GetPerformanceCounter();
			float dt = (float)(t2 - t1) / SDL_GetPerformanceFrequency();
			//SDL_Log("dt %f uint32_t %i", dt, uint32_t(t2 - t1));

			// If less than 0.5 milliseconds. strange things may start to happen if cpu usage raises to
			// about 97% (16ms/0.5ms=3.2%, 100-3.2=97). To be tested.. by the clients..
			if (dt > 0.0005) {
				num_immediate_swaps = 0;
				num_slow_swaps++;
			} else {
				num_immediate_swaps++;
				num_slow_swaps = 0;
			}

			// decide if resume rendering or not. with hysteresis.
			if (window_hidden) {
				if (num_slow_swaps > 10) {
					//SDL_Log("window visible");
					window_hidden = false;
				}
			} else {
				if (num_immediate_swaps > 10) {
					//SDL_Log("window hidden");
					window_hidden = true;
				}
			}

			if (window_hidden) {
				// just sleep a bit. handle sdl events, but do no rendering.
				SDL_Delay(10);
			}
		#else

			SDL_GL_SwapWindow(window);

		#endif
	}

	// Cleanup

	ImGui_ImplOpenGL3_Shutdown();
	ImGui_ImplSDL2_Shutdown();
	ImGui::DestroyContext();

	SDL_GL_DeleteContext(gl_context);
	SDL_DestroyWindow(window);
	SDL_Quit();

	return 0;
}
