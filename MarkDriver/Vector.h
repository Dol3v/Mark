#pragma once

#include "Macros.h"

constexpr size_t EXPANSION_SIZE = 2;

/*
	Dynamic array implementation.
*/
template <typename T, POOL_TYPE PoolType = PagedPool, ULONG PoolTag = DRIVER_TAG>
class Vector {

	class VectorIterator {

		// some tags, we won't use stuff from <algorithm> but it can't hurt
		using value_type = T;
		using pointer = T*;
		using reference = T&;

	public:
		VectorIterator(pointer ptr) : ptr(ptr) {}

		reference operator*() const { return *ptr; }
		pointer operator->() { return ptr; }

		// Prefix increment
		VectorIterator& operator++() { ptr++; return *this; }
		// Postfix increment
		VectorIterator operator++(int) { auto tmp = *this; ++(*this); return tmp; }

		friend bool operator== (const VectorIterator& First, const VectorIterator& Second) { return First.ptr == Second.ptr; }
		friend bool operator!= (const VectorIterator& First, const VectorIterator& Second) { return First.ptr != Second.ptr; }

	private:
		pointer ptr;
	};

public:
	Vector() {
		this->buffer = reinterpret_cast<T*>(::ExAllocatePoolWithTag(PoolType, sizeof(T), PoolTag));
		if (this->buffer == nullptr) {
			LOG_FAIL("Buffer allocation for type " STRINGIFY(T) " failed"); // prob STRINGIFY(T) would result in T tbh but it's not like we'll get to this line
		}
		this->capacity = 1;
	}

	Vector(const Vector& Other) {
		this->buffer = reinterpret_cast<T*>(::ExAllocatePoolWithTag(PoolType, sizeof(T) * Other.capacity, PoolTag));
		this->capacity = Other.capacity;
		this->elements = Other.elements;
		::RtlCopyMemory(this->buffer, Other.buffer, Other.elements);
	}

	Vector& operator=(const Vector& Other) {
		this->buffer = reinterpret_cast<T*>(::ExAllocatePoolWithTag(PoolType, sizeof(T) * Other.capacity, PoolTag));
		this->capacity = Other.capacity;
		this->elements = Other.elements;
		::RtlCopyMemory(this->buffer, Other.buffer, Other.elements);
		return *this;
	}

	Vector(const Vector&& Other) {
		this->buffer = Other.buffer;
		this->capacity = Other.capacity;
		this->elements = Other.elements;

		Other.buffer = nullptr;
		Other.capacity = 0;
		Other.elements = 0;
	}

	Vector& operator=(const Vector&& Other) {
		this->buffer = Other.buffer;
		this->capacity = Other.capacity;
		this->elements = Other.elements;

		Other.buffer = nullptr;
		Other.capacity = 0;
		Other.elements = 0;
		return *this;
	}

	/*
		Inserts an element into the vector.

		Raises STATUS_INSUFFICENT_RESOURCES if allocation failed
	*/
	void Insert(const T& Element, bool ShouldIncrease = true) {
		if (elements == capacity) {
			if (ShouldIncrease) {
				this->IncreaseTo(EXPANSION_SIZE * this->capacity);
			}
			else {
				this->Clear();
			}
		}
		this->buffer[this->elements++] = Element;
	}

	/*
		Retrieves an element from the vector.

		Raises STATUS_INDEX_OUT_OF_BOUNDS if given invalid index.
	*/
	T& Get(size_t Index) const {
		if (Index >= this->elements) {
			LOG_FAIL_VA("Index %ull is bigger than the %zu current elements", Index, this->elements);
			::ExRaiseStatus(STATUS_INDEX_OUT_OF_BOUNDS);
		}
		return this->buffer[Index];
	}

	/*
		Retrieves the last element from the list.

		Raises STATUS_BAD_DATA if the vector is empty.
	*/
	T& GetLast() const {
		if (this->elements == 0) {
			LOG_FAIL("Vector is empty, no last element exists");
			::ExRaiseStatus(STATUS_BAD_DATA);
		}
		return this->buffer[this->elements - 1];
	}

	/*
		Reserves a given number of empty slots.
	*/
	void Reserve(size_t NumberOfElements) {
		auto amountToIncrease = NumberOfElements - (this->capacity - this->elements);
		if (amountToIncrease > 0) {
			this->IncreaseTo(this->elements + amountToIncrease);
		} 
	}

	/*
		Saves only the [Start, End) entries specified.
	*/
	void SubVec(size_t Start, size_t End) {
		if (Start > End || End > this->elements)
			ExRaiseStatus(STATUS_INDEX_OUT_OF_BOUNDS);
		for (size_t i = Start; i < End; ++i) {
			memcpy(buffer + (i - Start), buffer + i, sizeof(T));
		}
		RtlZeroMemory(buffer + End, (elements - End + 1) * sizeof(T));
		this->elements = End - Start;
	}

	auto GetBuffer() {
		return this->buffer;
	}

	size_t Size() {
		return this->elements;
	}

	VectorIterator begin() { return VectorIterator(this->buffer); }
	VectorIterator end() { return VectorIterator(&this->buffer[this->elements]); }

	Vector::~Vector() {
		if (this->buffer != nullptr) {
			::ExFreePoolWithTag(this->buffer, PoolTag);
		}
	}

private:
	T* buffer = nullptr;
	size_t elements = 0;
	size_t capacity = 0;

	// Increase the size of the vector
	void IncreaseTo(size_t Amount) {
		NT_ASSERT(Amount > this->capacity);
		auto* newBuffer = static_cast<T*>(::ExAllocatePoolWithTag(PoolType, Amount * sizeof(T), PoolTag));
		if (newBuffer == nullptr) {
			LOG_FAIL_VA("Failed to expand vector by 2 from %zu", this->capacity);
			::ExRaiseStatus(STATUS_INSUFFICIENT_RESOURCES);
		}

		::RtlCopyMemory(newBuffer, this->buffer, this->capacity * sizeof(T));
		::ExFreePoolWithTag(this->buffer, PoolTag);
		this->buffer = newBuffer;
		this->capacity = Amount;
	}

	// Clears all elements in the buffer
	void Clear() {
		RtlZeroMemory(buffer, elements * sizeof(T));
		elements = 0;
	}
};
