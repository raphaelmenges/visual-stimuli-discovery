#include "LayerComparator.hpp"

namespace util
{
	namespace layer_comparator
	{
		Score<> compare(std::shared_ptr<const data::Layer> a, std::shared_ptr<const data::Layer> b)
		{
			Score<> score;

			/*
			if (a->get_type() == b->get_type())
			{
				score.add(0.5f); // might lead to merge different layers of same type for no reason
			*/

			if (a->get_xpath() == b->get_xpath())
			{
				score.add(1.0f);
			}
			return score;
		}
	}
}