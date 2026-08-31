import struct
import numpy as np
from enum import Enum

# DcMsg message is a bytes array with a header and a succession of DcMsg elements
#    HEADER   |    DcMsg     | DcMsg ...
#  3 uint64   | min 18 bytes | min 18 bytes

# HEADER
#  VERSION |                   SIZE                 |        ELEMENTS
#  uint64  |                  uint64                |         uint64
#   1000   | total size in bytes (including header) | no of DcMsg elements

# DcMsg scalar element (TypeCode BOOL to DOUBLE)
#   TYPECODE     |          NAME            |  VALUE
#    1 byte      |       16 bytes           | 1 byte (BOOL, UBYTE, BYTE) or 4 bytes (UINT, INT, FLOAT) or 8 bytes (ULONG, LONG, DOUBLE)
# TypeCode enum  | max 15 char + null term  |  value

# DcMsg variable-length element (TypeCode STRING, MESSAGE or MEMORYARRAY)
#   TYPECODE     |          NAME            |      SIZE     |    VALUE
#    1 byte      |       16 bytes           |    4 bytes    |  SIZE bytes
# TypeCode enum  | max 15 char + null term  | size (uint32) |    bytes
# string values are not null terminated

# DcMsg fixed-element array (TypeCode BOOLARRAY .. DOUBLEARRAY) - VALUE is COUNT * element size raw bytes
#   TYPECODE     |          NAME            |      SIZE     |    VALUE
#    1 byte      |       16 bytes           |    4 bytes    |  SIZE bytes
# TypeCode enum  | max 15 char + null term  | size (uint32) |    packed elements

# DcMsg object array element (TypeCode STRINGARRAY or MESSAGEARRAY) - each item is length-prefixed
#   TYPECODE     |          NAME            |  TOTAL SIZE   |    NO ITEMS   |  ITEM1 SIZE   | ITEM1 PAYLOAD
#    1 byte      |       16 bytes           |    4 bytes    |    4 bytes    |     4 bytes   |
# TypeCode enum  | max 15 char + null term  | size (uint32) | size (uint32) | size (uint32) |    bytes
# string values are not null terminated

_HEADER_ELEMENTS = 3
_MESS_VERSION = 1000
_NAME_SIZE = 16
_NUMBER_OF_TYPES = 24

class TypeCode(Enum):
    NA           = 0
    BOOL         = 1
    UBYTE        = 2
    BYTE         = 3
    UINT         = 4
    INT          = 5
    ULONG        = 6
    LONG         = 7
    FLOAT        = 8
    DOUBLE       = 9
    STRING       = 10
    MESSAGE      = 11
    MEMORYARRAY  = 12
    BOOLARRAY    = 13
    UBYTEARRAY   = 14
    BYTEARRAY    = 15
    UINTARRAY    = 16
    INTARRAY     = 17
    ULONGARRAY   = 18
    LONGARRAY    = 19
    FLOATARRAY   = 20
    DOUBLEARRAY  = 21
    STRINGARRAY  = 22
    MESSAGEARRAY = 23

SType = [
    0,  # NA
    1,  # BOOL
    1,  # UBYTE
    1,  # BYTE
    4,  # UINT
    4,  # INT
    8,  # ULONG
    8,  # LONG
    4,  # FLOAT
    8,  # DOUBLE
    0,  # STRING
    0,  # MESSAGE
    0,  # MEMORYARRAY
    0,  # BOOLARRAY
    0,  # UBYTEARRAY
    0,  # BYTEARRAY
    0,  # UINTARRAY
    0,  # INTARRAY
    0,  # ULONGARRAY
    0,  # LONGARRAY
    0,  # FLOATARRAY
    0,  # DOUBLEARRAY
    0,  # STRINGARRAY
    0   # MESSAGEARRAY
]

class DcMsg:
    def __init__(self, buff = None):
        self.read_only = False
        self.p_elem = dict()
        # self.p_indexes = []
        self.index = 0
        self.__init_header()
        if isinstance(buff, dict):
            for k, v in buff.items():
                self.AddValue(k, v)
        elif isinstance(buff, bytes):
            buff_l = len(buff)
            header_size = _HEADER_ELEMENTS * 8
            if buff_l < header_size:
                raise Exception(f'Unexpected argument size. Expected minumum {header_size} bytes but the argument have only {buff_l} bytes')
            self.p_header = struct.unpack(f'{_HEADER_ELEMENTS}Q', buff[: header_size])
            if buff_l != self.p_header[1]:
                raise Exception(f'Unexpected argument size. Expected {self.p_header[1]} bytes but the argument have only {buff_l} bytes')
            self.index = header_size
            if not self.__ParseBuffer(buff):
                raise Exception('Error parsing the data')
            self.read_only = True

    def __init_header(self):
        self.p_header = np.zeros(_HEADER_ELEMENTS, dtype = np.uint64)
        self.p_header[0] = _MESS_VERSION
        self.p_header[1] = _HEADER_ELEMENTS * 8
        self.p_data = bytearray()
        self.index = self.p_header[1]

    def __Name(self, name: str):
        nm = name.encode('utf-8')[:_NAME_SIZE - 1] + b'\0'
        return nm.ljust(_NAME_SIZE, b'\0')

    def __AddElement(self, name, code, value, value_b):
        if self.read_only:
            print("Read only DcMsg. The element ", name, " was not added")
            return False
        if name in self.p_elem.keys():
            print("The element ", name, " already exist and was not added")
            return False
        self.p_elem[name] = value
        element_b = struct.pack('B', code.value) + self.__Name(name) + value_b
        len_el = len(element_b)
        self.p_data.extend(element_b)
        # self.p_indexes.append(self.index)
        self.index += len_el
        self.p_header[1] += len_el
        self.p_header[2] += 1
        return True

    def __AddFixedArray(self, name, code, wire_arr, stored_value = None):
        payload = wire_arr.tobytes()
        if stored_value is None:
            stored_value = wire_arr
        return self.__AddElement(name, code, stored_value, struct.pack('I', len(payload)) + payload)

    def __ParseBuffer(self, buff):
        while self.index < self.p_header[1]:
            if self.p_header[1] - self.index < _NAME_SIZE + 1:
                print(f'Data truncated for the element {self.p_header[2]} (impossible to recover the type and the name)!')
                return False
            ty = buff[self.index]
            if ty < 1 or ty >= _NUMBER_OF_TYPES:
                print(f'[DcMsg] Unknown type for the element {self.p_header[2]}!')
                return False
            self.index += 1
            name_b = buff[self.index: self.index + _NAME_SIZE]
            name = name_b.split(b'\0', 1)[0].decode()
            self.index += _NAME_SIZE
            dtype = TypeCode(ty)
            size = SType[ty]
            if not size:
                if self.p_header[1] - self.index < 4:
                    print(f'Data truncated for the element {self.p_header[2]} (missing or incomplete element size)!')
                    return False
                size = struct.unpack_from('I', buff, self.index)[0]
                self.index += 4
            if self.p_header[1] - self.index < size:
                print(f'Data truncated for the element {self.p_header[2]}. Expected {size} bytes, but only {self.p_header[1] - self.index} bytes are available!')
                return False

            # Read the data
            if dtype == TypeCode.BOOL:
                value = np.bool_(struct.unpack_from('?', buff, self.index)[0])
            elif dtype == TypeCode.UBYTE:
                value = np.uint8(struct.unpack_from('B', buff, self.index)[0])
            elif dtype == TypeCode.BYTE:
                value = np.int8(struct.unpack_from('b', buff, self.index)[0])
            elif dtype == TypeCode.UINT:
                value = np.uint32(struct.unpack_from('I', buff, self.index)[0])
            elif dtype == TypeCode.INT:
                value = np.int32(struct.unpack_from('i', buff, self.index)[0])
            elif dtype == TypeCode.ULONG:
                value = np.uint64(struct.unpack_from('Q', buff, self.index)[0])
            elif dtype == TypeCode.LONG:
                value = np.int64(struct.unpack_from('q', buff, self.index)[0])
            elif dtype == TypeCode.FLOAT:
                value = np.float32(struct.unpack_from('f', buff, self.index)[0])
            elif dtype == TypeCode.DOUBLE:
                value = np.float64(struct.unpack_from('d', buff, self.index)[0])
            elif dtype == TypeCode.STRING:
                value = struct.unpack_from(f'{size}s', buff, self.index)[0].decode('utf-8')
            elif dtype == TypeCode.MESSAGE:
                value = DcMsg(buff[self.index: self.index + size])
            elif dtype == TypeCode.MEMORYARRAY:
                value = np.frombuffer(buff, dtype=np.uint8, count=size, offset=self.index).copy()
            elif dtype == TypeCode.BOOLARRAY:
                value = np.frombuffer(buff, dtype=np.uint8, count=size, offset=self.index).astype(bool)
            elif dtype == TypeCode.UBYTEARRAY:
                value = np.frombuffer(buff, dtype=np.uint8, count=size, offset=self.index).copy()
            elif dtype == TypeCode.BYTEARRAY:
                value = np.frombuffer(buff, dtype=np.int8, count=size, offset=self.index).copy()
            elif dtype == TypeCode.UINTARRAY:
                value = np.frombuffer(buff, dtype=np.uint32, count=size // 4, offset=self.index).copy()
            elif dtype == TypeCode.INTARRAY:
                value = np.frombuffer(buff, dtype=np.int32, count=size // 4, offset=self.index).copy()
            elif dtype == TypeCode.ULONGARRAY:
                value = np.frombuffer(buff, dtype=np.uint64, count=size // 8, offset=self.index).copy()
            elif dtype == TypeCode.LONGARRAY:
                value = np.frombuffer(buff, dtype=np.int64, count=size // 8, offset=self.index).copy()
            elif dtype == TypeCode.FLOATARRAY:
                value = np.frombuffer(buff, dtype=np.float32, count=size // 4, offset=self.index).copy()
            elif dtype == TypeCode.DOUBLEARRAY:
                value = np.frombuffer(buff, dtype=np.float64, count=size // 8, offset=self.index).copy()
            elif dtype == TypeCode.STRINGARRAY:
                pos = self.index
                count = struct.unpack_from('I', buff, pos)[0]
                pos += 4
                items = []
                for _ in range(count):
                    item_len = struct.unpack_from('I', buff, pos)[0]
                    pos += 4
                    items.append(buff[pos: pos + item_len].decode('utf-8'))
                    pos += item_len
                value = items
            elif dtype == TypeCode.MESSAGEARRAY:
                pos = self.index
                count = struct.unpack_from('I', buff, pos)[0]
                pos += 4
                items = []
                for _ in range(count):
                    msg_size = struct.unpack_from('I', buff, pos)[0]
                    pos += 4
                    items.append(DcMsg(buff[pos: pos + msg_size]))
                    pos += msg_size
                value = items
            else:
                print(f'Unknown type for the element {self.p_header[2]}!')
                return False
            self.index += size
            self.p_elem[name] = value
        el_s = len(self.p_elem)
        if el_s != self.p_header[2]:
            print(f'Unexpected number of elements. Found {el_s}, but found {self.p_header[2]}!')
            return False
        return True

    def Clear(self):
        self.p_elem.clear()
        # self.p_indexes.clear()
        self.index = 0
        self.__init_header()
        self.read_only = False

    def AddValue(self, name: str, value) -> TypeCode:
        if isinstance(value, bool):
            val = np.bool_(value)
            return self.__AddElement(name, TypeCode.BOOL, val, struct.pack('?', val))
        elif isinstance(value, int):
            if value >= 0:
                if value <= 0xFF:
                    val = np.uint8(value)
                    return self.__AddElement(name, TypeCode.UBYTE, val, struct.pack('B', val))
                elif value <= 0xFFFFFFFF:
                    val = np.uint32(value)
                    return self.__AddElement(name, TypeCode.UINT, val, struct.pack('I', val))
                else:
                    val = np.uint64(value)
                    return self.__AddElement(name, TypeCode.ULONG, val, struct.pack('Q', val))
            else:
                if value >= -128:
                    val = np.int8(value)
                    return self.__AddElement(name, TypeCode.BYTE, val, struct.pack('b', val))
                elif value >= -2147483648:
                    val = np.int32(value)
                    return self.__AddElement(name, TypeCode.INT, val, struct.pack('i', val))
                else:
                    val = np.int64(value)
                    return self.__AddElement(name, TypeCode.LONG, val, struct.pack('q', val))
        elif isinstance(value, float):
            val = np.float64(value)
            return self.__AddElement(name, TypeCode.DOUBLE, val, struct.pack('d', val))
        elif isinstance(value, np.generic):
            dtype = value.dtype
            if dtype == np.bool_:
                return self.__AddElement(name, TypeCode.BOOL, value, struct.pack('?', value))
            elif dtype == np.uint8:
                return self.__AddElement(name, TypeCode.UBYTE, value, struct.pack('B', value))
            elif dtype == np.int8:
                return self.__AddElement(name, TypeCode.BYTE, value, struct.pack('b', value))
            elif dtype == np.uint32:
                return self.__AddElement(name, TypeCode.UINT, value, struct.pack('I', value))
            elif dtype == np.int32:
                return self.__AddElement(name, TypeCode.INT, value, struct.pack('i', value))
            elif dtype == np.uint64:
                return self.__AddElement(name, TypeCode.ULONG, value, struct.pack('Q', value))
            elif dtype == np.int64:
                return self.__AddElement(name, TypeCode.LONG, value, struct.pack('q', value))
            elif dtype == np.float32:
                return self.__AddElement(name, TypeCode.FLOAT, value, struct.pack('f', value))
            elif dtype == np.float64:
                return self.__AddElement(name, TypeCode.DOUBLE, value, struct.pack('d', value))
            else:
                raise ValueError(f'Unsupported type: {type(value)}')
        elif isinstance(value, str):
            value_e = value.encode('utf-8')
            length = len(value_e)
            return self.__AddElement(name, TypeCode.STRING, value, struct.pack(f'I{length}s', length, value_e))
        elif isinstance(value, DcMsg):
            return self.AddMessage(name, value)
        elif isinstance(value, (bytes, bytearray)):
            return self.AddMemoryArray(name, value)
        elif isinstance(value, np.ndarray) and value.dtype in (np.uint8, np.int8):
            if value.ndim != 1:
                raise ValueError(f"Only 1D byte arrays are supported, got shape {value.shape}")
            return self.AddMemoryArray(name, value)
        else:
            raise ValueError(f'Unsupported type: {type(value)}')
        return False

    def AddBool(self, name: str, value) -> TypeCode:
        val = np.bool_(value)
        return self.__AddElement(name, TypeCode.BOOL, val, struct.pack('?', val))

    def AddUByte(self, name: str, value) -> TypeCode:
        val = np.uint8(value)
        return self.__AddElement(name, TypeCode.UBYTE, val, struct.pack('B', val))

    def AddByte(self, name: str, value) -> TypeCode:
        val = np.int8(value)
        return self.__AddElement(name, TypeCode.BYTE, val, struct.pack('b', val))

    def AddUInt(self, name: str, value) -> TypeCode:
        val = np.uint32(value)
        return self.__AddElement(name, TypeCode.UINT, val, struct.pack('I', val))

    def AddInt(self, name: str, value) -> TypeCode:
        val = np.int32(value)
        return self.__AddElement(name, TypeCode.INT, val, struct.pack('i', val))

    def AddULong(self, name: str, value) -> TypeCode:
        val = np.uint64(value)
        return self.__AddElement(name, TypeCode.ULONG, val, struct.pack('Q', val))

    def AddLong(self, name: str, value) -> TypeCode:
        val = np.int64(value)
        return self.__AddElement(name, TypeCode.LONG, val, struct.pack('q', val))

    def AddFloat(self, name: str, value) -> TypeCode:
        val = np.float32(value)
        return self.__AddElement(name, TypeCode.FLOAT, val, struct.pack('f', val))

    def AddDouble(self, name: str, value) -> TypeCode:
        val = np.float64(value)
        return self.__AddElement(name, TypeCode.DOUBLE, val, struct.pack('d', val))

    def AddString(self, name: str, value) -> TypeCode:
        value_e = value.encode('utf-8')
        length = len(value_e)
        return self.__AddElement(name, TypeCode.STRING, value, struct.pack(f'I{length}s', length, value_e))

    def AddMessage(self, name: str, value: 'DcMsg') -> TypeCode:
        payload = bytes(value.GetData())
        return self.__AddElement(name, TypeCode.MESSAGE, value, struct.pack('I', len(payload)) + payload)

    def AddMemoryArray(self, name: str, value) -> TypeCode:
        if isinstance(value, (bytes, bytearray)):
            arr = np.frombuffer(bytes(value), dtype=np.uint8)
        elif isinstance(value, np.ndarray) and value.dtype in (np.uint8, np.int8):
            if value.ndim != 1:
                raise ValueError(f"Only 1D byte arrays are supported, got shape {value.shape}")
            arr = value
        else:
            raise ValueError(f'Unsupported type: {type(value)}')
        return self.__AddFixedArray(name, TypeCode.MEMORYARRAY, arr)

    def AddBoolArray(self, name: str, value) -> TypeCode:
        wire = np.asarray(value, dtype=np.uint8)
        return self.__AddFixedArray(name, TypeCode.BOOLARRAY, wire, wire.astype(bool))

    def AddUByteArray(self, name: str, value) -> TypeCode:
        return self.__AddFixedArray(name, TypeCode.UBYTEARRAY, np.asarray(value, dtype=np.uint8))

    def AddByteArray(self, name: str, value) -> TypeCode:
        return self.__AddFixedArray(name, TypeCode.BYTEARRAY, np.asarray(value, dtype=np.int8))

    def AddUIntArray(self, name: str, value) -> TypeCode:
        return self.__AddFixedArray(name, TypeCode.UINTARRAY, np.asarray(value, dtype=np.uint32))

    def AddIntArray(self, name: str, value) -> TypeCode:
        return self.__AddFixedArray(name, TypeCode.INTARRAY, np.asarray(value, dtype=np.int32))

    def AddULongArray(self, name: str, value) -> TypeCode:
        return self.__AddFixedArray(name, TypeCode.ULONGARRAY, np.asarray(value, dtype=np.uint64))

    def AddLongArray(self, name: str, value) -> TypeCode:
        return self.__AddFixedArray(name, TypeCode.LONGARRAY, np.asarray(value, dtype=np.int64))

    def AddFloatArray(self, name: str, value) -> TypeCode:
        return self.__AddFixedArray(name, TypeCode.FLOATARRAY, np.asarray(value, dtype=np.float32))

    def AddDoubleArray(self, name: str, value) -> TypeCode:
        return self.__AddFixedArray(name, TypeCode.DOUBLEARRAY, np.asarray(value, dtype=np.float64))

    def AddStringArray(self, name: str, value) -> TypeCode:
        items = list(value)
        if len(items) == 0:
            return False
        parts = [struct.pack('I', len(items))]
        for s in items:
            s_b = s.encode('utf-8')
            parts.append(struct.pack('I', len(s_b)))
            parts.append(s_b)
        payload = b''.join(parts)
        return self.__AddElement(name, TypeCode.STRINGARRAY, items, struct.pack('I', len(payload)) + payload)

    def AddMessageArray(self, name: str, value) -> TypeCode:
        items = list(value)
        if len(items) == 0:
            return False
        parts = [struct.pack('I', len(items))]
        for m in items:
            m_b = bytes(m.GetData())
            parts.append(struct.pack('I', len(m_b)))
            parts.append(m_b)
        payload = b''.join(parts)
        return self.__AddElement(name, TypeCode.MESSAGEARRAY, items, struct.pack('I', len(payload)) + payload)

    def GetDictionary(self):
        return self.p_elem

    def AddDict(self, dic):
        if isinstance(dic, dict):
            for k, v in dic.items():
                self.AddValue(k, v)
        else:
            raise ValueError('The argument is not a dictionary')

    def GetData(self):
        header_b = struct.pack(f'{_HEADER_ELEMENTS}Q', *self.p_header)
        return header_b + self.p_data

    def List(self):
        print("Elements: ", len(self.p_elem))
        for i, k in enumerate(self.p_elem):
            print(f"{0}: {1}".format(k, self.p_elem[k]))
