#ifdef GM_VISUAL_DEBUG

#include "VisualDebug.hpp"
#include <Core/Core.hpp>
#include <clip.h>
#include <opencv2/opencv.hpp>

// Including CVUI into translation unit
#define CVUI_DISABLE_COMPILATION_NOTICES
#define CVUI_IMPLEMENTATION // insert implementation of cvui
#include <cvui.h>

// Some constants, partly from config
const int WINDOW_WIDTH = core::mt::get_config_value(800, { "visual_debug", "window_width" });
const int WINDOW_HEIGHT = core::mt::get_config_value(600, { "visual_debug", "window_height" });
const int WINDOW_PADDING = core::mt::get_config_value(5, { "visual_debug", "window_padding" });
const int ROW_HEIGHT = 20;
const cv::Scalar BACKGROUND_COLOR = cv::Scalar(49, 52, 49);

namespace core
{
	namespace visual_debug
	{
		/////////////////////////////////////////////////
		/// Values (part of datum)
		/////////////////////////////////////////////////

		Value::~Value() {}

		std::shared_ptr<MatrixList> MatrixList::add(const cv::Mat matrix, std::vector<cv::Point2i> points)
		{
			if (matrix.rows > 0 && matrix.cols > 0)
			{
				// Compress matrix losless into uchar vector with PNG encoding
				auto sp_buf = std::make_shared<std::vector<uchar> >();
				cv::imencode(".png", matrix, *sp_buf.get()); // matrix is implicitly cloned
				_matrices.push_back({ sp_buf, points });
			}
			else
			{
				core::mt::log_warn("Visual Debug: Could not store matrix as one dimension is zero.");
			}
			
			// Return this
			return this->shared_from_this();
		}

		void MatrixList::paint(int width, int height) const
		{
			cvui::beginRow(width, ROW_HEIGHT);

			// Name
			cvui::text("- " + _name);

			// "Copy to clipboard" buttons for matrices
			if (!_matrices.empty())
			{
				int i = 1;
				for (const auto& r_matrix : _matrices) // one for every matrix
				{
					cvui::space(5);
					if (cvui::button(60, 14, std::string("Copy ") + std::to_string(i)))
					{
						// Decode raw matrix
						cv::Mat matrix = cv::imdecode(*(r_matrix.first).get(), -1); // -1 ensures decoding with alpha channel
						
						// Convert matrix to BGRA
						cv::Mat matrix_bgra;
						if (matrix.channels() == 1)
						{
							cv::cvtColor(matrix, matrix_bgra, cv::COLOR_GRAY2BGRA);
						}
						else if(matrix.channels() == 3)
						{
							cv::cvtColor(matrix, matrix_bgra, cv::COLOR_BGR2BGRA);
						}
						else // 4 channels
						{
							matrix_bgra = matrix;
						}
						clip::image_spec spec;
						spec.width = matrix_bgra.cols;
						spec.height = matrix_bgra.rows;
						spec.bits_per_pixel = 32;
						spec.bytes_per_row = spec.width * 4;
						spec.blue_mask = 0xff;
						spec.green_mask = 0xff00;
						spec.red_mask = 0xff0000;
						spec.alpha_mask = 0xff000000;
						spec.blue_shift = 0;
						spec.green_shift = 8;
						spec.red_shift = 16;
						spec.alpha_shift = 24;
						clip::image img(matrix_bgra.data, spec);
						clip::set_image(img);
					}
					++i;
				}
			}

			cvui::endRow();

			height -= ROW_HEIGHT; // printing of names takes a row of height
			if (!_matrices.empty() && height > 0) // if there is no height available, forget about painting matrices
			{
				// Calculate size of matrices
				const int matrix_width = (width / (int)_matrices.size()) - 5; // -5 because of space after each matrix

				// Paint matrices
				cvui::beginRow(width, height);
				for (const auto& r_matrix : _matrices)
				{
					// Decode raw matrix
					cv::Mat matrix = cv::imdecode(*(r_matrix.first).get(), -1); // -1 ensures decoding with alpha channel
					
					// Scale matrix to fit into given space
					float scale = 1.f;
					matrix = core::opencv::scale_to_fit(matrix, matrix_width, height, &scale);
					
					// Convert matrix to BGR to fit expectations of cvui
					if(matrix.channels() == 1) // convert gray to BGR
					{
						cv::cvtColor(matrix, matrix, cv::COLOR_GRAY2BGR);
					}
					else if (matrix.channels() == 3)
					{
						// Nothing to do
					}
					else if (matrix.channels() == 4) // convert RGBA to BGR
					{
						cv::Mat background = core::opencv::create_chess_board(matrix.cols, matrix.rows);
						cv::cvtColor(background, background, cv::COLOR_GRAY2BGRA);
						core::opencv::blend(matrix, background, matrix);
						cv::cvtColor(matrix, matrix, cv::COLOR_BGRA2BGR);
					}

					// Draw points on matrix with BGR channels
					for (const auto& r_point : r_matrix.second)
					{
						// Circle keypoint, considering scale
						cv::circle(
							matrix,
							scale * r_point,
							2, cv::Scalar(0, 0, 0), 1, 8, 0);

						// Draw exact keypoint, considering scale
						cv::Vec3b& r_color = matrix.at<cv::Vec3b>(scale * r_point);
						r_color[0] = 0; // blue
						r_color[1] = 0; // green
						r_color[2] = 255; // red
					}

					// Display image
					cvui::image(matrix);

					// Add some space
					cvui::space(5);
					
				}
				cvui::endRow();
			}
		}

		std::shared_ptr<StringList> StringList::add(const std::string& value)
		{
			_strings.push_back(value);
			return this->shared_from_this();
		}

		void StringList::paint(int width, int height) const
		{
			// Name
			cvui::beginRow(width, ROW_HEIGHT);
			cvui::text("- " + _name);
			cvui::endRow();

			// Content
			cvui::beginRow(width, ROW_HEIGHT);
			for (const auto& r_string : _strings)
			{
				cvui::text(" " + r_string); // add space because CVUI looses first pixel column
			}
			cvui::endRow();
		}

		/////////////////////////////////////////////////
		/// Datum (part of dump)
		/////////////////////////////////////////////////

		std::shared_ptr<Datum> Datum::add(std::shared_ptr<const Value> sp_value)
		{
			_values.push_back(sp_value);
			return this->shared_from_this();
		}

		std::shared_ptr<Datum> Datum::add(std::shared_ptr<const Datum> sp_sub_datum)
		{
			_sub_dates.push_back(sp_sub_datum);
			return this->shared_from_this();
		}

		void Datum::paint(int datum_depth) const
		{
			// Depth indicator
			std::string depth_indicator(">>");
			for (int i = 0; i < datum_depth; ++i)
			{
				depth_indicator += ">";
			}

			// Check what to display (either own values or sub datum)
			const int sub_datum_count = (int)_sub_dates.size();
			cvui::beginRow(-1, ROW_HEIGHT);
				cvui::text(depth_indicator + " Datum: " + _name);
				if (sub_datum_count > 0)
				{
					cvui::space(10);
					cvui::checkbox("Show sub dates", &_show_sub_dates);
					if (_values.empty()) { _show_sub_dates = true; } // just show sub dates if no values are available
					cvui::space(10);
					if (cvui::button(30, 14, "-10")) { _sub_datum_idx = core::math::clamp(_sub_datum_idx - 10, 0, sub_datum_count - 1); }
					if (cvui::button(30, 14, "-1"))  { _sub_datum_idx = core::math::clamp(_sub_datum_idx - 1,  0, sub_datum_count - 1); }
					if (cvui::button(30, 14, "+1"))  { _sub_datum_idx = core::math::clamp(_sub_datum_idx + 1,  0, sub_datum_count - 1); }
					if (cvui::button(30, 14, "+10")) { _sub_datum_idx = core::math::clamp(_sub_datum_idx + 10, 0, sub_datum_count - 1); }
					cvui::space(10);
					cvui::text(std::to_string(_sub_datum_idx + 1) + "/" + std::to_string(_sub_dates.size())); // show index + 1
				}
			cvui::endRow();

			// Paint the content
			if (_show_sub_dates && sub_datum_count > 0)
			{
				_sub_dates.at(_sub_datum_idx)->paint(datum_depth + 1);
			}
			else
			{
				// First, go over values and estimate required heights
				int dynamic_space = // space available for i.e., matrix lists
					WINDOW_HEIGHT - (WINDOW_PADDING * 2) // initially, there is the window height minus padding on each side
					- (datum_depth + 1) * ROW_HEIGHT // each datum has a navigation bar
					- ROW_HEIGHT; // and the dump has a navigation bar
				int dynamic_spaced_values = 0; // i.e., matrix lists
				for (const auto& r_value : _values)
				{
					switch (r_value->get_type())
					{
					case Value::Type::StringList:
						dynamic_space -= (2 * ROW_HEIGHT); // one row for name and one for the data
						break;
					case Value::Type::MatrixList:
						++dynamic_spaced_values;
						break;
					default:
						assert(false); // force to cover all types of values
					}
				}

				// Print values
				const int width = WINDOW_WIDTH - (2 * WINDOW_PADDING);
				for (const auto& r_value : _values)
				{
					switch (r_value->get_type())
					{
					case Value::Type::StringList:
						r_value->paint(width, (2 * ROW_HEIGHT)); // one row for name and one for the data
						break;
					case Value::Type::MatrixList:
						r_value->paint(width, dynamic_space / dynamic_spaced_values);
						break;
					default:
						assert(false); // force to cover all types of values
					}
				}
			}
		}

		/////////////////////////////////////////////////
		/// Dump
		/////////////////////////////////////////////////

		std::shared_ptr<Dump> Dump::add(std::shared_ptr<Datum> sp_datum)
		{
			_dates.push_back(sp_datum);
			return this->shared_from_this();
		}

		void Dump::display() const
		{
			assert(("Dump::display: cannot be displayed outside of main thread",
				core::this_is_main_thread()));

			if (!_dates.empty())
			{
				// Window name
				std::string window_name = "Visual Dump";

				// Create a frame where components will be rendered to
				cv::Mat frame = cv::Mat(WINDOW_HEIGHT, WINDOW_WIDTH, CV_8UC3);

				// Init cvui and tell it to create a OpenCV window, i.e., cv::namedWindow(WINDOW_NAME)
				cvui::init(window_name);

				// Render loop
				while (core::opencv::is_window_open(window_name))
				{
					// Fill the frame with a nice color
					frame = BACKGROUND_COLOR;
				
					// Paint
					bool dump_exit = paint(frame);
				
					// Update cvui stuff and show everything on the screen
					cvui::update(window_name);
					cv::imshow(window_name, frame);

					// Break out of the displaying by hitting ESC
					if (cv::waitKey(20) == 27 || dump_exit)
					{
						cv::destroyWindow(window_name);
						break;
					}
				}
			}
			else
			{
				core::mt::log_warn("Dump \"", _name, "\" is empty and thus not displayed.");
			}
		}
		
		bool Dump::paint(cv::Mat frame) const
		{
			const int datum_count = (int)_dates.size();
			bool exit = true;
			
			if(datum_count > 0)
			{
				exit = false;
	
				// Start of overarching components
				cvui::beginRow(frame,
					0,
					0,
					WINDOW_WIDTH  - (2*WINDOW_PADDING),
					WINDOW_HEIGHT - (2*WINDOW_PADDING),
					0);
				cvui::beginColumn(frame, WINDOW_PADDING, WINDOW_PADDING); // manual padding
	
				// Dump row (first, checks whether user wants to see a different datum)
				cvui::beginRow(-1, ROW_HEIGHT);
					if (cvui::button(70, 14, "Exit Dump")) { exit = true; }
					cvui::space(10);
					cvui::text("> Dump: " + _name);
					cvui::space(10);
					if (cvui::button(30, 14, "-10")){ _datum_idx = core::math::clamp(_datum_idx - 10, 0, datum_count - 1); }
					if (cvui::button(30, 14, "-1"))	{ _datum_idx = core::math::clamp(_datum_idx - 1,  0, datum_count - 1); }
					if (cvui::button(30, 14, "+1"))	{ _datum_idx = core::math::clamp(_datum_idx + 1,  0, datum_count - 1); }
					if (cvui::button(30, 14, "+10")){ _datum_idx = core::math::clamp(_datum_idx + 10, 0, datum_count - 1); }
					cvui::space(10);
					cvui::text(std::to_string(_datum_idx+1) + "/" + std::to_string(_dates.size())); // show index + 1
				cvui::endRow();
				cvui::space(5);
	
				// Paint chosen datum
				_dates.at(_datum_idx)->paint();
	
				// End of overarching components
				cvui::endColumn();
				cvui::endRow();
			}
			
			return exit;
		}
		
		/////////////////////////////////////////////////
		/// Explorer
		/////////////////////////////////////////////////

		std::shared_ptr<Dump> Explorer::create_dump(std::string name, std::string category)
		{
			auto sp_dump = vd_dump(name);
			
			// Check whether category exists
			auto key_it = _dumps_map.find(category);
			if ( key_it == _dumps_map.end() )
			{
				// Create new category with vector of dumps
				_dumps_map[category] = { sp_dump };
			}
			else
			{
				// Push back to existing vector of dumps
				key_it->second.push_back(sp_dump);
			}
			
			return sp_dump;
		}

		void Explorer::display() const
		{
			assert(("Explorer::display: cannot be displayed outside of main thread",
				core::this_is_main_thread()));

			// Check that there is any dump to display
			if (_dumps_map.empty())
			{
				return;
			}

			// Window name
			std::string window_name = "Visual Explorer";

			// Create a frame where components will be rendered to
			cv::Mat frame = cv::Mat(WINDOW_HEIGHT, WINDOW_WIDTH, CV_8UC3);

			// Dump chosen in explorer
			std::shared_ptr<const Dump> sp_dump_to_display = nullptr;

			// Init cvui and tell it to create a OpenCV window, i.e., cv::namedWindow(WINDOW_NAME)
			cvui::init(window_name);

			// Render loop
			bool exit = false;
			while (!exit)
			{
				bool dump_exit = false;
			
				// Fill the frame with a nice color
				frame = BACKGROUND_COLOR;
			
				// Decide what to display
				if(sp_dump_to_display) // dump
				{
					dump_exit = sp_dump_to_display->paint(frame);
				}
				else // explorer
				{
					// Note: below cvui calls do not use any predefined sizing. Layout is automatically generated
	
					// Print categories and available dumps
					cvui::beginRow(frame,
						0,
						0,
						WINDOW_WIDTH  - (2*WINDOW_PADDING),
						WINDOW_HEIGHT - (2*WINDOW_PADDING));
					cvui::beginColumn(frame, WINDOW_PADDING, WINDOW_PADDING); // padding
					cvui::beginRow();
				
					// Go over categories
					for(const auto& r_category : _dumps_map)
					{
						// Each category is one column
						cvui::beginColumn();
	
						// Print category name
						cvui::text(r_category.first);
						cvui::space(5);
						
						// Show button for each dump in the category
						for(const auto& rsp_dump : r_category.second)
						{
							if (cvui::button(rsp_dump->get_name()))
							{
								sp_dump_to_display = rsp_dump;
							}
						}
						cvui::endColumn();
						cvui::space(10);
					}
					cvui::endRow();
					cvui::endColumn();
					cvui::endRow();
				}

				// Update cvui stuff and show everything on the screen
				cvui::update(window_name);
				cv::imshow(window_name, frame);
				
				// Check for closing the explorer
				if (cv::waitKey(20) == 27 || dump_exit)
				{
					if(sp_dump_to_display)
					{
						sp_dump_to_display = nullptr;
					}
					else
					{
						cv::destroyWindow(window_name);
					}
				}

				// Exit if no window is open at this point
				if(!core::opencv::is_window_open(window_name))
				{
					exit = true;
				}
			}
		}
	}
}

#endif // GM_VISUAL_DEBUG
