//! Merger, derived from Work.
/*!
Merges intra-user states into inter-user states
*/

#pragma once

#include <Core/VisualChangeClassifier.hpp>
#include <Core/Task.hpp>
#include <Data/InterUserState.hpp>
#include <Eigen/Dense>

namespace stage
{
	namespace merging
	{
		/////////////////////////////////////////////////
		/// Merger
		/////////////////////////////////////////////////

		// Clusterer class
		class Merger : public core::Work<data::InterUserStateContainer, core::PrintReport>
		{
		public:

			// Internal parser phases
			enum class Phase { InitSimiliarityMatrix, Merging, Finalize };

			// Constructor
			Merger(
				VD(std::shared_ptr<core::visual_debug::Dump> sp_dump, )
				std::shared_ptr<const core::VisualChangeClassifier> sp_classifier,
				std::string id,
				std::shared_ptr<data::IntraUserStates_const> sp_intras);

		protected:

			// Returns shared pointer to product if complete, otherwise nullptr
			virtual std::shared_ptr<ProductType> internal_step();

			// Report the progess of work
			virtual void internal_report(ReportType& r_report);

		private:

			// Initialize similarity matrix
			bool init_similarity_matrix();

			// Merge intra-user states into inter-user states
			bool merge();

			// Finalize inter-user states
			bool finalize();

			// Product of work
			std::shared_ptr<ProductType> _sp_container = nullptr; // inter-user state container

			// Members
			std::shared_ptr<const core::VisualChangeClassifier> _sp_classifier = nullptr;
			std::shared_ptr<data::IntraUserStates_const> _sp_intras;
			Phase _phase = Phase::InitSimiliarityMatrix;
			Eigen::Matrix<core::long64, Eigen::Dynamic, Eigen::Dynamic> _similarity_matrix;
			std::shared_ptr<data::InterUserStates> _sp_inters = nullptr;
			core::long64 _last_min_merged_similarity = std::numeric_limits<core::long64>::max();
			
		};
	}
}
