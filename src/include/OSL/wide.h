/*
Copyright (c) 2009-2013 Sony Pictures Imageworks Inc., et al.
All Rights Reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are
met:
* Redistributions of source code must retain the above copyright
  notice, this list of conditions and the following disclaimer.
* Redistributions in binary form must reproduce the above copyright
  notice, this list of conditions and the following disclaimer in the
  documentation and/or other materials provided with the distribution.
* Neither the name of Sony Pictures Imageworks nor the names of its
  contributors may be used to endorse or promote products derived from
  this software without specific prior written permission.
THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
"AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#pragma once

#include <type_traits>

#include "dual_vec.h"

OSL_NAMESPACE_ENTER

// TODO: add conditional compilation to change this
static constexpr int SimdLaneCount = 16;


/// Type for an opaque pointer to whatever the renderer uses to represent a
/// coordinate transformation.
typedef const void * TransformationPtr;

// Simple wrapper to identify a single lane index vs. a mask_value
class Lane
{
	const int m_index;
public:
	explicit OSL_INLINE
	Lane(int index)
	: m_index(index)
	{}

	Lane() = delete;

    OSL_INLINE Lane(const Lane &other)
        : m_index(other.m_index)
    {}

    OSL_INLINE int
	value() const {
    	return m_index;
    }
};

template <int WidthT>
class WideMask
{
public:
    typedef unsigned int value_type;
    static_assert(sizeof(value_type)*8 >= WidthT, "unsupported WidthT");
	static constexpr int width = WidthT; 

    OSL_INLINE WideMask()
    {}

    explicit OSL_INLINE
	WideMask(Lane lane)
    : m_value(1<<lane.value())
    {}

    explicit OSL_INLINE
	WideMask(bool all_on_or_off)
    : m_value((all_on_or_off) ? (0xFFFFFFFF >> (32-WidthT)) : 0)
    {}
    
    explicit OSL_INLINE
	WideMask(value_type value_)
        : m_value(value_)
    {}

    explicit OSL_INLINE
	WideMask(int value_)
        : m_value(static_cast<value_type>(value_))
    {}

    OSL_INLINE WideMask(const WideMask &other)
        : m_value(other.m_value)
    {}

    OSL_INLINE value_type value() const
    { return m_value; }

    // count number of active bits
    OSL_INLINE int count() const {
        value_type m(m_value);
        int count = 0;
        for (count = 0; m != 0; ++count) {
            m &= m-1;
        }
        return count;
    }

    
    OSL_INLINE WideMask invert() const
    {
    	return WideMask((~m_value)&(0xFFFFFFFF >> (32-WidthT)));
    }

    OSL_INLINE WideMask invert(const WideMask &mask) const
    {
        return WideMask(mask.m_value&((~m_value)&(0xFFFFFFFF >> (32-WidthT))));
    }

    
    // Testers
    OSL_INLINE bool operator[](int lane) const
    {
        // From testing code generation this is the preferred form
        return (m_value & (1<<lane))==(1<<lane);
    }

    OSL_INLINE bool is_on(int lane) const
    {
        // From testing code generation this is the preferred form
        return (m_value & (1<<lane))==(1<<lane);
    }

    OSL_INLINE bool is_off(int lane) const
    {
        // From testing code generation this is the preferred form
        return (m_value & (1<<lane))==0;
    }

    OSL_INLINE bool all_on() const
    {
        // TODO:  is this more expensive than == ?
        return (m_value >= (0xFFFFFFFF >> (32-WidthT)));
    }

    OSL_INLINE bool all_off() const
    {
        return (m_value == static_cast<value_type>(0));
    }

    OSL_INLINE bool any_on() const
    {
        return (m_value != static_cast<value_type>(0));
    }

    OSL_INLINE bool any_off() const
    {
        return (m_value < (0xFFFFFFFF >> (32-WidthT)));
    }

    OSL_INLINE bool any_off(const WideMask &mask) const
    {
        return m_value != (m_value & mask.m_value);
    }

    // Setters
    OSL_INLINE void set(int lane, bool flag)
    {
        if (flag) {
            m_value |= (1<<lane);
        } else {
            m_value &= (~(1<<lane));
        }
    }

    OSL_INLINE void set_on(int lane)
    {
        m_value |= (1<<lane);
    }

    OSL_INLINE void set_all_on()
    {
        m_value = (0xFFFFFFFF >> (32-WidthT));
    }

    OSL_INLINE void set_off(int lane)
    {
        m_value &= (~(1<<lane));
    }

    OSL_INLINE void set_all_off()
    {
        m_value = 0;
    }
    
    OSL_INLINE WideMask & 
    operator &=(const WideMask &other)
    {
        m_value = m_value&other.m_value;
        return *this;
    }

    OSL_INLINE WideMask & 
    operator |=(const WideMask &other)
    {
        m_value = m_value|other.m_value;
        return *this;
    }

    OSL_INLINE WideMask  
    operator & (const WideMask &other) const
    {
        return WideMask(m_value&other.m_value);
    }

    OSL_INLINE WideMask  
    operator | (const WideMask &other) const
    {
        return WideMask(m_value|other.m_value);
    }

    OSL_INLINE WideMask  
    operator ~() const
    {
        return invert();
    }
    
private:
    value_type m_value;
};

typedef WideMask<SimdLaneCount> Mask;
// Technically identical to Mask, but intended use is that 
// the implementor may ignore the mask and populate
// all data lanes of the destination object, however
// implementor may still find it usefull to avoid
// pulling/gathering data for that lane.
// Intent is for self documenting code
typedef WideMask<SimdLaneCount> WeakMask;



namespace internal {

template<int... IntegerListT>
struct int_sequence
{
};

template<int StartAtT, int EndBeforeT, typename IntSequenceT>
struct int_sequence_generator;

template<int StartAtT, int EndBeforeT, int... IntegerListT>
struct int_sequence_generator<StartAtT, EndBeforeT, int_sequence<IntegerListT...>>
{
    typedef typename int_sequence_generator<StartAtT+1, EndBeforeT, int_sequence<IntegerListT..., StartAtT>>::type type;
};

template<int EndBeforeT, int... IntegerListT>
struct int_sequence_generator<EndBeforeT, EndBeforeT, int_sequence<IntegerListT...>>
{
    typedef int_sequence<IntegerListT...> type;
};

template<int EndBeforeT, int StartAtT=0>
using make_int_sequence = typename int_sequence_generator<StartAtT, EndBeforeT, int_sequence<> >::type;

// We need the SFINAE type to be different for
// enable_ifType from disable_ifType so that we can apply both to
// the same template signature to avoid
// "error: invalid redeclaration of member function template"
// NOTE: std::enable_if_t is a c++14 library feature, our baseline
// and we wish to remain compatible with c++11 header libraries
template <bool TestT, typename TypeT = std::true_type>
using enable_if_type = typename std::enable_if<TestT, TypeT>::type;

} // namespace internal



template <typename DataT, int WidthT = SimdLaneCount>
struct Wide; // undefined


template <typename BuiltinT, int WidthT>
struct WideBuiltin
{
	typedef BuiltinT value_type;
	static constexpr int width = WidthT; 
	
	value_type data[WidthT];
	
	OSL_INLINE void
	set(int index, value_type value) 
	{
		data[index] = value;
	}

	OSL_INLINE void
	set_all(value_type value) 
	{
		OSL_INTEL_PRAGMA("forceinline recursive")
		{
			OSL_INTEL_PRAGMA("omp simd simdlen(WidthT)")								
			for(int i = 0; i < WidthT; ++i)
			{
					data[i] = value;
			}
		}
	}
	
	OSL_INLINE void 
	blendin(WideMask<WidthT> mask, const WideBuiltin & other) 
	{
		OSL_INTEL_PRAGMA("forceinline recursive")
		{
			OSL_INTEL_PRAGMA("omp simd simdlen(WidthT)")								
			for(int i = 0; i < WidthT; ++i)
			{
				if (mask[i]) {
					data[i] = other.get(i);
				}
			}
		}
	}

	OSL_INLINE void 
	blendin(WideMask<WidthT> mask, value_type value) 
	{
		OSL_INTEL_PRAGMA("forceinline recursive")
		{
			OSL_INTEL_PRAGMA("omp simd simdlen(WidthT)")								
			for(int i = 0; i < WidthT; ++i)
			{
				if (mask[i]) {
					data[i] = value;
				}
			}
		}
	}
	
protected:
	template<int HeadIndexT>
	OSL_INLINE void 
	set(internal::int_sequence<HeadIndexT>, const value_type & value)
	{
		set(HeadIndexT, value);
	}

	template<int HeadIndexT, int... TailIndexListT, typename... BuiltinListT>
	OSL_INLINE void 
	set(internal::int_sequence<HeadIndexT, TailIndexListT...>, value_type headValue, BuiltinListT... tailValues)
	{
		set(HeadIndexT, headValue);
		set(internal::int_sequence<TailIndexListT...>(), tailValues...);
		return;
	}
public:
	
	OSL_INLINE WideBuiltin() = default;

	template<typename... BuiltinListT, typename = internal::enable_if_type<(sizeof...(BuiltinListT) == WidthT)> >
	explicit OSL_INLINE
	WideBuiltin(const BuiltinListT &...values)
	{
		typedef internal::make_int_sequence<sizeof...(BuiltinListT)> int_seq_type;
		set(int_seq_type(), values...);
		return;
	}
	
	OSL_INLINE explicit  
	WideBuiltin(const value_type & uniformValue) 
	{
		OSL_INTEL_PRAGMA("forceinline recursive")
		{
			OSL_INTEL_PRAGMA("omp simd simdlen(WidthT)")								
			for(int i = 0; i < WidthT; ++i)
			{
				data[i] = uniformValue;
			}
		}
	}
	
	
	
	OSL_INLINE BuiltinT 
	get(int index) const 
	{
		return data[index];
	}	
	
	void dump(const char *name) const
	{
		if (name != nullptr) {
			std::cout << name << " = ";
		}
		std::cout << "{";				
		for(int i=0; i < WidthT; ++i)
		{
			std::cout << data[i];
			if (i < (WidthT-1))
				std::cout << ",";
				
		}
		std::cout << "}" << std::endl;
	}
};





// Specializations
template <int WidthT>
struct Wide<float, WidthT> : public WideBuiltin<float, WidthT> {};

template <int WidthT>
struct Wide<int, WidthT> : public WideBuiltin<int, WidthT> {};

template <int WidthT>
struct Wide<TransformationPtr, WidthT> : public WideBuiltin<TransformationPtr, WidthT> {};


// Vec4 isn't used by external interfaces, but some internal
// noise functions utilize a wide version of it.
typedef Imath::Vec4<Float>     Vec4;

template <int WidthT>
struct Wide<Vec4, WidthT>
{
	typedef Vec4 value_type;
	static constexpr int width = WidthT;
	float x[WidthT];
	float y[WidthT];
	float z[WidthT];
	float w[WidthT];

	OSL_INLINE void
	set(int index, const Vec4 & value)
	{
		x[index] = value.x;
		y[index] = value.y;
		z[index] = value.z;
		w[index] = value.w;
	}

	OSL_INLINE void
	blendin(WideMask<WidthT> mask, const Wide & other)
	{
		OSL_INTEL_PRAGMA("forceinline recursive")
		{
			OSL_INTEL_PRAGMA("omp simd simdlen(WidthT)")
			for(int i = 0; i < WidthT; ++i)
			{
				if (mask[i]) {
					x[i] = other.x.get(i);
					y[i] = other.y.get(i);
					z[i] = other.z.get(i);
					w[i] = other.w.get(i);
				}
			}
		}
	}

	OSL_INLINE void
	blendin(WideMask<WidthT> mask, const value_type & value)
	{
		OSL_INTEL_PRAGMA("forceinline recursive")
		{
			OSL_INTEL_PRAGMA("omp simd simdlen(WidthT)")
			for(int i = 0; i < WidthT; ++i)
			{
				if (mask[i]) {
					x[i] = value.x;
					y[i] = value.y;
					z[i] = value.z;
					w[i] = value.w;
				}
			}
		}
	}


protected:
	template<int HeadIndexT>
	OSL_INLINE void
	set(internal::int_sequence<HeadIndexT>, const Vec4 & value)
	{
		set(HeadIndexT, value);
	}

	template<int HeadIndexT, int... TailIndexListT, typename... Vec4ListT>
	OSL_INLINE void
	set(internal::int_sequence<HeadIndexT, TailIndexListT...>, Vec4 headValue, Vec4ListT... tailValues)
	{
		set(HeadIndexT, headValue);
		set(internal::int_sequence<TailIndexListT...>(), tailValues...);
		return;
	}
public:

	OSL_INLINE Wide() = default;
	// We want to avoid accidentially copying these when the intent was to just have
	// a reference
	Wide(const Wide &other) = delete;

	template<typename... Vec4ListT, typename = internal::enable_if_type<(sizeof...(Vec4ListT) == WidthT)> >
	explicit OSL_INLINE
	Wide(const Vec4ListT &...values)
	{
		typedef internal::make_int_sequence<sizeof...(Vec4ListT)> int_seq_type;
		set(int_seq_type(), values...);
		return;
	}


	OSL_INLINE Vec4
	get(int index) const
	{
		return Vec4(x[index], y[index], z[index], w[index]);
	}

	void dump(const char *name) const
	{
		if (name != nullptr) {
			std::cout << name << " = ";
		}
		std::cout << "x{";
		for(int i=0; i < WidthT; ++i)
		{
			std::cout << x[i];
			if (i < (WidthT-1))
				std::cout << ",";

		}
		std::cout << "}" << std::endl;
		std::cout << "y{";
		for(int i=0; i < WidthT; ++i)
		{
			std::cout << y[i];
			if (i < (WidthT-1))
				std::cout << ",";

		}
		std::cout << "}" << std::endl;
		std::cout << "z{"	;
		for(int i=0; i < WidthT; ++i)
		{
			std::cout << z[i];
			if (i < (WidthT-1))
				std::cout << ",";

		}
		std::cout << "}" << std::endl;
		std::cout << "w{"	;
		for(int i=0; i < WidthT; ++i)
		{
			std::cout << w[i];
			if (i < (WidthT-1))
				std::cout << ",";

		}
		std::cout << "}" << std::endl;
	}

};

template <int WidthT>
struct Wide<Vec3, WidthT>
{	
	typedef Vec3 value_type;
	static constexpr int width = WidthT; 
	float x[WidthT];
	float y[WidthT];
	float z[WidthT];
	
	OSL_INLINE void 
	set(int index, const Vec3 & value) 
	{
		x[index] = value.x;
		y[index] = value.y;
		z[index] = value.z;
	}
	
	OSL_INLINE void 
	blendin(WideMask<WidthT> mask, const Wide & other) 
	{
		OSL_INTEL_PRAGMA("forceinline recursive")
		{
			OSL_INTEL_PRAGMA("omp simd simdlen(WidthT)")								
			for(int i = 0; i < WidthT; ++i)
			{
				if (mask[i]) {
					x[i] = other.x.get(i);
					y[i] = other.y.get(i);
					z[i] = other.z.get(i);
				}
			}
		}
	}
	
	OSL_INLINE void 
	blendin(WideMask<WidthT> mask, const value_type & value) 
	{
		OSL_INTEL_PRAGMA("forceinline recursive")
		{
			OSL_INTEL_PRAGMA("omp simd simdlen(WidthT)")								
			for(int i = 0; i < WidthT; ++i)
			{
				if (mask[i]) {
					x[i] = value.x;
					y[i] = value.y;
					z[i] = value.z;
				}
			}
		}
	}
	
	
protected:
	template<int HeadIndexT>
	OSL_INLINE void 
	set(internal::int_sequence<HeadIndexT>, const Vec3 & value)
	{
		set(HeadIndexT, value);
	}

	template<int HeadIndexT, int... TailIndexListT, typename... Vec3ListT>
	OSL_INLINE void 
	set(internal::int_sequence<HeadIndexT, TailIndexListT...>, Vec3 headValue, Vec3ListT... tailValues)
	{
		set(HeadIndexT, headValue);
		set(internal::int_sequence<TailIndexListT...>(), tailValues...);
		return;
	}
public:
	
	OSL_INLINE Wide() = default;
	// We want to avoid accidentially copying these when the intent was to just have
	// a reference
	Wide(const Wide &other) = delete;

	template<typename... Vec3ListT, typename = internal::enable_if_type<(sizeof...(Vec3ListT) == WidthT)> >
	explicit OSL_INLINE
	Wide(const Vec3ListT &...values)
	{
		typedef internal::make_int_sequence<sizeof...(Vec3ListT)> int_seq_type;
		set(int_seq_type(), values...);
		return;
	}
	

	OSL_INLINE Vec3 
	get(int index) const 
	{
		return Vec3(x[index], y[index], z[index]);
	}		
	
	void dump(const char *name) const
	{
		if (name != nullptr) {
			std::cout << name << " = ";
		}
		std::cout << "x{";				
		for(int i=0; i < WidthT; ++i)
		{
			std::cout << x[i];
			if (i < (WidthT-1))
				std::cout << ",";
				
		}
		std::cout << "}" << std::endl;
		std::cout << "y{";				
		for(int i=0; i < WidthT; ++i)
		{
			std::cout << y[i];
			if (i < (WidthT-1))
				std::cout << ",";
				
		}
		std::cout << "}" << std::endl;
		std::cout << "z{"	;			
		for(int i=0; i < WidthT; ++i)
		{
			std::cout << z[i];
			if (i < (WidthT-1))
				std::cout << ",";
				
		}
		std::cout << "}" << std::endl;
	}
	
};

template <int WidthT>
struct Wide<Vec2, WidthT>
{
    typedef Vec2 value_type;
    static constexpr int width = WidthT;
    float x[WidthT];
    float y[WidthT];

    OSL_INLINE void
    set(int index, const Vec2 & value)
    {
        x[index] = value.x;
        y[index] = value.y;
    }

    OSL_INLINE void
    blendin(WideMask<WidthT> mask, const Wide & other)
    {
        OSL_INTEL_PRAGMA("forceinline recursive")
        {
            OSL_INTEL_PRAGMA("omp simd simdlen(WidthT)")
            for(int i = 0; i < WidthT; ++i)
            {
                if (mask[i]) {
                    x[i] = other.x.get(i);
                    y[i] = other.y.get(i);
                }
            }
        }
    }

    OSL_INLINE void
    blendin(WideMask<WidthT> mask, const value_type & value)
    {
        OSL_INTEL_PRAGMA("forceinline recursive")
        {
            OSL_INTEL_PRAGMA("omp simd simdlen(WidthT)")
            for(int i = 0; i < WidthT; ++i)
            {
                if (mask[i]) {
                    x[i] = value.x;
                    y[i] = value.y;
                }
            }
        }
    }


protected:
    template<int HeadIndexT>
    OSL_INLINE void
    set(internal::int_sequence<HeadIndexT>, const Vec2 & value)
    {
        set(HeadIndexT, value);
    }

    template<int HeadIndexT, int... TailIndexListT, typename... Vec2ListT>
    OSL_INLINE void
    set(internal::int_sequence<HeadIndexT, TailIndexListT...>, Vec2 headValue, Vec2ListT... tailValues)
    {
        set(HeadIndexT, headValue);
        set(internal::int_sequence<TailIndexListT...>(), tailValues...);
        return;
    }
public:

    OSL_INLINE Wide() = default;
    // We want to avoid accidentially copying these when the intent was to just have
    // a reference
    Wide(const Wide &other) = delete;

    template<typename... Vec2ListT, typename = internal::enable_if_type<(sizeof...(Vec2ListT) == WidthT)> >
    explicit OSL_INLINE
    Wide(const Vec2ListT &...values)
    {
        typedef internal::make_int_sequence<sizeof...(Vec2ListT)> int_seq_type;
        set(int_seq_type(), values...);
        return;
    }


    OSL_INLINE Vec2
    get(int index) const
    {
        return Vec2(x[index], y[index]);
    }

    void dump(const char *name) const
    {
        if (name != nullptr) {
            std::cout << name << " = ";
        }
        std::cout << "x{";
        for(int i=0; i < WidthT; ++i)
        {
            std::cout << x[i];
            if (i < (WidthT-1))
                std::cout << ",";

        }
        std::cout << "}" << std::endl;
        std::cout << "y{";
        for(int i=0; i < WidthT; ++i)
        {
            std::cout << y[i];
            if (i < (WidthT-1))
                std::cout << ",";

        }
        std::cout << "}" << std::endl;
    }

};

template <int WidthT>
struct Wide<Color3, WidthT>
{
	typedef Color3 value_type;	
    static constexpr int width = WidthT;
    float x[WidthT];
    float y[WidthT];
    float z[WidthT];

    OSL_INLINE void
    set(int index, const Color3 & value)
    {
        x[index] = value.x;
        y[index] = value.y;
        z[index] = value.z;
    }

	OSL_INLINE void 
	blendin(WideMask<WidthT> mask, const Wide & other) 
	{
		OSL_INTEL_PRAGMA("forceinline recursive")
		{
			OSL_INTEL_PRAGMA("omp simd simdlen(WidthT)")								
			for(int i = 0; i < WidthT; ++i)
			{
				if (mask[i]) {
					x[i] = other.x.get(i);
					y[i] = other.y.get(i);
					z[i] = other.z.get(i);
				}
			}
		}
	}
    
	OSL_INLINE void 
	blendin(WideMask<WidthT> mask, const value_type & value) 
	{
		OSL_INTEL_PRAGMA("forceinline recursive")
		{
			OSL_INTEL_PRAGMA("omp simd simdlen(WidthT)")								
			for(int i = 0; i < WidthT; ++i)
			{
				if (mask[i]) {
					x[i] = value.x;
					y[i] = value.y;
					z[i] = value.z;
				}
			}
		}
	}
	
protected:
    template<int HeadIndexT>
    OSL_INLINE void
    set(internal::int_sequence<HeadIndexT>, const Color3 & value)
    {
        set(HeadIndexT, value);
    }

    template<int HeadIndexT, int... TailIndexListT, typename... Color3ListT>
    OSL_INLINE void
    set(internal::int_sequence<HeadIndexT, TailIndexListT...>, Color3 headValue, Color3ListT... tailValues)
    {
        set(HeadIndexT, headValue);
        set(internal::int_sequence<TailIndexListT...>(), tailValues...);
        return;
    }
public:

    OSL_INLINE Wide() = default;
    Wide(const Wide &other) = delete;

    template<typename... Color3ListT, typename = internal::enable_if_type<(sizeof...(Color3ListT) == WidthT)> >
    explicit OSL_INLINE
    Wide(const Color3ListT &...values)
    {
        typedef internal::make_int_sequence<sizeof...(Color3ListT)> int_seq_type;
        set(int_seq_type(), values...);
        return;
    }


    OSL_INLINE Color3
    get(int index) const
    {
        return Color3(x[index], y[index], z[index]);
    }

    void dump(const char *name) const
    {
        if (name != nullptr) {
            std::cout << name << " = ";
        }
        std::cout << "x{";
        for(int i=0; i < WidthT; ++i)
        {
            std::cout << x[i];
            if (i < (WidthT-1))
                std::cout << ",";

        }
        std::cout << "}" << std::endl;
        std::cout << "y{";
        for(int i=0; i < WidthT; ++i)
        {
            std::cout << y[i];
            if (i < (WidthT-1))
                std::cout << ",";

        }
        std::cout << "}" << std::endl;
        std::cout << "z{"	;
        for(int i=0; i < WidthT; ++i)
        {
            std::cout << z[i];
            if (i < (WidthT-1))
                std::cout << ",";

        }
        std::cout << "}" << std::endl;
    }

};

template <int WidthT>
struct Wide<Matrix44, WidthT>
{	
	typedef Matrix44 value_type;
	static constexpr int width = WidthT; 
	Wide<float, WidthT> x[4][4];
	
	OSL_INLINE void 
	set(int index, const Matrix44 & value) 
	{
		x[0][0].set(index, value.x[0][0]);
		x[0][1].set(index, value.x[0][1]);
		x[0][2].set(index, value.x[0][2]);
		x[0][3].set(index, value.x[0][3]);
		x[1][0].set(index, value.x[1][0]);
		x[1][1].set(index, value.x[1][1]);
		x[1][2].set(index, value.x[1][2]);
		x[1][3].set(index, value.x[1][3]);
		x[2][0].set(index, value.x[2][0]);
		x[2][1].set(index, value.x[2][1]);
		x[2][2].set(index, value.x[2][2]);
		x[2][3].set(index, value.x[2][3]);
		x[3][0].set(index, value.x[3][0]);
		x[3][1].set(index, value.x[3][1]);
		x[3][2].set(index, value.x[3][2]);
		x[3][3].set(index, value.x[3][3]);
	}

	OSL_INLINE void 
	blendin(WideMask<WidthT> mask, const Wide & other) 
	{
		OSL_INTEL_PRAGMA("forceinline recursive")
		{
			OSL_INTEL_PRAGMA("omp simd simdlen(WidthT)")								
			for(int i = 0; i < WidthT; ++i)
			{
				if (mask[i]) {
					x[0][0].set(i, other.x[0][0].get(i));
					x[0][1].set(i, other.x[0][1].get(i));
					x[0][2].set(i, other.x[0][2].get(i));
					x[0][3].set(i, other.x[0][3].get(i));
					x[1][0].set(i, other.x[1][0].get(i));
					x[1][1].set(i, other.x[1][1].get(i));
					x[1][2].set(i, other.x[1][2].get(i));
					x[1][3].set(i, other.x[1][3].get(i));
					x[2][0].set(i, other.x[2][0].get(i));
					x[2][1].set(i, other.x[2][1].get(i));
					x[2][2].set(i, other.x[2][2].get(i));
					x[2][3].set(i, other.x[2][3].get(i));
					x[3][0].set(i, other.x[3][0].get(i));
					x[3][1].set(i, other.x[3][1].get(i));
					x[3][2].set(i, other.x[3][2].get(i));
					x[3][3].set(i, other.x[3][3].get(i));
				}
			}
		}
	}
	
	OSL_INLINE void 
	blendin(WideMask<WidthT> mask, const value_type & value) 
	{
		OSL_INTEL_PRAGMA("forceinline recursive")
		{
			OSL_INTEL_PRAGMA("omp simd simdlen(WidthT)")								
			for(int i = 0; i < WidthT; ++i)
			{
				if (mask[i]) {
					x[0][0].set(i, value.x[0][0]);
					x[0][1].set(i, value.x[0][1]);
					x[0][2].set(i, value.x[0][2]);
					x[0][3].set(i, value.x[0][3]);
					x[1][0].set(i, value.x[1][0]);
					x[1][1].set(i, value.x[1][1]);
					x[1][2].set(i, value.x[1][2]);
					x[1][3].set(i, value.x[1][3]);
					x[2][0].set(i, value.x[2][0]);
					x[2][1].set(i, value.x[2][1]);
					x[2][2].set(i, value.x[2][2]);
					x[2][3].set(i, value.x[2][3]);
					x[3][0].set(i, value.x[3][0]);
					x[3][1].set(i, value.x[3][1]);
					x[3][2].set(i, value.x[3][2]);
					x[3][3].set(i, value.x[3][3]);
				}
			}
		}
	}
	
	
	OSL_INLINE Matrix44 
	get(int index) const 
	{
		return Matrix44(
			x[0][0].get(index), x[0][1].get(index), x[0][2].get(index), x[0][3].get(index),
			x[1][0].get(index), x[1][1].get(index), x[1][2].get(index), x[1][3].get(index),
			x[2][0].get(index), x[2][1].get(index), x[2][2].get(index), x[2][3].get(index),
			x[3][0].get(index), x[3][1].get(index), x[3][2].get(index), x[3][3].get(index));
	}		
};

template <int WidthT>
struct Wide<ustring, WidthT>
{	
	static constexpr int width = WidthT; 
    ustring str[WidthT];
    static_assert(sizeof(ustring) == sizeof(char*), "ustring must be pointer size");
	
	OSL_INLINE void 
	set(int index, const ustring& value) 
	{
        str[index] = value;
	}

	OSL_INLINE ustring 
	get(int index) const 
	{
        return str[index];
	}		
};

template <int WidthT>
struct Wide<Dual2<float>, WidthT>
{	
	typedef Dual2<float> value_type;
	static constexpr int width = WidthT; 
	float x[WidthT];
	float dx[WidthT];
	float dy[WidthT];
	
	OSL_INLINE void 
	set(int index, const value_type & value) 
	{
		x[index] = value.val();
		dx[index] = value.dx();
		dy[index] = value.dy();
	}
	
	OSL_INLINE void 
	blendin(WideMask<WidthT> mask, const Wide & other) 
	{
		OSL_INTEL_PRAGMA("forceinline recursive")
		{
			OSL_INTEL_PRAGMA("omp simd simdlen(WidthT)")								
			for(int i = 0; i < WidthT; ++i)
			{
				if (mask[i]) {
					x[i] = other.x.get(i);
					dx[i] = other.dx.get(i);
					dy[i] = other.dy.get(i);
				}
			}
		}
	}

	OSL_INLINE void 
	blendin(WideMask<WidthT> mask, const value_type & value) 
	{
		OSL_INTEL_PRAGMA("forceinline recursive")
		{
			OSL_INTEL_PRAGMA("omp simd simdlen(WidthT)")								
			for(int i = 0; i < WidthT; ++i)
			{
				if (mask[i]) {
					x[i] = value.x;
					dx[i] = value.dx;
					dy[i] = value.dy;
				}
			}
		}
	}
	
protected:
	template<int HeadIndexT>
	OSL_INLINE void 
	set(internal::int_sequence<HeadIndexT>, const value_type &value)
	{
		set(HeadIndexT, value);
	}

	template<int HeadIndexT, int... TailIndexListT, typename... ValueListT>
	OSL_INLINE void 
	set(internal::int_sequence<HeadIndexT, TailIndexListT...>, value_type headValue, ValueListT... tailValues)
	{
		set(HeadIndexT, headValue);
		set(internal::int_sequence<TailIndexListT...>(), tailValues...);
		return;
	}
public:
	
	// TODO:  should other wide types delete their copy constructors?
	OSL_INLINE Wide() = default;
	Wide(const Wide &other) = delete;

	template<typename... ValueListT, typename = internal::enable_if_type<(sizeof...(ValueListT) == WidthT)> >
	explicit OSL_INLINE
	Wide(const ValueListT &...values)
	{
		typedef internal::make_int_sequence<sizeof...(ValueListT)> int_seq_type;
		set(int_seq_type(), values...);
		return;
	}
	

	OSL_INLINE value_type 
	get(int index) const 
	{
		return value_type(x[index], dx[index], dy[index]);
	}		
};

template <int WidthT>
struct Wide<Dual2<Vec3>, WidthT>
{	
	typedef Dual2<Vec3> value_type;
	static constexpr int width = WidthT; 
	Wide<Vec3> x;
	Wide<Vec3> dx;
	Wide<Vec3> dy;
	
	OSL_INLINE void 
	set(int index, const value_type & value) 
	{
		x.set(index, value.val());
		dx.set(index, value.dx());
		dy.set(index, value.dy());
	}
	
	OSL_INLINE void 
	blendin(WideMask<WidthT> mask, const Wide & other) 
	{
		OSL_INTEL_PRAGMA("forceinline recursive")
		{
			OSL_INTEL_PRAGMA("omp simd simdlen(WidthT)")								
			for(int i = 0; i < WidthT; ++i)
			{
				if (mask[i]) {
					x.set(i, other.x.get(i);
					dx.set(i, other.dx.get(i);
					dy.set(i, other.dy.get(i);
				}
			}
		}
	}
	
	OSL_INLINE void 
	blendin(WideMask<WidthT> mask, const value_type & value) 
	{
		OSL_INTEL_PRAGMA("forceinline recursive")
		{
			OSL_INTEL_PRAGMA("omp simd simdlen(WidthT)")								
			for(int i = 0; i < WidthT; ++i)
			{
				if (mask[i]) {
					x.set(i, value.x;
					dx.set(i, value.dx;
					dy.set(i, value.dy;
				}
			}
		}
	}
	
	
protected:
	template<int HeadIndexT>
	OSL_INLINE void 
	set(internal::int_sequence<HeadIndexT>, const value_type &value)
	{
		set(HeadIndexT, value);
	}

	template<int HeadIndexT, int... TailIndexListT, typename... ValueListT>
	OSL_INLINE void 
	set(internal::int_sequence<HeadIndexT, TailIndexListT...>, value_type headValue, ValueListT... tailValues)
	{
		set(HeadIndexT, headValue);
		set(internal::int_sequence<TailIndexListT...>(), tailValues...);
		return;
	}
public:
	
	OSL_INLINE Wide() = default;
	Wide(const Wide &other) = delete;

	template<typename... ValueListT, typename = internal::enable_if_type<(sizeof...(ValueListT) == WidthT)> >
	explicit OSL_INLINE
	Wide(const ValueListT &...values)
	{
		typedef internal::make_int_sequence<sizeof...(ValueListT)> int_seq_type;
		set(int_seq_type(), values...);
		return;
	}
	

	OSL_INLINE value_type 
	get(int index) const 
	{
		return value_type(x.get(index), dx.get(index), dy.get(index));
	}		
};


template <typename DataT, int WidthT>
struct WideUniformProxy
{
	explicit OSL_INLINE
	WideUniformProxy(Wide<DataT, WidthT> & ref_wide_data)
	: m_ref_wide_data(ref_wide_data)
	{}

	// Must provide user defined copy constructor to 
	// get compiler to be able to follow individual 
	// data members through back to original object
	// when fully inlined the proxy should disappear
	OSL_INLINE
	WideUniformProxy(const WideUniformProxy &other)
	: m_ref_wide_data(other.m_ref_wide_data)
	{}
	
	// Sets all data lanes of wide to the value
	OSL_INLINE const DataT & 
	operator = (const DataT & value)  
	{
		OSL_INTEL_PRAGMA("forceinline recursive")
		{
			OSL_INTEL_PRAGMA("omp simd simdlen(WidthT)")								
			for(int i = 0; i < WidthT; ++i) {
				m_ref_wide_data.set(i, value);
			}
		}
		
		return value;
	}
	
private:
	Wide<DataT, WidthT> & m_ref_wide_data;
};

template <typename DataT, int WidthT>
OSL_INLINE void 
make_uniform(Wide<DataT, WidthT> &wide_data, const DataT &value)
{
	OSL_INTEL_PRAGMA("forceinline recursive")
	{
		OSL_INTEL_PRAGMA("omp simd simdlen(WidthT)")								
		for(int i = 0; i < WidthT; ++i) {
			wide_data.set(i, value);
		}
	}
}




template <typename DataT, int WidthT>
struct LaneProxy
{
	explicit OSL_INLINE
	LaneProxy(Wide<DataT, WidthT> & ref_wide_data, const int index)
	: m_ref_wide_data(ref_wide_data)
	, m_index(index)
	{}

	// Must provide user defined copy constructor to 
	// get compiler to be able to follow individual 
	// data members through back to original object
	// when fully inlined the proxy should disappear
	OSL_INLINE
	LaneProxy(const LaneProxy &other)
	: m_ref_wide_data(other.m_ref_wide_data)
	, m_index(other.m_index)
	{}
	
	OSL_INLINE 
	operator DataT const () const 
	{
		return m_ref_wide_data.get(m_index);
	}

	OSL_INLINE 
	DataT const get() const 
	{
		return m_ref_wide_data.get(m_index);
	}
	
	OSL_INLINE const DataT &
	operator = (const DataT & value)  
	{
		m_ref_wide_data.set(m_index, value);
		return value;
	}
	
	OSL_INLINE WideUniformProxy<DataT, WidthT> 
	uniform() 
	{
		return WideUniformProxy<DataT, WidthT>(m_ref_wide_data); 
	}
	
private:
	Wide<DataT, WidthT> & m_ref_wide_data;
	const int m_index;
};

template <typename DataT, int WidthT>
DataT const 
unproxy(const LaneProxy<DataT,WidthT> &proxy)
{
	return proxy.operator DataT const ();
}


template <typename DataT, int WidthT>
struct ConstLaneProxy
{
	explicit OSL_INLINE
	ConstLaneProxy(const Wide<DataT, WidthT> & ref_wide_data, const int index)
	: m_ref_wide_data(ref_wide_data)
	, m_index(index)
	{}

	// Must provide user defined copy constructor to 
	// get compiler to be able to follow individual 
	// data members through back to original object
	// when fully inlined the proxy should disappear
	OSL_INLINE
	ConstLaneProxy(const ConstLaneProxy &other)
	: m_ref_wide_data(other.m_ref_wide_data)
	, m_index(other.m_index)
	{}	
	
	OSL_INLINE
	operator DataT const () const 
	{
		return m_ref_wide_data.get(m_index);
	}

private:
	const Wide<DataT, WidthT> & m_ref_wide_data;
	const int m_index;
};

template <typename DataT, int WidthT>
DataT const 
unproxy(const ConstLaneProxy<DataT,WidthT> &proxy)
{
	return proxy.operator DataT const ();
}

template <typename DataT, int WidthT = SimdLaneCount>
struct ConstWideAccessor
{
	static constexpr int width = WidthT; 
	
	explicit OSL_INLINE
	ConstWideAccessor(const void *ptr_wide_data, int derivIndex=0)
	: m_ref_wide_data(reinterpret_cast<const Wide<DataT, WidthT> *>(ptr_wide_data)[derivIndex])
	{}
	
	explicit OSL_INLINE
	ConstWideAccessor(const Wide<DataT, WidthT> & ref_wide_data)
	: m_ref_wide_data(ref_wide_data)
	{}
	
	// Must provide user defined copy constructor to 
	// get compiler to be able to follow individual 
	// data members through back to original object
	// when fully inlined the proxy should disappear
	OSL_INLINE
	ConstWideAccessor(const ConstWideAccessor &other)
	: m_ref_wide_data(other.m_ref_wide_data)
	{}	
	
	
	typedef ConstLaneProxy<DataT, WidthT> ConstProxy;
	
	OSL_INLINE ConstProxy const 
	operator[](int index) const
	{
		return ConstProxy(m_ref_wide_data, index);
	}

private:
	const Wide<DataT, WidthT> & m_ref_wide_data;	
};


template <typename DataT, int WidthT = SimdLaneCount>
struct WideAccessor
{
	static constexpr int width = WidthT; 
	
	explicit OSL_INLINE
	WideAccessor(void *ptr_wide_data, int derivIndex=0)
	: m_ref_wide_data(reinterpret_cast<Wide<DataT, WidthT> *>(ptr_wide_data)[derivIndex])
	{}
	
	OSL_INLINE
	WideAccessor(Wide<DataT, WidthT> & ref_wide_data)
	: m_ref_wide_data(ref_wide_data)
	{}
	
	// Must provide user defined copy constructor to 
	// get compiler to be able to follow individual 
	// data members through back to original object
	// when fully inlined the proxy should disappear
	OSL_INLINE
	WideAccessor(const WideAccessor &other)
	: m_ref_wide_data(other.m_ref_wide_data)
	{}	
	
	
	typedef LaneProxy<DataT, WidthT> Proxy;
	typedef ConstLaneProxy<DataT, WidthT> ConstProxy;
	
	OSL_INLINE ConstProxy const 
	operator[](int index) const
	{
		return ConstProxy(m_ref_wide_data, index);
	}

	OSL_INLINE Proxy  
	operator[](int index)
	{
		return Proxy(m_ref_wide_data, index);
	}
	
private:
	Wide<DataT, WidthT> & m_ref_wide_data;	
};


template <typename DataT, int WidthT>
struct MaskedLaneProxy
{
	explicit OSL_INLINE 
	MaskedLaneProxy(Wide<DataT, WidthT> & ref_wide_data, const Mask & mask, const int index)
	: m_ref_wide_data(ref_wide_data)
	, m_mask(mask)
	, m_index(index)
	{}

	// Must provide user defined copy constructor to 
	// get compiler to be able to follow individual 
	// data members through back to original object
	// when fully inlined the proxy should disappear
	OSL_INLINE
	MaskedLaneProxy(const MaskedLaneProxy &other)
	: m_ref_wide_data(other.m_ref_wide_data)
	, m_mask(other.m_mask)
	, m_index(other.m_index)
	{}
	
	OSL_INLINE 
	operator DataT const () const 
	{
		return m_ref_wide_data.get(m_index);
	}

	OSL_INLINE const DataT &
	operator = (const DataT & value)  
	{
		if (m_mask[m_index]) {
			m_ref_wide_data.set(m_index, value);
		}
		return value;
	}
	
	// Although having free helper functions
	// might be cleaner, we choose to expose
	// this functionality here to increase 
	// visibility to end user whose IDE
	// might display these methods vs. free 
	// functions
    OSL_INLINE bool 
    is_on() const
    {
        return m_mask.is_on(m_index);
    }

    OSL_INLINE bool 
    is_off()
    {
        return m_mask.is_off(m_index);
    }
    
	OSL_INLINE 
	DataT const get() const 
	{
		return m_ref_wide_data.get(m_index);
	}
	
private:
	Wide<DataT, WidthT> & m_ref_wide_data;
	const Mask &m_mask;
	const int m_index;
};

template <typename DataT, int ArrayLenT, int WidthT>
struct MaskedArrayLaneProxy
{
	explicit OSL_INLINE 
	MaskedArrayLaneProxy(Wide<DataT, WidthT> * array_of_wide_data, const Mask & mask, const int index)
	: m_array_of_wide_data(array_of_wide_data)
	, m_mask(mask)
	, m_index(index)
	{}

	// Must provide user defined copy constructor to 
	// get compiler to be able to follow individual 
	// data members through back to original object
	// when fully inlined the proxy should disappear
	OSL_INLINE
	MaskedArrayLaneProxy(const MaskedArrayLaneProxy &other)
	: m_array_of_wide_data(other.m_array_of_wide_data)
	, m_mask(other.m_mask)
	, m_index(other.m_index)
	{}
	
	OSL_INLINE 
	MaskedArrayLaneProxy &
	operator = (const DataT (&value) [ArrayLenT] )  
	{
		if (m_mask[m_index]) {
			for(int i=0; i < ArrayLenT; ++i) {
				m_array_of_wide_data[i].set(m_index, value[i]);
			}
		}
		return *this;
	}
	
	// Although having free helper functions
	// might be cleaner, we choose to expose
	// this functionality here to increase 
	// visibility to end user whose IDE
	// might display these methods vs. free 
	// functions
    OSL_INLINE bool 
    is_on() const
    {
        return m_mask.is_on(m_index);
    }

    OSL_INLINE bool 
    is_off()
    {
        return m_mask.is_off(m_index);
    }
 
	OSL_INLINE MaskedLaneProxy<DataT, WidthT> 
	operator[](int array_index) const 
	{
		return MaskedLaneProxy<DataT, WidthT>(m_array_of_wide_data[array_index], m_mask, m_index);
	}
	
	OSL_INLINE void 
	get(DataT (&value) [ArrayLenT]) const 
	{
		for(int i=0; i < ArrayLenT; ++i) {
			value[i] = m_array_of_wide_data[i].get(m_index);
		}
		return;
	}
	
private:
	Wide<DataT, WidthT> * m_array_of_wide_data;
	const Mask &m_mask;
	const int m_index;
};


template <typename DataT, int WidthT>
struct MaskedUnboundedArrayLaneProxy
{
	explicit OSL_INLINE 
	MaskedUnboundedArrayLaneProxy(Wide<DataT, WidthT> * array_of_wide_data, int array_length, const Mask & mask, int index)
	: m_array_of_wide_data(array_of_wide_data)
	, m_array_length(array_length)
	, m_mask(mask)
	, m_index(index)
	{}

	// Must provide user defined copy constructor to 
	// get compiler to be able to follow individual 
	// data members through back to original object
	// when fully inlined the proxy should disappear
	OSL_INLINE
	MaskedUnboundedArrayLaneProxy(const MaskedUnboundedArrayLaneProxy &other)
	: m_array_of_wide_data(other.m_array_of_wide_data)
	, m_array_length(array_length)
	, m_mask(other.m_mask)
	, m_index(other.m_index)
	{}
	
	OSL_INLINE int 
	length() const { return m_array_length; }
	
	// Although having free helper functions
	// might be cleaner, we choose to expose
	// this functionality here to increase 
	// visibility to end user whose IDE
	// might display these methods vs. free 
	// functions
    OSL_INLINE bool 
    is_on() const
    {
        return m_mask.is_on(m_index);
    }

    OSL_INLINE bool 
    is_off()
    {
        return m_mask.is_off(m_index);
    }
 
	OSL_INLINE MaskedLaneProxy<DataT, WidthT> 
	operator[](int array_index) const 
	{
		DASSERT(array_index < m_array_length);
		return MaskedLaneProxy<DataT, WidthT>(m_array_of_wide_data[array_index], m_mask, m_index);
	}
	
	
private:
	Wide<DataT, WidthT> * m_array_of_wide_data;
	int m_array_length;
	const Mask &m_mask;
	const int m_index;
};



template <typename DataT, int WidthT = SimdLaneCount>
struct MaskedAccessor
{
	static constexpr int width = WidthT; 
	
	explicit OSL_INLINE
	MaskedAccessor(void *ptr_wide_data, Mask mask, int derivIndex=0)
	: m_ref_wide_data(reinterpret_cast<Wide<DataT, WidthT> *>(ptr_wide_data)[derivIndex])
	, m_mask(mask)
	{}
	
	explicit OSL_INLINE
	MaskedAccessor(Wide<DataT, WidthT> & ref_wide_data, Mask mask)
	: m_ref_wide_data(ref_wide_data)
	, m_mask(mask)
	{}
	
	// Must provide user defined copy constructor to 
	// get compiler to be able to follow individual 
	// data members through back to original object
	// when fully inlined the proxy should disappear
	OSL_INLINE
	MaskedAccessor(const MaskedAccessor &other)
	: m_ref_wide_data(other.m_ref_wide_data)
	, m_mask(other.m_mask)
	{}	
	
	
	typedef MaskedLaneProxy<DataT, WidthT> Proxy;
	
	OSL_INLINE Proxy  
	operator[](int index) 
	{
		return Proxy(m_ref_wide_data, m_mask, index);
	}

	OSL_INLINE Proxy const  
	operator[](int index) const
	{
		return Proxy(m_ref_wide_data, m_mask, index);
	}
	
private:
	Wide<DataT, WidthT> & m_ref_wide_data;
	Mask m_mask;
};

template <typename DataT, int ArrayLenT, int WidthT>
struct MaskedArrayAccessor
{
	static_assert(ArrayLenT > 0, "OSL logic bug");
	static constexpr int width = WidthT; 
	
	explicit OSL_INLINE
	MaskedArrayAccessor(void *ptr_wide_data, int derivIndex, Mask mask)
	: m_array_of_wide_data(&reinterpret_cast<Wide<DataT, WidthT> *>(ptr_wide_data)[ArrayLenT*derivIndex])
	, m_mask(mask)
	{}
	
	// Must provide user defined copy constructor to 
	// get compiler to be able to follow individual 
	// data members through back to original object
	// when fully inlined the proxy should disappear
	OSL_INLINE
	MaskedArrayAccessor(const MaskedArrayAccessor &other)
	: m_array_of_wide_data(other.m_array_of_wide_data)
	, m_mask(other.m_mask)
	{}	
	
	
	typedef MaskedArrayLaneProxy<DataT, ArrayLenT, WidthT> Proxy;
	
	OSL_INLINE Proxy  
	operator[](int index) 
	{
		return Proxy(m_array_of_wide_data, m_mask, index);
	}

	OSL_INLINE Proxy const  
	operator[](int index) const
	{
		return Proxy(m_array_of_wide_data, m_mask, index);
	}
	
private:
	Wide<DataT, WidthT> * m_array_of_wide_data;
	Mask m_mask;
};

template <typename DataT, int WidthT>
struct MaskedUnboundArrayAccessor
{
	static constexpr int width = WidthT; 
	
	explicit OSL_INLINE
	MaskedUnboundArrayAccessor(void *ptr_wide_data, int derivIndex, int array_length, Mask mask)
	: m_array_of_wide_data(&reinterpret_cast<Wide<DataT, WidthT> *>(ptr_wide_data)[array_length*derivIndex])
	, m_array_length(array_length)
	, m_mask(mask)
	{}
	
	// Must provide user defined copy constructor to 
	// get compiler to be able to follow individual 
	// data members through back to original object
	// when fully inlined the proxy should disappear
	OSL_INLINE
	MaskedUnboundArrayAccessor(const MaskedUnboundArrayAccessor &other)
	: m_array_of_wide_data(other.m_array_of_wide_data)
	, m_array_length(other.m_array_length)
	, m_mask(other.m_mask)
	{}	
	
	
	typedef MaskedUnboundedArrayLaneProxy<DataT, WidthT> Proxy;
	
	OSL_INLINE Proxy  
	operator[](int index) 
	{
		return Proxy(m_array_of_wide_data, m_array_length, m_mask, index);
	}

	OSL_INLINE Proxy const  
	operator[](int index) const
	{
		return Proxy(m_array_of_wide_data, m_array_length, m_mask, index);
	}
	
private:
	Wide<DataT, WidthT> * m_array_of_wide_data;
	int m_array_length;
	Mask m_mask;
};


// End users can add specialize wide for their own types
// and specialize traits to enable them to be used in the proxies
// NOTE: array detection is handled separately
template <typename DataT>
struct WideTraits; // undefined, all types used should be specialized
//{
	//static bool mathes(const TypeDesc &) { return false; }
//};

template <>
struct WideTraits<float> {
	static bool matches(const TypeDesc &type_desc) { 
		return type_desc.basetype == TypeDesc::FLOAT && 
		       type_desc.aggregate == TypeDesc::SCALAR; 
	}
};

template <>
struct WideTraits<int> {
	static bool matches(const TypeDesc &type_desc) { 
		return type_desc.basetype == TypeDesc::INT && 
		       type_desc.aggregate == TypeDesc::SCALAR; 
	}
};

template <>
struct WideTraits<char *> {
	static bool matches(const TypeDesc &type_desc) {
		return type_desc.basetype == TypeDesc::STRING && 
               type_desc.aggregate == TypeDesc::SCALAR; 
	}
};

template <>
struct WideTraits<ustring> {
	static bool matches(const TypeDesc &type_desc) {
		return type_desc.basetype == TypeDesc::STRING && 
               type_desc.aggregate == TypeDesc::SCALAR; 
	}
};

// We let Vec3 match any vector semantics as we don't have a seperate Point or Normal classes
template <>
struct WideTraits<Vec3> {
	static bool matches(const TypeDesc &type_desc) {
		return type_desc.basetype == TypeDesc::FLOAT && 
		    type_desc.aggregate == TypeDesc::VEC3; 
	}
};

template <>
struct WideTraits<Vec2> {
    static bool matches(const TypeDesc &type_desc) {
        return type_desc.basetype == TypeDesc::FLOAT &&
            type_desc.aggregate == TypeDesc::VEC2;
    }
};

template <>
struct WideTraits<Color3> {
	static bool matches(const TypeDesc &type_desc) {
		return type_desc.basetype == TypeDesc::FLOAT && 
		    type_desc.aggregate == TypeDesc::VEC3 &&
			type_desc.vecsemantics == TypeDesc::COLOR; 
	}
};

template <>
struct WideTraits<Matrix33> {
	static bool matches(const TypeDesc &type_desc) {
		return type_desc.basetype == TypeDesc::FLOAT && 
		type_desc.aggregate == TypeDesc::MATRIX33;
	}
};

template <>
struct WideTraits<Matrix44> {
	static bool matches(const TypeDesc &type_desc) {
		return type_desc.basetype == TypeDesc::FLOAT && 
		    type_desc.aggregate == TypeDesc::MATRIX44; }
};



template <int WidthT = SimdLaneCount>
class MaskedData
{
    void *m_ptr;
    TypeDesc m_type;
    Mask m_mask;
    bool m_has_derivs; 
public:

   static constexpr int width = WidthT;

   MaskedData() = delete;
   
   explicit OSL_INLINE
   MaskedData(TypeDesc type, bool has_derivs, Mask mask, void *ptr)
   : m_ptr(ptr)
   , m_type(type)
   , m_mask(mask)
   , m_has_derivs(has_derivs)
   {}
   
	// Must provide user defined copy constructor to 
	// get compiler to be able to follow individual 
	// data members through back to original object
	// when fully inlined the proxy should disappear
   OSL_INLINE MaskedData(const MaskedData &other)
   : m_ptr(other.m_ptr)
   , m_type(other.m_type)
   , m_mask(other.m_mask)
   , m_has_derivs(other.m_has_derivs)
   {}

   OSL_INLINE void *ptr() const { return m_ptr; }
   OSL_INLINE TypeDesc type() const { return m_type; }
   OSL_INLINE bool has_derivs() const { return m_has_derivs; }
   OSL_INLINE Mask mask() const { return m_mask; }
   OSL_INLINE bool valid() const { return m_ptr != nullptr; }

protected:
   
   
   template<typename DataT>
   OSL_INLINE bool 
   is_array_unbounded_selector(std::false_type) {
	   return (m_type.arraylen == std::extent<DataT>::value) && 
			   WideTraits<DataT>::matches(m_type);
   }

   template<typename DataT>
   OSL_INLINE bool 
   is_array_unbounded_selector(std::true_type) {
	   return (m_type.arraylen != 0) 
			  && WideTraits<DataT>::matches(m_type);
   }
   
   
   template<typename DataT>
   OSL_INLINE bool 
   is_array_selector(std::false_type) {
	   return (m_type.arraylen == 0) && 
			   WideTraits<DataT>::matches(m_type);
   }

   template<typename DataT>
   OSL_INLINE bool 
   is_array_selector(std::true_type) {
	   typedef typename std::remove_all_extents<DataT>::type ElementType;
	   return is_array_unbounded_selector<ElementType>(std::integral_constant<bool, (std::extent<DataT>::value == 0)>());
   }   
  
   template<typename DataT, int DerivIndexT>
   OSL_INLINE MaskedAccessor<DataT, WidthT>
   masked_impl(std::false_type /*is array*/) 
   { 
	   DASSERT(is<DataT>());
	   return MaskedAccessor<DataT, WidthT>(m_ptr, m_mask, DerivIndexT);
   }

   template<typename DataT, int DerivIndexT>
   OSL_INLINE MaskedUnboundArrayAccessor<typename std::remove_all_extents<DataT>::type, WidthT>
   masked_array_impl(std::true_type /*is unbounded array*/) 
   { 
	   DASSERT(is<DataT>());
	   return MaskedUnboundArrayAccessor<typename std::remove_all_extents<DataT>::type, WidthT>(m_ptr, DerivIndexT, m_type.arraylen, m_mask);
   }

   template<typename DataT, int DerivIndexT>
   OSL_INLINE MaskedArrayAccessor<typename std::remove_all_extents<DataT>::type, std::extent<DataT>::value, WidthT>
   masked_array_impl(std::false_type /*is unbounded array*/) 
   { 
	   DASSERT(is<DataT>());
	   return MaskedArrayAccessor<typename std::remove_all_extents<DataT>::type, std::extent<DataT>::value, WidthT>(m_ptr, DerivIndexT, m_mask);
   }
   
   template<typename DataT, int DerivIndexT>
   OSL_INLINE decltype(std::declval<MaskedData<WidthT>>().masked_array_impl<DataT, DerivIndexT>(std::integral_constant<bool, (std::extent<DataT>::value == 0)>()))  
   masked_impl(std::true_type /*is array*/) 
   { 
	   DASSERT(is<DataT>());
	   return masked_array_impl<DataT, DerivIndexT>(std::integral_constant<bool, (std::extent<DataT>::value == 0)>());
   }
   
public:
   template<typename DataT>
   OSL_INLINE bool 
   is() {
	   return is_array_selector<DataT>(std::is_array<DataT>::type());
   }
   
   template<typename DataT>
   OSL_INLINE decltype(std::declval<MaskedData<WidthT>>().masked_impl<DataT, 0>(typename std::is_array<DataT>::type()))
   masked() 
   { 
	   return masked_impl<DataT, 0>(typename std::is_array<DataT>::type());
   }

   
   template<typename DataT>
   OSL_INLINE decltype(std::declval<MaskedData<WidthT>>().masked_impl<DataT, 1>(typename std::is_array<DataT>::type()))
   maskedDx() 
   {
	   DASSERT(has_derivs());
	   return masked_impl<DataT, 1>(typename std::is_array<DataT>::type());
   }

   template<typename DataT>
   OSL_INLINE decltype(std::declval<MaskedData<WidthT>>().masked_impl<DataT, 2>(typename std::is_array<DataT>::type()))
   maskedDy() 
   { 
	   DASSERT(has_derivs());
	   return masked_impl<DataT, 2>(typename std::is_array<DataT>::type());
   }
   
   template<typename DataT>
   OSL_INLINE decltype(std::declval<MaskedData<WidthT>>().masked_impl<DataT, 3>(typename std::is_array<DataT>::type()))
   maskedDz() 
   { 
	   DASSERT(has_derivs());
	   return masked_impl<DataT, 3>(typename std::is_array<DataT>::type());
   }   
};

typedef MaskedData<SimdLaneCount> MaskedDataRef;


// The RefProxy pretty much just allows "auto" to be used on the stack to 
// keep a reference vs. a copy of DataT
template <typename DataT>
struct RefProxy
{
	explicit OSL_INLINE 
	RefProxy(DataT & ref_data)
	: m_ref_data(ref_data)
	{}

	// Must provide user defined copy constructor to 
	// get compiler to be able to follow individual 
	// data members through back to original object
	// when fully inlined the proxy should disappear
	OSL_INLINE
	RefProxy(const RefProxy &other)
	: m_ref_data(other.m_ref_data)
	{}

	OSL_INLINE 
	operator DataT & () 
	{
		return m_ref_data;
	}
	
	OSL_INLINE 
	operator DataT const () const 
	{
		return m_ref_data;
	}

	OSL_INLINE const DataT &
	operator = (const DataT & value)  
	{
		m_ref_data = value;
		return value;
	}
private:
	DataT & m_ref_data;
};


template <typename DataT, int ArrayLenT>
struct RefArrayProxy
{
	explicit OSL_INLINE
	RefArrayProxy(DataT (&ref_array_data)[ArrayLenT])
	: m_ref_array_data(ref_array_data)
	{}

	// Must provide user defined copy constructor to 
	// get compiler to be able to follow individual 
	// data members through back to original object
	// when fully inlined the proxy should disappear
	OSL_INLINE
	RefArrayProxy(const RefArrayProxy &other)
	: m_ref_array_data(other.m_ref_array_data)
	{}
	
	OSL_INLINE 
	RefArrayProxy &
	operator = (DataT (&value) [ArrayLenT] )
	{
		for(int i=0; i < ArrayLenT; ++i) {
			m_ref_array_data[i] = value[i];
		}
		return *this;
	}

	typedef DataT (&ArrayRefType)[ArrayLenT];
	
	OSL_INLINE
	operator ArrayRefType()
	{
		return m_ref_array_data;
	}

	
	OSL_INLINE DataT & 
	operator[](int array_index)  
	{
		DASSERT(array_index >= 0 && array_index < ArrayLenT);
		return m_ref_array_data[array_index];
	}

	OSL_INLINE DataT const & 
	operator[](int array_index) const  
	{
		DASSERT(array_index >= 0 && array_index < ArrayLenT);
		return m_ref_array_data[array_index];
	}
	
	OSL_INLINE void
	get(DataT (&value) [ArrayLenT]) const
	{
		for(int i=0; i < ArrayLenT; ++i) {
			value[i] = m_ref_array_data[i];
		}
		return;
	}
	
private:
	DataT (&m_ref_array_data)[ArrayLenT];
};



template <typename DataT>
struct RefUnboundedArrayProxy
{
	explicit OSL_INLINE
	RefUnboundedArrayProxy(DataT *array_data, int array_length)
	: m_array_data(array_data)
	, m_array_length(array_length)
	{}

	// Must provide user defined copy constructor to 
	// get compiler to be able to follow individual 
	// data members through back to original object
	// when fully inlined the proxy should disappear
	OSL_INLINE
	RefUnboundedArrayProxy(const RefUnboundedArrayProxy &other)
	: m_array_data(other.m_array_data)
	, m_array_length(other.m_array_length)
	{}
	
	OSL_INLINE int 
	length() const { return m_array_length; }
	
	OSL_INLINE DataT & 
	operator[](int array_index)  
	{
		DASSERT(array_index >= 0 && array_index < m_array_length);
		return m_array_data[array_index];
	}

	OSL_INLINE DataT const & 
	operator[](int array_index) const  
	{
		DASSERT(array_index >= 0 && array_index < m_array_length);
		return m_array_data[array_index];
	}
	
private:
	DataT * m_array_data;
	int m_array_length;
};



class DataRef
{
    void *m_ptr;
    TypeDesc m_type;
    bool m_has_derivs; 
public:
   DataRef() = delete;
   
   explicit OSL_INLINE
   DataRef(TypeDesc type, bool has_derivs, void *ptr)
   : m_ptr(ptr)
   , m_type(type)
   , m_has_derivs(has_derivs)
   {}
   
	// Must provide user defined copy constructor to 
	// get compiler to be able to follow individual 
	// data members through back to original object
	// when fully inlined the proxy should disappear
   OSL_INLINE DataRef(const DataRef &other)
   : m_ptr(other.m_ptr)
   , m_type(other.m_type)
   , m_has_derivs(other.m_has_derivs)
   {}
    
   OSL_INLINE void *ptr() const { return m_ptr; }
   OSL_INLINE TypeDesc type() const { return m_type; }
   OSL_INLINE bool has_derivs() const { return m_has_derivs; }
   OSL_INLINE bool valid() const { return m_ptr != nullptr; }

protected:
   
   // TODO: see if impl can be shared with MaskedData   
   template<typename DataT>
   OSL_INLINE bool 
   is_array_unbounded_selector(std::false_type) {
	   return (m_type.arraylen == std::extent<DataT>::value) && 
			   WideTraits<DataT>::matches(m_type);
   }

   template<typename DataT>
   OSL_INLINE bool 
   is_array_unbounded_selector(std::true_type) {
	   return (m_type.arraylen != 0) 
			  && WideTraits<DataT>::matches(m_type);
   }
   
   
   template<typename DataT>
   OSL_INLINE bool 
   is_array_selector(std::false_type) {
	   return (m_type.arraylen == 0) && 
			   WideTraits<DataT>::matches(m_type);
   }

   template<typename DataT>
   OSL_INLINE bool 
   is_array_selector(std::true_type) {
	   typedef typename std::remove_all_extents<DataT>::type ElementType;
	   return is_array_unbounded_selector<ElementType>(std::integral_constant<bool, (std::extent<DataT>::value == 0)>());
   }   
   
   
   template<typename DataT, int DerivIndexT>
   OSL_INLINE RefProxy<DataT>
   ref_impl(std::false_type /* is array */) 
   { 
	   DASSERT(is<DataT>());	   
	   return RefProxy<DataT>(reinterpret_cast<DataT *>(m_ptr)[DerivIndexT]);
   }

   template<typename DataT, int DerivIndexT>
   OSL_INLINE RefArrayProxy<typename std::remove_all_extents<DataT>::type, std::extent<DataT>::value> 
   ref_array_impl(std::false_type /* is array unbounded*/) 
   { 
	   DASSERT(is<DataT>());	   
	   // NOTE: DataT is a fixed size array, so the array length is baked into it, 
	   // thus the DerivIndex will step over the whole array, no need to multiply it
	   return RefArrayProxy<typename std::remove_all_extents<DataT>::type, std::extent<DataT>::value>(reinterpret_cast<DataT *>(m_ptr)[DerivIndexT]);
   }

   template<typename DataT, int DerivIndexT>
   OSL_INLINE RefUnboundedArrayProxy<typename std::remove_all_extents<DataT>::type> 
   ref_array_impl(std::true_type /* is array unbounded*/) 
   { 
	   DASSERT(is<DataT>());
	   typedef typename std::remove_all_extents<DataT>::type ElementType;
	   return RefUnboundedArrayProxy<ElementType>(&(reinterpret_cast<ElementType *>(m_ptr)[DerivIndexT*m_type.arraylen]), m_type.arraylen);
   }
   
   template<typename DataT, int DerivIndexT>
   OSL_INLINE decltype(std::declval<DataRef>().ref_array_impl<DataT, DerivIndexT>(std::integral_constant<bool, (std::extent<DataT>::value == 0)>()))  
   ref_impl(std::true_type/* is array */) 
   { 
	   DASSERT(is<DataT>());
	   return ref_array_impl<DataT, DerivIndexT>(std::integral_constant<bool, (std::extent<DataT>::value == 0)>());
   }
   
public:
   template<typename DataT>
   OSL_INLINE bool 
   is() {
	   return is_array_selector<DataT>(std::is_array<DataT>::type());
   }
   
   template<typename DataT>
   OSL_INLINE decltype(std::declval<DataRef>().ref_impl<DataT, 0>(typename std::is_array<DataT>::type()))
   ref() 
   { 
	   return ref_impl<DataT, 0>(typename std::is_array<DataT>::type());
   }

   
   template<typename DataT>
   OSL_INLINE decltype(std::declval<DataRef>().ref_impl<DataT, 1>(typename std::is_array<DataT>::type()))
   refDx() 
   {
	   DASSERT(has_derivs());
	   return ref_impl<DataT, 1>(typename std::is_array<DataT>::type());
   }

   template<typename DataT>
   OSL_INLINE decltype(std::declval<DataRef>().ref_impl<DataT, 2>(typename std::is_array<DataT>::type()))
   refDy() 
   { 
	   DASSERT(has_derivs());
	   return ref_impl<DataT, 2>(typename std::is_array<DataT>::type());
   }
   
   template<typename DataT>
   OSL_INLINE decltype(std::declval<DataRef>().ref_impl<DataT, 3>(typename std::is_array<DataT>::type()))
   refDz() 
   { 
	   DASSERT(has_derivs());
	   return ref_impl<DataT, 3>(typename std::is_array<DataT>::type());
   }   
};

class BatchedTextureOptionProvider
{
public:
    enum Options {
        SWIDTH = 0,         // int | float
        TWIDTH,             // int | float
        RWIDTH,             // int | float
        SBLUR,              // int | float
        TBLUR,              // int | float
        RBLUR,              // int | float
        SWRAP,              // int | string
        TWRAP,              // int | string
        RWRAP,              // int | string
        FILL,               // int | float
        TIME,               // int | float
        FIRSTCHANNEL,       // int
        SUBIMAGE,           // int | string
        INTERP,             // int | string
        MISSINGCOLOR,       // color
        MISSINGALPHA,       // float

        MAX_OPTIONS
    };
    enum DataType {
        INT = 0,
        COLOR = 0,
        FLOAT = 1,
        STRING = 1,
    };

    static constexpr unsigned int maskSize = 32;
    static_assert(MAX_OPTIONS <= maskSize, "expecting MAX_OPTIONS <= maskSize");
    typedef WideMask<maskSize> Mask;

    struct OptionData
    {
        Mask active;
        Mask varying;
        Mask type; // data type is int = 0 or float = 1
        Mask ALIGN; // not used, for 64 bit data alignment
        void* options[MAX_OPTIONS];
    };

private:
    const OptionData * m_opt;
    float m_missingcolor[4];

public:
    explicit
	BatchedTextureOptionProvider(const OptionData * data)
    : m_opt(data)
     ,m_missingcolor{0.f,0.f,0.f,0.f}
    {}

    void updateOption(TextureOpt &opt, unsigned int l)
    {
        // check we actually have valid option data.
        if (m_opt == nullptr) return;

#if 0
        std::cout << "size: " << sizeof(TextureOptions) << std::endl;
        std::cout << "active: " << &m_opt->active << " " << m_opt->active.value() << std::endl;
        std::cout << "varying: " << m_opt->varying.value() << std::endl;
        std::cout << "type: " << m_opt->type.value() << std::endl;
        for (int i = 0; i < m_opt->active.count(); ++i) {
            std::cout << "void* " << m_opt->options[i] << std::endl;
            std::cout << "int " << *(int*)m_opt->options[i] << std::endl;
        }
#endif
        int j = 0; // offset index to next void pointer

#define OPTION_CASE(i, optName)                                                             \
        if (m_opt->active[i]) {                                                                  \
            if (m_opt->varying[i]) {                                                             \
                if (m_opt->type[i] == static_cast<bool>(INT)) {                                  \
                    ConstWideAccessor<int> wideResult(m_opt->options[j]);    \
                    opt.optName = static_cast<float>(wideResult[l]);                    \
                }                                                                           \
                else {                                                                      \
                    ConstWideAccessor<float> wideResult(m_opt->options[j]);    \
                    opt.optName = wideResult[l];                                        \
                }                                                                           \
            }                                                                               \
            else {                                                                          \
                if (m_opt->type[i] == static_cast<bool>(INT)) {                                  \
                    opt.optName = static_cast<float>(*reinterpret_cast<int*>(m_opt->options[j]));\
                }                                                                           \
                else  {                                                                     \
                    opt.optName = *reinterpret_cast<float*>(m_opt->options[j]);                  \
                }                                                                           \
            }                                                                               \
            ++j;                                                                            \
        }

#define OPTION_CASE_DECODE(i, optName, decode, typeCast)                                    \
        if (m_opt->active[i]) {                                                                  \
            if (m_opt->varying[i]) {                                                             \
                if (m_opt->type[i] == static_cast<bool>(STRING)) {                               \
                    ConstWideAccessor<ustring> wideResult(m_opt->options[j]);    \
                    opt.optName = decode(static_cast<const ustring>(wideResult[l]));                                \
                }                                                                           \
                else {                                                                      \
                    ConstWideAccessor<int> wideResult(m_opt->options[j]);    \
                    opt.optName = (typeCast)static_cast<int>(wideResult[l]);                              \
                }                                                                           \
            }                                                                               \
            else {                                                                          \
                if (m_opt->type[i] == static_cast<bool>(STRING)) {                               \
                    ustring& castValue = *reinterpret_cast<ustring*>(m_opt->options[j]);         \
                    opt.optName = decode(castValue);                                        \
                }                                                                           \
                else                                                                        \
                    opt.optName = (typeCast)*reinterpret_cast<int*>(m_opt->options[j]);          \
            }                                                                               \
            ++j;                                                                            \
        }

        // Check all options
        OPTION_CASE(SWIDTH, swidth)
        OPTION_CASE(TWIDTH, twidth)
        OPTION_CASE(RWIDTH, rwidth)
        OPTION_CASE(SBLUR, sblur)
        OPTION_CASE(TBLUR, tblur)
        OPTION_CASE(RBLUR, rblur)
        OPTION_CASE_DECODE(SWRAP, swrap, TextureOpt::decode_wrapmode, TextureOpt::Wrap)
        OPTION_CASE_DECODE(TWRAP, twrap, TextureOpt::decode_wrapmode, TextureOpt::Wrap)
        OPTION_CASE_DECODE(RWRAP, rwrap, TextureOpt::decode_wrapmode, TextureOpt::Wrap)
        OPTION_CASE(FILL, fill)
        OPTION_CASE(TIME, time)
        if (m_opt->active[FIRSTCHANNEL]) {
            if (m_opt->varying[FIRSTCHANNEL]) {
                ConstWideAccessor<int> wideResult(m_opt->options[j]);    \
                opt.firstchannel = wideResult[l];
            }
            else {
                opt.firstchannel = *reinterpret_cast<int*>(m_opt->options[j]);
            }
            ++j;
        }
        if (m_opt->active[SUBIMAGE]) {
            if (m_opt->varying[SUBIMAGE]) {
                if (m_opt->type[SUBIMAGE] == static_cast<bool>(STRING)) {
                    ConstWideAccessor<ustring> wideResult(m_opt->options[j]);    \
                    opt.subimagename = wideResult[l];           \
                }
                else {
                    ConstWideAccessor<int> wideResult(m_opt->options[j]);    \
                    opt.subimage = wideResult[l];
                }
            }
            else {
                if (m_opt->type[SUBIMAGE] == static_cast<bool>(STRING)) {
                    ustring& castValue = *reinterpret_cast<ustring*>(m_opt->options[j]);
                    opt.subimagename = castValue;                   \
                }
                else
                    opt.subimage = *reinterpret_cast<int*>(m_opt->options[j]);
            }
            ++j;
        }
        OPTION_CASE_DECODE(INTERP, interpmode, texInterpToCode, TextureOpt::InterpMode)
        if (m_opt->active[MISSINGCOLOR]) {
            Color3 missingcolor;
            if (m_opt->varying[MISSINGCOLOR]) {
                ConstWideAccessor<Color3> wideResult(m_opt->options[j]);    \
                missingcolor = wideResult[l];
            }
            else {
                missingcolor = *reinterpret_cast<Color3*>(m_opt->options[j]);
            }
            m_missingcolor[0] = missingcolor.x;
            m_missingcolor[1] = missingcolor.y;
            m_missingcolor[2] = missingcolor.z;
            opt.missingcolor = m_missingcolor;
            ++j;
        }
        if (m_opt->active[MISSINGALPHA]) {
            if (m_opt->varying[MISSINGALPHA]) {
                ConstWideAccessor<float> wideResult(m_opt->options[j]);    \
                m_missingcolor[3] = wideResult[l];
            }
            else {
                m_missingcolor[3] = *reinterpret_cast<float*>(m_opt->options[j]);
            }
            opt.missingcolor = m_missingcolor;
            ++j;
        }
#undef OPTION_CASE
#undef OPTION_CASE_DECODE
    }

private:
    // this should be refactored into OIIO texture.h?
    OSL_INLINE TextureOpt::InterpMode texInterpToCode (ustring modename) const
    {
        static ustring u_linear ("linear");
        static ustring u_smartcubic ("smartcubic");
        static ustring u_cubic ("cubic");
        static ustring u_closest ("closest");

        TextureOpt::InterpMode mode = TextureOpt::InterpClosest;
        if (modename == u_smartcubic)
            mode = TextureOpt::InterpSmartBicubic;
        else if (modename == u_linear)
            mode = TextureOpt::InterpBilinear;
        else if (modename == u_cubic)
            mode = TextureOpt::InterpBicubic;
        else if (modename == u_closest)
            mode = TextureOpt::InterpClosest;
        return mode;
    }
};

// Wrapper class to provide outputs resusing existing MaskedDataRef wrapper
// one new method added "bool MaskedDataRef::valid()"
// The wrapper class itself exists to get the 3 different MaskedDataRef classes
// to all sharethe same mask value (after inlining) vs. 3 different copies
// NOTE: detection and access to derivatives for result and alpha can be done
// using methods "has_derivs", "maskedDx()", and "maskedDy()"
// Detection of nchannels shouldn't be necessary, instead check results().is<float>() or results.is<Color3>()
class BatchedTextureOutputs
{
public:
	explicit
	BatchedTextureOutputs(void* result, bool resultHasDerivs, int chans,
                          void* alpha, bool alphaHasDerivs,
                          void* errormessage, Mask mask)
        : m_result(result),
          m_resultHasDerivs(resultHasDerivs),
          m_resultType((chans == 1) ? TypeDesc::TypeFloat : TypeDesc::TypeColor),
          m_alpha(alpha),
          m_alphaHasDerivs(alphaHasDerivs),
          m_errormessage(errormessage),
          m_mask(mask)
    {
        ASSERT(chans == 1 || chans == 3);
    }

    OSL_INLINE Mask mask() const
    {
        return m_mask;
    }

    OSL_INLINE MaskedDataRef result()
    {
        //ASSERT(result().is<float>() || result().is<Color3>());
        //ASSERT(result().has_derivs() == true);
        return MaskedDataRef(m_resultType, m_resultHasDerivs, m_mask, m_result);
    }

    OSL_INLINE MaskedDataRef alpha()
    {
        // ASSERT(alpha().is<float>());
        // ASSERT(alpha().valid() == true || alpha().valid() == false);
        // ASSERT(alpha().has_derivs() == true || alpha().has_derivs() == false);
        return MaskedDataRef(TypeDesc::TypeFloat, m_alphaHasDerivs, m_mask, m_alpha);
    }

    OSL_INLINE MaskedDataRef errormessage()
    {
        // ASSERT(errormessage().is<ustring>());
        // ASSERT(errormessage().valid() == true || errormessage().valid() == false);
        // ASSERT(errormessage().has_derivs() == false);
        return MaskedDataRef(TypeDesc::TypeString, false, m_mask, m_errormessage);
    }

private:
    void* m_result;
    bool m_resultHasDerivs;
    TypeDesc m_resultType;
    void* m_alpha;
    bool m_alphaHasDerivs;
    void* m_errormessage;
    Mask m_mask;
};


OSL_NAMESPACE_EXIT