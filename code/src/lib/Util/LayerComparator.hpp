//! Layer comparator.
/*!
Interface for calculating similarity score between two layers.
*/

// TODO: maybe input here rather layers descriptor than layers directly
// TODO: descriptors could summarize multiple layers (might speed up clustering etc in the end)
// TODO: layer descriptors might be also useful to provide comprehensive description of layers to later user

#pragma once

#include <Util/Score.hpp>
#include <Data/Layer.hpp>

// Layer comparator
namespace util
{
	namespace layer_comparator
	{
		// Returns similiarity score of two layers
		Score<> compare(std::shared_ptr<const data::Layer> a, std::shared_ptr<const data::Layer> b);
	}
}