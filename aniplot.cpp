// Elmo Trolla, 2019
// License: pick one - public domain / UNLICENSE (https://www.unlicense.org) / MIT (https://opensource.org/licenses/MIT).

#include <vector>
#include <memory> // required for linux std::shared_ptr
#include <math.h>

#ifdef _WIN32
	#include <direct.h> // getcwd
#else
	#include <unistd.h> // getcwd
#endif

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
#include "protocol.h"


#undef min
#undef max

SDL_Window* window = NULL;

//#define IM_ARRAYSIZE(_ARR)  ((int)(sizeof(_ARR)/sizeof(*_ARR)))

#define INITIAL_SCREEN_WIDTH 1200
#define INITIAL_SCREEN_HEIGHT 700

#define LOG_IMVEC2(vec) SDL_Log(#vec" %.5f %.5f\n", vec.x, vec.y)
#define LOG_IMRECT(rc)  SDL_Log(#rc" min %.5f %.5f max %.5f %.5f\n", rc.Min.x, rc.Min.y, rc.Max.x, rc.Max.y)
#define LOG_PORTAL(rc)  SDL_Log(#rc" min %.5f %.5f max %.5f %.5f\n", rc.min.x, rc.min.y, rc.max.x, rc.max.y)
#define LOG_AACS(cs)    SDL_Log(#cs" pos %.5f %.5f axis %.5f %.5f\n", cs.pos.x, cs.pos.y, cs.xyaxis.x, cs.xyaxis.y)


void fill_graph_with_testdata(GraphChannel& graph)
{
	for(int i = 0; i < 10000; i++) {
		float v = sin(i*M_PI/180.);
		graph.data_channel.append_minmaxavg(v, v, v);
	}
}

typedef std::shared_ptr<GraphWidget>  GraphWidgetPtr;
typedef std::shared_ptr<GraphChannel> GraphChannelPtr;
typedef std::shared_ptr<GraphVisual>  GraphVisualPtr;


// TODO: oh this naming... so wrong.. can't do better on christmas
struct GraphWorldStream {
	GraphWorldStream(): stream_id(0), initialized(false), graph_widget(new GraphWidget) {}
	uint8_t stream_id; // 0 is also valid
	bool initialized; // x,y,x,h have values
	// coordinates on screen, all 4 are 0..1
	float x, y;
	float w, h;
	std::vector<GraphChannelPtr> graph_channels;
	std::vector<GraphVisualPtr>  graph_visuals;
	GraphWidgetPtr graph_widget;
};


// Holds streams.
// A stream can consist of multiple channels.
// Channels can be mapped to different graph_widgets. (well not yet)
// a visual can be set to always anchored, and return to default zoom after some time? (not yet)

// widgetscontainer

class GraphWorld
{
public:
	GraphWorld() {

		_fill_stream_with_channels(0, 1);
		update_stream_info(0,  0, 0, 1, 1); // full window size graph
	}

	GraphWorldStream streams[256];

	inline GraphChannel* get_graph_channel(uint8_t stream_id, uint8_t channel_index_in_stream) {
		// ensure that the wanted channel exists
		_fill_stream_with_channels(stream_id, channel_index_in_stream + 1);
		return streams[stream_id].graph_channels[channel_index_in_stream].get();
	}

	//void clear_stream(uint8_t stream_id) {
	//	// TODO: how??
	//}

	// stream_id : 0..255
	// channel_id : 0..255
	// datatype not implemented
	void update_channel_info(uint8_t stream_id, uint8_t channel_index_in_stream,
			uint8_t* channel_name, uint8_t* channel_unit, uint8_t datatype,
			uint32_t rgba, float line_width,
			float value_min, float value_max,
			float portal_x1, float portal_y1, float portal_x2, float portal_y2) {

		_fill_stream_with_channels(stream_id, channel_index_in_stream + 1);

		GraphWorldStream* stream = &streams[stream_id];

		// TODO: replace the whole channel if datatypes are different

		GraphChannel* channel = stream->graph_channels[channel_index_in_stream].get();
		GraphVisual*  visual  = stream->graph_visuals[channel_index_in_stream].get();

		// keep value_max always larger/equal than value_min
		if (value_min > value_max) {
			float d = value_max;
			value_min = value_max;
			value_max = d;
		}

		if (!visual->initialized) {
			visual->initialized = true;
			float h = value_max - value_min;
			// set height of the portal to contain the whole expected value range plus 10% from top/bottom.
			visual->set_visual_valuespace_mapping(ImRect(0,value_max+h*0.1, 1,value_min-h*0.1));
		}
		//visual->set_visual_valuespace_mapping(ImRect(0., 1., 1., -1));
		visual->line_color = ImColor(rgba);
		visual->line_width = line_width < 0.001f ? 0.001f : line_width;
		// How samplevalues should be mapped to a unit-box in valuespace.
		// ImRect(1, 10,  2, 255) maps samplenum 1 to 0s, sampleval 10 to 0V, samplenum 2 to 1s, sampleval 255 to 1V.
		channel->set_value_samplespace_mapping(ImRect(portal_x1, portal_y1, portal_x2, portal_y2));
		channel->set_value_limits(value_min, value_max);
		channel->name = (char*)channel_name;
		channel->unit = (char*)channel_unit;
	}

	// x, y, w, h : all 0..1
	void update_stream_info(uint8_t stream_id, float x1, float y1, float x2, float y2)
	{
		if (x2 < x1 || y2 < y1) return;

		_fill_stream_with_channels(stream_id, 1);
		streams[stream_id].x = x1;
		streams[stream_id].y = y1;
		streams[stream_id].w = x2 - x1;
		streams[stream_id].h = y2 - y1;
		streams[stream_id].stream_id = stream_id;
		streams[stream_id].initialized = true;
	}

	// make sure stream contains at least num_channels
	void _fill_stream_with_channels(uint8_t stream_id, uint8_t num_channels) {
		while (streams[stream_id].graph_channels.size() < num_channels) {
			GraphChannelPtr channel = GraphChannelPtr(new GraphChannel);
			GraphVisualPtr  visual  = GraphVisualPtr(new GraphVisual(channel.get()));
			streams[stream_id].graph_channels.push_back(channel);
			streams[stream_id].graph_visuals.push_back(visual);
			streams[stream_id].graph_widget->add_graph(visual.get());
		}
	}
};


#ifdef _WIN32
	//#include <iostream>
	#include <winsock2.h>
#else
	#include <sys/socket.h>
	#include <netinet/in.h> // sockaddr_in
	#include <arpa/inet.h>  // inet_addr
#endif


class UdpListener {
public:
	#ifdef _WIN32
		WSADATA _wsa_data;
		SOCKET _socket;
		sockaddr_in _address;
	#else
		int _socket;
		sockaddr_in _address;
	#endif // _WIN32

	bool _initialized;

	UdpListener() {
		uint16_t PORT = 59100;

		_initialized = false;

		#ifdef _WIN32

			if (WSAStartup(MAKEWORD(2, 2), &_wsa_data) != NO_ERROR) {
				SDL_Log("UdpListener: error in WSAStartup\n");
				WSACleanup();
				return;
			}

			_socket = socket(AF_INET, SOCK_DGRAM, 0);
			if (_socket == INVALID_SOCKET) {
				SDL_Log("UdpListener: error creating socket\n");
				WSACleanup();
				return;
			}

			// TODO: clean to zero
			_address.sin_family = AF_INET;
			_address.sin_addr.s_addr = inet_addr("0.0.0.0");
			_address.sin_port = htons(PORT);

			// If iMode!=0, non-blocking mode is enabled.
			u_long iMode = 1;
			ioctlsocket(_socket, FIONBIO, &iMode);

			if (bind(_socket, (SOCKADDR*)&_address, sizeof(_address)) == SOCKET_ERROR) {
				SDL_Log("UdpListener: failed to bind socket\n");
				WSACleanup();
				return;
			}

		#else

			_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
			if (_socket < 0) {
				SDL_Log("UdpListener: error creating socket\n");
				return;
			}

			// zero out the structure
			memset((char*)&_address, 0, sizeof(_address));

			_address.sin_family = AF_INET;
			_address.sin_addr.s_addr = inet_addr("0.0.0.0");
			_address.sin_port = htons(PORT);

			if (bind(_socket, (struct sockaddr*)&_address, sizeof(_address)) < 0) {
				SDL_Log("UdpListener: failed to bind socket\n");
				return;
			}

		#endif // _WIN32

		_initialized = true;
	}

	int _recvfrom(uint8_t* rxbuf, size_t maxlen) {
		sockaddr_in remote_addr;
		// TODO: need to differentiate between win and others? and the server_length stuff - what's that?
		#ifdef _WIN32
			int server_length = sizeof(struct sockaddr_in);
			return recvfrom(_socket, (char*)rxbuf, maxlen, 0, (SOCKADDR*)&remote_addr, &server_length);
		#else
			socklen_t server_length = sizeof(struct sockaddr_in);
			return recvfrom(_socket, (char*)rxbuf, maxlen, MSG_DONTWAIT, (struct sockaddr*)&remote_addr, &server_length);
		#endif
	}

	void tick(GraphWorld& graph_world) {
		if (!_initialized) return;

		uint8_t rxbuf[65535];

		int n = 1;
		while (n > 0) {
			bool packet_decoding_error = false;
			n = _recvfrom(rxbuf, sizeof(rxbuf));
			if (n < 2) break; // ignore packet if doesn't have at least packet_type and packet_version bytes.
			// TODO: rename n -> packet_len

			uint8_t packet_type = rxbuf[0];

			switch (packet_type) {
			case P_CHANNEL_SAMPLES: {

				p_channel_samples *p = (p_channel_samples *) rxbuf;
				if (p->packet_version != 1) {
					SDL_Log("ERROR: got packet P_CHANNEL_SAMPLES version %i but waited for version 1",
					        p->packet_version);
					packet_decoding_error = true;
					break;
				}

				if (n <= sizeof(p_channel_samples)) {
					SDL_Log("ERROR: got packet P_CHANNEL_SAMPLES but no samples in packet");
					packet_decoding_error = true;
					break;
				}

				GraphChannel* graph_channel = graph_world.get_graph_channel(p->stream_id,
				                                                            p->channel_index);
				float* samples = &p->samples[0];
				for (int i = 0; i < p->samples_bytes / 4; i++) {
					float v = samples[i];
					graph_channel->append_sample_minmaxavg(v, v, v);
				}
				break;
			}
			case P_CHANNEL_INFO: {

				p_channel_info* p = (p_channel_info*)rxbuf;
				if (p->packet_version != 2) {
					SDL_Log("ERROR: got packet P_CHANNEL_INFO version %i but waited for version 2",
					        p->packet_version);
					packet_decoding_error = true;
					break;
				}

				graph_world.update_channel_info(p->stream_id, p->channel_index,
				                                p->channel_name, p->unit, p->datatype,
				                                p->line_color_rgba, p->line_width,
				                                p->value_min, p->value_max,
				                                p->portal_x1, p->portal_y1, p->portal_x2, p->portal_y2);
				break;
			}
			case  P_LAYOUT: {

				p_layout* p = (p_layout*)rxbuf;
				if (p->packet_version != 1) {
					SDL_Log("ERROR: got packet P_LAYOUT version %i but waited for version 1",
					        p->packet_version);
					packet_decoding_error = true;
					break;
				}

				if (n < 3) {
					packet_decoding_error = true;
					break;
				}

				if (sizeof(p_layout) + p->num_streams * sizeof(stream_pos_t) != n) {
					SDL_Log("ERROR: got packet P_LAYOUT with wrong length");
					packet_decoding_error = true;
					break;
				}

				for (int i = 0; i < p->num_streams; i++) {
					stream_pos_t& sp = p->stream_pos[i];
					if (sp.x2 < sp.x1 || sp.y2 < sp.y1) {
						SDL_Log("ERROR: got packet P_LAYOUT with rectangle x2 < x1 or y2 < y1");
						packet_decoding_error = true;
						continue;
					}
					graph_world.update_stream_info(sp.stream_id, sp.x1, sp.y1, sp.x2, sp.y2);
				}

				break;
			}
			default: break;
			}


			if (packet_decoding_error) {
				// TODO: print out the hex-encoded packet}
			}
		}

		//sendto(_socket, txbuf, strlen(txbuf), 0, (SOCKADDR*)&remote_Addr, server_length);
	}
};


ImRect get_window_coords() {
	return ImRect(ImGui::GetWindowPos(), ImGui::GetWindowPos()+ImGui::GetWindowSize());
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
	//SDL_Log("Performance counter frequency: %"SDL_PRIu64"", (uint64_t)SDL_GetPerformanceFrequency());

	char dirbuf[10000];
	SDL_Log("wd: %s\n", getcwd(dirbuf, sizeof(dirbuf)));

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

	ImVec4 clear_color = ImColor(128, 128, 128);

	// This is necessary, or objects inside fullsize window are always a bit smaller than the window.
	ImGui::GetStyle().DisplaySafeAreaPadding = ImVec2(0,0);

	ImguiTextwrap textrend;

	GraphWorld graph_world;
	// creates one channel. without it, nothing would be rendered until someone updated some channels.
	GraphChannel* graph_channel = graph_world.get_graph_channel(0, 0);
	graph_channel->name = "default";

	UdpListener windows_udp_listener;

	bool window_hidden = false;

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
			}
			if (event.type == SDL_KEYDOWN) {
				if (event.key.keysym.sym == SDLK_ESCAPE) done = true;
			}
		}

		// Start the Dear ImGui frame
		ImGui_ImplOpenGL3_NewFrame();
		ImGui_ImplSDL2_NewFrame();
		ImGui::NewFrame();

		ImGuiIO &io = ImGui::GetIO();

		windows_udp_listener.tick(graph_world);

		if (!window_hidden) {

			//
			// scan through all graph_world.streams and render what are active
			//

			// if background color is completely transparent, then drawing the background is skipped by imgui.
			// TODO: replace with ImGuiWindowFlags_NoBackground when updating embedded imgui
			ImGui::PushStyleColor(ImGuiCol_WindowBg, IM_COL32(0, 0, 0, 0));

			ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
			ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));
			ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));

			ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0);
			ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 0);
			ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 0);
			ImGui::PushStyleVar(ImGuiStyleVar_GrabMinSize, 0);

			float window_w = ImGui::GetIO().DisplaySize.x;
			float window_h = ImGui::GetIO().DisplaySize.y;
			ImGui::SetNextWindowContentSize(ImVec2(window_w, window_h));
			ImGui::SetNextWindowSize(ImVec2(window_w, window_h));
			ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_Always);

			ImGui::Begin("Robota", NULL,
			             ImGuiWindowFlags_NoTitleBar |
			             ImGuiWindowFlags_NoMove |
			             ImGuiWindowFlags_NoResize |
			             ImGuiWindowFlags_NoCollapse |
			             ImGuiWindowFlags_NoSavedSettings |
			             ImGuiWindowFlags_NoScrollbar);

			for (int i = 0; i < 256; i++) {

				if (!graph_world.streams[i].initialized)
					continue;

				GraphWorldStream& gr = graph_world.streams[i];

				// +1 trick allows to get exact positioning with 1 pixel between widgets and 0 borderpixels.
				float calc_width = ImGui::GetIO().DisplaySize.x + 1;
				float calc_height = ImGui::GetIO().DisplaySize.y + 1;
				float graph_x1 = gr.x * calc_width;
				float graph_y1 = gr.y * calc_height;
				float graph_x2 = gr.x * calc_width + gr.w * calc_width - 1.f;
				float graph_y2 = gr.y * calc_height + gr.h * calc_height - 1.f;
				int graph_w = int(graph_x2 + 0.5) - int(graph_x1 + 0.5);
				int graph_h = int(graph_y2 + 0.5) - int(graph_y1 + 0.5);

				ImGui::SetCursorPos(ImVec2((int)(graph_x1 + 0.5f), (int)(graph_y1 + 0.5f)));

				char temp_window_name[255];
				ImFormatString(temp_window_name, sizeof(temp_window_name), "Robota-%i", i);

				graph_world.streams[i].graph_widget->DoGraph(temp_window_name, ImVec2(graph_w, graph_h));
			}

			//
			// Render fps to the always-on-top imgui layer. This has to the last thing in this
			// window before ImGui::End, or aniplot crashes after some time.
			//

			// have to call this every frame, because the draw list ptr can change
			textrend.init(ImGui::GetIO().Fonts->Fonts[0], ImGui::GetForegroundDrawList());

			char txt[50];
			ImFormatString(txt, sizeof(txt), "fps: %.1f (%.3f ms)", ImGui::GetIO().Framerate,
			               1000.0f / ImGui::GetIO().Framerate);
			ImRect windim = get_window_coords();
			textrend.set_bgcolor(ImColor(80, 80, 80, 100));
			textrend.set_fgcolor(ImColor(255, 255, 255, 100));
			textrend.drawtr(txt, windim.Max.x, windim.Min.y);

			ImGui::End();
			ImGui::PopStyleVar(7);
			ImGui::PopStyleColor(1);
		}

		// testing ground
		if (0 && !window_hidden) {
			// 1.0 ,1.0  2.0 ,2.0  - this draws 1x1 square one pixel away from the corner
			// 1.5 ,1.5  2.0 ,2.0  - this draws 1x1 square one pixel away from the corner
			// 1.51,1.51 2.0 ,2.0  - this draws nothing
			// 1.0 ,1.0  1.51,1.51 - this draws 1x1 square one pixel away from the corner
			// 1.5 ,1.5  1.51,1.51 - this draws 1x1 square one pixel away from the corner

			// 0.51,0.51 2.50,2.50 - this draws 1x1 square one pixel away from the corner

			//   /
			// |  .  |  .  |  .  |  .  |  .  |

			// rect : pixel coordinates start from pixel corner, not from pixel center.
			//        (0.5 - 1.5] - rendering starts from second pixel. smaller-equal than 0.5 is first pixel.
			//ImGui::GetWindowDrawList()->AddRectFilled(ImVec2(0.51, 0.51), ImVec2(2.50, 2.50), ImColor(128,128,128,255));

			// text : pixel coordinates start from pixel corner, not from pixel center.
			// [0., 1.) - render starts inside first pixel. 1. is start of the second pixel.
			//ImGui::GetWindowDrawList()->AddText(ImGui::GetWindowFont(), ImGui::GetWindowFont()->FontSize, ImVec2(1., 1.), ImColor(255,255,255,255), txt, NULL, 0, NULL);
		}

		//static bool show_test_window;
		//ImGui::ShowTestWindow(&show_test_window);

		//
		// Finally give the GPU something to do. Clear the screen and render the accumulated vertex buffers.
		//

		if (!window_hidden) {
			ImGui::Render();
			glViewport(0, 0, (int)io.DisplaySize.x, (int)io.DisplaySize.y);
			glClearColor(clear_color.x, clear_color.y, clear_color.z, clear_color.w);
			glClear(GL_COLOR_BUFFER_BIT);
			ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
		} else {
			// NewFrame() now asserts if neither Render or EndFrame have been called. Exposed EndFrame(). Made it legal to call EndFrame() more than one. (#1423)
			// ImGui_ImplSdlGL3_NewFrame calls NewFrame inside, so we need to end frame even when window_hidden
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
