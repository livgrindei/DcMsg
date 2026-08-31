#pragma once

// DcMsg message is a bytes array with a header and a succession of DcMsg elements
//    HEADER   |    DcMsg     | DcMsg ...
//  3 uint64   | min 18 bytes | min 18 bytes

// HEADER
//  VERSION |                   SIZE                 |        ELEMENTS
//  uint64  |                  uint64                |         uint64
//   1000   | total size in bytes (including header) | no of DcMsg elements

// DcMsg scalar element (TypeCode BOOL to DOUBLE)
//   TYPECODE     |          NAME            |  VALUE
//    1 byte      |       16 bytes           | 1 byte (BOOL, UBYTE, BYTE) or 4 bytes (UINT, INT, FLOAT) or 8 bytes (ULONG, LONG, DOUBLE)
// TypeCode enum  | max 15 char + null term  |  value

// DcMsg variable-length element (TypeCode STRING, MESSAGE or MEMORYARRAY)
//   TYPECODE     |          NAME            |      SIZE     |    VALUE
//    1 byte      |       16 bytes           |    4 bytes    |  SIZE bytes
// TypeCode enum  | max 15 char + null term  | size (uint32) |    bytes
// string values are not null terminated

// DcMsg fixed-element array (TypeCode BOOLARRAY .. DOUBLEARRAY) - VALUE is COUNT * element size raw bytes
//   TYPECODE     |          NAME            |      SIZE     |    VALUE
//    1 byte      |       16 bytes           |    4 bytes    |  SIZE bytes
// TypeCode enum  | max 15 char + null term  | size (uint32) |    packed elements

// DcMsg object array element (TypeCode STRINGARRAY or MESSAGEARRAY) - each item is length-prefixed
//   TYPECODE     |          NAME            |  TOTAL SIZE   |    NO ITEMS   |  ITEM1 SIZE   | ITEM1 PAYLOAD
//    1 byte      |       16 bytes           |    4 bytes    |    4 bytes    |     4 bytes   |
// TypeCode enum  | max 15 char + null term  | size (uint32) | size (uint32) | size (uint32) |    bytes
// string values are not null terminated

#include <cstdint>
#include <vector>
#include <cstring>
#include <cstdint>
#include <string>

#define DTYPE_SIZE_NAME 16
#define HEADER_ELEMENTS 3
#define NUMBER_OF_TYPES 24
#define NUMBER_OF_BASE_TYPES 10
#define MESS_VERSION 1000
#define MIN_MESS_VERSION 10

namespace DS
{
    class DcMsg
    {
        public:
            enum class EType : unsigned char
            {
                NA           = 0,
                BOOL         = 1,
                UBYTE        = 2,
                BYTE         = 3,
                UINT         = 4,
                INT          = 5,
                ULONG        = 6,
                LONG         = 7,
                FLOAT        = 8,
                DOUBLE       = 9,
                STRING       = 10,
                MESSAGE      = 11,
                MEMORYARRAY  = 12,
                BOOLARRAY    = 13,
                UBYTEARRAY   = 14,
                BYTEARRAY    = 15,
                UINTARRAY    = 16,
                INTARRAY     = 17,
                ULONGARRAY   = 18,
                LONGARRAY    = 19,
                FLOATARRAY   = 20,
                DOUBLEARRAY  = 21,
                STRINGARRAY  = 22,
                MESSAGEARRAY = 23
            };

            static constexpr unsigned char SType[NUMBER_OF_TYPES] = {
                0,
                1,
                1,
                1,
                4,
                4,
                8,
                8,
                4,
                8,
                0,
                0,
                0,
                0,
                0,
                0,
                0,
                0,
                0,
                0,
                0,
                0,
                0
            };

            static constexpr const char* StrType[NUMBER_OF_TYPES] = {
                "NA",
                "BOOL",
                "UBYTE",
                "BYTE",
                "UINT",
                "INT",
                "ULONG",
                "LONG",
                "FLOAT",
                "DOUBLE",
                "STRING",
                "MESSAGE",
                "MEMORYARRAY",
                "BOOLARRAY",
                "UBYTEARRAY",
                "BYTEARRAY",
                "UINTARRAY",
                "INTARRAY",
                "ULONGARRAY",
                "LONGARRAY",
                "FLOATARRAY",
                "DOUBLEARRAY",
                "STRINGARRAY",
                "MESSAGEARRAY"
            };
        
            struct DType
            {
                DcMsg::EType type;
                bool basic_type;
                uint64_t size;
                uint32_t obj_size;
                char name[DTYPE_SIZE_NAME];
                uint32_t index;

                DType()
                {
                    this->type = DcMsg::EType::NA;
                    this->basic_type = false;
                    this->size = 0;
                    this->obj_size = 1;
                    memset(this->name, 0, DTYPE_SIZE_NAME);
                    this->index = 0;
                };

                DType(DcMsg::EType type, const char* name)
                {
                    this->type = type;
                    this->size = SType[static_cast<unsigned char>(type)];
                    this->basic_type = this->size;
                    this->obj_size = 1;
                    strncpy(this->name, name, DTYPE_SIZE_NAME);
                    this->name[DTYPE_SIZE_NAME] = '\0';
                    this->index = 0;
                };

                DType(DcMsg::EType type, const char* name, uint64_t index)
                {
                    this->type = type;
                    this->size = SType[static_cast<unsigned char>(type)];
                    this->basic_type = this->size;
                    this->obj_size = 1;
                    strncpy(this->name, name, DTYPE_SIZE_NAME);
                    this->name[DTYPE_SIZE_NAME] = '\0';
                    this->index = index;
                };

                DType(DcMsg::EType type, const char* name, uint32_t index, uint64_t size, uint32_t obj_size = 1)
                {
                    this->type = type;
                    this->basic_type = DcMsg::SType[static_cast<unsigned char>(type)];
                    this->size = this->basic_type ? DcMsg::SType[static_cast<unsigned char>(type)] : size;
                    this->obj_size = obj_size;
                    strncpy(this->name, name, DTYPE_SIZE_NAME);
                    this->name[DTYPE_SIZE_NAME] = '\0';
                    this->index = index;
                };

                DcMsg::DType& operator=(const DcMsg::DType& a)
                {
                    this->type = a.type;
                    this->basic_type = a.basic_type;
                    this->size = a.size;
                    this->obj_size = a.obj_size;
                    memcpy(this->name, a.name, DTYPE_SIZE_NAME);
                    this->index = a.index;

                    return *this;
                };

                void Print()
                {
                    if (this->type == DcMsg::EType::MESSAGEARRAY)
                    {
                        printf("Type: MARRAY (not basic); Name: %s; Number of Object: %u; Object Size: %u; Index: %u\n", (char*)&this->name, this->obj_size, this->size, this->index);
                    }
                    else
                    {
                        printf("Type: %s (%s); Name: %s; Size: %u; Index: %u\n",DcMsg::StrType[static_cast<unsigned char>(this->type)], this->basic_type ? "basic" : "not basic", (char*)&this->name, this->size, this->index);
                    }
                };
            };

            enum class EError : unsigned char
            {
                None            = 0,
                ReadOnly        = 1,
                NotFound        = 2,
                TypeMismatch    = 3,
                NotBasicType    = 4,
                InvalidName     = 5,
                DuplicateName   = 6,
                InvalidArgument = 7,
                OutOfMemory     = 8,
                InvalidData     = 9
            };

        private:
            const uint64_t header_size = sizeof(uint64_t) * HEADER_ELEMENTS;
            bool readOnly;
            bool validData = false;
            std::vector<DcMsg::DType> data;
            void* p_data = nullptr;
            void* p_ex_data = nullptr;
            uint64_t p_size = 0;
            uint64_t mem_used = 0;
            uint64_t el_size = 0;
            mutable DcMsg::EError last_error = DcMsg::EError::None;

        public:
            DcMsg();
            DcMsg(void* data, uint64_t size);
            ~DcMsg();
            DcMsg(const DcMsg& other);
            DcMsg& operator=(const DcMsg& other);
            DcMsg(DcMsg&& other) noexcept;
            DcMsg& operator=(DcMsg&& other) noexcept;
            void Clear();
            DcMsg Clone() const;
            unsigned int Elements();
            uint64_t MemUsed();

            void PrintBuffer();
            void List();
            int Find(const char* name) const;
            bool AddBool(const char* name, const bool value);
            bool AddUByte(const char* name, const uint8_t value);
            bool AddByte(const char* name, const int8_t value);
            bool AddUShort(const char* name, const uint16_t value);
            bool AddShort(const char* name, const int16_t value);
            bool AddUInt(const char* name, const uint32_t value);
            bool AddInt(const char* name, const int32_t value);
            bool AddULong(const char* name, const uint64_t value);
            bool AddLong(const char* name, const int64_t value);
            bool AddFloat(const char* name, const float value);
            bool AddDouble(const char* name, const double value);
            bool AddString(const char* name, const char* value);
            bool AddString(const char* name, const std::string& value);
            bool AddMessage(const char* name, DcMsg& data);
            bool AddMemoryArray(const char* name, void* value, const uint32_t size);
            bool AddMessageArray(const char* name, std::vector<DcMsg>& data);
            bool AddBoolArray(const char* name, std::vector<bool>& data);
            bool AddUByteArray(const char* name, std::vector<uint8_t>& data);
            bool AddByteArray(const char* name, std::vector<int8_t>& data);
            bool AddUIntArray(const char* name, std::vector<uint32_t>& data);
            bool AddIntArray(const char* name, std::vector<int32_t>& data);
            bool AddULongArray(const char* name, std::vector<uint64_t>& data);
            bool AddLongArray(const char* name, std::vector<int64_t>& data);
            bool AddFloatArray(const char* name, std::vector<float>& data);
            bool AddDoubleArray(const char* name, std::vector<double>& data);
            bool AddStringArray(const char* name, std::vector<std::string>& data);
            bool UpdateBool(const char* name, const bool value);
            bool UpdateUByte(const char* name, const uint8_t value);
            bool UpdateByte(const char* name, const int8_t value);
            bool UpdateUShort(const char* name, const uint16_t value);
            bool UpdateShort(const char* name, const int16_t value);
            bool UpdateUInt(const char* name, const uint32_t value);
            bool UpdateInt(const char* name, const int32_t value);
            bool UpdateULong(const char* name, const uint64_t value);
            bool UpdateLong(const char* name, const int64_t value);
            bool UpdateFloat(const char* name, const float value);
            bool UpdateDouble(const char* name, const double value);
            bool GetBool(const char* name, bool& value);
            bool GetUByte(const char* name, uint8_t& value);
            bool GetByte(const char* name, int8_t& value);
            bool GetUShort(const char* name, uint16_t& value);
            bool GetShort(const char* name, int16_t& value);
            bool GetUInt(const char* name, uint32_t& value);
            bool GetInt(const char* name, int32_t& value);
            bool GetULong(const char* name, uint64_t& value);
            bool GetLong(const char* name, int64_t& value);
            bool GetFloat(const char* name, float& value);
            bool GetDouble(const char* name, double& value);
            bool GetString(const char* name, std::string& value);
            bool GetBoolArray(const char* name, std::vector<bool>& data);
            bool GetUByteArray(const char* name, std::vector<uint8_t>& data);
            bool GetByteArray(const char* name, std::vector<int8_t>& data);
            bool GetUIntArray(const char* name, std::vector<uint32_t>& data);
            bool GetIntArray(const char* name, std::vector<int32_t>& data);
            bool GetULongArray(const char* name, std::vector<uint64_t>& data);
            bool GetLongArray(const char* name, std::vector<int64_t>& data);
            bool GetFloatArray(const char* name, std::vector<float>& data);
            bool GetDoubleArray(const char* name, std::vector<double>& data);
            bool GetStringArray(const char* name, std::vector<std::string>& data);
            bool GetMemoryArray(const char* name, void** value, uint32_t& size);
            bool GetMessage(const char* name, DcMsg& data);
            bool GetMessageArray(const char* name, std::vector<DcMsg>& data);
            bool Delete(const char* name);
            void* GetData() const;
            void* GetData(uint64_t& size);
            bool IsValid();
            DcMsg::EError GetLastError() const;

        private:
            inline void* SourceData() const;
            void SetError(DcMsg::EError code, const char* fmt, ...) const;
            bool AddToBuffer(void* data, uint64_t size);
            inline bool InitHeader();
            bool AddElementToBuffer(DcMsg::DType& element, const void* data);
            inline bool CheckName(const char* name) const;
            template<typename T>
            inline bool AddValue(const char* name, DcMsg::EType type, const T& value);
            inline bool AddValue(const char* name, DcMsg::EType type, void* value, uint32_t size);
            bool ParseData(uint64_t& elements);
            template<typename T>
            inline bool RetrieveElement(const char* name, DcMsg::EType type, T& value);
            template<typename T>
            inline bool UpdateValue(const char* name, DcMsg::EType type, const T& value);
    };
} // namespace DS
