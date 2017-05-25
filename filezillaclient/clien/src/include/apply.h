#ifndef FILEZILLA_APPLY_HEADER
#define FILEZILLA_APPLY_HEADER

#include <tuple>
#include <type_traits>

// apply takes a function and a tuple as argument
// and calls the function with the tuple's elements as argument

// An integer sequence...
template<int ...>
struct int_seq {};

// ... and the machinery to construct it
template<int N, int ...Seq>
struct int_seq_gen : int_seq_gen<N - 1, N - 1, Seq...> {};

template<int ...Seq>
struct int_seq_gen<0, Seq...>
{
	typedef int_seq<Seq...> type;
};

// Apply tuple to ordinary functor
template<typename F, typename T, int... I>
auto apply_(F&& f, T&& t, int_seq<I...> const&) -> decltype(std::forward<F>(f)(std::get<I>(std::forward<T>(t))...))
{
	return std::forward<F>(f)(std::get<I>(std::forward<T>(t))...);
}

template<typename F, typename T, typename Seq = typename int_seq_gen<std::tuple_size<typename std::remove_reference<T>::type>::value>::type>
auto apply(F && f, T&& args) -> decltype(apply_(std::forward<F>(f), std::forward<T>(args), Seq()))
{
	return apply_(std::forward<F>(f), std::forward<T>(args), Seq());
}

// Apply tuple to pointer to member function
template<typename Obj, typename F, typename T, int... I>
auto apply_(Obj&& obj, F&& f, T&& t, int_seq<I...> const&) -> decltype((std::forward<Obj>(obj)->*std::forward<F>(f))(std::get<I>(std::forward<T>(t))...))
{
	return (std::forward<Obj>(obj)->*std::forward<F>(f))(std::get<I>(std::forward<T>(t))...);
}

template<typename Obj, typename F, typename T, typename Seq = typename int_seq_gen<std::tuple_size<typename std::remove_reference<T>::type>::value>::type>
auto apply(Obj&& obj, F && f, T&& args) -> decltype(apply_(std::forward<Obj>(obj), std::forward<F>(f), std::forward<T>(args), Seq()))
{
	return apply_(std::forward<Obj>(obj), std::forward<F>(f), std::forward<T>(args), Seq());
}

#endif
