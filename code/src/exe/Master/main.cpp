//! Main function of master.
/*!
Entry point of VisualStimuliDiscovery application.
*/

#include <Core/Core.hpp>
#include <Core/VisualChangeClassifier.hpp>
#include <Core/VisualDebug.hpp>
#include <Data/Session.hpp>
#include <Stage/Processing.hpp>
#include <Stage/Splitting.hpp>
#include <Stage/Merging.hpp>
#include <cxxopts.hpp>
#include <chrono>
#include <fstream>

// Declaration of functions
void work(std::string dataset_dir, std::string session, std::string training_participant);

//! Main function of master.
/*!
\return Indicator about success.
*/
int main(int argc, const char** argv)
{
	// Very first thing to do: remember this as main thread
	core::main_thread_id = std::this_thread::get_id();

	// Welcome
	core::mt::log_info("Welcome to the Master of VisualStimuliDiscovery!");

	// Mode of master
	enum class Mode { work, version_print };

	// Struct with startup parameters. Set values later collected from command line
	struct Startup
	{
		Mode mode = Mode::work;
	};
	Startup startup;

	/////////////////////////////////////////////////
	/// Command line arguments
	/////////////////////////////////////////////////

	// Create options object
	cxxopts::Options options("VisualStimuliDiscovery Master", "Server software of the GazeMining project.");

	// Add options
	try
	{
		options.add_options()
			("v,version", "Prints version of software", cxxopts::value<bool>())
			("d,dataset", "Directory with log records", cxxopts::value<std::string>()) // e.g., '/home/raphael/Dataset'
			("s,site", "Site to work on across participants (e.g., 'nih')", cxxopts::value<std::string>()) // e.g., 'nih'
			("t,training", "Participant data to train classifier with", cxxopts::value<std::string>()) // e.g., 'p4'
			;
	}
	catch (cxxopts::OptionSpecException e)
	{
		std::cerr << e.what() << std::endl;
		return -1;
	}

	// Retrieve options
	std::string dataset_dir = "";
	std::string site = "";
	std::string training_participant = "";
	try
	{
		// Parse arguments to options
		auto result = options.parse(argc, argv);

		// Version
		if (result.count("version"))
		{
			if (result["version"].as<bool>()) // check for true
			{
				startup.mode = Mode::version_print;
			}
		}
		
		// TODO: Allow version query without other parameters
		
		// Dataset
		if (result.count("dataset") == 0) { core::mt::log_info("You must provide the directory of the dataset!"); return -1; }
		dataset_dir = result["dataset"].as<std::string>();

		// Site
		if (result.count("site") == 0) { core::mt::log_info("You must specify the site to work on!"); return -1; }
		site = result["site"].as<std::string>();

		// Train
		if (result.count("training") == 0) { core::mt::log_info("You must provide a participant to use as training for the visual change classifier!"); return -1; }
		training_participant = result["training"].as<std::string>();
	}
	catch (cxxopts::OptionParseException e)
	{
		std::cerr << e.what() << std::endl;
		return -1;
	}

	/////////////////////////////////////////////////
	/// Execution of mode chosen at startup
	/////////////////////////////////////////////////

	core::mt::log_info("Dataset directory: ", dataset_dir);
	core::mt::log_info("Site: ", site);
	core::mt::log_info("Participant used for training: ", training_participant);

	switch (startup.mode)
	{
	case Mode::work:
		work(dataset_dir, site, training_participant);
		break;
	case Mode::version_print:
		std::cout << "Version " << std::string(GM_VERSION) << std::endl;
		break;
	}

	return 0;
}

// Function that does the work. Output is vector of session and vector of inter-user states
void work(std::string dataset_dir, std::string site, std::string training_participant)
{
	// Prepare visual debugging
	VD(core::visual_debug::Explorer visual_explorer;)

	// Prepare folder name for serializing
	auto t = std::time(nullptr);
	auto tm = *std::localtime(&t);
	std::ostringstream oss;
	oss << std::put_time(&tm, "%Y-%m-%d_%H-%M-%S") << "_" << site;
	auto serializing_folder = oss.str();
	
	/////////////////////////////////////////////////
	/// Sessions
	/////////////////////////////////////////////////

	// List of participants
	std::vector<std::string> participants =
		{
		  "p1",
		  "p2",
		  "p3",
		  "p4"
		};

	// {"p1/nih", 218},
	// {"p2/nih", 373},
	// {"p3/nih", 325},
	// {"p4/nih", 331}

	// Create list of log records to be parsed, including frame limits to consider initial Web page, only
	std::vector<std::string> log_records;
	for(const auto& p : participants)
	{
		log_records.push_back(p + "/" + site); // e.g., 'p1/nih'
	}

	// Create classifier of visual change
	auto sp_classifier = std::make_shared<const core::VisualChangeClassifier>(
		dataset_dir + "/" + training_participant + "/" + site + "_features.csv",
		dataset_dir + "/" + training_participant + "/" + site + "_labels-l1.csv");
	
	// Create sessions
	auto sp_sessions_tmp = std::make_shared<data::Sessions>();
	for(const auto& r_log_record : log_records)
	{
		// Prepare id without slash (otherwise, serialization screws up)
		std::string id = r_log_record;
		std::replace(id.begin(), id.end(), '/', '_');
		sp_sessions_tmp->push_back(std::make_shared<data::Session>(
			id,
			dataset_dir + "/" + r_log_record + ".webm",
			dataset_dir + "/" + r_log_record + ".json",
			-1)); // frames to process (-1 means all that are available)
	}

	// Make shared vector with shared sessions const
	auto sp_sessions = core::misc::make_const(sp_sessions_tmp);
	
	/////////////////////////////////////////////////
	/// Stages
	/////////////////////////////////////////////////

	// Processing stage. One container per session.
	std::chrono::steady_clock::time_point start_time = std::chrono::steady_clock::now();
	auto sp_log_datum_containers = stage::processing::run(VD(visual_explorer,) sp_sessions);
	auto processingMS = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start_time);

	// Splitting stage. One container per session.
	start_time = std::chrono::steady_clock::now();
	auto sp_intra_user_state_containers = stage::splitting::run(VD(visual_explorer,) sp_classifier, sp_log_datum_containers);
	auto splittingMS = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start_time);

	// Serialize intra-user states
	if (core::mt::get_config_value(true, { "serializing", "intra_user_states" }))
	{
		std::string intras_dir = std::string(GM_OUT_PATH) + serializing_folder + "/shots";

		// Go over containers (each session has one)
		for (const auto& r_container : *sp_intra_user_state_containers)
		{
			r_container->serialize(intras_dir);
		}
	}

	// Merging stage. One container per layer cluster.
	start_time = std::chrono::steady_clock::now();
	auto sp_inter_user_state_containers = stage::merging::run(VD(visual_explorer,) sp_classifier, sp_intra_user_state_containers);
	auto mergingMS = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start_time);

	// Serialize inter-user states
	if (core::mt::get_config_value(true, { "serializing", "inter_user_states" }))
	{
		std::string inters_dir = std::string(GM_OUT_PATH) + serializing_folder + "/stimuli";

		// Go over containers (each layer cluster has one)
		int i = 0;
		for (const auto& r_container : *sp_inter_user_state_containers)
		{
			r_container->serialize(inters_dir, std::to_string(i)); // one could also use the most common xpath of the layer cluster as id
			++i;
		}
	}

	// Serialize stage times
	{
		// Header
		std::ofstream out(std::string(GM_OUT_PATH) + serializing_folder + "/times.csv");
		out << "stage,";
		out << "time [ms]\n";

		// Times
		out << "processing,";
		out << std::to_string(processingMS.count()) << "\n";
		out << "splitting,";
		out << std::to_string(splittingMS.count()) << "\n";
		out << "merging,";
		out << std::to_string(mergingMS.count()) << "\n";
	}

	/////////////////////////////////////////////////
	/// Visual Debugging
	/////////////////////////////////////////////////

	// Display visual debugging
	VD(visual_explorer.display();)
}
