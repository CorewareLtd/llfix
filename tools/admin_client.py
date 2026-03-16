'''
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
'''
#!/usr/bin/python
# llfix python admin client
import sys
import socket

VERSION = "1.0.0"

class utilities:
    RED = '\033[91m'
    BLUE = '\033[94m'
    YELLOW = '\033[93m'
    GREEN = '\033[92m'
    CONSOLE_END = '\033[0m'

    @staticmethod
    def print_coloured(message, colour_code, end='\n'):
        print(colour_code + message + utilities.CONSOLE_END, end=end)

    @staticmethod
    def print_red(message, end='\n'):
        utilities.print_coloured(message, utilities.RED, end=end)

    @staticmethod
    def print_blue(message, end='\n'):
        utilities.print_coloured(message, utilities.BLUE, end=end)

    @staticmethod
    def print_yellow(message, end='\n'):
        utilities.print_coloured(message, utilities.YELLOW, end=end)

    @staticmethod
    def print_green(message, end='\n'):
        utilities.print_coloured(message, utilities.GREEN, end=end)

class TCPSocket:
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

def displayUsage():
    print('usage : python admin_client.py <server> <port_number>')

def main():
    try:
        if len(sys.argv) != 3 :
            displayUsage()
            exit(-1)

        print("Version : " + VERSION + "\n")

        server = str(sys.argv[1])
        port_number = int(sys.argv[2])

        send_socket = TCPSocket()
        send_socket.connect(server, port_number)

        while True:
            utilities.print_red('Press q to quit' )
            utilities.print_red('Or enter command and press enter to send')

            ret = input(":")

            if ret == "q":
                break
            else:
                actual_command = ret + "|"
                send_socket.send(actual_command)
                utilities.print_blue('Sent command')
                response = ""
                while True:
                    ret = send_socket.receive(10240)
                    if ret is not None:
                        response += ret
                        if response.endswith('|'):
                            break
                    else:
                        utilities.print_red("Disconnection detected. Quitting.\n")
                        send_socket.close()
                        exit(0)

                if len(response)>0:
                    trimmed_response = response[:-1]
                    utilities.print_yellow('Response : ' + trimmed_response)

        send_socket.close()

    except ValueError as err:
        print(err.args)

if __name__ == "__main__":
    main()