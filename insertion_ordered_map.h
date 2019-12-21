#ifndef INSERTION_ORDERED_MAP_INSERTION_ORDERED_MAP_H
#define INSERTION_ORDERED_MAP_INSERTION_ORDERED_MAP_H

#include <iostream>
#include <unordered_map>
#include <memory>
#include <list>

using namespace std;

class lookup_error : public exception {
    virtual const char *what() const throw() {
        return "lookup_error";
    }
};

template<class K, class V, typename Hash = std::hash<K>>
class map_buffer {

public :

    unordered_map<K, typename std::list<pair<K, V>>::iterator, Hash> map_data;

    list<pair<K, V>> list_data;

    unsigned refs;
    unsigned old_refs;
    bool unsharable;
    bool old_unsharable;

    using iterator = typename list<pair<K, V>>::const_iterator;

    void memorize() {
        old_unsharable = unsharable;
        old_refs = refs;
    }

    void restore() {
        unsharable = old_unsharable;
        refs = old_refs;
    }

    map_buffer() : map_data(), list_data(), refs(1), old_refs(1),
                    unsharable(false), old_unsharable(false) {}

    map_buffer(const map_buffer &other)
        : map_data(), list_data(other.list_data),
        refs(1), old_refs(1), unsharable(false), old_unsharable(false) {
        auto it = list_data.begin();
        while (it != list_data.end()) {
            map_data[it->first] = it;
            it++;
        }
    }

    map_buffer &operator=(const map_buffer &other) {
        auto old_map_data = map_data;
        auto old_list_data = list_data;
        memorize();
        map_data();
        list_data = list_data(other.list_data);
        refs = 1;
        unsharable = false;
        auto it = list_data.begin();
        try {
            while (it != list_data.end()) {
                map_data[it->first] = it;
                ++it;
            }
        } catch (...) { // exception from operator[]; restore previous state
            map_data = old_map_data;
            list_data = old_list_data;
            restore();
            throw;
        }
        return *this;
    }

    map_buffer(map_buffer &&other) noexcept {
        map_data = move(other.map_data);
        list_data = move(other.list_data);
        refs = other.refs;
        unsharable = other.unsharable;
    }
};

template<class K, class V, typename Hash = std::hash<K>>
class insertion_ordered_map {

    shared_ptr<map_buffer<K, V, Hash>> buf_ptr;

    shared_ptr<map_buffer<K, V, Hash>> old_buf_ptr;

    shared_ptr<map_buffer<K, V, Hash>> about_to_modify(bool mark_unsharable = false) {

        memorize();

        if (buf_ptr->refs > 1) { // makes it's own copy
            auto new_buf_ptr = make_shared<map_buffer<K, V, Hash>>(*buf_ptr);
            --buf_ptr->refs;
            buf_ptr = new_buf_ptr;
        }

        if (mark_unsharable) {
            buf_ptr->unsharable = true;
        } else {
            buf_ptr->refs = 1;
        }

        return old_buf_ptr;
    }

public:

    using iterator = typename map_buffer<K, V, Hash>::iterator;

    iterator begin() const noexcept {
        return buf_ptr->list_data.begin();
    }
    iterator end() const noexcept {
        return buf_ptr->list_data.end();
    }

    void memorize() {
        buf_ptr->memorize();
        old_buf_ptr = buf_ptr;
    }

    void restore() {
        buf_ptr->restore();
        buf_ptr = old_buf_ptr;
    }

    insertion_ordered_map() {
        buf_ptr = make_shared<map_buffer<K, V, Hash>>();
        old_buf_ptr = buf_ptr;
    };

    insertion_ordered_map(const insertion_ordered_map &other) {
        if (other.buf_ptr->unsharable) {
            buf_ptr = make_shared<map_buffer<K, V, Hash>>(*(other.buf_ptr));
            old_buf_ptr = buf_ptr;
        } else { // share resources
            buf_ptr = other.buf_ptr;
            old_buf_ptr = buf_ptr;
            ++buf_ptr->refs;
        }
    }

    insertion_ordered_map &operator=(insertion_ordered_map other) {
        if (this == &other) {
            return *this;
        }
        if (other.buf_ptr->unsharable) {
            buf_ptr = make_shared<map_buffer<K, V, Hash>>(*(other.buf_ptr));
            old_buf_ptr = buf_ptr;
        } else { // share resources
            buf_ptr = other.buf_ptr;
            ++buf_ptr->refs;
        }
        return *this;
    }

    ~insertion_ordered_map() {
        --buf_ptr->refs;
    }

    insertion_ordered_map(insertion_ordered_map &&other) noexcept {
        buf_ptr = make_shared<map_buffer<K, V, Hash>>(move(*other.buf_ptr));
        other = insertion_ordered_map();
        old_buf_ptr = buf_ptr;
    }

    V &operator[](K const &k) {

        about_to_modify(true);

        auto it = buf_ptr->map_data.find(k);
        if (it != buf_ptr->map_data.end()) { // found
            return (*(it->second)).second;
        } else {
            try {
                buf_ptr->list_data.push_back({k, V()});
            } catch (bad_alloc &e) {
                restore();
                throw;
            }
            try {
                auto tmp_it = buf_ptr->list_data.end();
                --tmp_it;
                buf_ptr->map_data[k] = tmp_it;
                return tmp_it->second;
            } catch (bad_alloc &e) {
                restore();
                buf_ptr->list_data.pop_back();
                throw;
            }
        }
    }

    V &at(K const &k) {
        typename unordered_map<K, typename std::list<pair<K, V>>::iterator, Hash>::iterator it;

        about_to_modify(true);
        try {
            it = buf_ptr->map_data.find(k);
        }
        catch (...) {
            restore();
            throw;
        }

        if (it != buf_ptr->map_data.end()) { // found
            return (*(it->second)).second;
        } else {
            throw lookup_error();
        }
    }
    V const &at(K const &k) const {
        auto map_it = buf_ptr->map_data.find(k);

        if (map_it != buf_ptr->map_data.end()) { // found
            return map_it->second;
        } else {
            throw lookup_error();
        }
    }

    bool insert(K const &k, V const &v) {

        typename std::list<pair<K, V>>::iterator list_it;
        auto map_it = buf_ptr->map_data.find(k);

        if(map_it == buf_ptr->map_data.end()) { // key not contained in the map

            about_to_modify(false);
            try {
                auto newPair = make_pair(k, v);
                buf_ptr->list_data.push_back(newPair);
                list_it = --(buf_ptr->list_data.end()); // pointing at new element
            } catch (...) {
                restore();
                throw;
            }

            try {
                buf_ptr->map_data.insert({k, list_it});
                return true;
            } catch (...) {
                buf_ptr->list_data.erase(list_it);
                restore();
                throw;
            }
        } else { // key already contained in the map
            list_it = map_it->second;
            pair<K, V> touched_element = make_pair(list_it->first, list_it->second);
            buf_ptr->list_data.erase(list_it);
            buf_ptr->list_data.push_back(touched_element);
            buf_ptr->map_data[k] = --buf_ptr->list_data.end(); // iterator to the moved element
            return false;
        }
    }


    void erase(K const &k) {
        typename unordered_map<K, typename std::list<pair<K, V>>::iterator, Hash>::iterator map_it;

        about_to_modify(false);
        try {
            map_it = buf_ptr->map_data.find(k);
        }
        catch (...) {
            restore();
        }

        if (map_it != buf_ptr->map_data.end()) { // found
            auto list_it = (*map_it).second;
            buf_ptr->map_data.erase(map_it);
            buf_ptr->list_data.erase(list_it);
        } else {
            throw lookup_error();
        }
    }

    size_t size() {
        return buf_ptr->map_data.size();
    }

    bool contains(K const &k) {
        return buf_ptr->map_data.find(k) != buf_ptr->map_data.end();
    }

    /*
    Sprawdzenie niepustości słownika. Zwraca true, gdy słownik jest pusty, a false
    w przeciwnym przypadku.
    Złożoność czasowa O(1).*/
    bool empty() const {
        return buf_ptr->list_data.empty();
    }

    /*
    Czyszczenie słownika. Usuwa wszystkie elementy ze słownika.
    Złożoność czasowa O(n).*/
    void clear() {
        try {
            about_to_modify(false);
        } catch (...) {
            throw;
        }
        buf_ptr->map_data.clear();
        buf_ptr->list_data.clear();
    }

    void merge(insertion_ordered_map const &other) {

        auto new_iom = *this;

        bool original_state = other.buf_ptr->unsharable; // deep copy for range loop
        other.buf_ptr->unsharable = true;
        auto other_copy = other;
        other.buf_ptr->unsharable = original_state;

        // exception safety is achieved through working on a copy and swapping if no exception was thrown
        for (auto const x: other_copy) {
            new_iom.insert(x.first, x.second);
        }
        *this = new_iom;
    }
};

#endif //INSERTION_ORDERED_MAP_INSERTION_ORDERED_MAP_H

