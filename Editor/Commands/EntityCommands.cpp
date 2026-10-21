#include "Editor/Commands/EntityCommands.h"
#include "Engine/Core/Serialization/SceneSerializer.h"

namespace Editor
{
	static void CollectSubtree(Core::Entity root, std::vector<Core::Entity>& outList)
	{
		if (!root.isValid()) return;
		outList.push_back(root);
		for (auto child : root.getChildren())
		{
			CollectSubtree(child, outList);
		}
	}

	// -------------------------------------------------------------
	// CreateEntityCommand
	// -------------------------------------------------------------
	CreateEntityCommand::CreateEntityCommand(Core::Scene* scene, Core::Entity entity)
		: m_scene(scene)
		, m_uuid(entity.getUUID())
	{
		Core::SceneSerializer serializer(m_scene);
		std::vector<Core::Entity> subtree;
		CollectSubtree(entity, subtree);
		m_snapshotJson = serializer.serializeEntities(subtree);
	}

	CreateEntityCommand::CreateEntityCommand(Core::Scene* scene, Core::UUID uuid, std::string snapshotJson)
		: m_scene(scene)
		, m_uuid(uuid)
		, m_snapshotJson(std::move(snapshotJson))
	{
	}

	void CreateEntityCommand::execute()
	{
		if (!m_scene) return;
		Core::Entity existing = m_scene->findEntityByUUID(m_uuid);
		if (!existing.isValid())
		{
			Core::SceneSerializer serializer(m_scene);
			serializer.deserializeEntities(m_snapshotJson, false, Core::Vector3(0.0f, 0.0f, 0.0f));
		}
	}

	void CreateEntityCommand::undo()
	{
		if (!m_scene) return;
		Core::Entity entity = m_scene->findEntityByUUID(m_uuid);
		if (entity.isValid())
		{
			entity.destroy();
		}
	}

	size_t CreateEntityCommand::getMemorySize() const
	{
		return sizeof(*this) + m_snapshotJson.capacity();
	}

	// -------------------------------------------------------------
	// DeleteEntityCommand
	// -------------------------------------------------------------
	DeleteEntityCommand::DeleteEntityCommand(Core::Scene* scene, Core::Entity entity)
		: m_scene(scene)
		, m_uuid(entity.getUUID())
	{
		Core::SceneSerializer serializer(m_scene);
		std::vector<Core::Entity> subtree;
		CollectSubtree(entity, subtree);
		m_snapshotJson = serializer.serializeEntities(subtree);
	}

	void DeleteEntityCommand::execute()
	{
		if (!m_scene) return;
		Core::Entity entity = m_scene->findEntityByUUID(m_uuid);
		if (entity.isValid())
		{
			entity.destroy();
		}
	}

	void DeleteEntityCommand::undo()
	{
		if (!m_scene) return;
		Core::Entity existing = m_scene->findEntityByUUID(m_uuid);
		if (!existing.isValid())
		{
			Core::SceneSerializer serializer(m_scene);
			serializer.deserializeEntities(m_snapshotJson, false, Core::Vector3(0.0f, 0.0f, 0.0f));
		}
	}

	size_t DeleteEntityCommand::getMemorySize() const
	{
		return sizeof(*this) + m_snapshotJson.capacity();
	}

	// -------------------------------------------------------------
	// ModifyTransformCommand
	// -------------------------------------------------------------
	ModifyTransformCommand::ModifyTransformCommand(
		Core::Scene* scene,
		Core::UUID uuid,
		const Core::TransformComponent& oldTransform,
		const Core::TransformComponent& newTransform,
		bool isContinuousDrag
	)
		: m_scene(scene)
		, m_uuid(uuid)
		, m_oldTransform(oldTransform)
		, m_newTransform(newTransform)
		, m_isContinuousDrag(isContinuousDrag)
	{
	}

	void ModifyTransformCommand::execute()
	{
		if (!m_scene) return;
		Core::Entity entity = m_scene->findEntityByUUID(m_uuid);
		if (entity.isValid())
		{
			auto& tc = entity.getTransform();
			tc = m_newTransform;
			m_scene->updateTransforms();
		}
	}

	void ModifyTransformCommand::undo()
	{
		if (!m_scene) return;
		Core::Entity entity = m_scene->findEntityByUUID(m_uuid);
		if (entity.isValid())
		{
			auto& tc = entity.getTransform();
			tc = m_oldTransform;
			m_scene->updateTransforms();
		}
	}

	size_t ModifyTransformCommand::getMemorySize() const
	{
		return sizeof(*this);
	}

	bool ModifyTransformCommand::mergeWith(const ICommand& other)
	{
		const auto* otherCmd = dynamic_cast<const ModifyTransformCommand*>(&other);
		if (!otherCmd)
		{
			return false;
		}

		if (m_uuid == otherCmd->m_uuid && m_isContinuousDrag && otherCmd->m_isContinuousDrag)
		{
			m_newTransform = otherCmd->m_newTransform;
			return true;
		}

		return false;
	}

	// -------------------------------------------------------------
	// ReparentCommand
	// -------------------------------------------------------------
	ReparentCommand::ReparentCommand(
		Core::Scene* scene,
		Core::UUID entityUuid,
		Core::UUID oldParentUuid,
		Core::UUID newParentUuid
	)
		: m_scene(scene)
		, m_entityUuid(entityUuid)
		, m_oldParentUuid(oldParentUuid)
		, m_newParentUuid(newParentUuid)
	{
		if (m_scene)
		{
			Core::Entity entity = m_scene->findEntityByUUID(m_entityUuid);
			if (entity.isValid())
			{
				m_oldTransform = entity.getTransform();
				Core::Matrix worldMat = entity.getWorldMatrix();
				if (m_newParentUuid.value() != 0)
				{
					Core::Entity newParent = m_scene->findEntityByUUID(m_newParentUuid);
					if (newParent.isValid())
					{
						Core::Matrix parentWorldInv = newParent.getWorldMatrix().Invert();
						Core::Matrix newLocal = worldMat * parentWorldInv;
						Core::Math::decomposeTRS(newLocal, m_newTransform.position, m_newTransform.rotation, m_newTransform.scale);
						Core::Vector3 rads = m_newTransform.rotation.ToEuler();
						m_newTransform.eulerDegrees = Core::Vector3(Core::Math::toDegrees(rads.x), Core::Math::toDegrees(rads.y), Core::Math::toDegrees(rads.z));
					}
					else
					{
						m_newTransform = m_oldTransform;
					}
				}
				else
				{
					Core::Math::decomposeTRS(worldMat, m_newTransform.position, m_newTransform.rotation, m_newTransform.scale);
					Core::Vector3 rads = m_newTransform.rotation.ToEuler();
					m_newTransform.eulerDegrees = Core::Vector3(Core::Math::toDegrees(rads.x), Core::Math::toDegrees(rads.y), Core::Math::toDegrees(rads.z));
				}
			}
		}
	}

	void ReparentCommand::execute()
	{
		if (!m_scene) return;
		Core::Entity entity = m_scene->findEntityByUUID(m_entityUuid);
		if (!entity.isValid()) return;

		Core::Entity newParent = (m_newParentUuid.value() != 0) ? m_scene->findEntityByUUID(m_newParentUuid) : Core::Entity{};
		entity.setParent(newParent, false);
		entity.getTransform() = m_newTransform;
		m_scene->updateTransforms();
	}

	void ReparentCommand::undo()
	{
		if (!m_scene) return;
		Core::Entity entity = m_scene->findEntityByUUID(m_entityUuid);
		if (!entity.isValid()) return;

		Core::Entity oldParent = (m_oldParentUuid.value() != 0) ? m_scene->findEntityByUUID(m_oldParentUuid) : Core::Entity{};
		entity.setParent(oldParent, false);
		entity.getTransform() = m_oldTransform;
		m_scene->updateTransforms();
	}

	size_t ReparentCommand::getMemorySize() const
	{
		return sizeof(*this);
	}

	// -------------------------------------------------------------
	// TransactionCommand
	// -------------------------------------------------------------
	TransactionCommand::TransactionCommand(std::string name)
		: m_name(std::move(name))
	{
	}

	void TransactionCommand::addCommand(std::unique_ptr<ICommand> command)
	{
		if (command)
		{
			m_commands.push_back(std::move(command));
		}
	}

	void TransactionCommand::execute()
	{
		for (auto& cmd : m_commands)
		{
			if (cmd)
			{
				cmd->execute();
			}
		}
	}

	void TransactionCommand::undo()
	{
		for (auto it = m_commands.rbegin(); it != m_commands.rend(); ++it)
		{
			if (*it)
			{
				(*it)->undo();
			}
		}
	}

	size_t TransactionCommand::getMemorySize() const
	{
		size_t total = sizeof(*this) + m_name.capacity() + m_commands.capacity() * sizeof(std::unique_ptr<ICommand>);
		for (const auto& cmd : m_commands)
		{
			if (cmd)
			{
				total += cmd->getMemorySize();
			}
		}
		return total;
	}
}
