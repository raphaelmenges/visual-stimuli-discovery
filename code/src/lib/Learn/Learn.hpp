//! Abstract base class for all learn classes.
/*!
This abstract base class takes care of the initialization of the Shogun toolkit.
*/

#pragma once

namespace learn
{
	class Learn
	{
	public:
		Learn();
		virtual ~Learn() = 0;
	};
}