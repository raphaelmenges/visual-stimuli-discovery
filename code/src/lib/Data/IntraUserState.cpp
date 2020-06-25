#include "IntraUserState.hpp"
#include <fstream>
#include <iomanip>
#include <ctime>

const int PIXEL_HISTORY_DEPTH = core::mt::get_config_value(5, { "splitting", "splitter", "pixel_history_depth" });

namespace data
{
	IntraUserState::IntraUserState(
		std::weak_ptr<const IntraUserStateContainer> wp_container,
		unsigned int frame_idx_start,
		std::vector<unsigned int> layer_access,
		const cv::Mat& pixels,
		int x_offset,
		int y_offset)
		:
		State(cv::Mat(1, 1, CV_8UC4)), // start with empty stitched screenshot (must be at least 1x1, why?)
		_wp_container(wp_container),
		_frame_idx_start(frame_idx_start),
		_frame_idx_end(frame_idx_start),
		_layer_accesses({ layer_access }) // vector with intitial access
	{
		// Store and stitch pixels
		store_and_stich(pixels, x_offset, y_offset);
	}

	// Push input data
	void IntraUserState::push_input_into(MouseData& r_mouse_data, GazeData& r_gaze_data) const
	{
		// Go over frames in intra-user state, get corresponding layer and retrieve the input data
		for (int i = (int)_frame_idx_start; i <= (int)_frame_idx_end; ++i)
		{
			if (auto sp_container = this->get_container().lock())
			{
				// Get all the input of that layer
				auto layer_access = this->get_layer_access(i); // get path to layer in that log datum
				auto sp_layer = sp_container->get_log_datum_container()->get()->at(i)->access_layer(layer_access); // access log datum with that path to retrieve layer
				auto input = sp_layer->get_input(); // retrieve original input

				// Get scrolling to transform from view to document space
				int scroll_x = sp_layer->get_scroll_x();
				int scroll_y = sp_layer->get_scroll_y();

				// Go over input and put it into the corresponding bin
				for (const auto& r_item : input)
				{
					auto type = r_item->get_type();
					switch (type)
					{
					case InputType::Move:
						r_mouse_data.push_back(
							{
								r_item->get_time_ms(),
								(static_cast<const data::CoordinateInput*>(r_item.get()))->get_view_x() + scroll_x,
								(static_cast<const data::CoordinateInput*>(r_item.get()))->get_view_y() + scroll_y,
								"move"
							});
						break;
					case InputType::Click:
						r_mouse_data.push_back(
							{
								r_item->get_time_ms(),
								(static_cast<const data::CoordinateInput*>(r_item.get()))->get_view_x() + scroll_x,
								(static_cast<const data::CoordinateInput*>(r_item.get()))->get_view_y() + scroll_y,
								"click"
							});
						break;
					case InputType::Gaze:
						r_gaze_data.push_back(
							{
								r_item->get_time_ms(),
								(static_cast<const data::CoordinateInput*>(r_item.get()))->get_view_x() + scroll_x,
								(static_cast<const data::CoordinateInput*>(r_item.get()))->get_view_y() + scroll_y
							});
						break;
					}
				}
			}
		}
	}

	void IntraUserState::serialize(std::string directory, std::string id) const
	{
		// Input structs to fill
		State::MouseData mouse_data;
		State::GazeData gaze_data;

		// Stitched screenshot
		cv::imwrite(directory + "/" + id + ".png", this->get_stitched_screenshot());

		// Push input
		push_input_into(mouse_data, gaze_data);
		
		// Mouse data
		{
			std::ofstream mouse_out(directory + "/" + id + "-mouse.csv");
			mouse_out << "timestamp,";
			mouse_out << "x,";
			mouse_out << "y,";
			mouse_out << "type\n";
			for (const auto& r_item : mouse_data)
			{
				mouse_out << std::to_string(std::get<0>(r_item)) << ","; // timestamp
				mouse_out << std::to_string(std::get<1>(r_item)) << ","; // x
				mouse_out << std::to_string(std::get<2>(r_item)) << ","; // y
				mouse_out << std::get<3>(r_item) << "\n"; // type
			}
		}
		
		// Gaze data
		{
			std::ofstream gaze_out(directory + "/" + id + "-gaze.csv");
			gaze_out << "timestamp,";
			gaze_out << "x,";
			gaze_out << "y\n";
			for (const auto& r_item : gaze_data)
			{
				gaze_out << std::to_string(std::get<0>(r_item)) << ","; // timestamp
				gaze_out << std::to_string(std::get<1>(r_item)) << ","; // x
				gaze_out << std::to_string(std::get<2>(r_item)) << "\n"; // y
			}
		}
		
		// Blind frames
		{
			std::ofstream blind_out(directory + "/" + id + "-blind.csv");
			blind_out << "frame_idx\n";
			for (const auto blind_frame_idx : _blind_frame_idxs)
			{
				blind_out << std::to_string(blind_frame_idx) << "\n"; // blind frame
			}
		}
	}

	std::vector<unsigned int> IntraUserState::get_layer_access(unsigned int frame_idx) const
	{
		return _layer_accesses.at(frame_idx - _frame_idx_start);
	}

	void IntraUserState::add_frame(
		std::vector<unsigned int> layer_access,
		const cv::Mat& pixels,
		int x_offset,
		int y_offset)
	{
		// Increase end (information actually already in layer access vector contained)
		_frame_idx_end += 1;

		// Push back layer access
		_layer_accesses.push_back(layer_access);

		// Store and stitch pixels
		store_and_stich(pixels, x_offset, y_offset);
	}

	void IntraUserState::push_blind_frame(std::vector<unsigned int> layer_access, bool front)
	{
		// Actual work
		if(front) // front
		{
			_frame_idx_start -= 1;
			_layer_accesses.push_front(layer_access);
		}
		else // back
		{
			_frame_idx_end += 1;
			_layer_accesses.push_back(layer_access);
		}
		
		// Remember the blind frame
		if(front)
		{
			_blind_frame_idxs.push_back(_frame_idx_start);
		}
		else
		{
			_blind_frame_idxs.push_back(_frame_idx_end);
		}
	}

	void IntraUserState::store_and_stich(const cv::Mat& input_pixels, int x_offset, int y_offset)
	{
		// Store input pixels as compressed PNG
		_pixel_history.push_back(
			std::tuple<std::shared_ptr<std::vector<uchar> >, int, int> // compressed pixels, x-offset, y-offset
			(std::make_shared<std::vector<uchar> >(), x_offset, y_offset));
		cv::imencode(".png", input_pixels, *(std::get<0>(_pixel_history.back()).get()));

		// Limit depth of the history
		int depth_diff = (int)_pixel_history.size() - PIXEL_HISTORY_DEPTH;
		for (int i = 0; i < depth_diff; ++i)
		{
			_pixel_history.pop_front();
		}

		// Make current stitched screenshot large enough to contain new pixels
		cv::Rect input_pixels_rect(x_offset, y_offset, input_pixels.cols, input_pixels.rows); // global coordinates
		cv::Mat stitched_screenshot_clone = get_stitched_screenshot().clone(); // clone of current stitched screenshot
		core::opencv::extend(stitched_screenshot_clone, input_pixels_rect); // extend stitched screenshot

		// Stitch stored pixels onto cloned stitched screenshot (but only within the extends of the input pixels, other pixels stay untouched)
		struct StitchPixels
		{
			cv::Mat ROI; // region of interest in stored matrix
			int x_rel_offset; // offset between input pixels and ROI in stored matrix
			int y_rel_offset; // offset between input pixels and ROI in stored matrix
		};
		std::vector<StitchPixels> stitch_pixels;
		for (const auto& stored : _pixel_history)
		{
			// Get intersection between pixels input rect and stored one in global space
			cv::Mat stored_mat = cv::imdecode(*(std::get<0>(stored)).get(), -1); // -1 ensures decoding with alpha channel
			int x_stored = std::get<1>(stored);
			int y_stored = std::get<2>(stored);
			cv::Rect intersect = input_pixels_rect & cv::Rect(x_stored, y_stored, stored_mat.cols, stored_mat.rows);

			// If no intersection between input and stored, does not matter for stitching
			if (intersect.empty()) { continue; }

			// Bring from global space into local space of stored matrix
			intersect.x -= x_stored;
			intersect.y -= y_stored;

			// Create struct
			StitchPixels tmp;
			tmp.ROI = stored_mat(intersect);
			tmp.x_rel_offset = x_offset - x_stored - intersect.x; // access offset for later iteration
			tmp.y_rel_offset = y_offset - y_stored - intersect.y; // access offset for later iteration
			stitch_pixels.push_back(tmp);
		}

		// Go over stored pixels and compose the stitched screenshot
		for (int x = 0; x < input_pixels.cols; ++x)
		{
			for (int y = 0; y < input_pixels.rows; ++y)
			{
				// Compute most prominent value
				std::map<int, int > rgb_freq; // RGB + count
				uchar a = 0;

				// Go over available screenshots to take pixels from
				for (const auto& stich_pixel : stitch_pixels)
				{
					// Compute relative x and y for the ROI
					int rel_x = x + stich_pixel.x_rel_offset;
					int rel_y = y + stich_pixel.y_rel_offset;

					// Check whether coordinates are still within range (should not happen)
					cv::Rect rect(0, 0, stich_pixel.ROI.cols, stich_pixel.ROI.rows);
					if (!rect.contains(cv::Point(rel_x, rel_y)))
					{
						continue;
					}

					// Store pixel values
					const auto& v = stich_pixel.ROI.at<const cv::Vec4b>(rel_y, rel_x);
					if (v[3] <= 0) { continue; } // skip totally transparent pixels

					// Convert RGB to single integer
					int rgb = (int)v[2];
					rgb = (rgb << 8) + (int)v[1];
					rgb = (rgb << 8) + (int)v[0];
					
					// Enter found value to map
					auto it = rgb_freq.find(rgb);
					if (it != rgb_freq.end()) { (it->second)++; }
					else { rgb_freq[rgb] = 1; }

					// Alpha channel (take maximum found)
					a = a < v[3] ? v[3] : a;
				}

				// TODO one could further improve: if there is an even count for two colors, compute mean etc.
				if (!rgb_freq.empty())
				{
					// Get most common color
					int most_rgb = 0;
					int most_rgb_cnt = 0;
					for (const auto& r_entry : rgb_freq)
					{
						if (r_entry.second > most_rgb_cnt)
						{
							most_rgb = r_entry.first;
							most_rgb_cnt = r_entry.second;
						}
					}

					// Below is important to use a reference to the pixel value!
					auto& v = stitched_screenshot_clone(input_pixels_rect).at<cv::Vec4b>(y, x);
					v[0] = most_rgb & 0xFF;
					v[1] = (most_rgb >> 8) & 0xFF;
					v[2] = (most_rgb >> 16) & 0xFF;
					v[3] = a;
				}
			}
		}

		// Set new stitched screenshot
		set_stitched_screenshot(stitched_screenshot_clone);
	}

	unsigned int IntraUserState::get_idx_in_container() const
	{
		if (auto sp_container = _wp_container.lock())
		{
			for (unsigned int i = 0; i < (unsigned int)sp_container->get()->size(); ++i)
			{
				auto tmp = sp_container->get()->at(i);
				if (this == tmp.get())
				{
					return i;
				}
			}
		}
		return 0;
	}

	void IntraUserStateContainer::serialize(std::string directory) const
	{
		// Check first, whether this container is empty
		if (this->get() && this->get()->empty()) { return; }
		
		// Create directory
		core::misc::create_directories(directory);

		// Vector to store meta information
		std::vector<std::vector<std::string> > meta;

		// Go over states in the container and serialize them
		int i = 0;
		for (const auto & r_state : *this->get())
		{
			// Serialize state
			r_state->serialize(
				directory,
				get_session()->get_id()
					+ "_" + std::to_string(i));

			// Store some meta information
			std::vector<std::string> tmp;
			tmp.push_back(std::to_string(i)); // intra_idx
			tmp.push_back(std::to_string(r_state->get_frame_idx_start())); // frame_idx_start
			tmp.push_back(std::to_string(r_state->get_frame_idx_end())); // frame_idx_end
			tmp.push_back(std::to_string((r_state->get_frame_idx_end() - r_state->get_frame_idx_start()) + 1)); // frame_count
			meta.push_back(tmp);

			// Increment state counter
			++i;
		}

		// Store meta information
		std::ofstream meta_out(directory + "/" + get_session()->get_id() + ".csv");
		meta_out << "shot_idx,"; // meta_out << "intra_idx,";
		meta_out << "frame_idx_start,";
		meta_out << "frame_idx_end,";
		meta_out << "frame_count\n";
		for (const auto& r_tmp : meta)
		{
			for (int i = 0; i < (int)r_tmp.size(); ++i)
			{
				// Item
				meta_out << r_tmp.at(i);

				// Delimiter
				if (i < (int)r_tmp.size() - 1)
				{
					meta_out << ",";
				}
				else
				{
					meta_out << "\n";
				}
			}
		}
		
		// Store information about empty frames
		std::ofstream empty_out(directory + "/" + get_session()->get_id() + "-empty.csv");
		
		// Convert data structure describing empty frames
		std::vector<std::pair<std::string, std::vector<unsigned int> > > empty;
		int max_entry_count = -1;
		for(const auto& r_entry : _empty_frames)
		{
			empty.push_back({ r_entry.first, std::vector<unsigned int>(r_entry.second.begin(), r_entry.second.end())});
			std::sort(empty.back().second.begin(), empty.back().second.end()); // sort frames
			max_entry_count = std::max(max_entry_count, (int)r_entry.second.size());
		}
		
		// Header
		for(int i = 0; i < (int)empty.size(); ++i)
		{
			// Item
			empty_out << empty.at(i).first;
	
			// Delimiter
			if (i < (int)empty.size() - 1)
			{
				empty_out << ",";
			}
			else
			{
				empty_out << "\n";
			}
		}
		
		// Data
		for(int j = 0; j < max_entry_count; ++j) // rows
		{
			for(int i = 0; i < (int)empty.size(); ++i) // columns
			{
				// Store entry
				const auto& r_pair = empty[i];
				if((int)r_pair.second.size() > j)
				{
					empty_out << r_pair.second[j];
				}
				
				// Delimiter
				if (i < (int)empty.size() - 1)
				{
					empty_out << ",";
				}
				else
				{
					empty_out << "\n";
				}
			}
		}
	}
}
