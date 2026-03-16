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
#include <cstdint>
#include <string>
#include <algorithm>
#include <iostream>

#include <llfix/core/utilities/filesystem_utilities.h>
#include <llfix/core/utilities/std_string_utilities.h>
#include <llfix/electronic_trading/session/sequence_store.h>

#define VERSION "1.0.0"

using namespace std;
using namespace llfix;

int main(int argc, char* argv[])
{
    if (argc < 2)
    {
        std::cout << "Usage : sequence_store_manager <sequence_store_path>\n";
        return -1;
    }

    std::string sequence_store_path = argv[1];

    if (FileSystemUtilities::does_file_exist(sequence_store_path) == false)
    {
        std::cout << sequence_store_path << " does not exist\n";
        return -1;
    }

    SequenceStore store;
    if (store.open(sequence_store_path) == false)
    {
        std::cout << "Failed to open " << sequence_store_path << "\n";
        return -2;
    }

    std::cout << "Succesfully opened " << sequence_store_path << " incoming sequence no : " << store.get_incoming_seq_no() << " outgoing sequence no : " << store.get_outgoing_seq_no() << "\n\n";

    std::string user_input;

    while (true)
    {
        std::cout << "Press 1 to view values\n";
        std::cout << "Press 2 to modify\n";
        std::cout << "Press v to display version\n";
        std::cout << "Press q to quit\n";

        cin >> (user_input);
        string lower_case_user_input = user_input;
        std::transform(lower_case_user_input.begin(), lower_case_user_input.end(), lower_case_user_input.begin(), ::tolower);

        if (lower_case_user_input[0] == 'q')
        {
            break;
        }
        else if (lower_case_user_input[0] == 'v')
        {
            std::cout << "\nVersion: " << VERSION << "\n\n";
        }
        else if (lower_case_user_input[0] == '1')
        {
            std::cout << "Incoming sequence no : " << store.get_incoming_seq_no() << "\n";
            std::cout << "Outgoing sequence no : " << store.get_outgoing_seq_no() << "\n";
        }
        else if (lower_case_user_input[0] == '2')
        {
            try
            {
                std::cout << "Enter incoming sequence no :";
                uint32_t incoming_seq_no = 0;
                cin >> incoming_seq_no;

                std::cout << "Enter outgoing sequence no :";
                uint32_t outgoing_seq_no = 0;
                cin >> outgoing_seq_no;

                store.set_incoming_seq_no(incoming_seq_no);
                store.set_outgoing_seq_no(outgoing_seq_no);
                store.save_to_disc();
            }
            catch(...)
            {}
        }
    }

    return 0;
}