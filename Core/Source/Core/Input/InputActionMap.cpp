#include "InputActionMap.h"

namespace Core
{
	InputActionMap::InputActionMap(InputManager* inputManager)
		: m_inputManager(inputManager)
	{
		initDefaultRacingBindings();
	}

	void InputActionMap::bindAction(const std::string& actionName, Key key)
	{
		ActionBinding binding;
		binding.type = ActionBinding::Type::Keyboard;
		binding.key = key;
		m_actions[actionName].push_back(binding);
	}

	void InputActionMap::bindAction(const std::string& actionName, MouseButton mouseButton)
	{
		ActionBinding binding;
		binding.type = ActionBinding::Type::Mouse;
		binding.mouseButton = mouseButton;
		m_actions[actionName].push_back(binding);
	}

	void InputActionMap::clearBindings(const std::string& actionName)
	{
		m_actions.erase(actionName);
	}

	bool InputActionMap::isActionDown(const std::string& actionName) const
	{
		if (!m_inputManager) return false;
		auto it = m_actions.find(actionName);
		if (it == m_actions.end()) return false;

		for (const auto& binding : it->second)
		{
			if (binding.type == ActionBinding::Type::Keyboard && m_inputManager->isKeyDown(binding.key))
				return true;
			if (binding.type == ActionBinding::Type::Mouse && m_inputManager->isMouseDown(binding.mouseButton))
				return true;
		}
		return false;
	}

	bool InputActionMap::isActionPressed(const std::string& actionName) const
	{
		if (!m_inputManager) return false;
		auto it = m_actions.find(actionName);
		if (it == m_actions.end()) return false;

		for (const auto& binding : it->second)
		{
			if (binding.type == ActionBinding::Type::Keyboard && m_inputManager->isKeyPressed(binding.key))
				return true;
			if (binding.type == ActionBinding::Type::Mouse && m_inputManager->isMousePressed(binding.mouseButton))
				return true;
		}
		return false;
	}

	bool InputActionMap::isActionReleased(const std::string& actionName) const
	{
		if (!m_inputManager) return false;
		auto it = m_actions.find(actionName);
		if (it == m_actions.end()) return false;

		for (const auto& binding : it->second)
		{
			if (binding.type == ActionBinding::Type::Keyboard && m_inputManager->isKeyReleased(binding.key))
				return true;
			if (binding.type == ActionBinding::Type::Mouse && m_inputManager->isMouseUp(binding.mouseButton))
				return true;
		}
		return false;
	}

	float InputActionMap::getAxis(const std::string& negativeAction, const std::string& positiveAction) const
	{
		float val = 0.0f;
		if (isActionDown(positiveAction)) val += 1.0f;
		if (isActionDown(negativeAction)) val -= 1.0f;
		return val;
	}

	void InputActionMap::initDefaultRacingBindings()
	{
		// Arcade driving
		bindAction("Accelerate", Key::W);
		bindAction("Brake", Key::S);
		bindAction("SteerLeft", Key::A);
		bindAction("SteerRight", Key::D);
		bindAction("Drift", Key::Space);
		bindAction("UseItem", Key::E);
		bindAction("LookBack", Key::C);

		// Modes & Tools
		bindAction("ToggleEditor", Key::F2);
		bindAction("ToggleDebug", Key::F1);

		// Editor flight
		bindAction("EditorForward", Key::W);
		bindAction("EditorBackward", Key::S);
		bindAction("EditorLeft", Key::A);
		bindAction("EditorRight", Key::D);
		bindAction("EditorUp", Key::E);
		bindAction("EditorDown", Key::Q);
		bindAction("EditorFast", Key::Shift);
	}
}
