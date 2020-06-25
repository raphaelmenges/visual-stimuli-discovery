cmake_minimum_required(VERSION 2.8)

# Name the project by folder
get_filename_component(TARGET_NAME ${CMAKE_CURRENT_SOURCE_DIR} NAME)
string(REPLACE " " "_" TARGET_NAME ${TARGET_NAME})

# Code
file(GLOB_RECURSE LOCAL_CODE *.hpp *.cpp)
include_directories(${CMAKE_CURRENT_SOURCE_DIR})

# Combine local code with parent scoped code
set(CODE ${LOCAL_CODE} ${INTERNAL_LIBRARY_CODE})

# Create executable
add_executable(${TARGET_NAME} ${CODE})

# Set label of project
set_property(TARGET ${TARGET_NAME} PROPERTY PROJECT_LABEL gm_exe_${TARGET_NAME})

# Linking
target_link_libraries(${TARGET_NAME} ${INTERNAL_LIBRARIES} ${EXTERNAL_LIBRARIES} ${FINAL_LIBRARIES})

# Filtering for Visual Studio of local code
if(MSVC)

	foreach(f ${LOCAL_CODE})
		
		# Relative path from folder to file
		file(RELATIVE_PATH SRCGR "${CMAKE_CURRENT_SOURCE_DIR}" ${f})
		set(SRCGR "src/${SRCGR}")
		
		# Extract the folder, i.e., remove the filename part
		string(REGEX REPLACE "(.*)(/[^/]*)$" "\\1" SRCGR ${SRCGR})

		# Source_group expects \\ (double antislash), not / (slash)
		string(REPLACE / \\ SRCGR ${SRCGR})
		source_group("${SRCGR}" FILES ${f})
		
	endforeach()

endif(MSVC)

# Filtering for Visual Studio of library code
if(MSVC)

	foreach(f ${INTERNAL_LIBRARY_CODE})
		
		# Relative path from folder to file
		file(RELATIVE_PATH SRCGR "${CMAKE_SOURCE_DIR}/src/lib" ${f})
		set(SRCGR "lib/${SRCGR}")
		
		# Extract the folder, i.e., remove the filename part
		string(REGEX REPLACE "(.*)(/[^/]*)$" "\\1" SRCGR ${SRCGR})

		# Source_group expects \\ (double antislash), not / (slash)
		string(REPLACE / \\ SRCGR ${SRCGR})
		source_group("${SRCGR}" FILES ${f})
		
	endforeach()

endif(MSVC)

# Get rid of unnecessary warnings
if(MSVC)
	set_target_properties(${TARGET_NAME} PROPERTIES LINK_FLAGS "/ignore:4099")
	set_target_properties(${TARGET_NAME} PROPERTIES LINK_FLAGS "/ignore:4503")
endif(MSVC)

# Deployment
if(${DEPLOY})
	add_custom_command(TARGET ${TARGET_NAME} 
					   POST_BUILD
					   COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:${TARGET_NAME}> ${DEPLOY_PATH})
endif()