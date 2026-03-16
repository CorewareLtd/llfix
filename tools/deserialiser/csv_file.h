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
#pragma once

#include <cstddef>
#include <fstream>
#include <string>
#include <vector>
#include <unordered_map>

#include <llfix/core/utilities/std_string_utilities.h>

class CSVFileRow
{
    public:

        bool has_column(const std::string& column_name) const
        {
            return m_data.find(column_name) != m_data.end();
        }

        std::string operator[](const std::string& column_name)
        {
            return m_data[column_name];
        }

        void insert(const std::string& column_name, const std::string& value)
        {
            m_data[column_name] = value;
        }

    private:
        std::unordered_map<std::string, std::string> m_data;
};

class CSVFile
{
    public:

        bool load_from(const std::string& csv_file_path)
        {
            std::ifstream file(csv_file_path);

            if (!file.is_open())
            {
                return false;
            }

            std::string line;
            std::size_t line_counter = 0;

            while (std::getline(file, line))
            {
                line_counter++;

                if(line[0] == '#')
                {
                    continue;
                }

                auto tokens = llfix::StringUtilities::split(line, ',');
                auto token_count = tokens.size();

                if(line_counter == 1)
                {
                    for(const auto& token : tokens)
                    {
                        m_column_names.push_back(token);
                    }

                    continue;
                }

                if(token_count != m_column_names.size() )
                {
                    file.close();
                    return false;
                }

                CSVFileRow row;

                for(std::size_t i = 0; i < token_count; i++)
                {
                    row.insert(m_column_names[i], tokens[i]);
                }

                m_rows.push_back(row);
            }

            file.close();
            return true;
        }

        using iterator = std::vector<CSVFileRow>::iterator;
        using const_iterator = std::vector<CSVFileRow>::const_iterator;

        iterator begin() { return m_rows.begin(); }
        iterator end() { return m_rows.end(); }

        const_iterator begin() const { return m_rows.begin(); }
        const_iterator end() const { return m_rows.end(); }

        const_iterator cbegin() const { return m_rows.cbegin(); }
        const_iterator cend() const { return m_rows.cend(); }

    private:
        std::vector<std::string> m_column_names;
        std::vector<CSVFileRow> m_rows;
};