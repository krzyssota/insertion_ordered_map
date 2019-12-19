#ifndef ISERTION_ORDERED_MAP_INSERTION_ORDERED_MAP_H
#define ISERTION_ORDERED_MAP_INSERTION_ORDERED_MAP_H

#include <iostream>
#include <unordered_map>
#include <bits/shared_ptr.h>


using namespace std;

class lookup_error: public exception {
    virtual const char* what() const throw() {
        return "lookup_error";
    }
};



template<typename K, typename V>
struct list_entry {
private :
    using map_entry = pair<K, list_entry<K, V>>;
public :
    V value;
    shared_ptr<map_entry> next;
    shared_ptr<map_entry> prev;

    list_entry(V v) : value(v), next(NULL), prev(NULL) {};
};

template<class K, class V, class Hash = std::hash<K>>
class map_buffer {
    unordered_map<K, list_entry<K, V>, Hash> buf;
    shared_ptr<list_entry<K, V>> first;
    shared_ptr<list_entry<K, V>> last;
    unsigned refs;
    bool unsharable;

public :

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
                new_data_ptr = make_shared(*data_ptr);
            }
            catch (bad_alloc &e) {
                // chyba zalezy od miejsca wywołania about_to_modify
                throw;
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

    insertion_ordered_map(insertion_ordered_map &&other) noexcept {
        data_ptr = make_shared<map_buffer<V, K, Hash>>(move(*other.data_ptr));
    }

    V &operator[](K const &k){

        about_to_modify(true);

        auto it = data_ptr->buf.find(k);
        if(it != data_ptr->buf.end()){ // found
            return it->second.value;
        } else {

            try {
                shared_ptr<insertion_ordered_map<K, V, Hash>> tmpCopy;
                tmpCopy = make_shared(*this); // copy our structure

                shared_ptr<list_entry<K, V>> valueEntry = make_shared<list_entry>(list_entry(V()));

                tmpCopy.insert({k, *valueEntry});
                tmpCopy.last->next = valueEntry;
                valueEntry->previous = tmpCopy.last;
                tmpCopy.last = valueEntry;

                data_ptr = &tmpCopy; // swap

                return valueEntry->value;

            } catch (bad_alloc &e) {
                // chyba nic poza wyrzuceniem do góry
                throw;
            }
        }
    }

  /*
    Usuwanie ze słownika. Usuwa wartość znajdującą się pod podanym kluczem k.
    Jeśli taki klucz nie istnieje, to podnosi wyjątek lookup_error.
    Złożoność czasowa oczekiwana O(1) + ewentualny czas kopiowania.*/
    void erase(K const &k) {
        auto it = data_ptr->buf.find(k);

        if(it != data_ptr->buf.end()) { // found
            auto p = it.second->prev;
            auto n = it.second->next;
            p->next = n;
            n->prev = p;
            data_ptr->buf.erase(k);
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

template <typename K, typename V>
class it {
    shared_ptr<K> first;
    shared_ptr<list_entry<V>> second;
public:
    it(){
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

    it& operator++(){
        first = make_shared((*this).second->next);
    }

};



#endif //INSERTION_ORDERED_MAP_INSERTION_ORDERED_MAP_H

