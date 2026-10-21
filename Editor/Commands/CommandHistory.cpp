#include "Editor/Commands/CommandHistory.h"

namespace Editor
{
	CommandHistory::CommandHistory(size_t maxMemoryBytes)
		: m_maxMemoryBytes(maxMemoryBytes)
	{
	}

	void CommandHistory::execute(std::unique_ptr<ICommand> command)
	{
		if (!command)
		{
			return;
		}

		command->execute();

		// Try to coalesce/merge with the top of the undo stack
		if (!m_undoStack.empty() && m_undoStack.back()->mergeWith(*command))
		{
			// Merged into previous command
			return;
		}

		// Clear redo stack on new action
		for (const auto& cmd : m_redoStack)
		{
			if (cmd)
			{
				size_t sz = cmd->getMemorySize();
				if (m_currentMemoryBytes >= sz)
				{
					m_currentMemoryBytes -= sz;
				}
				else
				{
					m_currentMemoryBytes = 0;
				}
			}
		}
		m_redoStack.clear();

		m_currentMemoryBytes += command->getMemorySize();
		m_undoStack.push_back(std::move(command));

		enforceMemoryLimit();
	}

	bool CommandHistory::undo()
	{
		if (m_undoStack.empty())
		{
			return false;
		}

		auto cmd = std::move(m_undoStack.back());
		m_undoStack.pop_back();

		cmd->undo();
		m_redoStack.push_back(std::move(cmd));
		return true;
	}

	bool CommandHistory::redo()
	{
		if (m_redoStack.empty())
		{
			return false;
		}

		auto cmd = std::move(m_redoStack.back());
		m_redoStack.pop_back();

		cmd->execute();
		m_undoStack.push_back(std::move(cmd));
		return true;
	}

	void CommandHistory::clear()
	{
		m_undoStack.clear();
		m_redoStack.clear();
		m_currentMemoryBytes = 0;
	}

	void CommandHistory::enforceMemoryLimit()
	{
		while (m_currentMemoryBytes > m_maxMemoryBytes && !m_undoStack.empty())
		{
			auto& oldest = m_undoStack.front();
			if (oldest)
			{
				size_t sz = oldest->getMemorySize();
				if (m_currentMemoryBytes >= sz)
				{
					m_currentMemoryBytes -= sz;
				}
				else
				{
					m_currentMemoryBytes = 0;
				}
			}
			m_undoStack.pop_front();
		}
	}
}
