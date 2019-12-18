#ifndef ISERTION_ORDERED_MAP_INSERTION_ORDERED_MAP_H
#define ISERTION_ORDERED_MAP_INSERTION_ORDERED_MAP_H

#include <iostream>
#include <unordered_map>


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
    list_entry<V> *first;
    list_entry<V> *last;

    public :

    map_buffer(const map_buffer &) = delete;
    map_buffer &operator=(const map_buffer &) = delete;
};

template<class K, class V, class Hash = std::hash<K>>
class insertion_ordered_map {
    map_buffer<K, V, Hash> *data;
public:

};


#endif //INSERTION_ORDERED_MAP_INSERTION_ORDERED_MAP_H

