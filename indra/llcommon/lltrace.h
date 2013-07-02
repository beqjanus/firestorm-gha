/** 
 * @file lltrace.h
 * @brief Runtime statistics accumulation.
 *
 * $LicenseInfo:firstyear=2001&license=viewerlgpl$
 * Second Life Viewer Source Code
 * Copyright (C) 2012, Linden Research, Inc.
 * 
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation;
 * version 2.1 of the License only.
 * 
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 * 
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 * 
 * Linden Research, Inc., 945 Battery Street, San Francisco, CA  94111  USA
 * $/LicenseInfo$
 */

#ifndef LL_LLTRACE_H
#define LL_LLTRACE_H

#include "stdtypes.h"
#include "llpreprocessor.h"

#include "llmemory.h"
#include "llrefcount.h"
#include "lltraceaccumulators.h"
#include "llthreadlocalstorage.h"
#include "lltimer.h"

#include <list>

#define LL_RECORD_BLOCK_TIME(block_timer) LLTrace::TimeBlock::Recorder LL_GLUE_TOKENS(block_time_recorder, __COUNTER__)(block_timer);

namespace LLTrace
{
class Recording;

template<typename T>
T storage_value(T val) { return val; }

template<typename UNIT_TYPE, typename STORAGE_TYPE> 
STORAGE_TYPE storage_value(LLUnit<STORAGE_TYPE, UNIT_TYPE> val) { return val.value(); }

template<typename UNIT_TYPE, typename STORAGE_TYPE> 
STORAGE_TYPE storage_value(LLUnitImplicit<STORAGE_TYPE, UNIT_TYPE> val) { return val.value(); }

class TraceBase
{
public:
	TraceBase(const char* name, const char* description);
	virtual ~TraceBase() {};
	virtual const char* getUnitLabel();

	const std::string& getName() const { return mName; }

protected:
	const std::string	mName;
	const std::string	mDescription;
};

template<typename ACCUMULATOR>
class TraceType 
:	public TraceBase,
	public LLInstanceTracker<TraceType<ACCUMULATOR>, std::string>
{
public:
	TraceType(const char* name, const char* description = NULL)
	:	LLInstanceTracker<TraceType<ACCUMULATOR>, std::string>(name),
		TraceBase(name, description),
		mAccumulatorIndex(AccumulatorBuffer<ACCUMULATOR>::getDefaultBuffer()->reserveSlot())
	{}

	LL_FORCE_INLINE ACCUMULATOR* getPrimaryAccumulator() const
	{
		ACCUMULATOR* accumulator_storage = AccumulatorBuffer<ACCUMULATOR>::getPrimaryStorage();
		return &accumulator_storage[mAccumulatorIndex];
	}

	size_t getIndex() const { return mAccumulatorIndex; }
	static size_t getNumIndices() { return AccumulatorBuffer<ACCUMULATOR>::getNumIndices(); }

private:
	const size_t		mAccumulatorIndex;
};


template<>
class TraceType<TimeBlockAccumulator::CallCountFacet>
:	public TraceType<TimeBlockAccumulator>
{
public:

	TraceType(const char* name, const char* description = "")
	:	TraceType<TimeBlockAccumulator>(name, description)
	{}
};

template<>
class TraceType<TimeBlockAccumulator::SelfTimeFacet>
	:	public TraceType<TimeBlockAccumulator>
{
public:

	TraceType(const char* name, const char* description = "")
		:	TraceType<TimeBlockAccumulator>(name, description)
	{}
};

template <typename T = F64>
class EventStatHandle
:	public TraceType<EventAccumulator>
{
public:
	typedef F64 storage_t;
	typedef TraceType<EventAccumulator> trace_t;

	EventStatHandle(const char* name, const char* description = NULL)
	:	trace_t(name, description)
	{}

	/*virtual*/ const char* getUnitLabel() { return LLGetUnitLabel<T>::getUnitLabel(); }

};

template<typename T, typename VALUE_T>
void record(EventStatHandle<T>& measurement, VALUE_T value)
{
	T converted_value(value);
	measurement.getPrimaryAccumulator()->record(storage_value(converted_value));
}

template <typename T = F64>
class SampleStatHandle
:	public TraceType<SampleAccumulator>
{
public:
	typedef F64 storage_t;
	typedef TraceType<SampleAccumulator> trace_t;

	SampleStatHandle(const char* name, const char* description = NULL)
	:	trace_t(name, description)
	{}

	/*virtual*/ const char* getUnitLabel() { return LLGetUnitLabel<T>::getUnitLabel(); }
};

template<typename T, typename VALUE_T>
void sample(SampleStatHandle<T>& measurement, VALUE_T value)
{
	T converted_value(value);
	measurement.getPrimaryAccumulator()->sample(storage_value(converted_value));
}

template <typename T = F64>
class CountStatHandle
:	public TraceType<CountAccumulator>
{
public:
	typedef F64 storage_t;
	typedef TraceType<CountAccumulator> trace_t;

	CountStatHandle(const char* name, const char* description = NULL) 
	:	trace_t(name)
	{}

	/*virtual*/ const char* getUnitLabel() { return LLGetUnitLabel<T>::getUnitLabel(); }
};

template<typename T, typename VALUE_T>
void add(CountStatHandle<T>& count, VALUE_T value)
{
	T converted_value(value);
	count.getPrimaryAccumulator()->add(storage_value(converted_value));
}

template<>
class TraceType<MemStatAccumulator::AllocationCountFacet>
:	public TraceType<MemStatAccumulator>
{
public:

	TraceType(const char* name, const char* description = "")
	:	TraceType<MemStatAccumulator>(name, description)
	{}
};

template<>
class TraceType<MemStatAccumulator::DeallocationCountFacet>
:	public TraceType<MemStatAccumulator>
{
public:

	TraceType(const char* name, const char* description = "")
	:	TraceType<MemStatAccumulator>(name, description)
	{}
};

template<>
class TraceType<MemStatAccumulator::ChildMemFacet>
	:	public TraceType<MemStatAccumulator>
{
public:

	TraceType(const char* name, const char* description = "")
		:	TraceType<MemStatAccumulator>(name, description)
	{}
};

class MemStatHandle : public TraceType<MemStatAccumulator>
{
public:
	typedef TraceType<MemStatAccumulator> trace_t;
	MemStatHandle(const char* name)
	:	trace_t(name)
	{}

	/*virtual*/ const char* getUnitLabel() { return "B"; }

	TraceType<MemStatAccumulator::AllocationCountFacet>& allocationCount() 
	{ 
		return static_cast<TraceType<MemStatAccumulator::AllocationCountFacet>&>(*(TraceType<MemStatAccumulator>*)this);
	}

	TraceType<MemStatAccumulator::DeallocationCountFacet>& deallocationCount() 
	{ 
		return static_cast<TraceType<MemStatAccumulator::DeallocationCountFacet>&>(*(TraceType<MemStatAccumulator>*)this);
	}

	TraceType<MemStatAccumulator::ChildMemFacet>& childMem() 
	{ 
		return static_cast<TraceType<MemStatAccumulator::ChildMemFacet>&>(*(TraceType<MemStatAccumulator>*)this);
	}
};

// measures effective memory footprint of specified type
// specialize to cover different types

template<typename T>
struct MemFootprint
{
	static size_t measure(const T& value)
	{
		return sizeof(T);
	}

	static size_t measure()
	{
		return sizeof(T);
	}
};

template<typename T>
struct MemFootprint<T*>
{
	static size_t measure(const T* value)
	{
		if (!value)
		{
			return 0;
		}
		return MemFootprint<T>::measure(*value);
	}

	static size_t measure()
	{
		return MemFootprint<T>::measure();
	}
};

template<typename T>
struct MemFootprint<std::basic_string<T> >
{
	static size_t measure(const std::basic_string<T>& value)
	{
		return value.capacity() * sizeof(T);
	}

	static size_t measure()
	{
		return sizeof(std::basic_string<T>);
	}
};

template<typename T>
struct MemFootprint<std::vector<T> >
{
	static size_t measure(const std::vector<T>& value)
	{
		return value.capacity() * MemFootprint<T>::measure();
	}

	static size_t measure()
	{
		return sizeof(std::vector<T>);
	}
};

template<typename T>
struct MemFootprint<std::list<T> >
{
	static size_t measure(const std::list<T>& value)
	{
		return value.size() * (MemFootprint<T>::measure() + sizeof(void*) * 2);
	}

	static size_t measure()
	{
		return sizeof(std::list<T>);
	}
};

template<typename DERIVED, size_t ALIGNMENT = LL_DEFAULT_HEAP_ALIGN>
class MemTrackable
{
	template<typename TRACKED, typename TRACKED_IS_TRACKER>
	struct TrackMemImpl;

	typedef MemTrackable<DERIVED, ALIGNMENT> mem_trackable_t;

public:
	typedef void mem_trackable_tag_t;

	virtual ~MemTrackable()
	{
		memDisclaim(mMemFootprint);
	}

	void* operator new(size_t size) 
	{
		MemStatAccumulator* accumulator = DERIVED::sMemStat.getPrimaryAccumulator();
		if (accumulator)
		{
			accumulator->mSize.sample(accumulator->mSize.getLastValue() + (F64)size);
			accumulator->mAllocatedCount++;
		}

		return ::operator new(size);
	}

	void operator delete(void* ptr, size_t size)
	{
		MemStatAccumulator* accumulator = DERIVED::sMemStat.getPrimaryAccumulator();
		if (accumulator)
		{
			accumulator->mSize.sample(accumulator->mSize.getLastValue() - (F64)size);
			accumulator->mAllocatedCount--;
			accumulator->mDeallocatedCount++;
		}
		::operator delete(ptr);
	}

	void *operator new [](size_t size)
	{
		MemStatAccumulator* accumulator = DERIVED::sMemStat.getPrimaryAccumulator();
		if (accumulator)
		{
			accumulator->mSize.sample(accumulator->mSize.getLastValue() + (F64)size);
			accumulator->mAllocatedCount++;
		}

		return ::operator new[](size);
	}

	void operator delete[](void* ptr, size_t size)
	{
		MemStatAccumulator* accumulator = DERIVED::sMemStat.getPrimaryAccumulator();
		if (accumulator)
		{
			accumulator->mSize.sample(accumulator->mSize.getLastValue() - (F64)size);
			accumulator->mAllocatedCount--;
			accumulator->mDeallocatedCount++;
		}
		::operator delete[](ptr);
	}

	// claim memory associated with other objects/data as our own, adding to our calculated footprint
	template<typename CLAIM_T>
	CLAIM_T& memClaim(CLAIM_T& value)
	{
		TrackMemImpl<CLAIM_T>::claim(*this, value);
		return value;
	}

	template<typename CLAIM_T>
	const CLAIM_T& memClaim(const CLAIM_T& value)
	{
		TrackMemImpl<CLAIM_T>::claim(*this, value);
		return value;
	}


	template<typename AMOUNT_T>
	AMOUNT_T& memClaimAmount(AMOUNT_T& size)
	{
		MemStatAccumulator* accumulator = DERIVED::sMemStat.getPrimaryAccumulator();
		mMemFootprint += (size_t)size;
		if (accumulator)
		{
			accumulator->mSize.sample(accumulator->mSize.getLastValue() + (F64)size);
		}
		return size;
	}

	// remove memory we had claimed from our calculated footprint
	template<typename CLAIM_T>
	CLAIM_T& memDisclaim(CLAIM_T& value)
	{
		TrackMemImpl<CLAIM_T>::disclaim(*this, value);
		return value;
	}

	template<typename CLAIM_T>
	const CLAIM_T& memDisclaim(const CLAIM_T& value)
	{
		TrackMemImpl<CLAIM_T>::disclaim(*this, value);
		return value;
	}

	template<typename AMOUNT_T>
	AMOUNT_T& memDisclaimAmount(AMOUNT_T& size)
	{
		MemStatAccumulator* accumulator = DERIVED::sMemStat.getPrimaryAccumulator();
		if (accumulator)
		{
			accumulator->mSize.sample(accumulator->mSize.getLastValue() - (F64)size);
		}
		return size;
	}

private:
	size_t mMemFootprint;

	template<typename TRACKED, typename TRACKED_IS_TRACKER = void>
	struct TrackMemImpl
	{
		static void claim(mem_trackable_t& tracker, const TRACKED& tracked)
		{
			MemStatAccumulator* accumulator = DERIVED::sMemStat.getPrimaryAccumulator();
			if (accumulator)
			{
				size_t footprint = MemFootprint<TRACKED>::measure(tracked);
				accumulator->mSize.sample(accumulator->mSize.getLastValue() + (F64)footprint);
				tracker.mMemFootprint += footprint;
			}
		}

		static void disclaim(mem_trackable_t& tracker, const TRACKED& tracked)
		{
			MemStatAccumulator* accumulator = DERIVED::sMemStat.getPrimaryAccumulator();
			if (accumulator)
			{
				size_t footprint = MemFootprint<TRACKED>::measure(tracked);
				accumulator->mSize.sample(accumulator->mSize.getLastValue() - (F64)footprint);
				tracker.mMemFootprint -= footprint;
			}
		}
	};

	template<typename TRACKED>
	struct TrackMemImpl<TRACKED, typename TRACKED::mem_trackable_tag_t>
	{
		static void claim(mem_trackable_t& tracker, TRACKED& tracked)
		{
			MemStatAccumulator* accumulator = DERIVED::sMemStat.getPrimaryAccumulator();
			if (accumulator)
			{
				accumulator->mChildSize.sample(accumulator->mChildSize.getLastValue() + (F64)MemFootprint<TRACKED>::measure(tracked));
			}
		}

		static void disclaim(mem_trackable_t& tracker, TRACKED& tracked)
		{
			MemStatAccumulator* accumulator = DERIVED::sMemStat.getPrimaryAccumulator();
			if (accumulator)
			{
				accumulator->mChildSize.sample(accumulator->mChildSize.getLastValue() - (F64)MemFootprint<TRACKED>::measure(tracked));
			}
		}
	};
};

}
#endif // LL_LLTRACE_H
