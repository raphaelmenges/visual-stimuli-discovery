#pragma once

#include <Core/Core.hpp>
#include <Core/VisualDebug.hpp>
#include <opencv2/core/types.hpp>
#include <memory>
#include <string>
#include <vector>
#include <functional>

// TODO: make serializable (together with log datum)
// TODO: catch node by type + xpath? to check whether node already exists?
// TODO: visibility attribute for layer?

namespace data
{
	// Types of layer
	enum class LayerType
	{
		None, Root, Fixed
	};

	// Types of input
	enum class InputType
	{
		Move, Click, Gaze
	};

	// Input structures
	class Input
	{
	public:
		Input(InputType type, core::long64 time_ms) : _type(type), _time_ms(time_ms) {}
		virtual ~Input() = 0;
		InputType get_type() const { return _type; }
		core::long64 get_time_ms() const { return _time_ms; }
	private:
		const InputType _type;
		core::long64 _time_ms; // can be used to sort input
	};
	class CoordinateInput : public Input
	{
	public:
		CoordinateInput(InputType type, core::long64 time_ms, int view_x, int view_y) : Input(type, time_ms)
		{
			_view_x = view_x;
			_view_y = view_y;
		};
		virtual ~CoordinateInput() = 0;
		int get_view_x() const { return _view_x; }
		int get_view_y() const { return _view_y; }
	private:
		// Store coordinates in viewport space as scrolling etc. might be refined later
		int _view_x;
		int _view_y;
	};
	class MoveInput : public CoordinateInput
	{
	public:
		MoveInput(core::long64 time_ms, int view_x, int view_y) : CoordinateInput(InputType::Move, time_ms, view_x, view_y) {}
	};
	class ClickInput : public CoordinateInput
	{
	public:
		ClickInput(core::long64 time_ms, int view_x, int view_y) : CoordinateInput(InputType::Click, time_ms, view_x, view_y) {}
	};
	class GazeInput : public CoordinateInput
	{
	public:
		GazeInput(core::long64 time_ms, int view_x, int view_y, bool valid) : CoordinateInput(InputType::Gaze, time_ms, view_x, view_y), _valid(valid) {}
		bool is_valid() const { return _valid; }
	private:
		bool _valid = false;
	};

	// Convert layer type to std::string
	std::string to_string(LayerType type);
	
	// Forward declaration
	class LogDatum;

	// Layer class
	class Layer : public std::enable_shared_from_this<Layer>
	{
	public:

		// Required for deep copy of layer
		friend class LogDatum;

		// Force the use of shared pointer
		static std::shared_ptr<Layer> create()
		{
			return std::shared_ptr<Layer>(new Layer());
		}

		// Append child. This layer is set as parent
		void append_child(std::shared_ptr<Layer> sp_layer);

		// Access child by index within children vector
		std::shared_ptr<const Layer> get_child(unsigned int idx) const;

		// Get count of children (for iteration)
		unsigned int get_child_count() const;

		// Get vector of all children
		std::vector<std::shared_ptr<const Layer> > get_children() const;
		std::vector<std::shared_ptr<Layer> > get_children();

		// Access layer or its child. Access describes indices of layers in structure and access_idx the index within that indices
		std::shared_ptr<const Layer> access(const std::vector<unsigned int>& r_access, unsigned int access_idx = 0) const;
		std::shared_ptr<Layer> access(const std::vector<unsigned int>& r_access, unsigned int access_idx = 0);

		// Get own layer mask with removed children pixels. In viewport space, 8bit gray depth
		cv::Mat get_view_mask() const;
		
		// Getter
		LayerType get_type()		const { return _type; }
		std::string get_xpath()		const { return _xpath; }
		cv::Point2i get_view_pos()	const { return _view_pos; }
		int get_view_width()		const { return _view_width; }
		int get_view_height()		const { return _view_height; }
		int get_scroll_x()			const { return _scroll_x; }
		int get_scroll_y()			const { return _scroll_y; }
		int get_zindex()			const { return _zindex; }

		// Setter (view is short term for viewport)
		void set_type(LayerType type)			{ _type = type; }
		void set_xpath(std::string xpath)		{ _xpath = xpath; }
		void set_view_pos(cv::Point2i view_pos)	{ _view_pos = view_pos; }
		void set_view_width(int view_width)		{ _view_width = view_width; }
		void set_view_height(int view_height)	{ _view_height = view_height; }
		void set_scroll_x(int scroll_x)			{ _scroll_x = std::max(0, scroll_x); }
		void set_scroll_y(int scroll_y)			{ _scroll_y = std::max(0, scroll_y); }
		void set_zindex(int zindex)				{ _zindex = zindex; }

		// Push back input
		void push_back_input(std::shared_ptr<const Input> sp_input);

		// Get input
		std::vector<std::shared_ptr<const Input> > get_input() const;
		
		// Create visual debug datum
		VD(std::shared_ptr<core::visual_debug::Datum> create_visual_debug_datum() const;)

	protected:

		// Get width and height of root elements (considered to be same as web view)
		void get_view_size_of_root(int& r_width, int& r_height) const;

		// Get simple layer mask (without removed children etc.). Not const return, may be further used!
		cv::Mat get_simple_view_mask() const;

		// Get accumulated mask of children and their children. Not const return, may be further used! Matrix might be empty with zero sizes
		cv::Mat get_children_view_mask() const;

	private:

		// Private constructor
		Layer() {}

		// Private copy constructor with default function (used for deep copy)
		Layer(const Layer&) = default;

		// Private assignment constructor
		Layer& operator=(const Layer&) = delete;

		// (Simple) members
		LayerType _type = LayerType::None;
		std::string _xpath = "";
		cv::Point2i _view_pos = cv::Point2i(0, 0); // upper left corner within web view in screencast frame
		int _view_width = 0; // width of visible potion in viewport
		int _view_height = 0; // height of visible potion viewport
		int _scroll_x = 0; // relative horizontal scrolling in relation to parent, positive value
		int _scroll_y = 0; // relative vertical scrolling in relation to parent, positive value
		int _zindex = 0; // z-index according to DOM

		// Shared members (must be treated special at deep copy)
		std::weak_ptr<const Layer> _wp_parent = std::weak_ptr<const Layer>();
		std::vector<std::shared_ptr<Layer> > _children; // order is important, are accessed by index

		// Members that are filled afterwards by parser. May not be considered in the deep copy process, as filled later
		std::vector<std::shared_ptr<const Input> > _input;
	};
}
