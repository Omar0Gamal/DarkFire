#pragma once
#include "Core/Log/log.h"
#include <vector>
#include <unordered_map>
#include <limits>
#include <cstdint>
#include <iostream>
#include <sstream>
#include <algorithm>
#include <bitset>
#include <memory>
#include <type_traits>
#include "ECS.h"

namespace Core
{
	typedef uint64_t EntityID;

	class ISparseSet {
	public:
		virtual ~ISparseSet() = default;
		virtual void Delete(EntityID) = 0;
		virtual void Clear() = 0;
	};

	template <typename T>
	class SparseSet : public ISparseSet 
	{
	public:
		SparseSet() 
		{
			m_dense.reserve(100);
		}

		T* Set(EntityID id, T obj) 
		{
			size_t index = GetDenseIndex(id);
			if (index != tombstone) 
			{
				m_dense[index] = obj;
				m_denseToEntity[index] = id;

				return &m_dense[index];
			}

			SetDenseIndex(id, m_dense.size());

			m_dense.push_back(obj);
			m_denseToEntity.push_back(id);

			return &m_dense.back();
		}

		T* Get(EntityID id) 
		{
			size_t index = GetDenseIndex(id);
			return (index != tombstone) ? &m_dense[index] : nullptr;
		}

		void Delete(EntityID id) override 
		{
			size_t deletedIndex = GetDenseIndex(id);
			if (!(deletedIndex != tombstone && !m_dense.empty())) 
			{ 
				darklog.warn(L"Trying to delete non-existent entity in sparse set");
				return;
			}

			SetDenseIndex(m_denseToEntity.back(), deletedIndex);
			SetDenseIndex(id, tombstone);

			std::swap(m_dense.back(), m_dense[deletedIndex]);
			std::swap(m_denseToEntity.back(), m_denseToEntity[deletedIndex]);

			m_dense.pop_back();
			m_denseToEntity.pop_back();
		}

		void Clear() override 
		{
			m_dense.clear();
			m_sparsePages.clear();
			m_denseToEntity.clear();
		}

		bool IsEmpty() const 
		{
			return m_dense.empty();
		}

		// Read-only dense list
		const std::vector<T>& Data() const 
		{
			return m_dense;
		}
	private:
		using Sparse = std::vector<size_t>;
		std::vector<Sparse> m_sparsePages;

		std::vector<T> m_dense;
		std::vector<EntityID> m_denseToEntity; // 1:1 vector where dense index == Entity Index

		const size_t SPARSE_MAX_SIZE = 1000;

		static constexpr size_t tombstone = (std::numeric_limits<size_t>::max)();

		void SetDenseIndex(EntityID id, size_t index) 
		{
			size_t page = id / SPARSE_MAX_SIZE;
			size_t sparseIndex = id % SPARSE_MAX_SIZE;

			if (page >= m_sparsePages.size())
				m_sparsePages.resize(page + 1);

			Sparse& sparse = m_sparsePages[page];
			if (sparseIndex >= sparse.size())
				sparse.resize(sparseIndex + 1, tombstone);

			sparse[sparseIndex] = index;
		}
		size_t GetDenseIndex(EntityID id) 
		{
			size_t page = id / SPARSE_MAX_SIZE;
			size_t sparseIndex = id % SPARSE_MAX_SIZE;

			if (page < m_sparsePages.size()) {
				Sparse& sparse = m_sparsePages[page];
				if (sparseIndex < sparse.size())
					return sparse[sparseIndex];
			}

			return tombstone;
		}
	};
}

