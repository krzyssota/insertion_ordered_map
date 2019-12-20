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

template<class K, class V, class Hash = std::hash<K>>
class map_buffer {

public :

    unordered_map<K, typename std::list<pair<K, V>>::iterator, Hash> map_data;

    list<pair<K, V>> ordered_list;

    unsigned refs;
    unsigned old_refs;
    bool unsharable;
    bool old_unsharable;

    void memorize() {
        old_unsharable = unsharable;
        old_refs = refs;
    }

    void restore() {
        unsharable = old_unsharable;
        refs = old_refs;
    }

    map_buffer() : refs(1), old_refs(1), unsharable(false), old_unsharable(false),
                   map_data(), ordered_list() {

    }

    map_buffer(const map_buffer<K, V, Hash> &other)  // copy constructor
            : map_data(other.map_data), ordered_list(other.ordered_list),
              refs(1), unsharable(false) {
    }

    map_buffer &operator=(const map_buffer &other) {
        this->map_data = map_data(other.map_data);
        this->ordered_list = ordered_list(other.ordered_list);
        this->refs = 1;
        this->unsharable = false;
        return *this;
    }

    map_buffer(map_buffer<K, V, Hash> &&other) noexcept {
        map_data = move(other.map_data);
        ordered_list = move(other.ordered_list);
        refs = other.refs;
        unsharable = other.unsharable;
    }
};

template<class K, class V, class Hash = std::hash<K>>
class insertion_ordered_map {

    shared_ptr<map_buffer<K, V, Hash>> buf_ptr;

    shared_ptr<map_buffer<K, V, Hash>> old_buf_ptr;

    shared_ptr<map_buffer<K, V, Hash>> about_to_modify(bool mark_unsharable = false) {

//        shared_ptr<map_buffer<K, V, Hash>> old_buf_ptr = buf_ptr;

        memorize();

        if (buf_ptr->refs > 1) { // && !buf_ptr->unsharable
            shared_ptr<map_buffer<K, V, Hash>> new_buf_ptr;
            try {

                new_buf_ptr = make_shared<map_buffer<K, V, Hash>>(*buf_ptr);

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

    insertion_ordered_map() {
        buf_ptr = make_shared<map_buffer<K, V, Hash>>();
        old_buf_ptr = buf_ptr;
    };

    /*Konstruktor kopiujący. Powinien mieć semantykę copy-on-write, a więc nie
      kopiuje wewnętrznych struktur danych, jeśli nie jest to potrzebne. Słowniki
      współdzielą struktury do czasu modyfikacji jednej z nich.
      złożoność czasowa O(1) lub oczekiwana O(n), jeśli konieczne jest wykonanie kopii.
      insertion_ordered_map(insertion_ordered_map const &other);*/
    insertion_ordered_map(const insertion_ordered_map &other) {
        if (other.buf_ptr->unsharable) {

            buf_ptr = make_shared<map_buffer<K, V, Hash>>(*(other.buf_ptr));
            old_buf_ptr = buf_ptr;

        } else {

            buf_ptr = other.buf_ptr;
            old_buf_ptr = buf_ptr;
            ++buf_ptr->refs;

        }
    }

    insertion_ordered_map &operator=(insertion_ordered_map &other) {
        if (other.buf_ptr->unsharable) {

            buf_ptr = make_shared<map_buffer<K, V, Hash>>(*(other.buf_ptr));
            old_buf_ptr = buf_ptr;

        } else {

            buf_ptr = other.buf_ptr;
            ++buf_ptr->refs;
        }
        return *this;
    }

    insertion_ordered_map(insertion_ordered_map &&other) noexcept {
        buf_ptr = make_shared<map_buffer<V, K, Hash>>(move(*other.buf_ptr));
        old_buf_ptr = buf_ptr;
    }

    V &operator[](K const &k) {

        try {
           about_to_modify(true);
        } catch (bad_alloc &e) {
            throw;
        }
        auto it = buf_ptr->map_data.find(k);
        if (it != buf_ptr->map_data.end()) { // found
            return (*(it->second)).second;
        } else {
            try {
                buf_ptr->ordered_list.push_back({k, V()});
            } catch (bad_alloc &e) {
                restore();
                throw;
            }
            try {
                buf_ptr->map_data.insert(k, V());
            } catch (bad_alloc &e) {
                restore();
                buf_ptr->ordered_list.pop_back();
                throw;
            }
        }
    }

    /*
    Wstawianie do słownika.
     Jeśli klucz k nie jest przechowywany w słowniku, to
    wstawia wartość v pod kluczem k i zwraca true.
     Jeśli klucz k już jest w słowniku, to wartość pod nim przypisana nie zmienia się, ale klucz zostaje
    przesunięty na koniec porządku iteracji, a metoda zwraca false.
     Jeśli jest to
            możliwe, należy unikać kopiowania elementów przechowywanych już w słowniku.
    Złożoność czasowa oczekiwana O(1) + ewentualny czas kopiowania słownika.*/
    bool insert(K const &k, V const &v) {

        auto list_ = buf_ptr->ordered_list;
        auto map_ = buf_ptr->map_data;
        typename std::list<pair<K, V>>::iterator list_it;
        typename std::unordered_map<K, typename std::list<pair<K, V>>::iterator, Hash> map_it = map_.find(k);

        if(map_it == map_.end()) { // not in the map

            try {
                about_to_modify(true); // potencjalny restore w środku
                auto newPair = {k, v};
                list_.push_back(newPair);
                list_it = list_.end() - 1; // pointing at new element
            } catch (bad_alloc &e) {
                // newPair not added to the list
                throw;
            }
            try {
                map_.insert({k, list_it});
                return true;
            } catch (bad_alloc &e) {
                list_.erase(list_it);
                throw;
            }
        } else { // key already contained in the map
            list_it = map_it.second;
            pair<K, V> touched_element = {list_it.first, list_it.second};
            list_.erase(list_it);
            list_.push_back(touched_element);
            // dlaczego szare?
            map_[k] = list_.end() - 1; // iterator to the moved element
        }
    }

    /*
      Usuwanie ze słownika. Usuwa wartość znajdującą się pod podanym kluczem k.
      Jeśli taki klucz nie istnieje, to podnosi wyjątek lookup_error.
      Złożoność czasowa oczekiwana O(1) + ewentualny czas kopiowania.*/
    void erase(K const &k) {
        auto map_it = buf_ptr->map_data.find(k);

        if (map_it != buf_ptr->map_data.end()) { // found
            auto list_it = (*map_it).second;
            buf_ptr->map_data.erase(map_it);
            buf_ptr->ordered_list.erase(list_it);
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


//template<typename K, typename V>
//class it {
//    shared_ptr<K> first;
//    shared_ptr<list_entry<K, V>> second;
//public:
//    it() {
//        //first = make_shared(NULL);
//        //second = make_shared(NULL);
//        return *this;
//    }
//
//    it(it<K, V> &other) {
//        try {
//            first = make_shared(other.first);
//            second = make_shared(other.second);
//        } catch (bad_alloc &e) {
//            // ?
//            throw;
//        }
//    }
//
//    it &operator++() {
//        first = make_shared((*this).second->next);
//    }
//};


#endif //INSERTION_ORDERED_MAP_INSERTION_ORDERED_MAP_H

