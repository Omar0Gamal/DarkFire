#include "Core/All.h"
#include <iostream>
#include <sstream>
#include <iomanip>
#include <cmath>

class KartGameGame : public Core::Game
{
public:
    KartGameGame(const wchar_t* title, int width, int height)
        : Core::Game(title, width, height)
    {
    }

    virtual ~KartGameGame() = default;

public:
    void onCreate() override
    {
        Core::Game::onCreate();

        auto world = getActiveWorld();
        if (!world) return;

        // 1. Load Font & Configure UI Canvas
        auto font = createFont(L"Assets/Fonts/arial.font");
        if (font)
        {
            getUICanvas()->setDefaultFont(font);
        }

        // 2. Setup Sun Light
        auto sunLight = world->createEntity<Core::LightEntity>("SunLight", Core::Vector3d(1.0f, 0.95f, 0.88f));
        if (sunLight)
        {
            sunLight->getComponent<Core::TransformComponent>().setRotation(Core::Vector3d(0.85f, 0.35f, 0.0f));
        }

        // 3. Setup Main Camera
        auto camera = world->createEntity<Core::CameraEntity>("MainCamera", 2000.0f);
        if (camera)
        {
            world->setMainCamera(camera->getID());
            camera->getComponent<Core::TransformComponent>()
                .setPosition(Core::Vector3d(0.0f, 5.0f, -12.0f))
                .setRotation(Core::Vector3d(0.25f, 0.0f, 0.0f));
        }

        // 4. Ground Plane
        auto ground = Core::AssetHelper::SpawnModel(
            world.get(),
            "GroundPlane",
            L"Assets/Meshes/box.obj",
            L"Assets/Textures/grass.jpg",
            Core::Vector3d(0, -0.5f, 0),
            Core::Vector3d(0, 0, 0),
            Core::Vector3d(500.0f, 1.0f, 500.0f)
        );
        if (ground)
        {
            ground->addComponent<Core::BoxColliderComponent>(Core::BoxColliderComponent(Core::Vector3d(250.0f, 0.5f, 250.0f)));
        }

        // 5. Test Kart Entity
        m_kartPosition = Core::Vector3d(0, 0.8f, 0);
        m_kartEntity = Core::AssetHelper::SpawnModel(
            world.get(),
            "PlayerKart",
            L"Assets/Meshes/box.obj",
            L"Assets/Textures/spaceship.jpg",
            m_kartPosition,
            Core::Vector3d(0, 0, 0),
            Core::Vector3d(2.4f, 0.9f, 4.0f)
        );
        if (m_kartEntity)
        {
            m_kartEntity->addComponent<Core::SphereColliderComponent>(Core::SphereColliderComponent(1.8f));
            m_kartEntity->addComponent<Core::RigidbodyComponent>(Core::RigidbodyComponent(950.0f, false));
        }

        // 6. UI HUD Setup
        auto canvas = getUICanvas();
        if (canvas)
        {
            // Title Banner
            m_titleLabel = canvas->addElement<Core::UILabel>(
                L"DARKFIRE ENGINE v2 - KART TESTBED",
                Core::Vector2d(20, 20),
                Core::UIAnchor::TopLeft
            );
            m_titleLabel->setScale(1.15f);
            m_titleLabel->setColor(Core::Vector4d(1.0f, 0.85f, 0.2f, 1.0f));

            // Mode Label
            m_modeLabel = canvas->addElement<Core::UILabel>(
                L"Mode: PLAY MODE (Press F2 for Track Editor)",
                Core::Vector2d(20, 50),
                Core::UIAnchor::TopLeft
            );
            m_modeLabel->setColor(Core::Vector4d(0.2f, 1.0f, 0.4f, 1.0f));

            // Controls Guide
            m_controlsLabel = canvas->addElement<Core::UILabel>(
                L"[F1] Physics Debug Wireframes | [F2] Track Editor Mode\n"
                L"[Drive] W/S or Up/Down: Throttle/Brake | A/D or Left/Right: Steer | Space: Boost\n"
                L"[Editor] WASD+RMB: Fly Cam | 1-6: Palette | LMB: Place | R: Rotate | W: Waypoints | F5: Save | F9: Load",
                Core::Vector2d(20, 80),
                Core::UIAnchor::TopLeft
            );
            m_controlsLabel->setScale(0.85f);
            m_controlsLabel->setColor(Core::Vector4d(0.8f, 0.85f, 0.95f, 0.95f));

            // Editor Status Label (Active when in Editor)
            m_editorStatusLabel = canvas->addElement<Core::UILabel>(
                L"",
                Core::Vector2d(20, 150),
                Core::UIAnchor::TopLeft
            );
            m_editorStatusLabel->setColor(Core::Vector4d(1.0f, 0.6f, 0.2f, 1.0f));

            // Speedometer HUD (Bottom Right)
            m_speedLabel = canvas->addElement<Core::UILabel>(
                L"SPEED: 0 KM/H",
                Core::Vector2d(240, 75),
                Core::UIAnchor::BottomRight
            );
            m_speedLabel->setScale(1.1f);
            m_speedLabel->setColor(Core::Vector4d(0.3f, 0.9f, 1.0f, 1.0f));

            m_speedBar = canvas->addElement<Core::UIProgressBar>(
                0.0f,
                Core::Vector2d(240, 45),
                Core::Vector2d(220, 18),
                Core::UIAnchor::BottomRight
            );

            // Boost Label & Bar
            m_boostLabel = canvas->addElement<Core::UILabel>(
                L"BOOST GAUGE",
                Core::Vector2d(240, 30),
                Core::UIAnchor::BottomRight
            );
            m_boostLabel->setScale(0.75f);
            m_boostLabel->setColor(Core::Vector4d(1.0f, 0.6f, 0.1f, 1.0f));

            m_boostBar = canvas->addElement<Core::UIProgressBar>(
                1.0f,
                Core::Vector2d(240, 12),
                Core::Vector2d(220, 12),
                Core::UIAnchor::BottomRight
            );
        }

        std::cout << "[KartGame] Testbed initialized successfully.\n";
    }

    void onUpdate(float deltaTime) override
    {
        Core::Game::onUpdate(deltaTime);

        auto trackEditor = getTrackEditor();
        bool isEditor = trackEditor ? trackEditor->isEditorActive() : false;

        if (isEditor)
        {
            if (m_modeLabel)
            {
                m_modeLabel->setText(L"Mode: TRACK EDITOR [ACTIVE] (Press F2 to return to Play Mode)");
                m_modeLabel->setColor(Core::Vector4d(1.0f, 0.4f, 0.2f, 1.0f));
            }

            if (m_editorStatusLabel && trackEditor)
            {
                const char* paletteNames[] = {
                    "Straight Road (14x25m)",
                    "Curved Road (14x18m)",
                    "Boost Pad (Speed Strip)",
                    "Item Box (Mystery Crate)",
                    "Barrier (Guardrail)",
                    "Tree (Environment Prop)"
                };
                int idx = trackEditor->getSelectedPaletteIndex();
                std::wstring pieceName = L"Unknown";
                if (idx >= 0 && idx < 6)
                {
                    std::string aName = paletteNames[idx];
                    pieceName = std::wstring(aName.begin(), aName.end());
                }

                std::wstringstream ss;
                ss << L"Selected Palette [" << (idx + 1) << L"/6]: " << pieceName << L"\n";
                ss << L"Waypoint Tool: " << (trackEditor->isWaypointToolActive() ? L"[ACTIVE] (Click to drop waypoint ribbons)" : L"[OFF]") << L"\n";
                ss << L"Placed Pieces: " << trackEditor->getPlacedPieces().size()
                   << L" | Recorded Waypoints: " << trackEditor->getWaypoints().size();
                m_editorStatusLabel->setText(ss.str());
            }
        }
        else
        {
            if (m_modeLabel)
            {
                m_modeLabel->setText(L"Mode: PLAY MODE (Press F2 for Track Editor)");
                m_modeLabel->setColor(Core::Vector4d(0.2f, 1.0f, 0.4f, 1.0f));
            }

            if (m_editorStatusLabel)
            {
                m_editorStatusLabel->setText(L"");
            }

            // Update Kart Driving Controls
            updateKartPhysics(deltaTime);
        }
    }

    void updateKartPhysics(float deltaTime)
    {
        if (!m_kartEntity) return;

        auto input = getInputManager();
        if (!input) return;

        auto& trans = m_kartEntity->getComponent<Core::TransformComponent>();

        // Controls
        bool throttle = input->isKeyDown(Core::Key::W) || input->isKeyDown(Core::Key::Up);
        bool brake = input->isKeyDown(Core::Key::S) || input->isKeyDown(Core::Key::Down);
        bool steerLeft = input->isKeyDown(Core::Key::A) || input->isKeyDown(Core::Key::Left);
        bool steerRight = input->isKeyDown(Core::Key::D) || input->isKeyDown(Core::Key::Right);
        bool boosting = input->isKeyDown(Core::Key::Space) && (m_boostAmount > 0.05f);

        // Boost consumption/recharge
        if (boosting)
        {
            m_boostAmount -= deltaTime * 0.35f;
            if (m_boostAmount < 0.0f) m_boostAmount = 0.0f;
        }
        else
        {
            m_boostAmount += deltaTime * 0.15f;
            if (m_boostAmount > 1.0f) m_boostAmount = 1.0f;
        }

        // Acceleration & top speeds
        float maxSpeed = boosting ? 65.0f : 40.0f;
        float accelRate = boosting ? 45.0f : 25.0f;
        float brakeRate = 35.0f;
        float naturalDrag = 8.0f;

        if (throttle)
        {
            m_kartSpeed += accelRate * deltaTime;
            if (m_kartSpeed > maxSpeed) m_kartSpeed = maxSpeed;
        }
        else if (brake)
        {
            m_kartSpeed -= brakeRate * deltaTime;
            if (m_kartSpeed < -15.0f) m_kartSpeed = -15.0f;
        }
        else
        {
            if (m_kartSpeed > 0.0f)
            {
                m_kartSpeed -= naturalDrag * deltaTime;
                if (m_kartSpeed < 0.0f) m_kartSpeed = 0.0f;
            }
            else if (m_kartSpeed < 0.0f)
            {
                m_kartSpeed += naturalDrag * deltaTime;
                if (m_kartSpeed > 0.0f) m_kartSpeed = 0.0f;
            }
        }

        // Steering
        float turnSpeed = 2.2f;
        if (std::abs(m_kartSpeed) > 0.5f)
        {
            float steerFactor = (m_kartSpeed < 0.0f) ? -1.0f : 1.0f;
            if (steerLeft) m_kartYaw -= turnSpeed * deltaTime * steerFactor;
            if (steerRight) m_kartYaw += turnSpeed * deltaTime * steerFactor;
        }

        // Forward vector in XZ plane
        Core::Vector3d forward(
            std::sin(m_kartYaw),
            0.0f,
            std::cos(m_kartYaw)
        );

        m_kartPosition = m_kartPosition + (forward * (m_kartSpeed * deltaTime));
        trans.setPosition(m_kartPosition);
        trans.setRotation(Core::Vector3d(0, m_kartYaw, 0));

        // Smooth 3rd-person chase camera
        auto world = getActiveWorld();
        if (world)
        {
            auto camEntity = world->getEntity<Core::Entity>(world->getMainCameraID());
            if (camEntity)
            {
                Core::Vector3d camTarget = m_kartPosition - (forward * 10.0f) + Core::Vector3d(0, 4.2f, 0);
                camEntity->getComponent<Core::TransformComponent>()
                    .setPosition(camTarget)
                    .setRotation(Core::Vector3d(0.25f, m_kartYaw, 0.0f));
            }
        }

        // Post-Processing radial blur effect during boost
        auto postProcess = getPostProcessPipeline();
        if (postProcess)
        {
            auto& settings = postProcess->getSettings();
            if (boosting && m_kartSpeed > 30.0f)
            {
                settings.radialBlurEnabled = true;
                settings.radialBlurIntensity = (std::min)(0.08f, settings.radialBlurIntensity + deltaTime * 0.2f);
            }
            else
            {
                settings.radialBlurIntensity -= deltaTime * 0.2f;
                if (settings.radialBlurIntensity <= 0.0f)
                {
                    settings.radialBlurIntensity = 0.0f;
                    settings.radialBlurEnabled = false;
                }
            }
        }

        // Update UI HUD
        if (m_speedBar)
        {
            m_speedBar->setValue((std::max)(0.0f, m_kartSpeed / 65.0f));
        }
        if (m_speedLabel)
        {
            int kmh = static_cast<int>(std::abs(m_kartSpeed) * 3.6f);
            m_speedLabel->setText(L"SPEED: " + std::to_wstring(kmh) + L" KM/H");
        }
        if (m_boostBar)
        {
            m_boostBar->setValue(m_boostAmount);
        }
    }

    void onQuit() override
    {
        Core::Game::onQuit();
    }

private:
    std::shared_ptr<Core::IEntity> m_kartEntity;
    Core::Vector3d m_kartPosition = { 0, 0.8f, 0 };
    float m_kartYaw = 0.0f;
    float m_kartSpeed = 0.0f;
    float m_boostAmount = 1.0f;

    // HUD references
    std::shared_ptr<Core::UILabel> m_titleLabel;
    std::shared_ptr<Core::UILabel> m_modeLabel;
    std::shared_ptr<Core::UILabel> m_controlsLabel;
    std::shared_ptr<Core::UILabel> m_editorStatusLabel;
    std::shared_ptr<Core::UILabel> m_speedLabel;
    std::shared_ptr<Core::UIProgressBar> m_speedBar;
    std::shared_ptr<Core::UILabel> m_boostLabel;
    std::shared_ptr<Core::UIProgressBar> m_boostBar;
};

#ifdef WINDOWS
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PSTR lpCmdLine, int nCmdShow)
{
    try
    {
        KartGameGame game(L"DarkFire v2 - Kart Game & Track Editor", 1280, 720);
        game.run();
    }
    catch (const std::exception& e)
    {
        MessageBoxA(NULL, e.what(), "DarkFire Game Error", MB_ICONERROR | MB_OK);
        return 1;
    }
    return 0;
}
#else
int main(int argc, char* argv[])
{
    KartGameGame game(L"DarkFire v2 - Kart Game & Track Editor", 1280, 720);
    game.run();
    return 0;
}
#endif
