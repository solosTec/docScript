/*
 * The MIT License (MIT)
 * 
 * Copyright (c) 2017 Sylko Olzscher 
 * 
 */ 

#ifndef NODE_HTTP_CONTROLLER_H
#define NODE_HTTP_CONTROLLER_H

#include <string>
#include <cstdint>

namespace plog
{
	class controller 
	{
	public:
		/**
		 * @param pool_size thread pool size 
		 * @param json_path path to JSON configuration file 
		 */
		controller(unsigned int pool_size, std::string const& json_path);
		
		/**
		 * Start controller loop.
		 * The controller loop controls startup and shutdown of the server.
		 * 
		 * @return EXIT_FAILURE in case of an error, otherwise EXIT_SUCCESS.
		 */
		int run(bool console);
		
		/**
		 * create configuration file with default values.
		 */
		int create_config(std::string const&) const;
		
	private:
		int create_xml_config() const;
		int create_json_config() const;
		
	private:
		const unsigned int pool_size_;
		const std::string json_path_;
	};
}

#endif
