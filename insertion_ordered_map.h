#ifndef ISERTION_ORDERED_MAP_INSERTION_ORDERED_MAP_H
#define ISERTION_ORDERED_MAP_INSERTION_ORDERED_MAP_H

#include <iostream>
#include <unordered_map>
#include <bits/shared_ptr.h>


using namespace std;

template<typename V>
struct list_entry {
    V value;
    list_entry<V> *next;
    list_entry<V> *previous;
};

template<class K, class V, class Hash = std::hash<K>>
class map_buffer {
    unordered_map<K, list_entry<V>, Hash> buf;
    shared_ptr<list_entry<V>> first;
    shared_ptr<list_entry<V>> last;
    unsigned refs;
    bool unsharable;

    public :

    map_buffer() : buf(), first(NULL), last(NULL),
    refs(1), unsharable(false){ // default constructor moze niepotrzebne
    }
    map_buffer(const map_buffer<K, V, Hash> & other)  // copy constructor
    : buf(other.buf), first(other.first), last(other.last),
    refs(1), unsharable(false) {

    }
    map_buffer &operator=(const map_buffer & other) {
        this->first = other.first;
        this->last = other.last;
        this->buf = buf(other.buf);
        this->refs = 1;
        this->unsharable = false;
        return *this;
    };
    map_buffer(map_buffer<K, V, Hash> &&other) noexcept {
        buf = move(other.buf);
        first = other.first;
        last = other.last;
        refs = other.refs;
        unsharable = other.unsharable;
    }
};

template<class K, class V, class Hash = std::hash<K>>
class insertion_ordered_map {

    shared_ptr<map_buffer<K, V, Hash>> data_ptr;
    void about_to_modify(bool mark_unsharable = false) {
        if(data_ptr->refs > 1) { // && !data_ptr->unsharable
            shared_ptr<map_buffer<K, V, Hash>> new_data_ptr;
            try {
                new_data_ptr = make_shared(*data_ptr); // ?new_data(data_ptr)?
            }
            catch (exception e) {

            }
            --data_ptr->refs;
            data_ptr = new_data_ptr;
        } else {
            // chyba nic bo unordered_map samo się powiekszy
        }
        if(mark_unsharable){
            data_ptr->unsharable = true;
        }
        else {
            data_ptr->refs = 1;
        }
    }

public:
    V &operator[](K const &k){
        about_to_modify(true);
        auto it = data_ptr->buf.find(k);
        if(it != data_ptr->buf.end()){
            //...
            return it->second.value;
        } else {
            //...
        }
    }

  /*Konstruktor kopiujący. Powinien mieć semantykę copy-on-write, a więc nie
    kopiuje wewnętrznych struktur danych, jeśli nie jest to potrzebne. Słowniki
    współdzielą struktury do czasu modyfikacji jednej z nich.
    złożoność czasowa O(1) lub oczekiwana O(n), jeśli konieczne jest wykonanie kopii.
    insertion_ordered_map(insertion_ordered_map const &other);*/
    insertion_ordered_map<K, V, Hash>(const insertion_ordered_map<K,V,Hash>& other) {
        if(other.data_ptr->unsharable) {
            data_ptr = new map_buffer<K, V, Hash>(other.data_ptr);
        } else {
            data_ptr = other.data_ptr;
            ++data_ptr->refs;
        }
    }

    insertion_ordered_map(insertion_ordered_map &&other) noexcept
    {
        data_ptr = make_shared<map_buffer<V, K, Hash>>(move(*other.data_ptr));
    }
};


#endif //INSERTION_ORDERED_MAP_INSERTION_ORDERED_MAP_H

