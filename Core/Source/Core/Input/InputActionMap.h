#pragma once
#include "InputManager.h"
#include <string>
#include <vector>
#include <unordered_map>

namespace Core
{
	struct ActionBinding
	{
		enum class Type { Keyboard, Mouse };
		Type type = Type::Keyboard;
		Key key = Key::Space;
		MouseButton mouseButton = MouseButton::Left;
	};

	class InputActionMap
	{
	public:
		InputActionMap(InputManager* inputManager);
		~InputActionMap() = default;

		void bindAction(const std::string& actionName, Key key);
		void bindAction(const std::string& actionName, MouseButton mouseButton);
		void clearBindings(const std::string& actionName);

		bool isActionDown(const std::string& actionName) const;
		bool isActionPressed(const std::string& actionName) const;
		bool isActionReleased(const std::string& actionName) const;

		float getAxis(const std::string& negativeAction, const std::string& positiveAction) const;

		void initDefaultRacingBindings();

	private:
		InputManager* m_inputManager = nullptr;
		std::unordered_map<std::string, std::vector<ActionBinding>> m_actions;
	};
}
