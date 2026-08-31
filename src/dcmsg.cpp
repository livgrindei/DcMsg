#include "dcmsg/dcmsg.h"
#include "logger.h"

#include <cstdio>
#include <cstdarg>
#include <cstring>
#include <cstdlib>
#include <cmath>
#include <inttypes.h>

#define MEM_BLOCK_SIZE 1024
#define MAX_STACK_VAR_SIZE 10240    // set arbitrary to 10k

namespace DS
{
    DcMsg::DcMsg()
    {
        this->readOnly = false;
        this->validData = false;
        this->InitHeader();
    }

    DcMsg::DcMsg(void *data, uint64_t size)
    {
        this->readOnly = true;
        this->p_ex_data = data;
        this->validData = false;
        if (!this->p_ex_data)
        {
            this->SetError(DcMsg::EError::InvalidArgument, "[DcMsg] Invalid data pointer!");
            return;
        }
        if (size < this->header_size)
        {
            this->SetError(DcMsg::EError::InvalidData, "[DcMsg] Dataset size is too small (min %lu bytes)!", this->header_size);
            return;
        }
        uint64_t* p_d64 = static_cast<uint64_t*>(this->p_ex_data);
        uint64_t version = p_d64[0];
        if (version < MIN_MESS_VERSION || version > MESS_VERSION)
        {
            this->SetError(DcMsg::EError::InvalidData, "[DcMsg] Invalid data version!");
            return;
        }

        this->mem_used = p_d64[1];
        this->el_size = p_d64[2];
        if (this->mem_used != size)
        {
            this->SetError(DcMsg::EError::InvalidData, "[DcMsg] Data size (%zu bytes) is not the same as the encoded value of %lu bytes!", size, this->mem_used);
            return;
        }

        uint64_t el;

        // ParseData reports the specific failure reason itself
        this->validData = this->ParseData(el);
    }

    DcMsg::~DcMsg()
    {
        if (this->p_data)
        {
            free(this->p_data);
            this->p_data = nullptr;
        }
    }

    DcMsg::DcMsg(const DcMsg &other)
    {
        this->readOnly   = other.readOnly;
        this->validData  = other.validData;
        this->data       = other.data;
        this->p_ex_data  = other.p_ex_data;

        this->p_size     = 0;
        this->mem_used   = 0;
        this->el_size    = other.el_size;

        this->p_data = nullptr;

        // Deep copy only if we own memory
        if (!this->readOnly && other.p_data)
        {
            this->p_data = std::malloc(other.p_size);
            if (!this->p_data) std::abort();
            std::memcpy(this->p_data, other.p_data, other.p_size);
            this->p_size   = other.p_size;
            this->mem_used = other.mem_used;
        }
        else
        {
            this->InitHeader();
        }
    }

    DcMsg &DcMsg::operator=(const DcMsg &other)
    {
        if (this == &other) return *this;
        if (this->p_data)
        {
            std::free(this->p_data);
            this->p_data = nullptr;
        }

        this->readOnly   = other.readOnly;
        this->validData  = other.validData;
        this->data       = other.data;
        this->p_ex_data  = other.p_ex_data;

        this->p_size     = 0;
        this->mem_used   = 0;
        this->el_size    = other.el_size;

        this->p_data = nullptr;

        // Deep copy only if we own memory
        if (!this->readOnly && other.p_data)
        {
            this->p_data = std::malloc(other.p_size);
            if (!this->p_data) std::abort();
            std::memcpy(this->p_data, other.p_data, other.p_size);
            this->p_size   = other.p_size;
            this->mem_used = other.mem_used;
        }
        else
        {
            this->InitHeader();
        }

        return *this;
    }

    DcMsg::DcMsg(DcMsg &&other) noexcept
    {
        this->readOnly   = other.readOnly;
        this->validData  = other.validData;
        this->data       = std::move(other.data);
        
        this->p_data     = other.p_data;
        this->p_ex_data  = other.p_ex_data;
        this->p_size     = other.p_size;
        this->mem_used   = other.mem_used;
        this->el_size    = other.el_size;

        other.readOnly = true;
        other.validData = false;
        other.p_data = nullptr;
        other.p_ex_data  = nullptr;
        other.p_size = 0;
        other.mem_used = 0;
        other.el_size = 0;
    }

    DcMsg &DcMsg::operator=(DcMsg &&other) noexcept
    {
        if (this == &other) return *this;
        if (this->p_data)
        {
            free(this->p_data);
            this->p_data = nullptr;
        }

        this->readOnly   = other.readOnly;
        this->validData  = other.validData;
        this->data       = std::move(other.data);

        this->p_data     = other.p_data;
        this->p_ex_data  = other.p_ex_data;
        this->p_size     = other.p_size;
        this->mem_used   = other.mem_used;
        this->el_size    = other.el_size;

        other.readOnly = true;
        other.validData = false;
        other.p_data = nullptr;
        other.p_ex_data  = nullptr;
        other.p_size = 0;
        other.mem_used = 0;
        other.el_size = 0;

        return *this;
    }

    void DcMsg::Clear()
    {
        if (this->readOnly)
        {
            return;
        }
        if (this->p_data)
        {
            free(this->p_data);
            this->p_data = nullptr;
        }
        this->p_size = 0;
        this->el_size = 0;
        this->mem_used = 0;
        this->data.clear();
        this->validData = false;
        this->InitHeader();
    }

    DcMsg DcMsg::Clone() const
    {
        DcMsg copy;
        free(copy.p_data);
        copy.p_data   = nullptr;
        copy.p_size   = 0;
        copy.mem_used = 0;

        const uint64_t size = this->mem_used;
        if (size > 0)
        {
            copy.p_data = std::malloc(size);
            if (!copy.p_data)
            {
                copy.SetError(DcMsg::EError::OutOfMemory, "[DcMsg] Error memory allocation!");
                copy.validData = false;
                return copy;
            }
            std::memcpy(copy.p_data, this->SourceData(), size);
            copy.p_size   = size;
            copy.mem_used = size;
        }

        copy.el_size   = this->el_size;
        copy.data      = this->data;
        copy.validData = this->validData;
        copy.readOnly  = false;

        return copy;
    }

    bool DcMsg::InitHeader()
    {
        uint64_t buff[HEADER_ELEMENTS] = {0};
        buff[0] = MESS_VERSION;
        this->validData = this->AddToBuffer(buff, sizeof(buff));
        return this->validData;
    }

    unsigned int DcMsg::Elements()
    {
        return static_cast<unsigned int>(this->data.size());
    }

    uint64_t DcMsg::MemUsed()
    {
        return this->mem_used;
    }

    bool DcMsg::AddToBuffer(void *data, uint64_t size)
    {
        // uint8_t* pt = static_cast<uint8_t*>(data);
        // printf("To write [%zu]: ", size);
        // for (size_t i = 0; i < size; i++)
        // {
        //     printf("%02X ", pt[i]);
        // }
        // printf("\n");
        
        long mem_available = static_cast<long>(this->p_size - this->mem_used);
        // printf("---- Mem available: %ld\n", mem_available);
        if (mem_available < static_cast<long>(size))
        {
            long mem_to_reserve = std::ceil(static_cast<double>(size - mem_available) / MEM_BLOCK_SIZE) * MEM_BLOCK_SIZE;
            void* p_temp = std::realloc(this->p_data, this->p_size + mem_to_reserve);
            // printf("---- Reserved: %ld\n", mem_to_reserve);
            if (!p_temp)
            {
                this->SetError(DcMsg::EError::OutOfMemory, "[DcMsg] Error memory allocation!");
                return false;
            }
            this->p_data = p_temp;
            this->p_size += mem_to_reserve;
        }
        std::memcpy(static_cast<char*>(this->p_data) + this->mem_used, data, size);
        this->mem_used += size;
        // printf("---- Mem used: %zu\n", this->mem_used);

        return true;
    }

    bool DcMsg::AddElementToBuffer(DcMsg::DType& element, const void* data)
    {
        if (this->readOnly)
        {
            this->SetError(DcMsg::EError::ReadOnly, "[DcMsg] Read only!");
            return false;
        }
        if (!data)
        {
            this->SetError(DcMsg::EError::InvalidArgument, "[DcMsg] Invalid pointer!");
            return false;
        }

        uint32_t header_size = HEADER_ELEMENTS - 1;
        uint64_t header[header_size];
        const uint64_t buff_size = element.basic_type ? DTYPE_SIZE_NAME + 1 + element.size : DTYPE_SIZE_NAME + 1 + sizeof(uint32_t) + element.size;
        uint32_t offset = DTYPE_SIZE_NAME + 1;
        if (buff_size > MAX_STACK_VAR_SIZE)
        {
            unsigned char* buff = (unsigned char*)malloc(buff_size);
            if (!buff)
            {
                this->SetError(DcMsg::EError::OutOfMemory, "[DcMsg] Error memory allocation!");
                return false;
            }
            buff[0] = static_cast<unsigned char>(element.type);
            memcpy(&buff[1], element.name, DTYPE_SIZE_NAME);
            if (element.basic_type)
            {
                memcpy(&buff[offset], data, element.size);
            }
            else
            {
                memcpy(&buff[offset], &element.size, sizeof(uint32_t));
                offset += sizeof(uint32_t);
                memcpy(&buff[offset], data, element.size);
            }
            element.index = this->mem_used + offset;
            this->AddToBuffer(buff, buff_size);
            free(buff);
        }
        else
        {
            char buff[buff_size];
            buff[0] = static_cast<unsigned char>(element.type);
            memcpy(&buff[1], element.name, DTYPE_SIZE_NAME);
            if (element.basic_type)
            {
                memcpy(&buff[offset], data, element.size);
            }
            else
            {
                memcpy(&buff[offset], &element.size, sizeof(uint32_t));
                offset += sizeof(uint32_t);
                memcpy(&buff[offset], data, element.size);
            }
            element.index = this->mem_used + offset;
            this->AddToBuffer(buff, buff_size);
        }
        this->el_size++;
        header[0] = this->mem_used;
        header[1] = this->el_size;

        uint64_t* ptr = static_cast<uint64_t*>(this->p_data);
        memcpy(ptr + 1, header, header_size * sizeof(uint64_t));

        return true;
    }

    inline bool DcMsg::CheckName(const char *name) const
    {
        if (strlen(name) >= DTYPE_SIZE_NAME)
        {
            this->SetError(DcMsg::EError::InvalidName, "[DcMsg] Data not added. Name '%s' is too long!", name);
            return false;
        }
        if (this->Find(name) >= 0)
        {
            this->SetError(DcMsg::EError::DuplicateName, "[DcMsg] Data not added. Name '%s' already exists!", name);
            return false;
        }
        return true;
    }

    template <typename T>
    inline bool DcMsg::AddValue(const char *name, DcMsg::EType type, const T &value)
    {
        if (!this->CheckName(name))
        {
            return false;
        }

        DcMsg::DType dt(type, name);
        if (!dt.basic_type)
        {
            this->SetError(DcMsg::EError::NotBasicType, "[DcMsg] Element is not a basic type!");
            return false;
        }

        if (this->AddElementToBuffer(dt, &value))
        {
            this->data.push_back(dt);
            this->last_error = DcMsg::EError::None;
            return true;
        }
        // AddElementToBuffer already reported the specific failure reason
        return false;
    }

    inline bool DcMsg::AddValue(const char *name, DcMsg::EType type, void* value, uint32_t size)
    {
        if (!this->CheckName(name))
        {
            return false;
        }

        DcMsg::DType dt(type, name);
        dt.size = size;

        if (this->AddElementToBuffer(dt, value))
        {
            this->data.push_back(dt);
            this->last_error = DcMsg::EError::None;
            return true;
        }
        // AddElementToBuffer already reported the specific failure reason
        return false;
    }

    template <typename T>
    inline bool DcMsg::RetrieveElement(const char *name, DcMsg::EType type, T &value)
    {
        int index = this->Find(name);
        if (index < 0)
        {
            this->SetError(DcMsg::EError::NotFound, "[DcMsg] Element '%s' not found!", name);
            return false;
        }
        DcMsg::DType& element = this->data[index];
        if (element.type != type)
        {
            this->SetError(DcMsg::EError::TypeMismatch, "[DcMsg] Element '%s' is not type %s!", name, DcMsg::StrType[static_cast<unsigned char>(type)]);
            return false;
        }
        if (!element.basic_type)
        {
            this->SetError(DcMsg::EError::NotBasicType, "[DcMsg] Element '%s' is not a basic!", name);
            return false;
        }

        memcpy(&value, static_cast<unsigned char*>(this->SourceData()) + element.index, element.size);
        this->last_error = DcMsg::EError::None;
        return true;
    }

    template <typename T>
    inline bool DcMsg::UpdateValue(const char *name, DcMsg::EType type, const T &value)
    {
        if (this->readOnly)
        {
            this->SetError(DcMsg::EError::ReadOnly, "[DcMsg] Read only!");
            return false;
        }
        int index = this->Find(name);
        if (index < 0)
        {
            this->SetError(DcMsg::EError::NotFound, "[DcMsg] Element '%s' not found!", name);
            return false;
        }
        DcMsg::DType& element = this->data[index];
        if (element.type != type)
        {
            this->SetError(DcMsg::EError::TypeMismatch, "[DcMsg] Element '%s' is not type %s!", name, DcMsg::StrType[static_cast<unsigned char>(type)]);
            return false;
        }
        if (!element.basic_type)
        {
            this->SetError(DcMsg::EError::NotBasicType, "[DcMsg] Element '%s' is not a basic!", name);
            return false;
        }

        memcpy(static_cast<unsigned char*>(this->p_data) + element.index, &value, element.size);
        this->last_error = DcMsg::EError::None;
        return true;
    }

    bool DcMsg::ParseData(uint64_t& elements)
    {
        elements = 0;
        if (!this->p_ex_data || this->mem_used < this->header_size)
        {
            return false;
        }
        uint32_t curr_index = this->header_size;

        while (curr_index < this->mem_used)
        {
            if (this->mem_used - curr_index < DTYPE_SIZE_NAME + 1)
            {
                this->SetError(DcMsg::EError::InvalidData, "[DcMsg] Data truncated for the element %zu (impossible to recover the type and the name)!", elements);
                return false;
            }
            unsigned char ty = static_cast<unsigned char*>(this->p_ex_data)[curr_index];
            if (ty < 1 || ty >= NUMBER_OF_TYPES)
            {
                this->SetError(DcMsg::EError::InvalidData, "[DcMsg] Unknown type for the element %zu!", elements);
                return false;
            }
            curr_index++;

            DcMsg::DType msg(static_cast<DcMsg::EType>(ty), reinterpret_cast<const char*>(static_cast<char*>(this->p_ex_data) + curr_index));
            curr_index += DTYPE_SIZE_NAME;
            if (!msg.basic_type)
            {
                if (this->mem_used - curr_index < sizeof(uint32_t))
                {
                    this->SetError(DcMsg::EError::InvalidData, "[DcMsg] Data truncated for the element %zu (missing or incomplete element size)!", elements);
                    return false;
                }
                memcpy(&msg.size, static_cast<char*>(this->p_ex_data) + curr_index, sizeof(uint32_t));
                curr_index += sizeof(uint32_t);
            }
            msg.index = curr_index;
            if (this->mem_used - curr_index < msg.size)
            {
                this->SetError(DcMsg::EError::InvalidData, "[DcMsg] Data truncated for the element %zu!", elements);
                return false;
            }
            this->data.push_back(msg);
            curr_index += msg.size;
            elements++;            
        }

        return true;
    }

    void DcMsg::PrintBuffer()
    {
        if (this->readOnly)
        {
            uint8_t* pt_8 = static_cast<uint8_t*>(this->p_ex_data);
            uint64_t* pt_64 = static_cast<uint64_t*>(this->p_ex_data);
            printf("Buffer: %" PRIu64 " - %" PRIu64 " [ ", pt_64[0], pt_64[1]);
            for (size_t i = 0; i < this->header_size; i++)
            {
                printf("%u ", pt_8[i]);
            }
            printf("]\n");
    
            for (size_t i = this->header_size; i < this->mem_used; i++)
            {
                printf("%u ", pt_8[i]);
            }
            printf("\n");
        }
        else
        {
            uint8_t* pt_8 = static_cast<uint8_t*>(this->p_data);
            uint64_t* pt_64 = static_cast<uint64_t*>(this->p_data);
            printf("Buffer: %" PRIu64 " - %" PRIu64 " [ ", pt_64[0], pt_64[1]);
            for (size_t i = 0; i < this->header_size; i++)
            {
                printf("%u ", pt_8[i]);
            }
            printf("]\n");
    
            for (size_t i = this->header_size; i < this->p_size; i++)
            {
                printf("%u ", pt_8[i]);
            }
            printf("\n");
        }        
    }

    void DcMsg::List()
    {
        printf("Elements: %lu\n", this->el_size);
        for (size_t i = 0; i < this->el_size; i++)
        {
            this->data[i].Print();
        }        
    }

    int DcMsg::Find(const char *name) const
    {
        for (size_t i = 0; i < this->data.size(); i++)
        {
            if (strncmp(this->data[i].name, name, DTYPE_SIZE_NAME) == 0)
            {
                return i;
            }
        }        
        return -1;
    }

    bool DcMsg::AddBool(const char *name, const bool value)
    {
        return AddValue(name, DcMsg::EType::BOOL, value);
    }

    bool DcMsg::AddUByte(const char *name, const uint8_t value)
    {
        return AddValue(name, DcMsg::EType::UBYTE, value);
    }

    bool DcMsg::AddByte(const char *name, const int8_t value)
    {
        return AddValue(name, DcMsg::EType::BYTE, value);
    }

    bool DcMsg::AddUShort(const char *name, const uint16_t value)
    {
        return AddValue(name, DcMsg::EType::UINT, static_cast<uint32_t>(value));
    }

    bool DcMsg::AddShort(const char *name, const int16_t value)
    {
        return AddValue(name, DcMsg::EType::INT, static_cast<int32_t>(value));
    }

    bool DcMsg::AddUInt(const char *name, const uint32_t value)
    {
        return AddValue(name, DcMsg::EType::UINT, value);
    }

    bool DcMsg::AddInt(const char *name, const int32_t value)
    {
        return AddValue(name, DcMsg::EType::INT, value);
    }

    bool DcMsg::AddULong(const char *name, const uint64_t value)
    {
        return AddValue(name, DcMsg::EType::ULONG, value);
    }

    bool DcMsg::AddLong(const char *name, const int64_t value)
    {
        return AddValue(name, DcMsg::EType::LONG, value);
    }

    bool DcMsg::AddFloat(const char *name, const float value)
    {
        return AddValue(name, DcMsg::EType::FLOAT, value);
    }

    bool DcMsg::AddDouble(const char *name, const double value)
    {
        return AddValue(name, DcMsg::EType::DOUBLE, value);
    }

    bool DcMsg::AddString(const char *name, const char *value)
    {
        return AddValue(name, DcMsg::EType::STRING, (void *)value, strlen(value));
    }

    bool DcMsg::AddString(const char *name, const std::string& value)
    {
        return AddValue(name, DcMsg::EType::STRING, (void *)value.data(), value.size());
    }

    bool DcMsg::AddBoolArray(const char *name, std::vector<bool> &data)
    {
        const uint32_t count = static_cast<uint32_t>(data.size());
        std::vector<uint8_t> buffer(count);
        for (uint32_t i = 0; i < count; i++)
        {
            buffer[i] = data[i] ? 1 : 0;
        }
        return AddValue(name, DcMsg::EType::BOOLARRAY, buffer.data(), count);
    }

    bool DcMsg::AddUByteArray(const char *name, std::vector<uint8_t> &data)
    {
        return AddValue(name, DcMsg::EType::UBYTEARRAY, data.data(), static_cast<uint32_t>(data.size()));
    }

    bool DcMsg::AddByteArray(const char *name, std::vector<int8_t> &data)
    {
        return AddValue(name, DcMsg::EType::BYTEARRAY, data.data(), static_cast<uint32_t>(data.size()));
    }

    bool DcMsg::AddUIntArray(const char *name, std::vector<uint32_t> &data)
    {
        return AddValue(name, DcMsg::EType::UINTARRAY, data.data(), static_cast<uint32_t>(data.size() * sizeof(uint32_t)));
    }

    bool DcMsg::AddIntArray(const char *name, std::vector<int32_t> &data)
    {
        return AddValue(name, DcMsg::EType::INTARRAY, data.data(), static_cast<uint32_t>(data.size() * sizeof(int32_t)));
    }

    bool DcMsg::AddULongArray(const char *name, std::vector<uint64_t> &data)
    {
        return AddValue(name, DcMsg::EType::ULONGARRAY, data.data(), static_cast<uint32_t>(data.size() * sizeof(uint64_t)));
    }

    bool DcMsg::AddLongArray(const char *name, std::vector<int64_t> &data)
    {
        return AddValue(name, DcMsg::EType::LONGARRAY, data.data(), static_cast<uint32_t>(data.size() * sizeof(int64_t)));
    }

    bool DcMsg::AddFloatArray(const char *name, std::vector<float> &data)
    {
        return AddValue(name, DcMsg::EType::FLOATARRAY, data.data(), static_cast<uint32_t>(data.size() * sizeof(float)));
    }

    bool DcMsg::AddDoubleArray(const char *name, std::vector<double> &data)
    {
        return AddValue(name, DcMsg::EType::DOUBLEARRAY, data.data(), static_cast<uint32_t>(data.size() * sizeof(double)));
    }

    bool DcMsg::AddStringArray(const char *name, std::vector<std::string> &data)
    {
        if (data.size() == 0)
        {
            this->SetError(DcMsg::EError::InvalidArgument, "[DcMsg] Element '%s' not added: the array is empty!", name);
            return false;
        }

        size_t buffer_size = sizeof(uint32_t);
        const uint32_t data_size = static_cast<uint32_t>(data.size());
        for (uint32_t i = 0; i < data_size; i++)
        {
            buffer_size += sizeof(uint32_t) + data[i].size();
        }

        bool rc = false;
        uint32_t idx = 0;
        if (buffer_size > MAX_STACK_VAR_SIZE)
        {
            unsigned char* buffer = (unsigned char*)malloc(buffer_size);
            if (!buffer)
            {
                this->SetError(DcMsg::EError::OutOfMemory, "[DcMsg] Error memory allocation!");
                return false;
            }
            memcpy(buffer, &data_size, sizeof(uint32_t));
            idx += sizeof(uint32_t);
            for (uint32_t i = 0; i < data_size; i++)
            {
                uint32_t s_len = static_cast<uint32_t>(data[i].size());
                memcpy(buffer + idx, &s_len, sizeof(uint32_t));
                idx += sizeof(uint32_t);
                memcpy(buffer + idx, data[i].data(), s_len);
                idx += s_len;
            }
            rc = this->AddValue(name, DcMsg::EType::STRINGARRAY, buffer, buffer_size);
            free(buffer);
        }
        else
        {
            char buffer[buffer_size];
            memcpy(buffer, &data_size, sizeof(uint32_t));
            idx += sizeof(uint32_t);
            for (uint32_t i = 0; i < data_size; i++)
            {
                uint32_t s_len = static_cast<uint32_t>(data[i].size());
                memcpy(buffer + idx, &s_len, sizeof(uint32_t));
                idx += sizeof(uint32_t);
                memcpy(buffer + idx, data[i].data(), s_len);
                idx += s_len;
            }
            rc = this->AddValue(name, DcMsg::EType::STRINGARRAY, buffer, buffer_size);
        }

        return rc;
    }

    bool DcMsg::AddMemoryArray(const char *name, void *value, const uint32_t size)
    {
        return AddValue(name, DcMsg::EType::MEMORYARRAY, value, size);
    }

    bool DcMsg::AddMessage(const char *name, DcMsg &data)
    {
        uint64_t size = 0;
        void* ptr = data.GetData(size);
        return AddValue(name, DcMsg::EType::MESSAGE, ptr, static_cast<u_int32_t>(size));;
    }

    bool DcMsg::AddMessageArray(const char *name, std::vector<DcMsg> &data)
    {
        if (data.size() == 0)
        {
            this->SetError(DcMsg::EError::InvalidArgument, "[DcMsg] Element '%s' not added: the array is empty!", name);
            return false;
        }

        size_t buffer_size = sizeof(uint32_t);
        const uint32_t data_size = static_cast<u_int32_t>(data.size());
        for (uint32_t i = 0; i < data_size; i++)
        {
            buffer_size += sizeof(uint32_t) + data[i].MemUsed();
        }

        bool rc = false;
        uint32_t idx = 0;
        if (buffer_size > MAX_STACK_VAR_SIZE)
        {
            unsigned char* buffer = (unsigned char*)malloc(buffer_size);
            if (!buffer)
            {
                this->SetError(DcMsg::EError::OutOfMemory, "[DcMsg] Error memory allocation!");
                return false;
            }
            memcpy(buffer, &data_size, sizeof(uint32_t));
            idx += sizeof(uint32_t);
            for (uint32_t i = 0; i < data_size; i++)
            {
                uint64_t m_s;
                void* m_p = data[i].GetData(m_s);
                uint32_t m_s_2 = static_cast<uint32_t>(m_s);
                memcpy(buffer + idx, &m_s_2, sizeof(uint32_t));
                idx += sizeof(uint32_t);
                memcpy(buffer + idx, m_p, m_s);
                idx += m_s;
            }
            rc = this->AddValue(name, DcMsg::EType::MESSAGEARRAY, buffer, buffer_size);
            free(buffer);
        }
        else
        {
            char buffer[buffer_size];
            memcpy(buffer, &data_size, sizeof(uint32_t));
            idx += sizeof(uint32_t);
            for (uint32_t i = 0; i < data_size; i++)
            {
                uint64_t m_s;
                void* m_p = data[i].GetData(m_s);
                uint32_t m_s_2 = static_cast<uint32_t>(m_s);
                memcpy(buffer + idx, &m_s_2, sizeof(uint32_t));
                idx += sizeof(uint32_t);
                memcpy(buffer + idx, m_p, m_s);
                idx += m_s;
            }
            rc = this->AddValue(name, DcMsg::EType::MESSAGEARRAY, buffer, buffer_size);
        }
        
        return rc;
    }

    bool DcMsg::UpdateBool(const char *name, const bool value)
    {
        return UpdateValue(name, DcMsg::EType::BOOL, value);
    }

    bool DcMsg::UpdateUByte(const char *name, const uint8_t value)
    {
        return UpdateValue(name, DcMsg::EType::UBYTE, value);
    }

    bool DcMsg::UpdateByte(const char *name, const int8_t value)
    {
        return UpdateValue(name, DcMsg::EType::BYTE, value);
    }

    bool DcMsg::UpdateUShort(const char *name, const uint16_t value)
    {
        return UpdateValue(name, DcMsg::EType::UINT, static_cast<uint32_t>(value));
    }

    bool DcMsg::UpdateShort(const char *name, const int16_t value)
    {
        return UpdateValue(name, DcMsg::EType::INT, static_cast<int32_t>(value));
    }

    bool DcMsg::UpdateUInt(const char *name, const uint32_t value)
    {
        return UpdateValue(name, DcMsg::EType::UINT, value);
    }

    bool DcMsg::UpdateInt(const char *name, const int32_t value)
    {
        return UpdateValue(name, DcMsg::EType::INT, value);
    }

    bool DcMsg::UpdateULong(const char *name, const uint64_t value)
    {
        return UpdateValue(name, DcMsg::EType::ULONG, value);
    }

    bool DcMsg::UpdateLong(const char *name, const int64_t value)
    {
        return UpdateValue(name, DcMsg::EType::LONG, value);
    }

    bool DcMsg::UpdateFloat(const char *name, const float value)
    {
        return UpdateValue(name, DcMsg::EType::FLOAT, value);
    }

    bool DcMsg::UpdateDouble(const char *name, const double value)
    {
        return UpdateValue(name, DcMsg::EType::DOUBLE, value);
    }

    bool DcMsg::GetBool(const char *name, bool &value)
    {
        return RetrieveElement(name, DcMsg::EType::BOOL, value);
    }

    bool DcMsg::GetUByte(const char *name, uint8_t &value)
    {
        return RetrieveElement(name, DcMsg::EType::UBYTE, value);
    }

    bool DcMsg::GetByte(const char *name, int8_t &value)
    {
        return RetrieveElement(name, DcMsg::EType::BYTE, value);
    }

    bool DcMsg::GetUShort(const char *name, uint16_t &value)
    {
        uint32_t val;
        const bool ok = RetrieveElement(name, DcMsg::EType::UINT, val);
        value = static_cast<uint16_t>(val);
        return ok;
    }

    bool DcMsg::GetShort(const char *name, int16_t &value)
    {
        int32_t val;
        const bool ok = RetrieveElement(name, DcMsg::EType::INT, val);
        value = static_cast<int16_t>(val);
        return ok;
    }

    bool DcMsg::GetUInt(const char *name, uint32_t &value)
    {
        return RetrieveElement(name, DcMsg::EType::UINT, value);
    }

    bool DcMsg::GetInt(const char *name, int32_t &value)
    {
        return RetrieveElement(name, DcMsg::EType::INT, value);
    }

    bool DcMsg::GetULong(const char *name, uint64_t &value)
    {
        return RetrieveElement(name, DcMsg::EType::ULONG, value);
    }

    bool DcMsg::GetLong(const char *name, int64_t &value)
    {
        return RetrieveElement(name, DcMsg::EType::LONG, value);
    }

    bool DcMsg::GetFloat(const char *name, float &value)
    {
        return RetrieveElement(name, DcMsg::EType::FLOAT, value);
    }

    bool DcMsg::GetDouble(const char *name, double &value)
    {
        return RetrieveElement(name, DcMsg::EType::DOUBLE, value);
    }

    bool DcMsg::GetString(const char *name, std::string &value)
    {
        int index = this->Find(name);
        if (index < 0) 
        {
            this->SetError(DcMsg::EError::NotFound, "[DcMsg] Element '%s' not found!", name);
            return false;
        }
        DcMsg::DType& element = this->data[index];
        if (element.type != DcMsg::EType::STRING) 
        {
            this->SetError(DcMsg::EError::TypeMismatch, "[DcMsg] Element '%s' is not type STRING!", name);
            return false;
        }
        value.resize(element.size);
        memcpy(value.data(), static_cast<unsigned char*>(this->SourceData()) + element.index, element.size);
        this->last_error = DcMsg::EError::None;
        return true;
    }

    bool DcMsg::GetBoolArray(const char *name, std::vector<bool> &data)
    {
        int index = this->Find(name);
        if (index < 0)
        {
            this->SetError(DcMsg::EError::NotFound, "[DcMsg] Element '%s' not found!", name);
            return false;
        }
        DcMsg::DType& element = this->data[index];
        if (element.type != DcMsg::EType::BOOLARRAY)
        {
            this->SetError(DcMsg::EError::TypeMismatch, "[DcMsg] Element '%s' is not type BOOL ARRAY!", name);
            return false;
        }
        unsigned char* p_current = static_cast<unsigned char*>(this->SourceData()) + element.index;
        data.resize(element.size);
        for (uint32_t i = 0; i < element.size; i++)
        {
            data[i] = p_current[i] != 0;
        }
        this->last_error = DcMsg::EError::None;
        return true;
    }

    bool DcMsg::GetUByteArray(const char *name, std::vector<uint8_t> &data)
    {
        int index = this->Find(name);
        if (index < 0)
        {
            this->SetError(DcMsg::EError::NotFound, "[DcMsg] Element '%s' not found!", name);
            return false;
        }
        DcMsg::DType& element = this->data[index];
        if (element.type != DcMsg::EType::UBYTEARRAY)
        {
            this->SetError(DcMsg::EError::TypeMismatch, "[DcMsg] Element '%s' is not type UBYTE ARRAY!", name);
            return false;
        }
        data.resize(element.size);
        memcpy(data.data(), static_cast<unsigned char*>(this->SourceData()) + element.index, element.size);
        this->last_error = DcMsg::EError::None;
        return true;
    }

    bool DcMsg::GetByteArray(const char *name, std::vector<int8_t> &data)
    {
        int index = this->Find(name);
        if (index < 0)
        {
            this->SetError(DcMsg::EError::NotFound, "[DcMsg] Element '%s' not found!", name);
            return false;
        }
        DcMsg::DType& element = this->data[index];
        if (element.type != DcMsg::EType::BYTEARRAY)
        {
            this->SetError(DcMsg::EError::TypeMismatch, "[DcMsg] Element '%s' is not type BYTE ARRAY!", name);
            return false;
        }
        data.resize(element.size);
        memcpy(data.data(), static_cast<unsigned char*>(this->SourceData()) + element.index, element.size);
        this->last_error = DcMsg::EError::None;
        return true;
    }

    bool DcMsg::GetUIntArray(const char *name, std::vector<uint32_t> &data)
    {
        int index = this->Find(name);
        if (index < 0)
        {
            this->SetError(DcMsg::EError::NotFound, "[DcMsg] Element '%s' not found!", name);
            return false;
        }
        DcMsg::DType& element = this->data[index];
        if (element.type != DcMsg::EType::UINTARRAY)
        {
            this->SetError(DcMsg::EError::TypeMismatch, "[DcMsg] Element '%s' is not type UINT ARRAY!", name);
            return false;
        }
        data.resize(element.size / sizeof(uint32_t));
        memcpy(data.data(), static_cast<unsigned char*>(this->SourceData()) + element.index, element.size);
        this->last_error = DcMsg::EError::None;
        return true;
    }

    bool DcMsg::GetIntArray(const char *name, std::vector<int32_t> &data)
    {
        int index = this->Find(name);
        if (index < 0)
        {
            this->SetError(DcMsg::EError::NotFound, "[DcMsg] Element '%s' not found!", name);
            return false;
        }
        DcMsg::DType& element = this->data[index];
        if (element.type != DcMsg::EType::INTARRAY)
        {
            this->SetError(DcMsg::EError::TypeMismatch, "[DcMsg] Element '%s' is not type INT ARRAY!", name);
            return false;
        }
        data.resize(element.size / sizeof(int32_t));
        memcpy(data.data(), static_cast<unsigned char*>(this->SourceData()) + element.index, element.size);
        this->last_error = DcMsg::EError::None;
        return true;
    }

    bool DcMsg::GetULongArray(const char *name, std::vector<uint64_t> &data)
    {
        int index = this->Find(name);
        if (index < 0)
        {
            this->SetError(DcMsg::EError::NotFound, "[DcMsg] Element '%s' not found!", name);
            return false;
        }
        DcMsg::DType& element = this->data[index];
        if (element.type != DcMsg::EType::ULONGARRAY)
        {
            this->SetError(DcMsg::EError::TypeMismatch, "[DcMsg] Element '%s' is not type ULONG ARRAY!", name);
            return false;
        }
        data.resize(element.size / sizeof(uint64_t));
        memcpy(data.data(), static_cast<unsigned char*>(this->SourceData()) + element.index, element.size);
        this->last_error = DcMsg::EError::None;
        return true;
    }

    bool DcMsg::GetLongArray(const char *name, std::vector<int64_t> &data)
    {
        int index = this->Find(name);
        if (index < 0)
        {
            this->SetError(DcMsg::EError::NotFound, "[DcMsg] Element '%s' not found!", name);
            return false;
        }
        DcMsg::DType& element = this->data[index];
        if (element.type != DcMsg::EType::LONGARRAY)
        {
            this->SetError(DcMsg::EError::TypeMismatch, "[DcMsg] Element '%s' is not type LONG ARRAY!", name);
            return false;
        }
        data.resize(element.size / sizeof(int64_t));
        memcpy(data.data(), static_cast<unsigned char*>(this->SourceData()) + element.index, element.size);
        this->last_error = DcMsg::EError::None;
        return true;
    }

    bool DcMsg::GetFloatArray(const char *name, std::vector<float> &data)
    {
        int index = this->Find(name);
        if (index < 0)
        {
            this->SetError(DcMsg::EError::NotFound, "[DcMsg] Element '%s' not found!", name);
            return false;
        }
        DcMsg::DType& element = this->data[index];
        if (element.type != DcMsg::EType::FLOATARRAY)
        {
            this->SetError(DcMsg::EError::TypeMismatch, "[DcMsg] Element '%s' is not type FLOAT ARRAY!", name);
            return false;
        }
        data.resize(element.size / sizeof(float));
        memcpy(data.data(), static_cast<unsigned char*>(this->SourceData()) + element.index, element.size);
        this->last_error = DcMsg::EError::None;
        return true;
    }

    bool DcMsg::GetDoubleArray(const char *name, std::vector<double> &data)
    {
        int index = this->Find(name);
        if (index < 0)
        {
            this->SetError(DcMsg::EError::NotFound, "[DcMsg] Element '%s' not found!", name);
            return false;
        }
        DcMsg::DType& element = this->data[index];
        if (element.type != DcMsg::EType::DOUBLEARRAY)
        {
            this->SetError(DcMsg::EError::TypeMismatch, "[DcMsg] Element '%s' is not type DOUBLE ARRAY!", name);
            return false;
        }
        data.resize(element.size / sizeof(double));
        memcpy(data.data(), static_cast<unsigned char*>(this->SourceData()) + element.index, element.size);
        this->last_error = DcMsg::EError::None;
        return true;
    }

    bool DcMsg::GetStringArray(const char *name, std::vector<std::string> &data)
    {
        int index = this->Find(name);
        if (index < 0)
        {
            this->SetError(DcMsg::EError::NotFound, "[DcMsg] Element '%s' not found!", name);
            return false;
        }
        DcMsg::DType& element = this->data[index];
        if (element.type != DcMsg::EType::STRINGARRAY)
        {
            this->SetError(DcMsg::EError::TypeMismatch, "[DcMsg] Element '%s' is not type STRING ARRAY!", name);
            return false;
        }

        unsigned char* p_current = static_cast<unsigned char*>(this->SourceData()) + element.index;
        uint32_t data_size;
        memcpy(&data_size, p_current, sizeof(uint32_t));
        p_current += sizeof(uint32_t);

        data.resize(data_size);
        for (size_t i = 0; i < data_size; i++)
        {
            uint32_t str_len;
            memcpy(&str_len, p_current, sizeof(str_len));
            p_current += sizeof(str_len);
            data[i].assign(reinterpret_cast<char*>(p_current), str_len);
            p_current += str_len;
        }

        this->last_error = DcMsg::EError::None;
        return true;
    }

    bool DcMsg::GetMemoryArray(const char *name, void **value, uint32_t &size)
    {
        int index = this->Find(name);
        if (index < 0) 
        {
            this->SetError(DcMsg::EError::NotFound, "[DcMsg] Element '%s' not found!", name);
            return false;
        }
        DcMsg::DType& element = this->data[index];
        if (element.type != DcMsg::EType::MEMORYARRAY) 
        {
            this->SetError(DcMsg::EError::TypeMismatch, "[DcMsg] Element '%s' is not type BYTES ARRAY!", name);
            return false;
        }
        *value = realloc(*value, element.size);
        memcpy(*value, static_cast<unsigned char*>(this->SourceData()) + element.index, element.size);
        size = element.size;
        this->last_error = DcMsg::EError::None;
        return true;
    }

    bool DcMsg::GetMessage(const char *name, DcMsg &data)
    {
        int index = this->Find(name);
        if (index < 0) 
        {
            this->SetError(DcMsg::EError::NotFound, "[DcMsg] Element '%s' not found!", name);
            return false;
        }
        DcMsg::DType& element = this->data[index];
        if (element.type != DcMsg::EType::MESSAGE) 
        {
            this->SetError(DcMsg::EError::TypeMismatch, "[DcMsg] Element '%s' is not type MESSAGES!", name);
            return false;
        }
        data = DcMsg(static_cast<unsigned char*>(this->SourceData()) + element.index, element.size);
        this->last_error = DcMsg::EError::None;
        return data.IsValid();
    }

    bool DcMsg::GetMessageArray(const char *name, std::vector<DcMsg> &data)
    {
        int index = this->Find(name);
        if (index < 0) 
        {
            this->SetError(DcMsg::EError::NotFound, "[DcMsg] Element '%s' not found!", name);
            return false;
        }
        DcMsg::DType& element = this->data[index];
        if (element.type != DcMsg::EType::MESSAGEARRAY) 
        {
            this->SetError(DcMsg::EError::TypeMismatch, "[DcMsg] Element '%s' is not type MESSAGES ARRAY!", name);
            return false;
        }
        
        unsigned char* p_current = static_cast<unsigned char*>(this->SourceData()) + element.index;
        uint32_t data_size;
        memcpy(&data_size, p_current, sizeof(uint32_t));
        p_current += sizeof(uint32_t);

        data.resize(data_size);
        for (size_t i = 0; i < data_size; i++)
        {
            uint32_t msg_size;
            std::memcpy(&msg_size, p_current, sizeof(msg_size));
            p_current += sizeof(msg_size);
            data[i] = DcMsg(p_current, msg_size);
            p_current += msg_size;
        }

        this->last_error = DcMsg::EError::None;
        return true;
    }

    bool DcMsg::Delete(const char *name)
    {
        if (this->readOnly)
        {
            this->SetError(DcMsg::EError::ReadOnly, "[DcMsg] Read only!");
            return false;
        }
        int idx = this->Find(name);
        if (idx < 0)
        {
            this->SetError(DcMsg::EError::NotFound, "[DcMsg] Element '%s' not found!", name);
            return false;
        }

        DcMsg::DType& element = this->data[idx];
        const uint32_t header_bytes = element.basic_type ? (DTYPE_SIZE_NAME + 1) : (DTYPE_SIZE_NAME + 1 + sizeof(uint32_t));
        const uint64_t elem_index = element.index;
        const uint64_t elem_start = elem_index - header_bytes;
        const uint64_t elem_total = header_bytes + element.size;
        const uint64_t elem_end = elem_start + elem_total;

        unsigned char* base = static_cast<unsigned char*>(this->p_data);
        if (elem_end < this->mem_used)
        {
            memmove(base + elem_start, base + elem_end, this->mem_used - elem_end);
        }
        this->mem_used -= elem_total;

        for (auto& d : this->data)
        {
            if (d.index > elem_index)
            {
                d.index -= elem_total;
            }
        }
        this->data.erase(this->data.begin() + idx);
        this->el_size--;

        uint64_t header[HEADER_ELEMENTS - 1];
        header[0] = this->mem_used;
        header[1] = this->el_size;
        uint64_t* ptr = static_cast<uint64_t*>(this->p_data);
        memcpy(ptr + 1, header, (HEADER_ELEMENTS - 1) * sizeof(uint64_t));

        this->last_error = DcMsg::EError::None;
        return true;
    }

    inline void *DcMsg::SourceData() const
    {
        return this->readOnly ? this->p_ex_data : this->p_data;
    }

    void DcMsg::SetError(DcMsg::EError code, const char *fmt, ...) const
    {
        this->last_error = code;

        char buf[512];
        va_list args;
        va_start(args, fmt);
        vsnprintf(buf, sizeof(buf), fmt, args);
        va_end(args);

        DS::Logger::Instance().LogError("%s", buf);
    }

    DcMsg::EError DcMsg::GetLastError() const
    {
        return this->last_error;
    }

    void *DcMsg::GetData() const
    {
        return this->readOnly ? this->p_ex_data : this->p_data;
    }

    void *DcMsg::GetData(uint64_t &size)
    {
        size = this->mem_used;
        return this->readOnly ? this->p_ex_data : this->p_data;
    }

    bool DcMsg::IsValid()
    {
        return this->validData;
    }

} // namespace DS