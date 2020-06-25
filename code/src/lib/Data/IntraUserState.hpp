#pragma once

#include <Data/State.hpp>
#include <Core/Core.hpp>
#include <Data/Session.hpp>
#include <Data/LogDatum.hpp>
#include <memory>
#include <string>
#include <vector>
#include <deque>

// TODO: decide how to decide on general "layer" of intra-user state. must be available to cluster into intra-user states later
// Note: captures enclosed part in time with begin and end

namespace data
{
	// Forward declaration
	class IntraUserStateContainer;

	// Intra-User State
	class IntraUserState : public State
	{
	public:

		// Constructor
		IntraUserState(
			std::weak_ptr<const IntraUserStateContainer> wp_container,
			unsigned int frame_idx_start,
			std::vector<unsigned int> layer_access, // initial layer
			const cv::Mat& pixels, // initial pixels for stitched screenshot (will be cloned internally)
			int x_offset, // x-offset of the pixels
			int y_offset); // y-offset of the pixels

		// Push input data
		void push_input_into(MouseData& r_mouse_data, GazeData& r_gaze_data) const;
		
		// Serialize state. Directory must exist
		void serialize(std::string directory, std::string id) const;

		// Get layer access of a certain datacast frame (frame_idx is in log datum indices)
		std::vector<unsigned int> get_layer_access(unsigned int frame_idx) const;

		// Push consecutive frame
		void add_frame(
			std::vector<unsigned int> layer_access, // access to the corresponding layer
			const cv::Mat& pixels, // further pixels for stitched screenshot (will be cloned internally)
			int x_offset, // x-offset of the added pixels
			int y_offset); // y-offset of the added pixels
			
		// Push frame without changing screenshot (used in cleaner)
		// Expects caller to take care of sanity, e.g., not pushing behind limits of screencast
		void push_blind_frame(
			std::vector<unsigned int> layer_access,
			bool front = false);
			
		// Getter
		unsigned int get_frame_idx_start() const { return _frame_idx_start; }
		unsigned int get_frame_idx_end() const { return _frame_idx_end; }
		unsigned int get_frame_count() const { return ((int)_frame_idx_end - (int)_frame_idx_start) + 1;}
		std::weak_ptr<const IntraUserStateContainer> get_container() const { return _wp_container; }

		// Get index of intra-user state in its container
		unsigned int get_idx_in_container() const;
		
		// Get frame count of contained frames
		virtual unsigned int get_total_frame_count() const { return get_frame_count(); }
		
		// Get count of contained sessions
		virtual unsigned int get_total_session_count() const { return 1; }
		
	private:

		// Store and stitch screenshot from pixels
		void store_and_stich(const cv::Mat& pixels, int x_offset, int y_offset);

		// Members
		std::weak_ptr<const IntraUserStateContainer> _wp_container;
		unsigned int _frame_idx_start; // start index in screencast (including)
		unsigned int _frame_idx_end; // end index in screencast (including)
		std::deque<std::vector<unsigned int> > _layer_accesses; // for each frame_idx (_frame_idx_end-_frame_idx_start+1 many), store which layer from the log dates is captured here
		std::deque<std::tuple<std::shared_ptr<std::vector<uchar> >, int, int> > _pixel_history; // compressed pixels, x-offset and y-offset
		std::vector<unsigned int> _blind_frame_idxs; // idxs of frames that are not contributing to the screenshot
		// TODO: store viewport position for each frame
	};
	
	// Typedefs
	typedef std::vector<std::shared_ptr<IntraUserState> > IntraUserStates;
	typedef const std::vector<std::shared_ptr<const IntraUserState> > IntraUserStates_const;
	
	// Intra-User State Container (for one session)
	class IntraUserStateContainer
	{
	public:
	
		// Constructor
		IntraUserStateContainer(
			std::shared_ptr<const LogDatumContainer> sp_log_datum_container)
			:
			_sp_log_datum_container(sp_log_datum_container) {}

		// Push back one intra-user state
		void push_back(std::shared_ptr<IntraUserState> sp_intra_user_state)
		{
			_sp_intra_user_states->push_back(sp_intra_user_state);
			_sp_intra_user_states_const = core::misc::make_const(_sp_intra_user_states);
		}
		
		// Get intra-user states
		std::shared_ptr<IntraUserStates> get()
		{
			return _sp_intra_user_states;
		}
		
		// Get const intra-user states
		std::shared_ptr<IntraUserStates_const> get() const
		{
			return _sp_intra_user_states_const;
		}

		// Clear intra-user states
		void clear()
		{
			_sp_intra_user_states->clear();
			_sp_intra_user_states_const = core::misc::make_const(_sp_intra_user_states);
		}

		// Get session
		std::shared_ptr<const Session> get_session() const { return _sp_log_datum_container->get_session(); }

		// Get log datum container
		std::shared_ptr<const LogDatumContainer> get_log_datum_container() const { return _sp_log_datum_container; }

		// Serialize container (directory is automatically created)
		void serialize(std::string directory) const;
		
		// Add empty frames (layer id can be anything, maybe xpath of layer if available)
		void add_empty_frame(std::string layer_id, unsigned int empty_frame)
		{
			auto iter = _empty_frames.find(layer_id);
			if(iter != _empty_frames.end())
			{
				iter->second.emplace(empty_frame);
			}
			else
			{
				_empty_frames[layer_id] = { empty_frame };
			}
		}
	
	private:

		// Remove copy and assignment operators
		IntraUserStateContainer(const IntraUserStateContainer&) = delete;
		IntraUserStateContainer& operator=(const IntraUserStateContainer&) = delete;
	
		// Members
		std::shared_ptr<const LogDatumContainer> _sp_log_datum_container = nullptr;
		std::shared_ptr<IntraUserStates> _sp_intra_user_states = std::make_shared<IntraUserStates>();
		std::shared_ptr<IntraUserStates_const> _sp_intra_user_states_const = std::make_shared<IntraUserStates_const>();
		std::map<std::string, std::set<unsigned int> > _empty_frames; // maps layer id to set of empty frames
	};
	
	// Typedefs
	typedef std::vector<std::shared_ptr<IntraUserStateContainer> > IntraUserStateContainers;
	typedef const std::vector<std::shared_ptr<const IntraUserStateContainer> > IntraUserStateContainers_const;
}
