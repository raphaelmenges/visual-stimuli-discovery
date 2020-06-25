#include "Processing.hpp"

#include <Stage/Processing/Parser.hpp>
#include <Stage/Processing/Tuning.hpp>
#include <Core/Core.hpp>

namespace stage
{
	namespace processing
	{
		std::shared_ptr<data::LogDatumContainers_const> run(
			VD(core::visual_debug::Explorer& r_visual_explorer, )
			std::shared_ptr<data::Sessions_const> sp_sessions)
		{
			core::mt::log_info("# Processing Stage");

			core::mt::log_info("## Parsing");

			// Create empty output of the stage (one log datum container per session)
			auto sp_log_datum_containers = std::make_shared<data::LogDatumContainers>();

			// Have one parser per session to create log dates
			typedef core::Task<parser::LogRecord, 1> ParserTask;
			core::TaskContainer<ParserTask> parsers;

			// Create one parser for each session
			for (auto sp_session : *sp_sessions.get())
			{
				// Create visual debug dump
				VD(
				std::shared_ptr<core::visual_debug::Dump> sp_dump = nullptr;
				if (core::mt::get_config_value(false, { "visual_debug", "enable_for", "parser" }))
				{
					sp_dump = r_visual_explorer.create_dump(sp_session->get_id(), "1.1 Processing Stage: Parser");
				})

				// Create parser task
				auto sp_task = std::make_shared<ParserTask>(
					VD(sp_dump, ) // provide visual debug dump
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

			core::mt::log_info("## Tuning");

			// Have one ORBscroll per session
			typedef core::Task<tuning::ORBscroll, 1> ORBscrollTask;
			core::TaskContainer<ORBscrollTask> orb_scrolls;

			// Create one ORBscroll for each log dates container
			for (auto sp_log_dates_container : *sp_log_datum_containers.get())
			{
				// Create visual debug dump
				VD (
				std::shared_ptr<core::visual_debug::Dump> sp_dump = nullptr;
				if (core::mt::get_config_value(false, { "visual_debug", "enable_for", "orb_scroll" }))
				{
					sp_dump = r_visual_explorer.create_dump(sp_log_dates_container->get_session()->get_id(), "1.2 Processing Stage: ORB Scroll");
				})

				// Create ORBscroll task
				auto sp_task = std::make_shared<ORBscrollTask>(
					VD(sp_dump, ) // provide visual debug dump
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

			// Return ready-to-use log dates containers
			return core::misc::make_const(sp_log_datum_containers);
		}
	}
}
