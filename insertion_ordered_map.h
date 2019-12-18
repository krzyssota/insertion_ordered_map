#ifndef ISERTION_ORDERED_MAP_INSERTION_ORDERED_MAP_H
#define ISERTION_ORDERED_MAP_INSERTION_ORDERED_MAP_H

#include <iostream>
#include <unordered_map>


using namespace std;
const size_t unshareable = numeric_limits<size_t>::max();

template<typename V>
struct list_entry {
    V value;
    list_entry<V> *next;
    list_entry<V> *previous;
};

template<class K, class V, class Hash = std::hash<K>>
class map_buffer {
    unordered_map<K, list_entry<V>, Hash> buf;
    list_entry<V> *first; // fixme smart pointer
    list_entry<V> *last;
    unsigned refs;
    bool unshareable;

    public :

    map_buffer() : buf(), first(NULL), last(NULL),
    refs(1), unshareable(false){ // default constructor moze niepotrzebne
    }
    map_buffer(const map_buffer<K, V, Hash> & other)  // copy constructor
    : buf(other.buf), first(other.first), last(other.last),
    refs(1), unshareable(false) {

    }
    map_buffer &operator=(const map_buffer &) = delete;
    map_buffer(map_buffer<K, V, Hash> &&other) noexcept {
        buf = move(other.buf);
        first = other.first;
        last = other.last;
        refs = other.refs;
        unshareable = other.unshareable;
    }
};

template<class K, class V, class Hash = std::hash<K>>
class insertion_ordered_map {

    map_buffer<K, V, Hash> *data;
    void about_to_modify(bool mark_unshareable = false) {
        if(data->refs > 1 && !data->unshareable) {
            auto* new_data = new map_buffer<K, V, Hash>(data); // ?new_data(data)?
            --data->refs;
            data = new_data;
        } else {
            // chyba nic bo unordered_map samo się powiekszy
        }
        if(mark_unshareable){
            data->unshareable = true;
            data->refs = unshareable; // ???
        }
        else {
            data->refs = 1;
        }
    }

public:
    V &operator[](K const &k){
        about_to_modify(true);
        auto *it = data->buf.find(k);
        if(*it != data->buf.end()){
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
        if(other.data->unshareable) {
            data = new map_buffer<K, V, Hash>(other.data);
        } else {
            data = other.data;
            ++data->refs;
        }
    }
    Fibo(Fibo&& that) noexcept : fibset(std::move(that.fibset)) {};
    insertion_ordered_map(insertion_ordered_map &&other) noexcept {
        data(move(other.data));
    }
}


#endif //INSERTION_ORDERED_MAP_INSERTION_ORDERED_MAP_H

