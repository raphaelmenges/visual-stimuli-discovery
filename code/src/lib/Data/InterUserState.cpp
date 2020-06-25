#include "InterUserState.hpp"
#include <fstream>

void make_states_unique(std::vector<std::shared_ptr<const data::IntraUserState> >& vector)
{
	// Collect indices to be deleted
	std::set<int> to_delete;
	for (int i = 0; i < (int)vector.size(); ++i)
	{
		for (int j = i - 1; j >= 0; --j)
		{
			if (vector.at(i).get() == vector.at(j).get())
			{
				to_delete.insert(i);
			}
		}
	}

	// Delete those indices
	std::vector<int> to_delete_vec(to_delete.begin(), to_delete.end());
	std::sort(to_delete_vec.begin(), to_delete_vec.end());
	for (int i = (int)to_delete_vec.size() - 1; i >= 0; --i)
	{
		vector.erase(vector.begin() + to_delete_vec.at(i));
	}
}

namespace data
{
	std::shared_ptr<InterUserState> InterUserState::merge(
		std::shared_ptr<const InterUserState> a,
		std::shared_ptr<const InterUserState> b,
		cv::Mat merged_stitched_screenshot)
	{
		auto a_vec = a->get_states();
		auto b_vec = b->get_states();
		std::vector<std::shared_ptr<const IntraUserState> > states;
		states.insert(states.end(), a_vec.begin(), a_vec.end());
		states.insert(states.end(), b_vec.begin(), b_vec.end());
		make_states_unique(states);
		
		return std::make_shared<InterUserState>(states, merged_stitched_screenshot);
	}

	InterUserState::InterUserState(
		IntraUserStates_const states,
		cv::Mat stitched_screenshot) :
		State(stitched_screenshot),
		_states(states) // vector with intitial intra-user states
	{
		make_states_unique(_states);
	}

	data::InterUserState::MetaInfo InterUserState::serialize(std::string directory, std::string id) const
	{
		// Stitched screenshot
		cv::imwrite(directory + id + ".png", this->get_stitched_screenshot());

		// Store some infos about intra-user states in the inter-user state to serialize
		std::vector<std::vector<std::string> > intras;

		// Prepare a structure for meta info about the inter-user state
		MetaInfo meta;

		// Go over contained intra-user states and collect input (each intra-user state belongs to a session)
		std::map<std::string, std::vector<std::tuple<int, MouseData, GazeData> > > session_input; // intra_idx (within session), mouse_data and gaze_data
		for (const auto& rsp_intra : _states)
		{
			// Get session of the intra-user state
			if (auto sp_container = rsp_intra->get_container().lock())
			{
				std::string session_id = sp_container->get_session()->get_id();
				int intra_idx = rsp_intra->get_idx_in_container();

				// Create new input entry if session found first time in inter-user state among contained intra-user states
				if (session_input.find(session_id) == session_input.end())
				{
					session_input[session_id] = { {} }; // mouse data, gaze data

					// Update meta information
					meta.session_count += 1;
				}

				// Collect input from the intra-user state (might be implemented with less copying)
				MouseData mouse_data;
				GazeData gaze_data;
				rsp_intra->push_input_into(mouse_data, gaze_data);
				session_input[session_id].push_back( { intra_idx, mouse_data, gaze_data } );

				// Remember infos about the intra-user state
				int frame_count = (rsp_intra->get_frame_idx_end() - rsp_intra->get_frame_idx_start()) + 1;
				intras.push_back({
					session_id, // session id
					std::to_string(intra_idx), // intra_idx
					std::to_string(rsp_intra->get_frame_idx_start()), // frame_idx_start
					std::to_string(rsp_intra->get_frame_idx_end()), // frame_idx_end
					std::to_string(frame_count) }); // frame_count

				// Update meta information
				meta.intra_count += 1;
				meta.frame_count += frame_count;
			}
		}

		// Input data TODO: Merge consecutive Intra-User States here (or delegate this task to the user of the serialized data)
		{
			// Mouse data header
			std::ofstream mouse_out(directory + id + "-mouse.csv");
			mouse_out << "session,";
			mouse_out << "shot_idx,";// mouse_out << "intra_idx,"; // index of intra-user state within session
			mouse_out << "timestamp,";
			mouse_out << "x,";
			mouse_out << "y,";
			mouse_out << "type\n";

			// Gaze data header
			std::ofstream gaze_out(directory + id + "-gaze.csv");
			gaze_out << "session,";
			gaze_out << "shot_idx,"; // gaze_out << "intra_idx,"; // index of intra-user state within session
			gaze_out << "timestamp,";
			gaze_out << "x,";
			gaze_out << "y\n";

			// There is a map from session id to list of mouse and gaze input
			for (const auto& r_session : session_input) // go over sessions covered by the inter-user state
			{
				// Go over list of mouse and gaze input, each pair coming from on intra-user state
				for (const auto& r_input : r_session.second) // go over input data for session
				{
					// Take mouse data and go over entries there
					for (const auto& r_mouse_data : std::get<1>(r_input)) // go over single mouse data points
					{
						mouse_out << r_session.first << ","; // session
						mouse_out << std::get<0>(r_input) << ","; // intra_idx
						mouse_out << std::to_string(std::get<0>(r_mouse_data)) << ","; // timestamp
						mouse_out << std::to_string(std::get<1>(r_mouse_data)) << ","; // x
						mouse_out << std::to_string(std::get<2>(r_mouse_data)) << ","; // y
						mouse_out << std::get<3>(r_mouse_data) << "\n"; // type
					}

					// Take gaze data and go over entries there
					for (const auto& r_gaze_data : std::get<2>(r_input)) // go over single gaze data points
					{
						gaze_out << r_session.first << ","; // session
						gaze_out << std::get<0>(r_input) << ","; // intra_idx
						gaze_out << std::to_string(std::get<0>(r_gaze_data)) << ","; // timestamp
						gaze_out << std::to_string(std::get<1>(r_gaze_data)) << ","; // x
						gaze_out << std::to_string(std::get<2>(r_gaze_data)) << "\n"; // y
					}
				}
			}
		}

		// Intras data
		std::ofstream intras_out(directory + id + "-shots.csv");
		intras_out << "session_id,";
		intras_out << "shot_idx,"; // intras_out << "intra_idx,";
		intras_out << "frame_idx_start,";
		intras_out << "frame_idx_end,";
		intras_out << "frame_count\n";
		for (const auto& r_tmp : intras)
		{
			for (int i = 0; i < (int)r_tmp.size(); ++i)
			{
				// Item
				intras_out << r_tmp.at(i);

				// Delimiter
				if (i < (int)r_tmp.size() - 1)
				{
					intras_out << ",";
				}
				else
				{
					intras_out << "\n";
				}
			}
		}

		return meta;
	}
	
	void InterUserState::add_state(
		std::shared_ptr<const IntraUserState> state,
		cv::Mat stitched_screenshot)
	{
		// Push back state
		_states.push_back(state);
		make_states_unique(_states);

		// Replace stitched screenshot
		set_stitched_screenshot(stitched_screenshot);
	}
	
	std::vector<std::shared_ptr<const IntraUserState> > InterUserState::get_states() const
	{
		return _states;
	}

	void InterUserStateContainer::serialize(std::string directory, std::string id) const
	{
		// Check first, whether this container is empty
		if (this->get()->empty()) { return; }

		// Retrieve xpath from first inter-user state's first intra-user state
		auto sp_first_intra = this->get()->front()->get_states().front();
		auto frame_idx_start = sp_first_intra->get_frame_idx_start();
		auto layer_acess = sp_first_intra->get_layer_access(frame_idx_start);
		auto wp_intra_container = sp_first_intra->get_container();
		if (auto sp_intra_container = wp_intra_container.lock())
		{
			std::string xpath = sp_intra_container->get_log_datum_container()->get()->at(frame_idx_start)->access_layer(layer_acess)->get_xpath();
			std::replace(xpath.begin(), xpath.end(), '/', '~'); // replace slash with something else. Otherwise, subfolders are created
			id = id + "_" + xpath; // append id of inter-user state container with xpath
		}

		// Create directory
		core::misc::create_directories(directory + "/" + id);

		// Collect some meta infos about each inter-user state
		std::vector<std::vector<std::string> > meta;

		// Go over states in the container and serialize them
		int i = 0;
		for (const auto & r_state : *this->get())
		{
			// Serialize state
			auto meta_data = r_state->serialize(
				directory + "/" + id + "/",
				std::to_string(i)); // id of inter-user state

			// Update meta data collection
			meta.push_back({
				std::to_string(i),
				std::to_string(meta_data.session_count),
				std::to_string(meta_data.intra_count),
				std::to_string(meta_data.frame_count) });

			// Increment state counter
			++i;
		}

		// Meta data
		std::ofstream meta_out(directory + "/" + id + "-meta.csv");
		meta_out << "stimulus_idx,";  // meta_out << "inter_idx,";
		meta_out << "session_count,";
		meta_out << "shot_count,";  // meta_out << "intra_count,";
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
	}
}
