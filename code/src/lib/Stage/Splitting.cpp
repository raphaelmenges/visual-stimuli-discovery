#include "Splitting.hpp"

#include <Core/Core.hpp>
#include <Stage/Splitting/Splitter.hpp>
#include <Stage/Splitting/Cleaner.hpp>

namespace stage
{
	namespace splitting
	{
		std::shared_ptr<data::IntraUserStateContainers_const> run(
			VD(core::visual_debug::Explorer& r_visual_explorer, )
			std::shared_ptr<const core::VisualChangeClassifier> sp_classifier,
			std::shared_ptr<data::LogDatumContainers_const> sp_log_datum_containers)
		{
			core::mt::log_info("# Splitting Stage");

			// Create empty output of the stage (one intra-user states container per session)
			auto sp_intra_user_state_containers = std::make_shared<data::IntraUserStateContainers>();
			
			/////////////////////////////////////////////////
			/// Splitting
			/////////////////////////////////////////////////

			core::mt::log_info("## Splitting");

			// Have one splitter per session
			typedef core::Task<Splitter, 1> SplitterTask;
			core::TaskContainer<SplitterTask> splitters;

			// Create one splitter for each session
			for (std::shared_ptr<const data::LogDatumContainer> sp_log_datum_container : *sp_log_datum_containers.get())
			{
				// Create visual debug dump
				VD(
				std::shared_ptr<core::visual_debug::Dump> sp_dump = nullptr;
				if (core::mt::get_config_value(false, { "visual_debug", "enable_for", "splitter" }))
				{
					sp_dump = r_visual_explorer.create_dump(sp_log_datum_container->get_session()->get_id(), "2.1 Splitter Stage: Splitter");
				})

				// Create splitter task
				auto sp_task = std::make_shared<SplitterTask>(
					VD(sp_dump, ) // provide visual debug dump
					sp_classifier, // classifier of visual change
					sp_log_datum_container); // log dates container

				// Put pack into vector of splitters
				splitters.push_back(sp_task);
			}

			// Report about progress on splitter tasks
			splitters.wait_and_report();

			// Collect products for each session
			for (auto& rsp_splitter : splitters.get())
			{
				sp_intra_user_state_containers->push_back(rsp_splitter->get_product());
			}
			
			/////////////////////////////////////////////////
			/// Cleaning
			/////////////////////////////////////////////////
			/// Cleaner gets a modifiable version of the intra-user states container!
			/// Would be cleaner if it gets const versions and performs copying...

			core::mt::log_info("## Cleaning");

			// Have one cleaner per session
			typedef core::Task<Cleaner, 1> CleanerTask;
			core::TaskContainer<CleanerTask> cleaners;
			
			// Create one cleaner for each intra-user state container (aka session)
			for (auto sp_intra_user_state_container : *sp_intra_user_state_containers.get())
			{
				// Create visual debug dump
				VD(
				std::shared_ptr<core::visual_debug::Dump> sp_dump = nullptr;
				if (core::mt::get_config_value(false, { "visual_debug", "enable_for", "cleaner" }))
				{
					sp_dump = r_visual_explorer.create_dump(sp_intra_user_state_container->get_session()->get_id(), "2.2 Splitter Stage: Cleaner");
				})

				// Create cleaner task
				auto sp_task = std::make_shared<CleanerTask>(
					VD(sp_dump, )
					sp_intra_user_state_container); // intra-user state container

				// Put pack into vector of cleaners
				cleaners.push_back(sp_task);
			}

			// Report about progress on cleaner tasks
			cleaners.wait_and_report();

			// Reset local intra-user states containers object to be filled with cleaned containers
			sp_intra_user_state_containers = std::make_shared<data::IntraUserStateContainers>();
			
			// Collect products for each session
			for (auto& rsp_cleaner : cleaners.get())
			{
				sp_intra_user_state_containers->push_back(rsp_cleaner->get_product());
			}

			// Return ready-to-use intra-user states containers
			return core::misc::make_const(sp_intra_user_state_containers);
		}
	}
}
