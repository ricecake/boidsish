#include <iostream>
#include <tuple>
#include <type_traits>

template<typename T>
concept is_tuple_like = requires {
    std::tuple_size<T>::value;
};

int main() {
    std::cout << "Tuple: " << is_tuple_like<std::tuple<int, int>> << std::endl;
    std::cout << "Int: " << is_tuple_like<int> << std::endl;
    return 0;
}
