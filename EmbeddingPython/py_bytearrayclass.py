'''py_class.py - Python source designed to demonstrate the use of python embedding'''

class ByteArrayClass:
    def __init__(self):
        self.a = 6
        self.b = 5
        self.buffer = bytearray([0x05, 0x04, 0x03, 0x02, 0x01, 0x00])

    def setByteArray(self, c1,c2,c3,c4,c5,c6, length):
        tmp_buffer = self.buffer
        self.buffer = bytearray([c1,c2,c3,c4,c5,c6])
        return tmp_buffer
