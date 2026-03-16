///////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// DEFINES
#ifndef LLFIX_UNIT_TEST
#define LLFIX_UNIT_TEST
#endif

#ifndef LLFIX_ENABLE_DICTIONARY
#define LLFIX_ENABLE_DICTIONARY
#endif

#ifndef LLFIX_ENABLE_BINARY_FIELDS
#define LLFIX_ENABLE_BINARY_FIELDS
#endif
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#include <llfix/common.h>
#include "../unit_test.h"

#include <iostream>
using namespace std;

#include <cstdlib>
#include <cstring>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>
#include <memory>

#include <llfix/core/utilities/filesystem_utilities.h>
#include <llfix/core/compiler/unused.h>
#include <llfix/incoming_fix_message.h>
#include <llfix/fix_session_settings.h>
#include <llfix/fix_session.h>
#include <llfix/fix_dictionary_validator.h>
#include <llfix/fix_string_view.h>
using namespace llfix;
using namespace FixDictionaryValidator;

FixStringView* get_fix_string_view(const char* input);
void process_format_validation(UnitTest& unit_test, llfix::FixDictionaryValidator::FieldType test_field_type, const std::string& test_tag_value, const std::string& test_case_name, bool is_positive, uint32_t expected_error_code = 0);

template <typename T>
void build_key_from(const T& source, std::string& target)
{
    std::size_t key_length = source.length();

    for (std::size_t i = 0; i < key_length; i++)
    {
        target[i] = source[i];
    }

    target[key_length] = '\0';
}

int main(int argc, char* argv[])
{
    UnitTest unit_test;

    //////////////////////////////////////////////////////////
    // DICTIONARY LOADINGS
    {
        std::string test_dictionary = llfix::FileSystemUtilities::convert_relative_path_to_absolute_path("./minimal.xml");

        std::string message_key = "6";
        uint32_t encoded_message_key = FixUtilities::pack_message_type(message_key);

        FixSession test_session;

        FixSessionSettings settings;
        settings.max_serialised_file_size = 4096;
        settings.sender_comp_id = "a";
        settings.target_comp_id = "b";

        settings.begin_string = "FIX.4.0";
        settings.application_dictionary_path = test_dictionary;

        if (test_session.initialise("test", settings) == false)
        {
            std::cout << "Failed to load test FIX session";
            return -1;
        }

        auto validator = FixSession::get_validator(1);

        // Field definitions
        unit_test.test_equals(validator.m_field_definitions.size(), 35, "dictionary loading", "loaded field count");
        unit_test.test_equals(validator.m_field_definitions[8].m_allowed_values.size(), 3, "dictionary loading", "loaded field allowed value count");
        unit_test.test_equals(validator.m_field_definitions[8].is_value_for_this_field("1") , true, "dictionary loading", "loaded field allowed value 1");
        unit_test.test_equals(validator.m_field_definitions[8].is_value_for_this_field("2") , true, "dictionary loading", "loaded field allowed value 2");
        unit_test.test_equals(validator.m_field_definitions[8].is_value_for_this_field("3") , true, "dictionary loading", "loaded field allowed value 3");
        unit_test.test_equals(validator.m_field_definitions[8].is_value_for_this_field("4"), false, "dictionary loading", "loaded field allowed value 4");

        // Message definitions
        unit_test.test_equals(validator.m_message_definitions.size(), 1, "dictionary loading", "loaded message count");

        unit_test.test_equals(validator.m_message_definitions[encoded_message_key].m_required_fields.size(), 5, "dictionary loading", "loaded message required fields count");
        unit_test.test_equals(validator.m_message_definitions[encoded_message_key].m_required_fields.find(3) != validator.m_message_definitions[encoded_message_key].m_required_fields.end(), true, "dictionary loading", "loaded message required tag - direct field");
        unit_test.test_equals(validator.m_message_definitions[encoded_message_key].m_required_fields.find(69) != validator.m_message_definitions[encoded_message_key].m_required_fields.end(), true, "dictionary loading", "loaded message required tag - child component's field");
        unit_test.test_equals(validator.m_message_definitions[encoded_message_key].m_required_fields.find(13) != validator.m_message_definitions[encoded_message_key].m_required_fields.end(), true, "dictionary loading", "loaded message required tag - grand child component's field");
        unit_test.test_equals(validator.m_message_definitions[encoded_message_key].m_required_fields.find(1) != validator.m_message_definitions[encoded_message_key].m_required_fields.end(), true, "dictionary loading", "loaded message required tag - header required");
        unit_test.test_equals(validator.m_message_definitions[encoded_message_key].m_required_fields.find(6) != validator.m_message_definitions[encoded_message_key].m_required_fields.end(), true, "dictionary loading", "loaded message required tag - trailer required");

        unit_test.test_equals(validator.m_message_definitions[encoded_message_key].m_non_required_fields.size(), 10, "dictionary loading", "loaded message nonrequired fields count");
        unit_test.test_equals(validator.m_message_definitions[encoded_message_key].m_non_required_fields.find(4) != validator.m_message_definitions[encoded_message_key].m_non_required_fields.end(), true, "dictionary loading", "loaded nonrequired field - direct field");
        unit_test.test_equals(validator.m_message_definitions[encoded_message_key].m_non_required_fields.find(10) != validator.m_message_definitions[encoded_message_key].m_non_required_fields.end(), true, "dictionary loading", "loaded nonrequired field - child component's field 1");
        unit_test.test_equals(validator.m_message_definitions[encoded_message_key].m_non_required_fields.find(14) != validator.m_message_definitions[encoded_message_key].m_non_required_fields.end(), true, "dictionary loading", "loaded nonrequired field - grand child component's field 1");
        unit_test.test_equals(validator.m_message_definitions[encoded_message_key].m_non_required_fields.find(15) != validator.m_message_definitions[encoded_message_key].m_non_required_fields.end(), true, "dictionary loading", "loaded nonrequired field - grand child component's field 2");
        unit_test.test_equals(validator.m_message_definitions[encoded_message_key].m_non_required_fields.find(16) != validator.m_message_definitions[encoded_message_key].m_non_required_fields.end(), true, "dictionary loading", "loaded nonrequired field - grand child component's field 3");
        unit_test.test_equals(validator.m_message_definitions[encoded_message_key].m_non_required_fields.find(11) != validator.m_message_definitions[encoded_message_key].m_non_required_fields.end(), true, "dictionary loading", "loaded nonrequired field - child component's field 2");
        unit_test.test_equals(validator.m_message_definitions[encoded_message_key].m_non_required_fields.find(12) != validator.m_message_definitions[encoded_message_key].m_non_required_fields.end(), true, "dictionary loading", "loaded nonrequired field - child component's field 3");
        unit_test.test_equals(validator.m_message_definitions[encoded_message_key].m_non_required_fields.find(2) != validator.m_message_definitions[encoded_message_key].m_non_required_fields.end(), true, "dictionary loading", "loaded nonrequired field - header");
        unit_test.test_equals(validator.m_message_definitions[encoded_message_key].m_non_required_fields.find(7) != validator.m_message_definitions[encoded_message_key].m_non_required_fields.end(), true, "dictionary loading", "loaded nonrequired field - trailer");

        // Repeating group definitions
        unit_test.test_equals(validator.m_required_repeating_group_definitions[encoded_message_key].size(), 2, "dictionary loading", "loaded required repeating group count");
        unit_test.test_equals(validator.m_required_repeating_group_definitions[encoded_message_key][101].m_count_tag, 101, "dictionary loading", "required count tag1");
        unit_test.test_equals(validator.m_required_repeating_group_definitions[encoded_message_key][104].m_count_tag, 104, "dictionary loading", "required count tag1");

        // Repeating group NoGroupTwo/t101 for msgtype '6'
        /*
            <group name='NoGroupTwo' required='Y'>

                <field name='Eighteen' required='N' />
                <field name='Nineteen' required='Y' />
                <component name='PtysSubGrp' required='N' />
                <component name='ComponentNestedInGroup' required='Y' />

                <group name='NoGroupNested' required='Y'>
                    <field name='TwentyTwo' required='N' />
                    <field name='TwentyThree' required='Y' />
                </group>

            </group>

            <component name='ComponentNestedInGroup'>
                <field name='TwentySix' required='Y' />
                <field name='TwentySeven' required='N' />
            </component>

            <field number='101' name='NoGroupTwo' type='NUMINGROUP' />
        */

        auto encoded_msg_type = FixUtilities::pack_message_type("6");

        unit_test.test_equals(FixSession::get_repeating_group_specs().is_a_repeating_group_count_tag(encoded_msg_type, 101), true, "dictionary loading repeating group", "repeating group count tag");

        unit_test.test_equals(FixSession::get_repeating_group_specs().is_a_repeating_group_tag(encoded_msg_type, 101, 18), true, "dictionary loading repeating group", "direct repeating group tag1");
        unit_test.test_equals(FixSession::get_repeating_group_specs().is_a_repeating_group_tag(encoded_msg_type, 101, 19), true, "dictionary loading repeating group", "direct repeating group tag2");
        unit_test.test_equals(FixSession::get_repeating_group_specs().is_a_repeating_group_tag(encoded_msg_type, 101, 26), true, "dictionary loading repeating group", "repeating group component tag1");
        unit_test.test_equals(FixSession::get_repeating_group_specs().is_a_repeating_group_tag(encoded_msg_type, 101, 27), true, "dictionary loading repeating group", "repeating group component tag2");

        unit_test.test_equals(FixSession::get_repeating_group_specs().is_a_repeating_group_tag(encoded_msg_type, 101, 802), true, "dictionary loading repeating group", "repeating group component rg tag1");
        unit_test.test_equals(FixSession::get_repeating_group_specs().is_a_repeating_group_tag(encoded_msg_type, 101, 523), true, "dictionary loading repeating group", "repeating group component rg tag2");
        unit_test.test_equals(FixSession::get_repeating_group_specs().is_a_repeating_group_tag(encoded_msg_type, 101, 803), true, "dictionary loading repeating group", "repeating group component rg tag3");
        unit_test.test_equals(FixSession::get_repeating_group_specs().is_a_repeating_group_tag(encoded_msg_type, 101, 104), true, "dictionary loading repeating group", "repeating group nested group field tag1");
        unit_test.test_equals(FixSession::get_repeating_group_specs().is_a_repeating_group_tag(encoded_msg_type, 101, 22), true, "dictionary loading repeating group", "repeating group nested group field tag2");
        unit_test.test_equals(FixSession::get_repeating_group_specs().is_a_repeating_group_tag(encoded_msg_type, 101, 23), true, "dictionary loading repeating group", "repeating group nested group field tag3");

        std::cout << test_session.get_repeating_group_specs().to_string();
    }

    //////////////////////////////////////////////////////////
    // MULTIPLE DICTIONARY LOADINGS
    {
        std::string test_dictionary = llfix::FileSystemUtilities::convert_relative_path_to_absolute_path("../dictionaries/FIX50SP2.xml");
        std::string test_dictionary_transport = llfix::FileSystemUtilities::convert_relative_path_to_absolute_path("../dictionaries/FIXT11.xml");

        FixSessionSettings settings;
        settings.max_serialised_file_size = 4096;
        settings.sender_comp_id = "a";
        settings.target_comp_id = "b";

        settings.begin_string = "FIXT.1.1";
        settings.default_app_ver_id = 7;

        settings.application_dictionary_path = test_dictionary;
        settings.transport_dictionary_path = test_dictionary_transport;

        settings.initialise_derived_settings();

        std::vector<std::unique_ptr<FixSession>> test_sessions;
        constexpr std::size_t test_session_count = 8;

        for(std::size_t i =0;i<test_session_count; i++)
        {
            auto new_session = new FixSession;

            if (new_session->initialise("test" + std::to_string(i+1), settings) == false)
            {
                std::cout << "Failed to load test FIX session";
                return -1;
            }

            test_sessions.push_back(std::unique_ptr<FixSession>(new_session));
        }

        for(std::size_t i =0;i<test_session_count; i++)
        {
            // Validator is cached in this test. Therefore we test if rg specs was propagated correctly to all sessions
            unit_test.test_equals(FixSession::get_repeating_group_specs().is_a_repeating_group_count_tag(FixUtilities::pack_message_type("D"), 453), true, "multiple dictionary loadings", "repeating group specs test session" + std::to_string(i+1));
        }
    }

    //////////////////////////////////////////////////////////
    // FIELD DEFINITION VALIDATIONS

    // ALLOWED VALUES EMPTY
    {
        IncomingFixMessage incoming_message;
        if (incoming_message.initialise() == false) { std::cout << "Failed to init IncomingFixMessage\n"; return -1; }
        incoming_message.set_tag(35, get_fix_string_view("D"));

        Validator validator;
        MessageDefinition definition;
        definition.add_required_field({ 35 });
        validator.specify_message_definition("D", definition);

        uint32_t reject_code{ 0 };
        unit_test.test_equals(validator.validate(incoming_message, reject_code), false, "validations", "allowed values empty");
        unit_test.test_equals(reject_code, 3, "validations", "allowed values empty reject_code");
    }

    // ALLOWED VALUES POSITIVE
    {
        IncomingFixMessage incoming_message;
        if (incoming_message.initialise() == false) { std::cout << "Failed to init IncomingFixMessage\n"; return -1; }
        incoming_message.set_tag(35, get_fix_string_view("D"));
        incoming_message.set_tag(40, get_fix_string_view("one"));

        Validator validator;

        MessageDefinition definition;
        definition.add_required_field({ 35 });
        definition.add_non_required_field({ 40 });
        validator.specify_message_definition("D", definition);

        FieldDefinition fdefinition_40;
        fdefinition_40.add_allowed_value("one");
        validator.specify_field_definition(40, fdefinition_40);

        FieldDefinition fdefinition_35;
        validator.specify_field_definition(35, fdefinition_35);

        uint32_t reject_code{ 0 };
        unit_test.test_equals(validator.validate(incoming_message, reject_code), true, "validations", "allowed values positive");
    }

    // ALLOWED VALUES NEGATIVE
    {
        IncomingFixMessage incoming_message;
        if (incoming_message.initialise() == false) { std::cout << "Failed to init IncomingFixMessage\n"; return -1; }
        incoming_message.set_tag(35, get_fix_string_view("D"));
        incoming_message.set_tag(40, get_fix_string_view("one"));

        Validator validator;
        MessageDefinition definition;
        definition.add_required_field({ 35 });
        definition.add_non_required_field({ 40 });
        validator.specify_message_definition("D", definition);

        FieldDefinition fdefinition;
        fdefinition.add_allowed_value("two");
        validator.specify_field_definition(40, fdefinition);

        FieldDefinition fdefinition_35;
        validator.specify_field_definition(35, fdefinition_35);

        uint32_t reject_code{ 0 };
        unit_test.test_equals(validator.validate(incoming_message, reject_code), false, "validations", "allowed values negative");
        unit_test.test_equals(reject_code, 5, "validations", "allowed values negative negative reject_code");
    }

    // FORMAT VALIDATIONS STRING
    process_format_validation(unit_test, llfix::FixDictionaryValidator::FieldType::STRING, "Y", "format validation string", true);

    // FORMAT VALIDATIONS BOOLEAN POSITIVE
    process_format_validation(unit_test, llfix::FixDictionaryValidator::FieldType::BOOL, "Y", "format validation boolean positive", true);

    // FORMAT VALIDATIONS BOOLEAN NEGATIVE
    process_format_validation(unit_test, llfix::FixDictionaryValidator::FieldType::BOOL, "X", "format validation boolean negative", false, 6);

    // FORMAT VALIDATIONS BOOLEAN NEGATIVE INVALID LENGTH
    process_format_validation(unit_test, llfix::FixDictionaryValidator::FieldType::BOOL, "YZ", "format validation boolean negative invalid length", false, 6);

    // FORMAT VALIDATIONS INT POSITIVE
    process_format_validation(unit_test, llfix::FixDictionaryValidator::FieldType::INT, "5", "format validation int positive", true);

    // FORMAT VALIDATIONS INT POSITIVE WITH PLUS SIGN
    process_format_validation(unit_test, llfix::FixDictionaryValidator::FieldType::INT, "+5", "format validation int positive with plus sign", true);

    // FORMAT VALIDATIONS INT POSITIVE WITH MINUS SIGN
    process_format_validation(unit_test, llfix::FixDictionaryValidator::FieldType::INT, "-5", "format validation int positive with minus sign", true);

    // FORMAT VALIDATIONS INT NEGATIVE
    process_format_validation(unit_test, llfix::FixDictionaryValidator::FieldType::INT, "X", "format validation int negative", false, 6);

    // FORMAT VALIDATIONS DOUBLE POSITIVE
    process_format_validation(unit_test, llfix::FixDictionaryValidator::FieldType::FLOAT, "5.55", "format validation double positive", true);

    // FORMAT VALIDATIONS DOUBLE POSITIVE WITH PLUS SIGN
    process_format_validation(unit_test, llfix::FixDictionaryValidator::FieldType::FLOAT, "+5.55", "format validation double positive with plus sign", true);

    // FORMAT VALIDATIONS DOUBLE POSITIVE WITH MINUS SIGN
    process_format_validation(unit_test, llfix::FixDictionaryValidator::FieldType::FLOAT, "-5.55", "format validation double positive with minus sign", true);

    // FORMAT VALIDATIONS DOUBLE NEGATIVE
    process_format_validation(unit_test, llfix::FixDictionaryValidator::FieldType::FLOAT, "5-5", "format validation double negative", false, 6);

    // FORMAT VALIDATIONS TIMESTAMP POSITIVE
    process_format_validation(unit_test, llfix::FixDictionaryValidator::FieldType::TIMESTAMP, "20250101-01:01:01.012234", "format validation TIMESTAMP positive", true);

    // FORMAT VALIDATIONS TIMESTAMP NEGATIVE
    process_format_validation(unit_test, llfix::FixDictionaryValidator::FieldType::TIMESTAMP, "20250101-01:01:0x.012234", "format validation TIMESTAMP negative", false, 6);

    // FORMAT VALIDATIONS TIMESTAMP NEGATIVE INVALID LENGTH
    process_format_validation(unit_test, llfix::FixDictionaryValidator::FieldType::TIMESTAMP, "20250101-01:01:0", "format validation TIMESTAMP negative invalid length", false, 6);

    // FORMAT VALIDATIONS DATE_ONLY POSITIVE
    process_format_validation(unit_test, llfix::FixDictionaryValidator::FieldType::DATE_ONLY, "20250101", "format validation DATE_ONLY positive", true);

    // FORMAT VALIDATIONS DATE_ONLY NEGATIVE
    process_format_validation(unit_test, llfix::FixDictionaryValidator::FieldType::DATE_ONLY, "2025x101", "format validation DATE_ONLY negative", false, 6);

    // FORMAT VALIDATIONS DATE_ONLY NEGATIVE INVALID LENGTH
    process_format_validation(unit_test, llfix::FixDictionaryValidator::FieldType::DATE_ONLY, "2025010101", "format validation DATE_ONLY negative invalid length", false, 6);

    // FORMAT VALIDATIONS TIMEONLY POSITIVE
    process_format_validation(unit_test, llfix::FixDictionaryValidator::FieldType::TIME_ONLY, "20:20:20.0", "format validation TIMEONLY positive", true);

    // FORMAT VALIDATIONS TIMEONLY NEGATIVE
    process_format_validation(unit_test, llfix::FixDictionaryValidator::FieldType::TIME_ONLY, "20:20:2", "format validation TIMEONLY negative", false, 6);

    // FORMAT VALIDATIONS TIMEONLY NEGATIVE INVALID LENGTH
    process_format_validation(unit_test, llfix::FixDictionaryValidator::FieldType::TIME_ONLY, "20:20:2", "format validation TIMEONLY negative invalid length", false, 6);

    //////////////////////////////////////////////////////////
    // MESSAGE DEFINITION VALIDATIONS

    // EMPTY (NO DEFINITIONS)
    {
        IncomingFixMessage incoming_message;
        if (incoming_message.initialise() == false) { std::cout << "Failed to init IncomingFixMessage\n"; return -1; }
        incoming_message.set_tag(35, get_fix_string_view("D"));

        Validator validator;
        uint32_t reject_code{ 0 };
        unit_test.test_equals(validator.validate(incoming_message, reject_code), true, "validations", "empty");
    }

    // MSG TYPE VALIDATION NEGATIVE
    {
        IncomingFixMessage incoming_message;
        if (incoming_message.initialise() == false) { std::cout << "Failed to init IncomingFixMessage\n"; return -1; }
        incoming_message.set_tag(35, get_fix_string_view("x"));

        Validator validator;
        MessageDefinition definition;
        validator.specify_message_definition("D", definition);

        uint32_t reject_code{ 0 };
        unit_test.test_equals(validator.validate(incoming_message, reject_code), false, "validations", "message type negative");
        unit_test.test_equals(reject_code, 11, "validations", "message type negative reject_code");
    }

    // MSG TYPE VALIDATION POSITIVE
    {
        IncomingFixMessage incoming_message;
        if (incoming_message.initialise() == false) { std::cout << "Failed to init IncomingFixMessage\n"; return -1; }
        incoming_message.set_tag(35, get_fix_string_view("D"));

        Validator validator;
        MessageDefinition definition;
        definition.add_required_field({ 35 });
        validator.specify_message_definition("D", definition);

        FieldDefinition fdefinition_35;
        validator.specify_field_definition(35, fdefinition_35);

        uint32_t reject_code{ 0 };
        unit_test.test_equals(validator.validate(incoming_message, reject_code), true, "validations", "message type positive");

        IncomingFixMessage incoming_message2;
        if (incoming_message2.initialise() == false) { std::cout << "Failed to init IncomingFixMessage\n"; return -1; }
        incoming_message2.set_tag(35, get_fix_string_view("D"));

        unit_test.test_equals(validator.validate(incoming_message2, reject_code), true, "validations", "message type positive 2");
    }

    // MISSING REQUIRED TAG NEGATIVE
    {
        IncomingFixMessage incoming_message;
        if (incoming_message.initialise() == false) { std::cout << "Failed to init IncomingFixMessage\n"; return -1; }
        incoming_message.set_tag(35, get_fix_string_view("D"));

        Validator validator;
        MessageDefinition definition;
        definition.add_required_field({11});
        validator.specify_message_definition("D", definition);

        uint32_t reject_code{ 0 };
        unit_test.test_equals(validator.validate(incoming_message, reject_code), false, "validations", "missing required tag negative");
        unit_test.test_equals(reject_code, 1, "validations", "missing required tag negative reject_code");
    }

    // MISSING REQUIRED TAG POSITIVE
    {
        IncomingFixMessage incoming_message;
        if (incoming_message.initialise() == false) { std::cout << "Failed to init IncomingFixMessage\n"; return -1; }
        incoming_message.set_tag(35, get_fix_string_view("D"));

        incoming_message.set_tag(11, get_fix_string_view("order1"));

        Validator validator;
        MessageDefinition definition;
        definition.add_required_field({ 35 });
        definition.add_required_field({ 11 });
        validator.specify_message_definition("D", definition);

        FieldDefinition fdefinition_35;
        validator.specify_field_definition(35, fdefinition_35);

        FieldDefinition fdefinition_11;
        validator.specify_field_definition(11, fdefinition_11);

        uint32_t reject_code{ 0 };
        unit_test.test_equals(validator.validate(incoming_message, reject_code), true, "validations", "missing required tag positive");
    }

    // UNKNOWN TAG NEGATIVE
    {
        IncomingFixMessage incoming_message;
        if (incoming_message.initialise() == false) { std::cout << "Failed to init IncomingFixMessage\n"; return -1; }
        incoming_message.set_tag(35, get_fix_string_view("D"));

        incoming_message.set_tag(55, get_fix_string_view("VOD.L"));

        Validator validator;
        MessageDefinition definition;
        definition.add_required_field({ 35 });
        validator.specify_message_definition("D", definition);

        FieldDefinition fdefinition_35;
        validator.specify_field_definition(35, fdefinition_35);

        FieldDefinition fdefinition_55;
        validator.specify_field_definition(55, fdefinition_55);

        uint32_t reject_code{ 0 };
        unit_test.test_equals(validator.validate(incoming_message, reject_code), false, "validations", "unknown tag negative");
        unit_test.test_equals(reject_code, 2, "validations", "unknown tag negative reject_code");
    }

    // UNKNOWN TAG POSITIVE
    {
        IncomingFixMessage incoming_message;
        if (incoming_message.initialise() == false) { std::cout << "Failed to init IncomingFixMessage\n"; return -1; }
        incoming_message.set_tag(35, get_fix_string_view("D"));

        incoming_message.set_tag(55, get_fix_string_view("VOD.L"));

        Validator validator;
        MessageDefinition definition;
        definition.add_required_field({ 35 });
        definition.add_non_required_field({ 55 });
        validator.specify_message_definition("D", definition);

        FieldDefinition fdefinition_35;
        validator.specify_field_definition(35, fdefinition_35);

        FieldDefinition fdefinition_55;
        validator.specify_field_definition(55, fdefinition_55);

        uint32_t reject_code{ 0 };
        unit_test.test_equals(validator.validate(incoming_message, reject_code), true, "validations", "unknown tag positive");
    }

    //////////////////////////////////////////////////////////
    // REPEATING GROUPS VALIDATIONS

    // EMPTY (NO RG DEFINITIONS)
    {
        IncomingFixMessage incoming_message;
        if (incoming_message.initialise() == false) { std::cout << "Failed to init IncomingFixMessage\n"; return -1; }
        incoming_message.set_tag(35, get_fix_string_view("D"));

        Validator validator;
        uint32_t reject_code{ 0 };
        unit_test.test_equals(validator.validate_repeating_groups(incoming_message, reject_code), true, "repeating group validations", "empty");
    }

    // REQUIRED REPEATING GROUP DOES NOT EXIST
    {
        IncomingFixMessage incoming_message;
        if (incoming_message.initialise() == false) { std::cout << "Failed to init IncomingFixMessage\n"; return -1; }
        incoming_message.set_tag(35, get_fix_string_view("D"));

        Validator validator;
        validator.specify_repeating_group_count_tag("D", 453);

        uint32_t reject_code{ 0 };
        unit_test.test_equals(validator.validate_repeating_groups(incoming_message, reject_code), false, "repeating group validations", "required group does not exist");
        unit_test.test_equals(reject_code, 1, "repeating group validations", "required group does not exist reject_code");
    }

    // REQUIRED REPEATING GROUP - POSITIVE
    {
        IncomingFixMessage incoming_message;
        if (incoming_message.initialise() == false) { std::cout << "Failed to init IncomingFixMessage\n"; return -1; }
        incoming_message.set_tag(35, get_fix_string_view("D"));
        FixSession::get_repeating_group_specs().specify_repeating_group("D", 453, 448, 447, 452);

        incoming_message.set_repeating_group_tag(453, get_fix_string_view("1"));

        Validator validator;
        validator.specify_repeating_group_count_tag("D", 453);

        uint32_t reject_code{ 0 };
        unit_test.test_equals(validator.validate_repeating_groups(incoming_message, reject_code), true, "repeating group validations", "required group positive");
    }

    // REQUIRED REPEATING GROUP - MULTIPLE COUNT/LEADING TAG
    {
        IncomingFixMessage incoming_message;
        if (incoming_message.initialise() == false) { std::cout << "Failed to init IncomingFixMessage\n"; return -1; }
        incoming_message.set_tag(35, get_fix_string_view("D"));
        FixSession::get_repeating_group_specs().specify_repeating_group("D", 453, 448, 447, 452);

        incoming_message.set_repeating_group_tag(453, get_fix_string_view("1"));
        incoming_message.set_repeating_group_tag(453, get_fix_string_view("2"));

        Validator validator;
        validator.specify_repeating_group_count_tag("D", 453);

        uint32_t reject_code{ 0 };
        unit_test.test_equals(validator.validate_repeating_groups(incoming_message, reject_code), false, "repeating group validations", "required group multiple count/leading tag");
        unit_test.test_equals(reject_code, 13, "repeating group validations", "required group multiple count/leading tag reject_code");
    }

    // REQUIRED REPEATING GROUP - NON NUMERIC COUNT/LEADING TAG
    {
        IncomingFixMessage incoming_message;
        if (incoming_message.initialise() == false) { std::cout << "Failed to init IncomingFixMessage\n"; return -1; }
        incoming_message.set_tag(35, get_fix_string_view("D"));
        FixSession::get_repeating_group_specs().specify_repeating_group("D", 453, 448, 447, 452);

        incoming_message.set_repeating_group_tag(453, get_fix_string_view("a"));

        Validator validator;
        validator.specify_repeating_group_count_tag("D", 453);

        uint32_t reject_code{ 0 };
        unit_test.test_equals(validator.validate_repeating_groups(incoming_message, reject_code), false, "repeating group validations", "required group non numeric count/leading tag");
        unit_test.test_equals(reject_code, 6, "repeating group validations", "required group non numeric count/leading tag reject_code");
    }

    // PRINT THE REPORT
    cout << unit_test.get_summary_report("Fix dictionary validator");
    cout.flush();

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

FixStringView* get_fix_string_view(const char* val)
{
    FixStringView* ret = new FixStringView;
    ret->set_buffer(const_cast<char*>(val), strlen(val));
    return ret;
}

void process_format_validation(UnitTest& unit_test, llfix::FixDictionaryValidator::FieldType test_field_type, const std::string& test_tag_value, const std::string& test_case_name, bool is_positive, uint32_t expected_error_code)
{
    IncomingFixMessage incoming_message;
    incoming_message.initialise();
    incoming_message.set_tag(35, get_fix_string_view("D"));

    incoming_message.set_tag(40, get_fix_string_view(test_tag_value.c_str()));

    Validator validator;

    MessageDefinition definition;
    definition.add_required_field({ 35 });
    definition.add_required_field({ 40 });

    validator.specify_message_definition("D", definition);

    FieldDefinition fdefinition_35;
    fdefinition_35.m_type = llfix::FixDictionaryValidator::FieldType::STRING;
    validator.specify_field_definition(35, fdefinition_35);

    FieldDefinition fdefinition_40;
    fdefinition_40.m_type = test_field_type;
    validator.specify_field_definition(40, fdefinition_40);

    uint32_t reject_code{ 0 };
    unit_test.test_equals(validator.validate(incoming_message, reject_code), is_positive, "validations", test_case_name);

    if (is_positive == false)
    {
        unit_test.test_equals(reject_code, expected_error_code, "validations", test_case_name + " reject_code");
    }
}