#pragma once

#include <Core/Core.hpp>
#include <Core/VisualDebug.hpp>
#include <thread>
#include <mutex>
#include <chrono>
#include <future>

namespace core
{
	// Template for work, taking product and report type as argument
	// Work is not thread-safe on its own
	template<typename P, typename R>
	class Work
	{
	public:

		// Constructor, taking initial report as parameter
		Work(
			VD(std::shared_ptr<core::visual_debug::Dump> sp_dump,)
			R initial_report)
			:
			VD(_sp_dump(sp_dump),)
			_report(initial_report) {}

		// Remove copy and assignment constructors
		Work(const Work&) = delete;
		Work& operator=(Work const&) = delete;

		// Returns shared pointer to product if complete, otherwise nullptr
		std::shared_ptr<P> step()
		{
			return internal_step();
		}

		// Report progess
		void report()
		{
			internal_report(_report);
		}

		// Get report copy
		R get_report_copy() const
		{
			return _report;
		}

		// Typedefs to allow owner to know types of delegates
		typedef P ProductType;
		typedef R ReportType;

	protected:

		// Implemented by subclass
		virtual std::shared_ptr<ProductType> internal_step() = 0;
		virtual void internal_report(ReportType& r_report) = 0;

		// Dump for visual debugging
		VD(std::shared_ptr<core::visual_debug::Dump> _sp_dump;)

	private:

		// Instance of the report
		R _report;
	};

	// Task template taking work type and step_size (default = 1) as arguments
	// Task accesses work object in a thread-safe way
	template <typename W, int S = 1>
	class Task
	{
	public:

		// Constructor passing parameters to work object. Starts instantly to work
		template <class...W_params>
		Task(
			const W_params&... params
		) : _product(_promise.get_future())
		{
			// Create work with provided parameters
			_up_work = std::unique_ptr<W>(new W(params...));

			// Function to execute
			std::function<std::shared_ptr<typename W::ProductType>()> function = [&]
			{
				// Execute work until it is done
				std::shared_ptr<typename W::ProductType> sp_product = nullptr;
				do {
					// Perform steps of work
					static_assert(S > 0, "Task requires a step size bigger than zero!");
					int i = 0;
					for (; i < S; i++)
					{
						sp_product = _up_work->step();
						if (sp_product != nullptr)
						{
							break;
						}
					}

					// Perform reporting in own scope to use lock_guard
					{
						std::lock_guard<std::mutex> lock(_report_mutex);
						_up_work->report();
					}

				} while (sp_product == nullptr);

				// Return product of work in an efficient way
				return sp_product;
			};


#ifdef GM_MT_TASK
			// Launch asynchronous execution of work
			_product = std::async(std::launch::async, function);
#else
			// Execute work in callee thread (useful for debugging purposes)
			_promise.set_value(function());
#endif // GM_MT_TASK

		}

		// Remove copy and assignment constructors
		Task(const Task&) = delete;
		Task& operator=(Task const&) = delete;

		virtual ~Task() {}

		auto get_report_copy() const -> typename W::ReportType
		{
			std::lock_guard<std::mutex> lock(_report_mutex);
			return _up_work->get_report_copy();
		}

		bool working() const
		{
			auto status = _product.wait_for(std::chrono::milliseconds(0));
			return status != std::future_status::ready;
		}

		// Waits for work to finish and make product available
		std::shared_ptr<typename W::ProductType> get_product()
		{
			return _product.get();
		}

	private:

		// Members
		std::unique_ptr<W> _up_work = nullptr;
		std::promise<std::shared_ptr<typename W::ProductType> > _promise;
		std::future<std::shared_ptr<typename W::ProductType> > _product;
		mutable std::mutex _report_mutex; // can be used in const methods, too
	};

	// Task pack and helper functions
	template<typename D, typename T>
	class TaskPack
	{
	public:

		// Members
		std::shared_ptr<D> sp_data;
		std::shared_ptr<T> sp_task;
	};

	// Task container
	template <typename T>
	class TaskContainer
	{
	public:

		// Push back task
		void push_back(std::shared_ptr<T> sp_task)
		{
			_tasks.push_back(sp_task);
		}

		// Check whether any of the tasks is still working
		bool any_working() const
		{
			return std::accumulate(
				_tasks.begin(),
				_tasks.end(),
				false,
				[](const bool acc, std::shared_ptr<const T> sp_task)
			{
				return acc || sp_task->working();
			});
		}

		// Report progess of all tasks (assuming they are implementing "PrintReport" 
		void report_progress() const
		{
			for (const auto& rsp_task : _tasks)
			{
				rsp_task->get_report_copy().print(); // relies on the existence of a print function
			}
		}

		// Wait and report
		void wait_and_report()
		{
			while (any_working())
			{
				std::this_thread::sleep_for(std::chrono::milliseconds(core::mt::get_config_value(500, { "general", "task_report_wait_ms" })));
				report_progress();
			}
		}

		// Get tasks (e.g., to retrieve products)
		std::vector<std::shared_ptr<T> > get()
		{
			return _tasks;
		}
	
	private:

		std::vector<std::shared_ptr<T> > _tasks;
	};

	// Simple report class that can report onto the console
	class PrintReport
	{
	public:

		// Constructor
		PrintReport(std::string id) : _id(id) {}

		// Print to console
		void print() const
		{
			core::mt::log_info(_id, ": ", core::misc::to_percentage_str(_progress));
		}

		// Setters
		void set_progress(float progress)
		{
			_progress = progress;
		}

	private:

		std::string _id = "";
		float _progress = 0.f; // should be between zero and one
	};
}
