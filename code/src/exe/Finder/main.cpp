#include <Core/Core.hpp>
#include <Core/Task.hpp>
#include <Stage/Processing.hpp>
#include <Stage/Processing/Parser.hpp>
#include <Stage/Processing/Tuning.hpp>
#include <Feature/PixelDiff.hpp>
#include <Feature/EdgeChangeFraction.hpp>
#include <Feature/MSSIM.hpp>
#include <Feature/PSNR.hpp>
#include <Feature/SiftMatch.hpp>
#include <opencv2/opencv.hpp>
#include <cxxopts.hpp>
#include <fstream>
#include <experimental/filesystem>

#ifdef __linux__
namespace fs = std::experimental::filesystem;
#elif _WIN32
#include <experimental/filesystem> // C++-standard header file name
#include <filesystem> // Microsoft-specific implementation header file name
namespace fs = std::experimental::filesystem::v1;
#endif

// Implementation provided by visual debug header (yet, visual debug is not used)
#include <cvui.h>

/////////////////////////////////////////////////
/// Defines
/////////////////////////////////////////////////

const int WINDOW_WIDTH = 2300;
const int WINDOW_HEIGHT = 1300;
const cv::Scalar BACKGROUND_COLOR = cv::Scalar(49, 52, 49);
const cv::Scalar BACKGROUND_LABELED_COLOR = cv::Scalar(128, 52, 49);
const std::string WINDOW_NAME = "Finder";

// List of participants
std::vector<std::string> participants =
{
	"p1",
	"p2",
	"p3",
	"p4"
};

/////////////////////////////////////////////////
/// Structures
/////////////////////////////////////////////////

// Struct for the stimuli
struct Stimulus
{
	cv::Mat pixels;
	std::string id;
	std::map<std::string, std::set<int> > session_frame_idxs; // frame indices per session
};

// Struct for each frame
struct FrameInfo
{
	std::string session;
	int frame_idx;
	std::map<std::string, double> features; // allows to compare frame with frame-as-represented in stimulus
	bool same = true;
	bool pixel_perfect_same = false;
	std::vector<uchar> video_frame_data; // compressed image data
	std::vector<uchar> stimulus_data; // compressed image data
};

/////////////////////////////////////////////////
/// Main
/////////////////////////////////////////////////

// Main function
int main(int argc, const char** argv)
{
	core::mt::log_info("Welcome to the Finder of VisualStimuliDiscovery!");

	/////////////////////////////////////////////////
	/// Command line arguments
	/////////////////////////////////////////////////

	// Variables to fill
	std::string site = "";
	std::string visual_change_dataset_dir = "";
	std::string stimuli_root_dataset_dir = "";
	std::string output_dir = "";

	// Create options object
	cxxopts::Options options("VisualStimuliDiscovery Finder", "Finder software of the GazeMining project.");
	
	// Add options
	try
	{
		options.add_options()
			("d,visual-change-dataset", "Directory with log records.", cxxopts::value<std::string>()) // e.g., '/home/raphael/Dataset'
			("i,stimuli-root-dataset", "Directory with discovered stimuli of root layer to evaluate.", cxxopts::value<std::string>()) // e.g., "/home/stimuli/0_html"
			("s,site", "Site to work on across participants (e.g., 'nih').", cxxopts::value<std::string>()) // e.g., 'nih'
			("o,output", "Directory for output.", cxxopts::value<std::string>()); // e.g., "/home/finder_output"
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

		// Visual change dataset directory
		if (result.count("visual-change-dataset"))
		{
			visual_change_dataset_dir = result["visual-change-dataset"].as<std::string>();
		}

		// Stimuli of root layer directory
		if (result.count("stimuli-root-dataset"))
		{
			stimuli_root_dataset_dir = result["stimuli-root-dataset"].as<std::string>();
		}

		// Site
		if (result.count("site"))
		{
			site = result["site"].as<std::string>();
		}

		// Output directory
		if (result.count("output"))
		{
			output_dir = result["output"].as<std::string>();
		}
	}
	catch (cxxopts::OptionParseException e)
	{
		std::cerr << e.what() << std::endl;
		return -1;
	}

	/////////////////////////////////////////////////
	/// Load stimuli
	/////////////////////////////////////////////////

	core::mt::log_info("Loading stimuli...");

	// Collect stimuli from root layer
	std::vector<Stimulus> stimuli;
	for (auto& s : fs::directory_iterator(stimuli_root_dataset_dir))
	{
		if (fs::is_regular_file(s) && fs::path(s).extension() == ".png")
		{
			Stimulus stimulus;

			// Load pixels from PNG
			stimulus.pixels = cv::imread(fs::path(s).generic_string(), -1); // store stimuli as BGRA
			stimulus.id = fs::path(s).stem().generic_string();

			// Load list of represented frames from CSV
			std::string shots_filepath = stimuli_root_dataset_dir + "/" + stimulus.id + "-shots.csv";
			core::mt::log_info(shots_filepath); // inform user about processed file
			std::ifstream shots_file(shots_filepath);

			// Load content of file into buffer
			std::stringstream buffer;
			buffer << shots_file.rdbuf();
			shots_file.close();

			// Go over lines
			std::string line;
			std::getline(buffer, line); // skip header
			while (std::getline(buffer, line))
			{
				if (!line.empty())
				{
					auto tokens = core::misc::tokenize(line);
					std::string session = tokens.at(0);
					int frame_idx_start = std::atoi(tokens.at(2).c_str());
					int frame_idx_end = std::atoi(tokens.at(3).c_str());
					for (int i = frame_idx_start; i <= frame_idx_end; ++i)
					{
						stimulus.session_frame_idxs[session].emplace(i);
					}
				}
			}

			stimuli.push_back(stimulus);
		}
	}

	/////////////////////////////////////////////////
	/// Load log records
	/////////////////////////////////////////////////

	core::mt::log_info("Loading log records...");

	// Create list of log records to be parsed
	std::vector<std::string> log_records;
	for (const auto& p : participants)
	{
		log_records.push_back(p + "/" + site); // e.g., 'p1/nih'
	}

	// Create sessions
	auto sp_sessions_tmp = std::make_shared<data::Sessions>();
	for (const auto& r_log_record : log_records)
	{
		// Prepare id without slash (otherwise, serialization screws up)
		std::string id = r_log_record;
		std::replace(id.begin(), id.end(), '/', '_');
		sp_sessions_tmp->push_back(std::make_shared<data::Session>(
			id,
			visual_change_dataset_dir + "/" + r_log_record + ".webm",
			visual_change_dataset_dir + "/" + r_log_record + ".json",
			-1)); // frames to process (-1 means all that are available)
	}

	// Make shared vector with shared sessions const
	auto sp_sessions = core::misc::make_const(sp_sessions_tmp);

	// Create empty oFrameInfoutput of the stage (one log datum container per session)
	auto sp_log_datum_containers = std::make_shared<data::LogDatumContainers>();

	// Have one parser per session to create log dates
	typedef core::Task<stage::processing::parser::LogRecord, 1> ParserTask;
	core::TaskContainer<ParserTask> parsers;
	
	// Create one parser for each session
	for (auto sp_session : *sp_sessions.get())
	{
		// Create parser task
		auto sp_task = std::make_shared<ParserTask>(
			VD(nullptr, ) // provide visual debug dump
			sp_session); // session

		// Put pack into vector of parsers
		parsers.push_back(sp_task);
	}

	// Report about progress on parsing tasks
	parsers.wait_and_report();

	// Collect products for each session
	for (auto& rsp_parser : parsers.get())
	{
		sp_log_datum_containers->push_back(rsp_parser->get_product());
	}

	core::mt::log_info("Tuning log records...");

	// Have one ORBscroll per session
	typedef core::Task<stage::processing::tuning::ORBscroll, 1> ORBscrollTask;
	core::TaskContainer<ORBscrollTask> orb_scrolls;

	// Create one ORBscroll for each log dates container
	for (auto sp_log_dates_container : *sp_log_datum_containers.get())
	{
		// Create ORBscroll task
		auto sp_task = std::make_shared<ORBscrollTask>(
			VD(nullptr, ) // provide visual debug dump
			sp_log_dates_container); // log datum container

		// Put pack into vector of ORBscrolls
		orb_scrolls.push_back(sp_task);
	}

	// Reset local log datum containers to be filled with tuned versions
	sp_log_datum_containers = std::make_shared<data::LogDatumContainers>();

	// Report about progress on ORBscroll tasks
	orb_scrolls.wait_and_report();

	// Collect products for each session
	for (auto& rsp_orb_scroll : orb_scrolls.get())
	{
		sp_log_datum_containers->push_back(rsp_orb_scroll->get_product());
	}
	
	// Make containers const (required by walker)
	auto sp_log_datum_containers_const = core::misc::make_const(sp_log_datum_containers);

	/////////////////////////////////////////////////
	/// Match log images (root-layer-part, only) to stimulus
	/////////////////////////////////////////////////

	std::vector<FrameInfo> frame_infos;
	for (auto sp_log_datum_container : *sp_log_datum_containers_const)
	{
		std::string session = sp_log_datum_container->get_session()->get_id();
		auto up_walker = std::unique_ptr<util::LogDatesWalker>(new util::LogDatesWalker(sp_log_datum_container->get(), sp_log_datum_container->get_session()->get_webm_path()));
		core::mt::log_info("Working on: ", session);

		// Walk over frames
		while (up_walker->step()) // another frame is available
		{
			// Retrieve values for that frame
			auto sp_log_image = up_walker->get_log_image();
			int frame_idx = up_walker->get_frame_idx();
			auto sp_log_datum = up_walker->get_log_datum();
			auto sp_root = sp_log_datum->get_root();
			cv::Mat mask = sp_root->get_view_mask();

			// Inform about progress
			core::mt::log_info("Session: ", session, " Frame: ", frame_idx, "...");

			// Go over stimuli and check where the frame should be represented
			Stimulus* p_stimulus = nullptr;
			for (auto& r_stimulus : stimuli)
			{
				for (int stimulus_frame_idx : r_stimulus.session_frame_idxs[session])
				{
					if (stimulus_frame_idx == frame_idx)
					{
						p_stimulus = &r_stimulus;
						break; // frame should be represented by exactly one stimulus
					}
				}
				if (p_stimulus) { break; } // frame should be represented by exactly one stimulus
			}

			// Warning to the user (may happen when root layer completely masked by fixed elements etc.)
			if (!p_stimulus)
			{
				core::mt::log_info("Frame not found in any stimulus!");
				continue;
			}

			// Create ROI that captures area in stimulus pixels that should represent frame
			auto sp_stimulus_pixels = std::make_shared<cv::Mat>(p_stimulus->pixels.clone());
			cv::Rect ROI(sp_log_datum->get_root()->get_scroll_x(), sp_log_datum->get_root()->get_scroll_y(), sp_root->get_view_width(), sp_root->get_view_height());
			core::opencv::extend(*sp_stimulus_pixels, ROI); // extend stimulus pixels if required (should not be the case)
			*sp_stimulus_pixels = (*sp_stimulus_pixels)(ROI);
			
			// Apply layer mask to both matrices
			auto sp_frame_pixels = std::make_shared<cv::Mat>(sp_log_image->get_viewport_pixels());
			cv::Mat background = cv::Mat::zeros(sp_frame_pixels->size(), sp_frame_pixels->type()); // initialize empty image
			core::opencv::blend(*sp_frame_pixels, background, mask, *sp_frame_pixels); // blend pixels onto it
			core::opencv::blend(*sp_stimulus_pixels, background, mask, *sp_stimulus_pixels); // blend pixels onto it

			// Features
			std::map<std::string, double> features;

			/*

			// Pixel diff
			auto pixel_diff = (feature::PixelDiff(sp_frame_pixels, sp_stimulus_pixels)).get();
			features.insert(pixel_diff.begin(), pixel_diff.end());

			// Edge change ratio
			auto edge_change_ratio = (feature::EdgeChangeFraction(sp_frame_pixels, sp_stimulus_pixels)).get();
			features.insert(edge_change_ratio.begin(), edge_change_ratio.end());

			// MSSIM
			auto mssim = (feature::MSSIM(sp_frame_pixels, sp_stimulus_pixels)).get();
			features.insert(mssim.begin(), mssim.end());

			// PSNR
			auto psnr = (feature::PSNR(sp_frame_pixels, sp_stimulus_pixels)).get();
			features.insert(psnr.begin(), psnr.end());

			// SIFT
			auto sift_match = (feature::SiftMatch(sp_frame_pixels, sp_stimulus_pixels)).get();
			features.insert(sift_match.begin(), sift_match.end());

			*/

			// Put into frame info
			FrameInfo frame_info;
			frame_info.session = session;
			frame_info.frame_idx = frame_idx;
			frame_info.features = features;
			frame_info.pixel_perfect_same = core::opencv::pixel_perfect_same(*sp_frame_pixels, *sp_stimulus_pixels);;
			if(!frame_info.pixel_perfect_same)
			{
				cv::imencode(".png", *sp_frame_pixels, frame_info.video_frame_data);
				cv::imencode(".png", *sp_stimulus_pixels, frame_info.stimulus_data);
			}
			
			frame_infos.push_back(frame_info);
		}
	}
	
#ifdef GM_VISUAL_DEBUG
	core::mt::log_info("Show user interface...");

	// Create a frame where components will be rendered into
	cv::Mat frame = cv::Mat(WINDOW_HEIGHT, WINDOW_WIDTH, CV_8UC3);
	
	// Init cvui and tell it to create a OpenCV window, i.e., cv::namedWindow(window_name)
	cvui::init(WINDOW_NAME);
	
	// General
	bool exit = false;
	int idx = 0;
	
	// Collect frame infos to display (non-perfect)
	std::vector<FrameInfo*> frame_infos_to_display;
	for(auto& r_frame_info : frame_infos)
	{
		if(!r_frame_info.pixel_perfect_same)
		{
			frame_infos_to_display.push_back(&r_frame_info);
		}
	}
	int count = (int)frame_infos_to_display.size();
	
	// Render loop
	while (!exit)
	{
		// Fetch keyboard key
		auto key = cv::waitKeyEx(20);
		
		// Display content from current frame info
		FrameInfo* p_frame_info = frame_infos_to_display.at(idx);
		
		// Fill the frame with a nice color
		if(p_frame_info->same)
		{
			frame = BACKGROUND_LABELED_COLOR;
		}
		else
		{
			frame = BACKGROUND_COLOR;
		}
		
		cvui::beginColumn(frame, 0, 0);
		cvui::space(5);
		
		// Image data
		cvui::beginRow();
		cv::Mat video_frame_mat = cv::imdecode(p_frame_info->video_frame_data, cv::IMREAD_COLOR); // BGR is sufficient
		cv::Mat stimulus_mat = cv::imdecode(p_frame_info->stimulus_data, cv::IMREAD_COLOR); // BGR is sufficient
		cvui::space(5);
		cvui::image(video_frame_mat);
		cvui::space(5);
		cvui::image(stimulus_mat);
		cvui::endRow();
		cvui::space(5);
		
		// Control panel
		cvui::beginRow();
		cvui::space(5);
		cvui::text("Left: Screencast Frame    Right: Stimulus ROI");
		cvui::space(30);
		cvui::text("Session: " + p_frame_info->session + " Frame idx: " + std::to_string(p_frame_info->frame_idx));
		cvui::space(5);
		if(cvui::button("<-")) { --idx; idx = core::math::clamp(idx, 0, count-1); }
		if(cvui::button("->")) { ++idx; idx = core::math::clamp(idx, 0, count-1); }
		cvui::space(5);
		cvui::checkbox("Same", &(p_frame_info->same));
		cvui::space(20);
		if(cvui::button("Store Labeling"))
		{
			// Get available features
			std::vector<std::string> feature_names;
			for (auto& r_item : frame_infos.at(0).features)
			{
				feature_names.push_back(r_item.first);
			}
			int feature_count = (int)feature_names.size();
	
			std::ofstream out(output_dir + "/" + site + "_finder.csv");
			out << "session,";
			out << "frame_idx,";
			for (int i = 0; i < feature_count; ++i)
			{
				out << feature_names.at(i);
				out << ",";
			}
			out << "same,";
			out << "pixel_perfect_same\n";
	
			for (auto& r_frame_info : frame_infos)
			{
				out << r_frame_info.session << ",";
				out << r_frame_info.frame_idx << ",";
				for (int i = 0; i < feature_count; ++i)
				{
					out << r_frame_info.features[feature_names.at(i)];
					out << ",";
				}
				out << (r_frame_info.same ? "1" : "0") << ",";
				out << (r_frame_info.pixel_perfect_same ? "1" : "0") << "\n";
			}
		}
		cvui::endRow();
		cvui::space(5);
		
		// Diff image
		cvui::beginRow();
		cvui::space(768);
		cv::Mat diff_mat;
		cv::Mat video_frame_mat_gray, stimulus_mat_gray;
		cv::cvtColor(video_frame_mat, video_frame_mat_gray, cv::COLOR_BGR2GRAY); // Y would be even better
		cv::cvtColor(stimulus_mat, stimulus_mat_gray, cv::COLOR_BGR2GRAY); // Y would be even better
		cv::absdiff(video_frame_mat_gray, stimulus_mat_gray, diff_mat);
		float scale = 0.5f;
		cv::Mat diff_mat_scaled;
		cv::resize(
			diff_mat,
			diff_mat_scaled,
			cv::Size(
				(int)(scale * (float)diff_mat.cols),
				(int)(scale * (float)diff_mat.rows)),
				0,
				0,
				cv::INTER_LINEAR);
		cv::cvtColor(diff_mat_scaled, diff_mat_scaled, cv::COLOR_GRAY2BGR);
		cvui::image(diff_mat_scaled);
		cvui::endRow();
		
		cvui::endColumn();
		
		// Update cvui stuff and show everything on the screen
		cvui::update(WINDOW_NAME);
		cv::imshow(WINDOW_NAME, frame);
	
		// Handle keyboard
		switch(key)
		{
		case 65361: // left (does not work under Windows)
			--idx; idx = core::math::clamp(idx, 0, count-1);
			break;
		case 65363: // right (does not work under Windows)
			++idx; idx = core::math::clamp(idx, 0, count-1);
			break;
		case 13: // return
			p_frame_info->same = !p_frame_info->same;
			break;
		case 27: // ESC
			cv::destroyWindow(WINDOW_NAME);
			break;
		}
	
		// Exit if no window is open at this point
		if (!core::opencv::is_window_open(WINDOW_NAME))
		{
			exit = true;
		}
	}
#else
	core::mt::log_info("Cannot show user interface as compiled without support for visual debug!");
#endif

	/////////////////////////////////////////////////
	/// Exit
	/////////////////////////////////////////////////

	// Wait for user to press any key
	core::mt::log_info("Exit application!");
	return 0;
}
