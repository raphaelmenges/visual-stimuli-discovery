//! Log Explorer
/*!
Displays content of a log record, consisting of a webm and a json file.
*/

#define CVUI_DISABLE_COMPILATION_NOTICES
#define CVUI_IMPLEMENTATION // insert implementation of cvui

#include <Core/Core.hpp>
#include <opencv2/core/utility.hpp>
#include <opencv2/opencv.hpp>
#include <opencv2/core/types.hpp>
#include <opencv2/features2d.hpp>
#include <opencv2/calib3d.hpp>
#include <cvui.h>
#include <nlohmann/json.hpp>
#include <libsimplewebm.hpp>
#include <cxxopts.hpp>
#include <iostream>
#include <fstream>
#include <set>

// TODO:
// Scrolling estimation does not work well for, e.g., 9gag

using json = nlohmann::json;

// http://answers.opencv.org/question/93317/orb-keypoints-distribution-over-an-image/?answer=93395#post-id-93395
void adaptive_non_maximal_suppresion(std::vector<cv::KeyPoint>& keypoints,
	const int numToKeep)
{
	if ((int)keypoints.size() < numToKeep) { return; }

	//
	// Sort by response
	//
	std::sort(keypoints.begin(), keypoints.end(),
		[&](const cv::KeyPoint& lhs, const cv::KeyPoint& rhs)
	{
		return lhs.response > rhs.response;
	});

	std::vector<cv::KeyPoint> anmsPts;

	std::vector<double> radii;
	radii.resize(keypoints.size());
	std::vector<double> radiiSorted;
	radiiSorted.resize(keypoints.size());

	const float robustCoeff = 1.11; // see paper

	for (int i = 0; i < (int)keypoints.size(); ++i)
	{
		const float response = keypoints[i].response * robustCoeff;
		double radius = std::numeric_limits<double>::max();
		for (int j = 0; j < i && keypoints[j].response > response; ++j)
		{
			radius = std::min(radius, cv::norm(keypoints[i].pt - keypoints[j].pt));
		}
		radii[i] = radius;
		radiiSorted[i] = radius;
	}

	std::sort(radiiSorted.begin(), radiiSorted.end(),
		[&](const double& lhs, const double& rhs)
	{
		return lhs > rhs;
	});

	const double decisionRadius = radiiSorted[numToKeep];
	for (int i = 0; i < (int)radii.size(); ++i)
	{
		if (radii[i] >= decisionRadius)
		{
			anmsPts.push_back(keypoints[i]);
		}
	}

	anmsPts.swap(keypoints);
}

struct FixedElement
{
	cv::Rect2i rect = cv::Rect2i(0, 0, 0, 0);
	// TODO start / end ms
	// TODO xpath
};

struct DataDatum
{
	cv::Point2i viewport_screen_position = cv::Point2i(0, 0);
	cv::Point2i viewport_size = cv::Point2i(0, 0);
	int global_scroll_y = 0;
};

class Parser
{
public:

	Parser(std::string json_path)
	{
		// Load json file
		std::ifstream json_file(json_path);
		json events;
		json_file >> events;

		// Get fields of json
		auto fixed_elements = events.at("FixedElements");
		auto meta_events = events.at("MetaEvents");

		// TODO testing
		// std::cout << meta_events.dump(4) << std::endl;

		// Go over fixed elements
		for (const auto& fixed_element : fixed_elements)
		{
			FixedElement tmp;
			tmp.rect.x = fixed_element["topLeftY"].get<int>();
			tmp.rect.y = fixed_element["topLeftX"].get<int>();
			tmp.rect.width = fixed_element["width"].get<int>();
			tmp.rect.height = fixed_element["height"].get<int>();
			_fixed_elements.push_back(tmp);
		}

		// Go over meta_events
		for (const auto& meta_event : meta_events)
		{
			std::string type = meta_event["type"].get<std::string>();
			if(type == "scroll") // scrolling
			{
				long ms = meta_event["videoTs"].get<long>();
				int global_scroll_y = meta_event["scrollY"].get<int>();
				_global_scrolls_y[ms] = global_scroll_y;
			}
			else if (type == "mainWindowGeometry") // main window
			{
				long ms = meta_event["videoTs"].get<long>();
				int x = meta_event["x"].get<int>();
				int y = meta_event["y"].get<int>();
				_main_window_pos[ms] = cv::Point2i(x, y);
			}
			else if (type == "browserWindowGeometry") // browser widget in main window
			{
				long ms = meta_event["videoTs"].get<long>();
				int x = meta_event["x"].get<int>();
				int y = meta_event["y"].get<int>();
				_browser_widget_pos[ms] = cv::Point2i(x, y);
			}
			else if (type == "browserToolsGeometry") // tools widget in browser widget
			{
				long ms = meta_event["videoTs"].get<long>();
				int x = meta_event["x"].get<int>();
				int y = meta_event["y"].get<int>();
				_tools_widget_pos[ms] = cv::Point2i(x, y);
			}
			else if (type == "tabWidgetGeometry") // tab widget in tools widget
			{
				long ms = meta_event["videoTs"].get<long>();
				int x = meta_event["x"].get<int>();
				int y = meta_event["y"].get<int>();
				_tab_widget_pos[ms] = cv::Point2i(x, y);
			}
			else if (type == "stackedTabsGeometry") // stacked tab widget in tab widget
			{
				long ms = meta_event["videoTs"].get<long>();
				int x = meta_event["x"].get<int>();
				int y = meta_event["y"].get<int>();
				_stacked_tab_widget_pos[ms] = cv::Point2i(x, y);
			}
			else if (type == "webViewGeometry") // web view widget in stacked tab widget
			{
				long ms = meta_event["videoTs"].get<long>();

				// Position
				int x = meta_event["x"].get<int>();
				int y = meta_event["y"].get<int>();
				_web_view_widget_pos[ms] = cv::Point2i(x, y);

				// Size
				int width = meta_event["width"].get<int>();
				int height = meta_event["height"].get<int>();
				_web_view_widget_size[ms] = cv::Point2i(width, height);
			}
		}
	}

	const std::vector<FixedElement>& retrieve_fixed_elements() const
	{
		return _fixed_elements;
	}

	// Get datum containing log data about certain point of time
	DataDatum get_datum(long ms) const
	{
		// Build log datum from data
		DataDatum datum;

		// ### Scrolling ###
		for (auto it = _global_scrolls_y.begin(); it != _global_scrolls_y.end(); it++)
		{
			// Check timestamp
			if (it->first <= ms) { datum.global_scroll_y = it->second; }
			else { break; }
		}

		// ### Viewport on screen ###
		cv::Point2i main_window_pos(0, 0);
		cv::Point2i browser_widget_pos(0, 0);
		cv::Point2i tools_widget_pos(0, 0);
		cv::Point2i tab_widget_pos(0, 0);
		cv::Point2i stacked_tab_widget_pos(0, 0);
		cv::Point2i web_view_pos(0, 0);

		// Main window
		for (auto it = _main_window_pos.begin(); it != _main_window_pos.end(); it++)
		{
			// Check timestamp
			if (it->first <= ms) { main_window_pos = it->second; }
			else { break; }
		}

		// Browser widget
		for (auto it = _browser_widget_pos.begin(); it != _browser_widget_pos.end(); it++)
		{
			// Check timestamp
			if (it->first <= ms) { browser_widget_pos = it->second; }
			else { break; }
		}

		// Tools widget
		for (auto it = _tools_widget_pos.begin(); it != _tools_widget_pos.end(); it++)
		{
			// Check timestamp
			if (it->first <= ms) { tools_widget_pos = it->second; }
			else { break; }
		}

		// Tab widget
		for (auto it = _tab_widget_pos.begin(); it != _tab_widget_pos.end(); it++)
		{
			// Check timestamp
			if (it->first <= ms) { tab_widget_pos = it->second; }
			else { break; }
		}

		// Stacked tab widget
		for (auto it = _stacked_tab_widget_pos.begin(); it != _stacked_tab_widget_pos.end(); it++)
		{
			// Check timestamp
			if (it->first <= ms) { stacked_tab_widget_pos = it->second; }
			else { break; }
		}

		// Web view widget
		for (auto it = _web_view_widget_pos.begin(); it != _web_view_widget_pos.end(); it++)
		{
			// Check timestamp
			if (it->first <= ms) { web_view_pos = it->second; }
			else { break; }
		}

		// Accumulate positions
		datum.viewport_screen_position = main_window_pos + browser_widget_pos + tools_widget_pos + tab_widget_pos + stacked_tab_widget_pos + web_view_pos;

		// Size of web view
		for (auto it = _web_view_widget_size.begin(); it != _web_view_widget_size.end(); it++)
		{
			// Check timestamp
			if (it->first <= ms) { datum.viewport_size = it->second; }
			else { break; }
		}

		// Return datum
		return datum;
	}

private:

	std::map<long, int> _global_scrolls_y;
	std::map<long, cv::Point2i> _main_window_pos;
	std::map<long, cv::Point2i> _browser_widget_pos;
	std::map<long, cv::Point2i> _tools_widget_pos;
	std::map<long, cv::Point2i> _tab_widget_pos;
	std::map<long, cv::Point2i> _stacked_tab_widget_pos;
	std::map<long, cv::Point2i> _web_view_widget_pos;
	std::map<long, cv::Point2i> _web_view_widget_size;
	std::vector<FixedElement> _fixed_elements;
};

class LogDatum
{
public:

	LogDatum(const Parser& r_parser, const simplewebm::Image& r_image) : _image(r_image)
	{
		// Time from seconds to milliseconds
		long ms = (long)(r_image.time * 1000.0) + 500; // TODO assuming that first frame of video is taken after one frame time

		// Retrieve datum of that time
		_data_datum = r_parser.get_datum(ms);

		// Extract viewport rectangle TODO: here is some +1 error that has been fixed in master
		{
			// Position
			int x = _data_datum.viewport_screen_position.x;
			int y = _data_datum.viewport_screen_position.y;
			x = core::math::clamp(x, 0, _image.width - 1);
			y = core::math::clamp(y, 0, _image.height - 1);

			// Width
			int far_x = _data_datum.viewport_screen_position.x + _data_datum.viewport_size.x;
			far_x = core::math::clamp(far_x, 0, _image.width - 1);
			int width = far_x - x;

			// Height
			int far_y = _data_datum.viewport_screen_position.y + _data_datum.viewport_size.y;
			far_y = core::math::clamp(far_y, 0, _image.height - 1);
			int height = far_y - y;

			// Mask
			_viewport_rect = cv::Rect(x, y, width, height); // geometrical coordinate system, not mathematical
		}

		// Image to OpenCV mat
		_image_matrix = cv::Mat(
			_image.height,
			_image.width,
			CV_8UC3,
			const_cast<char *>(_image.data.data()));

		// OpenCV mat of image to viewport mat
		_viewport_matrix = _image_matrix(_viewport_rect);

		// Viewport mat to grayscale
		cv::Mat viewport_gray;
		cv::cvtColor(_viewport_matrix, viewport_gray, CV_RGB2GRAY); // copies image from RGB to Gray

		// Detect ORB keypoints (TODO: crashes at some point if no? keypoints are found or so..investigate!)
		const int CELL_COUNT_X = 4; // cell count
		const int CELL_COUNT_Y = 3;
		int cell_size_x = viewport_gray.size().width / CELL_COUNT_X;
		int cell_size_y = viewport_gray.size().height / CELL_COUNT_Y; // TODO: think whether boundaries are perfect... (probably not)
		cv::Ptr<cv::FeatureDetector> detector = cv::ORB::create(5000 / (CELL_COUNT_X * CELL_COUNT_Y)); // make ~5000 keypoints
		_keypoints.clear();
		for (int y = 0; y < CELL_COUNT_Y; y++)
		{
			for (int x = 0; x < CELL_COUNT_X; x++)
			{
				std::vector<cv::KeyPoint> keypoints;
				cv::Mat mask(viewport_gray.size(), CV_8UC1, cv::Scalar(0));
				cv::Rect rect(x * cell_size_x, y * cell_size_y, cell_size_x, cell_size_y);
				cv::Mat mask_area = mask(rect);
				mask_area = cv::Scalar(1);
				detector->detect(viewport_gray, keypoints, mask);
				_keypoints.insert(_keypoints.end(), keypoints.begin(), keypoints.end());
			}
		}

		// Filter keypoints for nice distribution -> now using custom grid approach
		// adaptive_non_maximal_suppresion(_keypoints, 500); // melt down to 500 keypoints, which are nicely distributed over the viewport

		// Compute descriptors
		cv::Ptr<cv::DescriptorExtractor> extractor = cv::ORB::create((int)_keypoints.size());
		extractor->compute(viewport_gray, _keypoints, _descriptors);

		// Compute intra similarity
		cv::Ptr<cv::BFMatcher> matcher = cv::BFMatcher::create(cv::NORM_HAMMING);
		std::vector<std::vector<cv::DMatch> > matches;
		matcher->knnMatch(_descriptors, _descriptors, matches, 2); // k = 1 would only output similarity with itself

		// Throw away keypoints which descriptors are matching too good (similar within image)
		std::set<int> to_delete;
		for (const auto& r_match : matches)
		{
			for (const auto& r_inner_match : r_match)
			{
				if ((r_inner_match.queryIdx != r_inner_match.trainIdx) && r_inner_match.distance < 10)
				{
					to_delete.emplace(r_inner_match.queryIdx);
					to_delete.emplace(r_inner_match.trainIdx);
				}
			}
		}

		// Remove all too good keypoints (back to front)
		for (auto it = to_delete.rbegin(); it != to_delete.rend(); ++it)
		{
			_keypoints.erase(_keypoints.begin() + *it);
		}

		// Recompute descriptors (probably not required, one could remove descriptor rows according to deleted keypoints)
		_descriptors = cv::Mat();
		extractor = cv::ORB::create((int)_keypoints.size());
		extractor->compute(viewport_gray, _keypoints, _descriptors);
	}
	
	// Members
	simplewebm::Image _image; // from video data
	DataDatum _data_datum; // from log data
	cv::Rect _viewport_rect;
	cv::Mat _image_matrix;
	cv::Mat _viewport_matrix;
	std::vector<cv::KeyPoint> _keypoints;
	cv::Mat _descriptors;
};

// Global variables
const char* WINDOW_NAME = "Log Explorer";

//! Main function of Log Explorer.
/*!
\return Indicator about success.
*/
int main(int argc, const char* argv[])
{
	// Paths
	/*
	std::string webm_path = "C:/gm_tests/WeST.webm";
	std::string json_path = "C:/gm_tests/WeST.json";
	*/
	/*
	std::string webm_path = "C:/gm_tests/9gag.webm";
	std::string json_path = "C:/gm_tests/9gag.json";
	*/
	/*
	std::string webm_path = "C:/gm_tests/Polygon.webm";
	std::string json_path = "C:/gm_tests/Polygon.json";
	*/
	/*
	std::string webm_path = "C:/gm_tests/window.webm";
	std::string json_path = "C:/gm_tests/window.json";
	*/

	// TODO Testing
	std::string webm_path = std::string(GM_RES_PATH) + "/examples/digg/session_1.webm";
	std::string json_path = std::string(GM_RES_PATH) + "/examples/digg/session_1.json";

	// Create options object
	cxxopts::Options options("VisualStimuliDiscovery Log Explorer", "Explore log records from the GazeMining Logger application.");

	// Add options
	try
	{
		options.add_options()
			("video", "Path to the .webm of the log record", cxxopts::value<std::string>())
			("data", "Path to the .json of the log record", cxxopts::value<std::string>())
			;
	}
	catch (cxxopts::OptionSpecException e)
	{
		std::cerr << e.what() << std::endl;
		return -1;
	}

	// Retrieve options
	try
	{
		// Parse arguments to options
		auto result = options.parse(argc, argv);

		// Video
		if (result.count("video"))
		{
			webm_path = result["video"].as<std::string>();
		}

		// Data
		if (result.count("data"))
		{
			json_path = result["data"].as<std::string>();
		}
	}
	catch (cxxopts::OptionParseException e)
	{
		std::cerr << e.what() << std::endl;
		return -1;
	}

	// Info for user
	std::cout << "Welcome to the Log Explorer!" << std::endl;
	std::cout << "Path to webm: " << webm_path << std::endl;
	std::cout << "Path to json: " << json_path << std::endl;
	std::cout << "Press any key to start the exploration." << std::endl;
	core::misc::wait_any_key();

	// Load json data
	std::cout << "Loading data log...";
	Parser parser(json_path);
	std::cout << "done" << std::endl;

	// Load webm data
	auto sp_images = std::shared_ptr<std::vector<simplewebm::Image> >(new std::vector<simplewebm::Image>); // holds retrieved images
	auto up_video_walker = simplewebm::create_video_walker(webm_path);
	std::cout << "Loading video log...";
	while (up_video_walker->walk(sp_images, 1))
	{
		std::cout << ".";
	}
	std::cout << "done" << std::endl;

	// Processing log record
	std::cout << "Processing log record...";
	std::vector<LogDatum> log_dates;
	for (int frame = 0; frame < (int)sp_images->size(); ++frame)
	{
		// Load corresponding image of video
		const auto& r_image = sp_images->at(frame);

		// Push back log datum object
		log_dates.push_back(LogDatum(parser, r_image));

		// Report progress
		std::cout << ".";
	}
	std::cout << "done" << std::endl;

	std::cout << "Press ESC in GUI window to exit. Have fun exploring!" << std::endl;

#ifdef GM_VISUAL_DEBUG
	{
		// Check whether there were loaded images from the video
		if (sp_images->empty()) // later accessing first element to estimate size of window
		{
			return 0;
		}

		// Estimate resolution of video
		int video_width = sp_images->at(0).width;
		int video_height = sp_images->at(0).height;

		// Calculate size for the window frame
		int window_frame_width = std::max(800, video_width);
		int window_frame_height = std::max(600, video_height + 100); // video height + area for interface

		// Create a frame where components will be rendered to
		cv::Mat frame = cv::Mat(window_frame_height, window_frame_width, CV_8UC3);

		// Init cvui and tell it to create a OpenCV window, i.e., cv::namedWindow(WINDOW_NAME)
		cvui::init(WINDOW_NAME);

		// Values to be controlled through the UI
		int ui_video_frame_idx = 0;
		bool ui_viewport_mask = false;
		bool ui_highlight_fixed_elements = false;
		bool ui_highlight_keypoints = false;

		// Render the UI
		while (true)
		{
			// Fill the frame with a nice color
			frame = cv::Scalar(49, 52, 49);

			// Start of enclosing column
			cvui::beginColumn(frame, 0, video_height, -1, -1, 1);

			// First row
			cvui::beginRow(-1, -1, 3);

				// Control frame index
				if (cvui::button("-100")) { ui_video_frame_idx -= 100; }
				if (cvui::button("-10")) { ui_video_frame_idx -= 10; }
				cvui::counter(&ui_video_frame_idx);
				if (cvui::button("+10")) { ui_video_frame_idx += 10; }
				if (cvui::button("+100")) { ui_video_frame_idx += 100; }
				ui_video_frame_idx = core::math::clamp(ui_video_frame_idx, 0, int(sp_images->size()) - 1);

				// Retrieve corresponding log datum
				const auto& r_log_datum = log_dates.at(ui_video_frame_idx);

				// Time info about frame
				cvui::text("Frame time: " + std::to_string(r_log_datum._image.time));

			// End of first row
			cvui::endRow();

			// Compute offset according to feature detector
			double estimated_scrolling_delta = 0.0;
			if (ui_video_frame_idx > 0)
			{
				// Previous log datum
				const auto& r_previous_log_datum = log_dates.at(ui_video_frame_idx - 1);

				// Brute force matcher
				cv::Ptr<cv::BFMatcher> matcher = cv::BFMatcher::create(cv::NORM_HAMMING);
				std::vector<cv::DMatch> matches;
				matcher->match(r_previous_log_datum._descriptors, r_log_datum._descriptors, matches);

				// Filter for features which are on the same viewport position and similar for both frames
				std::set<int> to_delete;
				for (int i = 0; i < (int)matches.size(); i++)
				{
					const auto& r_match = matches.at(i);
					int index_0 = r_match.queryIdx;
					int index_1 = r_match.trainIdx;
					if (r_match.distance <= 1 // similarity of feature vector
						&& (core::math::euclidean_dist(r_previous_log_datum._keypoints.at(index_0).pt, r_log_datum._keypoints.at(index_1).pt) < 2.f)) // spatial similarity
					{
						to_delete.emplace(i);
					}
				}

				// Remove to_delete matches
				for (auto it = to_delete.rbegin(); it != to_delete.rend(); ++it)
				{
					matches.erase(matches.begin() + *it);
				}

				// Sort matches
				std::sort(matches.begin(), matches.end()); // sort predicate is given by OpenCV

				// Only get first n matches
				unsigned int new_size = std::min(100u, (unsigned int)matches.size());
				matches.resize(new_size);

				// Filter only for relevant matches
				matches.erase(std::remove_if(matches.begin(), matches.end(), [&](const cv::DMatch& r_match)->bool { return r_match.distance > 5; }), matches.end());
				
				// Average distance of well matched keypoints
				/*/
				if (!matches.empty())
				{
					for (const auto& r_match : matches)
					{
						int index_0 = r_match.queryIdx;
						int index_1 = r_match.trainIdx;
						estimated_scrolling_delta += (double)(r_previous_log_datum._keypoints.at(index_0).pt.y - r_log_datum._keypoints.at(index_1).pt.y); // it seems that the origin is in the upper left corner. Features move up when somebody scrolls down!
					}
					estimated_scrolling_delta /= (double)matches.size(); // TODO: average is not best idea: rather use supporters etc.
				}
				*/
				
				// Calculate homography using RANSAC to estimate transformation between frames
				// Average distance of well matched keypoints
				std::vector<cv::Point2f> previous_matched_keypoints, matched_keypoints;
				if (!matches.empty())
				{
					for (const auto& r_match : matches)
					{
						int index_0 = r_match.queryIdx;
						int index_1 = r_match.trainIdx;
						previous_matched_keypoints.push_back(r_previous_log_datum._keypoints.at(index_0).pt);
						matched_keypoints.push_back(r_log_datum._keypoints.at(index_1).pt);
					}

					cv::Mat H = cv::findHomography(previous_matched_keypoints, matched_keypoints, CV_RANSAC);

					// Extract y-scrolling
					if (!H.empty()) // if no match found is empty
					{
						estimated_scrolling_delta = -H.at<double>(1, 2); // TODO: think about which element do we access here

						// std::cout << H << std::endl; // debugging purposes
					}
				}
			}

			// Second row
			cvui::beginRow(-1, -1, 3);

				// Output data
				cvui::text("Frame Count: " + std::to_string(sp_images->size()));
				cvui::text("Scrolling: " + std::to_string(r_log_datum._data_datum.global_scroll_y));
				cvui::text("Estimated Scrolling Delta: " + std::to_string(estimated_scrolling_delta));
				cvui::text("Viewport Screen Position: (" + std::to_string(r_log_datum._data_datum.viewport_screen_position.x) + ", " + std::to_string(r_log_datum._data_datum.viewport_screen_position.y) + ")");
				cvui::text("Viewport Size: (" + std::to_string(r_log_datum._data_datum.viewport_size.x) + ", " + std::to_string(r_log_datum._data_datum.viewport_size.y) + ")");
				// TODO: plot more info from feature extraction (to get an idea how many features are necessary)

			// End of second row
			cvui::endRow();

			// Third row
			cvui::beginRow(-1, -1, 3);

				// Options
				cvui::checkbox("Viewport Mask", &ui_viewport_mask);
				cvui::checkbox("Highlight Fixed Elements", &ui_highlight_fixed_elements);
				cvui::checkbox("Highlight Keypoints", &ui_highlight_keypoints);

			// End of third row
			cvui::endRow();

			// End of enclosing column
			cvui::endColumn();

			// Deep copy of image matrix
			cv::Mat image_matrix = r_log_datum._image_matrix.clone();

			// Apply viewport mask if required
			if (ui_viewport_mask)
			{
				cv::Mat inverse_mask(image_matrix.size(), CV_8UC1, cv::Scalar(1)); // single-channel mask with size of image, filled with 1
				cv::Mat inverse_mask_ROI = inverse_mask(r_log_datum._viewport_rect); // get pointer to ROI part of mask
				inverse_mask_ROI = cv::Scalar(0); // fill ROI in inverse mask with zero
				image_matrix.setTo(cv::Scalar(0), inverse_mask); // set values where mask equals 1 to 0
			}

			// Highlight fixed elements if required
			if (ui_highlight_fixed_elements)
			{
				// Go over fixed elements
				for (const auto& r_fixed_element : parser.retrieve_fixed_elements())
				{
					cv::Scalar color = cv::Scalar(255,0,0); // blue
					cv::rectangle( // from page to video screen capture space
						image_matrix,
						r_fixed_element.rect.tl() + r_log_datum._data_datum.viewport_screen_position,
						r_fixed_element.rect.br() + r_log_datum._data_datum.viewport_screen_position,
						color,
						1, 8, 0);
				}
			}

			// Highlight keypoits if required
			if (ui_highlight_keypoints)
			{
				for (const auto& r_keypoint : r_log_datum._keypoints)
				{
					// Circle keypoint
					cv::circle(
						image_matrix,
						r_keypoint.pt + cv::Point2f(r_log_datum._viewport_rect.tl()),
						2, cv::Scalar(0, 0, 0), 2, 8, 0);

					// Draw exact keypoint
					cv::Vec3b& r_color = image_matrix.at<cv::Vec3b>(r_keypoint.pt + cv::Point2f(r_log_datum._viewport_rect.tl()));
					r_color[0] = 0;
					r_color[1] = 0;
					r_color[2] = 255;
				}
			}

			// Display image
			cvui::image(frame, 0, 0, image_matrix);

			// Update cvui stuff and show everything on the screen
			cvui::imshow(WINDOW_NAME, frame);

			// Wait for ESC
			if (cv::waitKey(20) == 27)
			{
				break;
			}
		}
	}
#else
	{
		std::cout << "Error: Visual debug flag required." << std::endl;
	}
#endif

	return 0;
}
