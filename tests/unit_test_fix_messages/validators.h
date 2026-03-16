#pragma once

#include <cstdint>
#include <cstddef>
#include <string>
#include <vector>
#include <algorithm>
#include <cctype>

#include <llfix/core/utilities/std_string_utilities.h>

void parse_fix_buffer(const char* buffer, std::size_t buffer_size, std::vector<uint32_t>& tags, std::vector<std::string>& values)
{
    std::size_t buffer_read{ 0 };
    bool looking_for_equals{ true };

    std::size_t current_tag_start{ 0 };
    std::size_t current_tag_length{ 0 };

    std::size_t current_value_start{ 0 };
    std::size_t current_value_length{ 0 };

    while (true)
    {
        if (looking_for_equals)
        {
            if (buffer[buffer_read] == '=')
            {
                looking_for_equals = false;

                current_value_start = buffer_read + 1;
                current_value_length = 0;
            }
            else
            {
                current_tag_length++;
            }
        }
        else // looking for delimiter
        {
            if (buffer[buffer_read] == ((char)1))
            {
                uint32_t tag = std::stoi(std::string(std::string_view(buffer + current_tag_start, current_tag_length)));

                char value[1024];
                std::memcpy(value, buffer + current_value_start, current_value_length);
                value[current_value_length] = '\0';

                tags.push_back(tag);
                values.push_back(value);

                looking_for_equals = true;

                current_tag_start = buffer_read + 1;
                current_tag_length = 0;
            }
            else
            {
                current_value_length++;
            }
        }

        buffer_read++;

        if (buffer_read == buffer_size)
        {
            break;
        }
    }
}

bool validate_checksum(const char* buffer, std::size_t buffer_size)
{
    uint32_t t10_val{ 0 };
    uint32_t computed_checksum{ 0 };

    std::vector<uint32_t> tags;
    std::vector<std::string> values;
    parse_fix_buffer(buffer, buffer_size, tags, values);

    uint32_t i = (uint32_t)tags.size() - 1;
    while (true)
    {
        if (tags[i] == 10)
        {
            t10_val = (uint32_t)(std::stoi(values[i]));
        }
        i--;
        if (i == 0) break;
    }

    std::size_t checksum_calculation_length = buffer_size - 7; // 7 -> 10=xxx|

    for (std::size_t i = 0; i < checksum_calculation_length; i++)
    {
        computed_checksum += static_cast<int>(buffer[i]);
    }

    computed_checksum = computed_checksum % 256;

    return t10_val == computed_checksum;
}

bool validate_body_length(const char* buffer, std::size_t buffer_size)
{
    std::vector<uint32_t> tags;
    std::vector<std::string> values;
    parse_fix_buffer(buffer, buffer_size, tags, values);

    uint32_t tag9_val{ 0 };
    uint32_t computed_body_length{ 0 };

    for (std::size_t i = 0; i < tags.size() - 1; i++)
    {
        auto current_tag = tags[i];

        if (current_tag != 9)
        {
            if (current_tag != 8)
            {
                computed_body_length += uint32_t(values[i].length() + 2 + std::to_string(current_tag).length()); // +2 -> equals and delimiter
            }
        }
        else
        {
            tag9_val = (uint32_t)std::stoi(values[i]);
        }
    }

    return tag9_val == computed_body_length;
}

bool validate_integer_fix_tag_value(std::string value)
{
    for (const auto& ch : value)
    {
        if (!std::isdigit(ch) && ch != '-')
        {
            return false;
        }
    }

    return true;
}

bool validate_double_fix_tag_value(std::string value)
{
    for (const auto& character : value)
    {
        if (character != '.' && std::isdigit(static_cast<unsigned char>(character) == false))
        {
            return false;
        }
    }

    return true;
}

bool validate_time_fix_tag_value(std::string value)
{
    for (const auto& character : value)
    {
        if (character != '.' && std::isdigit(static_cast<unsigned char>(character) == false) && character != ':' && character != '-')
        {
            return false;
        }
    }

    return true;
}


/*
    Validations :

            All tags should be digits only

            Int tags should have digit values only ( 9  34  54 38 40 59 10 etc )
            Double tags should have digit or . ( 44 etc )
            Time tags should have digit or : or - or . ( 52 60 etc )

            There should be only one = between 2 delimiters
            There should be at least one = between 2 delimiters

            No new lines in the buffer

            First 2 tags should be 8 and 9
            Last tag should be 10
*/
bool validate_message(const char* buffer, std::size_t buffer_length)
{
    std::string string_buffer;

    for (std::size_t i = 0; i < buffer_length; i++)
    {
        char current_char = buffer[i];

        // NEW LINE CHECKS
        if (current_char == '\0')
        {
            return false;
        }

        if (current_char == ((char)(1)))
        {
            string_buffer += '|';
        }
        else
        {
            string_buffer += current_char;
        }
    }

    auto tag_value_pairs = llfix::StringUtilities::split(string_buffer, '|');

    int index = 0;

    for (const auto& tag_value_pair : tag_value_pairs)
    {
        auto tokens = llfix::StringUtilities::split(tag_value_pair, '=');

        if (tokens.size() != 2)
        {
            return false;
        }

        auto current_tag = tokens[0];
        auto current_val = tokens[1];

        bool is_current_tag_digits_only = std::all_of(current_tag.begin(), current_tag.end(), ::isdigit);

        if (is_current_tag_digits_only == false)
        {
            return false;
        }

        auto current_tag_integer = std::stoi(current_tag);

        if(index==0)
        {
            if(current_tag_integer!=8)
            {
                return false;
            }
        }
        else if(index == 1)
        {
            if(current_tag_integer!=9)
            {
                return false;
            }
        }

        if(tag_value_pairs.size() - 1 == static_cast<std::size_t>(index))
        {
            if(current_tag_integer!=10)
            {
                return false;
            }
        }

        if (current_tag_integer == 52 || current_tag_integer == 60)
        {
            if (validate_time_fix_tag_value(current_val) == false)
            {
                return false;
            }
        }

        if (current_tag_integer == 44)
        {
            if (validate_double_fix_tag_value(current_val) == false)
            {
                return false;
            }
        }

        if (current_tag_integer == 9 || current_tag_integer == 34 || current_tag_integer == 54 || current_tag_integer == 38 || current_tag_integer == 40 || current_tag_integer == 59 || current_tag_integer == 10)
        {
            if (validate_integer_fix_tag_value(current_val) == false)
            {
                return false;
            }
        }

        index++;
    }

    return true;
}