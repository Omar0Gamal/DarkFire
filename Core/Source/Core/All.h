#pragma once

// Core Engine Headers - Include this for easy access to all engine features

// Prerequisites and Types
#include "Core/Graphics/Prerequisites.h"
#include "Core/Math/Vector3d.h"
#include "Core/Math/Vector4d.h"
#include "Core/Math/Mat4x4.h"
#include "Core/Math/Box.h"

// Logging
#include <Core/log/Log.h>

// Input
#include "Core/Input/InputManager.h"

// Resources
#include "Core/Resource/Material.h"
#include "Core/Resource/Texture.h"
#include "Core/Resource/Mesh.h"

// Game Framework
#include "Core/Game/Game.h"
#include "Core/Game/Display.h"
#include "Core/Game/ECS.h"
#include "Core/Game/WorldManager.h"
#include "Core/Game/World.h"
#include "Core/Game/IEntity.h"

// Components
#include "Core/Game/Component/CameraComponent.h"
#include "Core/Game/Component/TransformComponent.h"
#include "Core/Game/Component/LightComponent.h"
#include "Core/Game/Component/MeshComponent.h"
#include "Core/Game/Component/ColliderComponent.h"
#include "Core/Game/Component/RigidbodyComponent.h"
#include "Core/Game/Component/TextComponent.h"
#include "Core/Game/Component/ImageComponent.h"

// Render Pipeline
#include "Core/Graphics/RenderPipeline.h"
#include "Core/Graphics/RenderPass.h"