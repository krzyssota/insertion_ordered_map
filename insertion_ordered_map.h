#ifndef ISERTION_ORDERED_MAP_INSERTION_ORDERED_MAP_H
#define ISERTION_ORDERED_MAP_INSERTION_ORDERED_MAP_H

#include <iostream>
#include <unordered_map>
#include <bits/shared_ptr.h>


using namespace std;

class lookup_error : public exception {
    virtual const char *what() const throw() {
        return "lookup_error";
    }
};

// TODO
// mapa ma miec Value smartfpointer na list_entry i naprawic konstruktory
template<typename K, typename V>
struct list_entry {
private :
public :
    K key;
    V value;
    shared_ptr<list_entry> next;
    shared_ptr<list_entry> prev;

    list_entry(V v) : value(v), next(NULL), prev(NULL) {};
};

template<class K, class V, class Hash = std::hash<K>>
class map_buffer {
    unordered_map<K, list_entry<K, V>, Hash> map;
    shared_ptr<list_entry<K, V>> first;
    shared_ptr<list_entry<K, V>> last;
    unsigned refs;
    unsigned old_refs;
    bool unsharable;
    bool old_unsharable;

public :

    void memorize() {
        old_unsharable = unsharable;
        old_refs = refs;
    }

    void restore() {
        unsharable = old_unsharable;
        refs = old_refs;
    }

    map_buffer(const map_buffer<K, V, Hash> &other)  // copy constructor
            : map(other.map), first(other.first), last(other.last),
              refs(1), unsharable(false) {
    }

    map_buffer &operator=(const map_buffer &other) {
        this->map = map(other.map); // tutaj kopia
        this->first = findPtr(other.first);
        this->last = findPtr(other.last);
        this->refs = 1;
        this->unsharable = false;
        return *this;
    }

    map_buffer(map_buffer<K, V, Hash> &&other) noexcept {
        map = move(other.map);
        first = other.first;
        last = other.last;
        refs = other.refs;
        unsharable = other.unsharable;
    }

    shared_ptr<list_entry<K, V>> findPtr(list_entry<K, V> &other) {
        auto it = map.find(other.key);
        if (it != other.end()) {
            return make_shared(it->second); // wskaznik do obiektu list_entry
        } else {
            // never happens
        }
    }
};

template<class K, class V, class Hash = std::hash<K>>
class insertion_ordered_map {

    shared_ptr<map_buffer<K, V, Hash>> old_buf_ptr;

    shared_ptr<map_buffer<K, V, Hash>> buf_ptr;

    shared_ptr<map_buffer<K, V, Hash>> about_to_modify(bool mark_unsharable = false) {

//        shared_ptr<map_buffer<K, V, Hash>> old_buf_ptr = buf_ptr;

        memorize();

        if (buf_ptr->refs > 1) { // && !buf_ptr->unsharable
            shared_ptr<map_buffer<K, V, Hash>> new_buf_ptr;
            try {
                new_buf_ptr = make_shared(*buf_ptr);
            }
            catch (bad_alloc &e) {
                // chyba zalezy od miejsca wywołania about_to_modify
                throw;
            }
            --buf_ptr->refs;
            buf_ptr = new_buf_ptr;
        } else {
            // chyba nic bo unordered_map samo się powiekszy
        }

        if (mark_unsharable) {
            buf_ptr->unsharable = true;
        } else {
            buf_ptr->refs = 1;
        }

        return old_buf_ptr;
    }

public:

    void memorize() {
        buf_ptr->memorize();
        old_buf_ptr = buf_ptr;
    }

    void restore() {
        buf_ptr->restore();
        buf_ptr = old_buf_ptr;
    }


    /*Konstruktor kopiujący. Powinien mieć semantykę copy-on-write, a więc nie
      kopiuje wewnętrznych struktur danych, jeśli nie jest to potrzebne. Słowniki
      współdzielą struktury do czasu modyfikacji jednej z nich.
      złożoność czasowa O(1) lub oczekiwana O(n), jeśli konieczne jest wykonanie kopii.
      insertion_ordered_map(insertion_ordered_map const &other);*/
    insertion_ordered_map<K, V, Hash>(const insertion_ordered_map<K, V, Hash> &other) {
        if (other.buf_ptr->unsharable) {
            buf_ptr = new map_buffer<K, V, Hash>(other.buf_ptr);
        } else {
            buf_ptr = other.buf_ptr;
            ++buf_ptr->refs;
        }
    }

    insertion_ordered_map(insertion_ordered_map &&other) noexcept {
        buf_ptr = make_shared<map_buffer<V, K, Hash>>(move(*other.buf_ptr));
    }

    V &operator[](K const &k) {

        try {
           about_to_modify(true);
        } catch (bad_alloc &e) {
            throw;
        }
        auto it = buf_ptr->map.find(k);
        if (it != buf_ptr->map.end()) { // found
            return it->second.value;
        } else {

            try {
                shared_ptr<list_entry<K, V>> valueEntry = make_shared(list_entry(V()));

                buf_ptr.insert({k, *valueEntry});
                buf_ptr.last->next = valueEntry;
                valueEntry->previous = buf_ptr.last;
                buf_ptr.last = valueEntry;

                return valueEntry->value;

            } catch (bad_alloc &e) {
                restore();
                throw;
            }
        }
    }

    /*
      Usuwanie ze słownika. Usuwa wartość znajdującą się pod podanym kluczem k.
      Jeśli taki klucz nie istnieje, to podnosi wyjątek lookup_error.
      Złożoność czasowa oczekiwana O(1) + ewentualny czas kopiowania.*/
    void erase(K const &k) {
        auto it = buf_ptr->map.find(k);

        if (it != buf_ptr->map.end()) { // found
            auto p = it.second->prev;
            auto n = it.second->next;
            p->next = n;
            n->prev = p;
            buf_ptr->map.erase(k);
        } else {
            throw lookup_error();
        }
    }

    /*
    Scalanie słowników. Dodaje kopie wszystkich elementów podanego słownika other
    do bieżącego słownika (this). Wartości pod kluczami już obecnymi w bieżącym
            słowniku nie są nadpisywane. Klucze ze słownika other pojawiają się w porządku
    iteracji na końcu, zachowując kolejność względem siebie.
    Złożoność czasowa oczekiwana O(n + m), gdzie m to rozmiar słownika other.*/
    void merge(insertion_ordered_map const &other) {

    }


};


template<typename K, typename V>
class it {
    shared_ptr<K> first;
    shared_ptr<list_entry<K, V>> second;
public:
    it() {
        //first = make_shared(NULL);
        //second = make_shared(NULL);
        return *this;
    }

    it(it<K, V> &other) {
        try {
            first = make_shared(other.first);
            second = make_shared(other.second);
        } catch (bad_alloc &e) {
            // ?
            throw;
        }
    }

    it &operator++() {
        first = make_shared((*this).second->next);
    }
};


#endif //INSERTION_ORDERED_MAP_INSERTION_ORDERED_MAP_H

