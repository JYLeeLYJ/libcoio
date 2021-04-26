#ifndef COIO_MATCH_TEMPLATE_HPP
#define COIO_MATCH_TEMPLATE_HPP

#include <type_traits>

namespace coio{
    
    //cannot match alias template
    template<template<class ...> class TMP>
    struct match_template {
        
        template<class T>
        struct is_match : std::false_type{};

        template<class ...T>
        struct is_match <TMP<T...>> : std::true_type{};
    };

    template<template<class ...> class TMP , class T>
    inline constexpr bool is_match_template_v = typename match_template<TMP>::is_match<T>{};

}

#endif