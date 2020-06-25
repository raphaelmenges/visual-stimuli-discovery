#include <Core/Core.hpp>
#include <Core/VisualDebug.hpp>
#include <cxxopts.hpp>
#include <libsimplewebm.hpp>
#include <chrono>
#include <fstream>
#include <algorithm>
#include <experimental/filesystem>
#include <future>
#include <set>

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
const int WINDOW_WIDTH = 1900;
const int WINDOW_HEIGHT = 1000;
const cv::Scalar DEFAULT_BACKGROUND_COLOR = cv::Scalar(49, 52, 49);
const std::string WINDOW_NAME = "Preciser";

/////////////////////////////////////////////////
/// Structures
/////////////////////////////////////////////////

struct StimulusLabel
{
	std::string layer_id;
	std::string stimulus_id;
	std::string label;
};

enum class PreciserLabel { POS_CONTRIB, NEG_CONTRIB, NEUTRAL };

std::string to_string(PreciserLabel label)
{
	switch(label)
	{
	case PreciserLabel::POS_CONTRIB:
		return "POS_CONTRIB";
	case PreciserLabel::NEG_CONTRIB:
		return "NEG_CONTRIB";
	case PreciserLabel::NEUTRAL:
		return "NEUTRAL";
	}
}

/////////////////////////////////////////////////
/// Main
/////////////////////////////////////////////////

// Main function
int main(int argc, const char** argv)
{
	core::mt::log_info("Welcome to the Preciser of VisualStimuliDiscovery!");

	/////////////////////////////////////////////////
	/// Command line arguments
	/////////////////////////////////////////////////

	// Variables to fill
	std::string visual_change_dataset_dir = "";
	std::string stimuli_dataset_dir = "";
	std::string evaluation_dataset_dir = "";
	std::string site = "";
	std::string evaluation_id = "";

	// Create options object
	cxxopts::Options options("VisualStimuliDiscovery Preciser", "Preciser software of the GazeMining project.");
	
	// Add options
	try
	{
		options.add_options()
			("d,visual-change-dataset", "Directory with log records.", cxxopts::value<std::string>()) // e.g., '/home/raphael/Dataset'
			("i,stimuli-dataset", "Directory with discovered stimuli to evaluate.", cxxopts::value<std::string>()) // e.g., "/home/stimuli"
			("o,evaluation-dataset", "Directory for output.", cxxopts::value<std::string>()) // e.g., "/home/evaluator"
			("s,site", "Site to work on across participants (e.g., 'nih').", cxxopts::value<std::string>()) // e.g., 'nih'
			("e,evaluation", "Identifier of evaluation.", cxxopts::value<std::string>()); // e.g., "e1-webmd"
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
		
		// Stimuli dataset directory
		if (result.count("stimuli-dataset"))
		{
			stimuli_dataset_dir = result["stimuli-dataset"].as<std::string>();
		}
		
		// Evaluation dataset directory
		if (result.count("evaluation-dataset"))
		{
			evaluation_dataset_dir = result["evaluation-dataset"].as<std::string>();
		}
		
		// Site
		if (result.count("site"))
		{
			site = result["site"].as<std::string>();
		}
		
		// Evaluation id
		if (result.count("evaluation"))
		{
			evaluation_id = result["evaluation"].as<std::string>();
		}
	}
	catch (cxxopts::OptionParseException e)
	{
		std::cerr << e.what() << std::endl;
		return -1;
	}

	/////////////////////////////////////////////////
	/// Load screencasts
	/////////////////////////////////////////////////

	core::mt::log_info("Loading videos...");

	// Compressed video frame
	struct VideoFrame
	{
		std::vector<uchar> data;
		double time = 0.0;
	};

	// Function to load video frames
	auto load_video = [](std::string filepath) -> std::shared_ptr<std::vector<VideoFrame> >
	{
		auto walker = simplewebm::create_video_walker(filepath);
		auto sp_images = std::shared_ptr<std::vector<simplewebm::Image> >(new std::vector<simplewebm::Image>);
		auto sp_product = std::shared_ptr<std::vector<VideoFrame> >(new std::vector<VideoFrame>);
		while(true)
		{
			auto status = walker->walk(sp_images, 1);
			if(status == simplewebm::Status::OK ||status == simplewebm::Status::DONE)
			{
				for(const auto& r_image : *sp_images)
				{
					cv::Mat mat(r_image.height, r_image.width, CV_8UC3, const_cast<char *>(r_image.data.data()));
					VideoFrame video_frame;
					cv::imencode(".png", mat, video_frame.data);
					video_frame.time = r_image.time;
					sp_product->push_back(video_frame);
				}
			}
			sp_images->clear();
			if(status != simplewebm::Status::OK) { break; }
		}
		return sp_product;
	};

	// Asynchroniously load videos
	auto future_video_frames_p1(std::async(std::launch::async, load_video , visual_change_dataset_dir + "/p1/" + site + ".webm"));
	auto future_video_frames_p2(std::async(std::launch::async, load_video , visual_change_dataset_dir + "/p2/" + site + ".webm"));
	auto future_video_frames_p3(std::async(std::launch::async, load_video , visual_change_dataset_dir + "/p3/" + site + ".webm"));
	auto future_video_frames_p4(std::async(std::launch::async, load_video , visual_change_dataset_dir + "/p4/" + site + ".webm"));

	// Prepare variables for videos mode
	auto sp_video_frames_p1 = future_video_frames_p1.get();
	auto sp_video_frames_p2 = future_video_frames_p2.get();
	auto sp_video_frames_p3 = future_video_frames_p3.get();
	auto sp_video_frames_p4 = future_video_frames_p4.get();
	
#ifdef GM_VISUAL_DEBUG
	core::mt::log_info("Show user interface...");

	// Create a frame where components will be rendered into
	cv::Mat frame = cv::Mat(WINDOW_HEIGHT, WINDOW_WIDTH, CV_8UC3);
	
	// Init cvui and tell it to create a OpenCV window, i.e., cv::namedWindow(window_name)
	cvui::init(WINDOW_NAME);
	
	// General
	bool exit = false;
	
	// Frames to display
	std::map<std::string, std::vector<std::pair<int, PreciserLabel> > > frame_entries; // session mapped to pair of frame idx and preciser label
	
	// Load evaluator file with stimuli 
	std::vector<StimulusLabel> marked_stimuli;
	
	// Make own scope for file handling
	{
		std::string labels_stimuli_filepath = evaluation_dataset_dir + "/" + evaluation_id + "-stimuli.csv";
		std::ifstream labels_stimuli_file(labels_stimuli_filepath);

		// Load content of file into buffer
		std::stringstream buffer;
		buffer << labels_stimuli_file.rdbuf();
		labels_stimuli_file.close();
	
		// Go over lines
		std::vector<StimulusLabel> labels;
		std::string line;
		std::getline(buffer, line); // skip header
		while(std::getline(buffer, line))
		{
			if(!line.empty())
			{
				StimulusLabel item;
				auto tokens = core::misc::tokenize(line);
				item.layer_id = tokens.at(0);
				item.stimulus_id = tokens.at(1);
				item.label = tokens.at(2);
				labels.push_back(item);
			}
		}
		
		// Filter for marked stimulis
		for(auto stimulus_label : labels)
		{
			if(stimulus_label.label == "1")
			{
				marked_stimuli.push_back(stimulus_label);
				core::mt::log_info("Layer id: ", stimulus_label.layer_id, " Stimulus id: ", stimulus_label.stimulus_id);
			}
		}
	}
	
	// Read information about marked stimuli to know which frames to display
	for(auto stimulus_label : marked_stimuli)
	{
		// Own scope
		{
			std::string shots_filepath = stimuli_dataset_dir + "/" + site + "/stimuli/" + stimulus_label.layer_id + "/" + stimulus_label.stimulus_id + "-shots.csv";
			std::ifstream shots_file(shots_filepath);
			
			// Load content of file into buffer
			std::stringstream buffer;
			buffer << shots_file.rdbuf();
			shots_file.close();
			
			// Go over lines
			std::string line;
			std::getline(buffer, line); // skip header
			while(std::getline(buffer, line))
			{
				if(!line.empty())
				{
					// Extract session and frame indices
					auto tokens = core::misc::tokenize(line);
					std::string session = tokens.at(0);
					int frame_idx_start = std::atoi(tokens.at(2).c_str());
					int frame_idx_end = std::atoi(tokens.at(3).c_str());
					core::mt::log_info("Session: ", session, " Start: ", frame_idx_start, " End: ", frame_idx_end);
					
					// Create array of frames
					std::vector<std::pair<int, PreciserLabel> > frame_entry_array;
					for(int i = frame_idx_start; i <= frame_idx_end; ++i)
					{
						frame_entry_array.push_back({ i, PreciserLabel::NEUTRAL });
					}
					
					// Put information in overall structure
					auto iter = frame_entries.find(session);
					if(iter != frame_entries.end())
					{
						for(auto item : frame_entry_array)
						{
							(*iter).second.push_back(item);
						}
					}
					else
					{
						frame_entries[session] = frame_entry_array;
					}
					
					// Make frames unique
					auto& r_vector = frame_entries[session];
					r_vector.erase(std::unique(r_vector.begin(), r_vector.end() ), r_vector.end());
				}
			}
		}
	}

	// Render loop
	std::string current_session = "p1_" + site;
	int frame_entry_idx = 0;
	while (!exit)
	{
		// Background
		frame = DEFAULT_BACKGROUND_COLOR;

		// Fetch keyboard key
		auto key = cv::waitKeyEx(20);

		cvui::beginColumn(frame, 0, 0);

		// Choose session
		cvui::beginRow();
		for(const auto& r_session_frame_entries : frame_entries)
		{
			if(cvui::button(r_session_frame_entries.first))
			{
				current_session = r_session_frame_entries.first;
				frame_entry_idx = 0;
			}
		}
		cvui::endRow();
		cvui::space(5);

		// Get the correct session
		auto sp_video_frames = sp_video_frames_p1;
		if(current_session == ("p2_" + site))
		{
			sp_video_frames = sp_video_frames_p2;
		}
		else if(current_session == ("p3_" + site))
		{
			sp_video_frames = sp_video_frames_p3;
		}
		else if(current_session == ("p4_" + site))
		{
			sp_video_frames = sp_video_frames_p4;
		}
		int frame_entries_count = (int)frame_entries[current_session].size();

		// Controls
		cvui::beginRow();

		// Choose frame to display
		cvui::space(10);
		if(cvui::button("<-")) { --frame_entry_idx; frame_entry_idx = std::max(0, frame_entry_idx); }
		if(cvui::button("->")) { ++frame_entry_idx; frame_entry_idx = std::min(frame_entries_count-1, frame_entry_idx); }
		cvui::text(std::to_string(frame_entry_idx + 1) + "/" + std::to_string(frame_entries_count));
		
		// Choose label
		cvui::space(10);
		auto label = frame_entries[current_session].at(frame_entry_idx).second;
		bool pos_contrib = label == PreciserLabel::POS_CONTRIB;
		bool neg_contrib = label == PreciserLabel::NEG_CONTRIB;
		bool neutral = label == PreciserLabel::NEUTRAL;
		cvui::checkbox("POS_CONTRIB", &pos_contrib);
		cvui::checkbox("NEG_CONTRIB", &neg_contrib);
		cvui::checkbox("NEUTRAL", &neutral);
		
		// Save button
		cvui::space(10);
		if(cvui::button("Store Labeling"))
		{
			std::ofstream out(evaluation_dataset_dir + "/" + evaluation_id + "-contrib.csv");
			out << "session,";
			out << "frame_idx,";
			out << "label\n";
			
			for(const auto& r_session : frame_entries)
			{
				for(const auto& r_entry : r_session.second)
				{
					out << r_session.first << ","; // session
					out << std::to_string(r_entry.first) << ","; // frame_idx
					out << to_string(r_entry.second) << "\n"; // preciser label
				}
			}
		}
		
		cvui::endRow();
		cvui::space(5);
		
		// Display frame from screencast
		cvui::beginRow();
		int frame_idx = -1;
		if(frame_entry_idx >= 0)
		{
			frame_idx = frame_entries[current_session].at(frame_entry_idx).first;
			cv::Mat video_frame = cv::imdecode(sp_video_frames->at(frame_idx).data, cv::IMREAD_COLOR);
			cvui::image(video_frame);
		}
		cvui::endRow();
		
		// Display frame index as in screencast
		cvui::beginRow();
		cvui::text("Screencast Frame idx: " + std::to_string(frame_idx));
		cvui::endRow();
		
		cvui::endColumn();

		// Update cvui stuff and show everything on the screen
		cvui::update(WINDOW_NAME);
		cv::imshow(WINDOW_NAME, frame);

		// Handle keyboard
		switch(key)
		{
		case 65361: // left (does not work under Windows)
			--frame_entry_idx;
			frame_entry_idx = std::max(0, frame_entry_idx);
			break;
		case 65363: // right (does not work under Windows)
			++frame_entry_idx;
			frame_entry_idx = std::min(frame_entries_count-1, frame_entry_idx);
			break;
		case 65362: // up (does not work under Windows)
			frame_entries[current_session].at(frame_entry_idx).second = PreciserLabel::POS_CONTRIB;
			break;
		case 65364: // down (does not work under Windows)
			frame_entries[current_session].at(frame_entry_idx).second = PreciserLabel::NEG_CONTRIB;
			break;
		case 65288: // backspace (does not work under Windows)
			frame_entries[current_session].at(frame_entry_idx).second = PreciserLabel::NEUTRAL;
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

	core::mt::log_info("Exit application!");
	return 0;
}
