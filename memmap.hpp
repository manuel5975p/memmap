#ifndef MEMMAP_HPP
#define MEMMAP_HPP
#ifdef __unix__
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
using mmap_size_t = off_t;
#elif defined(_WIN32)
#include <Windows.h>
using mmap_size_t = DWORD;
#else
#error OS not supported
#endif
#include <cassert>
#include <exception>
#include <stdexcept>
#include <filesystem>
#include <type_traits>
template <bool v, typename T, typename R>
struct first_if {};
template <typename T, typename R>
struct first_if<true, T, R> {
    using type = T;
};
template <typename T, typename R>
struct first_if<false, T, R> {
    using type = R;
};
template <bool v, typename T, typename R>
using first_if_t = typename first_if<v, T, R>::type;
template <bool constness>
struct filemap {
private:
    first_if_t<constness, const char*, char*> m_data;
#ifdef __unix__
    int filedesc;
#elif defined(_WIN32)
    HANDLE m_file;
    HANDLE m_mapping;
#endif
    mmap_size_t m_size;

public:
    filemap() : m_data(nullptr), 
#ifdef __unix__
    filedesc(-1)
#elif defined(_MSC_VER)
    m_file(INVALID_HANDLE_VALUE), m_mapping(NULL)
#endif
    , m_size(0){};
    filemap(const std::filesystem::path& paf) : filemap(paf.c_str()) {}
    filemap(const std::string& path) : filemap(path.c_str()) {}
    filemap(const char* path)
        : m_data(nullptr),

#ifdef __unix__
          filedesc(-1)
#elif defined(_WIN32)
          m_file(INVALID_HANDLE_VALUE), m_mapping(NULL)
#endif
    {
#ifdef __unix__
        filedesc = open(path, constness ? (O_RDONLY) : (O_RDWR));
        if (filedesc == -1) {
            throw std::invalid_argument("File could not be opened");
        }
        m_size = lseek(filedesc, 0, SEEK_END);
        if (m_size == ((mmap_size_t)-1)) {
            throw std::invalid_argument("Size could not be determined");
        }
        m_data = (char*)mmap(0, m_size, constness ? (PROT_READ) : (PROT_READ | PROT_WRITE), MAP_SHARED, filedesc, 0);
        if ((void*)m_data == MAP_FAILED) {
            ::close(filedesc);
            throw std::invalid_argument("File could not be mapped");
        }
#elif defined(_WIN32)
        m_file = CreateFileA(path, constness ? GENERIC_READ : (GENERIC_READ | GENERIC_WRITE), FILE_SHARE_READ, NULL, OPEN_EXISTING,
                             FILE_ATTRIBUTE_NORMAL, NULL);

        if (m_file == INVALID_HANDLE_VALUE) {
            throw std::runtime_error("Opening file failed");
        }

        m_size = GetFileSize(m_file, NULL);
        m_mapping = CreateFileMapping(m_file, NULL, constness ? PAGE_READONLY : PAGE_READWRITE, 0, 0, NULL);

        if (m_mapping == NULL) {
            throw std::invalid_argument("Mapping failed");
        }
        m_data = (first_if_t<constness, const char*, char*>)MapViewOfFile(m_mapping, constness ? FILE_MAP_READ : FILE_MAP_WRITE, 0, 0, 0);
#endif
    }
    template <typename hoger = bool>
    std::enable_if_t<std::integral_constant<hoger, !constness>::value, char*> begin() {
        return m_data;
    }
    template <typename hoger = bool>
    std::enable_if_t<std::integral_constant<hoger, !constness>::value, char*> end() {
        return m_data + size();
    }
    const char* begin() const {
        return m_data;
    }
    const char* end() const {
        return m_data + size();
    }
    template <typename hoger = bool>
    std::enable_if_t<std::integral_constant<hoger, !constness>::value, char*> data() {
        return m_data;
    }
    const char* data() const {
        return m_data;
    }
    template <typename hoger = bool>
    std::enable_if_t<std::integral_constant<hoger, !constness>::value, char&> operator[](size_t i) {
        assert(i < m_size);
        assert(!constness);
        return m_data[i];
    }
    template <typename hoger = bool>
    std::enable_if_t<std::integral_constant<hoger, constness>::value, const char&> operator[](size_t i) const {
        assert(i < m_size);
        return m_data[i];
    }
    const char& operator[](size_t i) const {
        assert(i < m_size);
        return m_data[i];
    }
    void resize(mmap_size_t size) {
#ifdef __unix__
        assert(!constness);
        if (ftruncate(filedesc, size) == -1) {
            throw std::invalid_argument("ftruncate failed");
        }
        if (munmap((void*)m_data, m_size)) {
            throw std::invalid_argument("munmap failed");
        }
        m_size = size;
        m_data = (char*)mmap(0, m_size, constness ? (PROT_READ) : (PROT_READ | PROT_WRITE), MAP_SHARED, filedesc, 0);
        if ((void*)m_data == MAP_FAILED) {
            throw std::invalid_argument("File could not be mapped");
        }
#elif defined(_WIN32)
        assert(!constness);
        UnmapViewOfFile(m_data);
        m_data = nullptr;
        CloseHandle(m_mapping);
        m_mapping = NULL;
        SetFilePointer(m_file, size, 0, FILE_BEGIN);
        SetEndOfFile(m_file);
        m_size = size;
        m_mapping = CreateFileMapping(m_file, NULL, constness ? PAGE_READONLY : PAGE_READWRITE, 0, 0, NULL);

        if (m_mapping == NULL) {
            throw std::invalid_argument("Mapping failed");
        }
        m_data = (first_if_t<constness, const char*, char*>)MapViewOfFile(m_mapping, constness ? FILE_MAP_READ : FILE_MAP_WRITE, 0, 0, 0);
#endif
    }
    void close() {
#ifdef __unix__
        if (m_data) {
            munmap((void*)m_data, m_size);
            m_data = nullptr;
        }
        if (filedesc != -1) {
            ::close(filedesc);
            filedesc = -1;
        }
#elif defined(_WIN32)
        if (m_data) {
            UnmapViewOfFile(m_data);
            m_data = nullptr;
        }
        if (m_mapping) {
            CloseHandle(m_mapping);
            m_mapping = NULL;
        }
        if (m_file != INVALID_HANDLE_VALUE) {
            CloseHandle(m_file);
            m_file = INVALID_HANDLE_VALUE;
        }
#endif
    }
    ~filemap() {
        close();
    }
    filemap(const filemap<constness>& o) = delete;
    filemap& operator=(const filemap<constness>& o) = delete;

    filemap(filemap<constness>&& o){
        m_data = o.m_data;
        m_size = o.m_size;
        o.m_data = nullptr;
        o.m_size = 0;
        #ifdef __unix__
        filedesc = o.filedesc;
        o.filedesc = -1;
        #elif defined(_MSC_VER)
        m_file    = o.m_file;
        m_mapping = o.m_mapping;
        o.m_file = INVALID_HANDLE_VALUE;
        o.m_mapping = NULL;
        #endif
    }
    filemap& operator=(filemap<constness>&& o){
        m_data = o.m_data;
        m_size = o.m_size;
        o.m_data = nullptr;
        o.m_size = 0;
        #ifdef __unix__
        filedesc = o.filedesc;
        o.filedesc = -1;
        #elif defined(_MSC_VER)
        m_file    = o.m_file;
        m_mapping = o.m_mapping;
        o.m_file = INVALID_HANDLE_VALUE;
        o.m_mapping = NULL;
        #endif
        return *this;
    }
    mmap_size_t size() const {
        return m_size;
    }
};
using const_filemap = filemap<true>;
#endif
