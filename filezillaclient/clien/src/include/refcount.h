#ifndef __REFCOUNT_H__
#define __REFCOUNT_H__

#include <memory>
#include <type_traits>

// Template class to refcount objects with COW semantics.
// This class is thread-safe under the following assumptions:
// - The object stored in it must be thread-safe for reading
// - Any instance Get() is called on must never be used in
//   different threads.
template<class T> class CRefcountObject final
{
public:
	CRefcountObject();
	CRefcountObject(CRefcountObject<T> const& v);
	CRefcountObject(CRefcountObject<T> && v);
	explicit CRefcountObject(const T& v);

	void clear();

	T& Get();

	const T& operator*() const;
	const T* operator->() const;

	// Comparison operators are deep. If two intances point to
	// different objects, those objects are compared
	bool operator==(CRefcountObject<T> const& cmp) const;
	bool operator==(T const& cmp) const;
	bool operator<(CRefcountObject<T> const& cmp) const;
	bool operator<(T const& cmp) const;

	inline bool operator!=(const CRefcountObject<T>& cmp) const { return !(*this == cmp); }

	CRefcountObject<T>& operator=(CRefcountObject<T> const& v);
	CRefcountObject<T>& operator=(CRefcountObject<T> && v);
protected:
	std::shared_ptr<T> data_;
};

template<class T> class CRefcountObject_Uninitialized final
{
	/* Almost same as CRefcountObject but does not allocate
	   an object initially.
	   You need to ensure to assign some data prior to calling
	   operator* or ->, otherwise you'll dereference the null-pointer.
	 */
public:
	CRefcountObject_Uninitialized();
	CRefcountObject_Uninitialized(const CRefcountObject_Uninitialized<T>& v);
	explicit CRefcountObject_Uninitialized(const T& v);

	void clear();

	T& Get();

	const T& operator*() const;
	const T* operator->() const;

	bool operator==(const CRefcountObject_Uninitialized<T>& cmp) const;
	inline bool operator!=(const CRefcountObject_Uninitialized<T>& cmp) const { return !(*this == cmp); }
	bool operator<(const CRefcountObject_Uninitialized<T>& cmp) const;

	CRefcountObject_Uninitialized<T>& operator=(const CRefcountObject_Uninitialized<T>& v);

	bool operator!() const { return !data_; }
	explicit operator bool() const { return data_; }

	bool empty() const { return data_.get(); }
protected:
	std::shared_ptr<T> data_;
};

template<class T> bool CRefcountObject<T>::operator==(CRefcountObject<T> const& cmp) const
{
	if (data_ == cmp.data_)
		return true;

	return *data_ == *cmp.data_;
}

template<class T> bool CRefcountObject<T>::operator==(T const& cmp) const
{
	return *data_ == cmp;
}

template<class T> CRefcountObject<T>::CRefcountObject()
	: data_(std::make_shared<T>())
{
}

template<class T> CRefcountObject<T>::CRefcountObject(CRefcountObject<T> const& v)
	: data_(v.data_)
{
}

template<class T> CRefcountObject<T>::CRefcountObject(CRefcountObject<T> && v)
	: data_(std::move(v.data_))
{
}

template<class T> CRefcountObject<T>::CRefcountObject(const T& v)
	: data_(std::make_shared<T>(v))
{
}

template<class T> T& CRefcountObject<T>::Get()
{
	if (!data_.unique()) {
		data_ = std::make_shared<T>(*data_);
	}

	return *data_.get();
}

template<class T> CRefcountObject<T>& CRefcountObject<T>::operator=(CRefcountObject<T> const& v)
{
	data_ = v.data_;
	return *this;
}

template<class T> CRefcountObject<T>& CRefcountObject<T>::operator=(CRefcountObject<T> && v)
{
	data_ = std::move(v.data_);
	return *this;
}

template<class T> bool CRefcountObject<T>::operator<(CRefcountObject<T> const& cmp) const
{
	if (data_ == cmp.data_)
		return false;

	return *data_.get() < *cmp.data_.get();
}

template<class T> bool CRefcountObject<T>::operator<(T const& cmp) const
{
	if (!data_) {
		return true;
	}
	return *data_.get() < cmp;
}

template<class T> void CRefcountObject<T>::clear()
{
	if (data_.unique()) {
		*data_.get() = T();
	}
	else {
		data_ = std::make_shared<T>();
	}
}

template<class T> const T& CRefcountObject<T>::operator*() const
{
	return *data_;
}

template<class T> const T* CRefcountObject<T>::operator->() const
{
	return data_.get();
}

// The same for the uninitialized version
template<class T> bool CRefcountObject_Uninitialized<T>::operator==(const CRefcountObject_Uninitialized<T>& cmp) const
{
	if (data_ == cmp.data_)
		return true;

	if (!data_) {
		return !cmp.data_;
	}
	else if (!cmp.data_) {
		return false;
	}
	return *data_.get() == *cmp.data_.get();
}

template<class T> CRefcountObject_Uninitialized<T>::CRefcountObject_Uninitialized()
{
}

template<class T> CRefcountObject_Uninitialized<T>::CRefcountObject_Uninitialized(const CRefcountObject_Uninitialized<T>& v)
	: data_(v.data_)
{
}

template<class T> CRefcountObject_Uninitialized<T>::CRefcountObject_Uninitialized(const T& v)
	: data_(std::make_shared<T>(v))
{
}

template<class T> T& CRefcountObject_Uninitialized<T>::Get()
{
	if (!data_) {
		data_ = std::make_shared<T>();
	}
	else if (!data_.unique()) {
		data_ = std::make_shared<T>(*data_);
	}

	return *data_.get();
}

template<class T> CRefcountObject_Uninitialized<T>& CRefcountObject_Uninitialized<T>::operator=(const CRefcountObject_Uninitialized<T>& v)
{
	data_ = v.data_;
	return *this;
}

template<class T> bool CRefcountObject_Uninitialized<T>::operator<(const CRefcountObject_Uninitialized<T>& cmp) const
{
	if (data_ == cmp.data_) {
		return false;
	}
	else if (!data_) {
		return cmp.data_;
	}
	else if (!cmp.data_) {
		return false;
	}

	return *data_.get() < *cmp.data_.get();

}

template<class T> void CRefcountObject_Uninitialized<T>::clear()
{
	data_.reset();
}

template<class T> const T& CRefcountObject_Uninitialized<T>::operator*() const
{
	return *data_.get();
}

template<class T> const T* CRefcountObject_Uninitialized<T>::operator->() const
{
	return data_.get();
}

#endif //__REFCOUNT_H__
