#pragma once

#include <memory>
#include <string>
#include <vector>

namespace data
{
	// Session class
	class Session
	{
	public:
	
		// Constructor
		Session(std::string id, std::string webm_path, std::string json_path, int frame_limit = -1)
			: _id(id), _webm_path(webm_path), _json_path(json_path), _frame_limit(frame_limit) {}
		
		// Getter
		std::string get_id() const { return _id; }
		std::string get_webm_path() const { return _webm_path; }
		std::string get_json_path() const { return _json_path; }
		int get_frame_limit() const { return _frame_limit; }
		
	private:
	
		// Private copy constructor
		Session(const Session&) = delete;

		// Private assignment constructor
		Session& operator=(const Session&) = delete;

		// Members
		std::string _id = ""; // some id (useful for logging purposes)
		std::string _webm_path = ""; // screencast
		std::string _json_path = ""; // datacast
		int _frame_limit = -1; // limits amount of considered frames. -1 if there is no limit
	};
	
	// Typedefs
	typedef std::vector<std::shared_ptr<Session> > Sessions;
	typedef const std::vector<std::shared_ptr<const Session> > Sessions_const;
}
