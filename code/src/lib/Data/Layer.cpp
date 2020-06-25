#include "Layer.hpp"
#include <Core/Core.hpp>
#include <opencv2/opencv.hpp>

namespace data
{
	// Convert layer type to std::string
	std::string to_string(LayerType type)
	{
		switch (type)
		{
		case LayerType::Root:
			return "root";
		case LayerType::Fixed:
			return "fixed";
		default: // none
			return "none";
		}
	}

	Input::~Input()
	{
		// Nothing to do
	}

	CoordinateInput::~CoordinateInput()
	{
		// Nothing to do
	}

	void Layer::append_child(std::shared_ptr<Layer> sp_layer)
	{
		assert(("Layer::append_child: Layer has already a parent",
				sp_layer->_wp_parent.expired()));
		
		sp_layer->_wp_parent = this->shared_from_this(); // tell layer who is its parent
		_children.push_back(sp_layer); // add layer to children
	}

	std::shared_ptr<const Layer> Layer::get_child(unsigned int idx) const
	{
		return _children.at(idx);
	}

	unsigned int Layer::get_child_count() const
	{
		return (unsigned int)_children.size();
	}

	std::vector<std::shared_ptr<const Layer> > Layer::get_children() const
	{
		std::vector<std::shared_ptr<const Layer> > output;
		output.reserve((int)_children.size());
		for (const auto& rsp_child : _children)
		{
			output.push_back(rsp_child);
		}
		return output;
	}

	std::vector<std::shared_ptr<Layer> > Layer::get_children()
	{
		std::vector<std::shared_ptr<Layer> > output;
		output.reserve((int)_children.size());
		for (auto& rsp_child : _children)
		{
			output.push_back(rsp_child);
		}
		return output;
	}

	std::shared_ptr<const Layer> Layer::access(const std::vector<unsigned int>& r_access, unsigned int access_idx) const
	{
		if (access_idx >= r_access.size())
		{
			return this->shared_from_this();
		}
		else
		{
			auto sp_child = _children.at(r_access.at(access_idx));
			return sp_child->access(r_access, ++access_idx);
		}
	}

	std::shared_ptr<Layer> Layer::access(const std::vector<unsigned int>& r_access, unsigned int access_idx)
	{
		if (access_idx >= r_access.size())
		{
			return this->shared_from_this();
		}
		else
		{
			auto sp_child = _children.at(r_access.at(access_idx));
			return sp_child->access(r_access, ++access_idx);
		}
	}

	cv::Mat Layer::get_view_mask() const
	{
		// Get own mask
		cv::Mat mask = get_simple_view_mask();

		// Get masks of children
		cv::Mat children_mask = get_children_view_mask();
		if (!children_mask.size().empty())
		{
			// Subtract children from own mask			
			cv::absdiff(mask, children_mask, mask);
		}

		// Return mask
		return mask;
	}

	void Layer::push_back_input(std::shared_ptr<const Input> sp_input)
	{
		_input.push_back(sp_input);
	}

	std::vector<std::shared_ptr<const Input> > Layer::get_input() const
	{
		return _input;
	}

	// Create visual debug datum
	VD(std::shared_ptr<core::visual_debug::Datum> Layer::create_visual_debug_datum() const
	{
		// Create datum
		auto sp_datum = vd_datum("Layer");

		// Add fields of layer
		sp_datum->add(vd_strings("Type")->add(to_string(_type)));
		sp_datum->add(vd_strings("xpath")->add(_xpath));
		sp_datum->add(vd_strings("view_pos_x")->add(std::to_string(_view_pos.x)));
		sp_datum->add(vd_strings("view_pos_y")->add((std::to_string(_view_pos.y))));
		sp_datum->add(vd_strings("view_width")->add((std::to_string(_view_width))));
		sp_datum->add(vd_strings("view_height")->add((std::to_string(_view_height))));
		sp_datum->add(vd_strings("scroll_x")->add((std::to_string(_scroll_x))));
		sp_datum->add(vd_strings("scroll_y")->add((std::to_string(_scroll_y))));
		sp_datum->add(vd_strings("z-index")->add((std::to_string(_zindex))));
		sp_datum->add(vd_strings("input_count")->add((std::to_string(_input.size()))));

		// Add mask of layer
		sp_datum->add(vd_matrices("Mask (Viewport Space)")->add(get_view_mask()));

		// Recursive call over children
		for (const auto& rsp_child : _children)
		{
			sp_datum->add(rsp_child->create_visual_debug_datum());
		}

		// Return datum
		return sp_datum;
	})

	void Layer::get_view_size_of_root(int& r_view_width, int& r_view_height) const
	{
		if (auto sp_parent = _wp_parent.lock())
		{
			// There is another level in the layer tree, delegate the task upwards
			sp_parent->get_view_size_of_root(r_view_width, r_view_height);
		}
		else // I AM ROOT
		{
			r_view_width = _view_width;
			r_view_height = _view_height;
		}
	}

	cv::Mat Layer::get_simple_view_mask() const
	{
		// Estimate size of viewport
		int width, height = 0;
		get_view_size_of_root(width, height);
		
		// Create a mask for this layer
		cv::Mat mask = cv::Mat::zeros(height, width, CV_8UC1); // as big as viewport
		
		// Intersect viewport and layer rect to know which pixels to set
		cv::Rect layer_rect(_view_pos.x, _view_pos.y, _view_width, _view_height);
		cv::Rect viewport_rect(0, 0, width, height);
		cv::Rect intersection = viewport_rect & layer_rect;
		
		// Set pixels of layers in viewport to white
		if (intersection.area() > 0)
		{
			mask(intersection) = cv::Scalar(255); // layer mask in viewport space
		}

		// Return const reference to mask
		return mask;
	}

	cv::Mat Layer::get_children_view_mask() const
	{
		// Estimate size of viewport
		int width, height = 0;
		get_view_size_of_root(width, height);
		cv::Mat acc_mask = cv::Mat::zeros(height, width, CV_8UC1); // as big as viewport

		// Go over children and get their mask
		for (const auto& sp_child : _children)
		{
			// Get simple mask of child and add to acc_mask
			acc_mask |= sp_child->get_simple_view_mask();

			// Ask child to do the the same with their children and add to acc_mask
			acc_mask |= sp_child->get_children_view_mask();
		}

		return acc_mask;
	}
}
