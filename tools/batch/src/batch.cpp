/*
 * The MIT License (MIT)
 * 
 * Copyright (c) 2018 Sylko Olzscher 
 * 
 */ 


#include "batch.h"
#include "../../src/driver.h"

#include <cyng/json.h>
#include <cyng/factory/factory.hpp>
#include <cyng/dom/reader.h>
#include <cyng/value_cast.hpp>
#include <cyng/set_cast.h>
#include <cyng/parser/chrono_parser.h>
#include <cyng/io/serializer.h>
#include <cyng/io/io_chrono.hpp>
#include <cyng/factory/set_factory.h>

#include <boost/filesystem.hpp>
#include <boost/range/iterator_range.hpp>
#include <boost/range/adaptor/reversed.hpp>
#include <boost/algorithm/string.hpp>
#include <iostream>

namespace cyng
{
	namespace docscript
	{
	
		batch::batch(std::vector< std::string >const& inc
			, int verbose)
		: includes_(inc.begin(), inc.end())
			, verbose_(verbose)
		{}
		
		batch::~batch()
		{}

		int batch::run(boost::filesystem::path const& inp
			, boost::filesystem::path const& out
			, bool gen_robot
			, bool gen_sitemap)
		{
			if (boost::filesystem::is_directory(inp)) {

				for (auto const& entry : boost::make_iterator_range(boost::filesystem::directory_iterator(inp), {})) {

					process_file(entry, out);
				}

				//
				//	generate index file
				//
				generate_index(out, gen_robot, gen_sitemap);

				return EXIT_SUCCESS;
			}			

			//
			//	not a directory
			//
			std::cerr << "***error: " << inp << " is not a directory" << std::endl;
			return EXIT_FAILURE;
		}

		void batch::process_file(boost::filesystem::path const& inp
			, boost::filesystem::path const& out)
		{
			//
			//	exclude all files without the extension ".docscript"
			//
			if (inp.has_extension()) {

				if (boost::algorithm::equals(".docscript", inp.extension().string())) {

					//
					//	generate some temporary file names for intermediate files
					//
					boost::filesystem::path tmp = boost::filesystem::temp_directory_path() / (boost::filesystem::path(inp).filename().string() + ".bin");

					//
					//	Construct driver instance
					//
					driver d(includes_, verbose_);

					//
					//	output file
					//
					boost::filesystem::path out_file = out / inp.filename();
					out_file.replace_extension(".html");

					if (verbose_ > 0) {
						std::cout << "***info: processing file " 
							<< inp 
							<< " ==> "
							<< out_file
							<< std::endl;
					}

					//
					//	Start driver with the main/input file
					//
					d.run(boost::filesystem::path(inp).filename()
						, tmp
						, cyng::docscript::verify_extension(out_file, "html")
						, true	//	only HTML body
						, true);	//	generate meta data

				}
			}
		}

		void batch::generate_index(boost::filesystem::path const& out
			, bool gen_robot
			, bool gen_sitemap)
		{
			//
			//	chronological order
			//
			std::map<std::chrono::system_clock::time_point, tuple_t> meta_map;

			if (boost::filesystem::is_directory(out)) {

				for (auto const& entry : boost::make_iterator_range(boost::filesystem::directory_iterator(out), {})) {

					if (boost::algorithm::equals(".json", entry.path().extension().string())) {

						//
						//	parse JSON file
						//
						const auto obj = read_meta_data(entry);
						if (!obj) {
							std::cerr << "***error: "
								<< entry
								<< " contains no meta data"
								<< std::endl;
						}
						else {

							auto const reader = make_reader(obj);
							auto const str = value_cast<std::string>(reader.get("released"), "");
							//std::pair<std::chrono::system_clock::time_point, bool > 
							auto const released = parse_db_timestamp(str);
							if (released.second) {
								tuple_t tmp;
								meta_map.emplace(released.first, value_cast(obj, tmp));
							}
							else {

								//
								// try "last-write-time":
								//
								auto const str = value_cast<std::string>(reader.get("last-write-time"), "");
								//std::pair<std::chrono::system_clock::time_point, bool > 
								auto const released = parse_db_timestamp(str);
								if (released.second) {
									tuple_t tmp;
									meta_map.emplace(released.first, value_cast(obj, tmp));
								}
								else {
									std::cerr << "***error: "
										<< entry
										<< " contains neither \"released\" nor \"last-write-time\" tag"
										<< std::endl;

								}
							}
						}
					}
				}

				//
				//	generate index page
				//
				generate_index_page(out, meta_map);

				//
				//	generate index map
				//
				generate_index_map(out, meta_map);

				if (gen_robot) {

					//
					//	generate robots.txt
					//
					generate_robots_txt(out, meta_map, gen_sitemap);
				}

				if (gen_sitemap) {

					//
					//	generate sitemap
					//
					generate_sitemap(out, meta_map);
				}
			}
			else {

				std::cerr
					<< "***error: "
					<< out
					<< " is not a directory"
					<< std::endl;
			}
		}

		void batch::generate_index_page(boost::filesystem::path const& out
			, std::map<std::chrono::system_clock::time_point, tuple_t> const& meta_map)
		{
			boost::filesystem::path p = out / "index.html";

			std::ofstream ofs(p.string(), std::ios::out | std::ios::trunc);
			if (!ofs.is_open())
			{ 
				std::cerr
					<< "***error: cannot open index page "
					<< p
					<< std::endl;

			}
			else
			{
				ofs
					<< "<div class=\"docscript\">"
					<< std::endl
					;

				//
				//	reverse iterate over all meta data
				//
				for (auto const& meta : boost::adaptors::reverse(meta_map)) {

					auto const map = to_param_map(meta.second);

					if (verbose_ > 9) {
						for (auto const& m : map) {
							std::cout << "***info: "
								<< m.first
								<< " := "
								<< cyng::io::to_str(m.second)
								<< std::endl;
						}
					}

					auto const pos_title = map.find("title");
					auto const pos_file_name = map.find("file-name");
					auto const pos_slug = map.find("slug");
					auto const pos_entropy = map.find("text-entropy");	// double
					auto const pos_symbols = map.find("input-symbols");	//	size_t

					if (pos_title != map.end() 
						&& pos_file_name != map.end()
						&& pos_slug != map.end()
						&& pos_entropy != map.end()
						&& pos_symbols != map.end()) {

						//
						//	assume that the path has depth of one
						//
						auto const blog_path = out.filename().string();

						//	<a href="#" title="4860 characters" onclick="load_page(&quot;test.html&quot;);">docScript Test</a>

						ofs
							<< "\t<div>"
							<< std::endl
							<< "\t\t<a href=\"/"
							<< blog_path
							<< "/posts?slug="
							<< cyng::io::to_str(pos_slug->second)
							<< "\" onclick=\"load_page('"
							<< cyng::io::to_str(pos_file_name->second)
							//	don't follow href
							<< "'); return false;\" title=\""
							<< cyng::io::to_str(pos_symbols->second)
							<< ' '
							<< "input symbols with an entropy of "
							<< cyng::io::to_str(pos_entropy->second)
							<< "\">"
							<< cyng::io::to_str(pos_title->second)
							<< "</a>"
							<< std::endl
							<< "\t\t<p class=\"docscript-timestamp\">"
							<< date_to_str(meta.first)
							<< "</p>"
							<< std::endl
							<< "\t</div>"
							<< std::endl
							;
					}
				}

				ofs
					<< "</div>"
					<< std::endl
					;

			}
		}

		void batch::generate_index_map(boost::filesystem::path const& out
			, std::map<std::chrono::system_clock::time_point, tuple_t> const& meta_map)
		{
			boost::filesystem::path p = out / "index.json";

			std::ofstream ofs(p.string(), std::ios::out | std::ios::trunc);
			if (!ofs.is_open())
			{
				std::cerr
					<< "***error: cannot open index map "
					<< p
					<< std::endl;

			}
			else
			{
				//
				//	data vector of all available posts
				//
				tuple_t data;

				//
				//	reverse iterate over all meta data
				//
				std::size_t index{ 0 };
				for (auto const& meta : boost::adaptors::reverse(meta_map)) {

					auto const map = to_param_map(meta.second);


					auto const pos_title = map.find("title");
					auto const pos_file_name = map.find("file-name");
					auto const pos_slug = map.find("slug");
					auto const pos_symbols = map.find("input-symbols");	//	size_t

					if (pos_title != map.end()
						&& pos_file_name != map.end()
						&& pos_symbols != map.end()
						&& pos_slug != map.end()) {

						auto map = param_map_factory("title", pos_title->second)
							("file-name", pos_file_name->second)
							("index", index)
							("size", pos_symbols->second)
							();
						data.push_back(set_factory(cyng::io::to_str(pos_slug->second), map));
						++index;
					}
				}

				//
				//	write as JSON file
				//
				cyng::json::write(ofs, cyng::make_object(data));

			}

		}

		void batch::generate_robots_txt(boost::filesystem::path const& out
			, std::map<std::chrono::system_clock::time_point, tuple_t> const&
			, bool gen_sitemap)
		{
			boost::filesystem::path p = out / "robots.txt";

			std::ofstream ofs(p.string(), std::ios::out | std::ios::trunc);
			if (!ofs.is_open())
			{
				std::cerr
					<< "***error: cannot robot file"
					<< p
					<< std::endl;

			}
			else {

				//
				//	ToDo: generate robots.txt
				//

				//
				//	assume that the path has depth of one
				//
				auto const blog_path = out.filename().string();

				ofs
					<< "User-agent: *"
					<< std::endl
					<< "Allow: /"
					<< blog_path
					<< std::endl
					;

				if (gen_sitemap) {

					ofs
						<< "Sitemap: sitemap.xml"
						<< std::endl
						;
				}

			}
		}

		void batch::generate_sitemap(boost::filesystem::path const& out
			, std::map<std::chrono::system_clock::time_point, tuple_t> const&)
		{
			boost::filesystem::path p = out / "sitemap.xml";

			std::ofstream ofs(p.string(), std::ios::out | std::ios::trunc);
			if (!ofs.is_open())
			{
				std::cerr
					<< "***error: cannot open sitemap "
					<< p
					<< std::endl;

			}
			else {

				//
				//	ToDo: generate sitemap
				//
			}
		}

		object batch::read_meta_data(boost::filesystem::path const& p)
		{

			//
			//	exclude all files without the extension ".json"
			//
			if (p.has_extension()) {

				if (boost::algorithm::equals(".json", p.extension().string())) {

					if (verbose_ > 1) {
						std::cout << "***info: read meta data "
							<< p
							<< std::endl;
					}

					return cyng::json::read_file(p.string());

				}
			}

			return make_object();
		}
	}	
}
