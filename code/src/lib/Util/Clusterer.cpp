#include "Clusterer.hpp"

#include <Util/LayerComparator.hpp>

const float LAYER_CLUSTER_THRESHOLD = core::mt::get_config_value(0.75f, { "model", "layer_cluster_threshold" });

namespace util
{
	namespace clusterer
	{
		template<typename T>
		std::vector<
			std::shared_ptr<std::vector<
				std::shared_ptr<T> > > >
		compute(std::vector<std::shared_ptr<T> >  intras)
		{
			// Create empty output
			std::vector<
				std::shared_ptr<std::vector<
				std::shared_ptr<T> > > > output;

			// Count of intra-user states
			unsigned int intras_size = (unsigned int)intras.size();

			// Create similarity matrix between all intra-user states
			cv::Mat layer_similarity_matrix = cv::Mat::zeros(intras_size, intras_size, CV_32F); // strictly upper triangle part is used, as similarity is symmetric

			// Calculate layer similarity
			for (unsigned int i = 0; i < intras_size; ++i) // rows of matrix
			{
				for (unsigned int j = i; j < intras_size; ++j) // columns of matrix
				{
					// Do not compare with itself
					if (i == j)
					{
						layer_similarity_matrix.at<float>(cv::Point(i, i)) = -1.f;
						continue;
					}

					// Go over all layers of both intra-user states
					auto sp_intra_a = intras.at(i);
					auto sp_intra_b = intras.at(j);

					// Get containers
					auto sp_intra_a_container = sp_intra_a->get_container().lock();
					auto sp_intra_b_container = sp_intra_b->get_container().lock();

					// Check whether weak pointer could be made shared
					if (sp_intra_a_container && sp_intra_b_container)
					{
						// Get log dates
						auto sp_log_dates_a = sp_intra_a_container->get_log_datum_container();
						auto sp_log_dates_b = sp_intra_b_container->get_log_datum_container();

						// Go over all frames of both intra-user states and compare layers
						util::ScoreAcc<> score_acc;
						for (
							unsigned int idx_a = sp_intra_a->get_frame_idx_start();
							idx_a <= sp_intra_a->get_frame_idx_end();
							++idx_a)
						{
							auto sp_layer_a = sp_log_dates_a->get()->at(idx_a)->access_layer(sp_intra_a->get_layer_access(idx_a));
							for (
								unsigned int idx_b = sp_intra_b->get_frame_idx_start();
								idx_b <= sp_intra_b->get_frame_idx_end();
								++idx_b)
							{
								auto sp_layer_b = sp_log_dates_b->get()->at(idx_b)->access_layer(sp_intra_b->get_layer_access(idx_b));

								// Compare both layers and accumulate score
								score_acc.push_back(util::layer_comparator::compare(sp_layer_a, sp_layer_b));
							}
						}

						layer_similarity_matrix.at<float>(cv::Point(j, i)) = score_acc.calc_average();
						layer_similarity_matrix.at<float>(cv::Point(i, j)) = score_acc.calc_average();;
					}
				}
			}

			// Store the information which intra-user state has been put into which cluster, -1 if not yet put into a cluster
			std::vector<int> intras_to_cluster(intras_size, -1);

			// Find maximum value in layer-wise similarity of two intra-user states
			double min_val, max_val;
			cv::Point2i min_loc, max_loc;
			cv::minMaxLoc(layer_similarity_matrix, &min_val, &max_val, &min_loc, &max_loc);
			while (max_val >= LAYER_CLUSTER_THRESHOLD) // do it while there are still entries in the similarity matrix. But only to certain threshold
			{
				// Get the most similar intra-user states, layer-wise
				int i = max_loc.y; // row
				int j = max_loc.x; // column

				// Set similiarity to -1 to avoid working on this pair of intra-user states again
				layer_similarity_matrix.at<float>(cv::Point(j, i)) = -1.f;
				layer_similarity_matrix.at<float>(cv::Point(i, j)) = -1.f;

				// Check whether intra-user states have been already put into a cluster
				bool i_clustered = intras_to_cluster.at(i) >= 0;
				bool j_clustered = intras_to_cluster.at(j) >= 0;

				// Act according to the configuration
				if (i_clustered && j_clustered)
				{
					// Both are already clustered, do not bother
				}
				else if (i_clustered)
				{
					// i has been added to a cluster, push back also j to that cluster
					int idx = intras_to_cluster.at(i);
					output.at(idx)->push_back(intras.at(j));
					intras_to_cluster.at(j) = idx;

				}
				else if (j_clustered)
				{
					// j has been added to a cluster, push back also i to that cluster
					int idx = intras_to_cluster.at(j);
					output.at(idx)->push_back(intras.at(i));
					intras_to_cluster.at(i) = idx;
				}
				else
				{
					// None of the intra-user states has been added to a cluster, create a new cluster
					int idx = (int)output.size();
					output.push_back(
						std::make_shared<std::vector<std::shared_ptr<T> > >());
					output.at(idx)->push_back(intras.at(i));
					output.at(idx)->push_back(intras.at(j));
					intras_to_cluster.at(i) = idx;
					intras_to_cluster.at(j) = idx;
				}

				// Update max value
				cv::minMaxLoc(layer_similarity_matrix, &min_val, &max_val, &min_loc, &max_loc);
			}

			// Go over not yet clustered orphans who have not found a fitting partner to be clustered with
			for (int i = 0; i < (int)intras_size; ++i)
			{
				if (intras_to_cluster.at(i) < 0)
				{
					output.push_back(
						std::make_shared<std::vector<std::shared_ptr<T> > >());
					output.back()->push_back(intras.at(i));
				}
			}
			
			return output;
		}
		
		// Const version
		template std::vector< // list of clusters
			std::shared_ptr<std::vector< // list of intra-user states in a cluster
			std::shared_ptr<const data::IntraUserState> > > > // intra-user states
		compute<const data::IntraUserState>(std::vector<std::shared_ptr<const data::IntraUserState> > intras);
		
		// Non-const version
		template std::vector< // list of clusters
			std::shared_ptr<std::vector< // list of intra-user states in a cluster
			std::shared_ptr<data::IntraUserState> > > > // intra-user states
		compute<data::IntraUserState>(std::vector<std::shared_ptr<data::IntraUserState> > intras);
	}
}
