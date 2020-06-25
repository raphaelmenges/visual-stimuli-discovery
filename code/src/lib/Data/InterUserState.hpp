#pragma once

#include <Data/IntraUserState.hpp>

namespace data
{
	// Inter-User State
	class InterUserState : public State
	{
	public:

		// Meta information for serialization of inter-user state container
		struct MetaInfo
		{
			int session_count = 0;
			int intra_count = 0;
			int frame_count = 0;
		};

		// Merge to inter-user states into one TODO: maybe make merged stitched screenshot optional and implement merging of screenshot in method
		static std::shared_ptr<InterUserState> merge(
			std::shared_ptr<const InterUserState> a,
			std::shared_ptr<const InterUserState> b,
			cv::Mat merged_stitched_screenshot);

		// Constructor
		InterUserState(
			IntraUserStates_const states, // initial contained intra-user states
			cv::Mat stitched_screenshot); // initial stitched screenshot

		// Serialize state. Directory must exist
		MetaInfo serialize(std::string directory, std::string id) const;
			
		// Add intra-user state
		void add_state(
			std::shared_ptr<const IntraUserState> state, // intra-user state that is added to the inter-user state
			cv::Mat stitched_screenshot); // replacement of the stitched screenshot
			
		// Get intra-user states
		std::vector<std::shared_ptr<const IntraUserState> > get_states() const;
		
		// Get frame count of contained frames
		virtual unsigned int get_total_frame_count() const
		{
			unsigned int count = 0;
			for(const auto& rsp_state : _states)
			{
				count += rsp_state->get_frame_count();
			}
			return count;
		}
		
		// Get count of contained sessions
		virtual unsigned int get_total_session_count() const
		{
			std::set<std::string> session_ids; // only unique sessions are counted
			for(const auto& rsp_state : _states)
			{
				if(auto sp_container = rsp_state->get_container().lock())
				{
					session_ids.emplace(sp_container->get_session()->get_id());
				}
				
			}
			return (unsigned int)session_ids.size();
		}

	private:

		// Members
		std::vector<std::shared_ptr<const IntraUserState> > _states;
		
	};	
	
	// Typedefs
	typedef std::vector<std::shared_ptr<InterUserState> > InterUserStates;
	typedef const std::vector<std::shared_ptr<const InterUserState> > InterUserStates_const;
	
	// Inter-User State Container (for one layer cluster)
	class InterUserStateContainer
	{
	public:
	
		// Constructor
		InterUserStateContainer() {}
			
		// Set inter-user states
		void set(std::shared_ptr<InterUserStates> sp_inter_user_states)
		{
			_sp_inter_user_states = sp_inter_user_states;
			_sp_inter_user_states_const = nullptr;
		}
		
		// Get inter-user states
		std::shared_ptr<InterUserStates_const> get() const
		{
			if (_sp_inter_user_states_const == nullptr)
			{
				_sp_inter_user_states_const = core::misc::make_const(_sp_inter_user_states);
			}
			return _sp_inter_user_states_const;
		}

		// Serialize container (directory is automatically created)
		void serialize(std::string directory, std::string id) const;
	
	private:

		// Remove copy and assignment operators
		InterUserStateContainer(const InterUserStateContainer&) = delete;
		InterUserStateContainer& operator=(const InterUserStateContainer&) = delete;
	
		// Members
		std::shared_ptr<InterUserStates> _sp_inter_user_states = std::make_shared<InterUserStates>();
		mutable std::shared_ptr<InterUserStates_const> _sp_inter_user_states_const = nullptr;  // is updated if required by getter
	};
	
	// Typedefs
	typedef std::vector<std::shared_ptr<InterUserStateContainer> > InterUserStateContainers;
	typedef const std::vector<std::shared_ptr<const InterUserStateContainer> > InterUserStateContainers_const;
}
