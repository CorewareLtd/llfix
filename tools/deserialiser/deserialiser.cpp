/*
MIT License

Copyright (c) 2026 Coreware Limited

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/
#include <cstddef>
#include <string>
#include <sstream>
#include <vector>
#include <iostream>

#include <llfix/core/os/vdso.h>
#include <llfix/core/utilities/filesystem_utilities.h>
#include <llfix/core/utilities/serialisation_utilities.h>

#include "fix_deserialiser.h"

#include <cxxopts.hpp>

#define VERSION "1.0.0"

using namespace std;

int main(int argc, char* argv[])
{
    std::string serialisation_path;
    std::string output_file;
    bool exclude_timestamps = false;
    bool human_readible_tag_names = false;
    bool no_processing = false;
    std::string tag_definitions_csv_file = "tags.csv";

    cxxopts::Options options("llfix Deserialiser");

    try
    {
        options.add_options()
            ("i,input_serialisation_path", "Input serialisation path", cxxopts::value<std::string>()->default_value(""))
            ("o,output_file", "Output file", cxxopts::value<std::string>()->default_value(""))
            ("c,csv_file_tag_definitions", "Tag definitions CSV file", cxxopts::value<std::string>()->default_value("tags.csv"))
            ("e,exclude_timestamps", "Exclude timestamps", cxxopts::value<bool>()->default_value("false"))
            ("t,textual_tags", "Tag names instead of numbers", cxxopts::value<bool>()->default_value("false"))
            ("n,no_processing", "Disables validating decoded messages and replacing delimiters with pipes. Will also turn textual tags off", cxxopts::value<bool>()->default_value("false"))
            ("v,version", "Print version")
            ("h,help", "Print usage");

        auto result = options.parse(argc, argv);
        ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
        if (result.count("help"))
        {
            std::cout << options.help() << "\n";
            return 0;
        }

        if (result.count("version"))
        {
            std::cout << "Version: " << VERSION << "\n";
            return 0;
        }

        std::vector<std::string> missing;

        if (!result.count("input_serialisation_path"))  missing.push_back("-i");
        if (!result.count("output_file"))               missing.push_back("-o");

        if (!missing.empty())
        {
            std::cerr << "Missing required option(s): ";

            for (auto& opt : missing)
                std::cerr << opt << " ";

            std::cerr << "\n\n" << options.help() << "\n";

            return 1;
        }
        ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
        serialisation_path = result["input_serialisation_path"].as<std::string>();
        output_file = result["output_file"].as<std::string>();
        exclude_timestamps = result["exclude_timestamps"].as<bool>();
        human_readible_tag_names = result["textual_tags"].as<bool>();
        no_processing = result["no_processing"].as<bool>();
        tag_definitions_csv_file = result["csv_file_tag_definitions"].as<std::string>();
    }
    catch (const cxxopts::exceptions::exception& e)
    {
        std::cerr << "Option parsing error: " << e.what() << "\n";
        return 2;
    }

    if (llfix::FileSystemUtilities::does_path_exist(serialisation_path) == false)
    {
        std::cerr << serialisation_path << " does not exist\n";
        return 3;
    }

    FixDeserialiser deserialiser;
    deserialiser.set_dont_process_flag(no_processing);

    if (human_readible_tag_names)
    {
        if(deserialiser.set_fields_names_from(tag_definitions_csv_file) == false)
        {
            std::cout << "WARNING : Failed to load tags definition CSV file " << tag_definitions_csv_file << "\n";
        }
    }

    //////////////////////////////////////////////////////////////////////////////////////////////////////////
    // INPUT
    auto files = llfix::SerialisationUtilities::get_all_serialised_files(serialisation_path);

    if (files.size() == 0)
    {
        std::cerr << "No serialised files found under " << serialisation_path << "\n";
        return 4;
    }

    //////////////////////////////////////////////////////////////////////////////////////////////////////////
    // DESERIALIATION
    for (const auto& file_path : files)
    {
        try
        {
            llfix::SerialisationUtilities::deserialise_file(deserialiser, file_path);
        }
        catch (...)
        {
            std::cerr << "Failed to deserialise " << file_path << ". Please make sure input path has valid llfix bin files.\n";
            return 5;
        }
    }

    //////////////////////////////////////////////////////////////////////////////////////////////////////////
    // OUTPUT
    std::stringstream target;

    deserialiser.sort();
    auto entries = deserialiser.get_entries();

    for (const auto& entry : *entries)
    {
        if(exclude_timestamps == false)
        {
            target << "----------------------------------------\n";
            target << "TIMESTAMP=" << llfix::VDSO::convert_nanoseconds_since_epoch_to_time_string(entry.nanosecs_since_epoch) << "\n";
            target << "Success=" << static_cast<int>(entry.success) << "\n";
        }

        target << entry.message;

        target << "\n";
    }

    llfix::FileSystemUtilities::delete_file_if_exists(output_file);

    if (llfix::FileSystemUtilities::append_text_to_file(output_file, target.str()))
    {
        std::cout << "Successfully created " << output_file << "\n";
    }
    else
    {
        std::cerr << "An error occured while creating " << output_file << "\n";
        return 6;
    }

    //////////////////////////////////////////////////////////////////////////////////////////////////////////
    // WARNINGS
    auto warnings = deserialiser.get_warnings();

    if(!warnings.empty())
    {
        std::string warnings_file_content;
        for(const auto& warning_message : warnings)
        {
            warnings_file_content += warning_message + "\n";
        }

        std::string warning_file_name = "warnings.txt";
        llfix::FileSystemUtilities::delete_file_if_exists(warning_file_name);
        llfix::FileSystemUtilities::append_text_to_file(warning_file_name, warnings_file_content.c_str());
        std::cout << "Please check the deserialiser warnings in " << warning_file_name << "\n";
    }

    return 0;
}