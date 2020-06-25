#include "Learn.hpp"
#include <shogun/base/init.h>

namespace learn
{
	// Singleton class
	class ShogunManager
	{
	public:

		// Constructor
		ShogunManager()
		{
			shogun::init_shogun_with_defaults();
		}

		// Destructor
		~ShogunManager()
		{
			shogun::exit_shogun();
		}
	};

	// Singleton implementation
	void trigger_shogun_manager()
	{
		static ShogunManager manager;
	}

	// Learn constructor
	Learn::Learn()
	{
		// The manager takes care that Shogun is only initialized once!
		trigger_shogun_manager();
	}

	// Learn destructor
	Learn::~Learn() {}
}