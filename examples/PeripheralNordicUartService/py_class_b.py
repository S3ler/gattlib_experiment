''' Demonstrate the use of python threading'''

import threading


newMessageArrived = False
newMessage = [0] * 20


def init():
    print 'printed from MyThread...'


class MyThread(threading.Thread):
    def __init__(self):
        threading.Thread.__init__(self)
    def run(self):
        init()
    def send_user_input(self, c1,c2,c3,c4,c5,c6,c7,c8,c9,c10,c11,c12,c13,c14,c15,c16,c17,c18,c19,length):
        to_send_buffer = bytearray([c1,c2,c3,c4,c5,c6,c7,c8,c9,c10,c11,c12,c13,c14,c15,c16,c17,c18,c19])
        to_send_length = length
        to_send_buffer = to_send_buffer[0:to_send_length]
        return True
    def is_new_message_arrived(self):
        global newMessageArrived
        return newMessageArrived
    def get_new_message(self):
        global newMessage
        return bytearray(newMessage)