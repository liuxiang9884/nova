#ifndef NNMAP_HPP
#define NNMAP_HPP

#include <unistd.h>
#include <iterator>
#include <string>
#include <system_error>
#include <cstdint>
#include <type_traits>
#include <sys/stat.h>
#include <sys/mman.h>



# define INVALID_HANDLE_VALUE -1

using file_handle_type = int;
const static file_handle_type invalid_handle = INVALID_HANDLE_VALUE;

enum class AccessMode
{
    ReadOnly = 0,
    WriteOnly = 1,
    ReadWrite = 2
};

enum { map_entire_file = 0 };

namespace detail
{
    template<typename CharT, typename S>
    struct is_c_str_helper
    {
        static constexpr bool value = std::is_same<
            CharT*,
            // TODO: I'm so sorry for this... Can this be made cleaner?
            typename std::add_pointer<
                typename std::remove_cv<
                    typename std::remove_pointer<
                        typename std::decay<
                            S
                        >::type
                    >::type
                >::type
            >::type
        >::value;
    };
    template<typename S>
    struct is_c_str
    {
        static constexpr bool value = is_c_str_helper<char, S>::value;
    };

    template<typename S>
    struct is_c_str_or_c_wstr
    {
        static constexpr bool value = is_c_str<S>::value;
    };

    inline std::error_code last_error() noexcept
    {
        std::error_code error;
        error.assign(errno, std::system_category());
        return error;
    }
    template<typename String,
    typename = decltype(std::declval<String>().empty()),
    typename = typename std::enable_if<!is_c_str_or_c_wstr<String>::value>::type> 
    bool empty(const String& path)
    {
        return path.empty();
    }
    template<
    typename String,
    typename = typename std::enable_if<is_c_str_or_c_wstr<String>::value>::type
    > bool empty(String path)
    {
        return !path || (*path == 0);
    }

    template<typename String>
    file_handle_type open_file(const String& path, const AccessMode mode,
        std::error_code& error)
    {
        error.clear();
        const auto handle = ::open(path.c_str(),
            mode == AccessMode::ReadOnly ? O_RDONLY : O_RDWR);
        if(detail::empty(path))
        {
            error = std::make_error_code(std::errc::invalid_argument);
            return invalid_handle;
        }
        if(handle == invalid_handle)
        {
            error = detail::last_error();
        }
        return handle;
    }
    inline size_t query_file_size(file_handle_type handle, std::error_code& error)
    {
        error.clear();


        struct stat sbuf;
        if(::fstat(handle, &sbuf) == -1)
        {
            error = detail::last_error();
            return 0;
        }
        return sbuf.st_size;

    }
    struct MmapContext
    {
        char* data;
        int64_t length;
        int64_t mapped_length;

    };
    inline size_t page_size()
    {
        static const size_t page_size = []
        {

            return sysconf(_SC_PAGE_SIZE);

        }();
        return page_size;
    }

    inline size_t make_offset_page_aligned(size_t offset) noexcept
    {
        const size_t page_size_ = page_size();
        // Use integer division to round down to the nearest page alignment.
        return offset / page_size_ * page_size_;
    }

    inline MmapContext memory_map(const file_handle_type file_handle, const int64_t offset,
        const int64_t length, const AccessMode mode, std::error_code& error)
    {
        const int64_t aligned_offset = make_offset_page_aligned(offset);
        const int64_t length_to_map = offset - aligned_offset + length;

        char* mapping_start = static_cast<char*>(::mmap(
                0, // Don't give hint as to where to map.
                length_to_map,
                mode == AccessMode::ReadOnly ? PROT_READ : PROT_WRITE,
                MAP_SHARED,
                file_handle,
                aligned_offset));
        if(mapping_start == MAP_FAILED)
        {
            error = detail::last_error();
            return {};
        }
        MmapContext ctx;
        ctx.data = mapping_start + offset - aligned_offset;
        ctx.length = length;
        ctx.mapped_length = length_to_map;
        return ctx;
    }
}



template<AccessMode Access_Mode, typename ByteT>
struct BasicMmap
{
    using value_type = ByteT;
    using size_type = size_t;
    using reference = value_type&;
    using const_reference = const value_type&;
    using pointer = value_type*;
    using const_pointer = const value_type*;
    using difference_type = std::ptrdiff_t;
    using iterator = pointer;
    using const_iterator = const_pointer;
    using reverse_iterator = std::reverse_iterator<iterator>;
    using const_reverse_iterator = std::reverse_iterator<const_iterator>;
    using iterator_category = std::random_access_iterator_tag;
    using handle_type = file_handle_type;

    static_assert(sizeof(ByteT) == sizeof(char), "ByteT must be the same size as char.");

private:
    // Points to the first requested byte, and not to the actual start of the mapping.
    pointer data = nullptr;

    // Length--in bytes--requested by user (which may not be the length of the
    // full mapping) and the length of the full mapping.
    size_type length = 0;
    size_type mapped_length = 0;

    // Letting user map a file using both an existing file handle and a path
    // introcudes some complexity (see `is_handle_internal_`).
    // On POSIX, we only need a file handle to create a mapping, while on
    // Windows systems the file handle is necessary to retrieve a file mapping
    // handle, but any subsequent operations on the mapped region must be done
    // through the latter.
    handle_type file_handle = INVALID_HANDLE_VALUE;

    // Letting user map a file using both an existing file handle and a path
    // introcudes some complexity in that we must not close the file handle if
    // user provided it, but we must close it if we obtained it using the
    // provided path. For this reason, this flag is used to determine when to
    // close `file_handle_`.
    bool is_handle_internal;

public:
    /**
     * The default constructed mmap object is in a non-mapped state, that is,
     * any operation that attempts to access nonexistent underlying data will
     * result in undefined behaviour/segmentation faults.
     */
    BasicMmap() = default;

    template<typename String>
    BasicMmap(const String& path, const size_type offset = 0, const size_type length = map_entire_file)
    {
        std::error_code error;
        map(path, offset, length, error);
        if(error) { throw std::system_error(error); }
    }

    /**
     * The same as invoking the `map` function, except any error that may occur
     * while establishing the mapping is wrapped in a `std::system_error` and is
     * thrown.
     */
    BasicMmap(const handle_type handle, const size_type offset = 0, const size_type length = map_entire_file)
    {
        std::error_code error;
        Map(handle, offset, length, error);
        if(error) { throw std::system_error(error); }
    }
    BasicMmap(const BasicMmap&) = delete;
    BasicMmap(BasicMmap&&);
    BasicMmap& operator=(const BasicMmap&) = delete;
    BasicMmap& operator=(BasicMmap&&);

    /**
     * If this is a read-write mapping, the destructor invokes sync. Regardless
     * of the access mode, unmap is invoked as a final step.
     */
    ~BasicMmap();

    /**
     * On UNIX systems 'file_handle' and 'mapping_handle' are the same. On Windows,
     * however, a mapped region of a file gets its own handle, which is returned by
     * 'mapping_handle'.
     */
    handle_type FileHandle() const noexcept { return file_handle; }
    handle_type MappingHandle() const noexcept;

    /** Returns whether a valid memory mapping has been created. */
    bool IsOpen() const noexcept { return file_handle != invalid_handle; }

    /**
     * Returns true if no mapping was established, that is, conceptually the
     * same as though the length that was mapped was 0. This function is
     * provided so that this class has Container semantics.
     */
    bool Empty() const noexcept { return Length() == 0; }

    /** Returns true if a mapping was established. */
    bool IsMapped() const noexcept;

    /**
     * `size` and `length` both return the logical length, i.e. the number of bytes
     * user requested to be mapped, while `mapped_length` returns the actual number of
     * bytes that were mapped which is a multiple of the underlying operating system's
     * page allocation granularity.
     */
    size_type Size() const noexcept { return Length(); }
    size_type Length() const noexcept { return length; }
    size_type MappedLength() const noexcept { return mapped_length; }

    /** Returns the offset relative to the start of the mapping. */
    size_type MappingOffset() const noexcept
    {
        return mapped_length - length;
    }

    /**
     * Returns a pointer to the first requested byte, or `nullptr` if no memory mapping
     * exists.
     */
    template<AccessMode A = Access_Mode,typename = typename std::enable_if<A == AccessMode::ReadWrite>::type> 
    pointer Data() noexcept { return data; }
    const_pointer Data() const noexcept { return data; }

    /**
     * Returns an iterator to the first requested byte, if a valid memory mapping
     * exists, otherwise this function call is undefined behaviour.
     */
    template<AccessMode A = Access_Mode, typename = typename std::enable_if<A == AccessMode::ReadWrite>::type> 
    iterator Begin() noexcept { return Data(); }
    const_iterator Begin() const noexcept { return Data(); }
    const_iterator Cbegin() const noexcept { return Data(); }

    /**
     * Returns an iterator one past the last requested byte, if a valid memory mapping
     * exists, otherwise this function call is undefined behaviour.
     */
    template<AccessMode A = Access_Mode,typename = typename std::enable_if<A == AccessMode::ReadWrite>::type> iterator end() noexcept { return Data() + Length(); }
    const_iterator End() const noexcept { return Data() + Length(); }
    const_iterator Cend() const noexcept { return Data() + Length(); }

    /**
     * Returns a reverse iterator to the last memory mapped byte, if a valid
     * memory mapping exists, otherwise this function call is undefined
     * behaviour.
     */
    template<AccessMode A = Access_Mode,typename = typename std::enable_if<A == AccessMode::ReadWrite>::type> 
    reverse_iterator Rbegin() noexcept { return reverse_iterator(end()); }
    const_reverse_iterator Rbegin() const noexcept
    { return const_reverse_iterator(End()); }
    const_reverse_iterator Crbegin() const noexcept
    { return const_reverse_iterator(End()); }

    /**
     * Returns a reverse iterator past the first mapped byte, if a valid memory
     * mapping exists, otherwise this function call is undefined behaviour.
     */
    template<AccessMode A = Access_Mode,typename = typename std::enable_if<A == AccessMode::ReadWrite>::type> 
    reverse_iterator Rend() noexcept { return reverse_iterator(Begin()); }
    const_reverse_iterator Rend() const noexcept
    { return const_reverse_iterator(Begin()); }
    const_reverse_iterator Crend() const noexcept
    { return const_reverse_iterator(Begin()); }

    /**
     * Returns a reference to the `i`th byte from the first requested byte (as returned
     * by `data`). If this is invoked when no valid memory mapping has been created
     * prior to this call, undefined behaviour ensues.
     */
    reference operator[](const size_type i) noexcept { return data[i]; }
    const_reference operator[](const size_type i) const noexcept { return data[i]; }

    /**
     * Establishes a memory mapping with AccessMode. If the mapping is unsuccesful, the
     * reason is reported via `error` and the object remains in a state as if this
     * function hadn't been called.
     *
     * `path`, which must be a path to an existing file, is used to retrieve a file
     * handle (which is closed when the object destructs or `unmap` is called), which is
     * then used to memory map the requested region. Upon failure, `error` is set to
     * indicate the reason and the object remains in an unmapped state.
     *
     * `offset` is the number of bytes, relative to the start of the file, where the
     * mapping should begin. When specifying it, there is no need to worry about
     * providing a value that is aligned with the operating system's page allocation
     * granularity. This is adjusted by the implementation such that the first requested
     * byte (as returned by `data` or `begin`), so long as `offset` is valid, will be at
     * `offset` from the start of the file.
     *
     * `length` is the number of bytes to map. It may be `map_entire_file`, in which
     * case a mapping of the entire file is created.
     */
    template<typename String>
    void Map(const String& path, const size_type offset,
            const size_type length, std::error_code& error);

    /**
     * Establishes a memory mapping with AccessMode. If the mapping is unsuccesful, the
     * reason is reported via `error` and the object remains in a state as if this
     * function hadn't been called.
     *
     * `path`, which must be a path to an existing file, is used to retrieve a file
     * handle (which is closed when the object destructs or `unmap` is called), which is
     * then used to memory map the requested region. Upon failure, `error` is set to
     * indicate the reason and the object remains in an unmapped state.
     * 
     * The entire file is mapped.
     */
    template<typename String>
    void Map(const String& path, std::error_code& error)
    {
        Map(path, 0, map_entire_file, error);
    }

    /**
     * Establishes a memory mapping with AccessMode. If the mapping is
     * unsuccesful, the reason is reported via `error` and the object remains in
     * a state as if this function hadn't been called.
     *
     * `handle`, which must be a valid file handle, which is used to memory map the
     * requested region. Upon failure, `error` is set to indicate the reason and the
     * object remains in an unmapped state.
     *
     * `offset` is the number of bytes, relative to the start of the file, where the
     * mapping should begin. When specifying it, there is no need to worry about
     * providing a value that is aligned with the operating system's page allocation
     * granularity. This is adjusted by the implementation such that the first requested
     * byte (as returned by `data` or `begin`), so long as `offset` is valid, will be at
     * `offset` from the start of the file.
     *
     * `length` is the number of bytes to map. It may be `map_entire_file`, in which
     * case a mapping of the entire file is created.
     */
    void Map(const handle_type handle, const size_type offset,
            const size_type length, std::error_code& error);

    /**
     * Establishes a memory mapping with AccessMode. If the mapping is
     * unsuccesful, the reason is reported via `error` and the object remains in
     * a state as if this function hadn't been called.
     *
     * `handle`, which must be a valid file handle, which is used to memory map the
     * requested region. Upon failure, `error` is set to indicate the reason and the
     * object remains in an unmapped state.
     * 
     * The entire file is mapped.
     */
    void Map(const handle_type handle, std::error_code& error)
    {
        Map(handle, 0, map_entire_file, error);
    }

    /**
     * If a valid memory mapping has been created prior to this call, this call
     * instructs the kernel to unmap the memory region and disassociate this object
     * from the file.
     *
     * The file handle associated with the file that is mapped is only closed if the
     * mapping was created using a file path. If, on the other hand, an existing
     * file handle was used to create the mapping, the file handle is not closed.
     */
    void Unmap();

    void Swap(BasicMmap& other);

    /** Flushes the memory mapped page to disk. Errors are reported via `error`. */
    template<AccessMode A = Access_Mode>
    typename std::enable_if<A == AccessMode::ReadWrite, void>::type
    Sync(std::error_code& error);

    /**
     * All operators compare the address of the first byte and size of the two mapped
     * regions.
     */

private:
    template<AccessMode A = Access_Mode,typename = typename std::enable_if<A == AccessMode::ReadWrite>::type> 
    pointer GetMappingStart() noexcept
    {
        return !Data() ? nullptr : Data() - MappingOffset();
    }

    const_pointer GetMappingStart() const noexcept
    {
        return !Data() ? nullptr : Data() - MappingOffset();
    }

    /**
     * The destructor syncs changes to disk if `AccessMode` is `write`, but not
     * if it's `read`, but since the destructor cannot be templated, we need to
     * do SFINAE in a dedicated function, where one syncs and the other is a noop.
     */
    template<AccessMode A = Access_Mode>
    typename std::enable_if<A == AccessMode::ReadWrite, void>::type
    ConditionalSync();
    template<AccessMode A = Access_Mode>
    typename std::enable_if<A == AccessMode::ReadOnly, void>::type
    ConditionalSync();
};

template<AccessMode Access_Mode, typename ByteT>
bool operator==(const BasicMmap<Access_Mode, ByteT>& a,
        const BasicMmap<Access_Mode, ByteT>& b);

template<AccessMode Access_Mode, typename ByteT>
bool operator!=(const BasicMmap<Access_Mode, ByteT>& a,
        const BasicMmap<Access_Mode, ByteT>& b);

template<AccessMode Access_Mode, typename ByteT>
bool operator<(const BasicMmap<Access_Mode, ByteT>& a,
        const BasicMmap<Access_Mode, ByteT>& b);

template<AccessMode Access_Mode, typename ByteT>
bool operator<=(const BasicMmap<Access_Mode, ByteT>& a,
        const BasicMmap<Access_Mode, ByteT>& b);

template<AccessMode Access_Mode, typename ByteT>
bool operator>(const BasicMmap<Access_Mode, ByteT>& a,
        const BasicMmap<Access_Mode, ByteT>& b);

template<AccessMode Access_Mode, typename ByteT>
bool operator>=(const BasicMmap<Access_Mode, ByteT>& a,
        const BasicMmap<Access_Mode, ByteT>& b);

/**
 * This is the basis for all read-only mmap objects and should be preferred over
 * directly using `basic_mmap`.
 */
template<typename ByteT>
using BasicMmapSource = BasicMmap<AccessMode::ReadOnly, ByteT>;

/**
 * This is the basis for all read-write mmap objects and should be preferred over
 * directly using `basic_mmap`.
 */
template<typename ByteT>
using BasicMmapSink = BasicMmap<AccessMode::ReadWrite, ByteT>;

using MmapSource = BasicMmapSource<char>;
using UmmapSource = BasicMmapSource<unsigned char>;

using MmapSink = BasicMmapSink<char>;
using UmmapSink = BasicMmapSink<unsigned char>;

template<typename MMap,typename MappingToken> 
MMap MakeMmap(const MappingToken& token, int64_t offset, int64_t length, std::error_code& error)
{
    MMap mmap;
    mmap.Map(token, offset, length, error);
    return mmap;
}


/**
 * Convenience factory method.
 *
 * MappingToken may be a String (`std::string`, `std::string_view`, `const char*`,
 * `std::filesystem::path`, `std::vector<char>`, or similar), or a
 * `mmap_source::handle_type`.
 */
template<typename MappingToken>
MmapSource MakeMmapSource(const MappingToken& token, MmapSource::size_type offset,
    MmapSource::size_type length, std::error_code& error)
{
    return MakeMmap<MmapSource>(token, offset, length, error);
}

template<typename MappingToken>
MmapSource MakeMmapSource(const MappingToken& token, std::error_code& error)
{
    return MakeMmapSource(token, 0, map_entire_file, error);
}

/**
 * Convenience factory method.
 *
 * MappingToken may be a String (`std::string`, `std::string_view`, `const char*`,
 * `std::filesystem::path`, `std::vector<char>`, or similar), or a
 * `mmap_sink::handle_type`.
 */
template<typename MappingToken>
MmapSink MakeMmapSink(const MappingToken& token, MmapSink::size_type offset,
    MmapSink::size_type length, std::error_code& error)
{
    return MakeMmapSink<MmapSink>(token, offset, length, error);
}

template<typename MappingToken>
MmapSink MakeMmapSink(const MappingToken& token, std::error_code& error)
{
    return MakeMmapSink(token, 0, map_entire_file, error);
}


template<AccessMode Access_Mode, typename ByteT>
BasicMmap<Access_Mode, ByteT>::~BasicMmap()
{
    ConditionalSync();
    Unmap();
}

template<AccessMode Access_Mode, typename ByteT>
BasicMmap<Access_Mode, ByteT>::BasicMmap(BasicMmap&& other)
    : data(std::move(other.data))
    , length(std::move(other.length))
    , mapped_length(std::move(other.mapped_length))
    , file_handle(std::move(other.file_handle))
    , is_handle_internal(std::move(other.is_handle_internal))
{
    other.data = nullptr;
    other.length = other.mapped_length = 0;
    other.file_handle = invalid_handle;
}

template<AccessMode Access_Mode, typename ByteT>
BasicMmap<Access_Mode, ByteT>&
BasicMmap<Access_Mode, ByteT>::operator=(BasicMmap&& other)
{
    if(this != &other)
    {
        // First the existing mapping needs to be removed.
        Unmap();
        data = std::move(other.data);
        length = std::move(other.length);
        mapped_length = std::move(other.mapped_length);
        file_handle = std::move(other.file_handle);
        is_handle_internal = std::move(other.is_handle_internal);

        // The moved from basic_mmap's fields need to be reset, because
        // otherwise other's destructor will unmap the same mapping that was
        // just moved into this.
        other.data = nullptr;
        other.length = other.mapped_length = 0;
        other.is_handle_internal = false;
    }
    return *this;
}

template<AccessMode Access_Mode, typename ByteT>
typename BasicMmap<Access_Mode, ByteT>::handle_type
BasicMmap<Access_Mode, ByteT>::MappingHandle() const noexcept
{
    return file_handle;
}

template<AccessMode Access_Mode, typename ByteT>
template<typename String>
void BasicMmap<Access_Mode, ByteT>::Map(const String& path, const size_type offset,
        const size_type length, std::error_code& error)
{
    error.clear();
    if(detail::empty(path))
    {
        error = std::make_error_code(std::errc::invalid_argument);
        return;
    }
    const auto handle = detail::open_file(path, AccessMode::ReadWrite, error);
    if(error)
    {
        return;
    }

    Map(handle, offset, length, error);
    // This MUST be after the call to map, as that sets this to true.
    if(!error)
    {
        is_handle_internal = true;
    }
}

template<AccessMode Access_Mode, typename ByteT>
void BasicMmap<Access_Mode, ByteT>::Map(const handle_type handle,
        const size_type offset, const size_type length, std::error_code& error)
{
    error.clear();
    if(handle == invalid_handle)
    {
        error = std::make_error_code(std::errc::bad_file_descriptor);
        return;
    }

    const auto file_size = detail::query_file_size(handle, error);
    if(error)
    {
        return;
    }

    if(offset + length > file_size)
    {
        error = std::make_error_code(std::errc::invalid_argument);
        return;
    }

    const auto ctx = detail::memory_map(handle, offset,
            length == map_entire_file ? (file_size - offset) : length,
            Access_Mode, error);
    if(!error)
    {
        // We must unmap the previous mapping that may have existed prior to this call.
        // Note that this must only be invoked after a new mapping has been created in
        // order to provide the strong guarantee that, should the new mapping fail, the
        // `map` function leaves this instance in a state as though the function had
        // never been invoked.
        Unmap();
        file_handle = handle;
        is_handle_internal = false;
        data = reinterpret_cast<pointer>(ctx.data);
        this->length = ctx.length;
        mapped_length = ctx.mapped_length;
    }
}

template<AccessMode Access_Mode, typename ByteT>
template<AccessMode A>
typename std::enable_if<A == AccessMode::ReadWrite, void>::type
BasicMmap<Access_Mode, ByteT>::Sync(std::error_code& error)
{
    error.clear();
    if(!IsOpen())
    {
        error = std::make_error_code(std::errc::bad_file_descriptor);
        return;
    }

    if(Data())
    {

        if(::msync(GetMappingStart(), mapped_length, MS_SYNC) != 0)
        {
            error = detail::last_error();
            return;
        }
    }
}

template<AccessMode Access_Mode, typename ByteT>
void BasicMmap<Access_Mode, ByteT>::Unmap()
{
    if(!IsOpen()) { return; }
    // TODO do we care about errors here?
    if(data) { ::munmap(const_cast<pointer>(GetMappingStart()), mapped_length); }

    // If `file_handle_` was obtained by our opening it (when map is called with
    // a path, rather than an existing file handle), we need to close it,
    // otherwise it must not be closed as it may still be used outside this
    // instance.
    if(is_handle_internal)
    {
        ::close(file_handle);
    }

    // Reset fields to their default values.
    data = nullptr;
    length = mapped_length = 0;
    file_handle = invalid_handle;
}

template<AccessMode Access_Mode, typename ByteT>
bool BasicMmap<Access_Mode, ByteT>::IsMapped() const noexcept
{
    return IsOpen();
}

template<AccessMode Access_Mode, typename ByteT>
void BasicMmap<Access_Mode, ByteT>::Swap(BasicMmap& other)
{
    if(this != &other)
    {
        using std::swap;
        swap(data, other.data);
        swap(file_handle, other.file_handle);

        swap(length, other.length);
        swap(mapped_length, other.mapped_length);
        swap(is_handle_internal, other.is_handle_internal);
    }
}

template<AccessMode Access_Mode, typename ByteT>
template<AccessMode A>
typename std::enable_if<A == AccessMode::ReadWrite, void>::type
BasicMmap<Access_Mode, ByteT>::ConditionalSync()
{
    // This is invoked from the destructor, so not much we can do about
    // failures here.
    std::error_code ec;
    Sync(ec);
}

template<AccessMode Access_Mode, typename ByteT>
template<AccessMode A>
typename std::enable_if<A == AccessMode::ReadOnly, void>::type
BasicMmap<Access_Mode, ByteT>::ConditionalSync()
{
    // noop
}

template<AccessMode Access_Mode, typename ByteT>
bool operator==(const BasicMmap<Access_Mode, ByteT>& a,
        const BasicMmap<Access_Mode, ByteT>& b)
{
    return a.Data() == b.Data()
        && a.Size() == b.Size();
}

template<AccessMode Access_Mode, typename ByteT>
bool operator!=(const BasicMmap<Access_Mode, ByteT>& a,
        const BasicMmap<Access_Mode, ByteT>& b)
{
    return !(a == b);
}

template<AccessMode Access_Mode, typename ByteT>
bool operator<(const BasicMmap<Access_Mode, ByteT>& a,
        const BasicMmap<Access_Mode, ByteT>& b)
{
    if(a.Data() == b.Data()) { return a.Size() < b.Size(); }
    return a.Data() < b.Data();
}

template<AccessMode Access_Mode, typename ByteT>
bool operator<=(const BasicMmap<Access_Mode, ByteT>& a,
        const BasicMmap<Access_Mode, ByteT>& b)
{
    return !(a > b);
}

template<AccessMode Access_Mode, typename ByteT>
bool operator>(const BasicMmap<Access_Mode, ByteT>& a,
        const BasicMmap<Access_Mode, ByteT>& b)
{
    if(a.Data() == b.Data()) { return a.Size() > b.Size(); }
    return a.Data() > b.Data();
}

template<AccessMode Access_Mode, typename ByteT>
bool operator>=(const BasicMmap<Access_Mode, ByteT>& a,
        const BasicMmap<Access_Mode, ByteT>& b)
{
    return !(a < b);
}
#endif
