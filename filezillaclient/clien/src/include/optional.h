#ifndef FILEZILLA_OPTIONAL_HEADER
#define FILEZILLA_OPTIONAL_HEADER

#include <type_traits>

template<class T> class CSparseOptional final
{
	// This class does what std::optional would do, if we had it.
	// Note that we do not perform "small object optimization" under the assumption
	// that in most cases the optional is not set.
public:
	CSparseOptional();
	explicit CSparseOptional(T const& v);
	CSparseOptional(CSparseOptional<T> const& v);
	CSparseOptional(CSparseOptional<T> && v);
	~CSparseOptional();

	void clear();

	explicit operator bool() const { return v_ != 0; };

	T& operator*() { return *v_; }
	T const& operator*() const { return *v_; }

	T* operator->() { return v_; }
	T const* operator->() const { return v_; }

	bool operator==(CSparseOptional<T> const& cmp) const;
	inline bool operator!=(CSparseOptional<T> const& cmp) const { return !(*this == cmp); }
	bool operator<(CSparseOptional<T> const& cmp) const;

	CSparseOptional<T>& operator=(CSparseOptional<T> const& v);
	CSparseOptional<T>& operator=(CSparseOptional<T> && v);
protected:
	T* v_;
};

template<class T> CSparseOptional<T>::CSparseOptional()
	: v_()
{
}

template<class T> CSparseOptional<T>::CSparseOptional(T const& v)
	: v_(new T(v))
{
}

template<class T> CSparseOptional<T>::CSparseOptional(CSparseOptional<T> const& v)
{
	if( v ) {
		v_ = new T(*v);
	}
	else {
		v_ = 0;
	}
}

template<class T> CSparseOptional<T>::CSparseOptional(CSparseOptional<T> && v)
{
	v_ = v.v_;
	v.v_ = 0;
}

template<class T> CSparseOptional<T>::~CSparseOptional()
{
	delete v_;
}

template<class T> void CSparseOptional<T>::clear()
{
	delete v_;
	v_ = 0;
}

template<class T> CSparseOptional<T>& CSparseOptional<T>::operator=(CSparseOptional<T> const& v)
{
	if( this != &v ) {
		delete v_;
		if( v.v_ ) {
			v_ = new T(*v.v_);
		}
		else {
			v_ = 0;
		}
	}

	return *this;
}

template<class T> CSparseOptional<T>& CSparseOptional<T>::operator=(CSparseOptional<T> && v)
{
	if( this != &v ) {
		delete v_;
		v_ = v.v_;
		v.v_ = 0;
	}

	return *this;
}

template<class T> bool CSparseOptional<T>::operator==(CSparseOptional<T> const& cmp) const
{
	if( !v_ && !cmp.v_ ) {
		return true;
	}

	if( !v_ || !cmp.v_ ) {
		return false;
	}

	return *v_ == *cmp.v_;
}

template<class T> bool CSparseOptional<T>::operator<(CSparseOptional<T> const& cmp) const
{
	if( !v_ || !cmp.v_ ) {
		return cmp.operator bool();
	}

	return *v_ < *cmp.v_;
}

#endif
