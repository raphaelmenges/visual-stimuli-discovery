#include "Merging.hpp"

#include <Core/Core.hpp>
#include <Util/Clusterer.hpp>
#include <Stage/Merging/Merger.hpp>

// TODO: layer cluster object, containing id? e.g. most common xpath? then, id for merger can be handled more like it has been done for splitter

namespace stage
{
	namespace merging
	{
		std::shared_ptr<data::InterUserStateContainers_const> run(
			VD(core::visual_debug::Explorer& r_visual_explorer, )
			std::shared_ptr<const core::VisualChangeClassifier> sp_classifier,
			std::shared_ptr<data::IntraUserStateContainers_const> sp_intra_containers)
		{
			core::mt::log_info("# Merging Stage");

			// Create empty output of the stage (one inter-user state container per layer cluster)
			auto sp_containers = std::make_shared<data::InterUserStateContainers>();

			/////////////////////////////////////////////////
			/// Layer Clustering
			/////////////////////////////////////////////////
			
			// Collect all intra-user states from all containers into one big vector
			std::vector<std::shared_ptr<const data::IntraUserState> > intras;
			for (const auto sp_intra_container : *sp_intra_containers.get())
			{
				intras.insert(intras.end(), sp_intra_container->get()->begin(), sp_intra_container->get()->end());
			}
			
			// Cluster the intra-user states across users per cluster
			auto layer_clusters = util::clusterer::compute(intras);

			/////////////////////////////////////////////////
			/// State Merging
			/////////////////////////////////////////////////
			// Notes:
			// - States with non-overlapping stitched screenshots are treated as non-similiar (similarity is set to zero in Merge model)

			core::mt::log_info("## State Merging");

			// Have one merger per intra-user state cluster
			typedef core::Task<Merger, 1> MergerTask;
			core::TaskContainer<MergerTask> mergers;

			// Create one merger for each cluster
			for (int i = 0; i < (int)layer_clusters.size(); ++i)
			{
				// Create visual debug dump
				VD(
				std::shared_ptr<core::visual_debug::Dump> sp_dump = nullptr;
				if (core::mt::get_config_value(false, { "visual_debug", "enable_for", "merger" }))
				{
					sp_dump = r_visual_explorer.create_dump("cluster_" + std::to_string(i), "3.1 Merging");
				})

				// Create merger task
				auto sp_task = std::make_shared<MergerTask>(
					VD(sp_dump, ) // provide visual debug dump
					sp_classifier, // visual change classifier
					"cluster_" + std::to_string(i),
					layer_clusters.at(i)); // shared pointer to intra-user states of cluster

				// Put pack into vector of mergers
				mergers.push_back(sp_task);
			}

			// Report about progress on merger tasks
			mergers.wait_and_report();

			// Collect products for each session
			for (auto& rsp_merger : mergers.get())
			{
				sp_containers->push_back(rsp_merger->get_product());
			}

			// Return ready-to-use inter-user states containers
			return core::misc::make_const(sp_containers);
		}
	}
}
