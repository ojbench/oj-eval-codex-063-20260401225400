#ifndef BPT_MEMORYRIVER_HPP
#define BPT_MEMORYRIVER_HPP

#include <fstream>
#include <string>

using std::string;
using std::fstream;
using std::ifstream;
using std::ofstream;

// File-backed storage with space reclamation via a free list.
// Layout (bytes):
// [info_len ints for user][1 int free_list_head][- blocks of T ...]
// - free_list_head stores the byte offset of the first free block, or -1 if none.
// - A freed block stores the next free offset as an int at its start.
// - Indices returned by write() are byte offsets to the beginning of T.

template<class T, int info_len = 2>
class MemoryRiver {
private:
    fstream file;
    string file_name;
    int sizeofT = sizeof(T);

    static constexpr int extra_header_ints = 1; // internal: free list head

    std::streamoff user_header_bytes() const { return static_cast<std::streamoff>(info_len) * sizeof(int); }
    std::streamoff free_head_pos() const { return user_header_bytes(); }
    std::streamoff full_header_bytes() const { return user_header_bytes() + static_cast<std::streamoff>(extra_header_ints) * sizeof(int); }

    void open_io() {
        file.open(file_name, std::ios::in | std::ios::out | std::ios::binary);
        if (!file.is_open()) {
            file.clear();
            ofstream creator(file_name, std::ios::binary | std::ios::trunc);
            int zero = 0;
            for (int i = 0; i < info_len; ++i) creator.write(reinterpret_cast<char*>(&zero), sizeof(int));
            creator.close();
            file.open(file_name, std::ios::in | std::ios::out | std::ios::binary);
        }
        ensure_free_head_initialized();
    }

    void close_io() { if (file.is_open()) file.close(); }

    // Ensure the extra header (free list head) exists; initialize to -1 if absent.
    void ensure_free_head_initialized() {
        file.seekg(0, std::ios::end);
        std::streamoff sz = file.tellg();
        if (sz < full_header_bytes()) {
            int neg1 = -1;
            file.clear();
            file.seekp(user_header_bytes(), std::ios::beg);
            file.write(reinterpret_cast<char*>(&neg1), sizeof(int));
            file.flush();
        }
    }

    int read_free_head() {
        int head;
        file.seekg(free_head_pos(), std::ios::beg);
        file.read(reinterpret_cast<char*>(&head), sizeof(int));
        return head;
    }

    void write_free_head(int head) {
        file.seekp(free_head_pos(), std::ios::beg);
        file.write(reinterpret_cast<char*>(&head), sizeof(int));
        file.flush();
    }

public:
    MemoryRiver() = default;

    explicit MemoryRiver(const string& file_name) : file_name(file_name) {}

    void initialise(string FN = ) {
        if (FN != ) file_name = FN;
        ofstream out(file_name, std::ios::binary | std::ios::trunc);
        int tmp = 0;
        for (int i = 0; i < info_len; ++i)
            out.write(reinterpret_cast<char *>(&tmp), sizeof(int));
        int neg1 = -1; // free list head
        out.write(reinterpret_cast<char*>(&neg1), sizeof(int));
        out.close();
    }

    // 读出第n个int的值赋给tmp，1_base
    void get_info(int &tmp, int n) {
        if (n <= 0 || n > info_len) return;
        open_io();
        file.seekg(static_cast<std::streamoff>(n - 1) * sizeof(int), std::ios::beg);
        file.read(reinterpret_cast<char*>(&tmp), sizeof(int));
        close_io();
    }

    // 将tmp写入第n个int的位置，1_base
    void write_info(int tmp, int n) {
        if (n <= 0 || n > info_len) return;
        open_io();
        file.seekp(static_cast<std::streamoff>(n - 1) * sizeof(int), std::ios::beg);
        file.write(reinterpret_cast<char*>(&tmp), sizeof(int));
        file.flush();
        close_io();
    }

    // 写入对象，返回起始位置索引
    int write(T &t) {
        open_io();
        int head = read_free_head();
        int index;
        if (head == -1) {
            file.seekp(0, std::ios::end);
            index = static_cast<int>(file.tellp());
            file.write(reinterpret_cast<char*>(&t), sizeof(T));
            file.flush();
        } else {
            index = head;
            int next;
            file.seekg(index, std::ios::beg);
            file.read(reinterpret_cast<char*>(&next), sizeof(int));
            file.seekp(index, std::ios::beg);
            file.write(reinterpret_cast<char*>(&t), sizeof(T));
            write_free_head(next);
        }
        close_io();
        return index;
    }

    // 覆写指定位置的对象
    void update(T &t, const int index) {
        open_io();
        file.seekp(static_cast<std::streamoff>(index), std::ios::beg);
        file.write(reinterpret_cast<char*>(&t), sizeof(T));
        file.flush();
        close_io();
    }

    // 读取指定位置的对象
    void read(T &t, const int index) {
        open_io();
        file.seekg(static_cast<std::streamoff>(index), std::ios::beg);
        file.read(reinterpret_cast<char*>(&t), sizeof(T));
        close_io();
    }

    // 删除（回收到空闲链表）
    void Delete(int index) {
        open_io();
        int head = read_free_head();
        file.seekp(static_cast<std::streamoff>(index), std::ios::beg);
        file.write(reinterpret_cast<char*>(&head), sizeof(int));
        write_free_head(index);
        file.flush();
        close_io();
    }
};


#endif //BPT_MEMORYRIVER_HPP
