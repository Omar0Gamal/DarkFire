#pragma once

#include "Editor/Commands/ICommand.h"
#include "Engine/Core/Scene/Scene.h"
#include "Engine/Core/Scene/Entity.h"
#include "Engine/Core/Scene/UUID.h"
#include "Engine/Core/Scene/Components.h"
#include <string>
#include <vector>
#include <memory>

namespace Editor
{
	class CreateEntityCommand : public ICommand
	{
	public:
		CreateEntityCommand(Core::Scene* scene, Core::Entity entity);
		CreateEntityCommand(Core::Scene* scene, Core::UUID uuid, std::string snapshotJson);

		void execute() override;
		void undo() override;
		size_t getMemorySize() const override;
		std::string_view getName() const override { return "Create Entity"; }

		Core::UUID getEntityUUID() const noexcept { return m_uuid; }

	private:
		Core::Scene* m_scene = nullptr;
		Core::UUID m_uuid;
		std::string m_snapshotJson;
	};

	class DeleteEntityCommand : public ICommand
	{
	public:
		DeleteEntityCommand(Core::Scene* scene, Core::Entity entity);

		void execute() override;
		void undo() override;
		size_t getMemorySize() const override;
		std::string_view getName() const override { return "Delete Entity"; }

		Core::UUID getEntityUUID() const noexcept { return m_uuid; }

	private:
		Core::Scene* m_scene = nullptr;
		Core::UUID m_uuid;
		std::string m_snapshotJson;
	};

	class ModifyTransformCommand : public ICommand
	{
	public:
		ModifyTransformCommand(
			Core::Scene* scene,
			Core::UUID uuid,
			const Core::TransformComponent& oldTransform,
			const Core::TransformComponent& newTransform,
			bool isContinuousDrag = false
		);

		void execute() override;
		void undo() override;
		size_t getMemorySize() const override;
		std::string_view getName() const override { return "Modify Transform"; }

		bool mergeWith(const ICommand& other) override;

	private:
		Core::Scene* m_scene = nullptr;
		Core::UUID m_uuid;
		Core::TransformComponent m_oldTransform;
		Core::TransformComponent m_newTransform;
		bool m_isContinuousDrag = false;
	};

	class ReparentCommand : public ICommand
	{
	public:
		ReparentCommand(
			Core::Scene* scene,
			Core::UUID entityUuid,
			Core::UUID oldParentUuid,
			Core::UUID newParentUuid
		);

		void execute() override;
		void undo() override;
		size_t getMemorySize() const override;
		std::string_view getName() const override { return "Reparent Entity"; }

	private:
		Core::Scene* m_scene = nullptr;
		Core::UUID m_entityUuid;
		Core::UUID m_oldParentUuid;
		Core::UUID m_newParentUuid;
		Core::TransformComponent m_oldTransform;
		Core::TransformComponent m_newTransform;
	};

	class TransactionCommand : public ICommand
	{
	public:
		explicit TransactionCommand(std::string name = "Compound Transaction");

		void addCommand(std::unique_ptr<ICommand> command);

		void execute() override;
		void undo() override;
		size_t getMemorySize() const override;
		std::string_view getName() const override { return m_name; }

		bool isEmpty() const noexcept { return m_commands.empty(); }

	private:
		std::string m_name;
		std::vector<std::unique_ptr<ICommand>> m_commands;
	};
}
