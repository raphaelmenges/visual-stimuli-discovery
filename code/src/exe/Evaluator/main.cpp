#include <Core/Core.hpp>
#include <Core/VisualDebug.hpp>
#include <cxxopts.hpp>
#include <libsimplewebm.hpp>
#include <chrono>
#include <fstream>
#include <algorithm>
#include <experimental/filesystem>
#include <future>

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
const int TASK_COLUMN_WIDTH = 800;
const int LABEL_COLUMN_WIDTH = 1050;
const double WIND_SPEED = 2.0;
const cv::Scalar DEFAULT_BACKGROUND_COLOR = cv::Scalar(49, 52, 49);
const cv::Scalar AUTO_LABELING_BACKGROUND_COLOR = cv::Scalar(49, 52, 100);
const std::string WINDOW_NAME = "Evaluator";
const int HIGHLIGHT_BORDER_SIZE = 6;

/////////////////////////////////////////////////
/// Structures
/////////////////////////////////////////////////

enum class Playback { Pause, Play, FastBackward, FastForward };
enum class Mode { Start, Videos, Stimuli, End };

std::string mode_to_string(Mode mode)
{
	switch(mode)
	{
		case Mode::Start:
			return "mode_start";
		case Mode::Videos:
			return "mode_videos";
		case Mode::Stimuli:
			return "mode_stimuli";
		case Mode::End:
			return "mode_end";
	}
	return "";
}

/////////////////////////////////////////////////
/// Main
/////////////////////////////////////////////////

// Main function
int main(int argc, const char** argv)
{
	core::mt::log_info("Welcome to the Evaluator of VisualStimuliDiscovery!");

	/////////////////////////////////////////////////
	/// Command line arguments
	/////////////////////////////////////////////////

	// Variables to fill
	std::string task_filepath = "";
	std::string dataset_dir = "";
	std::string site = "";
	std::string stimuli_dataset_dir = "";
	std::string output_dir = "";
	std::string evaluation_id = "";
	bool video_first = false;

	// Create options object
	cxxopts::Options options("VisualStimuliDiscovery Evaluator", "Evaluator software of the GazeMining project.");
	
	// Add options
	try
	{
		options.add_options()
			("d,visual-change-dataset", "Directory with log records.", cxxopts::value<std::string>()) // e.g., '/home/raphael/Dataset'
			("i,stimuli-dataset", "Directory with discovered stimuli to evaluate.", cxxopts::value<std::string>()) // e.g., "/home/stimuli"
			("t,task", "Filepath to image of the task.", cxxopts::value<std::string>()) // e.g., "/home/task.png"
			("s,site", "Site to work on across participants (e.g., 'nih').", cxxopts::value<std::string>()) // e.g., 'nih'
			("o,output", "Directory for output.", cxxopts::value<std::string>()) // e.g., "/home/evaluator"
			("e,evaluation", "Identifier of evaluation.", cxxopts::value<std::string>()) // e.g., "e1-webmd"
			("v,video-first", "First screencast and then stimuli are presented.", cxxopts::value<bool>(video_first));
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

		// Task filepath
		if (result.count("task"))
		{
			task_filepath = result["task"].as<std::string>();
		}

		// Visual change dataset directory
		if (result.count("visual-change-dataset"))
		{
			dataset_dir = result["visual-change-dataset"].as<std::string>();
		}

		// Site
		if (result.count("site"))
		{
			site = result["site"].as<std::string>();
		}

		// Stimuli directory
		if (result.count("stimuli-dataset"))
		{
			stimuli_dataset_dir = result["stimuli-dataset"].as<std::string>();
		}

		// Output directory
		if (result.count("output"))
		{
			output_dir = result["output"].as<std::string>();
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
	/// Load stimuli
	/////////////////////////////////////////////////

	core::mt::log_info("Loading stimuli...");

	// Struct for the stimuli
	struct Stimulus
	{
		cv::Mat pixels;
		std::string layer_id;
		std::string id;
	};
	std::vector<Stimulus> stimuli;

	// Collect stimuli
	for(auto& d: fs::directory_iterator(stimuli_dataset_dir))
	{
		if(fs::is_directory(d))
		{
			for(auto& s: fs::directory_iterator(d))
			{
				if(fs::is_regular_file(s) && fs::path(s).extension() == ".png")
				{
					Stimulus stimulus;
					auto bgra_pixels = cv::imread(fs::path(s).generic_string(), -1);
					auto rect = core::opencv::covering_rect_bgra(bgra_pixels); // only covered rect is taken
					cv::cvtColor(bgra_pixels(rect), stimulus.pixels, cv::COLOR_BGRA2BGR); // store stimuli as BGR
					stimulus.layer_id =  fs::path(d).filename().generic_string();
					stimulus.id = fs::path(s).stem().generic_string();
					stimuli.push_back(stimulus);
				}
			}
		}
	}

	// Sort stimuli (even so, 10 comes before 2 because strings and not integers are used)
	std::sort(stimuli.begin(), stimuli.end(), [] (const Stimulus& a, const Stimulus& b)
	{
		// First, compare length of layer ids (the longer the layer id, the deeper the layer)
		if(a.layer_id.length() < b.layer_id.length())
		{
			return true;
		}
		if(a.layer_id.length() > b.layer_id.length())
		{
			return false;
		}

		// In case the layer ids have the same length, compare on character level
		int tmp = a.layer_id.compare(b.layer_id);
		if(tmp != 0)
		{
			return tmp < 0;
			
		}

		// Else, compare the stimulis ids directly
		return a.id.compare(b.id) < 0;
	});

	// Labels
	auto sp_stimuli_labels = std::make_shared<std::vector<bool> >(stimuli.size(), false);

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
	auto future_video_frames_p1(std::async(std::launch::async, load_video , dataset_dir + "/p1/" + site + ".webm"));
	auto future_video_frames_p2(std::async(std::launch::async, load_video , dataset_dir + "/p2/" + site + ".webm"));
	auto future_video_frames_p3(std::async(std::launch::async, load_video , dataset_dir + "/p3/" + site + ".webm"));
	auto future_video_frames_p4(std::async(std::launch::async, load_video , dataset_dir + "/p4/" + site + ".webm"));

	// Prepare variables for videos mode
	auto sp_video_frames_p1 = future_video_frames_p1.get();
	auto sp_video_frames_p2 = future_video_frames_p2.get();
	auto sp_video_frames_p3 = future_video_frames_p3.get();
	auto sp_video_frames_p4 = future_video_frames_p4.get();
	auto sp_video_frame_labels_p1 = std::make_shared<std::vector<bool> >(sp_video_frames_p1->size(), false);
	auto sp_video_frame_labels_p2 = std::make_shared<std::vector<bool> >(sp_video_frames_p2->size(), false);
	auto sp_video_frame_labels_p3 = std::make_shared<std::vector<bool> >(sp_video_frames_p3->size(), false);
	auto sp_video_frame_labels_p4 = std::make_shared<std::vector<bool> >(sp_video_frames_p4->size(), false);

#ifdef GM_VISUAL_DEBUG
	core::mt::log_info("Show user interface...");

	// Create a frame where components will be rendered into
	cv::Mat frame = cv::Mat(WINDOW_HEIGHT, WINDOW_WIDTH, CV_8UC3);
	
	// Init cvui and tell it to create a OpenCV window, i.e., cv::namedWindow(window_name)
	cvui::init(WINDOW_NAME);

	// General
	std::vector<Mode> modes;
	if(video_first)
	{
		modes = { Mode::Start, Mode::Videos, Mode::Stimuli, Mode::End };
	}
	else
	{
		modes = { Mode::Start, Mode::Stimuli, Mode::Videos, Mode::End };
	}
	int mode_idx = 0;
	bool exit = false;
	auto last_time = std::chrono::system_clock::now();
	cv::Mat task_image = cv::imread(task_filepath, cv::IMREAD_COLOR);
	
	// Videos Mode
	double video_time = 0.0;
	Playback playback = Playback::Pause;
	bool auto_labeling = false;
	int prev_frame_idx = -1;
	std::string video_participant = "P1";
	
	// Stimuli Mode
	int stimuli_idx = 0;
	int prev_stimuli_idx = -1;
	int viewport_scrolling_y = 0;
	
	// Simple logging to later measure timings
	class SimpleLog
	{
	public:

		SimpleLog(std::string output_dir, std::string evaluation_id)
		{
			_start_time = std::chrono::steady_clock::now();
			_out.open(output_dir + "/" + evaluation_id + "-events.csv");
		}
		
		~SimpleLog()
		{
			_out.close();
		}

		// Push event (time is taken automatically)
		void push_event(std::string type, std::string value = "")
		{
			auto local_time = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - _start_time);
			_out << std::to_string(local_time.count()) << ",";
			_out << type << ",";
			_out << value << "\n";
		}
		
	private:

		// Members
		std::chrono::steady_clock::time_point _start_time;
		std::ofstream _out;
	};

	// Render loop
	SimpleLog simple_log(output_dir, evaluation_id);
	simple_log.push_event("mode_change", mode_to_string(modes.at(mode_idx))); // initial mode
	while (!exit)
	{
		// Background
		if(auto_labeling)
		{
			frame = AUTO_LABELING_BACKGROUND_COLOR;
		}
		else
		{
			frame = DEFAULT_BACKGROUND_COLOR;
		}

		// Delta time
		auto curr_time = std::chrono::system_clock::now();
		std::chrono::duration<double> delta = curr_time-last_time; // delta time in seconds
		double delta_time = delta.count();

		// Fetch keyboard key
		auto key = cv::waitKeyEx(20);

		// Render current mode
		Mode mode = modes.at(mode_idx);

		// #############
		// ### START ###
		// #############

		if(mode == Mode::Start)
		{
			int h_padding = (WINDOW_WIDTH - task_image.cols) / 2;
		
			cvui::beginRow(frame, h_padding, 0, task_image.rows, WINDOW_HEIGHT);
			cvui::beginColumn();
			cvui::space(200);
			cvui::image(task_image);
			cvui::space(5);
			cvui::text("Task: Look for this element.", 0.7);
			cvui::space(10);
			cvui::text("Press enter to start!", 0.9);

			// Handle keyboard
			switch(key)
			{
			case 13: // return
				++mode_idx;
				simple_log.push_event("mode_change", mode_to_string(modes.at(mode_idx)));
				break;
			}

			cvui::endColumn();
			cvui::endRow();
		}

		// ###########
		// ### END ###
		// ###########

		else if(mode == Mode::End)
		{
			core::mt::log_info("Store labels...");

			// Store stimuli labels
			{
				std::ofstream stimuli_labels_out(output_dir + "/" + evaluation_id + "-stimuli.csv");
				stimuli_labels_out << "layer_id,";
				stimuli_labels_out << "stimulus_id,";
				stimuli_labels_out << "label\n";
				
				for(int i = 0; i < (int)sp_stimuli_labels->size(); ++i)
				{
					stimuli_labels_out << stimuli.at(i).layer_id << ",";
					stimuli_labels_out << stimuli.at(i).id << ",";
					stimuli_labels_out << std::to_string(sp_stimuli_labels->at(i)) << "\n";
				}
			}
			
			// Store video labels
			{
				std::ofstream video_labels_out(output_dir + "/" + evaluation_id + "-screencasts.csv");
				video_labels_out << "P1,";
				video_labels_out << "P2,";
				video_labels_out << "P3,";
				video_labels_out << "P4\n";
				
				// Go over maximum of available frames across videos
				int p1_count = (int)sp_video_frame_labels_p1->size();
				int p2_count = (int)sp_video_frame_labels_p2->size();
				int p3_count = (int)sp_video_frame_labels_p3->size();
				int p4_count = (int)sp_video_frame_labels_p4->size();
				int max_count = std::max(p1_count, std::max(p2_count, std::max(p3_count, p4_count)));
				
				for(int i = 0; i < max_count; ++i)
				{
					if(i < p1_count) {video_labels_out << std::to_string(sp_video_frame_labels_p1->at(i)); } video_labels_out << ",";
					if(i < p2_count) {video_labels_out << std::to_string(sp_video_frame_labels_p2->at(i)); } video_labels_out << ",";
					if(i < p3_count) {video_labels_out << std::to_string(sp_video_frame_labels_p3->at(i)); } video_labels_out << ",";
					if(i < p4_count) {video_labels_out << std::to_string(sp_video_frame_labels_p4->at(i)); } video_labels_out << "\n";
				}
			}
			
			// Finally exit the application
			exit = true;
			simple_log.push_event("exit");
		}

		// ##############
		// ### VIDEOS ###
		// ##############

		else if(mode == Mode::Videos)
		{
			// Set pointers to correct screencast and labels
			auto sp_video_frames = sp_video_frames_p1;
			auto sp_video_frame_labels = sp_video_frame_labels_p1;
			if(video_participant == "P2")
			{
				sp_video_frames = sp_video_frames_p2;
				sp_video_frame_labels = sp_video_frame_labels_p2;
			}
			if(video_participant == "P3")
			{
				sp_video_frames = sp_video_frames_p3;
				sp_video_frame_labels = sp_video_frame_labels_p3;
			}
			if(video_participant == "P4")
			{
				sp_video_frames = sp_video_frames_p4;
				sp_video_frame_labels = sp_video_frame_labels_p4;
			}

			// Fetch some values
			const double video_duration = sp_video_frames->back().time;

			// Video playback
			switch(playback)
			{
				case Playback::Pause:
					// Do nothing
					break;
				case Playback::Play:
					video_time += delta_time; // continue playing
					video_time = std::min(video_duration, video_time);
					break;
				case Playback::FastBackward:
					video_time -= WIND_SPEED * delta_time;
					video_time = std::max(0.0, video_time);
					break;
				case Playback::FastForward:
					video_time += WIND_SPEED * delta_time; // continue playing
					video_time = std::min(video_duration, video_time);
					break;
			}

			// Fetch video frame to display
			int frame_idx = 0;
			for(; frame_idx < (int)sp_video_frames->size(); ++frame_idx)
			{
				if((int)sp_video_frames->size() <= (frame_idx+1) || sp_video_frames->at(frame_idx).time > video_time)
				{
					break;
				}
			}
			cv::Mat video_frame = cv::imdecode(sp_video_frames->at(frame_idx).data, cv::IMREAD_COLOR);

			// Auto labeling
			if(auto_labeling && prev_frame_idx >= 0)
			{
				if(prev_frame_idx < frame_idx) // forward, label as true
				{
					sp_video_frame_labels->at(prev_frame_idx) = true;
				}
				else if(prev_frame_idx > frame_idx) // backward, label as false
				{
					sp_video_frame_labels->at(frame_idx) = false;
					if(frame_idx < (int)sp_video_frames->size() -1)
					{
						sp_video_frame_labels->at(frame_idx+1) = false;
					}
				}
			}
			
			// Begin drawing onto frame
			int h_padding = (WINDOW_WIDTH - (TASK_COLUMN_WIDTH + LABEL_COLUMN_WIDTH)) / 3;
			cvui::beginRow(frame, 0, 0, WINDOW_WIDTH, WINDOW_HEIGHT, 0);

			// ###################
			// ### TASK COLUMN ###
			// ###################
			cvui::beginColumn(frame, h_padding, 0, TASK_COLUMN_WIDTH);

			cvui::space(200);
			auto scaled_task_image = core::opencv::scale_to_fit(task_image, TASK_COLUMN_WIDTH, WINDOW_HEIGHT - 256);
			int h_task_padding = (TASK_COLUMN_WIDTH - scaled_task_image.cols) / 2;
			cvui::beginRow();
				cvui::space(h_task_padding);
				cvui::image(scaled_task_image);
			cvui::endRow();
			cvui::space(5);
			cvui::beginRow();
				cvui::space(h_task_padding);
				cvui::text("Task: Look for this element", 0.7);
			cvui::endRow();

			cvui::endColumn();

			// ####################
			// ### LABEL COLUMN ###
			// ####################
			cvui::beginColumn(frame, 2 * h_padding + TASK_COLUMN_WIDTH, 0, LABEL_COLUMN_WIDTH, WINDOW_HEIGHT);
			cvui::space(20);

			// General controls
			cvui::beginRow();
				cvui::text(video_participant, 0.7);
				if(cvui::button("P1"))
				{
					video_participant = "P1";
					simple_log.push_event("change_session", video_participant);
					
					// Reset the video state
					video_time = 0.0;
					playback = Playback::Pause;
					auto_labeling = false;
					prev_frame_idx = -1;
				}
				if(cvui::button("P2"))
				{
					video_participant = "P2";
					simple_log.push_event("change_session", video_participant);
					
					// Reset the video state
					video_time = 0.0;
					playback = Playback::Pause;
					auto_labeling = false;
					prev_frame_idx = -1;
				}
				if(cvui::button("P3"))
				{
					video_participant = "P3";
					simple_log.push_event("change_session", video_participant);
					
					// Reset the video state
					video_time = 0.0;
					playback = Playback::Pause;
					auto_labeling = false;
					prev_frame_idx = -1;
				}
				if(cvui::button("P4"))
				{
					video_participant = "P4";
					simple_log.push_event("change_session", video_participant);
					
					// Reset the video state
					video_time = 0.0;
					playback = Playback::Pause;
					auto_labeling = false;
					prev_frame_idx = -1;
				}
				cvui::space(5); // TODO: this is both horizontally and vertically applied?!
				if(cvui::button("Done with all videos!"))
				{
					// Go to next mode
					++mode_idx;
					simple_log.push_event("mode_change", mode_to_string(modes.at(mode_idx)));
				}
			cvui::endRow();
			cvui::space(5);
			
			// Highlight border in viewport if labeled with true
			if(sp_video_frame_labels->at(frame_idx))
			{
				const int bz = HIGHLIGHT_BORDER_SIZE;
				cv::Mat back(video_frame.rows, video_frame.cols, CV_8UC3, cv::Scalar(255, 191, 122));
				back(cv::Rect(bz-1, bz-1, video_frame.cols - (2*bz) + 2, video_frame.rows - (2*bz) + 2)) = cv::Scalar(0, 0, 0);
				cv::Rect mask(bz, bz, video_frame.cols - (2*bz), video_frame.rows - (2*bz));
				video_frame(mask).copyTo(back(mask));
				video_frame = back;
			}

			// Paint video frame
			cvui::beginRow();
				cvui::space(5);
				cvui::image(video_frame);
				cvui::space(5);
			cvui::endRow();
			cvui::space(2);

			// Timeline of video
			cvui::beginRow();
				bool trackbar_used = cvui::trackbar(video_frame.cols, &video_time, (double)0.0, (double)sp_video_frames->back().time);
				if(trackbar_used) { simple_log.push_event("trackbar_use"); }
			cvui::endRow();
			cvui::space(5);

			// Visualization of labeling
			std::vector<double> video_frame_labels_doubles;
			for(const auto& r_label : *sp_video_frame_labels) { video_frame_labels_doubles.push_back(r_label ? 1.0 : 0.0);}
			cvui::beginRow();
				int sparkline_padding = 14;
				cvui::space(sparkline_padding);
				cvui::sparkline(video_frame_labels_doubles, video_frame.cols - (2*sparkline_padding), 16, 0xCECECE);
			cvui::endRow();

			// Controls
			cvui::beginRow();
				cvui::space(10);
				if(cvui::button("<-"))
				{
					int local_prev_frame_idx = core::math::clamp(frame_idx - 2, 0, (int)sp_video_frames->size()-1);
					video_time = sp_video_frames->at(local_prev_frame_idx).time;
					playback = Playback::Pause; // stop playing
					simple_log.push_event("frame_change_prev_button");
				}
				cvui::space(5);
				if(cvui::button("->"))
				{
					int local_next_frame_idx = core::math::clamp(frame_idx, 0, (int)sp_video_frames->size()-1);
					video_time = sp_video_frames->at(local_next_frame_idx).time;
					playback = Playback::Pause; // stop playing
					simple_log.push_event("frame_change_next_button");
				}
				cvui::space(10);
				if(cvui::button("<<"))
				{
					playback = Playback::FastBackward;
					simple_log.push_event("fast_backward");
				}
				if(cvui::button("Play"))
				{
					playback = Playback::Play;
					simple_log.push_event("play_button");
				}
				if(cvui::button(">>"))
				{
					playback = Playback::FastForward;
					simple_log.push_event("fast_forward");
				}
				cvui::space(5);
				if(cvui::button("Pause"))
				{
					playback = Playback::Pause;
					simple_log.push_event("pause_button");
				}
				cvui::space(32);
				if(cvui::button("Clear All Labels"))
				{
					for(int i = 0; i < (int)sp_video_frame_labels->size(); ++i) { sp_video_frame_labels->at(i) = false; }
					simple_log.push_event("clear_labels");
				}
				cvui::space(256);
				bool label = sp_video_frame_labels->at(frame_idx);
				cvui::checkbox("Contains Element", &label);
				if(label != sp_video_frame_labels->at(frame_idx)) { simple_log.push_event("label_button", label ? "true" : "false"); } // log about label change
				sp_video_frame_labels->at(frame_idx) = label;
			cvui::endRow();

			cvui::endColumn();

			// #####################

			cvui::endRow();

			// Handle keyboard
			switch(key)
			{
			case 65361: // left (does not work under Windows)
				video_time = sp_video_frames->at(core::math::clamp(frame_idx - 2, 0, (int)sp_video_frames->size()-1)).time;
				playback = Playback::Pause; // stop playing
				simple_log.push_event("frame_change_prev_key");
				break;
			case 65363: // right (does not work under Windows)
				video_time = sp_video_frames->at(core::math::clamp(frame_idx, 0, (int)sp_video_frames->size()-1)).time;
				playback = Playback::Pause; // stop playing
				simple_log.push_event("frame_change_next_key");
				break;
			case 13: // return
				sp_video_frame_labels->at(frame_idx) = !sp_video_frame_labels->at(frame_idx);
				simple_log.push_event("label_key", sp_video_frame_labels->at(frame_idx) ? "true" : "false");
				break;
			case 32: // space
				auto_labeling = !auto_labeling;
				simple_log.push_event("auto_labeling", auto_labeling ? "on" : "off"); // automatically turned of at change of session
				break;
			}

			// Log if frame idx is different
			if(frame_idx != prev_frame_idx)
			{
				simple_log.push_event("frame_change", std::to_string(frame_idx));
			}

			// Update prev frame
			prev_frame_idx = frame_idx;
		}

		// ###############
		// ### Stimuli ###
		// ###############

		else // Mode == Stimuli
		{
			// Get stimulus to display
			const auto& r_stimulus = stimuli.at(stimuli_idx);

			// Begin drawing onto frame
			int h_padding = (WINDOW_WIDTH - (TASK_COLUMN_WIDTH + LABEL_COLUMN_WIDTH)) / 3;
			cvui::beginRow(frame, 0, 0, WINDOW_WIDTH, WINDOW_HEIGHT, 0);

			// ###################
			// ### TASK COLUMN ###
			// ###################
			cvui::beginColumn(frame, h_padding, 0, TASK_COLUMN_WIDTH, WINDOW_HEIGHT);

			cvui::space(200);
			auto scaled_task_image = core::opencv::scale_to_fit(task_image, TASK_COLUMN_WIDTH, WINDOW_HEIGHT - 256);
			int h_task_padding = (TASK_COLUMN_WIDTH - scaled_task_image.cols) / 2;
			cvui::beginRow();
				cvui::space(h_task_padding);
				cvui::image(scaled_task_image);
			cvui::endRow();
			cvui::space(5);
			cvui::beginRow();
				cvui::space(h_task_padding);
				cvui::text("Task: Look for this element", 0.7);
			cvui::endRow();
			
			cvui::endColumn();

			// ####################
			// ### LABEL COLUMN ###
			// ####################
			cvui::beginColumn(frame, 2 * h_padding + TASK_COLUMN_WIDTH, 0, LABEL_COLUMN_WIDTH);

			// General controls
			cvui::space(20);
			cvui::beginRow();
			
			if(cvui::button("Done with all images!"))
			{
				// Go to next mode
				++mode_idx;
				simple_log.push_event("mode_change", mode_to_string(modes.at(mode_idx)));
			}
			cvui::endRow();
			cvui::space(5);

			// Stimulus
			cvui::beginRow();

				// Viewport
				cv::Mat viewport = cv::Mat(768, 1024, CV_8UC3, cv::Scalar(0,0,0));
				cv::Rect content_rect(0, 0, r_stimulus.pixels.cols, r_stimulus.pixels.rows);
				cv::Rect viewport_in_content_rect(0, viewport_scrolling_y, viewport.cols, viewport.rows);
				cv::Rect intersection = content_rect & viewport_in_content_rect;
				if(!intersection.empty())
				{
					r_stimulus.pixels(intersection).copyTo(viewport(cv::Rect(0, 0, intersection.width, intersection.height)));
				}

				// Highlight border in viewport if labeled with true
				if(sp_stimuli_labels->at(stimuli_idx))
				{
					const int bz = HIGHLIGHT_BORDER_SIZE;
					cv::Mat back(viewport.rows, viewport.cols, CV_8UC3, cv::Scalar(255, 191, 122));
					back(cv::Rect(bz-1, bz-1, viewport.cols - (2*bz) + 2, viewport.rows - (2*bz) + 2)) = cv::Scalar(0, 0, 0);
					cv::Rect mask(bz, bz, viewport.cols - (2*bz), viewport.rows - (2*bz));
					viewport(mask).copyTo(back(mask));
					viewport = back;
				}

				// Render viewport
				cvui::image(viewport);

				// Scrollbar
				cvui::space(2);
				cv::Mat scrollbar = cv::Mat(768, 12, CV_8UC3, cv::Scalar(32,32,32));
				if(r_stimulus.pixels.rows > 0)
				{
					float scrollbox_rel_y = core::math::clamp(((float)viewport_scrolling_y / (float)r_stimulus.pixels.rows), 0.f, 1.f);
					float scrollbox_rel_height = core::math::clamp(((float)viewport.rows / (float)r_stimulus.pixels.rows), 0.f, 1.f);
					cv::Rect scrollbox_rect(0, (int)(768.f * scrollbox_rel_y), 12, (int)(768.f * scrollbox_rel_height));
					scrollbar(scrollbox_rect) = cv::Scalar(196,196,196);
				}
				cvui::image(scrollbar);

			cvui::endRow();
			cvui::space(5);
			
			// Controls
			cvui::beginRow();
				if(cvui::button("Previous"))
				{
					stimuli_idx = core::math::clamp(stimuli_idx - 1, 0, (int)stimuli.size() - 1);
					viewport_scrolling_y = 0;
				}
				if(cvui::button("Next"))
				{
					stimuli_idx = core::math::clamp(stimuli_idx + 1, 0, (int)stimuli.size() - 1);
					viewport_scrolling_y = 0;
				}
				if(cvui::button("Scroll Up"))
				{
					viewport_scrolling_y = core::math::clamp(viewport_scrolling_y - 64, 0, std::max(0, r_stimulus.pixels.rows - viewport.rows));
					simple_log.push_event("stimulus_scroll_up_button", std::to_string(viewport_scrolling_y));
				}
				if(cvui::button("Scroll Down"))
				{
					viewport_scrolling_y = core::math::clamp(viewport_scrolling_y + 64, 0, std::max(0, r_stimulus.pixels.rows - viewport.rows));
					simple_log.push_event("stimulus_scroll_down_button", std::to_string(viewport_scrolling_y));
				}
				cvui::space(256);
				bool label = sp_stimuli_labels->at(stimuli_idx);
				cvui::checkbox("Contains Element", &label);
				if(label != sp_stimuli_labels->at(stimuli_idx)) { simple_log.push_event("label_button", label ? "true" : "false"); } // log about label change
				sp_stimuli_labels->at(stimuli_idx) = label;
				cvui::space(32);
				cvui::text("Stimulus: " + std::to_string(stimuli_idx + 1) + "/" + std::to_string((int)stimuli.size()));
				if(r_stimulus.pixels.rows > viewport.rows)
				{
					cvui::space(32);
					cvui::text("Scrollable!", 0.4, 0xFF0000);
				}
			cvui::endRow();

			cvui::endColumn();

			// #####################

			cvui::endRow();

			// Handle keyboard
			switch(key)
			{
			case 65361: // left (does not work under Windows)
				stimuli_idx = core::math::clamp(stimuli_idx - 1, 0, (int)stimuli.size() - 1);
				viewport_scrolling_y = 0;
				simple_log.push_event("stimulus_change_prev_key");
				break;
			case 65363: // right (does not work under Windows)
				stimuli_idx = core::math::clamp(stimuli_idx + 1, 0, (int)stimuli.size() - 1);
				viewport_scrolling_y = 0;
				simple_log.push_event("stimulus_change_next_key");
				break;
			case 65362: // up (does not work under Windows)
				viewport_scrolling_y = core::math::clamp(viewport_scrolling_y - 64, 0, std::max(0, r_stimulus.pixels.rows - viewport.rows));
				simple_log.push_event("stimulus_scroll_up_key", std::to_string(viewport_scrolling_y));
				break;
			case 65364: // down (does not work under Windows)
				viewport_scrolling_y = core::math::clamp(viewport_scrolling_y + 64, 0, std::max(0, r_stimulus.pixels.rows - viewport.rows));
				simple_log.push_event("stimulus_scroll_down_key", std::to_string(viewport_scrolling_y));
				break;
			case 13: // return
				sp_stimuli_labels->at(stimuli_idx) = !sp_stimuli_labels->at(stimuli_idx);
				simple_log.push_event("label_key", sp_stimuli_labels->at(stimuli_idx) ? "true" : "false");
				break;
			}
			
			// Log if stimuli idx is different
			if(stimuli_idx != prev_stimuli_idx)
			{
				simple_log.push_event("stimulus_change", std::to_string(stimuli_idx));
			}
			
			// Update prev stimuli idx
			prev_stimuli_idx = stimuli_idx;
		}

		// Update cvui stuff and show everything on the screen
		cvui::update(WINDOW_NAME);
		cv::imshow(WINDOW_NAME, frame);

		// Handle keyboard
		switch(key)
		{
		case 27: // ESC
			cv::destroyWindow(WINDOW_NAME);
			break;
		}

		// Exit if no window is open at this point
		if (!core::opencv::is_window_open(WINDOW_NAME))
		{
			exit = true;
		}
		
		// Update last time
		last_time = curr_time;
	}
#else
		core::mt::log_info("Cannot show user interface as compiled without support for visual debug!");
#endif
	core::mt::log_info("Exit application!");
	return 0;
}
