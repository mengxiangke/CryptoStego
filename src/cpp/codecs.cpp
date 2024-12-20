#include <vector>
#include <string>
#include <cstdint>
#include <algorithm>
#include <numeric>
#include <unordered_map>
#include <unordered_set>
#include <bitset>
#include <cmath>
#include <cstring>
#include <random>
#include <iostream>
#include <iomanip>
#include <cassert>
#include <emscripten.h>
#include <emscripten/val.h>
#include <emscripten/bind.h>

// Helper function to generate a deterministic permutation based on password using mt19937
// LinkedList class for permute
class LinkedList {
private:
    // Node structure
    struct Node {
        uint16_t value;
        Node* next;
        Node* prev;
        Node(uint16_t val) : value(val), next(nullptr), prev(nullptr) {}
    };

    Node* head;
    Node* tail;

public:
    // Constructor: initializes empty list
    LinkedList() : head(nullptr), tail(nullptr) {}

    // Constructor: builds list from vector
    LinkedList(const std::vector<uint16_t>& vec) : head(nullptr), tail(nullptr) {
        for (uint16_t val : vec) {
            append(val);
        }
    }

    // Destructor: frees all nodes
    ~LinkedList() {
        Node* current = head;
        while (current) {
            Node* tmp = current;
            current = current->next;
            delete tmp;
        }
    }

    // Append a value to the end of the list
    void append(uint16_t val) {
        Node* new_node = new Node(val);
        if (!head) { // Empty list
            head = tail = new_node;
        } else { // Non-empty list
            tail->next = new_node;
            new_node->prev = tail;
            tail = new_node;
        }
    }

    // Iterator class
    class Iterator {
    private:
        LinkedList* list;
        Node* current;
    public:
        // Constructor
        Iterator(LinkedList* lst, Node* node) : list(lst), current(node) {}

        // Dereference operator
        uint16_t& operator*() const {
            return current->value;
        }

        // Pre-increment
        Iterator& operator++() {
            if (current) current = current->next;
            return *this;
        }

        // Post-increment
        Iterator operator++(int) {
            Iterator tmp = *this;
            ++(*this);
            return tmp;
        }

        // Equality comparison
        bool operator==(const Iterator& other) const {
            return current == other.current;
        }

        // Inequality comparison
        bool operator!=(const Iterator& other) const {
            return current != other.current;
        }

        // Delete current node and move iterator to next node
        Iterator& delete_current() {
            if (!current) return *this; // Nothing to delete

            Node* node_to_delete = current;
            // Move current to next before deletion
            current = current->next;

            // Update links
            if (node_to_delete->prev) {
                node_to_delete->prev->next = node_to_delete->next;
            } else { // Deleting head
                list->head = node_to_delete->next;
            }

            if (node_to_delete->next) {
                node_to_delete->next->prev = node_to_delete->prev;
            } else { // Deleting tail
                list->tail = node_to_delete->prev;
            }

            delete node_to_delete;
            return *this;
        }
    };

    // Begin iterator
    Iterator begin() {
        return Iterator(this, head);
    }

    // End iterator
    Iterator end() {
        return Iterator(this, nullptr);
    }
};

std::vector<float> resize_linear(const std::vector<float>& src, int src_width, int src_height,
                                 int dst_width, int dst_height) {
    std::vector<float> dst(dst_width * dst_height, 0.0f);
    float x_ratio = static_cast<float>(src_width) / dst_width;
    float y_ratio = static_cast<float>(src_height) / dst_height;

    for(int dst_y = 0; dst_y < dst_height; ++dst_y) {
        float src_y = (dst_y + 0.5f) * y_ratio - 0.5f;
        int y0 = static_cast<int>(std::floor(src_y));
        int y1 = y0 + 1;
        float dy = src_y - y0;

        y0 = std::max(0, std::min(y0, src_height - 1));
        y1 = std::max(0, std::min(y1, src_height - 1));

        for(int dst_x = 0; dst_x < dst_width; ++dst_x) {
            float src_x = (dst_x + 0.5f) * x_ratio - 0.5f;
            int x0 = static_cast<int>(std::floor(src_x));
            int x1 = x0 + 1;
            float dx = src_x - x0;

            x0 = std::max(0, std::min(x0, src_width - 1));
            x1 = std::max(0, std::min(x1, src_width - 1));

            // Bilinear interpolation
            float top = src[y0 * src_width + x0] * (1 - dx) + src[y0 * src_width + x1] * dx;
            float bottom = src[y1 * src_width + x0] * (1 - dx) + src[y1 * src_width + x1] * dx;
            float value = top * (1 - dy) + bottom * dy;

            dst[dst_y * dst_width + dst_x] = value;
        }
    }

    return dst;
}

// Function to perform nearest neighbor resizing
std::vector<float> resize_nearest(const std::vector<float>& src, int src_width, int src_height,
                                 int dst_width, int dst_height) {
    std::vector<float> dst(dst_width * dst_height, 0.0f);
    float x_ratio = static_cast<float>(src_width) / dst_width;
    float y_ratio = static_cast<float>(src_height) / dst_height;

    for(int dst_y = 0; dst_y < dst_height; ++dst_y) {
        int src_y = static_cast<int>(std::round((dst_y + 0.5f) * y_ratio - 0.5f));
        src_y = std::max(0, std::min(src_y, src_height - 1));

        for(int dst_x = 0; dst_x < dst_width; ++dst_x) {
            int src_x = static_cast<int>(std::round((dst_x + 0.5f) * x_ratio - 0.5f));
            src_x = std::max(0, std::min(src_x, src_width - 1));

            dst[dst_y * dst_width + dst_x] = src[src_y * src_width + src_x];
        }
    }

    return dst;
}

// Function to calculate the score as described
double calculate_score(const std::vector<float>& x, int width = 256, int height = 256) {
    assert(x.size() == width * height && "Input vector size does not match width and height.");

    // Step 1: Resize x to 128x128 using linear interpolation
    int resized_width = 128;
    int resized_height = 128;
    std::vector<float> y_resized = resize_linear(x, width, height, resized_width, resized_height);

    // Step 2: Threshold y_resized at 0.5 to get y as float (0.0f or 1.0f)
    std::vector<float> y(y_resized.size(), 0.0f);
    for(size_t i = 0; i < y_resized.size(); ++i) {
        y[i] = (y_resized[i] > 0.5f) ? 1.0f : 0.0f;
    }

    // Step 3: Resize y to 256x256 using nearest neighbor to get z as int (0 or 1)
    std::vector<float> z_float = resize_nearest(y, resized_width, resized_height, width, height);
    std::vector<int> z(z_float.size(), 0);
    for(size_t i = 0; i < z_float.size(); ++i) {
        z[i] = static_cast<int>(z_float[i]);
    }

    // Step 4: Compare z with original x (cast to int) and compute the mean of equal elements
    int equal_count = 0;
    for(int i = 0; i < width * height; ++i) {
        int x_int = static_cast<int>(x[i]);
        if(x_int == z[i]) {
            equal_count++;
        }
    }

    double mean = static_cast<double>(equal_count) / (width * height);
    return mean;
}

// Helper function to generate a deterministic permutation based on password using mt19937
std::vector<uint16_t> generate_permutation(const std::string& password, const uint8_t shift) {
    std::vector<uint16_t> O(65536);
    std::iota(O.begin(), O.end(), 0);

    // Create a seed from the password using a hash function
    std::hash<std::string> hasher;
    size_t seed = hasher(password) + shift;

    // Initialize mt19937 with the seed (use lower 32 bits)
    std::mt19937 rng(static_cast<uint32_t>(seed));

    // Shuffle the vector O using mt19937
    std::shuffle(O.begin(), O.end(), rng);

    return O;
}

// Helper function to convert bytes to bits
std::vector<uint8_t> bytes_to_bits(const std::vector<uint8_t>& bytes) {
    std::vector<uint8_t> bits;
    bits.reserve(bytes.size() * 8);
    for (auto byte : bytes) {
        for (int i = 7; i >= 0; --i) {
            bits.push_back((byte >> i) & 1);
        }
    }
    return bits;
}

// Helper function to create a 10-bit unsigned integer from bits
uint16_t make_unsigned_int_10bit(const std::vector<uint8_t>& bits) {
    uint16_t value = 0;
    for (auto bit : bits) {
        value = (value << 1) | (bit & 1);
    }
    return value;
}

// Encode function
std::vector<uint8_t> encode(const std::vector<uint8_t>& raw_data, const std::string& password) {
    // Step 1: Check raw_data size
    if (raw_data.size() > 1023) {
        // Encode error: raw_data too large
        return std::vector<uint8_t>();
    }

    // Step 2: Generate permutation O based on password using mt19937
    std::vector<uint16_t> O = generate_permutation(password, 0);

    // calculate the repeats
    uint16_t L = static_cast<uint16_t>(raw_data.size());
    uint16_t repeat = 65536 / (L * 8 + 90 + 27); // 9 bit repr for each repeat of count and shift bits.

    // Step 3: Encode the length L as 10-bit unsigned integer
    std::bitset<10> L_bits_set(L);
    std::vector<uint8_t> L_bit_array;
    L_bit_array.reserve(10);
    for (int i = 9; i >= 0; --i) {
        L_bit_array.push_back(L_bits_set[i]);
    }

    // Step 4: Convert raw_data to bit array D
    std::vector<uint8_t> D = bytes_to_bits(raw_data);
    std::unordered_set<uint16_t> Oindex;

    // Step 5: Compose raw_bit_message as L + L + L ... (repeat times)
    std::vector<uint8_t> raw_bit_message;
    raw_bit_message.reserve(10 * repeat);
    for (uint16_t i=0; i< repeat; i++){
        raw_bit_message.insert(raw_bit_message.end(), L_bit_array.begin(), L_bit_array.end());
    }

    // Step 6: Initialize P as a vector of 65536 zeros
    std::vector<uint8_t> P(65536, 0);

    // Step 7: Encode each bit 9 times
    for (size_t i = 0; i < raw_bit_message.size(); ++i) {
        uint8_t bit = raw_bit_message[i];
        for (int k = 0; k < 9; ++k) {
            size_t pos = i * 9 + k;
            if (pos >= 65536) break; // Prevent out-of-bounds
            uint16_t index = O[pos];
            Oindex.insert(index);
            P[index] = bit;
        }
    }

    // calculate repeat times
    double max_score = 0;
    std::vector<uint8_t> best_P;

    for(uint8_t shift_idx = 0; shift_idx<8; shift_idx++){
        std::vector<uint8_t> PRep(P);
        std::bitset<3> shift(shift_idx);
        std::vector<uint8_t> shift_bit_array;
        shift_bit_array.reserve(3);
        for (int j = 2; j >= 0; --j) {
            shift_bit_array.push_back(shift[j]);
        }
        std::vector<uint8_t> shift_bit_msg;
        shift_bit_msg.reserve(3 * repeat);
        for (uint16_t i=0; i< repeat; i++){
            shift_bit_msg.insert(shift_bit_msg.end(), shift_bit_array.begin(), shift_bit_array.end());
        }

        for (size_t i = 0; i < shift_bit_msg.size(); ++i) {
            uint8_t bit = shift_bit_msg[i];
            for (int k = 0; k < 9; ++k) {
                size_t pos = i * 9 + k + 90 * repeat;
                if (pos >= 65536) break; // Prevent out-of-bounds
                uint16_t index = O[pos];
                Oindex.insert(index);
                PRep[index] = bit;
            }
        }

        uint32_t current_end_loc = 65535;
        std::vector<uint16_t> O1 = generate_permutation(password, shift_idx + 1);
        //generate orders
        LinkedList O1_LL(O1);
        for (auto it = O1_LL.begin(); it != O1_LL.end(); ) {
            if (Oindex.find(*it) != Oindex.end()) {
                // ocupied loc
                it.delete_current();
            } else {
                ++it;
            }
        }
        auto itO1_LL = O1_LL.begin();
        for (size_t i = 0; i < D.size(); ++i) {
            for (int k = 0; k < repeat; ++k) {
                size_t pos = i * repeat + k;
                if (itO1_LL == O1_LL.end()) break; // Prevent out-of-bounds
                PRep[*(itO1_LL++)] = D[i];
            }
        }

        std::vector<float> floatVec(65536);
        std::transform(PRep.begin(), PRep.end(), floatVec.begin(), [](uint8_t x) { return (float)x;});
        double p_score = calculate_score(floatVec, 256, 256);
        if(p_score > max_score){
            max_score = p_score;
            best_P = std::move(PRep);
        }
    }
    return best_P;
}

// Decode function
std::vector<uint8_t> decode(const std::vector<float>& coded_data, const std::string& password) {
    // Step 1: Check coded_data size
    if (coded_data.size() != 65536) {
        // Decode error: invalid coded_data size
        return std::vector<uint8_t>();
    }

    // Step 2: Generate permutation O based on password using mt19937
    LinkedList OLL(generate_permutation(password, 0));
    LinkedList* LL_ptr = &OLL;
    LinkedList::Iterator OLLit = LL_ptr->begin();
    LinkedList::Iterator* LLit_ptr = &OLLit;

    std::unordered_set<uint16_t> Oindex;
    uint32_t current_end_loc = 65535;

    // Step 3: Define decode_bit as a lambda function
    auto decode_bit = [&](std::vector<uint8_t>& bits_out, size_t repeat, bool set_oindex=false) -> uint8_t {
        std::vector<uint8_t> bit_1_01;
        std::vector<float> fc_prob_1_01;
        bit_1_01.reserve(repeat);
        fc_prob_1_01.reserve(repeat);

        // Read 9 bits
        for (int i = 0; i < repeat; ++i) {
            if (*LLit_ptr == LL_ptr->end()) break; // Prevent out-of-bounds

            uint16_t index = *((*LLit_ptr)++);

            if(set_oindex) Oindex.insert(index);

            float prob = coded_data[index];
            uint8_t bit = (prob > 0.0f) ? 1 : 0;
            bit_1_01.push_back(bit);
            fc_prob_1_01.push_back(prob);
        }

        // Count occurrences of 0 and 1
        std::unordered_map<uint8_t, int> bit_count;
        for (auto bit : bit_1_01) {
            bit_count[bit]++;
        }

        // Determine majority bit
        uint8_t majority_bit = 0;
        int max_count = 0;
        for (const auto& pair : bit_count) {
            if (pair.second > max_count) {
                max_count = pair.second;
                majority_bit = pair.first;
            }
        }

        if (max_count >= 0.7 * repeat) { // At least 70% bits of the same number
            return majority_bit;
        }

        // If no majority, compute sigmoid(mean) > 0.5
        float sum_sigmoid = 0.0f;
        for (auto bit : fc_prob_1_01) {
            // Since bits are 0 or 1, sigmoid(bit) is sigmoid(0)=0.5 or sigmoid(1)=~0.731
            sum_sigmoid += 1.0f / (1.0f + std::exp(-bit));
        }
        float mean_sigmoid = sum_sigmoid / bit_1_01.size();
        return (mean_sigmoid > 0.5f) ? 1 : 0;
    };

    // Step 4: Decode the length L by reading it 7 times, then up to repeat times
    uint16_t repeat = 7;
    uint16_t dlen = 0;
    std::unordered_map<uint16_t, uint16_t> decoded_len_count;

    for (int i = 0; i < repeat; ++i) {
        std::vector<uint8_t> int_bits;
        int_bits.reserve(10);
        for (int p = 0; p < 10; ++p) {
            uint8_t bit = decode_bit(int_bits, 9, true);
            int_bits.push_back(bit);
        }
        uint16_t uint_len = make_unsigned_int_10bit(int_bits);
        decoded_len_count[uint_len]++;
        if(i > 5){
            // calculate updated repeat
            uint16_t max_len_count = 0;
            for (const auto& pair : decoded_len_count) {
                if (pair.second > max_len_count) {
                    max_len_count = pair.second;
                    dlen = pair.first;
                }
            }
            if(max_len_count == 1) repeat++; else repeat = 65536 / (dlen * 8 + 90 + 27);
            if(repeat > 560 || repeat <= i) return std::vector<uint8_t>();    // 65536 / (90+27) =560.1
        }
    }

    // Step 4/5 rep, decode shift
    std::vector<uint16_t> decoded_shift;
    for (int i = 0; i < repeat; ++i) {
        std::vector<uint8_t> int_bits;
        int_bits.reserve(3);
        for (int p = 0; p < 3; ++p) {
            uint8_t bit = decode_bit(int_bits, 9, true);
            int_bits.push_back(bit);
        }
        uint16_t uint_shift = make_unsigned_int_10bit(int_bits);
        decoded_shift.push_back(uint_shift);
    }
    // std::cout << "shift decode: " << O[current_pos] << std::endl;

    // Step 5: Determine the most common L
    std::unordered_map<uint16_t, int> decoded_shift_count;
    for (auto len : decoded_shift) {
        decoded_shift_count[len]++;
    }

    uint16_t dshift = 0;
    int max_shift_count = 0;
    for (const auto& pair : decoded_shift_count) {
        if (pair.second > max_shift_count) {
            max_shift_count = pair.second;
            dshift = pair.first;
        }
    }

    if (max_shift_count < 2) { // No common value
        return std::vector<uint8_t>(); // Decode failed
    }
    // std::cout << "sft " << dshift << std::endl;
    LinkedList O1LL(generate_permutation(password, dshift + 1));
    LL_ptr = &O1LL;
    for (auto it = LL_ptr->begin(); it != LL_ptr->end(); ) {
        if (Oindex.find(*it) != Oindex.end()) {
            // ocupied loc
            it.delete_current();
        } else {
            ++it;
        }
    }

    LinkedList::Iterator O1LLit = LL_ptr->begin();
    LLit_ptr = &O1LLit;

    // Step 6: Decode the message based on dlen
    std::vector<uint8_t> msg;
    msg.reserve(dlen);
    for (size_t i = 0; i < dlen; ++i) {
        std::vector<uint8_t> byte_bits;
        byte_bits.reserve(8);
        for (int j = 0; j < 8; ++j) {
            uint8_t bit = decode_bit(byte_bits, repeat, false);
            byte_bits.push_back(bit);
        }
        // Convert bits to byte
        uint8_t byte = 0;
        for (auto bit : byte_bits) {
            byte = (byte << 1) | (bit & 1);
        }
        msg.push_back(byte);
    }

    return msg;
}

// Emscripten interface functions
emscripten::val encodeToBits(emscripten::val data_bytes, emscripten::val password) {
    // Convert data_bytes to std::vector<uint8_t>
    std::vector<uint8_t> raw_data = emscripten::convertJSArrayToNumberVector<uint8_t>(data_bytes);

    // Convert password to std::string
    std::string pwd = password.as<std::string>();

    // Call encode function
    std::vector<uint8_t> P = encode(raw_data, pwd);

    // If P is empty, return null
    if (P.empty()) {
        return emscripten::val::null();
    }

    // Convert P to JS Uint8Array
    emscripten::val result = emscripten::val::global("Uint8Array").new_(emscripten::typed_memory_view(P.size(), P.data()));

    return result;
}

emscripten::val decodeToBytes(emscripten::val data_probs, emscripten::val password) {
    // Convert data_probs to std::vector<float>
    std::vector<float> coded_data = emscripten::convertJSArrayToNumberVector<float>(data_probs);

    // Convert password to std::string
    std::string pwd = password.as<std::string>();

    // Call decode function
    std::vector<uint8_t> msg = decode(coded_data, pwd);

    // If msg is empty, return null
    if (msg.empty()) {
        return emscripten::val::null();
    }

    // Convert msg to JS Uint8Array
    emscripten::val result = emscripten::val::global("Uint8Array").new_(emscripten::typed_memory_view(msg.size(), msg.data()));

    return result;
}
EMSCRIPTEN_BINDINGS() {
    emscripten::function("encodeToBits", &encodeToBits);
    emscripten::function("decodeToBytes", &decodeToBytes);
}