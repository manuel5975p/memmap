#ifndef MEMMAP_HPP
#define MEMMAP_HPP
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <filesystem>
#include <type_traits>
#include <exception>
#include <cassert>
template<bool v, typename T, typename R>
struct first_if{};
template<typename T, typename R>
struct first_if<true, T, R>{
    using type = T;
};
template<typename T, typename R>
struct first_if<false, T, R>{
    using type = R;
};
template<bool v, typename T, typename R>
using first_if_t = typename first_if<v, T, R>::type;
template<bool constness>
struct filemap{
    private:
    first_if_t<constness, const char*, char*> data;
    int filedesc;
    off_t m_size;
    public:
    filemap(const std::filesystem::path& paf) : filemap(paf.c_str()){}
    filemap(const std::string& path) : filemap(path.c_str()){}
    filemap(const char* path){
        filedesc = open(path, constness ? (O_RDONLY) : (O_RDWR));
        if(filedesc == -1){
            throw std::invalid_argument("File could not be opened");
        }
        m_size = lseek(filedesc, 0, SEEK_END);
        if(m_size == ((off_t)-1)){
            throw std::invalid_argument("Size could not be determined");
        }
        data = (char*)mmap(0, m_size, constness ? (PROT_READ) : (PROT_READ | PROT_WRITE), MAP_SHARED, filedesc, 0);
        if((void*)data == MAP_FAILED){
            close(filedesc);
            throw std::invalid_argument("File could not be mapped");
        }
    }
    template<typename hoger = bool>
    std::enable_if_t<std::integral_constant<hoger, !constness>::value, char*> begin(){
        return data;
    }
    template<typename hoger = bool>
    std::enable_if_t<std::integral_constant<hoger, !constness>::value, char*> end(){
        return data + size();
    }
    const char* begin()const{
        return data;
    }
    const char* end()const{
        return data + size();
    }
    template<typename hoger = bool>
    std::enable_if_t<std::integral_constant<hoger, !constness>::value, char&> operator[](size_t i){
        assert(i < m_size);
        assert(!constness);
        return data[i];
    }
    template<typename hoger = bool>
    std::enable_if_t<std::integral_constant<hoger, constness>::value, const char&> operator[](size_t i)const {
        assert(i < m_size);
        return data[i];
    }
    const char& operator[](size_t i)const{
        assert(i < m_size);
        return data[i];
    }
    void resize(off_t size){
        assert(!constness);
        if(ftruncate(filedesc, size) == -1){
            throw std::invalid_argument("ftruncate failed");
        }
        if(munmap((void*)data, m_size)){
            throw std::invalid_argument("munmap failed");
        }
        m_size = size;
        data = (char*)mmap(0, m_size, constness ? (PROT_READ) : (PROT_READ | PROT_WRITE), MAP_SHARED, filedesc, 0);
        if((void*)data == MAP_FAILED){
            throw std::invalid_argument("File could not be mapped");
        }
    }
    ~filemap(){
        munmap((void*)data, m_size);
        close(filedesc);
    }
    off_t size()const{
        return m_size;
    }
};
using const_filemap = filemap<true>;
#endif