///////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// DEFINES
#ifndef LLFIX_UNIT_TEST
#define LLFIX_UNIT_TEST
#endif

#ifndef LLFIX_ENABLE_DICTIONARY
#define LLFIX_ENABLE_DICTIONARY
#endif
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#include <llfix/common.h>
#include "../unit_test.h"

#include <llfix/fix_dictionary_loader.h>
#include <llfix/core/utilities/filesystem_utilities.h>
#include <llfix/core/compiler/unused.h>
#include <vector>
#include <unordered_map>
#include <algorithm>

#include <cstdint>
#include <iostream>
using namespace std;

using namespace llfix;

int main(int argc, char* argv[])
{
    UnitTest unit_test;

    std::string quickfix_schemas_root_path = "../dictionaries/";

    std::unordered_map<uint32_t, std::string> all_fields;

    //////////////////////////////////////////////////////////////////
    // FIX 4.0
    {
        std::string target_file = quickfix_schemas_root_path + "FIX40.xml";
        FixDictionaryLoader loader;

        if (loader.load_from(target_file) == false)
        {
            std::cout << "Could not open " << target_file << "\n";

            auto errors = loader.errors();

            for (const auto& error : *errors)
            {
                std::cout << "Parser error : " << error << "\n";
            }

            return -1;
        }

        unit_test.test_equals(loader.errors()->size(), 0, "FIX4.0", "loader errors");
        unit_test.test_equals(loader.warnings()->size(), 0, "FIX4.0", "loader warnings");

        FixDictionary* dict = loader.get_dictionary();
        unit_test.test_equals(dict->major(), 4, "FIX4.0", "major");
        unit_test.test_equals(dict->minor(), 0, "FIX4.0", "minor");
        unit_test.test_equals(dict->service_pack(), 0, "FIX4.0", "servicepack");

        std::string begin_string;
        loader.get_dictionary()->get_begin_string(begin_string);
        unit_test.test_equals(begin_string, "FIX.4.0", "FIX4.0", "begin_string");
        unit_test.test_equals(loader.get_dictionary()->get_namespace_name(), "FIX40", "FIX4.0", "namespace");

        auto header = dict->header();
        unit_test.test_equals(header->fields.size(), 18, "FIX4.0", "header fields");
        unit_test.test_equals(header->groups.size(), 0, "FIX4.0", "header groups");
        unit_test.test_equals(header->components.size(), 0, "FIX4.0", "header components");

        auto trailer = dict->trailer();
        unit_test.test_equals(trailer->fields.size(), 3, "FIX4.0", "trailer fields");
        unit_test.test_equals(trailer->groups.size(), 0, "FIX4.0", "trailer groups");
        unit_test.test_equals(trailer->components.size(), 0, "FIX4.0", "trailer components");

        unit_test.test_equals(dict->messages()->size(), 27, "FIX4.0", "messages");
        unit_test.test_equals(dict->fields()->size(), 138, "FIX4.0", "fields");
        unit_test.test_equals(dict->components()->size(), 0, "FIX4.0", "components");

        unit_test.test_equals((*dict->fields())["MsgType"].values.size(), 27, "FIX4.0", "message type values");
        unit_test.test_equals((*dict->fields())["MsgType"].value_descriptions.size(), 27, "FIX4.0", "message type value descriptions");

        auto fields = *dict->fields();
        for (const auto& field : fields)
        {
            all_fields[field.second.tag] = field.second.name;
        }
    }

    //////////////////////////////////////////////////////////////////
    // FIX 4.1
    {
        std::string target_file = quickfix_schemas_root_path + "FIX41.xml";
        FixDictionaryLoader loader;

        if (loader.load_from(target_file) == false)
        {
            std::cout << "Could not open " << target_file << "\n";

            auto errors = loader.errors();

            for (const auto& error : *errors)
            {
                std::cout << "Parser error : " << error << "\n";
            }

            return -1;
        }

        unit_test.test_equals(loader.errors()->size(), 0, "FIX4.1", "loader errors");
        unit_test.test_equals(loader.warnings()->size(), 0, "FIX4.1", "loader warnings");

        FixDictionary* dict = loader.get_dictionary();
        unit_test.test_equals(dict->major(), 4, "FIX4.1", "major");
        unit_test.test_equals(dict->minor(), 1, "FIX4.1", "minor");
        unit_test.test_equals(dict->service_pack(), 0, "FIX4.1", "servicepack");

        std::string begin_string;
        loader.get_dictionary()->get_begin_string(begin_string);
        unit_test.test_equals(begin_string, "FIX.4.1", "FIX4.1", "begin_string");
        unit_test.test_equals(loader.get_dictionary()->get_namespace_name(), "FIX41", "FIX4.1", "namespace");

        auto header = dict->header();
        unit_test.test_equals(header->fields.size(), 22, "FIX4.1", "header fields");
        unit_test.test_equals(header->groups.size(), 0, "FIX4.1", "header groups");
        unit_test.test_equals(header->components.size(), 0, "FIX4.1", "header components");

        auto trailer = dict->trailer();
        unit_test.test_equals(trailer->fields.size(), 3, "FIX4.1", "trailer fields");
        unit_test.test_equals(trailer->groups.size(), 0, "FIX4.1", "trailer groups");
        unit_test.test_equals(trailer->components.size(), 0, "FIX4.1", "trailer components");

        unit_test.test_equals(dict->messages()->size(), 28, "FIX4.1", "messages");
        unit_test.test_equals(dict->fields()->size(), 206, "FIX4.1", "fields");
        unit_test.test_equals(dict->components()->size(), 0, "FIX4.1", "components");

        unit_test.test_equals((*dict->fields())["MsgType"].values.size(), 28, "FIX4.1", "message type values");
        unit_test.test_equals((*dict->fields())["MsgType"].value_descriptions.size(), 28, "FIX4.1", "message type value descriptions");

        GroupField no_allocs;
        bool found_no_allocs = false;

        for (auto& group : (*dict->messages())["J"].groups)
        {
            if (group.name == "NoAllocs")
            {
                no_allocs = group;
                found_no_allocs = true;
                break;
            }
        }

        unit_test.test_equals(found_no_allocs, true, "FIX4.1", "Loading NoAllocs group from J message");

        auto fields = *dict->fields();
        for (const auto& field : fields)
        {
            all_fields[field.second.tag] = field.second.name;
        }
    }

    //////////////////////////////////////////////////////////////////
    // FIX 4.2
    {
        std::string target_file = quickfix_schemas_root_path + "FIX42.xml";
        FixDictionaryLoader loader;

        if (loader.load_from(target_file) == false)
        {
            std::cout << "Could not open " << target_file << "\n";

            auto errors = loader.errors();

            for (const auto& error : *errors)
            {
                std::cout << "Parser error : " << error << "\n";
            }

            return -1;
        }

        unit_test.test_equals(loader.errors()->size(), 0, "FIX4.2", "loader errors");
        unit_test.test_equals(loader.warnings()->size(), 0, "FIX4.2", "loader warnings");

        FixDictionary* dict = loader.get_dictionary();
        unit_test.test_equals(dict->major(), 4, "FIX4.2", "major");
        unit_test.test_equals(dict->minor(), 2, "FIX4.2", "minor");
        unit_test.test_equals(dict->service_pack(), 0, "FIX4.2", "servicepack");

        std::string begin_string;
        loader.get_dictionary()->get_begin_string(begin_string);
        unit_test.test_equals(begin_string, "FIX.4.2", "FIX4.2", "begin_string");
        unit_test.test_equals(loader.get_dictionary()->get_namespace_name(), "FIX42", "FIX4.2", "namespace");

        auto header = dict->header();
        unit_test.test_equals(header->fields.size(), 27, "FIX4.2", "header fields");
        unit_test.test_equals(header->groups.size(), 0, "FIX4.2", "header groups");
        unit_test.test_equals(header->components.size(), 0, "FIX4.2", "header components");

        auto trailer = dict->trailer();
        unit_test.test_equals(trailer->fields.size(), 3, "FIX4.2", "trailer fields");
        unit_test.test_equals(trailer->groups.size(), 0, "FIX4.2", "trailer groups");
        unit_test.test_equals(trailer->components.size(), 0, "FIX4.2", "trailer components");

        unit_test.test_equals(dict->messages()->size(), 46, "FIX4.2", "messages");
        unit_test.test_equals(dict->fields()->size(), 405, "FIX4.2", "fields");
        unit_test.test_equals(dict->components()->size(), 0, "FIX4.2", "components");

        unit_test.test_equals((*dict->fields())["MsgType"].values.size(), 46, "FIX4.2", "message type values");
        unit_test.test_equals((*dict->fields())["MsgType"].value_descriptions.size(), 46, "FIX4.2", "message type value descriptions");

        auto fields = *dict->fields();
        for (const auto& field : fields)
        {
            all_fields[field.second.tag] = field.second.name;
        }
    }

    //////////////////////////////////////////////////////////////////
    // FIX 4.3
    {
        std::string target_file = quickfix_schemas_root_path + "FIX43.xml";
        FixDictionaryLoader loader;

        if (loader.load_from(target_file) == false)
        {
            std::cout << "Could not open " << target_file << "\n";

            auto errors = loader.errors();

            for (const auto& error : *errors)
            {
                std::cout << "Parser error : " << error << "\n";
            }

            return -1;
        }

        unit_test.test_equals(loader.errors()->size(), 0, "FIX4.3", "loader errors");
        unit_test.test_equals(loader.warnings()->size(), 0, "FIX4.3", "loader warnings");

        FixDictionary* dict = loader.get_dictionary();
        unit_test.test_equals(dict->major(), 4, "FIX4.3", "major");
        unit_test.test_equals(dict->minor(), 3, "FIX4.3", "minor");
        unit_test.test_equals(dict->service_pack(), 0, "FIX4.3", "servicepack");

        std::string begin_string;
        loader.get_dictionary()->get_begin_string(begin_string);
        unit_test.test_equals(begin_string, "FIX.4.3", "FIX4.3", "begin_string");
        unit_test.test_equals(loader.get_dictionary()->get_namespace_name(), "FIX43", "FIX4.3", "namespace");

        auto header = dict->header();
        unit_test.test_equals(header->fields.size(), 27, "FIX4.3", "header fields");
        unit_test.test_equals(header->groups.size(), 1, "FIX4.3", "header groups");
        unit_test.test_equals(header->components.size(), 0, "FIX4.3", "header components");

        auto trailer = dict->trailer();
        unit_test.test_equals(trailer->fields.size(), 3, "FIX4.3", "trailer fields");
        unit_test.test_equals(trailer->groups.size(), 0, "FIX4.3", "trailer groups");
        unit_test.test_equals(trailer->components.size(), 0, "FIX4.3", "trailer components");

        unit_test.test_equals(dict->messages()->size(), 68, "FIX4.3", "messages");
        unit_test.test_equals(dict->fields()->size(), 635, "FIX4.3", "fields");
        unit_test.test_equals(dict->components()->size(), 10, "FIX4.3", "components");

        unit_test.test_equals((*dict->fields())["MsgType"].values.size(), 68, "FIX4.3", "message type values");
        unit_test.test_equals((*dict->fields())["MsgType"].value_descriptions.size(), 68, "FIX4.3", "message type value descriptions");

        auto fields = *dict->fields();
        for (const auto& field : fields)
        {
            all_fields[field.second.tag] = field.second.name;
        }
    }

    //////////////////////////////////////////////////////////////////
    // FIX 4.4
    {
        std::string target_file = quickfix_schemas_root_path + "FIX44.xml";
        FixDictionaryLoader loader;

        if (loader.load_from(target_file) == false)
        {
            std::cout << "Could not open " << target_file << "\n";

            auto errors = loader.errors();

            for (const auto& error : *errors)
            {
                std::cout << "Parser error : " << error << "\n";
            }

            return -1;
        }

        unit_test.test_equals(loader.errors()->size(), 0, "FIX4.4", "loader errors");
        unit_test.test_equals(loader.warnings()->size(), 0, "FIX4.4", "loader warnings");

        FixDictionary* dict = loader.get_dictionary();
        unit_test.test_equals(dict->major(), 4, "FIX4.4", "major");
        unit_test.test_equals(dict->minor(), 4, "FIX4.4", "minor");
        unit_test.test_equals(dict->service_pack(), 0, "FIX4.4", "servicepack");

        std::string begin_string;
        loader.get_dictionary()->get_begin_string(begin_string);
        unit_test.test_equals(begin_string, "FIX.4.4", "FIX4.4", "begin_string");
        unit_test.test_equals(loader.get_dictionary()->get_namespace_name(), "FIX44", "FIX4.4", "namespace");

        auto header = dict->header();
        unit_test.test_equals(header->fields.size(), 26, "FIX4.4", "header fields");
        unit_test.test_equals(header->groups.size(), 1, "FIX4.4", "header groups");
        unit_test.test_equals(header->components.size(), 0, "FIX4.4", "header components");

        auto trailer = dict->trailer();
        unit_test.test_equals(trailer->fields.size(), 3, "FIX4.4", "trailer fields");
        unit_test.test_equals(trailer->groups.size(), 0, "FIX4.4", "trailer groups");
        unit_test.test_equals(trailer->components.size(), 0, "FIX4.4", "trailer components");

        unit_test.test_equals(dict->messages()->size(), 93, "FIX4.4", "messages");
        unit_test.test_equals(dict->fields()->size(), 912, "FIX4.4", "fields");
        unit_test.test_equals(dict->components()->size(), 104, "FIX4.4", "components");

        unit_test.test_equals((*dict->fields())["MsgType"].values.size(), 93, "FIX4.4", "message type values");
        unit_test.test_equals((*dict->fields())["MsgType"].value_descriptions.size(), 93, "FIX4.4", "message type value descriptions");

        auto fields = *dict->fields();
        for (const auto& field : fields)
        {
            all_fields[field.second.tag] = field.second.name;
        }
    }

    //////////////////////////////////////////////////////////////////
    // FIX 5.0 & FIXT 1.1
    {
        std::string target_file = quickfix_schemas_root_path + "FIX50.xml";
        std::string target_transport_file = quickfix_schemas_root_path + "FIXT11.xml";

        FixDictionaryLoader loader;

        if (loader.load_from(target_transport_file, true) == false)
        {
            std::cout << "Could not open " << target_transport_file << "\n";
            return -1;
        }

        if (loader.load_from(target_file) == false)
        {
            std::cout << "Could not open " << target_file << "\n";
            return -1;
        }

        unit_test.test_equals(loader.errors()->size(), 0, "FIX5.0", "loader errors");
        unit_test.test_equals(loader.warnings()->size(), 0, "FIX5.0", "loader warnings");

        FixDictionary* dict = loader.get_dictionary();
        unit_test.test_equals(dict->major(), 5, "FIX5.0", "major");
        unit_test.test_equals(dict->minor(), 0, "FIX5.0", "minor");
        unit_test.test_equals(dict->service_pack(), 0, "FIX5.0", "servicepack");

        unit_test.test_equals(dict->transport_major(), 1, "FIX5.0", "transport major");
        unit_test.test_equals(dict->transport_minor(), 1, "FIX5.0", "transport minor");
        unit_test.test_equals(dict->transport_service_pack(), 0, "FIX5.0", "transport servicepack");

        std::string begin_string;
        loader.get_dictionary()->get_begin_string(begin_string);
        unit_test.test_equals(begin_string, "FIXT.1.1", "FIX5.0", "begin_string");
        unit_test.test_equals(loader.get_dictionary()->get_namespace_name(), "FIX50", "FIX5.0", "namespace");

        auto header = dict->header();
        unit_test.test_equals(header->fields.size(), 29, "FIX5.0", "header fields");
        unit_test.test_equals(header->groups.size(), 1, "FIX5.0", "header groups");
        unit_test.test_equals(header->components.size(), 0, "FIX5.0", "header components");

        auto trailer = dict->trailer();
        unit_test.test_equals(trailer->fields.size(), 3, "FIX5.0", "trailer fields");
        unit_test.test_equals(trailer->groups.size(), 0, "FIX5.0", "trailer groups");
        unit_test.test_equals(trailer->components.size(), 0, "FIX5.0", "trailer components");

        unit_test.test_equals(dict->messages()->size(), 101, "FIX5.0", "messages");
        unit_test.test_equals(dict->fields()->size(), 1100, "FIX5.0", "fields");
        unit_test.test_equals(dict->components()->size(), 121, "FIX5.0", "components");

        unit_test.test_equals((*dict->fields())["MsgType"].values.size(), 101, "FIX5.0", "message type values");
        unit_test.test_equals((*dict->fields())["MsgType"].value_descriptions.size(), 101, "FIX5.0", "message type value descriptions");

        auto fields = *dict->fields();
        for (const auto& field : fields)
        {
            all_fields[field.second.tag] = field.second.name;
        }
    }

    //////////////////////////////////////////////////////////////////
    // FIX 5.0 SP1 & FIXT 1.1
    {
        std::string target_file = quickfix_schemas_root_path + "FIX50SP1.xml";
        std::string target_transport_file = quickfix_schemas_root_path + "FIXT11.xml";

        FixDictionaryLoader loader;

        if (loader.load_from(target_transport_file, true) == false)
        {
            std::cout << "Could not open " << target_transport_file << "\n";
            return -1;
        }

        if (loader.load_from(target_file) == false)
        {
            std::cout << "Could not open " << target_file << "\n";
            return -1;
        }

        unit_test.test_equals(loader.errors()->size(), 0, "FIX5.0 SP1", "loader errors");
        unit_test.test_equals(loader.warnings()->size(), 0, "FIX5.0 SP1", "loader warnings");

        FixDictionary* dict = loader.get_dictionary();
        unit_test.test_equals(dict->major(), 5, "FIX5.0 SP1", "major");
        unit_test.test_equals(dict->minor(), 0, "FIX5.0 SP1", "minor");
        unit_test.test_equals(dict->service_pack(), 1, "FIX5.0 SP1", "servicepack");

        unit_test.test_equals(dict->transport_major(), 1, "FIX5.0 SP1", "transport major");
        unit_test.test_equals(dict->transport_minor(), 1, "FIX5.0 SP1", "transport minor");
        unit_test.test_equals(dict->transport_service_pack(), 0, "FIX5.0 SP1", "transport servicepack");

        std::string begin_string;
        loader.get_dictionary()->get_begin_string(begin_string);
        unit_test.test_equals(begin_string, "FIXT.1.1", "FIX5.0 SP1", "begin_string");
        unit_test.test_equals(loader.get_dictionary()->get_namespace_name(), "FIX50SP1", "FIX5.0 SP1", "namespace");

        auto header = dict->header();
        unit_test.test_equals(header->fields.size(), 29, "FIX5.0 SP1", "header fields");
        unit_test.test_equals(header->groups.size(), 1, "FIX5.0 SP1", "header groups");
        unit_test.test_equals(header->components.size(), 0, "FIX5.0 SP1", "header components");

        auto trailer = dict->trailer();
        unit_test.test_equals(trailer->fields.size(), 3, "FIX5.0 SP1", "trailer fields");
        unit_test.test_equals(trailer->groups.size(), 0, "FIX5.0 SP1", "trailer groups");
        unit_test.test_equals(trailer->components.size(), 0, "FIX5.0 SP1", "trailer components");

        unit_test.test_equals(dict->messages()->size(), 113, "FIX5.0 SP1", "messages");
        unit_test.test_equals(dict->fields()->size(), 1373, "FIX5.0 SP1", "fields");
        unit_test.test_equals(dict->components()->size(), 163, "FIX5.0 SP1", "components");

        unit_test.test_equals((*dict->fields())["MsgType"].values.size(), 113, "FIX5.0 SP1", "message type values");
        unit_test.test_equals((*dict->fields())["MsgType"].value_descriptions.size(), 113, "FIX5.0 SP1", "message type value descriptions");

        auto fields = *dict->fields();
        for (const auto& field : fields)
        {
            all_fields[field.second.tag] = field.second.name;
        }
    }

    //////////////////////////////////////////////////////////////////
    // FIX 5.0 SP2 & FIXT 1.1
    {
        std::string target_file = quickfix_schemas_root_path + "FIX50SP2.xml";
        std::string target_transport_file = quickfix_schemas_root_path + "FIXT11.xml";

        FixDictionaryLoader loader;

        if (loader.load_from(target_transport_file, true) == false)
        {
            std::cout << "Could not open " << target_transport_file << "\n";
            return -1;
        }

        if (loader.load_from(target_file) == false)
        {
            std::cout << "Could not open " << target_file << "\n";
            return -1;
        }

        unit_test.test_equals(loader.errors()->size(), 0, "FIX5.0 SP2", "loader errors");
        unit_test.test_equals(loader.warnings()->size(), 0, "FIX5.0 SP2", "loader warnings");

        FixDictionary* dict = loader.get_dictionary();
        unit_test.test_equals(dict->major(), 5, "FIX5.0 SP2", "major");
        unit_test.test_equals(dict->minor(), 0, "FIX5.0 SP2", "minor");
        unit_test.test_equals(dict->service_pack(), 2, "FIX5.0 SP2", "servicepack");

        unit_test.test_equals(dict->transport_major(), 1, "FIX5.0 SP2", "transport major");
        unit_test.test_equals(dict->transport_minor(), 1, "FIX5.0 SP2", "transport minor");
        unit_test.test_equals(dict->transport_service_pack(), 0, "FIX5.0 SP2", "transport servicepack");

        std::string begin_string;
        loader.get_dictionary()->get_begin_string(begin_string);
        unit_test.test_equals(begin_string, "FIXT.1.1", "FIX5.0 SP2", "begin_string");
        unit_test.test_equals(loader.get_dictionary()->get_namespace_name(), "FIX50SP2", "FIX5.0 SP2", "namespace");

        auto header = dict->header();
        unit_test.test_equals(header->fields.size(), 29, "FIX5.0 SP2", "header fields");
        unit_test.test_equals(header->groups.size(), 1, "FIX5.0 SP2", "header groups");
        unit_test.test_equals(header->components.size(), 0, "FIX5.0 SP2", "header components");

        auto trailer = dict->trailer();
        unit_test.test_equals(trailer->fields.size(), 3, "FIX5.0 SP2", "trailer fields");
        unit_test.test_equals(trailer->groups.size(), 0, "FIX5.0 SP2", "trailer groups");
        unit_test.test_equals(trailer->components.size(), 0, "FIX5.0 SP2", "trailer components");

        unit_test.test_equals(dict->messages()->size(), 164, "FIX5.0 SP2", "messages");
        unit_test.test_equals(dict->fields()->size(), 6028, "FIX5.0 SP2", "fields");
        unit_test.test_equals(dict->components()->size(), 725, "FIX5.0 SP2", "components");

        unit_test.test_equals((*dict->fields())["MsgType"].values.size(), 164, "FIX5.0 SP2", "message type values");
        unit_test.test_equals((*dict->fields())["MsgType"].value_descriptions.size(), 164, "FIX5.0 SP2", "message type value descriptions");

        auto fields = *dict->fields();
        for (const auto& field : fields)
        {
            all_fields[field.second.tag] = field.second.name;
        }
    }

    // PRINT THE REPORT
    cout << unit_test.get_summary_report("FixDictionary");
    cout.flush();

    // EXPORT ALL FIELDS CSV
    llfix::FileSystemUtilities::delete_file_if_exists("tags.csv");
    std::string all_fields_content;
    all_fields_content += "TAG,NAME\n";

    std::vector<std::pair<uint32_t, std::string>> sorted_all_fields(all_fields.begin(), all_fields.end());
    std::sort(sorted_all_fields.begin(), sorted_all_fields.end(),[](auto& a, auto& b) { return a.first < b.first; });
    for (auto& [tag_no, tag_name] : sorted_all_fields)
    {
        all_fields_content += std::to_string(tag_no) + "," + tag_name + "\n";
    }

    all_fields_content.pop_back();

    llfix::FileSystemUtilities::append_text_to_file("tags.csv", all_fields_content);

    #if _WIN32
    bool pause = true;
    if(argc > 1)
    {
        if (std::strcmp(argv[1], "no_pause") == 0)
            pause = false;
    }
    if(pause)
        std::system("pause");
    #else
    LLFIX_UNUSED(argc);
    LLFIX_UNUSED(argv);
    #endif

    return unit_test.did_all_pass();
}