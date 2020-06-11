/*
 * The MIT License (MIT)
 * 
 * Copyright (c) 2019 Sylko Olzscher 
 * 
 */ 

#include <boost/program_options.hpp>
#include <cyng/compatibility/file_system.hpp>
#include <boost/config.hpp>
#include <boost/predef.h>
#include <fstream>
#include <iostream>
#include <DOCC_project_info.h>
#include "../../src/driver.h"
#if BOOST_OS_WINDOWS
#include <windows.h>
#endif

//#include <html/node.hpp>

/**
 * The first version generates HTML only. Future version generate hopefully SVG and PDF too.
 *
 * Start with
 * @code
 * build/docc -V9 ~/projects/docc/src/main/examples/readme
 * build/docc -V9 C:\projects\docc\src\main\examples\readme
 * @endcode
 */

int main(int argc, char* argv[]) {

	try
	{
		const cyng::filesystem::path cwd = cyng::filesystem::current_path();

		std::string config_file = "docc_" + std::string(DOCC_SUFFIX) + ".cfg";
		std::string inp_file = "main.docscript";
		std::string out_file = (cwd / "out.html").string();

		//
		//	generic options
		//
		boost::program_options::options_description generic("Generic options");
		generic.add_options()

			("help,h", "print usage message")
			("version,v", "print version string")
			("build,b", "last built timestamp and platform")
			("config,C", boost::program_options::value<std::string>(&config_file)->default_value(config_file), "configuration file")

			;

		//
		//	all compiler options
		//
		boost::program_options::options_description compiler("compiler");
		compiler.add_options()

			("source,S", boost::program_options::value(&inp_file)->default_value(inp_file), "main source file")
			("output,O", boost::program_options::value(&out_file)->default_value(out_file), "output file")
			("include-path,I", boost::program_options::value< std::vector<std::string> >()->default_value(std::vector<std::string>(1, cwd.string()), cwd.string()), "include path")
			//	verbose level
			("verbose,V", boost::program_options::value<int>()->default_value(0)->implicit_value(1), "verbose level")
			;

		boost::program_options::options_description gen("generator");
		gen.add_options()
			("generator.body", boost::program_options::bool_switch()->default_value(false), "generate (HTML) body only")
			("generator.meta", boost::program_options::bool_switch()->default_value(true), "generate a JSON file with meta data")
			("generator.index", boost::program_options::bool_switch()->default_value(true), "generate an index file \"index.json\"")
			("generator.type,T", boost::program_options::value<std::string>()->default_value("report"), "og:type (article/report)")
			;

		//
		//	all you can grab from the command line
		//
		boost::program_options::options_description cmdline_options;
		cmdline_options.add(generic).add(compiler).add(gen);

		//
		//	positional arguments
		//
		boost::program_options::positional_options_description p;
		p.add("source", -1);

		boost::program_options::variables_map vm;
		boost::program_options::store(boost::program_options::command_line_parser(argc, argv).options(cmdline_options).positional(p).run(), vm);
		boost::program_options::notify(vm);

		if (vm.count("help"))
		{
			std::cout
				<< cmdline_options
				<< std::endl
				;
			return EXIT_SUCCESS;
		}

		if (vm.count("version"))
		{
			std::cout
				<< "docScript compiler v"
				<< DOCC_VERSION
				<< std::endl
				;
			return EXIT_SUCCESS;
		}

		if (vm.count("build"))
		{
			std::cout
				<< "last built at : "
				<< DOCC_BUILD_DATE
				<< std::endl
				<< "Platform      : "
				<< DOCC_PLATFORM
				<< std::endl
				<< "Compiler      : "
				<< BOOST_COMPILER
				<< std::endl
				<< "StdLib        : "
				<< BOOST_STDLIB
				<< std::endl

#if defined(BOOST_LIBSTDCXX_VERSION)
				<< "StdLib++      : "
				<< BOOST_LIBSTDCXX_VERSION
				<< std::endl
#endif

				<< "BOOSTLib      : "
				<< BOOST_LIB_VERSION
				<< " ("
				<< BOOST_VERSION
				<< ")"
				<< std::endl

				<< "shared mutex  : "
#if defined(__CPP_SUPPORT_N4508)
				<< "yes"
#else
				<< "no"
#endif
				<< std::endl
				<< "uint8_t type  : "
#if defined(__CPP_SUPPORT_P0482R6)
				<< "yes"
#else
				<< "no"
#endif
				<< std::endl
				<< std::endl
				;
			return EXIT_SUCCESS;
		}


		std::ifstream ifs(config_file);
		if (!ifs)
		{
			std::cout
				<< "***info: config file: "
				<< config_file
				<< " not found"
				<< std::endl
				;
		}
		else
		{
			//
			//	options available from config file
			//
			boost::program_options::options_description file_options;
			file_options.add(compiler).add(gen);

			boost::program_options::store(boost::program_options::parse_config_file(ifs, file_options), vm);
			boost::program_options::notify(vm);
		}

		//
		//	generate some temporary file names for intermediate files
		//
		cyng::filesystem::path tmp = cyng::filesystem::temp_directory_path() / (cyng::filesystem::path(inp_file).filename().string() + ".bin");

		const int verbose = vm["verbose"].as< int >();
		if (verbose > 0)
		{
			std::cout
				<< "***info: verbose level: "
				<< verbose
				<< std::endl
				;

		}

		//
		//	read specified include paths
		//
		auto inc_paths = vm["include-path"].as< std::vector<std::string>>();

		//
		//	Add the path of the input file as include path, if it is not already specified
		//
		auto path = cyng::filesystem::path(inp_file).parent_path();
		auto pos = std::find(inc_paths.begin(), inc_paths.end(), path);
		if (pos == inc_paths.end()) {
			inc_paths.push_back(path.string());
		}

		//
		//	last entry is empty
		//
		inc_paths.push_back("");
		if (verbose > 0)
		{
			std::cout
				<< "Include paths are: "
				<< std::endl
				;
			std::copy(inc_paths.begin(), inc_paths.end(), std::ostream_iterator<std::string>(std::cout, "\n"));
		}

#if BOOST_OS_WINDOWS
		//
		//	set console outpt code page to UTF-8
		//	requires a TrueType font like Lucida 
		//
		if (::SetConsoleOutputCP(65001) == 0)
			//if (::SetConsoleOutputCP(12000) == 0)		//	UTF-32	
		{
			std::cerr
				<< "Cannot set console code page"
				<< std::endl
				;

		}
#endif
		//
		//	Construct driver instance
		//
  		docscript::driver d(inc_paths, verbose);

		//
		//	Start driver with the main/input file
		//
 		return d.run(cyng::filesystem::path(inp_file).filename()
 			, tmp
			, out_file
			, vm["generator.body"].as< bool >()
			, vm["generator.meta"].as< bool >()
			, vm["generator.index"].as< bool >()
			, vm["generator.type"].as< std::string >());

	}
	catch (std::exception& e)
	{
		std::cerr
			<< e.what()
			<< std::endl
			;
	}

	return EXIT_FAILURE;
		
}
