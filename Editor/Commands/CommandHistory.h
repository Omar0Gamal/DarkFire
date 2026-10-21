#pragma once

#include "Editor/Commands/ICommand.h"
#include <deque>
#include <memory>
#include <cstddef>
#include <string_view>

namespace Editor
{
	class CommandHistory
	{
	public:
		explicit CommandHistory(size_t maxMemoryBytes = 64 * 1024 * 1024); // 64 MB
		~CommandHistory() = default;

		void execute(std::unique_ptr<ICommand> command);

		template<typename T, typename... Args>
		void execute(Args&&... args)
		{
			execute(std::make_unique<T>(std::forward<Args>(args)...));
		}
		bool undo();
		bool redo();

		bool canUndo() const noexcept { return !m_undoStack.empty(); }
		bool canRedo() const noexcept { return !m_redoStack.empty(); }

		std::string_view getUndoName() const noexcept
		{
			return m_undoStack.empty() ? "" : m_undoStack.back()->getName();
		}

		std::string_view getRedoName() const noexcept
		{
			return m_redoStack.empty() ? "" : m_redoStack.back()->getName();
		}

		void clear();

		size_t getMemoryUsage() const noexcept { return m_currentMemoryBytes; }
		size_t getMaxMemory() const noexcept { return m_maxMemoryBytes; }

		size_t getUndoCount() const noexcept { return m_undoStack.size(); }
		size_t getRedoCount() const noexcept { return m_redoStack.size(); }

	private:
		void enforceMemoryLimit();

		size_t m_maxMemoryBytes = 64 * 1024 * 1024;
		size_t m_currentMemoryBytes = 0;

		std::deque<std::unique_ptr<ICommand>> m_undoStack;
		std::deque<std::unique_ptr<ICommand>> m_redoStack;
	};
}
