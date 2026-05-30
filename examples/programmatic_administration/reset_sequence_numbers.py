#!/usr/bin/python
import sys
import socket

class LlfixAdminClient:
    def __init__(self):
        self.socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.port_number = ""
        self.endpoint_address = ""

    def connect(self, endpoint_address, port_number):
        self.endpoint_address = endpoint_address
        self.port_number = port_number
        self.socket.setsockopt( socket.SOL_SOCKET, socket.SO_KEEPALIVE, 1)
        self.socket.connect((self.endpoint_address, self.port_number))

    def close(self):
        self.socket.close()

    def send(self, message):
        try:
            if sys.version_info > (2, 7):
                self.socket.sendto(message.encode(), (self.endpoint_address, self.port_number))
            else:
                self.socket.send(message, (self.endpoint_address, self.port_number))
        except (BrokenPipeError, ConnectionResetError, ConnectionAbortedError, OSError) as e:
            print(f"Disconnected while sending: {e}")

    def receive(self, buffer_size=1024):
        try:
            data = self.socket.recv(buffer_size)
            if sys.version_info > (2, 7):
                return data.decode('utf-8', errors='replace')
            else:
                return data
        except (socket.error, OSError) as e:
            print(f"Socket error: {e}")
            return None

    def send_command(self, command):
        actual_command = command + "|"
        self.send(actual_command)

        response = ""
        while True:
            ret = self.receive(10240)
            if ret is not None:
                response += ret
                if response.endswith('|'):
                    break
            else:
                print("Disconnection detected.\n")
                self.close()

        if len(response)>0:
                    response = response[:-1]
        return response

def displayUsage():
    print('usage : python admin_client.py <server> <port_number> <instance_name> <session_name>')

def main():
    try:
        if len(sys.argv) != 5 :
            displayUsage()
            exit(-1)

        server = str(sys.argv[1])
        port_number = int(sys.argv[2])
        instance_name = str(sys.argv[3])
        session_name = str(sys.argv[4])

        llfix_admin_client = LlfixAdminClient()
        llfix_admin_client.connect(server, port_number)

        # SET INCOMING
        ret = llfix_admin_client.send_command("set_incoming_sequence_number " + instance_name + " " + session_name + " 0")
        print("Incoming seq no command result : " + ret + "\n")

        # SET OUTGOING
        ret = llfix_admin_client.send_command("set_outgoing_sequence_number " + instance_name + " " + session_name + " 0")
        print("Outgoing seq no command result : " + ret + "\n")

        llfix_admin_client.close()

    except ValueError as err:
        print(err.args)

if __name__ == "__main__":
    main()