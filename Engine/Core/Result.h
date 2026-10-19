#pragma once

#include "Engine/Core/Defines.h"
#include <variant>
#include <string>
#include <optional>
#include <utility>

namespace Core
{
	template<typename T, typename E = std::string>
	class Result
	{
	public:
		static Result Ok(T val)
		{
			return Result(std::move(val));
		}

		static Result Err(E err)
		{
			return Result(std::move(err), ErrorTag{});
		}

		bool isOk() const { return m_storage.index() == 0; }
		bool isErr() const { return m_storage.index() == 1; }

		const T& value() const { return std::get<0>(m_storage); }
		T& value() { return std::get<0>(m_storage); }

		const E& error() const { return std::get<1>(m_storage); }
		E& error() { return std::get<1>(m_storage); }

		T unwrap()
		{
			DF_ASSERT(isOk(), "Attempted to unwrap an Err Result!");
			return std::move(std::get<0>(m_storage));
		}

		T valueOr(T defaultVal) const
		{
			if (isOk())
			{
				return std::get<0>(m_storage);
			}
			return defaultVal;
		}

	private:
		struct ErrorTag {};
		explicit Result(T val) : m_storage(std::in_place_index<0>, std::move(val)) {}
		Result(E err, ErrorTag) : m_storage(std::in_place_index<1>, std::move(err)) {}

		std::variant<T, E> m_storage;
	};

	// Void specialization for fallible operations without a return value
	template<typename E>
	class Result<void, E>
	{
	public:
		static Result Ok()
		{
			return Result();
		}

		static Result Err(E err)
		{
			return Result(std::move(err));
		}

		bool isOk() const { return !m_error.has_value(); }
		bool isErr() const { return m_error.has_value(); }

		const E& error() const { return *m_error; }
		E& error() { return *m_error; }

		void unwrap()
		{
			DF_ASSERT(isOk(), "Attempted to unwrap an Err Result!");
		}

	private:
		Result() : m_error(std::nullopt) {}
		explicit Result(E err) : m_error(std::move(err)) {}

		std::optional<E> m_error;
	};
}
