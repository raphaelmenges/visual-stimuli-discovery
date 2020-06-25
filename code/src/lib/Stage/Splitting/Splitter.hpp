//! Splitter, derived from Work.
/*!
Walks over frames, maps layers, decides to split and fills intra-user states container.
*/

#pragma once

#include <Core/VisualChangeClassifier.hpp>
#include <Core/Task.hpp>
#include <Data/Session.hpp>
#include <Data/LogDatum.hpp>
#include <Data/IntraUserState.hpp>
#include <Util/LogDatesWalker.hpp>
#include <libsimplewebm.hpp>

namespace stage
{
	namespace splitting
	{
		/////////////////////////////////////////////////
		/// Splitter (no abstract interface required)
		/////////////////////////////////////////////////

		// Splitter class
		class Splitter : public core::Work<data::IntraUserStateContainer, core::PrintReport>
		{
		public:

			// Constructor
			Splitter(
				VD(std::shared_ptr<core::visual_debug::Dump> sp_dump, )
				std::shared_ptr<const core::VisualChangeClassifier> sp_classifier,
				std::shared_ptr<const data::LogDatumContainer> sp_log_datum_container);

		protected:

			// Returns shared pointer to product if complete, otherwise nullptr
			virtual std::shared_ptr<ProductType> internal_step();

			// Report the progess of work
			virtual void internal_report(ReportType& r_report);

		private:
		
			// Put intra-user state into product (erases entry from _current)
			void put_to_product(int current_idx);

			// Product of work
			std::shared_ptr<ProductType> _sp_container; // intra-user states container

			// Members
			std::unique_ptr<util::LogDatesWalker> _up_walker = nullptr; // walks over log dates
			std::shared_ptr<const core::VisualChangeClassifier> _sp_classifier = nullptr;
			std::shared_ptr<data::LogDates_const> _sp_log_dates = nullptr; // pointer to log dates
			std::vector<std::unique_ptr<data::IntraUserState> > _current; // currently processed intra-user states
			VD(std::vector<std::shared_ptr<core::visual_debug::Datum> > _current_vd_split_checks;) // handled parallel to _current
		};
	}
}
