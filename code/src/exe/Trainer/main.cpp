#include <Core/Core.hpp>
#include <Core/VisualDebug.hpp>
#include <Core/Task.hpp>
#include <Core/Keyboard.hpp>
#include <Data/Session.hpp>
#include <Util/LogDatesWalker.hpp>
#include <Util/LayerComparator.hpp>
#include <Stage/Processing/Parser.hpp>
#include <Stage/Processing/Tuning.hpp>
#include <Descriptor/Histogram.hpp>
#include <Descriptor/OCR.hpp>
#include <Data/Dataset.hpp>
#include <Learn/DecisionTree.hpp>
#include <Learn/RandomForest.hpp>
#include <Learn/SVM.hpp>
#include <Feature/FeatureVector.hpp>
#include <Feature/PixelDiff.hpp>
#include <opencv2/opencv.hpp>
#include <cxxopts.hpp>

#ifdef GM_VISUAL_DEBUG
#include <clip.h>
#endif

#include <fstream>

// NOTE: Shogun does not yet support serialization of decision trees
// https://github.com/shogun-toolbox/shogun/issues/4242

// Implementation provided by visual debug header (yet, visual debug is not used)
#include <cvui.h>

// TODO:
// - show normalized features in GUI, too?

/////////////////////////////////////////////////
/// Defines
/////////////////////////////////////////////////
const int WINDOW_WIDTH = 2000;
const int WINDOW_HEIGHT = 1100;
const int WINDOW_PADDING = 5;
const int ROW_HEIGHT = 20;
const cv::Scalar BACKGROUND_COLOR = cv::Scalar(49, 52, 49);
const cv::Scalar BACKGROUND_LABELED_COLOR = cv::Scalar(128, 52, 49);
const std::string WINDOW_NAME = "Trainer";
const int OBSERVATION_DISPLAY_HEIGHT = 700;
const int FEATURE_EXTRACTION_THREAD_COUNT = 32;
const int OBSERVATION_MIN_OVERLAP_EXTENT = 5;

/////////////////////////////////////////////////
/// Structures
/////////////////////////////////////////////////

enum class Mode { Standard, Label, Feature_Computation, Store_View_Masks, Store_Scroll_Cache_Map, Store_Times };
enum class Prediction { Unlabled, Not_Different, Different };

// Single observation
struct Observation
{
	// Compressed pixels which are compared for the observation of "visual difference"
	std::shared_ptr<std::vector<uchar> > sp_prev_buff = std::make_shared<std::vector<uchar> >();
	std::shared_ptr<std::vector<uchar> > sp_cur_buff = std::make_shared<std::vector<uchar> >();

	// Information required for comparison, e.g., scrolling to align both pixel information
	std::shared_ptr<const data::Layer> sp_layer = nullptr;
	std::shared_ptr<const data::Layer> sp_prev_layer = nullptr; // only stored for outputting view masks
	int scroll_offset_x = 0;
	int scroll_offset_y = 0;
	int scroll_cache_idx = -1; // mapping from scroll cache idx to observation

	// Classification
	bool label = false;
	std::map<std::string, double> features;
	Prediction predict_label = Prediction::Unlabled;

	// Meta information
	int prev_video_frame_idx = -1;
	int cur_video_frame_idx = -1;
	int overlap_width = -1;
	int overlap_height = -1;
	std::map<std::string, int> feature_times; // times to compute features
};

/////////////////////////////////////////////////
/// Function declarations and definitions
/////////////////////////////////////////////////

// Create dataset from list of observations
std::shared_ptr<data::Dataset> create_dataset(const std::vector<Observation>& r_observations);

// Train classifier. Returns accuracy and stores in provided observations their predicted label
template<typename T>
float train_classifier(std::vector<Observation>& r_observations)
{
	// Create normalized dataset
	auto sp_dataset = create_dataset(r_observations);
	sp_dataset->normalize();

	// Create classifier
	T classifier(sp_dataset);

	// Classify the features used for training and store result in observations
	auto sp_labels = classifier.classify(sp_dataset);
	for (int i = 0; i < sp_labels->size(); ++i)
	{
		if ((*sp_labels)(i) < 1.0)
		{
			r_observations.at(i).predict_label = Prediction::Not_Different;
		}
		else
		{
			r_observations.at(i).predict_label = Prediction::Different;
		}
	}

	// Print the classifier
	classifier.print();

	// Return accuracy
	return (float) classifier.training_accuracy();
}

/////////////////////////////////////////////////
/// Main
/////////////////////////////////////////////////

// Main function
int main(int argc, const char** argv)
{
	core::mt::log_info("Welcome to the Trainer of VisualStimuliDiscovery!");

	/////////////////////////////////////////////////
	/// Command line arguments
	/////////////////////////////////////////////////

	// Variables to fill
	std::string log_record_dir = "", log_record_id = "", person = "";
	Mode mode = Mode::Standard;
	std::string window_name = WINDOW_NAME;
	bool skip_perfect = false;

	// Create options object
	cxxopts::Options options("VisualStimuliDiscovery Trainer", "Training software of the GazeMining project.");

	// Add options
	try
	{
		options.add_options()
			("d,directory", "Directory where session is stored, without slash at the end of path.", cxxopts::value<std::string>())
			("s,session", "Session to be loaded, consisting of a .webm and a .json file", cxxopts::value<std::string>())
			("m,mode", "Mode of trainer. 'standard', 'label', 'feature_computation', 'store_view_masks', 'store_scroll_cache_map', and 'store_times' are available.", cxxopts::value<std::string>())
			("p,person", "Name of the person who labels. Reflected in file name.", cxxopts::value<std::string>())
			("skip-perfect", "Skips observations without any BGR pixel value difference.", cxxopts::value<bool>(skip_perfect));
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

		// Directory
		if (result.count("directory"))
		{
			log_record_dir = result["directory"].as<std::string>();
		}
		else
		{
			throw cxxopts::OptionParseException("Directory option is missing!");
		}

		// Session
		if (result.count("session"))
		{
			log_record_id = result["session"].as<std::string>();
		}
		else
		{
			throw cxxopts::OptionParseException("Session option is missing!");
		}

		// Mode
		if (result.count("mode"))
		{
			std::string mode_string = result["mode"].as<std::string>();
			if (mode_string == "label")
			{
				mode = Mode::Label;
			}
			else if (mode_string == "feature_computation")
			{
				mode = Mode::Feature_Computation;
			}
			else if (mode_string == "store_view_masks")
			{
				mode = Mode::Store_View_Masks;
			}
			else if (mode_string == "store_scroll_cache_map")
			{
				mode = Mode::Store_Scroll_Cache_Map;
			}
			else if (mode_string == "store_times")
			{
				mode = Mode::Store_Times;
			}
		}

		// Person
		if (result.count("person"))
		{
			person = result["person"].as<std::string>();
		}
	}
	catch (cxxopts::OptionParseException e)
	{
		std::cerr << e.what() << std::endl;
		return -1;
	}

	// Print log record information
	core::mt::log_info("Directory: " + log_record_dir);
	core::mt::log_info("Session: " + log_record_id);
	if (!person.empty()) { core::mt::log_info("Person: " + person); }

	// Inform about mode
	switch (mode)
	{
	case Mode::Label:
		core::mt::log_info("Trainer instantiated in label mode.");
		window_name += " [LABEL MODE]";
		break;
	case Mode::Feature_Computation:
		core::mt::log_info("Trainer instantiated in feature computation mode.");
		break;
	case Mode::Store_View_Masks:
		core::mt::log_info("Trainer instantiated to store view masks.");
		break;
	case Mode::Store_Scroll_Cache_Map:
		core::mt::log_info("Trainer instantiated to store scroll map cache.");
		break;
	case Mode::Store_Times:
		core::mt::log_info("Trainer instantiated to store times.");
		break;
	default:
		core::mt::log_info("Trainer instantiated in standard mode.");
	}

	// Labels file name
	std::string label_file_name = "_labels";
	if (!person.empty())
	{
		label_file_name += "-" + person;
	}

	/////////////////////////////////////////////////
	/// Setup
	/////////////////////////////////////////////////

	// Files to load observation classification details from (saves one from computation of features)
	const bool load_dataset = false; // if false, features will be computed and lables remain with initial value
	const std::string input_features_file_path(log_record_dir + "/" + log_record_id + "_features.csv");
	const std::string input_labels_file_path(log_record_dir + "/" + log_record_id + label_file_name + ".csv");
	// Remark: THIS MUST MATCH THE LOG RECORD

	// Files to store observation classification details into
	const std::string output_features_file_path(log_record_dir + "/" + log_record_id + "_features.csv");
	const std::string output_labels_file_path(log_record_dir + "/" + log_record_id + label_file_name + ".csv");
	const std::string output_meta_file_path(log_record_dir + "/" + log_record_id + "_meta.csv");
	const std::string output_features_meta_file_path(log_record_dir + "/" + log_record_id + "_features_meta.csv");

	// Use pretrained decision tree classifier for initial labeling (TODO: make part of the command line)
	const bool pretrain_classifier = false;
	const std::string pretrain_features_file_path(std::string(GM_OUT_PATH) + "features.csv");
	const std::string pretrain_labels_file_path(std::string(GM_OUT_PATH) + "labels.csv");
	// Remark: Would be better to store decision tree itself. Yet, this is not possible with Shogun :(

	// Cached estimated scrolling in observations (aka tuning, so it has not to be done everytime)
	const std::string scroll_cache_file_path(log_record_dir + "/" + log_record_id + "_scroll_cache.csv");

	/////////////////////////////////////////////////
	/// Parse log record
	/////////////////////////////////////////////////

	core::mt::log_info("Parse log record...");

	// Load log record into a session
	auto sp_session = std::make_shared<data::Session>(
		log_record_id,
		log_record_dir + "/" + log_record_id + ".webm",
		log_record_dir + "/" + log_record_id + ".json");

	// Parse the session into log dates
	typedef core::Task<stage::processing::parser::LogRecord, 1> ParserTask;
	auto sp_parsing_task = std::make_shared<ParserTask>(
		VD(nullptr, ) // no visual debug is used
		sp_session); // session
	auto sp_log_datum_container = sp_parsing_task->get_product(); // parsing is done in thread, "get_product" will lock until thread is finished

	// Improve scrolling, throw away scrolling from json
	std::ifstream scroll_cache_file(scroll_cache_file_path);
	if (!scroll_cache_file.is_open()) // no cache available, thus, compute new scrolling
	{
		core::mt::log_info("Tune log record (may take a while)...");

		// Tune the scrolling of the log dates
		typedef core::Task<stage::processing::tuning::ORBscroll, 1> ORBscrollTask;
		auto sp_tuning_task = std::make_shared<ORBscrollTask>(
			VD(nullptr, ) // no visual debug is used
			sp_log_datum_container);
		sp_log_datum_container = sp_tuning_task->get_product();
	}
	
	////////////////////////////////////////////////
	/// Store times
	/////////////////////////////////////////////////

	if(mode == Mode::Store_Times)
	{
		std::ofstream times_out(log_record_dir + "/" + log_record_id + "_times.csv");
		
		util::LogDatesWalker log_dates_walker(
			core::misc::make_const(sp_log_datum_container->get()), // log dates from container
			log_record_dir + "/" + log_record_id + ".webm" // path to video of log record
		);
		while (log_dates_walker.step()) // another frame is available
		{
			auto time = log_dates_walker.get_log_datum()->get_frame_time();
			times_out << std::to_string(time) << "\n";
		}
		return 0; // mode is over
	}

	/////////////////////////////////////////////////
	/// Collect observations
	/////////////////////////////////////////////////

	core::mt::log_info("Collect observations...");

	std::vector<Observation> observations;
	int frame_total_count = 0;
	double screencast_duration = 0.0;

	// Own scope to cleanup stack
	{
		// Create log dates walker
		util::LogDatesWalker log_dates_walker(
			core::misc::make_const(sp_log_datum_container->get()), // log dates from container
			log_record_dir + "/" + log_record_id + ".webm" // path to video of log record
		);

		// Go over log dates
		std::vector<std::vector<Observation> > layer_obs; // observations separated per layer
		std::vector<std::pair<int, int> > obs_to_delete; // indices of incomplete observations, must be deleted later
		while (log_dates_walker.step()) // another frame is available
		{
			// Retrieve values for that frame
			auto sp_log_image = log_dates_walker.get_log_image();
			int frame_idx = log_dates_walker.get_frame_idx();
			auto layers_to_process = log_dates_walker.get_layer_packs();
			
			// Remember count of frames
			frame_total_count = std::max(frame_total_count, frame_idx+1); // max actually not required as frame_idx should grow incrementally
			
			// Remember duration of screencast
			screencast_duration = std::max(screencast_duration, log_dates_walker.get_log_datum()->get_frame_time());

			// Go over layers in that frame
			for (const auto& r_layer : layers_to_process)
			{
				bool processed = false; // whether layer has been processed

				// Find best fitting layer among existing observations
				for (auto& r_obs : layer_obs)
				{
					// TODO: here, be greedy and just take first good-enough layer
					if (util::layer_comparator::compare(r_layer.sptr, r_obs.back().sp_layer).value() >= 0.75f)
					{					
						// Store observation
						Observation obs;
						cv::imencode(".png", sp_log_image->get_layer_pixels(r_layer.sptr->get_view_mask()), *(obs.sp_cur_buff.get()));
						*obs.sp_prev_buff = *(r_obs.back().sp_cur_buff); // vector copied, not referenced
						obs.sp_layer = r_layer.sptr;
						obs.sp_prev_layer = r_obs.back().sp_layer;
						obs.scroll_offset_x = r_layer.sptr->get_scroll_x() - r_obs.back().sp_layer->get_scroll_x();
						obs.scroll_offset_y = r_layer.sptr->get_scroll_y() - r_obs.back().sp_layer->get_scroll_y();
						obs.prev_video_frame_idx = r_obs.back().cur_video_frame_idx;
						obs.cur_video_frame_idx = frame_idx;
						r_obs.push_back(obs);
						processed = true;
						break; // break for loop
					}
				}

				// For non-processed layers, add incomplete observation (required to be taken for r_obs.back() access)
				if (!processed)
				{
					// Store incomplete observation
					Observation obs;
					cv::imencode(".png", sp_log_image->get_layer_pixels(r_layer.sptr->get_view_mask()), *(obs.sp_cur_buff.get()));
					obs.sp_layer = r_layer.sptr;
					obs.cur_video_frame_idx = frame_idx;
					layer_obs.push_back({ obs });
					obs_to_delete.push_back(std::make_pair((int)(layer_obs.size()) - 1, 0)); // remember index to delete later
				}
			}
		}

		// Remove incomplete observations
		for (int i = (int)obs_to_delete.size() - 1; i >= 0; --i)
		{
			auto to_delete = obs_to_delete.at(i);
			auto& ref = layer_obs.at(to_delete.first);
			ref.erase(ref.begin() + to_delete.second);
		}

		// Collect observations into one vector (poor man's flatten)
		for (auto& r_layer : layer_obs)
		{
			for (auto& r_obs : r_layer)
			{
				observations.push_back(r_obs);
			}
		}
	}

	/////////////////////////////////////////////////
	/// Store or load scroll estimation
	/////////////////////////////////////////////////

	// Remark: further observations might be deleted after this step when there is no overlapping pixel area
	if (scroll_cache_file.is_open())
	{
		std::string line;
		int i = 0;
		while (std::getline(scroll_cache_file, line, '\n'))
		{
			if (!line.empty())
			{
				// Check whether there is still an observation
				if (i >= (int)observations.size())
				{
					core::mt::log_error("Scroll cache does not seem to fit the observations.");
					return -1;
				}

				// Load scroll offset for the observation
				auto tokens = core::misc::tokenize(line);
				int scroll_offset_x = std::stoi(tokens.at(0));
				int scroll_offset_y = std::stoi(tokens.at(1));
				observations.at(i).scroll_offset_x = scroll_offset_x;
				observations.at(i).scroll_offset_y = scroll_offset_y;
				observations.at(i).scroll_cache_idx = i;
				++i;
			}
		}

		// Check whether all observations were covered
		if (i == (int)observations.size())
		{
			core::mt::log_info("Scroll cache has been loaded: " + scroll_cache_file_path);
		}
		else
		{
			core::mt::log_warn("Not all observations could be updated from scroll cache.");
		}
	}
	else
	{
		// Store scroll cache
		std::ofstream scroll_cache_out(scroll_cache_file_path);
		if (scroll_cache_out.is_open())
		{
			for (auto& r_obs : observations)
			{
				scroll_cache_out << std::to_string(r_obs.scroll_offset_x) << "," << std::to_string(r_obs.scroll_offset_y) << "\n";
			}
			core::mt::log_info("Scroll cache has been created: " + scroll_cache_file_path);
		}
		else
		{
			core::mt::log_info("Scroll cache could not be created: " + scroll_cache_file_path);
		}
	}
	
	// TODO: Remove, just added to make computation shorter
	// observations.resize(40);

	/////////////////////////////////////////////////
	/// Replace pixels in observations by overlap
	/////////////////////////////////////////////////

	core::mt::log_info("Overlap pixels...");

	std::vector<int> obs_to_delete;
	int i = 0;
	for (auto& r_obs : observations)
	{
		// Decode observation
		cv::Mat prev_mat, cur_mat;
		prev_mat = cv::imdecode(*(r_obs.sp_prev_buff).get(), -1); // -1 ensures decoding with alpha channel
		cur_mat = cv::imdecode(*(r_obs.sp_cur_buff).get(), -1); // -1 ensures decoding with alpha channel

		// Translate prev pixels accordingly to scrolling in current pixels
		core::opencv::translate_matrix(prev_mat, (float)-r_obs.scroll_offset_x, (float)-r_obs.scroll_offset_y);

		// Store overlapping area of both pixels and prev_pixels
		bool overlap = core::opencv::overlap_and_crop(
			prev_mat,
			cur_mat,
			prev_mat,
			cur_mat);
			
		// Store meta information
		r_obs.overlap_width = cur_mat.cols;
		r_obs.overlap_height = cur_mat.rows;
		
		// Encode observation
		r_obs.sp_prev_buff->clear();
		r_obs.sp_cur_buff->clear();
		cv::imencode(".png", prev_mat, *(r_obs.sp_prev_buff).get());
		cv::imencode(".png", cur_mat, *(r_obs.sp_cur_buff).get());
		
		// Discard observation if there has been no overlap or overlap is too small
		if (!overlap || OBSERVATION_MIN_OVERLAP_EXTENT > r_obs.overlap_width || OBSERVATION_MIN_OVERLAP_EXTENT > r_obs.overlap_height)
		{
			obs_to_delete.push_back(i);
		}
		
		// Increment index
		++i;
	}

	// Remove non-overlapping observations
	for (int i = (int) obs_to_delete.size() - 1; i >= 0; --i)
	{
		auto idx = obs_to_delete.at(i);
		observations.erase(observations.begin() + idx);
	}
	
	// Remember the count for the report
	int observation_total_count = (int) observations.size();

	/////////////////////////////////////////////////
	/// Filter perfect matching observations
	/////////////////////////////////////////////////

	int observation_skipped_count = 0;

	if (skip_perfect)
	{
		core::mt::log_info("Filter perfectly matching observations...");

		// Go over observations and collect which to delete
		std::vector<int> obs_to_delete;
		int i = 0;
		for (auto& r_obs : observations)
		{
			// Decode observation
			cv::Mat prev_mat, cur_mat;
			prev_mat = cv::imdecode(*(r_obs.sp_prev_buff).get(), -1); // -1 ensures decoding with alpha channel
			cur_mat = cv::imdecode(*(r_obs.sp_cur_buff).get(), -1); // -1 ensures decoding with alpha channel
			
			// If both images are the same, skip observation
			if (core::opencv::pixel_perfect_same(prev_mat, cur_mat))
			{
				obs_to_delete.push_back(i);
			}
			++i;
		}

		// Remove perfectly similar observations
		for (int i = (int)obs_to_delete.size() - 1; i >= 0; --i)
		{
			auto idx = obs_to_delete.at(i);
			observations.erase(observations.begin() + idx);
		}

		observation_skipped_count = (int)obs_to_delete.size();
		core::mt::log_info("Skipped ", observation_skipped_count, " perfectly matching observations!");
	}

	/////////////////////////////////////////////////
	/// Store view masks per obervation and exit
	/////////////////////////////////////////////////
	
	if(mode == Mode::Store_View_Masks)
	{
		// Store view masks
		for(int i = 0; i < (int)observations.size(); ++i)
		{
			// Reference to observation
			const Observation& r_obs = observations.at(i);
			
			// Store view masks of observation
			cv::imwrite(log_record_dir + "/" + log_record_id + "_view_mask_" + std::to_string(i) + ".png", r_obs.sp_layer->get_view_mask());
			cv::imwrite(log_record_dir + "/" + log_record_id + "_view_mask_" + std::to_string(i) + "_prev.png", r_obs.sp_prev_layer->get_view_mask());
		}
		return 0; // mode is over
	}

	/////////////////////////////////////////////////
	/// Store mapping of observations to scroll cache indices
	/////////////////////////////////////////////////

	if(mode == Mode::Store_Scroll_Cache_Map)
	{
		std::ofstream scroll_cache_map_out(log_record_dir + "/" + log_record_id + "_scroll_cache_map.csv");
	
		// Store scroll cache map
		for(int i = 0; i < (int)observations.size(); ++i)
		{
			// Reference to observation
			const Observation& r_obs = observations.at(i);
			scroll_cache_map_out << std::to_string(r_obs.scroll_cache_idx) << "\n";
			
		}
		return 0; // mode is over
	}
	
	/////////////////////////////////////////////////
	/// Retrieve features for each observation
	/////////////////////////////////////////////////

	if (mode != Mode::Label) // not required if only labels should be decided
	{
		if (load_dataset) // load features and labels from files
		{
			core::mt::log_info("Load features and labels...");
			core::mt::log_info("Features: ", input_features_file_path);
			core::mt::log_info("Labels: ", input_labels_file_path);

			// Create dataset from file
			data::Dataset dataset(input_features_file_path, input_labels_file_path);

			// Retrieve features from dataset
			auto names = dataset.get_feature_names();
			auto values = dataset.get_observations_row_wise(names);
			for (int i = 0; i < values.size(); ++i)
			{
				auto feature_idx = i % (int)names.size(); // computes column within row
				auto obs_idx = i / (int)names.size(); // row of observation
				observations.at(obs_idx).features[names.at(feature_idx)] = values(i);
			}

			// Retrieve labels from dataset
			auto labels = dataset.get_labels();
			for (int i = 0; i < labels.size(); ++i)
			{
				observations.at(i).label = labels(i) > 0.0;
			}
		}
		else // compute features and leave labels initialized
		{
			// Compute features
			core::mt::log_info("Compute features of ", observations.size(), " observations...");
			
			// Define work of thread
			auto work = [&](int id, int start_idx, int end_idx) // end_idx excluding 
			{		
				// Go over assigned observations and extract features
				for(int idx = start_idx; idx < end_idx; ++idx)
				{
					int progress = (int)(100.f * ((float)(idx - start_idx) / (float)(end_idx - start_idx)));
					core::mt::log_info(
					"...work on observation ",
					idx,
					"... (thread ",
					std::to_string(id),
					" at ",
					std::to_string(progress),
					"%)");
					auto& r_obs = observations.at(idx);
				
					// Decode observation
					auto sp_prev_mat = std::make_shared<cv::Mat>();
					auto sp_cur_mat = std::make_shared<cv::Mat>();
					*sp_prev_mat = cv::imdecode(*(r_obs.sp_prev_buff).get(), -1); // -1 ensures decoding with alpha channel
					*sp_cur_mat = cv::imdecode(*(r_obs.sp_cur_buff).get(), -1); // -1 ensures decoding with alpha channel
			
					// Compute complete vector of features for observation
					auto feature_vector = feature::FeatureVector(sp_prev_mat, sp_cur_mat);
					r_obs.features = feature_vector.get();
					r_obs.feature_times = feature_vector.get_times();
				}
			};
			
			// Create threads
			int obs_count = (int)observations.size();
			int thread_count = std::min(obs_count, FEATURE_EXTRACTION_THREAD_COUNT);
			int range = obs_count / thread_count;
			std::vector<std::thread> threads;
			for(int i = 0; i < thread_count; ++i)
			{
				int start_idx = i * range;
				int end_idx = start_idx + range;
				if(i + 1 == thread_count) { end_idx = obs_count; }
				threads.push_back(std::thread(work, i, start_idx, end_idx));
			}
			
			// Wait for threads
			for(auto& r_thread : threads)
			{
				r_thread.join();
			}
			
			// File to store meta information about features
			std::ofstream meta_out(output_features_meta_file_path);

			// Inform about meta file
			if (meta_out.is_open() && !observations.empty())
			{
				// Store header
				meta_out << "observation_id,"; // id of observation
				meta_out << "layer_type,"; // layer type
				meta_out << "xpath,"; // layer xpath
				meta_out << "prev_video_frame,"; // previous video frame
				meta_out << "cur_video_frame,"; // current video frame
				meta_out << "overlap_width,"; // width of overlap
				meta_out << "overlap_height,"; // height of overlap
				for (const auto& r_time : observations.at(0).feature_times) // assumes that order of times in map stays consistent over frames
				{
					meta_out << r_time.first << ",";
				}
				meta_out << "scroll_offset_x,"; // scroll offset x
				meta_out << "scroll_offset_y"; // scroll offset y
				meta_out << "\n";
				
				// Store datapoint of each observation
				for(int i = 0; i < (int)observations.size(); ++i)
				{
					// Reference to observation
					const Observation& r_obs = observations.at(i);
				
					// Store data
					meta_out << std::to_string(i) << ","; // id of observation
					meta_out << data::to_string(r_obs.sp_layer->get_type()) << ","; // layer type
					meta_out << r_obs.sp_layer->get_xpath() << ","; // layer xpath
					meta_out << std::to_string(r_obs.prev_video_frame_idx) << ","; // previous video frame
					meta_out << std::to_string(r_obs.cur_video_frame_idx) << ","; // current video frame
					meta_out << std::to_string(r_obs.overlap_width) << ","; // width of overlap
					meta_out << std::to_string(r_obs.overlap_height) << ","; // height of overlap
					for (const auto& r_time : observations.at(i).feature_times) // assumes that order of times in map stays consistent over frames
					{
						meta_out << r_time.second << ",";
					}
					meta_out << std::to_string(r_obs.scroll_offset_x) << ","; // scroll offset x
					meta_out << std::to_string(r_obs.scroll_offset_y); // scroll offset y
					meta_out << "\n";
				}
				
				// Inform user
				core::mt::log_info("Meta feature information has been stored: " + output_features_meta_file_path);
			}
		}
	}
	
	/////////////////////////////////////////////////
	/// Store meta information about session
	/////////////////////////////////////////////////
	
	// File to store meta information about session
	{
		std::ofstream meta_out(output_meta_file_path);
	
		// Inform about meta file
		if (meta_out.is_open() && !observations.empty())
		{
			// Header
			meta_out << "observation_total_count,"; // count of observations after overlapping
			meta_out << "observation_count_skipped,"; // count of skipped observations
			meta_out << "screencast_frame_total_count,"; // count of frames in screencast
			meta_out << "screencast_seconds,"; // screencast duration in seconds
			meta_out << "datacast_seconds"; // datacast duration in seconds
			meta_out << "\n";
			
			// Data
			meta_out << observation_total_count  << ",";
			meta_out << observation_skipped_count << ",";
			meta_out << frame_total_count << ",";
			meta_out << screencast_duration << ",";
			meta_out << sp_log_datum_container->get_datacast_duration();
			meta_out << "\n";
		}
	}

	/////////////////////////////////////////////////
	/// Pretrain classifier
	/////////////////////////////////////////////////

	if (mode == Mode::Standard) // neither label mode nor feature_computation mode require this
	{
		// Assign labels to the observations based on a decision tree trained from another dataset
		if (pretrain_classifier)
		{
			core::mt::log_info("Label observations with pretrained tree...");
			core::mt::log_info("Features: ", pretrain_features_file_path);
			core::mt::log_info("Labels: ", pretrain_labels_file_path);

			// Load normalized dataset
			auto sp_pretrain_dataset = std::make_shared<data::Dataset>(pretrain_features_file_path, pretrain_labels_file_path);
			auto pretrain_min_max = sp_pretrain_dataset->get_min_max();
			sp_pretrain_dataset->normalize();

			// Create decision tree
			learn::DecisionTree tree(sp_pretrain_dataset);

			// Collect (yet unclassified) observations into a dataset
			auto sp_dataset = create_dataset(observations); // labels are all zero / false
			sp_dataset->normalize(pretrain_min_max); // normalize with min-max values of features used for training

			// Classify the observations using the created decision tree
			auto sp_labels = tree.classify(sp_dataset);
			for (int i = 0; i < (int)sp_labels->size(); ++i)
			{
				if ((*sp_labels)(i) > 0.0)
				{
					observations.at(i).predict_label = Prediction::Different;
				}
				else
				{
					observations.at(i).predict_label = Prediction::Not_Different;
				}
			}

			// Print the tree
			tree.print();
		}
	}

	/////////////////////////////////////////////////
	/// Show GUI (or just save features)
	/////////////////////////////////////////////////

	if (mode == Mode::Feature_Computation)
	{
		auto sp_dataset = create_dataset(observations);
		sp_dataset->save_features_as_CSV(output_features_file_path);
		core::mt::log_info("Stored features: " + output_features_file_path);
	}
	else // standard or label mode require GUI
	{
#ifdef GM_VISUAL_DEBUG
		core::mt::log_info("Show user interface...");

		// Navigation through data
		int obs_idx = 0;
		const int obs_count = (int)observations.size();
		double accuracy = 0.0;

		// Create a frame where components will be rendered to
		cv::Mat frame = cv::Mat(WINDOW_HEIGHT, WINDOW_WIDTH, CV_8UC3);

		// Init cvui and tell it to create a OpenCV window, i.e., cv::namedWindow(window_name)
		cvui::init(window_name);

		// Keyboard bools
		bool left_down = false;
		bool right_down = false;
		// bool return_down = false;

		// Render loop
		bool exit = false;
		bool last_frame_obs_label = false; // used to decide on background color
		while (!exit)
		{
			// Check keyboard for changing observation index (only on Windows for now)
			if (core::keyboard::poll_key(core::keyboard::Key::Left))
			{
				if (!left_down) { obs_idx = core::math::clamp(obs_idx - 1, 0, obs_count - 1); }
				left_down = true;
			}
			else
			{
				left_down = false;
			}
			if (core::keyboard::poll_key(core::keyboard::Key::Right))
			{
				if (!right_down) { obs_idx = core::math::clamp(obs_idx + 1, 0, obs_count - 1); }
				right_down = true;
			}
			else
			{
				right_down = false;
			}
			
			// Fill the frame with a nice color
			if(last_frame_obs_label)
			{
				frame = BACKGROUND_LABELED_COLOR;
			}
			else
			{
				frame = BACKGROUND_COLOR;
			}

			// Start of overarching components
			cvui::beginRow(frame,
				0,
				0,
				WINDOW_WIDTH - (2 * WINDOW_PADDING),
				WINDOW_HEIGHT - (2 * WINDOW_PADDING),
				0);
			cvui::beginColumn(frame, WINDOW_PADDING, WINDOW_PADDING); // manual padding

			// Navigation panel
			cvui::beginRow(-1, ROW_HEIGHT);
			cvui::space(10);
			std::string obs_idx_string = std::to_string(obs_idx);
			if (cvui::button(30, 14, "-10")) { obs_idx = core::math::clamp(obs_idx - 10, 0, obs_count - 1); }
			if (cvui::button(30, 14, "-1")) { obs_idx = core::math::clamp(obs_idx - 1, 0, obs_count - 1); }
			if (cvui::button(30, 14, "+1")) { obs_idx = core::math::clamp(obs_idx + 1, 0, obs_count - 1); }
			if (cvui::button(30, 14, "+10")) { obs_idx = core::math::clamp(obs_idx + 10, 0, obs_count - 1); }

			// Get reference to current observation (as now, obs_idx is chosen)
			auto& r_obs = observations.at(obs_idx);
			
			// Decode observation
			cv::Mat prev_mat, cur_mat;
			prev_mat = cv::imdecode(*(r_obs.sp_prev_buff).get(), -1); // -1 ensures decoding with alpha channel
			cur_mat = cv::imdecode(*(r_obs.sp_cur_buff).get(), -1); // -1 ensures decoding with alpha channel

			cvui::space(10);
			cvui::text("Observation " + std::to_string(obs_idx + 1) + "/" + std::to_string(obs_count));
			cvui::space(20);
			cvui::checkbox("Visually Different! ", &(r_obs.label));
			/*
			cvui::space(5);
			if (cvui::button(128, 14, "Reset Labels"))
			{
				for (auto& r_obs : observations)
				{
					r_obs.label = false;
				}
			}
			*/
			cvui::space(5);
			if (mode == Mode::Standard)
			{
				if (cvui::button(128, 14, "Save Features"))
				{
					auto sp_dataset = create_dataset(observations);
					// sp_dataset->normalize();
					sp_dataset->save_features_as_CSV(output_features_file_path);
					core::mt::log_info("Features saved: " + output_features_file_path);
				}
				cvui::space(5);
			}
			if (cvui::button(128, 14, "Save Labels"))
			{
				auto sp_dataset = create_dataset(observations);
				// sp_dataset->normalize();
				sp_dataset->save_labels_as_CSV(output_labels_file_path);
				core::mt::log_info("Labels saved: " + output_labels_file_path);
			}
			cvui::space(5);
			if (mode == Mode::Standard)
			{
				cvui::space(5);
				std::string predict_label_info = "";
				switch (r_obs.predict_label)
				{
				case Prediction::Unlabled:
					predict_label_info = "Unknown";
					break;
				case Prediction::Not_Different:
					predict_label_info = "*NOT* different!";
					break;
				case Prediction::Different:
					predict_label_info = "Different!";
					break;
				}
				cvui::text("Classifier labels sample as: " + predict_label_info);
			}
			cvui::space(5);
			
			// Copy to clipboard
			auto copy_to_clipboard = std::function<void(const cv::Mat&)>([](const cv::Mat& r_mat)
			{
				clip::image_spec spec;
				spec.width = r_mat.cols;
				spec.height = r_mat.rows;
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
				clip::image img(r_mat.data, spec);
				clip::set_image(img);
			});
			if (cvui::button(128, 14, "Copy Left"))
			{
				copy_to_clipboard(prev_mat);
			}
			if (cvui::button(128, 14, "Copy Right"))
			{
				copy_to_clipboard(cur_mat);
			}
			cvui::endRow();
			cvui::space(5);

			/*
			// Check keyboard for changing label
			if (core::keyboard::poll_key(core::keyboard::Key::Return))
			{
				if (!return_down) { r_obs.label = !r_obs.label; }
				return_down = true;
			}
			else
			{
				return_down = false;
			}
			*/
			
			// Draw pixels of observation
			int obs_pixels_draw_width = WINDOW_WIDTH - (2 * WINDOW_PADDING);
			int single_render_width = (obs_pixels_draw_width / 2) - 5; // two images, 5 pixels space in-between
			cvui::beginRow(-1, OBSERVATION_DISPLAY_HEIGHT);
			{
				cv::Mat pixels = core::opencv::scale_to_fit(prev_mat, single_render_width, OBSERVATION_DISPLAY_HEIGHT); // clones original matrix
				cv::Mat chess = core::opencv::create_chess_board(pixels.cols, pixels.rows);
				cv::cvtColor(chess, chess, cv::COLOR_GRAY2BGRA);
				core::opencv::blend(pixels, chess, pixels);
				cv::cvtColor(pixels, pixels, cv::COLOR_BGRA2BGR);
				cvui::image(pixels);
			}
			cvui::space(5);
			{
				cv::Mat pixels = core::opencv::scale_to_fit(cur_mat, single_render_width, OBSERVATION_DISPLAY_HEIGHT); // clones original matrix
				cv::Mat chess = core::opencv::create_chess_board(pixels.cols, pixels.rows);
				cv::cvtColor(chess, chess, cv::COLOR_GRAY2BGRA);
				core::opencv::blend(pixels, chess, pixels);
				cv::cvtColor(pixels, pixels, cv::COLOR_BGRA2BGR);
				cvui::image(pixels);
			}
			cvui::endRow();
			cvui::space(5);
			
			// Feature values
			for (const auto& item : r_obs.features)
			{
				cvui::beginRow();
				cvui::text(item.first + ": " + std::to_string(item.second));
				cvui::endRow();
			}
			cvui::space(5);

			// Machine learning panel
			cvui::beginRow(-1, ROW_HEIGHT);
			cvui::space(10);
			if (mode == Mode::Standard)
			{
				// Decision tree classifier
				if (cvui::button(196, 14, "Compute Decision Tree"))
				{
					accuracy = train_classifier<learn::DecisionTree>(observations);
				}

				// Random forest classifier
				if (cvui::button(196, 14, "Compute Random Forest"))
				{
					accuracy = train_classifier<learn::RandomForest>(observations);
				}

				// Random SVM classifier
				if (cvui::button(128, 14, "Compute SVM"))
				{
					accuracy = train_classifier<learn::SVM>(observations);
				}

				cvui::space(5);
				cvui::text("Accuracy on Training Data: " + std::to_string(accuracy));
			}

			cvui::endRow();
			cvui::space(5);

			// End of overarching components
			cvui::endColumn();
			cvui::endRow();

			// Update cvui stuff and show everything on the screen
			cvui::update(window_name);
			cv::imshow(window_name, frame);

			// Check for closing the window and other keystrokes
			auto key = cv::waitKeyEx(20);
			switch(key)
			{
			case 27: // ESC
				cv::destroyWindow(window_name);
				break;
			case 65361: // left (does not work under Windows)
				obs_idx = core::math::clamp(obs_idx - 1, 0, obs_count - 1);
				break;
			case 65363: // right (does not work under Windows)
				obs_idx = core::math::clamp(obs_idx + 1, 0, obs_count - 1);
				break;
			case 13: // return
				r_obs.label = !r_obs.label;
				break;
			}

			// Exit if no window is open at this point
			if (!core::opencv::is_window_open(window_name))
			{
				exit = true;
			}
			
			// Update label of that frame's observation label
			last_frame_obs_label = r_obs.label;
		}
#else
		core::mt::log_info("Cannot show user interface as compiled without support for visual debug!");
#endif
	}

	return 0;
}

/////////////////////////////////////////////////
/// Function definitions
/////////////////////////////////////////////////

// Create dataset from list of observations
std::shared_ptr<data::Dataset> create_dataset(const std::vector<Observation>& r_observations)
{
	// Collect all available features
	std::vector<std::string> feature_names;
	for (const auto& r_observation : r_observations)
	{
		for (const auto& r_entry : r_observation.features)
		{
			feature_names.push_back(r_entry.first);
		}
	}

	// Store feature observations
	auto sp_dataset = std::make_shared<data::Dataset>(feature_names);
	for (const auto& r_observation : r_observations)
	{
		sp_dataset->append_observation(r_observation.features, r_observation.label ? 1.0 : 0.0);
	}

	return sp_dataset;
}
