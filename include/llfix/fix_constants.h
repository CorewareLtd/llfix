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

#include <cstdint>
#include <cstddef>

namespace llfix
{

namespace FixConstants
{
    static constexpr char FIX_EQUALS = '=';
    static constexpr char FIX_DELIMITER = ((char)1);
    static constexpr int  FIX_CHECKSUM_MODULO = 256;
    static constexpr int  FIX_CHECKSUM_LENGTH = 3;
    static constexpr char FIX_BOOLEAN_TRUE = 'Y';
    static constexpr char FIX_BOOLEAN_FALSE = 'N';
    // ERROR CODES
    static constexpr uint32_t  FIX_ERROR_CODE_INVALID_TAG_NUMBER = 0;
    static constexpr uint32_t  FIX_ERROR_CODE_REQUIRED_TAG_MISSING = 1;
    static constexpr uint32_t  FIX_ERROR_CODE_TAG_UNDEFINED_FOR_MSG_TYPE = 2;
    static constexpr uint32_t  FIX_ERROR_CODE_UNDEFINED_TAG = 3;
    static constexpr uint32_t  FIX_ERROR_CODE_TAG_WITHOUT_VALUE = 4;
    static constexpr uint32_t  FIX_ERROR_CODE_VALUE_INCORRECT_FOR_TAG = 5;
    static constexpr uint32_t  FIX_ERROR_CODE_FORMAT_INCORRECT_FOR_TAG = 6;
    static constexpr uint32_t  FIX_ERROR_CODE_FORMAT_DECRYPTION_PROBLEM = 7;
    static constexpr uint32_t  FIX_ERROR_CODE_FORMAT_SIGNATURE_PROBLEM = 8;
    static constexpr uint32_t  FIX_ERROR_CODE_COMPID_PROBLEM = 9;
    static constexpr uint32_t  FIX_ERROR_CODE_SENDING_TIME_ACCURACY_PROBLEM = 10;
    static constexpr uint32_t  FIX_ERROR_CODE_INVALID_MSG_TYPE = 11;
    static constexpr uint32_t  FIX_ERROR_CODE_XML_VALIDATION_ERROR = 12;
    static constexpr uint32_t  FIX_ERROR_CODE_TAG_APPEARS_MORE_THAN_ONCE = 13;
    static constexpr uint32_t  FIX_ERROR_CODE_TAG_OUT_OF_ORDER = 14;
    static constexpr uint32_t  FIX_ERROR_CODE_RG_OUT_OF_ORDER = 15;
    static constexpr uint32_t  FIX_ERROR_CODE_INCORRECT_NUMINGROUP = 16;
    static constexpr uint32_t  FIX_ERROR_CODE_NON_BINARY_VALUE_WITH_SOH = 17;
    static constexpr uint32_t  FIX_ERROR_CODE_OTHER = 99;
    static constexpr uint32_t  FIX_MAX_ERROR_CODE = FIX_ERROR_CODE_OTHER;
    // TAGS: HEADER & CHECKSUM
    static constexpr uint32_t TAG_BEGIN_STRING = 8;
    static constexpr uint32_t TAG_BODY_LENGTH = 9;
    static constexpr uint32_t TAG_MSG_SEQ_NUM = 34;
    static constexpr uint32_t TAG_MSG_TYPE = 35;
    static constexpr uint32_t TAG_SENDER_COMP_ID = 49;
    static constexpr uint32_t TAG_TARGET_COMP_ID = 56;
    static constexpr uint32_t TAG_SENDING_TIME = 52;
    static constexpr uint32_t TAG_CHECKSUM = 10;
    // TAGS: LOGON
    static constexpr uint32_t TAG_ENCRYPT_METHOD = 98;
    static constexpr uint32_t TAG_HEART_BT_INT = 108;
    static constexpr uint32_t TAG_DEFAULT_APPL_VER_ID = 1137;
    static constexpr uint32_t TAG_USERNAME = 553;
    static constexpr uint32_t TAG_PASSWORD = 554;
    static constexpr uint32_t TAG_NEW_PASSWORD = 925;
    static constexpr uint32_t TAG_RESET_SEQ_NUM_FLAG = 141;
    static constexpr uint32_t TAG_NEXT_EXPECTED_SEQ_NUM = 789;
    // TAGS: TEST REQUEST
    static constexpr uint32_t TAG_TEST_REQ_ID = 112;
    // TAGS: RESEND REQUEST
    static constexpr uint32_t TAG_BEGIN_SEQ_NO = 7;
    static constexpr uint32_t TAG_END_SEQ_NO = 16;
    static constexpr uint32_t TAG_NEW_SEQ_NO = 36;
    static constexpr uint32_t TAG_ORIG_SENDING_TIME = 122;
    static constexpr uint32_t TAG_GAP_FILL_FLAG = 123;
    static constexpr uint32_t TAG_POSS_DUP_FLAG = 43;
    static constexpr uint32_t TAG_POSS_RESEND = 97;
    // TAGS: REJECTION
    static constexpr uint32_t TAG_TEXT = 58;
    static constexpr uint32_t TAG_REF_SEQ_NUM = 45;
    static constexpr uint32_t TAG_REF_TAG = 371;
    static constexpr uint32_t TAG_REF_MSG_TYPE = 372;
    static constexpr uint32_t TAG_SESSION_REJECT_REASON = 373;
    // MSGTYPEs: ADMIN/SESSION
    static constexpr char MSG_TYPE_LOGON = 'A';
    static constexpr char MSG_TYPE_LOGOUT = '5';
    static constexpr char MSG_TYPE_HEARTBEAT = '0';
    static constexpr char MSG_TYPE_TEST_REQUEST = '1';
    static constexpr char MSG_TYPE_RESEND_REQUEST = '2';
    static constexpr char MSG_TYPE_SEQUENCE_RESET = '4';
    static constexpr char MSG_TYPE_REJECT = '3';
    // MSGTYPEs: APPLICATION
    static constexpr char MSG_TYPE_EXECUTION_REPORT = '8';
    static constexpr char MSG_TYPE_NEW_ORDER = 'D';
    static constexpr char MSG_TYPE_ORDER_CANCEL = 'F';
    static constexpr char MSG_TYPE_ORDER_CANCEL_REPLACE = 'G';
    static constexpr char MSG_TYPE_ORDER_CANCEL_REJECT = '9';
    static constexpr char MSG_TYPE_BUSINESS_REJECT = 'j';
    // CUSTOM FOR LLFIX
    static constexpr std::size_t MAX_SUPPORTED_MESSAGE_TYPE_LENGTH = 4;
} // namespace

} // namespace