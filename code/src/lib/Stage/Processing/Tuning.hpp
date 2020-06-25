//!  Tuning interface, derived from Work.
/*!
Interface for multi-threaded tunings that are applied on log dates.
*/

// works on deep copy of log dates

/* TODO: general issues
 * - do it only for layers that are scroll able (aka not fixed...)
 * - think about assert to guarantee that as many frames are outputted as inputted
 */

#pragma once

#include <Core/Task.hpp>
#include <Util/LogDatesWalker.hpp>
#include <opencv2/core/types.hpp>
#include <memory>
#include <vector>
#include <deque>

namespace stage
{
	namespace processing
	{
		namespace tuning
		{
			/////////////////////////////////////////////////
			/// Interface of tunings
			/////////////////////////////////////////////////

			// Abstract parser interface
			class Interface : public core::Work<data::LogDatumContainer, core::PrintReport>
			{
			public:

				// Constructor
				Interface(
					VD(std::shared_ptr<core::visual_debug::Dump> sp_dump, )
					std::string id)
					:
					Work(VD(sp_dump, ) core::PrintReport(id)) {} // initial empty report

				// Abstract destructor
				virtual ~Interface() = 0;
			};

			/////////////////////////////////////////////////
			/// Fixing the scrolling with ORB features
			/////////////////////////////////////////////////

			// Apply fixing of scroll estimation through ORB features detection
			class ORBscroll : public Interface
			{
			public:

				// Constructor
				ORBscroll(
					VD(std::shared_ptr<core::visual_debug::Dump> sp_dump, )
					std::shared_ptr<const data::LogDatumContainer> sp_log_datum_container);

			protected:

				// Returns shared pointer to product if complete, otherwise nullptr
				virtual std::shared_ptr<ProductType> internal_step();

				// Report the progess of work
				virtual void internal_report(ReportType& r_report);

			private:

				// Simple class to keep track of layer including the ORB features
				class ExLayer // aka extended layers
				{
				public:
					ExLayer(
						std::shared_ptr<const data::LogImage> sp_image,
						std::shared_ptr<data::Layer> sp_layer);

					// Members
					std::shared_ptr<const data::LogImage> _sp_image = nullptr;
					std::shared_ptr<data::Layer> _sp_layer = nullptr; // non-const as that is tuned
					std::vector<cv::KeyPoint> _keypoints;
					cv::Mat _descriptors;
				};

				// Estimate scrolling between two ex layers. Returns whether successful
				bool estimate_relative_scrolling(
					VD(std::shared_ptr<core::visual_debug::Datum> sp_datum, )
					std::shared_ptr<const ExLayer> sp_prev,
					std::shared_ptr<const ExLayer> sp_current,
					float& r_scroll_x,
					float& r_scroll_y) const;

				// Product of work
				std::shared_ptr<ProductType> _sp_container;

				// Members
				std::unique_ptr<util::LogDatesWalker> _up_walker = nullptr;
				std::deque<std::shared_ptr<ExLayer> > _prev_ex_layers;
			};
		}
	}
}
