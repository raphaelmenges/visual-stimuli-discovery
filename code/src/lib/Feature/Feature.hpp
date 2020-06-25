//! Abstract interface for features.
/*!
Interface for features. Always represented as single float, non-normalized.
Normalization must be achieved with training data.
Features always show the *DIFFERENCE*, not the similiartiy
*/

#pragma once

#include <Core/Core.hpp>
#include <memory>
#include <map>
#include <vector>

namespace feature
{
	template<typename T>
	class Interface
	{
	public:

		// Constructor
		Interface(
			std::shared_ptr<const T> a,
			std::shared_ptr<const T> b)
		{
			// shared pointer not used here, just provided for the interface declaration
		}

		// Get feature value
		std::map<std::string, double> get() const { return _features; }

	protected:

		// Members
		std::map<std::string, double> _features; // maps name of feature to value
	};
}