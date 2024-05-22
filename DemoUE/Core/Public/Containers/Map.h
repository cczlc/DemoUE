#pragma once

/**
 * The base class of maps from keys to values.  Implemented using a TSet of key-value pairs with a custom KeyFuncs,
 * with the same O(1) addition, removal, and finding.
 *
 * The ByHash() functions are somewhat dangerous but particularly useful in two scenarios:
 * -- Heterogeneous lookup to avoid creating expensive keys like FString when looking up by const TCHAR*.
 *	  You must ensure the hash is calculated in the same way as ElementType is hashed.
 *    If possible put both ComparableKey and ElementType hash functions next to each other in the same header
 *    to avoid bugs when the ElementType hash function is changed.
 * -- Reducing contention around hash tables protected by a lock. It is often important to incur
 *    the cache misses of reading key data and doing the hashing *before* acquiring the lock.
 **/
template <typename KeyType, typename ValueType, typename SetAllocator, typename KeyFuncs>
class TMapBase
{
	template <typename OtherKeyType, typename OtherValueType, typename OtherSetAllocator, typename OtherKeyFuncs>
	friend class TMapBase;

public:
	typedef typename TTypeTraits<KeyType  >::ConstPointerType KeyConstPointerType;
	typedef typename TTypeTraits<KeyType  >::ConstInitType    KeyInitType;
	typedef typename TTypeTraits<ValueType>::ConstInitType    ValueInitType;
	typedef TPair<KeyType, ValueType> ElementType;

protected:
	TMapBase() = default;
	TMapBase(TMapBase&&) = default;
	TMapBase(const TMapBase&) = default;
	TMapBase& operator=(TMapBase&&) = default;
	TMapBase& operator=(const TMapBase&) = default;

	/** Constructor for moving elements from a TMap with a different SetAllocator */
	template<typename OtherSetAllocator>
	TMapBase(TMapBase<KeyType, ValueType, OtherSetAllocator, KeyFuncs>&& Other)
		: Pairs(MoveTemp(Other.Pairs))
	{ }

	/** Constructor for copying elements from a TMap with a different SetAllocator */
	template<typename OtherSetAllocator>
	TMapBase(const TMapBase<KeyType, ValueType, OtherSetAllocator, KeyFuncs>& Other)
		: Pairs(Other.Pairs)
	{ }

	/** Assignment operator for moving elements from a TMap with a different SetAllocator */
	template<typename OtherSetAllocator>
	TMapBase& operator=(TMapBase<KeyType, ValueType, OtherSetAllocator, KeyFuncs>&& Other)
	{
		Pairs = MoveTemp(Other.Pairs);
		return *this;
	}

	/** Assignment operator for copying elements from a TMap with a different SetAllocator */
	template<typename OtherSetAllocator>
	TMapBase& operator=(const TMapBase<KeyType, ValueType, OtherSetAllocator, KeyFuncs>& Other)
	{
		Pairs = Other.Pairs;
		return *this;
	}

public:

	/**
	 * Compare this map with another for equality. Does not make any assumptions about Key order.
	 * NOTE: this might be a candidate for operator== but it was decided to make it an explicit function
	 *  since it can potentially be quite slow.
	 *
	 * @param Other The other map to compare against
	 * @returns True if both this and Other contain the same keys with values that compare ==
	 */
	bool OrderIndependentCompareEqual(const TMapBase& Other) const
	{
		// first check counts (they should be the same obviously)
		if (Num() != Other.Num())
		{
			return false;
		}

		// since we know the counts are the same, we can just iterate one map and check for existence in the other
		for (typename ElementSetType::TConstIterator It(Pairs); It; ++It)
		{
			const ValueType* BVal = Other.Find(It->Key);
			if (BVal == nullptr)
			{
				return false;
			}
			if (!(*BVal == It->Value))
			{
				return false;
			}
		}

		// all fields in A match B and A and B's counts are the same (so there can be no fields in B not in A)
		return true;
	}

	/**
	 * Removes all elements from the map.
	 *
	 * This method potentially leaves space allocated for an expected
	 * number of elements about to be added.
	 *
	 * @param ExpectedNumElements The number of elements about to be added to the set.
	 */
	FORCEINLINE void Empty(int32 ExpectedNumElements = 0)
	{
		Pairs.Empty(ExpectedNumElements);
	}

	/** Efficiently empties out the map but preserves all allocations and capacities */
	FORCEINLINE void Reset()
	{
		Pairs.Reset();
	}

	/** Shrinks the pair set to avoid slack. */
	FORCEINLINE void Shrink()
	{
		Pairs.Shrink();
	}

	/** Compacts the pair set to remove holes */
	FORCEINLINE void Compact()
	{
		Pairs.Compact();
	}

	/** Compacts the pair set to remove holes. Does not change the iteration order of the elements. */
	FORCEINLINE void CompactStable()
	{
		Pairs.CompactStable();
	}

	/** Preallocates enough memory to contain Number elements */
	FORCEINLINE void Reserve(int32 Number)
	{
		Pairs.Reserve(Number);
	}

	/**
	 * Returns true if the map is empty and contains no elements.
	 *
	 * @returns True if the map is empty.
	 * @see Num
	 */
	bool IsEmpty() const
	{
		return Pairs.IsEmpty();
	}

	/** @return The number of elements in the map. */
	FORCEINLINE int32 Num() const
	{
		return Pairs.Num();
	}

	/** @return The max valid index of the elements in the sparse storage. */
	[[nodiscard]] FORCEINLINE int32 GetMaxIndex() const
	{
		return Pairs.GetMaxIndex();
	}

	/**
	 * Checks whether an element id is valid.
	 * @param Id - The element id to check.
	 * @return true if the element identifier refers to a valid element in this map.
	 */
	[[nodiscard]] FORCEINLINE bool IsValidId(FSetElementId Id) const
	{
		return Pairs.IsValidId(Id);
	}

	/** Return a mapped pair by internal identifier. Element must be valid (see @IsValidId). */
	[[nodiscard]] FORCEINLINE ElementType& Get(FSetElementId Id)
	{
		return Pairs[Id];
	}

	/** Return a mapped pair by internal identifier.  Element must be valid (see @IsValidId).*/
	[[nodiscard]] FORCEINLINE const ElementType& Get(FSetElementId Id) const
	{
		return Pairs[Id];
	}

	/**
	 * Get the unique keys contained within this map.
	 *
	 * @param OutKeys Upon return, contains the set of unique keys in this map.
	 * @return The number of unique keys in the map.
	 */
	template<typename Allocator> int32 GetKeys(TArray<KeyType, Allocator>& OutKeys) const
	{
		OutKeys.Reset();

		TSet<KeyType> VisitedKeys;
		VisitedKeys.Reserve(Num());

		// Presize the array if we know there are supposed to be no duplicate keys
		if (!KeyFuncs::bAllowDuplicateKeys)
		{
			OutKeys.Reserve(Num());
		}

		for (typename ElementSetType::TConstIterator It(Pairs); It; ++It)
		{
			// Even if bAllowDuplicateKeys is false, we still want to filter for duplicate
			// keys due to maps with keys that can be invalidated (UObjects, TWeakObj, etc.)
			if (!VisitedKeys.Contains(It->Key))
			{
				OutKeys.Add(It->Key);
				VisitedKeys.Add(It->Key);
			}
		}

		return OutKeys.Num();
	}

	/**
	 * Get the unique keys contained within this map.
	 *
	 * @param OutKeys Upon return, contains the set of unique keys in this map.
	 * @return The number of unique keys in the map.
	 */
	template<typename Allocator> int32 GetKeys(TSet<KeyType, Allocator>& OutKeys) const
	{
		OutKeys.Reset();

		// Presize the set if we know there are supposed to be no duplicate keys
		if (!KeyFuncs::bAllowDuplicateKeys)
		{
			OutKeys.Reserve(Num());
		}

		for (typename ElementSetType::TConstIterator It(Pairs); It; ++It)
		{
			OutKeys.Add(It->Key);
		}

		return OutKeys.Num();
	}

	/**
	 * Helper function to return the amount of memory allocated by this container .
	 * Only returns the size of allocations made directly by the container, not the elements themselves.
	 *
	 * @return Number of bytes allocated by this container.
	 * @see CountBytes
	 */
	FORCEINLINE SIZE_T GetAllocatedSize() const
	{
		return Pairs.GetAllocatedSize();
	}

	/**
	 * Track the container's memory use through an archive.
	 *
	 * @param Ar The archive to use.
	 * @see GetAllocatedSize
	 */
	FORCEINLINE void CountBytes(FArchive& Ar) const
	{
		Pairs.CountBytes(Ar);
	}

	/**
	 * Set the value associated with a key.
	 *
	 * @param InKey The key to associate the value with.
	 * @param InValue The value to associate with the key.
	 * @return A reference to the value as stored in the map. The reference is only valid until the next change to any key in the map.
	 */
	FORCEINLINE ValueType& Add(const KeyType& InKey, const ValueType& InValue) { return Emplace(InKey, InValue); }
	FORCEINLINE ValueType& Add(const KeyType& InKey, ValueType&& InValue) { return Emplace(InKey, MoveTempIfPossible(InValue)); }
	FORCEINLINE ValueType& Add(KeyType&& InKey, const ValueType& InValue) { return Emplace(MoveTempIfPossible(InKey), InValue); }
	FORCEINLINE ValueType& Add(KeyType&& InKey, ValueType&& InValue) { return Emplace(MoveTempIfPossible(InKey), MoveTempIfPossible(InValue)); }

	/** See Add() and class documentation section on ByHash() functions */
	FORCEINLINE ValueType& AddByHash(uint32 KeyHash, const KeyType& InKey, const ValueType& InValue) { return EmplaceByHash(KeyHash, InKey, InValue); }
	FORCEINLINE ValueType& AddByHash(uint32 KeyHash, const KeyType& InKey, ValueType&& InValue) { return EmplaceByHash(KeyHash, InKey, MoveTempIfPossible(InValue)); }
	FORCEINLINE ValueType& AddByHash(uint32 KeyHash, KeyType&& InKey, const ValueType& InValue) { return EmplaceByHash(KeyHash, MoveTempIfPossible(InKey), InValue); }
	FORCEINLINE ValueType& AddByHash(uint32 KeyHash, KeyType&& InKey, ValueType&& InValue) { return EmplaceByHash(KeyHash, MoveTempIfPossible(InKey), MoveTempIfPossible(InValue)); }

	/**
	 * Set a default value associated with a key.
	 *
	 * @param InKey The key to associate the value with.
	 * @return A reference to the value as stored in the map. The reference is only valid until the next change to any key in the map.
	 */
	FORCEINLINE ValueType& Add(const KeyType& InKey) { return Emplace(InKey); }
	FORCEINLINE ValueType& Add(KeyType&& InKey) { return Emplace(MoveTempIfPossible(InKey)); }

	/** See Add() and class documentation section on ByHash() functions */
	FORCEINLINE ValueType& AddByHash(uint32 KeyHash, const KeyType& InKey) { return EmplaceByHash(KeyHash, InKey); }
	FORCEINLINE ValueType& AddByHash(uint32 KeyHash, KeyType&& InKey) { return EmplaceByHash(KeyHash, MoveTempIfPossible(InKey)); }

	/**
	 * Set the value associated with a key.
	 *
	 * @param InKeyValue A Tuple containing the Key and Value to associate together
	 * @return A reference to the value as stored in the map. The reference is only valid until the next change to any key in the map.
	 */
	FORCEINLINE ValueType& Add(const TTuple<KeyType, ValueType>& InKeyValue) { return Emplace(InKeyValue.Key, InKeyValue.Value); }
	FORCEINLINE ValueType& Add(TTuple<KeyType, ValueType>&& InKeyValue) { return Emplace(MoveTempIfPossible(InKeyValue.Key), MoveTempIfPossible(InKeyValue.Value)); }

	/**
	 * Sets the value associated with a key.
	 *
	 * @param InKey The key to associate the value with.
	 * @param InValue The value to associate with the key.
	 * @return A reference to the value as stored in the map. The reference is only valid until the next change to any key in the map.	 */
	template <typename InitKeyType = KeyType, typename InitValueType = ValueType>
	ValueType& Emplace(InitKeyType&& InKey, InitValueType&& InValue)
	{
		const FSetElementId PairId = Pairs.Emplace(TPairInitializer<InitKeyType&&, InitValueType&&>(Forward<InitKeyType>(InKey), Forward<InitValueType>(InValue)));

		return Pairs[PairId].Value;
	}

	/** See Emplace() and class documentation section on ByHash() functions */
	template <typename InitKeyType = KeyType, typename InitValueType = ValueType>
	ValueType& EmplaceByHash(uint32 KeyHash, InitKeyType&& InKey, InitValueType&& InValue)
	{
		const FSetElementId PairId = Pairs.EmplaceByHash(KeyHash, TPairInitializer<InitKeyType&&, InitValueType&&>(Forward<InitKeyType>(InKey), Forward<InitValueType>(InValue)));

		return Pairs[PairId].Value;
	}

	/**
	 * Set a default value associated with a key.
	 *
	 * @param InKey The key to associate the value with.
	 * @return A reference to the value as stored in the map. The reference is only valid until the next change to any key in the map.
	 */
	template <typename InitKeyType = KeyType>
	ValueType& Emplace(InitKeyType&& InKey)
	{
		const FSetElementId PairId = Pairs.Emplace(TKeyInitializer<InitKeyType&&>(Forward<InitKeyType>(InKey)));

		return Pairs[PairId].Value;
	}

	/** See Emplace() and class documentation section on ByHash() functions */
	template <typename InitKeyType = KeyType>
	ValueType& EmplaceByHash(uint32 KeyHash, InitKeyType&& InKey)
	{
		const FSetElementId PairId = Pairs.EmplaceByHash(KeyHash, TKeyInitializer<InitKeyType&&>(Forward<InitKeyType>(InKey)));

		return Pairs[PairId].Value;
	}

	/**
	 * Remove all value associations for a key.
	 *
	 * @param InKey The key to remove associated values for.
	 * @return The number of values that were associated with the key.
	 */
	FORCEINLINE int32 Remove(KeyConstPointerType InKey)
	{
		const int32 NumRemovedPairs = Pairs.Remove(InKey);
		return NumRemovedPairs;
	}

	/** See Remove() and class documentation section on ByHash() functions */
	template<typename ComparableKey>
	FORCEINLINE int32 RemoveByHash(uint32 KeyHash, const ComparableKey& Key)
	{
		const int32 NumRemovedPairs = Pairs.RemoveByHash(KeyHash, Key);
		return NumRemovedPairs;
	}

	/**
	 * Find the key associated with the specified value.
	 *
	 * The time taken is O(N) in the number of pairs.
	 *
	 * @param Value The value to search for
	 * @return A pointer to the key associated with the specified value,
	 *     or nullptr if the value isn't contained in this map. The pointer
	 *     is only valid until the next change to any key in the map.
	 */
	const KeyType* FindKey(ValueInitType Value) const
	{
		for (typename ElementSetType::TConstIterator PairIt(Pairs); PairIt; ++PairIt)
		{
			if (PairIt->Value == Value)
			{
				return &PairIt->Key;
			}
		}
		return nullptr;
	}

	/**
	 * Filters the elements in the map based on a predicate functor.
	 *
	 * @param Pred The functor to apply to each element.
	 * @returns TMap with the same type as this object which contains
	 *          the subset of elements for which the functor returns true.
	 */
	template <typename Predicate>
	TMap<KeyType, ValueType, SetAllocator, KeyFuncs> FilterByPredicate(Predicate Pred) const
	{
		TMap<KeyType, ValueType, SetAllocator, KeyFuncs> FilterResults;
		FilterResults.Reserve(Pairs.Num());
		for (const ElementType& Pair : Pairs)
		{
			if (Pred(Pair))
			{
				FilterResults.Add(Pair);
			}
		}
		return FilterResults;
	}

	/**
	 * Find the value associated with a specified key.
	 *
	 * @param Key The key to search for.
	 * @return A pointer to the value associated with the specified key, or nullptr if the key isn't contained in this map.  The pointer
	 *			is only valid until the next change to any key in the map.
	 */
	FORCEINLINE ValueType* Find(KeyConstPointerType Key)
	{
		if (auto* Pair = Pairs.Find(Key))
		{
			return &Pair->Value;
		}

		return nullptr;
	}
	FORCEINLINE const ValueType* Find(KeyConstPointerType Key) const
	{
		return const_cast<TMapBase*>(this)->Find(Key);
	}

	/** See Find() and class documentation section on ByHash() functions */
	template<typename ComparableKey>
	FORCEINLINE ValueType* FindByHash(uint32 KeyHash, const ComparableKey& Key)
	{
		if (auto* Pair = Pairs.FindByHash(KeyHash, Key))
		{
			return &Pair->Value;
		}

		return nullptr;
	}
	template<typename ComparableKey>
	FORCEINLINE const ValueType* FindByHash(uint32 KeyHash, const ComparableKey& Key) const
	{
		return const_cast<TMapBase*>(this)->FindByHash(KeyHash, Key);
	}

private:
	FORCEINLINE static uint32 HashKey(const KeyType& Key)
	{
		return KeyFuncs::GetKeyHash(Key);
	}

	/**
	 * Find the value associated with a specified key, or if none exists,
	 * adds a value using the default constructor.
	 *
	 * @param Key The key to search for.
	 * @return A reference to the value associated with the specified key.
	 */
	template <typename InitKeyType>
	ValueType& FindOrAddImpl(uint32 KeyHash, InitKeyType&& Key)
	{
		if (auto* Pair = Pairs.FindByHash(KeyHash, Key))
		{
			return Pair->Value;
		}

		return AddByHash(KeyHash, Forward<InitKeyType>(Key));
	}

	/**
	 * Find the value associated with a specified key, or if none exists,
	 * adds the value
	 *
	 * @param Key The key to search for.
	 * @param Value The value to associate with the key.
	 * @return A reference to the value associated with the specified key.
	 */
	template <typename InitKeyType, typename InitValueType>
	ValueType& FindOrAddImpl(uint32 KeyHash, InitKeyType&& Key, InitValueType&& Value)
	{
		if (auto* Pair = Pairs.FindByHash(KeyHash, Key))
		{
			return Pair->Value;
		}

		return AddByHash(KeyHash, Forward<InitKeyType>(Key), Forward<InitValueType>(Value));
	}

public:

	/**
	 * Find the value associated with a specified key, or if none exists,
	 * adds a value using the default constructor.
	 *
	 * @param Key The key to search for.
	 * @return A reference to the value associated with the specified key.
	 */
	FORCEINLINE ValueType& FindOrAdd(const KeyType& Key) { return FindOrAddImpl(HashKey(Key), Key); }
	FORCEINLINE ValueType& FindOrAdd(KeyType&& Key) { return FindOrAddImpl(HashKey(Key), MoveTempIfPossible(Key)); }

	/** See FindOrAdd() and class documentation section on ByHash() functions */
	FORCEINLINE ValueType& FindOrAddByHash(uint32 KeyHash, const KeyType& Key) { return FindOrAddImpl(KeyHash, Key); }
	FORCEINLINE ValueType& FindOrAddByHash(uint32 KeyHash, KeyType&& Key) { return FindOrAddImpl(KeyHash, MoveTempIfPossible(Key)); }

	/**
	 * Find the value associated with a specified key, or if none exists,
	 * adds a value using the default constructor.
	 *
	 * @param Key The key to search for.
	 * @param Value The value to associate with the key.
	 * @return A reference to the value associated with the specified key.
	 */
	FORCEINLINE ValueType& FindOrAdd(const KeyType& Key, const ValueType& Value) { return FindOrAddImpl(HashKey(Key), Key, Value); }
	FORCEINLINE ValueType& FindOrAdd(const KeyType& Key, ValueType&& Value) { return FindOrAddImpl(HashKey(Key), Key, MoveTempIfPossible(Value)); }
	FORCEINLINE ValueType& FindOrAdd(KeyType&& Key, const ValueType& Value) { return FindOrAddImpl(HashKey(Key), MoveTempIfPossible(Key), Value); }
	FORCEINLINE ValueType& FindOrAdd(KeyType&& Key, ValueType&& Value) { return FindOrAddImpl(HashKey(Key), MoveTempIfPossible(Key), MoveTempIfPossible(Value)); }

	/** See FindOrAdd() and class documentation section on ByHash() functions */
	FORCEINLINE ValueType& FindOrAddByHash(uint32 KeyHash, const KeyType& Key, const ValueType& Value) { return FindOrAddImpl(KeyHash, Key, Value); }
	FORCEINLINE ValueType& FindOrAddByHash(uint32 KeyHash, const KeyType& Key, ValueType&& Value) { return FindOrAddImpl(KeyHash, Key, MoveTempIfPossible(Value)); }
	FORCEINLINE ValueType& FindOrAddByHash(uint32 KeyHash, KeyType&& Key, const ValueType& Value) { return FindOrAddImpl(KeyHash, MoveTempIfPossible(Key), Value); }
	FORCEINLINE ValueType& FindOrAddByHash(uint32 KeyHash, KeyType&& Key, ValueType&& Value) { return FindOrAddImpl(KeyHash, MoveTempIfPossible(Key), MoveTempIfPossible(Value)); }

	/**
	 * Find a reference to the value associated with a specified key.
	 *
	 * @param Key The key to search for.
	 * @return The value associated with the specified key, or triggers an assertion if the key does not exist.
	 */
	FORCEINLINE const ValueType& FindChecked(KeyConstPointerType Key) const
	{
		const auto* Pair = Pairs.Find(Key);
		check(Pair != nullptr);
		return Pair->Value;
	}

	/**
	 * Find a reference to the value associated with a specified key.
	 *
	 * @param Key The key to search for.
	 * @return The value associated with the specified key, or triggers an assertion if the key does not exist.
	 */
	FORCEINLINE ValueType& FindChecked(KeyConstPointerType Key)
	{
		auto* Pair = Pairs.Find(Key);
		check(Pair != nullptr);
		return Pair->Value;
	}

	/**
	 * Find the value associated with a specified key.
	 *
	 * @param Key The key to search for.
	 * @return The value associated with the specified key, or the default value for the ValueType if the key isn't contained in this map.
	 */
	FORCEINLINE ValueType FindRef(KeyConstPointerType Key) const
	{
		if (const auto* Pair = Pairs.Find(Key))
		{
			return Pair->Value;
		}

		return ValueType();
	}

	/**
	 * Check if map contains the specified key.
	 *
	 * @param Key The key to check for.
	 * @return true if the map contains the key.
	 */
	FORCEINLINE bool Contains(KeyConstPointerType Key) const
	{
		return Pairs.Contains(Key);
	}

	/** See Contains() and class documentation section on ByHash() functions */
	template<typename ComparableKey>
	FORCEINLINE bool ContainsByHash(uint32 KeyHash, const ComparableKey& Key) const
	{
		return Pairs.ContainsByHash(KeyHash, Key);
	}

	/** Copy the key/value pairs in this map into an array. */
	TArray<ElementType> Array() const
	{
		return Pairs.Array();
	}

	/**
	 * Generate an array from the keys in this map.
	 *
	 * @param OutArray Will contain the collection of keys.
	 */
	template<typename Allocator> void GenerateKeyArray(TArray<KeyType, Allocator>& OutArray) const
	{
		OutArray.Empty(Pairs.Num());
		for (typename ElementSetType::TConstIterator PairIt(Pairs); PairIt; ++PairIt)
		{
			OutArray.Add(PairIt->Key);
		}
	}

	/**
	 * Generate an array from the values in this map.
	 *
	 * @param OutArray Will contain the collection of values.
	 */
	template<typename Allocator> void GenerateValueArray(TArray<ValueType, Allocator>& OutArray) const
	{
		OutArray.Empty(Pairs.Num());
		for (typename ElementSetType::TConstIterator PairIt(Pairs); PairIt; ++PairIt)
		{
			OutArray.Add(PairIt->Value);
		}
	}

	/**
	 * Describes the map's contents through an output device.
	 *
	 * @param Ar The output device to describe the map's contents through.
	 */
	void Dump(FOutputDevice& Ar)
	{
		Pairs.Dump(Ar);
	}

protected:
	typedef TSet<ElementType, KeyFuncs, SetAllocator> ElementSetType;

	/** The base of TMapBase iterators. */
	template<bool bConst, bool bRangedFor = false>
	class TBaseIterator
	{
	public:
		typedef typename TChooseClass<
			bConst,
			typename TChooseClass<bRangedFor, typename ElementSetType::TRangedForConstIterator, typename ElementSetType::TConstIterator>::Result,
			typename TChooseClass<bRangedFor, typename ElementSetType::TRangedForIterator, typename ElementSetType::TIterator     >::Result
		>::Result PairItType;
	private:
		typedef typename TChooseClass<bConst, const TMapBase, TMapBase>::Result MapType;
		typedef typename TChooseClass<bConst, const KeyType, KeyType>::Result ItKeyType;
		typedef typename TChooseClass<bConst, const ValueType, ValueType>::Result ItValueType;
		typedef typename TChooseClass<bConst, const typename ElementSetType::ElementType, typename ElementSetType::ElementType>::Result PairType;

	public:
		FORCEINLINE TBaseIterator(const PairItType& InElementIt)
			: PairIt(InElementIt)
		{
		}

		FORCEINLINE TBaseIterator& operator++()
		{
			++PairIt;
			return *this;
		}

		/** conversion to "bool" returning true if the iterator is valid. */
		FORCEINLINE explicit operator bool() const
		{
			return !!PairIt;
		}
		/** inverse of the "bool" operator */
		FORCEINLINE bool operator !() const
		{
			return !(bool)*this;
		}

		FORCEINLINE bool operator==(const TBaseIterator& Rhs) const { return PairIt == Rhs.PairIt; }
		FORCEINLINE bool operator!=(const TBaseIterator& Rhs) const { return PairIt != Rhs.PairIt; }

		FORCEINLINE ItKeyType& Key()   const { return PairIt->Key; }
		FORCEINLINE ItValueType& Value() const { return PairIt->Value; }

		[[nodiscard]] FORCEINLINE FSetElementId GetId() const
		{
			return PairIt.GetId();
		}

		FORCEINLINE PairType& operator* () const { return  *PairIt; }
		FORCEINLINE PairType* operator->() const { return &*PairIt; }

	protected:
		PairItType PairIt;
	};

	/** The base type of iterators that iterate over the values associated with a specified key. */
	template<bool bConst>
	class TBaseKeyIterator
	{
	private:
		typedef typename TChooseClass<bConst, typename ElementSetType::TConstKeyIterator, typename ElementSetType::TKeyIterator>::Result SetItType;
		typedef typename TChooseClass<bConst, const KeyType, KeyType>::Result ItKeyType;
		typedef typename TChooseClass<bConst, const ValueType, ValueType>::Result ItValueType;

	public:
		/** Initialization constructor. */
		FORCEINLINE TBaseKeyIterator(const SetItType& InSetIt)
			: SetIt(InSetIt)
		{
		}

		FORCEINLINE TBaseKeyIterator& operator++()
		{
			++SetIt;
			return *this;
		}

		/** conversion to "bool" returning true if the iterator is valid. */
		FORCEINLINE explicit operator bool() const
		{
			return !!SetIt;
		}
		/** inverse of the "bool" operator */
		FORCEINLINE bool operator !() const
		{
			return !(bool)*this;
		}

		[[nodiscard]] FORCEINLINE FSetElementId GetId() const
		{
			return SetIt.GetId();
		}
		FORCEINLINE ItKeyType& Key() const { return SetIt->Key; }
		FORCEINLINE ItValueType& Value() const { return SetIt->Value; }

		FORCEINLINE decltype(auto) operator* () const { return SetIt.operator* (); }
		FORCEINLINE decltype(auto) operator->() const { return SetIt.operator->(); }

	protected:
		SetItType SetIt;
	};

	/** A set of the key-value pairs in the map. */
	ElementSetType Pairs;

public:
	void WriteMemoryImage(FMemoryImageWriter& Writer) const
	{
		Pairs.WriteMemoryImage(Writer);
	}

	void CopyUnfrozen(const FMemoryUnfreezeContent& Context, void* Dst) const
	{
		Pairs.CopyUnfrozen(Context, Dst);
	}

	static void AppendHash(const FPlatformTypeLayoutParameters& LayoutParams, FSHA1& Hasher)
	{
		ElementSetType::AppendHash(LayoutParams, Hasher);
	}

	/** Map iterator. */
	class TIterator : public TBaseIterator<false>
	{
	public:

		/** Initialization constructor. */
		FORCEINLINE TIterator(TMapBase& InMap, bool bInRequiresRehashOnRemoval = false)
			: TBaseIterator<false>(InMap.Pairs.CreateIterator())
			, Map(InMap)
			, bElementsHaveBeenRemoved(false)
			, bRequiresRehashOnRemoval(bInRequiresRehashOnRemoval)
		{
		}

		/** Destructor. */
		FORCEINLINE ~TIterator()
		{
			if (bElementsHaveBeenRemoved && bRequiresRehashOnRemoval)
			{
				Map.Pairs.Relax();
			}
		}

		/** Removes the current pair from the map. */
		FORCEINLINE void RemoveCurrent()
		{
			TBaseIterator<false>::PairIt.RemoveCurrent();
			bElementsHaveBeenRemoved = true;
		}

	private:
		TMapBase& Map;
		bool      bElementsHaveBeenRemoved;
		bool      bRequiresRehashOnRemoval;
	};

	/** Const map iterator. */
	class TConstIterator : public TBaseIterator<true>
	{
	public:
		FORCEINLINE TConstIterator(const TMapBase& InMap)
			: TBaseIterator<true>(InMap.Pairs.CreateConstIterator())
		{
		}
	};

	using TRangedForIterator = TBaseIterator<false, true>;
	using TRangedForConstIterator = TBaseIterator<true, true>;

	/** Iterates over values associated with a specified key in a const map. */
	class TConstKeyIterator : public TBaseKeyIterator<true>
	{
	private:
		using Super = TBaseKeyIterator<true>;
		using IteratorType = typename ElementSetType::TConstKeyIterator;

	public:
		using KeyArgumentType = typename IteratorType::KeyArgumentType;

		FORCEINLINE TConstKeyIterator(const TMapBase& InMap, KeyArgumentType InKey)
			: Super(IteratorType(InMap.Pairs, InKey))
		{
		}
	};

	/** Iterates over values associated with a specified key in a map. */
	class TKeyIterator : public TBaseKeyIterator<false>
	{
	private:
		using Super = TBaseKeyIterator<false>;
		using IteratorType = typename ElementSetType::TKeyIterator;

	public:
		using KeyArgumentType = typename IteratorType::KeyArgumentType;

		FORCEINLINE TKeyIterator(TMapBase& InMap, KeyArgumentType InKey)
			: Super(IteratorType(InMap.Pairs, InKey))
		{
		}

		/** Removes the current key-value pair from the map. */
		FORCEINLINE void RemoveCurrent()
		{
			TBaseKeyIterator<false>::SetIt.RemoveCurrent();
		}
	};

	/** Creates an iterator over all the pairs in this map */
	FORCEINLINE TIterator CreateIterator()
	{
		return TIterator(*this);
	}

	/** Creates a const iterator over all the pairs in this map */
	FORCEINLINE TConstIterator CreateConstIterator() const
	{
		return TConstIterator(*this);
	}

	/** Creates an iterator over the values associated with a specified key in a map */
	FORCEINLINE TKeyIterator CreateKeyIterator(typename TKeyIterator::KeyArgumentType InKey)
	{
		return TKeyIterator(*this, InKey);
	}

	/** Creates a const iterator over the values associated with a specified key in a map */
	FORCEINLINE TConstKeyIterator CreateConstKeyIterator(typename TConstKeyIterator::KeyArgumentType InKey) const
	{
		return TConstKeyIterator(*this, InKey);
	}

	friend struct TMapPrivateFriend;

public:
	/**
	 * DO NOT USE DIRECTLY
	 * STL-like iterators to enable range-based for loop support.
	 */
	FORCEINLINE TRangedForIterator      begin() { return TRangedForIterator(Pairs.begin()); }
	FORCEINLINE TRangedForConstIterator begin() const { return TRangedForConstIterator(Pairs.begin()); }
	FORCEINLINE TRangedForIterator      end() { return TRangedForIterator(Pairs.end()); }
	FORCEINLINE TRangedForConstIterator end() const { return TRangedForConstIterator(Pairs.end()); }
};

/** The base type of sortable maps. */
template <typename KeyType, typename ValueType, typename SetAllocator, typename KeyFuncs>
class TSortableMapBase : public TMapBase<KeyType, ValueType, SetAllocator, KeyFuncs>
{
protected:
	typedef TMapBase<KeyType, ValueType, SetAllocator, KeyFuncs> Super;

	TSortableMapBase() = default;
	TSortableMapBase(TSortableMapBase&&) = default;
	TSortableMapBase(const TSortableMapBase&) = default;
	TSortableMapBase& operator=(TSortableMapBase&&) = default;
	TSortableMapBase& operator=(const TSortableMapBase&) = default;

	/** Constructor for moving elements from a TMap with a different SetAllocator */
	template<typename OtherSetAllocator>
	TSortableMapBase(TSortableMapBase<KeyType, ValueType, OtherSetAllocator, KeyFuncs>&& Other)
		: Super(MoveTemp(Other))
	{
	}

	/** Constructor for copying elements from a TMap with a different SetAllocator */
	template<typename OtherSetAllocator>
	TSortableMapBase(const TSortableMapBase<KeyType, ValueType, OtherSetAllocator, KeyFuncs>& Other)
		: Super(Other)
	{
	}

	/** Assignment operator for moving elements from a TMap with a different SetAllocator */
	template<typename OtherSetAllocator>
	TSortableMapBase& operator=(TSortableMapBase<KeyType, ValueType, OtherSetAllocator, KeyFuncs>&& Other)
	{
		(Super&)*this = MoveTemp(Other);
		return *this;
	}

	/** Assignment operator for copying elements from a TMap with a different SetAllocator */
	template<typename OtherSetAllocator>
	TSortableMapBase& operator=(const TSortableMapBase<KeyType, ValueType, OtherSetAllocator, KeyFuncs>& Other)
	{
		(Super&)*this = Other;
		return *this;
	}

public:
	/**
	 * Sorts the pairs array using each pair's Key as the sort criteria, then rebuilds the map's hash.
	 * Invoked using "MyMapVar.KeySort( PREDICATE_CLASS() );"
	 */
	template<typename PREDICATE_CLASS>
	FORCEINLINE void KeySort(const PREDICATE_CLASS& Predicate)
	{
		Super::Pairs.Sort(FKeyComparisonClass<PREDICATE_CLASS>(Predicate));
	}

	/**
	 * Stable sorts the pairs array using each pair's Key as the sort criteria, then rebuilds the map's hash.
	 * Invoked using "MyMapVar.KeySort( PREDICATE_CLASS() );"
	 */
	template<typename PREDICATE_CLASS>
	FORCEINLINE void KeyStableSort(const PREDICATE_CLASS& Predicate)
	{
		Super::Pairs.StableSort(FKeyComparisonClass<PREDICATE_CLASS>(Predicate));
	}

	/**
	 * Sorts the pairs array using each pair's Value as the sort criteria, then rebuilds the map's hash.
	 * Invoked using "MyMapVar.ValueSort( PREDICATE_CLASS() );"
	 */
	template<typename PREDICATE_CLASS>
	FORCEINLINE void ValueSort(const PREDICATE_CLASS& Predicate)
	{
		Super::Pairs.Sort(FValueComparisonClass<PREDICATE_CLASS>(Predicate));
	}

	/**
	 * Stable sorts the pairs array using each pair's Value as the sort criteria, then rebuilds the map's hash.
	 * Invoked using "MyMapVar.ValueSort( PREDICATE_CLASS() );"
	 */
	template<typename PREDICATE_CLASS>
	FORCEINLINE void ValueStableSort(const PREDICATE_CLASS& Predicate)
	{
		Super::Pairs.StableSort(FValueComparisonClass<PREDICATE_CLASS>(Predicate));
	}

	/**
	 * Sort the free element list so that subsequent additions will occur in the lowest available
	 * TSet index resulting in tighter packing without moving any existing items. Also useful for
	 * some types of determinism. @see TSparseArray::SortFreeList() for more info.
	 */
	void SortFreeList()
	{
		Super::Pairs.SortFreeList();
	}

private:

	/** Extracts the pair's key from the map's pair structure and passes it to the user provided comparison class. */
	template<typename PREDICATE_CLASS>
	class FKeyComparisonClass
	{
		TDereferenceWrapper< KeyType, PREDICATE_CLASS> Predicate;

	public:

		FORCEINLINE FKeyComparisonClass(const PREDICATE_CLASS& InPredicate)
			: Predicate(InPredicate)
		{}

		FORCEINLINE bool operator()(const typename Super::ElementType& A, const typename Super::ElementType& B) const
		{
			return Predicate(A.Key, B.Key);
		}
	};

	/** Extracts the pair's value from the map's pair structure and passes it to the user provided comparison class. */
	template<typename PREDICATE_CLASS>
	class FValueComparisonClass
	{
		TDereferenceWrapper< ValueType, PREDICATE_CLASS> Predicate;

	public:

		FORCEINLINE FValueComparisonClass(const PREDICATE_CLASS& InPredicate)
			: Predicate(InPredicate)
		{}

		FORCEINLINE bool operator()(const typename Super::ElementType& A, const typename Super::ElementType& B) const
		{
			return Predicate(A.Value, B.Value);
		}
	};
};

/** A TMapBase specialization that only allows a single value associated with each key.*/
template<typename InKeyType, typename InValueType, typename SetAllocator /*= FDefaultSetAllocator*/, typename KeyFuncs /*= TDefaultMapHashableKeyFuncs<KeyType,ValueType,false>*/>
class TMap : public TSortableMapBase<InKeyType, InValueType, SetAllocator, KeyFuncs>
{
	template <typename, typename>
	friend class TScriptMap;

	static_assert(!KeyFuncs::bAllowDuplicateKeys, "TMap cannot be instantiated with a KeyFuncs which allows duplicate keys");

public:
	typedef InKeyType      KeyType;
	typedef InValueType    ValueType;
	typedef SetAllocator   SetAllocatorType;
	typedef KeyFuncs       KeyFuncsTyp

	typedef TSortableMapBase<KeyType, ValueType, SetAllocator, KeyFuncs> Super;
	typedef typename Super::KeyInitType KeyInitType;
	typedef typename Super::KeyConstPointerType KeyConstPointerType;
};