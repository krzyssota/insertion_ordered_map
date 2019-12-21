#ifndef INSERTION_ORDERED_MAP_INSERTION_ORDERED_MAP_H
#define INSERTION_ORDERED_MAP_INSERTION_ORDERED_MAP_H

#include <iostream>
#include <unordered_map>
#include <memory>
#include <list>
#include <cassert>


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

    list<pair<K, V>> ordered_list;

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

    map_buffer() : map_data(), ordered_list(), refs(1), old_refs(1), unsharable(false), old_unsharable(false) {

    }

    map_buffer(const map_buffer &other)  // copy constructor
            :  map_data(), ordered_list(other.ordered_list),
              refs(1), old_refs(1), unsharable(false), old_unsharable(false) {
        //cout << "copy constructor map_buffer dla " << &other << endl;
        auto it = ordered_list.begin();
        while (it != ordered_list.end()) {
            // ??? czy nie moze byc tutaj bad alloc?
            map_data[it->first] = it;
            it++;
        }
    }

    map_buffer &operator=(const map_buffer &other) {
        map_data();
        ordered_list = ordered_list(other.ordered_list);
        refs = 1;
        unsharable = false;
        auto it = ordered_list.begin();
        while (it != ordered_list.end()) {
            map_data[it->first] = it;
            it++;
        }
        return *this;
    }

    map_buffer(map_buffer &&other) noexcept {
        map_data = move(other.map_data);
        ordered_list = move(other.ordered_list);
        refs = other.refs;
        unsharable = other.unsharable;
    }
};

template<class K, class V, typename Hash = std::hash<K>>
class insertion_ordered_map {

    shared_ptr<map_buffer<K, V, Hash>> buf_ptr;

    shared_ptr<map_buffer<K, V, Hash>> old_buf_ptr;

    shared_ptr<map_buffer<K, V, Hash>> about_to_modify(bool mark_unsharable = false) {
        //cout << "about to modify dla " << this << ", mark_unsharable = " << mark_unsharable << endl;

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

    using iterator = typename map_buffer<K, V, Hash>::iterator;

    iterator begin() const noexcept {
        return buf_ptr->ordered_list.begin();
    }
    iterator end() const noexcept {
        return buf_ptr->ordered_list.end();
    }

    void print() {
        cout << endl;
        cout << "------------------------------------------------------------\n";
        cout << "|                   print " << this << "                   |\n";
        cout << "------------------------------------------------------------\n";

        cout << "refs buffera: " << buf_ptr->refs << endl;
        cout << "buffer unsharable: " << buf_ptr->unsharable << endl;
        cout << "adres buffera: " << buf_ptr << "\n\n";

        for (auto x: buf_ptr->ordered_list) {
            cout << "key: " << x.first << ", value: " << x.second << endl;
        }
        cout << "------------------------------------------------------------\n";
        cout << "\n\n\n";
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

    /*Konstruktor kopiujący. Powinien mieć semantykę copy-on-write, a więc nie
      kopiuje wewnętrznych struktur danych, jeśli nie jest to potrzebne. Słowniki
      współdzielą struktury do czasu modyfikacji jednej z nich.
      złożoność czasowa O(1) lub oczekiwana O(n), jeśli konieczne jest wykonanie kopii.
      insertion_ordered_map(insertion_ordered_map const &other);*/
    insertion_ordered_map(const insertion_ordered_map &other) {
        //cout << "copy constructor iom dla " << &other << endl;
        if (other.buf_ptr->unsharable) {

            buf_ptr = make_shared<map_buffer<K, V, Hash>>(*(other.buf_ptr));
            old_buf_ptr = buf_ptr;

        } else {
            buf_ptr = other.buf_ptr;
            old_buf_ptr = buf_ptr;
            ++buf_ptr->refs;

        }
        assert(!this->buf_ptr->unsharable);
    }

    insertion_ordered_map &operator=(insertion_ordered_map other) {
        if (this == &other) {
            return *this;
        }
        if (other.buf_ptr->unsharable) {

            buf_ptr = make_shared<map_buffer<K, V, Hash>>(*(other.buf_ptr));
            old_buf_ptr = buf_ptr;

        } else {

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
                auto tmp_it = buf_ptr->ordered_list.end();
                --tmp_it;
                buf_ptr->map_data[k] = tmp_it;
                return tmp_it->second;
            } catch (bad_alloc &e) {
                restore();
                buf_ptr->ordered_list.pop_back();
                throw;
            }
        }
    }
    /*
     Referencja wartości. Zwraca referencję na wartość przechowywaną w słowniku pod
     podanym kluczem k. Jeśli taki klucz nie istnieje w słowniku, to podnosi
     wyjątek lookup_error. Metoda ta powinna być dostępna w wersji z atrybutem
     const oraz bez niego.
     Złożoność czasowa oczekiwana O(1) + ewentualny czas kopiowania.*/
    V &at(K const &k) {
        try {
            about_to_modify(true);
        } catch (bad_alloc &e) {
            throw;
        }
        typename unordered_map<K, typename std::list<pair<K, V>>::iterator, Hash>::iterator it;
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
            // todo ??? restore()
            throw lookup_error();
        }
    }
    V const &at(K const &k) const {
        //todo na szybko, moze byc cos nie tak
        auto map_it = buf_ptr->map_data.find(k);

        if (map_it != buf_ptr->map_data.end()) { // found
            return map_it->second;
        } else {
            throw lookup_error();
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

        typename std::list<pair<K, V>>::iterator list_it;
        auto map_it = buf_ptr->map_data.find(k);

        if(map_it == buf_ptr->map_data.end()) { // not in the map

            try {
                about_to_modify(false); // potencjalny restore w środku
                auto newPair = make_pair(k, v);
                buf_ptr->ordered_list.push_back(newPair);
                list_it = --(buf_ptr->ordered_list.end()); // pointing at new element
            } catch (bad_alloc &e) {
                // newPair not added to the list
                throw;
            }

            try {
                buf_ptr->map_data.insert({k, list_it});
                return true;
            } catch (bad_alloc &e) {
                buf_ptr->ordered_list.erase(list_it);
                throw;
            }
        } else { // key already contained in the map
            list_it = map_it->second;
            pair<K, V> touched_element = make_pair(list_it->first, list_it->second);
            buf_ptr->ordered_list.erase(list_it);
            buf_ptr->ordered_list.push_back(touched_element);
            buf_ptr->map_data[k] = --buf_ptr->ordered_list.end(); // iterator to the moved element
            return false;
        }
    }

    /*
     *
      Usuwanie ze słownika. Usuwa wartość znajdującą się pod podanym kluczem k.
      Jeśli taki klucz nie istnieje, to podnosi wyjątek lookup_error.
      Złożoność czasowa oczekiwana O(1) + ewentualny czas kopiowania.*/
    void erase(K const &k) {
        try {
            about_to_modify(false); // potencjalny restore w środku
        } catch (bad_alloc &e) {
            throw;
        }
//        about_to_modify(false);
        typename unordered_map<K, typename std::list<pair<K, V>>::iterator, Hash>::iterator map_it;
        try {
            map_it = buf_ptr->map_data.find(k);
        }
        catch (...) {
            restore();
        }

        if (map_it != buf_ptr->map_data.end()) { // found
            auto list_it = (*map_it).second;
            buf_ptr->map_data.erase(map_it);
            buf_ptr->ordered_list.erase(list_it);
        } else {
            restore();
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
        return buf_ptr->ordered_list.empty();
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
        buf_ptr->ordered_list.clear();
    }

    /*
    Scalanie słowników. Dodaje kopie wszystkich elementów podanego słownika other
    do bieżącego słownika (this). Wartości pod kluczami już obecnymi w bieżącym
    słowniku nie są nadpisywane. Klucze ze słownika other pojawiają się w porządku
    iteracji na końcu, zachowując kolejność względem siebie.
    Złożoność czasowa oczekiwana O(n + m), gdzie m to rozmiar słownika other.*/
    void merge(insertion_ordered_map const &other) {
        auto new_iom = *this;
        for (auto const x: other) {
            new_iom.insert(x.first, x.second);
        }
        *this = new_iom;
    }
};

#endif //INSERTION_ORDERED_MAP_INSERTION_ORDERED_MAP_H

