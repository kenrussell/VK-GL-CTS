#ifndef _VKTPIPELINEUNIQUERANDOMITERATOR_HPP
#define _VKTPIPELINEUNIQUERANDOMITERATOR_HPP
/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2015 The Khronos Group Inc.
 * Copyright (c) 2015 Imagination Technologies Ltd.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and/or associated documentation files (the
 * "Materials"), to deal in the Materials without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Materials, and to
 * permit persons to whom the Materials are furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice(s) and this permission notice shall be included
 * in all copies or substantial portions of the Materials.
 *
 * The Materials are Confidential Information as defined by the
 * Khronos Membership Agreement until designated non-confidential by Khronos,
 * at which point this condition clause shall be removed.
 *
 * THE MATERIALS ARE PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * MATERIALS OR THE USE OR OTHER DEALINGS IN THE MATERIALS.
 *
 *//*!
 * \file
 * \brief Iterator over a unique sequence of items
 *//*--------------------------------------------------------------------*/

#include "tcuDefs.hpp"
#include "deRandom.hpp"
#include <set>
#include <vector>

namespace vkt
{
namespace pipeline
{

template <typename T>
class UniqueRandomIterator
{
public:
							UniqueRandomIterator	(deUint32 numItems, deUint32 numValues, int seed);
	virtual					~UniqueRandomIterator	(void) {}
	bool					hasNext					(void) const;
	T						next					(void);
	void					reset					(void);

protected:
	virtual T				getIndexedValue			(deUint32 index) = 0;

private:
	std::vector<deUint32>	m_indices;
	size_t					m_currentIndex;
};

class RandomFunction
{
private:
	de::Random m_random;

public:
	RandomFunction(int seed)
		: m_random(seed)
	{}

	virtual ~RandomFunction() {}

	deUint32 operator()(deUint32 max)
	{
		return m_random.getUint32() % max;
	}
};

template <typename T>
UniqueRandomIterator<T>::UniqueRandomIterator (deUint32 numItems, deUint32 numValues, int seed)
{
	DE_ASSERT(numItems <= numValues);

	RandomFunction randomFunc(seed);

	if (numItems == numValues)
	{
		// Fast way to populate the index sequence
		m_indices = std::vector<deUint32>(numItems);

		for (deUint32 itemNdx = 0; itemNdx < numItems; itemNdx++)
			m_indices[itemNdx] = itemNdx;
	}
	else
	{
		std::set<deUint32> uniqueIndices;

		// Populate set with "numItems" unique values between 0 and numValues - 1
		while (uniqueIndices.size() < numItems)
			uniqueIndices.insert(randomFunc(numValues));

		// Copy set into index sequence
		m_indices = std::vector<deUint32>(uniqueIndices.begin(), uniqueIndices.end());
	}

	// Scramble the indices
	std::random_shuffle(m_indices.begin(), m_indices.end(), randomFunc);

	reset();
}

template <typename T>
bool UniqueRandomIterator<T>::hasNext (void) const
{
	return m_currentIndex < m_indices.size();
}

template <typename T>
T UniqueRandomIterator<T>::next (void)
{
	DE_ASSERT(m_currentIndex < m_indices.size());

	return getIndexedValue(m_indices[m_currentIndex++]);
}

template <typename T>
void UniqueRandomIterator<T>::reset (void)
{
	m_currentIndex = 0;
}

} // pipeline
} // vkt

#endif // _VKTPIPELINEUNIQUERANDOMITERATOR_HPP