// Elmo Trolla, 2019
// License: pick one - public domain / UNLICENSE (https://www.unlicense.org) / MIT (https://opensource.org/licenses/MIT).

#pragma once


#include <string>

#include "imgui.h"

#include "mip_buf_t.h"
#include "imgui_textwrap.h"


#undef min
#undef max


enum GraphVisualFlags_ {
	GraphVisualFlags_DrawLeftGridLegend       = 1 << 0, // value axis legend
	GraphVisualFlags_DrawRightGridLegend      = 1 << 1, // value axis legend
	GraphVisualFlags_DrawTopGridLegend        = 1 << 2, // time axis legend
	GraphVisualFlags_DrawBottomGridLegend     = 1 << 3, // time axis legend
	GraphVisualFlags_DrawHorizontalGrid       = 1 << 4,
	GraphVisualFlags_DrawVerticalGrid         = 1 << 5,
	GraphVisualFlags_DrawMinMaxBackground     = 1 << 6,
	//GraphVisualFlags_DrawTopScrollbar         = 1 << 7, // scrollbar flags are the only things that shrink the visible graph window size. all other legends are rendered on the graph.
	//GraphVisualFlags_DrawBottomScrollbar      = 1 << 8,
	GraphVisualFlags_GridLegendIsAbsoluteTime = 1 << 9,
	GraphVisualFlags_GridLegendIsRelativeTime = 1 << 10,
};

// TODO: use static inline instead of simply inline? to get rid of some warnings?
//inline ImVec2 operator-(const ImVec2& v)                     { return ImVec2(-v.x, -v.y); } // fdkz:
//inline ImVec2 operator/(const float lhs, const ImVec2& rhs)  { return ImVec2(lhs/rhs.x, lhs/rhs.y); } // fdkz:
//inline ImVec2 operator*(const float lhs, const ImVec2& rhs)  { return ImVec2(lhs*rhs.x, lhs*rhs.y); } // fdkz:
//inline ImVec2& operator*=(ImVec2& lhs, const ImVec2& rhs)    { lhs.x *= rhs.x; lhs.y *= rhs.y; return lhs; } // fdkz:

// like imgui ImVec2, but uses double instead of float.
struct ImVec2d {
	double x, y;
	ImVec2d() { x = y = 0.0f; }
	ImVec2d(double _x, double _y) { x = _x; y = _y; }
	ImVec2d(const ImVec2& v) { x = v.x; y = v.y; }

	//const ImVec2 &toImVec2() { return ImVec2(x, y); };
	ImVec2 toImVec2() { return ImVec2(float(x), float(y)); };
};

inline ImVec2d operator*(const ImVec2d& lhs, const double rhs)      { return ImVec2d(lhs.x*rhs, lhs.y*rhs); }
inline ImVec2d operator/(const ImVec2d& lhs, const double rhs)      { return ImVec2d(lhs.x/rhs, lhs.y/rhs); }
inline ImVec2d operator+(const ImVec2d& lhs, const ImVec2d& rhs)    { return ImVec2d(lhs.x+rhs.x, lhs.y+rhs.y); }
inline ImVec2d operator-(const ImVec2d& lhs, const ImVec2d& rhs)    { return ImVec2d(lhs.x-rhs.x, lhs.y-rhs.y); }
inline ImVec2d operator*(const ImVec2d& lhs, const ImVec2d& rhs)    { return ImVec2d(lhs.x*rhs.x, lhs.y*rhs.y); }
inline ImVec2d operator/(const ImVec2d& lhs, const ImVec2d& rhs)    { return ImVec2d(lhs.x/rhs.x, lhs.y/rhs.y); }
inline ImVec2d operator-(const ImVec2d& v)                          { return ImVec2d(-v.x, -v.y); }
inline ImVec2d operator/(const double lhs, const ImVec2d& rhs)      { return ImVec2d(lhs/rhs.x, lhs/rhs.y); }
inline ImVec2d operator*(const double lhs, const ImVec2d& rhs)      { return ImVec2d(lhs*rhs.x, lhs*rhs.y); }
inline ImVec2d& operator*=(ImVec2d& lhs, const ImVec2d& rhs)        { lhs.x *= rhs.x; lhs.y *= rhs.y; return lhs; }
inline ImVec2d& operator+=(ImVec2d& lhs, const ImVec2d& rhs)        { lhs.x += rhs.x; lhs.y += rhs.y; return lhs; }
inline ImVec2d& operator-=(ImVec2d& lhs, const ImVec2d& rhs)        { lhs.x -= rhs.x; lhs.y -= rhs.y; return lhs; }
inline ImVec2d& operator*=(ImVec2d& lhs, const double rhs)          { lhs.x *= rhs; lhs.y *= rhs; return lhs; }
inline ImVec2d& operator/=(ImVec2d& lhs, const double rhs)          { lhs.x /= rhs; lhs.y /= rhs; return lhs; }
//static inline ImVec4 operator-(const ImVec4& lhs, const ImVec4& rhs)     { return ImVec4(lhs.x-rhs.x, lhs.y-rhs.y, lhs.z-rhs.z, lhs.w-lhs.w); }

// A rectangle with some helper functions to enable coordinatesystem-like behaviour.
// (min, max) is always mapped to (0, 1) of parent space.
struct PortalRect {
	ImVec2d min;
	ImVec2d max;

	PortalRect()                                          : min(0.,0.), max(1.,1.)   {}
	PortalRect(const ImVec2d& _min, const ImVec2d& _max)  : min(_min), max(_max)     {}
	PortalRect(double x1, double y1, double x2, double y2): min(x1, y1), max(x2, y2) {}
	PortalRect(const ImRect& r) : min(r.Min), max(r.Max) {}

	ImVec2d center()                  const { return ImVec2d((min.x+max.x)*0.5f, (min.y+max.y)*0.5f); }
	ImVec2d size()                    const { return ImVec2d(max.x-min.x, max.y-min.y); }
	double  w()                       const { return max.x-min.x; }
	double  h()                       const { return max.y-min.y; }
	bool   contains(const ImVec2d& p) const { return p.x >= min.x  &&  p.y >= min.y  &&  p.x < max.x  &&  p.y < max.y; }
	//ImRect get_imrect()               const { return ImRect(min.x, min.y, max.x, max.y); } // too unprecise

	void translate(ImVec2d delta) { min += delta; max += delta; }

	// ratio 1: no change. ratio 1.3: 100 -> 130. ratio 1/1.3: 130 -> 100. ratio 2: 100 -> 200, ratio 0.5: 200 -> 100
	void resize_by_ratio(const ImVec2d& _center, const ImVec2d& ratio) { min = _center + (min - _center) * ratio; max = _center + (max - _center) * ratio; }
	void clip(const PortalRect& _clip) { if (min.x < _clip.min.x) min.x = _clip.min.x; if (min.y < _clip.min.y) min.y = _clip.min.y; if (max.x > _clip.max.x) max.x = _clip.max.x; if (max.y > _clip.max.y) max.y = _clip.max.y; }

	PortalRect proj_in(const PortalRect& cs)   const { return PortalRect(proj_vin(cs.min), proj_vin(cs.max)); }
	PortalRect proj_out(const PortalRect& cs)  const { return cs.proj_in(*this); }

	// Project the vector from world-space into this coordinate system. (Coordinates between 0..1 appear inside this rectangle.)
	inline ImVec2d proj_vin(const ImVec2d& v)  const { return v * size() + min; }
	inline ImVec2d proj_vout(const ImVec2d& v) const { return (v - min) / size(); }
};

// Contains data about the graph:
//      MipBuf, name, unit, description
//      value_sample_portal : how to project samples from sample-space to value-space. this object resides in valuespace.
struct GraphChannel {
	//GraphChannel(float _sample_freq=1000) { set_value_samplespace_mapping(ImRect(0,0, _sample_freq,1)); }
	GraphChannel();

	MipBuf_t<float> data_channel;
	std::string name; // for the legend
	std::string unit; // displayed on the legend after value
	std::string description;

	// we HAVE to know this to calculate timepoints from sample numbers.
	//float sample_freq;
	// Useful to show data limits.
	float value_min;
	float value_max;
	PortalRect portal; // How to project samples from sample-space to value-space. Resides in sample-space.

	// convenience functions
	inline void append_sample(float v) { data_channel.append(v); }
	inline void append_sample_minmaxavg(float min, float max, float avg) { data_channel.append_minmaxavg(min, max, avg); }

	//void set_portal(PortalRect& _portal) { portal = _portal; } // sample_freq = fabs(_portal.max.x - _portal.min.x);
	void set_value_limits(float min, float max) { value_min = min; value_max = max; }
	// How samplevalues should be mapped to a unit-box in valuespace.
	// ImRect(1, 10,  2, 255) maps samplenum 1 to 0s, sampleval 10 to 0V, samplenum 2 to 1s, sampleval 255 to 1V.
	void set_value_samplespace_mapping(const ImRect& innerspace) { portal = PortalRect(innerspace); }

	// convert coordinates between spaces. for example 1V to raw sample val 255, time 0s to samplenum 0, ..

	// input: (time, value), return: (samplenum, sampleval)
	inline ImVec2d value_to_samplespace(const ImVec2d& value)  const { return portal.proj_vin(value); }
	// input: (samplenum, sampleval), return: (time, value)
	inline ImVec2d sample_to_valuespace(const ImVec2d& sample) const { return portal.proj_vout(sample); }
};

// Contains info about how to draw the data (color, linewidth, background color between min-max values..)
// There can be many GraphVisual objects per GraphChannel, so one GraphChannel can be drawn to many places in many different ways.
struct GraphVisual {
	GraphVisual(GraphChannel* _graph_channel, uint32_t flags=0);

	GraphChannel* graph_channel;  // GraphVisual does NOT take ownership of the graph_channel.
	ImVec4        line_color;
	ImVec4        line_color_minmax;
	ImVec4        minmax_bgcolor; // background that is filled outside of the value_min/value_max area given in GraphChannel. but is unused if value_min == value_max.
	ImVec4        bg_color;
	ImVec4        hor_grid_color;
	ImVec4        hor_grid_text_color;
	ImVec4        hor_grid_text_bgcolor;
	ImVec4        ver_grid_color;
	ImVec4        ver_grid_text_color;
	ImVec4        ver_grid_text_bgcolor;
	ImVec4        grid_legend_color;
	float         grid_min_div_horizontal_pix;
	float         grid_min_div_vertical_pix;
	float         line_width;     // thickness of the data-line in pixels
	bool          anchored;       // newest sample is anchored to the right window edge.
	bool          visible;        // graph line rendered or not.
	uint32_t      flags;          // GraphVisualFlags_
	PortalRect    portal;         // Everything coming out of this portal between 0..1 is visible. Resides in value-space.
	bool          changed;
	bool          initialized;

	// // zoom: visualcenterpos: coordinates on the graph window around which everything changes. usually the coordinates under the mouse. 0..1
	void resize_by_ratio(const ImVec2d& center, const ImVec2d& ratio) { portal.resize_by_ratio(portal.proj_vin(center), ratio); }

	// How should values be mapped to a box with origin 0,0 and size 1,1. 0,0 is top-left corner. Used to set zoom
	// and position, in other words configure how much of the data is currently visible through this GraphVisual.
	// innerspace is a rect of values and time.
	// innerspace ex: ((5.6V, 0s), (1.1V, 60s)) - this shows 1 minute worth of data from start of the recording,
	//                bottom of the window starts from 1.1V and top of the window ends with 5.6V).
	void set_visual_valuespace_mapping(const ImRect& innerspace) { portal = PortalRect(innerspace); }
	//ImRect get_visual_valuespace_mapping() { return portal.get_imrect(); }

	// input: (x, y) where visible x and y are 0..1, return: (time, value)
	inline ImVec2d visual_to_valuespace(const ImVec2d& visual)  { return portal.proj_vin(visual); }
	// input: (time, value), return: (x, y) where visible x and y are 0..1
	inline ImVec2d value_to_visualspace(const ImVec2d& value)   { return portal.proj_vout(value); }

	// convenience functions

	// input: (x, y) where visible x and y are 0..1, return: (samplenum, sampleval)
	inline ImVec2d visual_to_samplespace(const ImVec2d& visual) { return graph_channel->portal.proj_vin( portal.proj_vin(visual) ); }
	// input: (samplenum, sampleval), return: (x, y) where visible x and y are 0..1
	inline ImVec2d sample_to_visualspace(const ImVec2d& sample) { return portal.proj_vout( graph_channel->portal.proj_vout(sample) ); }
};


// Can contain many graphs (Graph objects).
// Handles user interaction (mouse dragging, keyboard movement)
// Animates movements smoothly
struct GraphWidget {
	GraphWidget();
	~GraphWidget();

	// Does NOT take ownership.
	void add_graph(GraphVisual* graph_visual);
	// size in pixels, with scrollbars (if scrollbars are turned on)
	void DoGraph(const char* label, ImVec2 size=ImVec2(0,0));

	ImVector<GraphVisual*> graph_visuals;

private:
	void _render_minmax_background(const PortalRect& screen_value_portal, GraphVisual& graph_visual, const ImRect& canvas_bb);
	// Return value is a number of gridlines to render. One line off the screen on both ends to be able to render half-visible legend.
	// If the return value (num_gridlines) is 0, other values are undefined.
	// min_val is allowed to be larger than max_val and size_pixels is allowed to be negative.
	int  _calculate_gridlines_base2(double min_val, double max_val, double size_pixels, double min_pixels_per_div, double* out_val_begin, double* out_val_end, double* out_val_step);
	void _calculate_gridlines_base10(double min_val, double max_val, double size_pixels, double min_pixels_per_div, double* out_val_begin, double* out_val_end, double* out_val_step, double* out_step_exponent, unsigned char* out_grid_mode);
	void _render_grid_horlines(const PortalRect& screen_value_portal, GraphVisual& graph_visual, const ImRect& canvas_bb);
	void _render_grid_horlegend(const PortalRect& screen_value_portal, GraphVisual& graph_visual, const ImRect& canvas_bb);
	void _render_grid_verlines(const PortalRect& screen_value_portal, GraphVisual& graph_visual, const ImRect& canvas_bb);
	void _render_grid_verlegend(const PortalRect& screen_value_portal, GraphVisual& graph_visual, const ImRect& canvas_bb);
	void _render_legend(const ImRect& canvas_bb);
	void _grid_timestr(double seconds, double step, char* outstr, int outstr_size);
	// draw the squiggly lines and also the min-max value for every sample if the graph is sufficiently zoomed out.
	// canvas_bb : in screenspace, pixels.
	void _draw_graphlines(const PortalRect& screen_sample_portal, GraphVisual& graph_visual, const ImRect& canvas_bb);

	////////

	bool draw_bottom_scrollbar;
	bool draw_top_scrollbar;

	// graph is moving from right to left? newest sample is anchored to the right window edge.
	bool anchored;
	//bool m_dragging;

	ImguiTextwrap* m_textrend;
};
