from tinyfix import *
from datetime import datetime, timezone, timedelta

class InvalidFixTagValuePair:
    def __init__(self):
        self.tag=0
        self.value=""
        self.is_dirty=True
        
def fix_utc_timestamp_with_delta(delta=0):
    t = datetime.now(timezone.utc) - timedelta(seconds=delta)
    return t.strftime("%Y%m%d-%H:%M:%S.%f")[:-3]

class InvalidFixMessage:
    def __init__(self):
        # Using array to be able to support repeating groups
        self.msg_type = ""
        self.tag_values = []
        self.tag_values_index = 0
        #--------------------------------------
        self.inject_invalid_block = False
        self.exclude_tag_35 = False
        self.exclude_tag_8 = False
        self.exclude_tag_9 = False
        self.exclude_tag_34 = False
        self.body_length_delta = 0
        self.hardcoded_comp_id = ""
        self.hardcoded_target_comp_id = ""
        self.hardcoded_begin_string = ""
        self.sending_time_delta_seconds=0
        #--------------------------------------

        for i in range(512):
            pair = InvalidFixTagValuePair()
            pair.is_dirty = True
            self.tag_values.append(pair)

        self.encoded = ""

    def set_msg_type(self, value_string):
        self.msg_type = value_string

    def set_tag(self, tag_int, value_string):
        if len(self.tag_values) == self.tag_values_index:
            pair = InvalidFixTagValuePair()
            pair.is_dirty = True
            self.tag_values.append(pair)

        self.tag_values[self.tag_values_index].tag = tag_int
        self.tag_values[self.tag_values_index].value = value_string
        self.tag_values[self.tag_values_index].is_dirty = False
        self.tag_values_index += 1

    def get_tag_value(self, tag):
        for pair in self.tag_values:
            if pair.is_dirty == False:
                if tag == pair.tag:
                    return pair.value
        return None

    def has_tag(self, tag):
        for pair in self.tag_values:
            if pair.is_dirty == False:
                if tag == pair.tag:
                    return True
        return False

    def get_encoded(self):
        return self.encoded

    def calculate_body_length(self, msgtype_string, fix_session):
        length = 0

        if len(self.hardcoded_comp_id) == 0:
            length += 4 + len(fix_session.comp_id)                          #4-> 49 & = & delimiter
        else:
            length += 4 + len(self.hardcoded_comp_id)                #4-> 49 & = & delimiter

        if len(self.hardcoded_target_comp_id)==0:
            length += 4 + len(fix_session.target_comp_id)                   #4-> 56 & = & delimiter
        else:
            length += 4 + len(self.hardcoded_target_comp_id)         #4-> 56 & = & delimiter

        if self.exclude_tag_34 is False:
            length += 4 + len(str(fix_session.outgoing_seq_no))                 #4-> 34 & = & delimiter

        length += 4 + len(str(fix_utc_timestamp_with_delta(self.sending_time_delta_seconds)))   #4-> 52 & = & delimiter

        if self.exclude_tag_35 is False:
            length += 4 + len(msgtype_string)

        for pair in self.tag_values:
            if pair.is_dirty == False:
                length += 2 + len(str(pair.tag)) + len(str(pair.value)) #2-> = & delimiter

        if self.inject_invalid_block:
            length += 3

        length += self.body_length_delta

        return length

    def get_checksum_string(self):
        checksum_data = self.encoded.encode('ascii')
        checksum = sum(checksum_data) % 256
        return f"{checksum:03}"  # always 3-digit format with leading zeros

    def encode(self, fix_session):
        body_length = self.calculate_body_length(self.msg_type, fix_session)

        self.encoded = ""

        if self.exclude_tag_8 is False:
            if len(self.hardcoded_begin_string) == 0:
                self.encoded += "8=" + fix_session.begin_string + FixConstants.SOH
            else:
                self.encoded += "8=" + self.hardcoded_begin_string + FixConstants.SOH

        if self.exclude_tag_9 is False:
            self.encoded += "9=" + str(body_length) + FixConstants.SOH

        if self.exclude_tag_35 is False:
            self.encoded += "35=" + self.msg_type+ FixConstants.SOH

        if self.exclude_tag_34 is False:
            self.encoded += "34=" + str(fix_session.outgoing_seq_no) + FixConstants.SOH

        if len(self.hardcoded_comp_id) == 0:
            self.encoded += "49=" + fix_session.comp_id + FixConstants.SOH
        else:
            self.encoded += "49=" + self.hardcoded_comp_id + FixConstants.SOH

        if len(self.hardcoded_target_comp_id) == 0:
            self.encoded += "56=" + fix_session.target_comp_id + FixConstants.SOH
        else:
            self.encoded += "56=" + self.hardcoded_target_comp_id + FixConstants.SOH

        self.encoded += "52=" + fix_utc_timestamp_with_delta(self.sending_time_delta_seconds) + FixConstants.SOH

        for pair in self.tag_values:
            if pair.is_dirty == False:
                self.encoded +=  str(pair.tag) + FixConstants.EQUALS + str(pair.value) + FixConstants.SOH

        if self.inject_invalid_block:
            self.encoded += "42" + FixConstants.SOH

        self.encoded += "10=" + self.get_checksum_string() + FixConstants.SOH

        for pair in self.tag_values:
            pair.is_dirty = True

        self.tag_values_index = 0

    def decode_from(self, decode_string):
        self.decoded = decode_string
        parts = self.decoded.split(FixConstants.SOH)

        for part in parts:
            if FixConstants.EQUALS in part:
                tokens = part.split(FixConstants.EQUALS)
                current_tag = int(tokens[0])
                current_value = str(tokens[1])

                self.set_tag(int(current_tag), str(current_value))

    def to_string(self):
        ret = ""

        if len(self.encoded) > 0:
            ret = self.encoded
        elif self.decoded is not None:
            if len(self.decoded)>0:
                ret = self.decoded

        ret = FixUtils.fix_to_readable(ret)
        return ret