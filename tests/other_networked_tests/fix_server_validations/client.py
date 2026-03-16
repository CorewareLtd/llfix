#!/usr/bin/python
import time
import signal
from tinyfix import *

import sys
sys.path.append(os.path.abspath(os.path.join(os.path.dirname(__file__), '..')))
from invalid_fix_message import *

class MyClientHandler(FixClientHandler):
    def handle(self):
        while True:
            if self.fix_session.state == SessionState.DISCONNECTED:
                self.connect(5)

                if self.fix_session.state == SessionState.PENDING_LOGON:
                    self.send_logon()
            else:
                self.send_heartbeat_if_necessary()

                current_msg = self.get_next_fix_message()

                if current_msg is None:
                    continue

                print("Received: " + current_msg.to_string() + "\n")
                msg_type = current_msg.get_tag_value(35)

                if msg_type == "A":
                    self.fix_session.state = SessionState.LOGGED_ON
                    print("Logon accepted.\n")

                    self.send_invalid_messages()
                    self.send_invalid_dictionary_messages()

    def on_disconnection(self):
        print("Connection lost\n")

    def send_logon(self):
        logon = FixMessage()
        logon.set_msg_type("A")
        logon.set_tag(1137, "7")
        logon.set_tag(98, "0")
        logon.set_tag(108, str(self.fix_session.heartbeat_interval))

        self.send(logon)

    def send_heartbeat_if_necessary(self):
        if time.time() - self.fix_session.last_sent_time >= self.fix_session.heartbeat_interval:
            hb = FixMessage()
            hb.set_msg_type("0")

            self.send(hb)
            print("Sent Heartbeat.\n")

    #----------------------------------------------------------------------------------------------------------------------------
    def send_invalid_messages(self):
        self.send_msg_with_low_body_length()
        self.send_msg_with_high_body_length()

        self.send_msg_with_invalid_checksum()

        self.send_msg_with_stale_timestamp()
        self.send_msg_with_empty_field()
        self.send_msg_with_duplicate_field()
        self.send_msg_with_invalid_begin_string()

        self.send_msg_with_invalid_comp_id()
        self.send_msg_with_invalid_target_comp_id()

        self.send_msg_with_no_t8()
        self.send_msg_with_no_t9()
        self.send_msg_with_no_t35()
        self.send_msg_with_no_t34()

        self.send_msg_with_non_numeric_tag()
        self.send_msg_with_with_no_equals_sign_block()

        self.send_repeating_group_without_leading_tag()
        self.send_repeating_group_with_duplicate_leading_tag()
        #self.send_repeating_group_with_low_leading_tag()
        #self.send_repeating_group_with_high_leading_tag()
        #self.send_repeating_group_with_missing_member_tag()
        #self.send_repeating_group_with_extra_member_tag()
        #self.send_repeating_group_with_incorrect_order()

        self.send_invalid_sequence_reset_message()

    def send_msg_with_invalid_begin_string(self):
        fix_message = InvalidFixMessage()
        fix_message.hardcoded_begin_string = "FIX.4.0"

        fix_message.set_msg_type("0")

        self.fix_session.increment_outgoing_seq_no()
        fix_message.encode(self.fix_session)

        self.sock.sendall(fix_message.encoded.encode())
        self.fix_session.update_last_sent_time()

    def send_msg_with_invalid_comp_id(self):
        fix_message = InvalidFixMessage()
        fix_message.hardcoded_comp_id = "blabla"

        fix_message.set_msg_type("0")

        self.fix_session.increment_outgoing_seq_no()
        fix_message.encode(self.fix_session)

        self.sock.sendall(fix_message.encoded.encode())
        self.fix_session.update_last_sent_time()

    def send_msg_with_invalid_target_comp_id(self):
        fix_message = InvalidFixMessage()
        fix_message.hardcoded_target_comp_id = "blabla"

        fix_message.set_msg_type("0")

        self.fix_session.increment_outgoing_seq_no()
        fix_message.encode(self.fix_session)

        self.sock.sendall(fix_message.encoded.encode())
        self.fix_session.update_last_sent_time()

    def send_msg_with_invalid_checksum(self):
        fix_message = FixMessage()
        fix_message.set_msg_type("0")

        self.fix_session.increment_outgoing_seq_no()
        fix_message.encode(self.fix_session)
        ######################################################
        encoded_len = len(fix_message.encoded)
        fix_message.encoded = fix_message.encoded[:encoded_len-2] + '0' + fix_message.encoded[encoded_len-1:]
        ######################################################
        self.sock.sendall(fix_message.get_encoded().encode())
        self.fix_session.update_last_sent_time()

    def send_msg_with_with_no_equals_sign_block(self):
        fix_message = InvalidFixMessage()
        fix_message.inject_invalid_block = True

        fix_message.set_msg_type("0")

        self.fix_session.increment_outgoing_seq_no()
        fix_message.encode(self.fix_session)

        self.sock.sendall(fix_message.encoded.encode())
        self.fix_session.update_last_sent_time()

    def send_msg_with_high_body_length(self):
        fix_message = InvalidFixMessage()
        fix_message.body_length_delta = 1

        fix_message.set_msg_type("0")

        self.fix_session.increment_outgoing_seq_no()
        fix_message.encode(self.fix_session)

        self.sock.sendall(fix_message.encoded.encode())
        self.fix_session.update_last_sent_time()

    def send_msg_with_low_body_length(self):
        fix_message = InvalidFixMessage()
        fix_message.body_length_delta = -1

        fix_message.set_msg_type("0")

        self.fix_session.increment_outgoing_seq_no()
        fix_message.encode(self.fix_session)

        self.sock.sendall(fix_message.encoded.encode())
        self.fix_session.update_last_sent_time()

    def send_msg_with_no_t8(self):
        fix_message = InvalidFixMessage()
        fix_message.exclude_tag_8 = True

        fix_message.set_msg_type("0")

        self.fix_session.increment_outgoing_seq_no()
        fix_message.encode(self.fix_session)

        self.sock.sendall(fix_message.encoded.encode())
        self.fix_session.update_last_sent_time()

    def send_msg_with_no_t34(self):
        fix_message = InvalidFixMessage()
        fix_message.exclude_tag_34 = True

        fix_message.set_msg_type("0")

        self.fix_session.increment_outgoing_seq_no()
        fix_message.encode(self.fix_session)

        self.sock.sendall(fix_message.encoded.encode())
        self.fix_session.update_last_sent_time()

    def send_msg_with_no_t35(self):
        fix_message = InvalidFixMessage()
        fix_message.exclude_tag_35 = True

        fix_message.set_msg_type("0")

        self.fix_session.increment_outgoing_seq_no()
        fix_message.encode(self.fix_session)

        self.sock.sendall(fix_message.encoded.encode())
        self.fix_session.update_last_sent_time()

    def send_msg_with_no_t9(self):
        fix_message = InvalidFixMessage()
        fix_message.exclude_tag_9 = True

        fix_message.set_msg_type("0")

        self.fix_session.increment_outgoing_seq_no()
        fix_message.encode(self.fix_session)

        self.sock.sendall(fix_message.encoded.encode())
        self.fix_session.update_last_sent_time()

    def send_msg_with_stale_timestamp(self):
        fix_message = InvalidFixMessage()
        fix_message.set_msg_type("0")

        ######################################################
        fix_message.sending_time_delta_seconds = 600
        ######################################################

        self.fix_session.increment_outgoing_seq_no()
        fix_message.encode(self.fix_session)

        self.sock.sendall(fix_message.get_encoded().encode())
        self.fix_session.update_last_sent_time()

    def send_msg_with_empty_field(self):
        fix_message = FixMessage()
        fix_message.set_msg_type("0")

        ######################################################
        fix_message.set_tag(112, "")
        ######################################################

        self.fix_session.increment_outgoing_seq_no()
        fix_message.encode(self.fix_session)

        self.sock.sendall(fix_message.get_encoded().encode())
        self.fix_session.update_last_sent_time()

    def send_msg_with_duplicate_field(self):
        fix_message = FixMessage()
        fix_message.set_msg_type("0")

        ######################################################
        fix_message.set_tag(112, "1")
        fix_message.set_tag(112, "1")
        ######################################################

        self.fix_session.increment_outgoing_seq_no()
        fix_message.encode(self.fix_session)

        self.sock.sendall(fix_message.get_encoded().encode())
        self.fix_session.update_last_sent_time()

    def send_msg_with_non_numeric_tag(self):
        fix_message = FixMessage()
        fix_message.set_msg_type("0")

        ######################################################
        fix_message.set_tag("x", "5")
        ######################################################

        self.fix_session.increment_outgoing_seq_no()
        fix_message.encode(self.fix_session)

        self.sock.sendall(fix_message.get_encoded().encode())
        self.fix_session.update_last_sent_time()

    def send_repeating_group_without_leading_tag(self):
        msg = FixMessage()
        msg.set_msg_type("0")

        # Repeating group 600
        msg.set_tag(600, "1")

        msg.set_tag(601, "a")
        msg.set_tag(602, "b")
        msg.set_tag(603, "c")

        # Repeating group 453
        #msg.set_tag(453, "2")

        msg.set_tag(448, "PARTY1")
        msg.set_tag(447, "D")
        msg.set_tag(452, "1")

        msg.set_tag(448, "PARTY2")
        msg.set_tag(447, "E")
        msg.set_tag(452, "2")

        self.send(msg)

    def send_repeating_group_with_duplicate_leading_tag(self):
        msg = FixMessage()
        msg.set_msg_type("0")

        # Repeating group 600
        msg.set_tag(600, "1")

        msg.set_tag(601, "a")
        msg.set_tag(602, "b")
        msg.set_tag(603, "c")

        # Repeating group 453
        msg.set_tag(453, "2")
        msg.set_tag(453, "2")

        msg.set_tag(448, "PARTY1")
        msg.set_tag(447, "D")
        msg.set_tag(452, "1")

        msg.set_tag(448, "PARTY2")
        msg.set_tag(447, "E")
        msg.set_tag(452, "2")

        self.send(msg)

    def send_repeating_group_with_low_leading_tag(self):
        msg = FixMessage()
        msg.set_msg_type("0")

        # Repeating group 600
        msg.set_tag(600, "1")

        msg.set_tag(601, "a")
        msg.set_tag(602, "b")
        msg.set_tag(603, "c")

        # Repeating group 453
        msg.set_tag(453, "1")

        msg.set_tag(448, "PARTY1")
        msg.set_tag(447, "D")
        msg.set_tag(452, "1")

        msg.set_tag(448, "PARTY2")
        msg.set_tag(447, "E")
        msg.set_tag(452, "2")

        self.send(msg)

    def send_repeating_group_with_high_leading_tag(self):
        msg = FixMessage()
        msg.set_msg_type("0")

        # Repeating group 600
        msg.set_tag(600, "1")

        msg.set_tag(601, "a")
        msg.set_tag(602, "b")
        msg.set_tag(603, "c")

        # Repeating group 453
        msg.set_tag(453, "3")

        msg.set_tag(448, "PARTY1")
        msg.set_tag(447, "D")
        msg.set_tag(452, "1")

        msg.set_tag(448, "PARTY2")
        msg.set_tag(447, "E")
        msg.set_tag(452, "2")

        self.send(msg)

    def send_repeating_group_with_missing_member_tag(self):
        msg = FixMessage()
        msg.set_msg_type("0")

        # Repeating group 600
        msg.set_tag(600, "1")

        msg.set_tag(601, "a")
        #msg.set_tag(602, "b")
        msg.set_tag(603, "c")

        # Repeating group 453
        msg.set_tag(453, "2")

        msg.set_tag(448, "PARTY1")
        msg.set_tag(447, "D")
        msg.set_tag(452, "1")

        msg.set_tag(448, "PARTY2")
        msg.set_tag(447, "E")
        msg.set_tag(452, "2")

        self.send(msg)

    def send_repeating_group_with_extra_member_tag(self):
        msg = FixMessage()
        msg.set_msg_type("0")

        # Repeating group 600
        msg.set_tag(600, "1")

        msg.set_tag(601, "a")
        msg.set_tag(602, "b")
        msg.set_tag(603, "c")

        # Repeating group 453
        msg.set_tag(453, "2")

        msg.set_tag(448, "PARTY1")
        msg.set_tag(447, "D")
        msg.set_tag(452, "1")

        msg.set_tag(448, "PARTY2")
        msg.set_tag(447, "E")
        msg.set_tag(452, "2")

        msg.set_tag(448, "PARTY3")

        self.send(msg)

    def send_repeating_group_with_incorrect_order(self):
        msg = FixMessage()
        msg.set_msg_type("0")

        # Repeating group 600
        msg.set_tag(600, "1")

        msg.set_tag(601, "a")
        msg.set_tag(602, "b")
        msg.set_tag(603, "c")

        # Repeating group 453
        msg.set_tag(453, "2")

        msg.set_tag(447, "D")
        msg.set_tag(452, "1")
        msg.set_tag(448, "PARTY1")

        msg.set_tag(452, "2")
        msg.set_tag(447, "E")
        msg.set_tag(448, "PARTY2")

        self.send(msg)

    def send_invalid_sequence_reset_message(self):
        self.fix_session.outgoing_seq_no = 0

        msg = FixMessage()
        msg.set_msg_type("4")
        msg.set_tag(36, "1")

        self.send(msg)
    #----------------------------------------------------------------------------------------------------------------------------
    # INVALID MESSAGES ( INVALID IN DICTIONARY )
    def send_invalid_dictionary_messages(self):
        self.send_invalid_msg_type()
        self.send_msg_with_required_tag_missing()
        self.send_msg_with_unassociated_tag()
        self.send_msg_with_undefined_tag()
        self.send_msg_with_non_allowed_value()
        self.send_msg_with_incorrect_value_format()
        self.send_msg_with_missing_repeating_group_count_tag()
        self.send_msg_with_duplicate_repeating_group_count_tag()
        self.send_msg_with_invalid_repeating_group_count_tag()

    def send_invalid_msg_type(self):
        fix_message = FixMessage()
        fix_message.set_msg_type("X42")
        self.send(fix_message)

    def send_msg_with_required_tag_missing(self):
        fix_message = FixMessage()
        fix_message.set_msg_type("D")
        self.send(fix_message)

    def send_msg_with_unassociated_tag(self):
        fix_message = FixMessage()
        fix_message.set_msg_type("0")
        fix_message.set_tag(54, "1")
        self.send(fix_message)

    def send_msg_with_undefined_tag(self):
        fix_message = FixMessage()
        fix_message.set_msg_type("0")
        fix_message.set_tag(5555, "J")
        self.send(fix_message)

    def send_msg_with_non_allowed_value(self):
        fix_message = FixMessage()
        fix_message.set_msg_type("D")
        fix_message.set_tag(11, "AAAAA1")
        fix_message.set_tag(60, self.fix_session.get_current_datetime_string())
        fix_message.set_tag(40, "2")
        fix_message.set_tag(54, "X")
        self.send(fix_message)

    def send_msg_with_incorrect_value_format(self):
        fix_message = FixMessage()
        fix_message.set_msg_type("D")
        fix_message.set_tag(11, "AAAAA1")
        fix_message.set_tag(60, "abc")
        fix_message.set_tag(40, "2")
        fix_message.set_tag(54, "1")
        self.send(fix_message)

    def send_msg_with_missing_repeating_group_count_tag(self):
        fix_message = FixMessage()
        fix_message.set_msg_type("D")
        fix_message.set_tag(11, "AAAAA1")
        fix_message.set_tag(60, self.fix_session.get_current_datetime_string())
        fix_message.set_tag(40, "2")
        fix_message.set_tag(54, "1")
        self.send(fix_message)

    def send_msg_with_duplicate_repeating_group_count_tag(self):
        fix_message = FixMessage()
        fix_message.set_msg_type("D")
        fix_message.set_tag(11, "AAAAA1")
        fix_message.set_tag(60, self.fix_session.get_current_datetime_string())
        fix_message.set_tag(40, "2")
        fix_message.set_tag(54, "1")
        fix_message.set_tag(453, "1")
        fix_message.set_tag(453, "1")
        self.send(fix_message)

    def send_msg_with_invalid_repeating_group_count_tag(self):
        fix_message = FixMessage()
        fix_message.set_msg_type("D")
        fix_message.set_tag(11, "AAAAA1")
        fix_message.set_tag(60, self.fix_session.get_current_datetime_string())
        fix_message.set_tag(40, "2")
        fix_message.set_tag(54, "1")
        fix_message.set_tag(453, "x")
        self.send(fix_message)

def signal_handler(signal, frame):
        print('You pressed Ctrl+C!')
        sys.exit(0)

def main():
    try:
        signal.signal(signal.SIGINT, signal_handler)

        # FIX SESSION
        fix_session = FixSession()
        fix_session.begin_string = "FIXT.1.1"
        fix_session.comp_id = "CLIENT1"
        fix_session.target_comp_id = "EXECUTOR"
        fix_session.endpoint_address = "127.0.0.1"
        fix_session.port = 5001

        # FIX CLIENT
        client = FixClient(fix_session, MyClientHandler)
        client.start()

        while True:
            time.sleep(1)

    except ValueError as err:
        print(err.args)

#Entry point
if __name__ == "__main__":
    main()