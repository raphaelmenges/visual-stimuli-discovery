//!  Parser interface, derived from Work.
/*!
Interface for multi-threaded parsers that provide log dates. Implementation of interface for local LogRecord provided.
*/

// TODO: beware!; some parts of the datacast might be empty
// Assumption: browser viewport completely visible. Breaks otherwise. Viewport is cut from screenshot in "LogImage" class
// TODO: use status of libsimplewebm to provide user with more feedback
// TODO: fixed header slightly later recognized than video start. introduce some shift to consider everything until X from start?

#pragma once

#include <Core/Task.hpp>
#include <Data/LogDatum.hpp>
#include <nlohmann/json.hpp>
#include <opencv2/core/types.hpp>
#include <memory>
#include <vector>

using json = nlohmann::json;

namespace stage
{
	namespace processing
	{
		namespace parser
		{
			/////////////////////////////////////////////////
			/// Interface of parsers
			/////////////////////////////////////////////////

			// Report of parsers
			class Report : public core::PrintReport
			{
			public:
				Report(std::string id) : PrintReport(id) {}

				// TODO: move below to private space
				unsigned int frame_idx = 0; // currently processed frame
			};

			// Abstract parser interface. Is derivate of work with log dates as product
			class Interface : public core::Work<data::LogDatumContainer, Report>
			{
			public:

				// Constructor
				Interface(
					VD(std::shared_ptr<core::visual_debug::Dump> sp_dump, )
					std::string id)
					:
					Work(VD(sp_dump, ) Report(id)) {} // initial empty report

				// Abstract destructor
				virtual ~Interface() = 0;
			};

			/////////////////////////////////////////////////
			/// Implementation of interface for LogRecord
			/////////////////////////////////////////////////

			// Implementation of parser interface for locally stored log records
			class LogRecord : public Interface
			{
			public:

				// Internal parser phases
				enum class Phase { Events, Layers, Input }; // these are blocks in the datacast

				// Constructor
				LogRecord(
					VD(std::shared_ptr<core::visual_debug::Dump> sp_dump, )
					std::shared_ptr<const data::Session> sp_session);

			protected:

				// Returns shared pointer to product if complete, otherwise nullptr
				virtual std::shared_ptr<ProductType> internal_step();

				// Report the progess of work
				virtual void internal_report(ReportType& r_report);

			private:

				// Phase of parsing events, performs one step of parsing (aka one frame), returns whether done
				bool parse_events();

				// Phase of parsing layers, returns whether done
				bool parse_layers();

				// Phase of parsing input like interaction and gaze data, returns whether done
				bool parse_input();

				// Product of work
				std::shared_ptr<ProductType> _sp_container;

				// Data of log record in memory for parsing
				json _datacast;
				std::shared_ptr<std::vector<double> > _sp_times = std::shared_ptr<std::vector<double> >(new std::vector<double>); // required info from screencast
				unsigned int _frame_count = 0;

				// Internal phase of the parser (for the stepwise execution)
				Phase _phase = Phase::Events; // events are parsed in log records, layers are then integrated to these structures

				// Events phase
				json _events;
				json::iterator _it_events; // go over events, framewise
				unsigned int _events_frame_idx = 0;

				// Layers phase
				json _layers;
				json::iterator _it_layers;

				// Input phase
				json _mouse;
				json::iterator _it_mouse;
				json _gaze;
				json::iterator _it_gaze;

				// Other
				std::string _webm_path;
				std::vector<core::long64> document_change_times_ms; // in milliseconds
				int _frame_duration = 0; // duration of one frame in the screencast
			};
		}
	}
}
