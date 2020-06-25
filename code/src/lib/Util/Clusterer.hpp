//! Clusters intra-user states by layer.
/*!
Takes a vector of intra-user states and clusters them by layer, using a comparator.
*/

#pragma once

#include <Data/IntraUserState.hpp>

namespace util
{
	// Function to cluster intra-user states by layer
	namespace clusterer
	{
		// Function that does the work
		template<typename T>
		std::vector< // list of clusters
			std::shared_ptr<std::vector< // list of intra-user states in a cluster
				std::shared_ptr<T> > > > // intra-user states
		compute(std::vector<std::shared_ptr<T> > intras);
	}
}
