#pragma once

#include <Core/Core.hpp>
#include <Data/Session.hpp>
#include <Data/Layer.hpp>
#include <opencv2/core/types.hpp>
#include <memory>
#include <string>
#include <vector>

// TODO: make serializable

namespace data
{
	// LogDatum class. One log datum stored for each frame in the screencast
	class LogDatum
	{
	public:

		// Constructor
		LogDatum(double frame_time) : _frame_time(frame_time)
		{
			// Assume some root in the Web page, thus create root layer
			_sp_root = Layer::create();
			_sp_root->set_type(data::LayerType::Root);
			_sp_root->set_xpath("html"); // html is root, body might be already a fixed layer
		}

		// Deep copy of log datum, including the frame time
		std::unique_ptr<LogDatum> deep_copy() const
		{
			return deep_copy(this->_frame_time);
		}

		// Deep copy of log datum. Taking frame time of corresponding (new) frame in the screencast
		std::unique_ptr<LogDatum> deep_copy(double frame_time) const
		{
			// Start of with simple copy, does not yet deep copy the layers
			std::unique_ptr<LogDatum> up_clone = std::unique_ptr<LogDatum>(new LogDatum(*this)); // call standard copy constructor
			up_clone->_frame_time = frame_time;

			// Deep copy the layer structure
			up_clone->_sp_root = std::shared_ptr<Layer>(new Layer(*(up_clone->_sp_root)));
			recursive_deep_copy_layer_children(up_clone->_sp_root);

			// Return deep copy
			return up_clone;
		}

		// Get root layer
		std::shared_ptr<const Layer> get_root() const
		{
			return _sp_root;
		}
		std::shared_ptr<Layer> get_root()
		{
			return _sp_root;
		}

		// Access layer by indices navigating through the tree of layers
		std::shared_ptr<const Layer> access_layer(const std::vector<unsigned int>& r_access) const
		{
			return _sp_root->access(r_access);
		}
		std::shared_ptr<Layer> access_layer(const std::vector<unsigned int>& r_access)
		{
			return _sp_root->access(r_access);
		}

		// Getter
		double get_frame_time()						const { return _frame_time; }
		cv::Point2i get_viewport_on_screen_pos()	const { return _viewport_on_screen_pos; }
		cv::Point2i get_viewport_pos()				const { return _viewport_pos; }
		int get_viewport_width()					const { return _viewport_width; }
		int get_viewport_height()					const { return _viewport_height; }

		// Setter
		void set_viewport_on_screen_position(cv::Point2i viewport_pos)	{ _viewport_on_screen_pos = viewport_pos; }
		void set_viewport_pos(cv::Point2i viewport_pos)					{ _viewport_pos = viewport_pos; }
		void set_viewport_width(int viewport_width)						{ _viewport_width = viewport_width; _sp_root->set_view_width(viewport_width); }
		void set_viewport_height(int viewport_height)					{ _viewport_height = viewport_height; _sp_root->set_view_height(viewport_height); }
		
		// Create visual debug datum
		VD(std::shared_ptr<core::visual_debug::Datum> create_visual_debug_datum() const
		{
			// Create datum
			auto sp_datum = vd_datum("Log Datum");
			sp_datum->add(vd_strings("frame_time")->add(std::to_string(_frame_time)));

			// Add dates of layers recursively
			sp_datum->add(_sp_root->create_visual_debug_datum());

			// Return datum
			return sp_datum;
		})

	private:

		// Private copy constructor with default implementation
		LogDatum(const LogDatum&) = default; // required for deep copy

		// Private assignment constructor
		LogDatum& operator=(const LogDatum&) = delete;

		// Recursive helper for deep copy
		void recursive_deep_copy_layer_children(std::shared_ptr<Layer> sp_layer) const
		{
			auto children = sp_layer->_children; // remember children of layer
			sp_layer->_children.clear(); // clear children in layer
			sp_layer->_wp_parent = std::weak_ptr<const Layer>(); // clear parent

			// Go over children and deep copy them
			for (const auto& rsp_child : children)
			{
				auto sp_child = std::shared_ptr<Layer>(new Layer(*rsp_child)); // call standard copy constructor on layer
				recursive_deep_copy_layer_children(sp_child); // perform deep copy of children of child
				sp_layer->append_child(sp_child); // append now deep copied child
			}
		}

		// Members
		std::shared_ptr<Layer> _sp_root = nullptr; // future root node, see constructor
		double _frame_time = 0.0; // time of corresponding frame in screencast in seconds
		cv::Point2i _viewport_on_screen_pos = cv::Point2i(0,0); // viewport position on screen (required to transform, i.e., gaze data)
		cv::Point2i _viewport_pos = cv::Point2i(0,0); // upper left corner of viewport in screencast frame (should be (0,0) if only viewport is recorded in screencast)
		int _viewport_width = 0; // width of viewport in pixels
		int _viewport_height = 0; // height of viewport in pixels
		// TODO: URL change? Tab change? etc...
	};
	
	// Typedefs
	typedef std::vector<std::shared_ptr<LogDatum> > LogDates;
	typedef const std::vector<std::shared_ptr<const LogDatum> > LogDates_const;
	
	// Log Datum Container (for one session)
	class LogDatumContainer
	{
	public:
	
		// Constructor
		LogDatumContainer(
			std::shared_ptr<const Session> sp_session,
			double datacast_duration)
			:
			_sp_session(sp_session),
			_datacast_duration(datacast_duration)
			{}

		// Push back log datum
		void push_back(std::shared_ptr<LogDatum> sp_log_datum)
		{
			_sp_log_dates->push_back(sp_log_datum);
			_sp_log_dates_const = nullptr;
		}
		
		// Get log dates
		std::shared_ptr<LogDates> get() { return _sp_log_dates; }
		std::shared_ptr<LogDates_const> get() const
		{
			if (_sp_log_dates_const == nullptr)
			{
				_sp_log_dates_const = core::misc::make_const(_sp_log_dates);
			}
			return _sp_log_dates_const;
		}

		// Get session
		std::shared_ptr<const Session> get_session() const { return _sp_session; }
		
		// Get duration of datacast in seconds
		double get_datacast_duration() const { return _datacast_duration; }
		
	private:

		// Remove copy and assignment operators
		LogDatumContainer(const LogDatumContainer&) = delete;
		LogDatumContainer& operator=(const LogDatumContainer&) = delete;
	
		// Members
		std::shared_ptr<const Session> _sp_session;
		double _datacast_duration = 0.0; // duration of record according to the .json in seconds
		std::shared_ptr<LogDates> _sp_log_dates = std::make_shared<LogDates>();
		mutable std::shared_ptr<LogDates_const> _sp_log_dates_const = nullptr; // is updated if required by getter
	};
	
	// Typedefs
	typedef std::vector<std::shared_ptr<LogDatumContainer> > LogDatumContainers;
	typedef const std::vector<std::shared_ptr<const LogDatumContainer> > LogDatumContainers_const;
}
