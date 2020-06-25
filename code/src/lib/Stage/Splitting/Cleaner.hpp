//! Cleaner, derived from Work.
/*!
Cleans up intra-user states containers by merging too small states into bigger ones.
*/

#pragma once

#include <Core/Task.hpp>
#include <Data/IntraUserState.hpp>

namespace stage
{
	namespace splitting
	{
		/////////////////////////////////////////////////
		/// Cleaner (no abstract interface required)
		/////////////////////////////////////////////////

		// Cleaner class
		class Cleaner : public core::Work<data::IntraUserStateContainer, core::PrintReport>
		{
		public:

			// Constructor
			Cleaner(
				VD(std::shared_ptr<core::visual_debug::Dump> sp_dump, )
				std::shared_ptr<data::IntraUserStateContainer> sp_intra_user_state_container);

		protected:

			// Returns shared pointer to product if complete, otherwise nullptr
			virtual std::shared_ptr<ProductType> internal_step();

			// Report the progess of work
			virtual void internal_report(ReportType& r_report);

		private:

			// Product of work
			std::shared_ptr<ProductType> _sp_container; // intra-user states container (original is only modified!)
		};
	}
}
